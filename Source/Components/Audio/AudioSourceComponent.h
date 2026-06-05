#pragma once
#include "Engine/Audio/Audio.h"
#include "Components/Base/Component.h"

/// シーン上に配置して音声を再生するコンポーネント
class AudioSourceComponent : public Component
{
public:
    AudioSourceComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :Component(name, owner) {}
    ~AudioSourceComponent() override;

    void Initialize() override {}
    void Tick(float deltaTime) override;
public:
    /// 再生する音声ファイルを設定
    void SetSource(const std::wstring& filePath);

    /// 再生開始
    void Play();

    /// 停止
    void Stop(bool playTails = true);

    /// 一時停止
    void Pause();

    /// 再開
    void Resume();

public:
    /// 音量設定
    void SetVolume(float volume);

    /// 音量取得
    float GetVolume() const;

    /// パン設定
    void SetPan(float pan);

    /// パン取得
    float GetPan() const { return pan; }

    /// ピッチ設定
    void SetPitch(float pitch);

    /// ピッチ取得
    float GetPitch() const { return pitch; }

public:
    /// ループ設定
    void SetLoop(bool enable) { this->isLooping = enable; };

    /// ループ状態取得
    bool IsLoop() const { return isLooping; }

    /// ループ範囲設定
    void SetLoopRange(float begin, float length);

public:
    /// 再生中か
    bool IsPlaying() const;

    /// 現在の再生時間
    float GetPlaybackTime() const;

    /// 前フレームからの再生時間差分
    float GetPlaybackDeltaTime();

    /// キュー中バッファ数取得
    uint32_t GetBufferQueueCount();

    /// 音声全体の長さ
    float GetTotalDuration() const;

public:
    /// BGM扱いにする
    void SetBgm(bool enable) { this->isBgm = enable; }

    /// インスペクタ描画
    void DrawImGuiInspector() override;

private:

    /// 音声ファイルパス
    std::wstring sourceFilePath;

    /// サウンド種別
    CoreSoundType soundType = CoreSoundType::SE;

    /// ループ再生するか
    bool isLooping = false;

    /// 3D音声として扱うか
    bool use3DAudio = false;

    /// BGMの場合はポーズなどで音を一時停止しないためのフラグ
    bool isBgm = false;

    /// システムポーズによる停止中か
    bool wasSystemPaused = false;

private:
    /// パン (-1.0 ~ 1.0)
    float pan = 0.0f;

    /// ピッチ
    float pitch = 1.0f;

private:

    /// 前回再生サンプル位置
    float lastSamplesPlayed = 0.0f;

private:

    /// オーディオバッファ
    std::shared_ptr<CoreAudio::CoreAudioBuffer> audioBuffer;

    /// XAudio2 ソースボイス
    IXAudio2SourceVoice* sourceVoice = nullptr;

private:

    static inline float masterVolume = 1.0f;
    static inline float bgmVolume = 1.0f;
    static inline float seVolume = 1.0f;

private:
    friend class C3DAudio;
};