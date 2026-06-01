#pragma once
#include <chrono>
#include <string>
#include "Logger.h"

#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
#define CONCAT_IMPL(x,y) x##y
#define CONCAT(x,y) CONCAT_IMPL(x,y)
#define PROFILE_SCOPE(name) ScopedTimer CONCAT(timer_, __LINE__)(name)
class ScopedTimer
{
public:

    ScopedTimer(const std::string& name)
        : name(name)
    {
        start = std::chrono::high_resolution_clock::now();
    }

    ~ScopedTimer()
    {
        auto end = std::chrono::high_resolution_clock::now();

        double time =
            std::chrono::duration<double>(end - start).count();

        Logger::Log("[Timer] " + name + " : " + std::to_string(time) + " sec");
    }

private:
    std::string name;
    std::chrono::high_resolution_clock::time_point start;
};