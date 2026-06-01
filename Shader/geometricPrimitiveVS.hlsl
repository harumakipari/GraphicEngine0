#include "Constants.hlsli"
#include "geometricPrimitive.hlsli"
#include "Lights.hlsli"
VS_OUT main(float4 positon : POSITION, float4 normal : NORMAL)
{
    VS_OUT vout;
    vout.position = mul(positon, mul(world, viewProjection));

    normal.w = 0;
    float4 N = normalize(mul(normal, world));
    float4 L = normalize(float4(-lightDirection.xyz, 0));
    
    vout.color.rgb = materialColor.rgb * max(0, dot(L, N));
    vout.color.a = materialColor.a;
    
    return vout;
}