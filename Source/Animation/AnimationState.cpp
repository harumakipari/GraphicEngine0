#include "pch.h"
#include "AnimationState.h"

float AnimationCurve::Evaluate(const float t) const
{
    if (keys.empty())
        return 1.0f;

    if (t <= keys.front().time)
        return keys.front().value;

    if (t >= keys.back().time)
        return keys.back().value;

    for (size_t i = 0; i + 1 < keys.size(); i++)
    {
        const auto& a = keys[i];
        const auto& b = keys[i + 1];

        if (t >= a.time && t <= b.time)
        {
            float s = (t - a.time) / (b.time - a.time);
            return std::lerp(a.value, b.value, s);
        }
    }

    return 1.0f;
}
