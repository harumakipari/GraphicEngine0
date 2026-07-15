#include "GltfModel.hlsli"

GBUFFER_PS_OUT main(INSTANCE_VS_OUT pin, bool isFrontFace : SV_IsFrontFace)
{
    GBUFFER_PS_OUT pout;
    float emissivePower = pin.instancePlusParameter.x;
    float3 emissive = pin.instanceColor.rgb;
    //emissive *= emissivePower;

    pout.position = pin.wPosition; // world 空間
    float3 N = normalize(pin.wNormal.xyz);
    int instanceObjectType = pin.instancePlusParameter.y;
    pout.gBuffer3Normal = float4(N.xyz, instanceObjectType); // world 空間
    pout.albedo = float4(1, 1, 1, 1); // 仮。点光源はemissiveで色をつけるからここでは白にしておく
    float2 velocity = CalculateUvSpaceVelocity(pin.currentClipPosition, pin.previousClipPosition);
    pout.velocity = float4(velocity, 1, 1);

    // 元々wは１だったがスカイマップなどの時に使用するため、２は点光源であることを示すフラグ
    pout.emissive = float4(emissive, GBUFFER_FLAG_EMISSIVE);
    pout.material = float4(0.0, 0.0, 0.0, 0.0);
    return pout;
}