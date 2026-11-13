#pragma once
#include "Engine/Audio/Audio.h"
#include "Components/Base/Component.h"

class AudioSourceComponent : public Component
{
public:
    AudioSourceComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :Component(name, owner) {}
    ~AudioSourceComponent() override;

    void Initialize() override {}
    void Tick(float deltaTime) override;
    void DrawImGuiInspector() override;

    void SetSource(const std::wstring& filePath);

    void Play();
    void RestartFromBeginning();

    void Stop(bool playTails = true);

    void Pause();

    void Resume();

    void SetVolume(float volume);
    float GetVolume() const;
    void SetPan(float pan);
    float GetPan() const { return pan; }
    void SetPitch(float pitch);
    float GetPitch() const { return pitch; }

    void SetLoop(bool enable) { this->isLooping = enable; };
    bool IsLoop() const { return isLooping; }
    void SetLoopRange(float begin, float length);
    bool IsPlaying() const;

    /// 現在の再生時間
    float GetPlaybackTime() const;
    /// 前フレームからの再生時間差分
    float GetPlaybackDeltaTime();
    /// キュー中バッファ数取得
    uint32_t GetBufferQueueCount();
    /// 音声全体の長さ
    float GetTotalDuration() const;
    /// BGM扱いにする
    void SetBgm(bool enable) { this->isBgm = enable; }

private:
    /// 音声ファイルパス
    std::wstring sourceFilePath;
    /// サウンド種別
    CoreSoundType soundType = CoreSoundType::SE;
    bool isLooping = false;
    /// 3D音声として扱うか
    bool use3DAudio = false;
    /// BGMの場合はポーズなどで音を一時停止しないためのフラグ
    bool isBgm = false;
    /// システムポーズによる停止中か
    bool wasSystemPaused = false;
    /// パン (-1.0 ~ 1.0)
    float pan = 0.0f;
    bool hasPanOverride = false;
    /// ピッチ
    float pitch = 1.0f;
    /// 前回再生サンプル位置
    float lastSamplesPlayed = 0.0f;
    /// オーディオバッファ
    std::shared_ptr<CoreAudio::CoreAudioBuffer> audioBuffer;
    /// XAudio2 ソースボイス
    IXAudio2SourceVoice* sourceVoice = nullptr;

    static inline float masterVolume = 1.0f;
    static inline float bgmVolume = 1.0f;
    static inline float seVolume = 1.0f;

    friend class C3DAudio;
};