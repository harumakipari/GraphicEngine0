#include "GltfModel.hlsli"

GBUFFER_PS_OUT main(VS_OUT pin, bool isFrontFace : SV_IsFrontFace)
{
    GBUFFER_PS_OUT pout;
    float3 emissive = cpuColor.rgb;
    emissive *= emissionPower;
    pout.position = pin.wPosition; // world 空間
    float3 N = normalize(pin.wNormal.xyz);
    pout.gBuffer3Normal = float4(N.xyz, 0); // world 空間
    pout.albedo = float4(1, 1, 1, 1); // 仮。点光源はemissiveで色をつけるからここでは白にしておく
#if 1
    float n =
    sin(pin.wPosition.y * 15 + elapsedTime * 4) *
    sin(pin.wPosition.x * 10 + elapsedTime * 3);

    emissive *= 0.8 + 0.4 * n;

// --- コア ---
    float2 uv = pin.texcoord;
    float2 centered = uv - 0.5;

    centered.y *= 1.5;

// 歪み
    float distort =
    sin(pin.wPosition.y * 10 + elapsedTime * 3) * 0.05;

    centered.x += distort;

    float dist = length(centered);

// コア
    float core = smoothstep(0.6, 0.0, dist);

// 下強く
    float vertical = 1.0 - uv.y;
    core *= vertical;

// 強調
    core = pow(core, 1.5);

// --- 色レイヤー ---
    float3 coreColor = float3(1.0, 1.0, 1.0);
    float3 yellow = float3(1.0, 0.8, 0.2);
    float3 orange = float3(1.0, 0.3, 0.0);

// 中心（白）
    emissive = lerp(emissive, coreColor * emissionPower, core * 0.8);

// 中間（黄色）
    float mid = saturate(core * 2.0);
    emissive = lerp(emissive, yellow * emissionPower, mid * 0.3);

// 外側（オレンジ）
    float outer = 1.0 - core;
    emissive = lerp(emissive, orange * emissionPower, outer * 0.5);

// 外側暗く
    emissive *= lerp(0.4, 1.0, core);

    #endif 
    // 元々wは１だったがスカイマップなどの時に使用するため、２は点光源であることを示すフラグ
    pout.emissive = float4(emissive, 2);
    pout.material = float4(0.0, 0.0, 0.0, 0.0);
    return pout;
}