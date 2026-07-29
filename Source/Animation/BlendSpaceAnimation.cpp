#include "pch.h"
#include "BlendSpaceAnimation.h"

// インプットから2Dブレンド空間での重みを計算する
std::vector<BlendSpace::BlendResult> BlendSpace::CalculateWeights(const DirectX::XMFLOAT2 input)
{
#if 1
    std::vector<BlendResult> result;

    if (samples.empty())
        return result;

    float inputAngle = DirectX::XMConvertToDegrees(atan2f(input.x, fabsf(input.y)));
    float minDistanceA = FLT_MAX;
    float minDistanceB = FLT_MAX;

    const Sample* sampleA = nullptr;
    const Sample* sampleB = nullptr;


    for (const auto& sample : samples)
    {
        float distance = fabsf(inputAngle - sample.angle);

        if (distance < minDistanceA)
        {
            minDistanceB = minDistanceA;
            sampleB = sampleA;

            minDistanceA = distance;
            sampleA = &sample;
        }
        else if (distance < minDistanceB)
        {
            minDistanceB = distance;
            sampleB = &sample;
        }
    }

    if (sampleA && sampleB == nullptr)
    {
        result.push_back({ sampleA->clip,1.0f });
        return result;
    }

    float total = minDistanceA + minDistanceB;

    result.push_back({
        sampleA->clip,
        minDistanceB / total
        });

    result.push_back({
        sampleB->clip,
        minDistanceA / total
        });

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
