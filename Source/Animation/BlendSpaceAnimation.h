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

public:

    void AddAnimation(size_t clip, DirectX::XMFLOAT2 position)
    {
        samples.push_back({ clip, position });
    }

    const std::vector<Sample>& GetSamples() const
    {
        return samples;
    }
private:
    std::vector<Sample> samples;
};