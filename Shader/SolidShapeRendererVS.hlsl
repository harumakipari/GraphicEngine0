#include "SolidShapeRenderer.hlsli"

VS_OUT main(float4 position : POSITION, float4 normal : NORMAL)
{
    normal.w = 0;
	
    VS_OUT vout;
    vout.position = mul(position, mul(world, viewProjection));
    vout.normal = mul(normal, world).xyz;

    return vout;
}
