#pragma once

#include <windows.h>

class Time
{
public:
    Time();
    ~Time() = default;
    Time(const Time&) = delete;
    Time& operator=(const Time&) = delete;
    Time(Time&&) noexcept = delete;
    Time& operator=(Time&&) noexcept = delete;

    // Returns the total time elapsed since Reset() was called, NOT counting any
    // time when the clock is stopped.
    float TimeStamp() const; // in seconds

    void Reset(); // Call before message loop.

    void Start(); // Call when unpaused.

    void Stop(); // Call when paused.

    void Tick(); // Call every frame.

    static void DrawImGui();

public:
    static float DeltaTime()   // in seconds
    {
        return static_cast<float>(deltaTime);
    }

    static float UnscaledDeltaTime()
    {
        return static_cast<float>(unscaledDeltaTime);
    }

    // スロー再生　scale倍にスローにしてduration後に戻る
    static void SetSlow(float scale, float duration)
    {
        timeScale = scale;
        slowTimer = duration;
    }

    static inline float timeScale{ 1.0f };

private:

    static inline double deltaTime{ 0.0f };
    static inline double unscaledDeltaTime{ 0.0f };

    static inline float slowTimer = 0.0f;

private:
    double secondsPerCount{ 0.0 };

    LONGLONG baseTime{ 0LL };
    LONGLONG pausedTime{ 0LL };
    LONGLONG stopTime{ 0LL };
    LONGLONG lastTime{ 0LL };
    LONGLONG thisTime{ 0LL };

    bool stopped{ false };
};

class benchmark
{
    LARGE_INTEGER ticks_per_second;
    LARGE_INTEGER start_ticks;
    LARGE_INTEGER current_ticks;

public:
    benchmark()
    {
        QueryPerformanceFrequency(&ticks_per_second);
        QueryPerformanceCounter(&start_ticks);
        QueryPerformanceCounter(&current_ticks);
    }
    ~benchmark() = default;
    benchmark(const benchmark&) = delete;
    benchmark& operator=(const benchmark&) = delete;
    benchmark(benchmark&&) noexcept = delete;
    benchmark& operator=(benchmark&&) noexcept = delete;

    void begin()
    {
        QueryPerformanceCounter(&start_ticks);
    }
    float end()
    {
        QueryPerformanceCounter(&current_ticks);
        return static_cast<float>(current_ticks.QuadPart - start_ticks.QuadPart) / static_cast<float>(ticks_per_second.QuadPart);
    }
};
