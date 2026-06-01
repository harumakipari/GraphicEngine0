#include "FullScreenQuad.hlsli"
#include "Constants.hlsli"
#include "Sampler.hlsli"
#include "FilterFunctions.hlsli"
#include "Lights.hlsli"
#include "ModelType.hlsli"

Texture2D sceneColorTexture : register(t0);
Texture2D bokehTexture : register(t1);
Texture2D depthTexture : register(t2);


float4 main(VS_OUT pin) : SV_TARGET
{
    // シーンからライティング済みのカラーテクスチャ
    float4 sceneColor = sceneColorTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
    float4 color = sceneColor;

    // シーンから深度値を取得
    float depthNdc = depthTexture.Sample(samplerStates[POINT], pin.texcoord).x;

    float4 positionNdc;
    // uv -> ndc 
    positionNdc.x = pin.texcoord.x * +2 - 1;
    positionNdc.y = pin.texcoord.y * -2 + 1;
    positionNdc.z = depthNdc;
    positionNdc.w = 1;

    // ndc -> view 
    float4 positionViewSpace = mul(positionNdc, inverseProjection); // ndc → clip 
    positionViewSpace = positionViewSpace / positionViewSpace.w; // clip -> view 
    //return float4((positionViewSpace.z) / 100.0, 0, 0, 1);
    // ndc -> world 
    float4 positionWorldSpace = mul(positionNdc, inverseViewProjection);
    positionWorldSpace /= positionWorldSpace.w;


    // DOFの処理
    if (enableDof)
    {
        // 深度からview space Z
        float viewSpaceZ = positionViewSpace.z;

        // ブレンド係数
        float alpha = abs(viewSpaceZ - focusDistance) / dofRange;
        alpha = pow(alpha, 2.0);
        alpha = saturate(alpha);

        // 色取得
        float3 originColor = color.rgb;
        float3 bokehColor = bokehTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord).rgb;

        // DOF合成
        color.rgb = lerp(originColor, bokehColor, alpha);
    }
    float4 finalColor = color;

    // 分割表示
    if (pin.texcoord.x < splitU)
    {
        // 左側はポストなし
        finalColor = sceneColor;
    }

    if (enableToneMapping == 1)
    {
        // トーンマップは共通にする
        finalColor.rgb = JodieReinhardToneMap(finalColor.rgb);

	    // 色相、彩度、明度、コントラストを調整する。
        finalColor.rgb = HueSaturation(finalColor.rgb, hueShift, saturation);
        finalColor.rgb = BrightnessContrast(finalColor.rgb, brightness, contrast);

        // 色の調整をする
        finalColor.rgb = RGBColorMap(finalColor.rgb, colorMapRGB);

        finalColor.rgb = ToneCurve(finalColor.rgb, toneMappingValue);
    }


    return finalColor;
}