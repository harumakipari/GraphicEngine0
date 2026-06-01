#include "Trail.hlsli"
#include "Constants.hlsli"

VS_OUT main(VS_IN input)
{
    VS_OUT vout ;

    vout.pos = mul(float4(input.pos, 1), viewProjection);
    vout.alpha = input.alpha;
    vout.uv = input.uv;

    return vout;
}