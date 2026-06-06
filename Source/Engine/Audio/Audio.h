#pragma once
#include <map>
#include <xaudio2.h>
#include <mmreg.h>

#include <wrl.h>
#include <cstdint>

#include "Engine/Utility/Win32Utils.h"

/**
 * @file
 * @brief オーディオ（BGM/SE）の再生・管理を行うユーティリティ。
 * @details XAudio2 を用いた初期化、マスター/サブミックスの音量制御、ワンショット再生、
 *          オーディオソースの寿命管理などを提供します。
 */

 /// サウンド種別
enum CoreSoundType  :uint8_t
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
		/**
		 * @brief 参照済みリソースの弱参照キャッシュ。
		 * @note キーが `const wchar_t*` のため、同一パスでもポインタ値が異なると別キーとなります。
		 */
		static inline std::map<std::wstring, std::weak_ptr<CoreAudioBuffer>> resources;
	};

	/** @brief 初期化処理（XAudio2 のセットアップ）。*/
	static void Initialize();

	/** @brief マスターボリューム設定。*/
	static void SetMasterVolume(float volume) { masterVoice->SetVolume(volume); }

	/** @brief マスターボリューム取得。*/
	static void GetMasterVolume(float& volume) { masterVoice->GetVolume(&volume); }

	/** @brief BGM 全体のボリューム設定。*/
	static void SetBgmVolume(float volume) { submixVoices[BGM]->SetVolume(volume); }

	/** @brief BGM 全体のボリューム取得。*/
	static void GetBgmVolume(float& volume) { submixVoices[BGM]->GetVolume(&volume); }

	/** @brief SE 全体のボリューム設定。*/
	static void SetSeVolume(float volume) { submixVoices[SE]->SetVolume(volume); }

	/** @brief SE 全体のボリューム取得。*/
	static void GetSeVolume(float& volume) { submixVoices[SE]->GetVolume(&volume); }

	/** @brief デストラクタ。リソースを解放します。*/
	virtual ~CoreAudio();

    // システム全体のミュート設定（緊急で追加）
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

    // システム全体のミュート設定（緊急で追加）
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

	/**
	 * @brief 単発の効果音（または BGM）を再生します。
	 * @param filePath 音声ファイルパス。
	 * @param volume 再生音量（0-1 目安）。
	 */
	static void PlayOneShot(const wchar_t* filePath, float volume = 1.0f);

	/**
	 * @brief フレーム更新。
	 * @param deltaTime 経過時間（秒）。
	 * @details 再生完了した一時ソースの破棄などを行います。
	 */
	static void Update(float deltaTime);
	/**
	 * @brief すべての一時ソースを破棄します。
	 */
	static void ClearAll() {
		audioSources.clear();
		erases.clear();
	}


private:
	/** @brief アクティブな一時ソース群。*/
	static inline std::vector<std::shared_ptr<CoreStandaloneAudioSource>> audioSources;
	/** @brief 破棄予定の一時ソース群。*/
	static inline std::vector<std::shared_ptr<CoreStandaloneAudioSource>> erases;

	static inline bool isMutedBySystem = false;	// 緊急で追加
	static inline bool isSystemPaused = false;	// 緊急で追加

private:
	friend class AudioSourceComponent;
	friend class CoreStandaloneAudioSource;
	/**
	 * @brief XAudio2 のソースボイスを作成します。
	 * @param buffer 再生元のオーディオバッファ。
	 * @param sourceVoice 生成結果の出力先。
	 * @param type サウンド種別（BGM/SE）。
	 */
	static void CreateAudioSource(std::shared_ptr<CoreAudioBuffer> buffer, IXAudio2SourceVoice** sourceVoice, CoreSoundType type);

private:
	//#ifdef X3DAUDIO
	//	/** @brief X3DAudio のハンドル。*/
	//	static inline BYTE x3dAudioHandle;
	//#endif // X3DAUDIO
private:
	friend class AudioListener;
	friend class C3DAudio;
	/** @brief XAudio2 インスタンス。*/
	static inline Microsoft::WRL::ComPtr<IXAudio2> xaudio2;
	/** @brief マスターボイス。*/
	static inline IXAudio2MasteringVoice* masterVoice = nullptr;
	/** @brief サブミックスボイス（BGM/SE）。*/
	static inline IXAudio2SubmixVoice* submixVoices[CoreSoundType::EnumCount];
};

/**
 * @brief 単体で動作する簡易オーディオソース。
 * @details ファイルを指定して再生・停止・音量操作を行います。
 */
class CoreStandaloneAudioSource
{
public:
	/**
	 * @brief コンストラクタ。
	 * @param filePath 音声ファイルパス。
	 */
	CoreStandaloneAudioSource(const wchar_t* filePath);
	/** @brief デストラクタ。*/
	~CoreStandaloneAudioSource();

	/** @brief 再生を開始します。*/
	void Play(bool loop = false);
	/** @brief 再生を停止します。*/
	void Stop(bool playTails = true);
	/** @brief 音量を設定します。*/
	void SetVolume(float volume);
	/** @brief 現在の音量を取得します。*/
	float GetVolume();

	/** @brief 再生中かを返します。*/
	bool IsPlaying();

	/** @brief デバッグ GUI 描画。*/
	void DrawGUI();

private:
	/** @brief サウンド種別。*/
	CoreSoundType type;

	/** @brief 再生対象のオーディオバッファ。*/
	std::shared_ptr<CoreAudio::CoreAudioBuffer> sptrBuffer;
	/** @brief XAudio2 のソースボイス。*/
	IXAudio2SourceVoice* sourceVoice;

	/** @brief マスター音量。*/
	static inline float masterVolume = 1.0f;
	/** @brief BGM 音量。*/
	static inline float bgmVolume = 1.0f;
	/** @brief SE 音量。*/
	static inline float seVolume = 1.0f;
};