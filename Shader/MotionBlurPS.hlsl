#include "MotionBlur.hlsli"
#include "Sampler.hlsli"

Texture2D<float4> gbufferBaseColor : register(t0); //< ここがシーンバッファに書き換わるので注意
Texture2D<float4> gbufferEmissiveColor : register(t1);
Texture2D<float4> gbufferNormal : register(t2);
Texture2D<float4> gbufferParameter : register(t3);
Texture2D<float> gbufferDepth : register(t4);
Texture2D<float4> gbufferVelocity : register(t5);
Texture2D<float4> neighborMaxVelocity : register(t6); //< 近傍最大速度バッファ

cbuffer MOTION_BLUR_CONSTANT_BUFFER : register(b2)
{
    int motionBlurIteration;
    float motionBlurJitter;
    float motionBlurExposureTime;
    float motionBlurFpsRate;
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

float4 AdjustVelocity(float4 velocity)
{
    velocity.xy *= motionBlurExposureTime * motionBlurFpsRate;
    return velocity;
}

float CalcSoftDepthCompare(float za, float zb)
{
    static const float SoftDepthExtend = 0.1f;
    return saturate(1.0f - (za - zb) / SoftDepthExtend);
}

float CalcCone(float L, float2 V)
{
    float ret = 1.0f - (L / length(V));
    return saturate(ret);
}

float CalcCylinder(float L, float2 V)
{
    float lv = length(V);
    return 1.0f - smoothstep(0.95f * lv, 1.05f * lv, L);
}

float4 main(VS_OUT pin) : SV_TARGET
{
#define FLT_EPS 1.19209e-07f

    float2 gbufferTexcoord = pin.texCoord.xy;

    //  GBufferテクスチャから情報をデコード
    float3 baseColor = gbufferBaseColor.SampleLevel(samplerStates[POINT], gbufferTexcoord, 0);
    float linearDepth = gbufferDepth.SampleLevel(samplerStates[POINT], gbufferTexcoord, 0);

    uint width, height;
    gbufferBaseColor.GetDimensions(width, height);
    const float2 texSize = float2((float) width, (float) height);
    const float2 halfPixel = float2(0.5f, 0.5f);
    const float halfPixelLength = length(halfPixel);

    float4 color = float4(baseColor, 1);
    float2 Vn = float2(0, 0);

    //  速度が小さい場合は無視
    if (length(Vn) >= halfPixelLength + FLT_EPS)
    {
        float2 X = pin.texCoord.xy * texSize;
        int3 nX = int3((int) X.x, (int) X.y, 0);
        
        float4 tVx = AdjustVelocity(gbufferVelocity.Load(nX));
        float2 Vx = ReconstVelocity(tVx.xy);
        float Zx = linearDepth;

        float weight = 1.0f / max(length(Vx), halfPixelLength);
        color *= weight;

        float2 random_value = rand2(pin.texCoord.xy);
        float jitter = ((random_value.x + random_value.y) * 2.0f - 1.0f) * motionBlurJitter;

        //  速度ベクトル方向にブラーをかける
        int ignore = (motionBlurIteration - 1) / 2;
	    [loop]
        for (int i = 0; i < motionBlurIteration; ++i)
        {
            if (i == ignore)
                continue;

			//  代表ベクトルはVxとVnを交互に採用
            float2 Va = Vn;
            if ((i & 0x01) != 0)
            {
                Va = Vx;
            }

            float t = lerp(-1.0f, 1.0f, ((float) i + jitter) / motionBlurIteration);
            float2 Y = X + Va * t + halfPixel;
            int3 nY = int3((int) Y.x, (int) Y.y, 0);

            float Zy = gbufferDepth.Load(nY);
            float2 L = length(X - Y);

            float2 Vy = ReconstVelocity(AdjustVelocity(gbufferVelocity.Load(nY)).xy);
            float ay = CalcSoftDepthCompare(Zx, Zy) * CalcCone(L, Vy)
						+ CalcSoftDepthCompare(Zy, Zx) * CalcCone(L, Vx)
						+ CalcCylinder(L, Vy)
                        * CalcCylinder(L, Vx) * 2.0f;

            weight += ay;
            color += gbufferBaseColor.Load(nY) * ay;
        }

        color /= weight;
    }
    //	ガンマ係数
    static const float GammaFactor = 2.2f;

    //  ガンマ補正
    color.rgb = pow(color.rgb, 1.0f / GammaFactor);
    return float4(color.xyz, 1);
}
