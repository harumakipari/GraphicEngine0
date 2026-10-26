#include "pch.h"
#include "Audio.h"

#include <imgui.h>
#include <memory>


namespace
{
    class ScopedFileHandle
    {
    public:
        explicit ScopedFileHandle(HANDLE handle) noexcept : handle_(handle) {}

        ~ScopedFileHandle()
        {
            if (handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr)
            {
                CloseHandle(handle_);
            }
        }

        ScopedFileHandle(const ScopedFileHandle&) = delete;
        ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;

        HANDLE Get() const noexcept { return handle_; }

    private:
        HANDLE handle_ = INVALID_HANDLE_VALUE;
    };

    HRESULT GetLastErrorAsHRESULT_()
    {
        const DWORD error = GetLastError();
        return HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_READ_FAULT : error);
    }

    HRESULT SetFilePointerChecked_(HANDLE hfile, LONG distance, DWORD moveMethod)
    {
        SetLastError(ERROR_SUCCESS);
        const DWORD result = SetFilePointer(hfile, distance, nullptr, moveMethod);
        if (result == INVALID_SET_FILE_POINTER && GetLastError() != ERROR_SUCCESS)
        {
            return GetLastErrorAsHRESULT_();
        }
        return S_OK;
    }

    HRESULT ReadFileExactly_(HANDLE hfile, void* buffer, DWORD size)
    {
        DWORD bytesRead = 0;
        if (!ReadFile(hfile, buffer, size, &bytesRead, nullptr))
        {
            return GetLastErrorAsHRESULT_();
        }
        if (bytesRead != size)
        {
            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
        }
        return S_OK;
    }
}

HRESULT FindChunk_(HANDLE hfile, DWORD fourcc, DWORD& chunkSize, DWORD& chunkDataPosition)
{
    HRESULT hr = SetFilePointerChecked_(hfile, 0, FILE_BEGIN);
    if (FAILED(hr)) return hr;

    DWORD bytesRead = 0;
    DWORD riffEnd = 0;
    bool insideRiff = false;

    for (;;)
    {
        if (insideRiff && (bytesRead > riffEnd || riffEnd - bytesRead < sizeof(DWORD) * 2))
            return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

        DWORD chunkType = 0;
        DWORD chunkDataSize = 0;
        hr = ReadFileExactly_(hfile, &chunkType, sizeof(chunkType));
        if (FAILED(hr)) return hr;
        bytesRead += sizeof(chunkType);
        hr = ReadFileExactly_(hfile, &chunkDataSize, sizeof(chunkDataSize));
        if (FAILED(hr)) return hr;
        bytesRead += sizeof(chunkDataSize);

        const DWORD chunkDataPositionLocal = bytesRead;
        if (chunkType == 'FFIR')
        {
            if (chunkDataSize < sizeof(DWORD) || chunkDataSize > MAXDWORD - sizeof(DWORD) * 2)
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

            if (chunkType == fourcc)
            {
                chunkSize = chunkDataSize;
                chunkDataPosition = chunkDataPositionLocal;
                return S_OK;
            }

            if (insideRiff)
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

            DWORD formType = 0;
            hr = ReadFileExactly_(hfile, &formType, sizeof(formType));
            if (FAILED(hr)) return hr;
            bytesRead += sizeof(formType);
            riffEnd = sizeof(DWORD) * 2 + chunkDataSize;
            insideRiff = true;
            continue;
        }

        const ULONGLONG paddedChunkSize = static_cast<ULONGLONG>(chunkDataSize) + (chunkDataSize & 1u);
        const ULONGLONG chunkEnd = static_cast<ULONGLONG>(bytesRead) + paddedChunkSize;
        if (!insideRiff || chunkEnd > riffEnd || paddedChunkSize > MAXLONG)
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

        if (chunkType == fourcc)
        {
            chunkSize = chunkDataSize;
            chunkDataPosition = chunkDataPositionLocal;
            return S_OK;
        }

        hr = SetFilePointerChecked_(hfile, static_cast<LONG>(paddedChunkSize), FILE_CURRENT);
        if (FAILED(hr)) return hr;
        bytesRead = static_cast<DWORD>(chunkEnd);
    }
}

HRESULT ReadChunkData_(HANDLE hfile, LPVOID buffer, DWORD bufferSize, DWORD bufferOffset)
{
    if (buffer == nullptr && bufferSize != 0)
        return E_INVALIDARG;

    const HRESULT hr = SetFilePointerChecked_(hfile, static_cast<LONG>(bufferOffset), FILE_BEGIN);
    if (FAILED(hr)) return hr;
    return ReadFileExactly_(hfile, buffer, bufferSize);
}
void CoreAudio::CreateAudioSource(std::shared_ptr<CoreAudioBuffer> buffer,
	IXAudio2SourceVoice** sourceVoice, CoreSoundType type)
{
	// BGM,SEの送り先設定
	XAUDIO2_SEND_DESCRIPTOR send[CoreSoundType::EnumCount] = { { 0, submixVoices[BGM] }, { 0, submixVoices[SE] } };
	XAUDIO2_VOICE_SENDS sends[CoreSoundType::EnumCount] = { {1, &send[BGM]}, {1, &send[SE]} };

	HRESULT hr = xaudio2->CreateSourceVoice(sourceVoice, (WAVEFORMATEX*)&buffer->wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sends[type], nullptr);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void CoreAudio::Initialize()
{
	HRESULT hr;

	//XAudio2の初期化
	hr = XAudio2Create(xaudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = xaudio2->CreateMasteringVoice(&masterVoice);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// BGMとSE用のサブミキサー・ボイスを作成（ステレオ: 2チャンネル）
	hr = xaudio2->CreateSubmixVoice(&submixVoices[BGM], 2, 44100);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = xaudio2->CreateSubmixVoice(&submixVoices[SE], 2, 44100);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// X3DAudio の初期化
	//C3DAudio::Initialize();
}

CoreAudio::~CoreAudio()
{
	masterVoice->DestroyVoice();
	submixVoices[0]->DestroyVoice();
	submixVoices[1]->DestroyVoice();
	xaudio2->Release();
}

float CoreAudio::CoreAudioBuffer::GetDuration() const
{
	// 再生時間 = バッファサイズ(バイト) / 1秒あたりのバイト数
	const uint32_t byteRate = wfx.Format.nAvgBytesPerSec;
	// byteRate が 0 の場合、無限ループなどで再生時間が定義できない場合があるためガード
	if (byteRate == 0) return 0.0;
	// 秒数を返す
	return static_cast<float>(buffer.AudioBytes) / static_cast<float>(byteRate);
}

std::shared_ptr<CoreAudio::CoreAudioBuffer> CoreAudio::CoreAudioBuffer::GetResource(const std::wstring& filePath)
{
    //すでに存在していたらリソースを返す
    if (resources.contains(filePath) && !resources.at(filePath).expired())
    {
        return resources.at(filePath).lock();
    }

    const ScopedFileHandle file(CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
    if (file.Get() == INVALID_HANDLE_VALUE)
        return nullptr;

    DWORD chunkSize = 0;
    DWORD chunkPosition = 0;
    HRESULT hr = FindChunk_(file.Get(), 'FFIR'/*RIFF*/, chunkSize, chunkPosition);
    if (FAILED(hr) || chunkSize < sizeof(DWORD))
        return nullptr;

    DWORD fileType = 0;
    hr = ReadChunkData_(file.Get(), &fileType, sizeof(fileType), chunkPosition);
    if (FAILED(hr) || fileType != 'EVAW'/*WAVE*/)
        return nullptr;

    std::shared_ptr<CoreAudioBuffer> audioBuffer = std::make_shared<CoreAudioBuffer>();
    hr = FindChunk_(file.Get(), ' tmf'/*FMT*/, chunkSize, chunkPosition);
    constexpr DWORD minWaveFormatSize = sizeof(WAVEFORMATEX) - sizeof(WORD);
    if (FAILED(hr) || chunkSize < minWaveFormatSize || chunkSize > sizeof(audioBuffer->wfx))
        return nullptr;

    hr = ReadChunkData_(file.Get(), &audioBuffer->wfx, chunkSize, chunkPosition);
    if (FAILED(hr))
        return nullptr;

    // Keep PCM data locally until the complete read succeeds.
    hr = FindChunk_(file.Get(), 'atad'/*DATA*/, chunkSize, chunkPosition);
    if (FAILED(hr) || chunkSize == 0)
        return nullptr;

    auto data = std::make_unique<BYTE[]>(chunkSize);
    hr = ReadChunkData_(file.Get(), data.get(), chunkSize, chunkPosition);
    if (FAILED(hr))
        return nullptr;

    audioBuffer->buffer.AudioBytes = chunkSize;  //size of the audio buffer in bytes
    audioBuffer->buffer.pAudioData = data.release(); // CoreAudioBuffer owns this allocation.
    audioBuffer->buffer.Flags = XAUDIO2_END_OF_STREAM; // tell the source voice not to expect any data after this buffer

    resources[filePath] = audioBuffer;
    return audioBuffer;
}
std::shared_ptr<CoreStandaloneAudioSource> CoreAudio::PlayOneShot(const wchar_t* filePath, float volume)
{
	std::shared_ptr<CoreStandaloneAudioSource> source = std::make_shared<CoreStandaloneAudioSource>(filePath);
	source->SetVolume(volume);
	if (isMutedBySystem)	// ミュートだったら
		return source;
	source->Play();
	audioSources.emplace_back(source);
	return source;
}

std::shared_ptr<CoreStandaloneAudioSource> CoreAudio::PlayOneShot(const std::string& filePath, float volume)
{
	std::filesystem::path path(filePath);
	return PlayOneShot(path.c_str(), volume);
}

void CoreAudio::Update(float deltaTime)
{
	//削除処理
	for (auto& source : audioSources)
	{
		if (!source->IsPlaying())
		{
			erases.emplace_back(source);
		}
	}
	audioSources.erase(std::remove_if(audioSources.begin(), audioSources.end(),
		[&](const auto& audioSource) {
			return std::find(erases.begin(), erases.end(), audioSource) != erases.end();
		}),
		audioSources.end());
	erases.clear();
}

CoreStandaloneAudioSource::CoreStandaloneAudioSource(const wchar_t* filePath)
{
	this->type = std::wstring(filePath).find(L"BGM") != std::wstring::npos ? CoreSoundType::BGM : CoreSoundType::SE;
	sptrBuffer = CoreAudio::CoreAudioBuffer::GetResource(filePath);
	CoreAudio::CreateAudioSource(sptrBuffer, &sourceVoice, type);
}
CoreStandaloneAudioSource::~CoreStandaloneAudioSource()
{
	Stop();
	sourceVoice->DestroyVoice();
}

void CoreStandaloneAudioSource::Play(bool loop)
{
	_ASSERT_EXPR(sourceVoice, L"ソースが設定されていません。");

	HRESULT hr;

	XAUDIO2_VOICE_STATE voiceState = {};
	sourceVoice->GetState(&voiceState);

	if (voiceState.BuffersQueued)
	{
		//Stop(false, 0);
		return;
	}

	XAUDIO2_BUFFER* pBuffer = &sptrBuffer->buffer;

	pBuffer->LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
	hr = sourceVoice->SubmitSourceBuffer(pBuffer);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = sourceVoice->Start(0);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void CoreStandaloneAudioSource::Stop(bool playTails)
{
	XAUDIO2_VOICE_STATE voiceState{};
	sourceVoice->GetState(&voiceState);

	if (!voiceState.BuffersQueued)
	{
		return;
	}

	HRESULT hr;
	hr = sourceVoice->Stop(playTails ? XAUDIO2_PLAY_TAILS : 0);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = sourceVoice->FlushSourceBuffers();
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void CoreStandaloneAudioSource::SetVolume(float volume)
{
	HRESULT hr = sourceVoice->SetVolume(volume);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

float CoreStandaloneAudioSource::GetVolume()
{
	float volume;
	sourceVoice->GetVolume(&volume);
	return volume;
}

// ピッチを設定する
void CoreStandaloneAudioSource::SetPitch(float pitch)
{
	this->pitch = pitch;

	if (sourceVoice)
	{
		sourceVoice->SetFrequencyRatio(pitch);
	}
}
// ピッチを取得する
float CoreStandaloneAudioSource::GetPitch() const
{
	return pitch;
}

bool CoreStandaloneAudioSource::IsPlaying()
{
	XAUDIO2_VOICE_STATE voiceState{};
	sourceVoice->GetState(&voiceState);
	return voiceState.BuffersQueued > 0;
}

void CoreStandaloneAudioSource::DrawGUI()
{
#ifdef USE_IMGUI
	if (ImGui::Button("Play")) {
		Play();
	}
	if (ImGui::Button("Stop")) {
		Stop();
	}

	static float volume;
	if (sourceVoice)
	{
		sourceVoice->GetVolume(&volume);
		if (ImGui::SliderFloat("Volume", &volume, 0, 1)) {
			sourceVoice->SetVolume(volume);
		}
	}

	ImGui::Separator();

	if (ImGui::SliderFloat("MasterVolume", &masterVolume, 0, 1)) {
		CoreAudio::SetMasterVolume(masterVolume);
	}
	if (ImGui::SliderFloat("BgmVolume", &bgmVolume, 0, 1)) {
		CoreAudio::SetBgmVolume(bgmVolume);
	}
	if (ImGui::SliderFloat("SeVolume", &seVolume, 0, 1)) {
		CoreAudio::SetSeVolume(seVolume);
	}
#endif // USE_IMGUI
}