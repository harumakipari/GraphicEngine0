#include "MotionBlur.hlsli"


Texture2D<float4> gbuffer_velocity : register(t0);

float4 main(VS_OUT pin) : SV_TARGET
{
    uint width, height;
    gbuffer_velocity.GetDimensions(width, height);
    int3 tex_size = int3(width, height, 0);
    int3 uv = int3((float) width * pin.texCoord.x, (float) height * pin.texCoord.y, 0);

    float2 ret = (float2) 0.0f;
    float maxV = 0.0f;

    //  ブロック内で最も大きい速度を保存
	[loop]
    for (int y = 0; y < TileSize; ++y)
    {
		[loop]
        for (int x = 0; x < TileSize; ++x)
        {
            //  記録した速度ベクトルからタイルを横断するような速度ベクトルを生成
            int3 t = clamp(uv + int3(x, y, 0), 0, tex_size);
            float2 tv = gbuffer_velocity.Load(t).xy;
            float l = dot(tv, tv);
            if (l > maxV)
            {
                ret = tv;
                maxV = l;
            }
        }
    }
    return float4(ret, 0.0f, 1.0f);
}
