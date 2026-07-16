#pragma once

#include <vector>
#include <DirectXMath.h>


class BlendSpace
{
public:
    struct Sample
    {
        size_t clip;
        DirectX::XMFLOAT2 position;
    };

    struct BlendResult
    {
        size_t clip;
        float weight;
    };
public:

    void AddAnimation(const size_t clip, const DirectX::XMFLOAT2 position)
    {
        samples.push_back({ clip, position });
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