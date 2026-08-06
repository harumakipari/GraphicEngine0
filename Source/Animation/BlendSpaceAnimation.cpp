#include "pch.h"
#include "BlendSpaceAnimation.h"

std::vector<BlendSpace::BlendResult> BlendSpace::CalculateWeights(const DirectX::XMFLOAT2 input)
{
    std::vector<BlendResult> result;

    if (samples.empty() || !std::isfinite(input.x) || !std::isfinite(input.y))
        return result;

    const float inputAngle =
        DirectX::XMConvertToDegrees(atan2f(input.x, input.y));

    if (samples.size() == 1)
    {
        result.push_back({
            samples.front().clip,
            1.0f,
            samples.front().phaseOffset
            });
        return result;
    }

    for (size_t i = 0; i + 1 < samples.size(); ++i)
    {
        const Sample& sampleA = samples[i];
        const Sample& sampleB = samples[i + 1];

        if (sampleA.angle <= inputAngle && inputAngle <= sampleB.angle)
        {
            const float angleRange = sampleB.angle - sampleA.angle;
            if (fabsf(angleRange) <= FLT_EPSILON)
            {
                result.push_back({
                    sampleA.clip,
                    1.0f,
                    sampleA.phaseOffset
                    });
                return result;
            }

            const float t = std::clamp(
                (inputAngle - sampleA.angle) / angleRange,
                0.0f,
                1.0f);

            result.push_back({ sampleA.clip, 1.0f - t, sampleA.phaseOffset });
            result.push_back({ sampleB.clip, t, sampleB.phaseOffset });
            return result;
        }
    }

    // Circular interval across +180/-180.
    const Sample& sampleA = samples.back();
    const Sample& sampleB = samples.front();
    const float wrappedInputAngle =
        inputAngle < sampleB.angle ? inputAngle + 360.0f : inputAngle;
    const float wrappedAngleB = sampleB.angle + 360.0f;
    const float angleRange = wrappedAngleB - sampleA.angle;

    if (std::isfinite(wrappedInputAngle) && angleRange > FLT_EPSILON)
    {
        const float t = std::clamp(
            (wrappedInputAngle - sampleA.angle) / angleRange,
            0.0f,
            1.0f);
        result.push_back({ sampleA.clip, 1.0f - t, sampleA.phaseOffset });
        result.push_back({ sampleB.clip, t, sampleB.phaseOffset });
        return result;
    }

    result.push_back({
        samples.back().clip,
        1.0f,
        samples.back().phaseOffset
        });
    return result;
}