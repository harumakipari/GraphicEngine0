#pragma once
#include "Engine/Audio/CoreAudio.h"
#include "Components/Base/Component.h"

/**
 * @file
 * @brief オーディオソース（コンポーネント）。
 * @details XAudio2 を用いて音声の再生/停止/音量/ループ設定などを行います。
 *          ファイルパスから `Audio::AudioBuffer` を取得して再生します。
 */

 /**
  * @brief シーン上に配置して音声を再生するコンポーネント。
  */
class CoreAudioSourceComponent : public Component
{
public:
    /** @brief 既定コンストラクタ。*/
    CoreAudioSourceComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :Component(name, owner) {}
    /** @brief デストラクタ。*/
    ~CoreAudioSourceComponent() override;

    void Initialize() override {}

    /**
     * @brief フレーム更新。
     * @param deltaTime 経過時間（秒）。
     */
    void Tick(float deltaTime) override;

    void SetIsBgm(const bool isBgm) { this->isBgm = isBgm; }

    /**
     * @brief ソース（音声ファイル）を設定します。
     * @param filePath ファイルパス。
     */
    void SetSource(const std::wstring& filePath);

    /**
     * @brief 再生を開始します。
     */
    void Play();
    /**
     * @brief 再生を停止します。
     * @param playTails テイル（残響等）を再生してから停止するか。
     */
    void Stop(bool playTails = true);

    /**
     * @brief 再生を一時停止します。
     */
    void Pause();

    /**
     * @brief 再生を再開します。
     */
    void Resume();

    /**
     * @brief 音量を設定します。
     * @param volume 音量（0-1 目安）。
     */
    void SetVolume(float volume);

    /**
     * @brief 現在の音量を取得します。
     * @return 音量（0-1 目安）。
     */
    void GetVolume(float& volume);

    /**
     * @brief 再生中かを返します。
     * @return `true` で再生中、`false` で停止中。
     */
    bool IsPlaying();

    /**
     * @brief パンを設定します。
     * @param pan パン（-1.0 左、0 中央、1.0 右）。
     */
    void SetPan(float pan);

    /**
     * @brief 現在のパンを取得します。
     * @return パン（-1.0 左、0 中央、1.0 右）。
     */
    float GetPan() const { return m_Pan; }

    /**
     * @brief ループ再生の有無を設定します。
     * @param loop `true` でループ再生、`false` で一回再生。
     */
    void SetLoop(bool loop) { this->loop = loop; }

    /**
     * @brief ループ再生の有無を取得します。
     * @return `true` でループ再生、`false` で一回再生。
     */
    bool IsLoop() const { return loop; }

    /**
     * @brief ループ再生の範囲を設定します。
     * @param begin ループ開始位置（秒）。
     * @param length ループ長（begin からの長さ）。
     */
    void SetLoopOption(float begin, float length);

    /**
     * @brief 再生時間を取得します。
     * @return 再生時間（秒）。
     */
    float GetPlaybackTime() const;

    /**
     * @brief 再生のデルタ時間を取得します。
     * @return 再生デルタ時間（秒）。
     */
    float GetPlaybackDeltaTime();

    /**
     * @brief キューに残っているバッファ数を取得します。
     * @return バッファキュー数（再生中なら 0 より大きい）。
     */
    uint32_t GetBufferQueueCount();

    /**
     * @brief オーディオの総再生時間を取得します。
     * @return 再生時間（秒）。
     */
    float GetTotalDuration() const;


    /**
    * @brief ピッチ（再生速度）を設定します。
    * @param pitch 1.0f = 通常、1.2f = 速い、0.8f = 遅い
    */
    void SetPitch(float pitch);


    /**
    * @brief 現在のピッチを取得します。
    */
    float GetPitch() const { return m_Pitch; }

    /**
     * @brief インスペクタ用のプロパティ表示。
     */
    void DrawImGuiInspector() override;



private:
    /** @brief 設定中のファイルパス。*/
    std::wstring filePath;
    /** @brief サウンド種別（BGM/SE）。*/
    CoreSoundType type;
    /** @brief ループ設定。*/
    bool loop = false;
    /** @brief 3D 音源として扱うか。*/
    bool use3DAudio = false;
    /** @brief パン（-1.0 左、0 中央、1.0 右）。*/
    float m_Pan = 0.0f;
    /** @brief ピッチ（-1.0 左、0 中央、1.0 右）*/
    float m_Pitch = 1.0f;
    /** @brief 再生対象のオーディオバッファ。*/
    std::shared_ptr<CoreAudio::CoreAudioBuffer> m_SptrBuffer;
    friend class C3DAudio;
    /** @brief XAudio2 のソースボイス。*/
    IXAudio2SourceVoice* sourceVoice = nullptr;


    /** @brief マスター音量。*/
    static inline float masterVolume = 1.0f;
    /** @brief BGM 音量。*/
    static inline float bgmVolume = 1.0f;
    /** @brief SE 音量。*/
    static inline float seVolume = 1.0f;

private:
    float m_LastSamplesPlayed = 0.0f;
    bool wasSystemPaused = false;   // システムによる一時停止状態を記録するフラグ
    bool isBgm = false; // BGMの場合はポーズなどで音を一時停止しないためのフラグ
};