#pragma once

#include <vector>
#include <DirectXMath.h>


struct BlendSample
{
    size_t clip = 0;
    float weight = 0.0f;
};

struct BlendResult
{
    BlendSample samples[4];
    int count = 0;
};

class BlendSpace
{
public:
    struct Sample
    {
        size_t clip;
        float angle = 0.0f;
        float phaseOffset = 0.0f;
    };

    struct BlendResult
    {
        size_t clip;
        float weight;
        float phaseOffset;
    };
public:
    void AddAnimation(const size_t clip, const float angle, const float phaseOffset = 0.0f)
    {
        samples.push_back({ clip, angle, WrapPhase(phaseOffset) });

        std::sort(
            samples.begin(),
            samples.end(),
            [](const Sample& a, const Sample& b)
            {
                return a.angle < b.angle;
            }
        );
    }

    static float WrapPhase(const float phase)
    {
        if (!std::isfinite(phase))
            return 0.0f;

        return phase - std::floor(phase);
    }

    // インプットから2Dブレンド空間での重みを計算する
    std::vector<BlendResult> CalculateWeights(DirectX::XMFLOAT2 input) ;

private:
    std::vector<Sample> samples;
};