#include "pch.h"
#include "BlendSpaceAnimation.h"

// インプットから2Dブレンド空間での重みを計算する
std::vector<BlendSpace::BlendResult> BlendSpace::CalculateWeights(const DirectX::XMFLOAT2 input) const
{
    std::vector<BlendResult> result;
    float totalWeight = 0.0f;
    for (auto& sample : samples)
    {
        float dx = input.x - sample.position.x;
        float dy = input.y - sample.position.y;
        float distance = sqrtf(dx * dx + dy * dy);
        // 0除算防止
        float weight = 1.0f / (distance + 0.001f);
        result.push_back({sample.clip,weight});
        totalWeight += weight;
    }
    // 正規化
    for (auto& r : result)
    {
        r.weight /= totalWeight;
    }
    return result;
}
