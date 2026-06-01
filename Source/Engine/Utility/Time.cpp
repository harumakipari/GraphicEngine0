#include "pch.h"
#include "Time.h"
#include <imgui.h>

Time::Time()
{
	LONGLONG counts_per_sec;
	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&counts_per_sec));
	secondsPerCount = 1.0 / static_cast<double>(counts_per_sec);

	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&thisTime));
	baseTime = thisTime;
	lastTime = thisTime;
}

void Time::Reset() // Call before message loop.
{
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&thisTime));
	baseTime = thisTime;
	lastTime = thisTime;

	stopTime = 0;
	stopped = false;
}

void Time::Start() // Call when unpaused.
{
	LONGLONG start_time;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&start_time));

	// Accumulate the time elapsed between stop and start pairs.
	//
	//                     |<-------d------->|
	// ----*---------------*-----------------*------------> time
	//  baseTime       stopTime        start_time     
	if (stopped)
	{
		pausedTime += (start_time - stopTime);
		lastTime = start_time;
		stopTime = 0;
		stopped = false;
	}
}

void Time::Stop()
{
	if (!stopped)
	{
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&stopTime));
		stopped = true;
	}
}

void Time::Tick() // 毎フレーム呼び出す。
{
	if (stopped)
	{
		deltaTime = 0.0;
		return;
	}

	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&thisTime));
	// 現在のフレームと前のフレームとの時間差。
	deltaTime = (thisTime - lastTime) * secondsPerCount * static_cast<double>(timeScale);
	unscaledDeltaTime = (thisTime - lastTime) * secondsPerCount;

	// 次のフレームの準備をする。
	lastTime = thisTime;

	// 負の値を強制的に排除する。
	// DXSDKのCDXUTTimerの説明によると、プロセッサが省電力モードに移行した場合や、別のプロセッサに割り当て直された場合、
	// mDeltaTimeが負の値になる可能性がある。
	if (deltaTime < 0.0f)
	{
		deltaTime = 0.0f;
	}

	// スロー処理
	if (slowTimer > 0.0f)
	{
		slowTimer -= static_cast<float>(unscaledDeltaTime);

		if (slowTimer <= 0.0f)
		{
			//timeScale = 1.0f;

			timeScale = std::lerp(timeScale, 1.0f, 0.2f);

			if (fabs(timeScale - 1.0f) < 0.01f)
			{
				timeScale = 1.0f;
			}
		}
	}
	//else
	//{
	//	timeScale = std::lerp(timeScale, 1.0f, 0.2f);

	//	if (fabs(timeScale - 1.0f) < 0.01f)
	//	{
	//		timeScale = 1.0f;
	//	}
	//}
}

float Time::TimeStamp() const  // in seconds
{
	// If we are stopped, do not count the time that has passed since we stopped.
	// Moreover, if we previously already had a pause, the distance 
	// stopTime - baseTime includes paused time, which we do not want to count.
	// To correct this, we can subtract the paused time from mStopTime:  
	//
	//                     |<--pausedTime-->|
	// ----*---------------*-----------------*------------*------------*------> time
	//  baseTime       stopTime        start_time     stopTime    thisTime

	if (stopped)
	{
		return static_cast<float>(((stopTime - pausedTime) - baseTime) * secondsPerCount);
	}

	// The distance thisTime - mBaseTime includes paused time,
	// which we do not want to count.  To correct this, we can subtract 
	// the paused time from thisTime:  
	//
	//  (thisTime - pausedTime) - baseTime 
	//
	//                     |<--pausedTime-->|
	// ----*---------------*-----------------*------------*------> time
	//  baseTime       stopTime        start_time     thisTime
	else
	{
		return static_cast<float>(((thisTime - pausedTime) - baseTime) * secondsPerCount);
	}
}

void Time::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::Begin(U8("Time")))
	{
		ImGui::Text("DeltaTime : %.4f", Time::DeltaTime());
		ImGui::Text("Unscaled : %.4f", Time::UnscaledDeltaTime());

		ImGui::Separator();

		ImGui::SliderFloat(
			"Time Scale",
			&Time::timeScale,
			0.0f,   // 完全停止
			3.0f,   // 3倍速
			"%.2f"
		);

		if (ImGui::Button("Reset TimeScale"))
		{
			Time::timeScale = 1.0f;
		}
	}
	ImGui::End();
#endif
}