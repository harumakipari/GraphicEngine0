#include "pch.h"
#include "Audio.h"

#include <x3daudio.h>

#include <windows.h>
#include <winerror.h>

IXAudio2* AudioDevice::xaudio2 = NULL;
IXAudio2MasteringVoice* AudioDevice::masterVoice = NULL;


HRESULT FindChunk(HANDLE hfile, DWORD fourcc, DWORD& chunk_size, DWORD& chunk_data_position)
{
	HRESULT hr = S_OK;

	if (INVALID_SET_FILE_POINTER == SetFilePointer(hfile, 0, NULL, FILE_BEGIN))
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	DWORD chunkType;
	DWORD chunkDataSize;
	DWORD riffDataSize = 0;
	DWORD fileType;
	DWORD bytesRead = 0;
	DWORD offset = 0;

	while (hr == S_OK)
	{
		DWORD number_of_bytes_read;
		if (0 == ReadFile(hfile, &chunkType, sizeof(DWORD), &number_of_bytes_read, NULL))
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}

		if (0 == ReadFile(hfile, &chunkDataSize, sizeof(DWORD), &number_of_bytes_read, NULL))
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}

		switch (chunkType)
		{
		case 'FFIR'/*RIFF*/:
			riffDataSize = chunkDataSize;
			chunkDataSize = 4;
			if (0 == ReadFile(hfile, &fileType, sizeof(DWORD), &number_of_bytes_read, NULL))
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
			}
			break;

		default:
			if (INVALID_SET_FILE_POINTER == SetFilePointer(hfile, chunkDataSize, NULL, FILE_CURRENT))
			{
				return HRESULT_FROM_WIN32(GetLastError());
			}
		}

		offset += sizeof(DWORD) * 2;

		if (chunkType == fourcc)
		{
			chunk_size = chunkDataSize;
			chunk_data_position = offset;
			return S_OK;
		}

		offset += chunkDataSize;

		if (bytesRead >= riffDataSize)
		{
			return S_FALSE;
		}
	}

	return S_OK;

}

HRESULT ReadChunkData(HANDLE hFile, LPVOID buffer, DWORD buffer_size, DWORD buffer_offset)
{
	HRESULT hr = S_OK;
	if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, buffer_offset, NULL, FILE_BEGIN))
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}
	DWORD numberOfBytesRead;
	if (0 == ReadFile(hFile, buffer, buffer_size, &numberOfBytesRead, NULL))
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
	}
	return hr;
}

AudioBuffer::AudioBuffer(const wchar_t* filename)
{
	HRESULT hr;

	// Open the file
	HANDLE hfile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (INVALID_HANDLE_VALUE == hfile)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	if (INVALID_SET_FILE_POINTER == SetFilePointer(hfile, 0, NULL, FILE_BEGIN))
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	DWORD chunkSize;
	DWORD chunkPosition;
	//check the file type, should be 'WAVE' or 'XWMA'
	FindChunk(hfile, 'FFIR'/*RIFF*/, chunkSize, chunkPosition);
	DWORD filetype;
	ReadChunkData(hfile, &filetype, sizeof(DWORD), chunkPosition);
	_ASSERT_EXPR(filetype == 'EVAW'/*WAVE*/, L"Only support 'WAVE'");

	FindChunk(hfile, ' tmf'/*FMT*/, chunkSize, chunkPosition);
	ReadChunkData(hfile, &wfx_, chunkSize, chunkPosition);

	//fill out the audio data buffer with the contents of the fourccDATA chunk
	FindChunk(hfile, 'atad'/*DATA*/, chunkSize, chunkPosition);
	BYTE* data = new BYTE[chunkSize];
	ReadChunkData(hfile, data, chunkSize, chunkPosition);

	buffer_.AudioBytes = chunkSize;  //size of the audio buffer in bytes
	buffer_.pAudioData = data;  //buffer containing audio data
	buffer_.Flags = XAUDIO2_END_OF_STREAM; // tell the source voice not to expect any data after this buffer
}

AudioBuffer::~AudioBuffer()
{
	delete[] buffer_.pAudioData;
}

AudioSourceVoice::AudioSourceVoice(std::shared_ptr<AudioBuffer>& audio_buffer) : audioBuffer_(audio_buffer)
{
	HRESULT hr;

	hr = AudioDevice::xaudio2->CreateSourceVoice(&sourceVoice_, (WAVEFORMATEX*)&audio_buffer->wfx_);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

AudioSourceVoice::~AudioSourceVoice()
{
	sourceVoice_->DestroyVoice();
}
void AudioSourceVoice::Play(int loop_count)
{
	HRESULT hr;

	XAUDIO2_VOICE_STATE voice_state = {};
	sourceVoice_->GetState(&voice_state);

	if (voice_state.BuffersQueued)
	{
		//stop(false, 0);
		return;
	}

	audioBuffer_->buffer_.LoopCount = loop_count;
	hr = sourceVoice_->SubmitSourceBuffer(&audioBuffer_->buffer_);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = sourceVoice_->Start(0);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}
void AudioSourceVoice::Stop(bool play_tails/*Continue emitting effect output after the voice is stopped. */)
{
	XAUDIO2_VOICE_STATE voice_state = {};
	sourceVoice_->GetState(&voice_state);
	if (!voice_state.BuffersQueued)
	{
		return;
	}

	HRESULT hr;
	hr = sourceVoice_->Stop(play_tails ? XAUDIO2_PLAY_TAILS : 0);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = sourceVoice_->FlushSourceBuffers();
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

}

void AudioSourceVoice::Volume(float volume)
{
	HRESULT hr = sourceVoice_->SetVolume(volume);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

bool AudioSourceVoice::Queuing()
{
	XAUDIO2_VOICE_STATE voice_state = {};
	sourceVoice_->GetState(&voice_state);
	return voice_state.BuffersQueued;
}

//#define SPEAKER_MONO (0x0)
//#define SPEAKER_FRONT_LEFT (0x1)
//#define SPEAKER_FRONT_RIGHT (0x2)
//#define SPEAKER_STEREO SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT
//#define SPEAKER_FRONT_CENTER (0x4)
//#define SPEAKER_LOW_FREQUENCY (0x8)
//#define SPEAKER_BACK_LEFT (0x10)
//#define SPEAKER_BACK_RIGHT (0x20)
//#define SPEAKER_5POINT1 SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT

void AudioSourceVoice::Pan(float pan_value/*pan_value of -1.0 indicates all left speaker, 1.0 is all right speaker, 0.0 is split between left and right*/)
{
	// https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--pan-a-sound
	float outputMatrix[8] = {};

	float left = 0.5f - pan_value / 2;
	float right = 0.5f + pan_value / 2;

	DWORD channel_mask;
	AudioDevice::masterVoice->GetChannelMask(&channel_mask);
	switch (channel_mask)
	{
	case SPEAKER_MONO:
		outputMatrix[0] = 1.0;
		break;
	case SPEAKER_STEREO:
	case SPEAKER_2POINT1:
	case SPEAKER_SURROUND:
		outputMatrix[0] = left;
		outputMatrix[1] = right;
		break;
	case SPEAKER_QUAD:
		outputMatrix[0] = outputMatrix[2] = left;
		outputMatrix[1] = outputMatrix[3] = right;
		break;
	case SPEAKER_4POINT1:
		outputMatrix[0] = outputMatrix[3] = left;
		outputMatrix[1] = outputMatrix[4] = right;
		break;
	case SPEAKER_5POINT1:
	case SPEAKER_7POINT1:
	case SPEAKER_5POINT1_SURROUND:
		outputMatrix[0] = outputMatrix[4] = left;
		outputMatrix[1] = outputMatrix[5] = right;
		break;
	case SPEAKER_7POINT1_SURROUND:
		outputMatrix[0] = outputMatrix[4] = outputMatrix[6] = left;
		outputMatrix[1] = outputMatrix[5] = outputMatrix[7] = right;
		break;
	}

	XAUDIO2_VOICE_DETAILS voiceDetails;
	sourceVoice_->GetVoiceDetails(&voiceDetails);

	XAUDIO2_VOICE_DETAILS masterVoiceDetails;
	AudioDevice::masterVoice->GetVoiceDetails(&masterVoiceDetails);

	HRESULT hr = sourceVoice_->SetOutputMatrix(NULL, voiceDetails.InputChannels, 2/*master_voice_details.InputChannels*/, outputMatrix);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}
