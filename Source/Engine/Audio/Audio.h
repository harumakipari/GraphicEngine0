#pragma once
#include <map>
#include <xaudio2.h>
#include <mmreg.h>

#include <wrl.h>
#include <cstdint>

#include "Engine/Utility/Win32Utils.h"


 /// サウンド種別
enum CoreSoundType :uint8_t
{
    BGM,
    SE,
    EnumCount
};
class CoreStandaloneAudioSource;

/// オーディオ再生管理クラス
class CoreAudio
{
public:
    /// 読み込み済み音声データ
    class CoreAudioBuffer
    {
    public:
        CoreAudioBuffer() = default;

        ~CoreAudioBuffer()
        {
            delete[] buffer.pAudioData;
        }

    public:
        /// 音声の総再生時間取得
        float GetDuration() const;

        /// 音声リソース取得
        static std::shared_ptr<CoreAudioBuffer> GetResource(const std::wstring& filePath);

    public:
        /// フォーマット情報
        WAVEFORMATEXTENSIBLE wfx = { 0 };

        /// XAudio2 再生用バッファ
        XAUDIO2_BUFFER buffer = { 0 };

    private:
        friend class Audio;
       // 参照済みリソースの弱参照キャッシュ。
        static inline std::map<std::wstring, std::weak_ptr<CoreAudioBuffer>> resources;
    };

    static void Initialize();

    static void SetMasterVolume(float volume) { masterVoice->SetVolume(volume); }

    static void GetMasterVolume(float& volume) { masterVoice->GetVolume(&volume); }

    static void SetBgmVolume(float volume) { submixVoices[BGM]->SetVolume(volume); }

    static void GetBgmVolume(float& volume) { submixVoices[BGM]->GetVolume(&volume); }

    static void SetSeVolume(float volume) { submixVoices[SE]->SetVolume(volume); }

    static void GetSeVolume(float& volume) { submixVoices[SE]->GetVolume(&volume); }

    virtual ~CoreAudio();

    // システム全体のミュート設定
    static void SetMutedBySystem(bool enabled)
    {
        isMutedBySystem = enabled;
        if (isMutedBySystem)
        {
            Logger::Log(U8("音声を無効化した"));
        }
        else
        {
            Logger::Log(U8("音声を有効化した"));
        }
    }

    static bool GetMutedBySystem()
    {
        return isMutedBySystem;
    }

    // システム全体のミュート設定
    static void SetSystemPaused(bool enabled)
    {
        isSystemPaused = enabled;
        if (isSystemPaused)
        {
            Logger::Log(U8("ポーズのため音声を無効化した"));
        }
        else
        {
            Logger::Log(U8("ポーズのため音声を有効化した"));
        }
    }

    static bool GetSystemPaused()
    {
        return isSystemPaused;
    }

public:

    static std::shared_ptr<CoreStandaloneAudioSource> PlayOneShot(const wchar_t* filePath, float volume = 1.0f);
    static std::shared_ptr<CoreStandaloneAudioSource> PlayOneShot(const std::string& filePath, float volume = 1.0f);

 
    static void Update(float deltaTime);
  
    static void ClearAll()
    {
        audioSources.clear();
        erases.clear();
    }


private:
    // アクティブな一時ソース群
    static inline std::vector<std::shared_ptr<CoreStandaloneAudioSource>> audioSources;
    // 破棄予定の一時ソース群
    static inline std::vector<std::shared_ptr<CoreStandaloneAudioSource>> erases;

    static inline bool isMutedBySystem = false;	
    static inline bool isSystemPaused = false;	

private:
    friend class AudioSourceComponent;
    friend class CoreStandaloneAudioSource;
  
    static void CreateAudioSource(std::shared_ptr<CoreAudioBuffer> buffer, IXAudio2SourceVoice** sourceVoice, CoreSoundType type);

private:
    friend class AudioListener;
    friend class C3DAudio;
    static inline Microsoft::WRL::ComPtr<IXAudio2> xaudio2;
    static inline IXAudio2MasteringVoice* masterVoice = nullptr;
    static inline IXAudio2SubmixVoice* submixVoices[CoreSoundType::EnumCount];
};

class CoreStandaloneAudioSource
{
public:
  
    CoreStandaloneAudioSource(const wchar_t* filePath);
    ~CoreStandaloneAudioSource();

    void Play(bool loop = false);
    void Stop(bool playTails = true);
    void SetVolume(float volume);
    float GetVolume();
    void SetPitch(float pitch);
    float GetPitch()const;
    bool IsPlaying();
    void DrawGUI();

private:
    CoreSoundType type;

    std::shared_ptr<CoreAudio::CoreAudioBuffer> sptrBuffer;
    IXAudio2SourceVoice* sourceVoice = nullptr;

    static inline float masterVolume = 1.0f;
    static inline float bgmVolume = 1.0f;
    static inline float seVolume = 1.0f;
    static inline float pitch = 1.0f;
};