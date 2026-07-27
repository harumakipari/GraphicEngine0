#include "pch.h"
#include "BlendSpaceAnimation.h"

// インプットから2Dブレンド空間での重みを計算する
std::vector<BlendSpace::BlendResult> BlendSpace::CalculateWeights(const DirectX::XMFLOAT2 input) const
{
#if 0
    std::vector<BlendResult> result;
    float totalWeight = 0.0f;
    for (auto& sample : samples)
    {
        float dx = input.x - sample.position.x;
        float dy = input.y - sample.position.y;
        float distance = sqrtf(dx * dx + dy * dy);
        // 0除算防止
        float weight = 1.0f / (distance + 0.001f);
        result.push_back({ sample.clip,weight });
        totalWeight += weight;
    }
    // 正規化
    for (auto& r : result)
    {
        r.weight /= totalWeight;
    }
    return result;
#else
    std::vector<BlendResult> result;

    if (samples.empty())
        return result;


    // 近い順に2つ探す
    float minDistanceA = FLT_MAX;
    float minDistanceB = FLT_MAX;

    const Sample* sampleA = nullptr;
    const Sample* sampleB = nullptr;


    for (const auto& sample : samples)
    {
        float dx = input.x - sample.position.x;
        float dy = input.y - sample.position.y;

        float distance =
            sqrtf(dx * dx + dy * dy);


        if (distance < minDistanceA)
        {
            // 今まで1位だったものを2位へ
            minDistanceB = minDistanceA;
            sampleB = sampleA;

            // 新しい1位
            minDistanceA = distance;
            sampleA = &sample;
        }
        else if (distance < minDistanceB)
        {
            minDistanceB = distance;
            sampleB = &sample;
        }
    }


    // 一番近いものしかない場合
    if (sampleA && sampleB == nullptr)
    {
        result.push_back(
            {
                sampleA->clip,
                1.0f
            });

        return result;
    }


    // 距離による線形補間
    float totalDistance =
        minDistanceA + minDistanceB;


    float weightA =
        minDistanceB / totalDistance;

    float weightB =
        minDistanceA / totalDistance;


    result.push_back(
        {
            sampleA->clip,
            weightA
        });


    result.push_back(
        {
            sampleB->clip,
            weightB
        });


    return result;
#endif // 0

}
