#include "MotionBlur.hlsli"
#include "Sampler.hlsli"

Texture2D<float4> gbuffer_base_color : register(t0); //< ここがシーンバッファに書き換わるので注意
Texture2D<float4> gbuffer_emissive_color : register(t1);
Texture2D<float4> gbuffer_normal : register(t2);
Texture2D<float4> gbuffer_parameter : register(t3);
Texture2D<float> gbuffer_depth : register(t4);
Texture2D<float4> gbuffer_velocity : register(t5);
Texture2D<float4> neighbor_max_velocity : register(t6); //< 近傍最大速度バッファ

cbuffer MOTION_BLUR_CONSTANT_BUFFER : register(b2)
{
    int motion_blur_iteration;
    float motion_blur_jitter;
    float motion_blur_exposure_time;
    float motion_blur_fps_rate;
};

float rand(float t)
{
    return frac(sin(dot(float2(t, t), float2(12.9898f, 78.233f))) * 43758.5453123f);
}

float2 rand2(float2 s)
{
    s = float2(dot(s, float2(12.9898f, 78.233f)), dot(s, float2(269.5f, 183.3f)));
    return frac(sin(s) * float2(43758.5453123f, 43758.5453123f));
}

float4 adjust_velocity(float4 velocity)
{
    velocity.xy *= motion_blur_exposure_time * motion_blur_fps_rate;
    return velocity;
}

float calc_soft_depth_compare(float za, float zb)
{
    static const float SoftDepthExtend = 0.1f;
    return saturate(1.0f - (za - zb) / SoftDepthExtend);
}

float calc_cone(float L, float2 V)
{
    float ret = 1.0f - (L / length(V));
    return saturate(ret);
}

float calc_cylinder(float L, float2 V)
{
    float lv = length(V);
    return 1.0f - smoothstep(0.95f * lv, 1.05f * lv, L);
}

float4 main(VS_OUT pin) : SV_TARGET
{
#define FLT_EPS 1.19209e-07f

    float2 gbuffer_texcoord = pin.texCoord.xy;

    //  GBufferテクスチャから情報をデコード
    float3 baseColor = gbuffer_base_color.SampleLevel(samplerStates[POINT], gbuffer_texcoord, 0);
    float linear_depth = gbuffer_depth.SampleLevel(samplerStates[POINT], gbuffer_texcoord, 0);

    uint width, height;
    gbuffer_base_color.GetDimensions(width, height);
    const float2 tex_size = float2((float) width, (float) height);
    const float2 HalfPixel = float2(0.5f, 0.5f);
    const float HalfPixelLength = length(HalfPixel);

    float4 color = float4(baseColor, 1);

    //float4 tVn = adjust_velocity(neighbor_max_velocity.SampleLevel(motion_blur_sampler_state, pin.texCoord.xy, 0));
    float2 Vn = float2(0, 0);
    //float2 Vn = ReconstVelocity(tVn.xy);

    //  速度が小さい場合は無視
    if (length(Vn) >= HalfPixelLength + FLT_EPS)
    {
        float2 X = pin.texCoord.xy * tex_size;
        int3 nX = int3((int) X.x, (int) X.y, 0);
        
        float4 tVx = adjust_velocity(gbuffer_velocity.Load(nX));
        float2 Vx = ReconstVelocity(tVx.xy);
        float Zx = linear_depth;

        float weight = 1.0f / max(length(Vx), HalfPixelLength);
        color *= weight;

        float2 random_value = rand2(pin.texCoord.xy);
        float jitter = ((random_value.x + random_value.y) * 2.0f - 1.0f) * motion_blur_jitter;

        //  速度ベクトル方向にブラーをかける
        int ignore = (motion_blur_iteration - 1) / 2;
	    [loop]
        for (int i = 0; i < motion_blur_iteration; ++i)
        {
            if (i == ignore)
                continue;

			//  代表ベクトルはVxとVnを交互に採用
            float2 Va = Vn;
            if ((i & 0x01) != 0)
            {
                Va = Vx;
            }

            float t = lerp(-1.0f, 1.0f, ((float) i + jitter) / motion_blur_iteration);
            float2 Y = X + Va * t + HalfPixel;
            int3 nY = int3((int) Y.x, (int) Y.y, 0);

            float Zy = gbuffer_depth.Load(nY);
            float2 L = length(X - Y);

            float2 Vy = ReconstVelocity(adjust_velocity(gbuffer_velocity.Load(nY)).xy);
            float ay = calc_soft_depth_compare(Zx, Zy) * calc_cone(L, Vy)
						+ calc_soft_depth_compare(Zy, Zx) * calc_cone(L, Vx)
						+ calc_cylinder(L, Vy)
                        * calc_cylinder(L, Vx) * 2.0f;

            weight += ay;
            color += gbuffer_base_color.Load(nY) * ay;
        }

        color /= weight;
    }
    //	ガンマ係数
    static const float GammaFactor = 2.2f;

    //  ガンマ補正
    color.rgb = pow(color.rgb, 1.0f / GammaFactor);
    return float4(color.xyz, 1);
}
