#include "MotionBlur.hlsli"

Texture2D<float4> tile_max_velocity : register(t1);

static const int2 Offsets[] =
{
    int2(-1, -1),
    int2(0, -1),
    int2(+1, -1),

    int2(-1, 0),
    int2(+1, 0),

    int2(-1, +1),
    int2(0, +1),
    int2(+1, +1),
};

float4 main(VS_OUT pin) : SV_TARGET
{
    uint width, height;
    tile_max_velocity.GetDimensions(width, height);
    int3 tex_size = int3(width, height, 0);
    int3 uv = int3(width * pin.texCoord.x, height * pin.texCoord.y, 0);

    float4 ret = tile_max_velocity.Load(uv);
    float maxV = dot(ret.xy, ret.xy);

    //  近傍のタイル8ブロック確認して最も早いものを利用する
    for (int i = 0; i < 8; ++i)
    {
        int3 tuv = clamp(uv + int3(Offsets[i], 0), 0, tex_size);
        float4 tv = tile_max_velocity.Load(tuv);
        float l = dot(tv.xy, tv.xy);
        ret = lerp(ret, tv, l > maxV);
        maxV = lerp(maxV, l, l > maxV);
    }

    return ret;
}
