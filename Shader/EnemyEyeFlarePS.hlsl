#include "GltfModel.hlsli"

GBUFFER_PS_OUT main(VS_OUT pin, bool isFrontFace : SV_IsFrontFace)
{
    GBUFFER_PS_OUT pout;
    float3 emissive = cpuColor.rgb;
    emissive *= emissionPower;
    pout.position = pin.wPosition; // world 空間
    float3 N = normalize(pin.wNormal.xyz);
    pout.gBuffer3Normal = float4(N.xyz, objectType); // world 空間
    pout.albedo = float4(cpuColor.rgb, 1); // 仮。点光源はemissiveで色をつけるからここでは白にしておく

    // 元々wは１だったがスカイマップなどの時に使用するため、２は点光源であることを示すフラグ
    pout.emissive = float4(emissive, GBUFFER_FLAG_EMISSIVE);
    pout.material = float4(0.0, 0.0, 0.0, materialType);
    float2 velocity = CalculateUvSpaceVelocity(pin.currentClipPosition, pin.previousClipPosition);
    pout.velocity = float4(velocity, 1, 1);

    return pout;
}
