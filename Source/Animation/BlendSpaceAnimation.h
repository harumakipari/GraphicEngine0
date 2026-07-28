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
        float angle = 0.0f;;
    };

    struct BlendResult
    {
        size_t clip;
        float weight;
    };
public:
    void AddAnimation(const size_t clip, float angle)
    {
        samples.push_back({ clip, angle });
    }

    const std::vector<Sample>& GetSamples() const
    {
        return samples;
    }

    // インプットから2Dブレンド空間での重みを計算する
    std::vector<BlendResult> CalculateWeights(DirectX::XMFLOAT2 input) const;

private:
    std::vector<Sample> samples;
};