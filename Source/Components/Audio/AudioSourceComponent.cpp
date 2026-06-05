#include "pch.h"
#include "AudioSourceComponent.h"
#include "Graphics/Core/Graphics.h"

#include <x3daudio.h>
#include <imgui.h>

void AudioSourceComponent::SetSource(const std::wstring& filePath)
{
    // 再生中なら停止
    Stop();
    if (sourceVoice)
    {
        sourceVoice->DestroyVoice();
    }
    // ファイルパスに "BGM" が含まれていれば BGM、含まれていなければ SE として扱う
    this->soundType = std::wstring(filePath).find(L"BGM") != std::wstring::npos ? CoreSoundType::BGM : CoreSoundType::SE;
    // ファイルパスを保存
    this->sourceFilePath = filePath;
    // ループ設定
    this->isLooping = (this->soundType == CoreSoundType::BGM) ? true : false;
    // オーディオバッファを取得
    audioBuffer = CoreAudio::CoreAudioBuffer::GetResource(filePath);
    // ソースボイスを作成
    CoreAudio::CreateAudioSource(audioBuffer, &sourceVoice, soundType);
}

AudioSourceComponent::~AudioSourceComponent()
{
    Stop();
    if (sourceVoice)
    {
        sourceVoice->DestroyVoice();
    }
}

void AudioSourceComponent::Tick(float deltaTime)
{
    //バッファやソースボイスが設定されていなければ何もしない
    if (!audioBuffer || !sourceVoice)
    {
        return;
    }

    if (!isBgm)
    {// SE の場合はシステムのミュートやポーズ状態を反映する
        // 追加：システムポーズ検知
        if (CoreAudio::GetSystemPaused())
        {
            // 初回だけPause
            if (!wasSystemPaused)
            {
                Pause();
                wasSystemPaused = true;
            }
            return;
        }

        // ポーズ解除された瞬間だけResume
        if (wasSystemPaused)
        {
            Resume();
            wasSystemPaused = false;
        }
    }


    //音源が3D音源として扱わない場合は何もしない
    if (!use3DAudio) return;

#ifdef X3DAUDIO
    // 3D音源として扱う場合はリスナーと音源の位置関係から音量やパンを計算する
    C3DAudio::Culculate3DAudio(this->gameObject);
#endif // X3DAUDIO
}

void AudioSourceComponent::Play()
{
    //バッファやソースボイスが設定されていなければ何もしない
    if (!audioBuffer || !sourceVoice)
    {
        return;
    }

    HRESULT hr = S_OK;

    XAUDIO2_VOICE_STATE voiceState = {};
    sourceVoice->GetState(&voiceState);

    // すでに再生中なら何もしない
    if (voiceState.BuffersQueued)
    {
        //Stop(false, 0);
        return;
    }

    // 再生停止(テイル無し)
    Stop(false);

    // バッファをソースボイスにセット
    XAUDIO2_BUFFER* pBuffer = &audioBuffer->buffer;
    pBuffer->LoopCount = isLooping ? XAUDIO2_LOOP_INFINITE : 0;
    hr = sourceVoice->SubmitSourceBuffer(pBuffer);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // 再生開始
    hr = sourceVoice->Start(0);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void AudioSourceComponent::Stop(bool playTails)
{
    //バッファやソースボイスが設定されていなければ何もしない
    if (!audioBuffer || !sourceVoice)
    {
        return;
    }

    XAUDIO2_VOICE_STATE voiceState{};
    sourceVoice->GetState(&voiceState);

    // 再生中でなければ何もしない
    if (!voiceState.BuffersQueued)
    {
        return;
    }

    HRESULT hr;
    hr = sourceVoice->Stop(playTails ? XAUDIO2_PLAY_TAILS : 0);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    hr = sourceVoice->FlushSourceBuffers();
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // SamplesPlayed リセット
    hr = sourceVoice->Discontinuity();
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

}

void AudioSourceComponent::Pause()
{
    //バッファやソースボイスが設定されていなければ何もしない
    if (!audioBuffer || !sourceVoice)
    {
        return;
    }
    HRESULT hr = sourceVoice->Stop(0);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void AudioSourceComponent::Resume()
{
    //バッファやソースボイスが設定されていなければ何もしない
    if (!audioBuffer || !sourceVoice)
    {
        return;
    }
    HRESULT hr = sourceVoice->Start(0);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void AudioSourceComponent::SetVolume(float volume)
{
    //バッファやソースボイスが設定されていなければ何もしない
    if (!audioBuffer || !sourceVoice)
    {
        return;
    }
    HRESULT hr = sourceVoice->SetVolume(volume);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

float AudioSourceComponent::GetVolume() const
{
    //バッファやソースボイスが設定されていなければ何もしない
    if (!audioBuffer || !sourceVoice)
    {
        return  0.0f;

    }
    float volume = 0.0f;
    sourceVoice->GetVolume(&volume);
    return volume;
}


bool AudioSourceComponent::IsPlaying() const
{
    XAUDIO2_VOICE_STATE voiceState{};
    sourceVoice->GetState(&voiceState);
    return voiceState.BuffersQueued > 0;
}

void AudioSourceComponent::SetPan(float pan)
{
    this->pan = std::clamp(pan, -1.0f, 1.0f);

    // TODO:何故か完全に左に寄ってしまう
    float outputMatrix[8] = {};
    float left = 1.0f - (this->pan * 0.5f + 0.5f);
    float right = this->pan * 0.5f + 0.5f;

    DWORD channelMask;
    CoreAudio::masterVoice->GetChannelMask(&channelMask);
    switch (channelMask)
    {
    case SPEAKER_MONO:
    {
        outputMatrix[0] = (left + right) * 0.5f; // Mono
        break;
    }
    case SPEAKER_STEREO:
    case SPEAKER_2POINT1:
    case SPEAKER_SURROUND:
    {
        outputMatrix[0] = left;  // Front Left
        outputMatrix[1] = right; // Front Right
        break;
    }
    case SPEAKER_QUAD:
    {
        outputMatrix[0] = left;  // Front Left
        outputMatrix[1] = right; // Front Right
        outputMatrix[2] = left;  // Back Left
        outputMatrix[3] = right; // Back Right
        break;
    }
    case SPEAKER_4POINT1:
    {
        outputMatrix[0] = left;  // Front Left
        outputMatrix[1] = right; // Front Right
        outputMatrix[2] = 0.0f;  // Front Center
        outputMatrix[3] = 0.0f;  // Low Frequency
        outputMatrix[4] = left;  // Back Left
        outputMatrix[5] = right; // Back Right
        break;
    }
    case SPEAKER_5POINT1:
    {
        outputMatrix[0] = left;  // Front Left
        outputMatrix[1] = right; // Front Right
        outputMatrix[2] = 0.0f;  // Front Center
        outputMatrix[3] = 0.0f;  // Low Frequency
        outputMatrix[4] = left;  // Back Left
        outputMatrix[5] = right; // Back Right
        break;
    }
    case SPEAKER_7POINT1:
    {
        outputMatrix[0] = left;  // Front Left
        outputMatrix[1] = right; // Front Right
        outputMatrix[2] = 0.0f;  // Front Center
        outputMatrix[3] = 0.0f;  // Low Frequency
        outputMatrix[4] = left;  // Side Left
        outputMatrix[5] = right; // Side Right
        outputMatrix[6] = left;  // Back Left
        outputMatrix[7] = right; // Back Right
        break;
    }
    case SPEAKER_5POINT1_SURROUND:
    {
        outputMatrix[0] = left;  // Front Left
        outputMatrix[1] = right; // Front Right
        outputMatrix[2] = 0.0f;  // Front Center
        outputMatrix[3] = 0.0f;  // Low Frequency
        outputMatrix[4] = 0.0f;  // Side Left
        outputMatrix[5] = 0.0f;  // Side Right
        outputMatrix[6] = left;  // Back Left
        outputMatrix[7] = right; // Back Right
        break;
    }
    case SPEAKER_7POINT1_SURROUND:
    {
        outputMatrix[0] = left;  // Front Left
        outputMatrix[1] = right; // Front Right
        outputMatrix[2] = 0.0f;  // Front Center
        outputMatrix[3] = 0.0f;  // Low Frequency
        outputMatrix[4] = 0.0f;  // Side Left
        outputMatrix[5] = 0.0f;  // Side Right
        outputMatrix[6] = left;  // Back Left
        outputMatrix[7] = right; // Back Right
        break;
    }
    default:
        break;
    }

    XAUDIO2_VOICE_DETAILS voiceDetails;
    sourceVoice->GetVoiceDetails(&voiceDetails);

    XAUDIO2_VOICE_DETAILS masterVoiceDetails;
    CoreAudio::masterVoice->GetVoiceDetails(&masterVoiceDetails);

    XAUDIO2_VOICE_DETAILS subVoiceDetails;
    CoreAudio::submixVoices[soundType]->GetVoiceDetails(&subVoiceDetails);


    HRESULT hr = sourceVoice->SetOutputMatrix(NULL, voiceDetails.InputChannels, subVoiceDetails.InputChannels, outputMatrix);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

}

void AudioSourceComponent::SetLoopRange(float begin, float length)
{
    //バッファやソースボイスが設定されていなければ何もしない
    if (!audioBuffer || !sourceVoice)
    {
        return;
    }
    XAUDIO2_BUFFER* pBuffer = &audioBuffer->buffer;
    const WAVEFORMATEX& format = audioBuffer->wfx.Format;

    const UINT32 sampleRate = format.nSamplesPerSec;
    const UINT32 blockAlign = format.nBlockAlign;
    const UINT32 totalSamples = pBuffer->AudioBytes / blockAlign;

    UINT32 loopBegin = static_cast<UINT32>(sampleRate * begin);
    UINT32 loopLength = static_cast<UINT32>(sampleRate * length);

    // 範囲チェック
    if (loopBegin >= totalSamples) {
        loopBegin = 0;
        loopLength = 0;
        pBuffer->LoopCount = 0; // 無効化
    }
    else if (loopBegin + loopLength > totalSamples) {
        loopLength = totalSamples - loopBegin;
    }

    pBuffer->LoopBegin = loopBegin;
    pBuffer->LoopLength = loopLength;
}

float AudioSourceComponent::GetPlaybackTime() const
{
    //バッファやソースボイスが設定されていなければ 0 を返す
    if (!audioBuffer || !sourceVoice)
    {
        return 0.0f;
    }
    XAUDIO2_VOICE_STATE voiceState{};
    sourceVoice->GetState(&voiceState);
    const WAVEFORMATEX& format = audioBuffer->wfx.Format;
    const UINT32 sampleRate = format.nSamplesPerSec;
    const float playbackTime = static_cast<float>(voiceState.SamplesPlayed) / static_cast<float>(sampleRate);
    return playbackTime;
}

float AudioSourceComponent::GetPlaybackDeltaTime()
{
    //バッファやソースボイスが設定されていなければ 0 を返す
    if (!audioBuffer || !sourceVoice)
    {
        return 0.0f;
    }
    XAUDIO2_VOICE_STATE state{};
    sourceVoice->GetState(&state);

    uint64_t samples = state.SamplesPlayed;

    // 前回のサンプル数からの差分を計算して秒数に変換
    float deltaTime = (samples - lastSamplesPlayed) / static_cast<float>(audioBuffer->wfx.Format.nSamplesPerSec);
    lastSamplesPlayed = static_cast<float>(samples);
    return deltaTime;
}


uint32_t AudioSourceComponent::GetBufferQueueCount()
{
    //バッファやソースボイスが設定されていなければ 0 を返す
    if (!audioBuffer || !sourceVoice)
    {
        return 0;
    }
    XAUDIO2_VOICE_STATE voiceState{};
    sourceVoice->GetState(&voiceState);
    return voiceState.BuffersQueued;
}

float AudioSourceComponent::GetTotalDuration() const
{
    // バッファが設定されていなければ 0 を返す
    if (!audioBuffer)
    {
        return 0.0f;
    }
    return audioBuffer->GetDuration();
}

void AudioSourceComponent::SetPitch(const float pitch)
{
    if (!sourceVoice)
    {
        return;
    }

    this->pitch = std::clamp(pitch, 0.1f, XAUDIO2_MAX_FREQ_RATIO);

    HRESULT hr = sourceVoice->SetFrequencyRatio(this->pitch);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}



void AudioSourceComponent::DrawImGuiInspector()
{
#ifdef USE_IMGUI
    ImGui::PushID(this);

    ImGui::SameLine();
    // 現在のファイルパスを表示
    std::wstring path = sourceFilePath.empty() ? L"No file" : sourceFilePath;
    std::string pathUtf8 = WStringToUTF8(path);
    ImGui::Text("%s", pathUtf8.c_str());
    // ソースボイスが設定されていなければ何もしない
    if (sourceVoice)
    {
        // 3D音源関連の設定
        {
            // 3D音源として扱うか
            ImGui::Checkbox("3D Audio", &use3DAudio);

            // パン設定
            if (ImGui::SliderFloat("Pan", &pan, -1.0f, 1.0f))
            {
                SetPan(pan);
            }
        }

#if 0
        ImGui::Separator();
        // ループ設定
        static float loopBegin = 0.0f;
        static float loopLength = 0.0f;
        if (ImGui::InputFloat("Loop Begin", &loopBegin)) {
            loopBegin = max(0.0f, loopBegin);
            SetLoopOption(loopBegin, loopLength);
        }
        if (ImGui::InputFloat("Loop Length", &loopLength)) {
            loopLength = max(0.0f, loopLength);
            SetLoopOption(loopBegin, loopLength);
        }
#endif // 0

        // ループ有無
        ImGui::Checkbox("Loop", &isLooping);


        // 再生・停止ボタン
        if (ImGui::Button("Play")) {
            Play();
        }
        if (ImGui::Button("Stop")) {
            Stop();
        }

        // 音量スライダー
        static float volume;
        sourceVoice->GetVolume(&volume);
        if (ImGui::SliderFloat("Volume", &volume, 0, 1)) {
            sourceVoice->SetVolume(volume);
        }
    }
    if (ImGui::SliderFloat("Pitch", &pitch, 0.5f, 2.0f))
    {
        SetPitch(pitch);
    }

    ImGui::Separator();

    // マスター音量、BGM音量、SE音量
    {
        if (ImGui::SliderFloat("MasterVolume", &masterVolume, 0, 1)) {
            CoreAudio::SetMasterVolume(masterVolume);
        }
        if (ImGui::SliderFloat("BgmVolume", &bgmVolume, 0, 1)) {
            CoreAudio::SetBgmVolume(bgmVolume);
        }
        if (ImGui::SliderFloat("SeVolume", &seVolume, 0, 1)) {
            CoreAudio::SetSeVolume(seVolume);
        }
    }

    ImGui::PopID();

#endif // USE_IMGUI
}