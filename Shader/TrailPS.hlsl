#include "Trail.hlsli"
float4 main(VS_OUT input) : SV_TARGET
{
    float noise = frac(sin(dot(input.uv, float2(12.9898, 78.233))) * 43758.5453);

    float mask = smoothstep(0.1, 1.0, noise);

    float2 uv = input.uv;
    float center = abs(uv.y - 0.5) * 2.0;
    float starShape = smoothstep(1.0, 0.0, center);

    float alpha = input.alpha * mask * starShape;

    float3 col = lerp(
        float3(0.6, 0.9, 1.0),
        float3(0, 0, 0),
        input.uv.x
    );

    //col *= 2.0;
    //col *= mask;
    //col *= starShape;

    //float sparkle = smoothstep(0.95, 1.0, noise);
    //col += sparkle * 2.0;

    return float4(col, alpha);
}