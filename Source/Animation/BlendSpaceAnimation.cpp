#include "pch.h"
#include "BlendSpaceAnimation.h"

// インプットから2Dブレンド空間での重みを計算する
std::vector<BlendSpace::BlendResult> BlendSpace::CalculateWeights(const DirectX::XMFLOAT2 input)
{
    std::vector<BlendResult> result;

    if (samples.empty())
        return result;

    float inputAngle = DirectX::XMConvertToDegrees(atan2f(input.x, fabsf(input.y)));


    // サンプルが1つしかない場合
    if (samples.size() == 1)
    {
        result.push_back({
            samples.front().clip,
            1.0f
            });

        return result;
    }

    // 最小角度より外側なら最小角度へ固定
    if (inputAngle <= samples.front().angle)
    {
        result.push_back({
            samples.front().clip,
            1.0f
            });

        return result;
    }

    // 最大角度より外側なら最大角度へ固定
    if (inputAngle >= samples.back().angle)
    {
        result.push_back({
            samples.back().clip,
            1.0f
            });

        return result;
    }

    // 入力角度を挟む隣接2サンプルを探す
    for (size_t i = 0; i + 1 < samples.size(); ++i)
    {
        const Sample& sampleA = samples[i];
        const Sample& sampleB = samples[i + 1];

        if (sampleA.angle <= inputAngle &&
            inputAngle <= sampleB.angle)
        {
            const float angleRange = sampleB.angle - sampleA.angle;

            // 同じ角度のサンプルが登録されていた場合のゼロ除算対策
            if (fabsf(angleRange) <= FLT_EPSILON)
            {
                result.push_back({
                    sampleA.clip,
                    1.0f
                    });

                return result;
            }

            // AからBに向かって0～1
            const float t =
                std::clamp(
                    (inputAngle - sampleA.angle) / angleRange,
                    0.0f,
                    1.0f
                );

            result.push_back({
                sampleA.clip,
                1.0f - t
                });

            result.push_back({
                sampleB.clip,
                t
                });

            return result;
        }
    }

    // 本来ここには来ないが、安全のため最大角度へ固定
    result.push_back({
        samples.back().clip,
        1.0f
        });

    return result;


}
