#pragma once

#include <windows.h>
#include <array>
#include <source_location>

class Time
{
public:
    struct ScaleChangeDebug
    {
        unsigned long long milliseconds = 0;
        float before = 1.0f;
        float after = 1.0f;
        const char* reason = "";
        const char* caller = "";
    };

public:
    Time();
    ~Time() = default;
    Time(const Time&) = delete;
    Time& operator=(const Time&) = delete;
    Time(Time&&) noexcept = delete;
    Time& operator=(Time&&) noexcept = delete;

    float TimeStamp() const;

    void Reset();

    void Start();

    void Stop();

    void Tick();

    static void DrawImGui();

    static float DeltaTime() { return static_cast<float>(deltaTime); }

    static float UnscaledDeltaTime() { return static_cast<float>(unscaledDeltaTime); }

    // スロー再生　scale倍にスローにしてduration後に戻る
    static void SetSlow(const float scale, const float duration, const std::source_location& caller = std::source_location::current())
    {
        ObserveScaleDebug("Direct write observed before SetSlow (caller unknown)");
        RecordScaleDebug(timeScale, scale, "SetSlow", caller.function_name());
        timeScale = scale;
        slowTimer = duration;
        observedScaleDebug = timeScale;
    }

    static const std::array<ScaleChangeDebug, 32>& GetScaleHistoryDebug() { return scaleHistoryDebug; }
    static size_t GetScaleHistoryCountDebug() { return scaleHistoryCountDebug; }
    static float GetSlowTimer() { return slowTimer; }
    static bool IsHitStopActive() { return timeScale == 0.0f && slowTimer > 0.0f; }

private:
    static void RecordScaleDebug(float before, float after, const char* reason, const char* caller);
    static void ObserveScaleDebug(const char* reason);

public:
    static inline float timeScale{ 1.0f };

private:
    static inline double deltaTime{ 0.0f };
    static inline double unscaledDeltaTime{ 0.0f };

    static inline float slowTimer = 0.0f;

    static std::array<ScaleChangeDebug, 32> scaleHistoryDebug;
    static inline size_t scaleHistoryCountDebug = 0;
    static inline float observedScaleDebug = 1.0f;

    double secondsPerCount{ 0.0 };

    LONGLONG baseTime{ 0LL };
    LONGLONG pausedTime{ 0LL };
    LONGLONG stopTime{ 0LL };
    LONGLONG lastTime{ 0LL };
    LONGLONG thisTime{ 0LL };

    bool stopped{ false };
};

class Benchmark
{
    LARGE_INTEGER ticksPerSecond;
    LARGE_INTEGER startTicks;
    LARGE_INTEGER currentTicks;

public:
    Benchmark()
    {
        QueryPerformanceFrequency(&ticksPerSecond);
        QueryPerformanceCounter(&startTicks);
        QueryPerformanceCounter(&currentTicks);
    }
    ~Benchmark() = default;
    Benchmark(const Benchmark&) = delete;
    Benchmark& operator=(const Benchmark&) = delete;
    Benchmark(Benchmark&&) noexcept = delete;
    Benchmark& operator=(Benchmark&&) noexcept = delete;

    void begin()
    {
        QueryPerformanceCounter(&startTicks);
    }
    float end()
    {
        QueryPerformanceCounter(&currentTicks);
        return static_cast<float>(currentTicks.QuadPart - startTicks.QuadPart) / static_cast<float>(ticksPerSecond.QuadPart);
    }
};
