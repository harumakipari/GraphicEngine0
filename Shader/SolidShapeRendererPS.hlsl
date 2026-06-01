#include "SolidShapeRenderer.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    float3 N = normalize(pin.normal);
    float3 L = normalize(-lightDirection.xyz);
    float power = max(0, dot(L, N));

    power = power * 0.5 + 0.5f;

    float4 color = materialColor;
    color.rgb *= power;

    return color;
}
