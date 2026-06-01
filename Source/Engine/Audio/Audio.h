#pragma once

#include <xaudio2.h>
#include <wrl.h>

#include <mmreg.h>
#include <memory>

#include "Engine/Utility/Win32Utils.h"

class AudioDevice
{
public:
	static IXAudio2* xaudio2;
	static IXAudio2MasteringVoice* masterVoice;

	static bool Initialize()
	{
		HRESULT hr = S_OK;

		hr = XAudio2Create(&xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = xaudio2->CreateMasteringVoice(&masterVoice);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		return hr == S_OK;
	}
	static void Uninitialize()
	{
		masterVoice->DestroyVoice();
		xaudio2->Release();
	}
};

class AudioSourceVoice;
class AudioBuffer
{
	WAVEFORMATEXTENSIBLE wfx_ = { 0 };
	XAUDIO2_BUFFER buffer_ = { 0 };
public:
	AudioBuffer(const wchar_t* filename);
	virtual ~AudioBuffer();
	AudioBuffer(const AudioBuffer& rhs) = delete;
	AudioBuffer& operator=(const AudioBuffer& rhs) = delete;
	AudioBuffer(AudioBuffer&&) noexcept = delete;
	AudioBuffer& operator=(AudioBuffer&&) noexcept = delete;

	friend class AudioSourceVoice;
};

class AudioSourceVoice
{
	IXAudio2SourceVoice* sourceVoice_;
	std::shared_ptr<AudioBuffer> audioBuffer_;

public:
	AudioSourceVoice(std::shared_ptr<AudioBuffer>& audio_buffer);
	virtual ~AudioSourceVoice();
	AudioSourceVoice(const AudioSourceVoice& rhs) = delete;
	AudioSourceVoice& operator=(const AudioSourceVoice& rhs) = delete;
	AudioSourceVoice(AudioSourceVoice&&) noexcept = delete;
	AudioSourceVoice& operator=(AudioSourceVoice&&) noexcept = delete;

	void Play(int loop_count = 0/*255 : XAUDIO2_LOOP_INFINITE*/);
	void Stop(bool play_tails = true);
	void Volume(float volume);
	void Pan(float pan_value/*pan_value of -1.0 indicates all left speaker, 1.0 is all right speaker, 0.0 is split between left and right*/);
	bool Queuing();
};

