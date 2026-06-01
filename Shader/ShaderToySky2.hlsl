#include "Constants.hlsli"

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};



typedef float2 vec2;
typedef float3 vec3;
typedef float4 vec4;
typedef int2 ivec2;
typedef int3 ivec3;
typedef int4 ivec4;
typedef uint2 uvec2;
typedef uint3 uvec3;
typedef uint4 uvec4;
typedef float2x2 mat2;
typedef float3x3 mat3;
typedef float4x4 mat4;
#define mix lerp
#define mod fmod
#define fract frac
#define atan atan2
#define inversesqrt rsqrt
#define texture Sample
#define textureLod SampleLevel
#define texelFetch Load
#define sampler2D Texture2D

#define iterations 17
#define formuparam 0.53

#define volsteps 20
#define stepsize 0.1

#define zoom   0.800
#define tile   0.850
#define speed  0.010 

#define brightness 0.0015
#define darkmatter 0.300
#define distfading 0.730
#define saturation 0.850


float S(float a, float b, float t)
{
    t = saturate((t - a) / (b - a));
    return t * t * (3.0 - 2.0 * t);
}

float N21(float2 p)
{
    float3 a = frac(float3(p.x, p.y, p.x) * float3(213.897, 653.453, 253.098));
    a += dot(a, a.yzx + 79.76);
    return frac((a.x + a.y) * a.z);
}

float2 GetPos(float2 id, float2 offs, float t)
{
    float n = N21(id + offs);
    float n1 = frac(n * 10.0);
    float n2 = frac(n * 100.0);
    float a = t + n;
    return offs + float2(sin(a * n1), cos(a * n2)) * 0.4;
}

float df_line(float2 a, float2 b, float2 p)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h);
}

float lineShape(float2 a, float2 b, float2 uv)
{
    float r1 = 0.04;
    float r2 = 0.01;

    float d = df_line(a, b, uv);
    float d2 = length(a - b);
    float fade = S(1.5, 0.5, d2);
    fade += S(0.05, 0.02, abs(d2 - 0.75));
    return S(r1, r2, d) * fade;
}

float NetLayer(float2 st, float n, float t)
{
    float2 id = floor(st) + n;
    st = frac(st) - 0.5;

    float2 p[9];
    int i = 0;
    for (float y = -1.0; y <= 1.0; y++)
    {
        for (float x = -1.0; x <= 1.0; x++)
        {
            p[i++] = GetPos(id, float2(x, y), t);
        }
    }

    float m = 0.0;
    float sparkle = 0.0;

    for (i = 0; i < 9; i++)
    {
        m += lineShape(p[4], p[i], st);

        float d = length(st - p[i]);
        float s = 0.005 / (d * d);
        s *= S(1.0, 0.7, d);

        float pulse = sin((frac(p[i].x) + frac(p[i].y) + t) * 5.0) * 0.4 + 0.6;
        pulse = pow(pulse, 20.0);

        s *= pulse;
        sparkle += s;
    }

    m += lineShape(p[1], p[3], st);
    m += lineShape(p[1], p[5], st);
    m += lineShape(p[7], p[5], st);
    m += lineShape(p[7], p[3], st);

    float sPhase = (sin(t + n) + sin(t * 0.1)) * 0.25 + 0.5;
    sPhase += pow(sin(t * 0.1) * 0.5 + 0.5, 50.0) * 5.0;

    m += sparkle * sPhase;
    return m;
}


void mainImage(out float4 fragColor, in float2 fragCoord)
{
    const int NUM_LAYERS = 4;

    float2 uv = (fragCoord - iResolution.xy * 0.5) / iResolution.y;
    float2 iMouse = float2(0.1, 0.1); // マウス位置を初期化
    float2 M = iMouse.xy / iResolution.xy - 0.5;

    float t = elapsedTime * 0.1;
    float s = sin(t);
    float c = cos(t);
    float2x2 rot = float2x2(c, -s, s, c);

    float2 st = mul(uv, rot);
    M = mul(M * 2.0, rot);

    float m = 0.0;
    for (float i = 0.0; i < 1.0; i += 1.0 / NUM_LAYERS)
    {
        float z = frac(t + i);
        float size = lerp(15.0, 1.0, z);
        float fade = S(0.0, 0.6, z) * S(1.0, 0.8, z);
        m += fade * NetLayer(st * size - M * z, i, elapsedTime);
    }

    //float fft = iChannel0.Load(int3(70, 0, 0)).x;
    //float glow = -uv.y * fft * 2.0;

    float fft = sin(elapsedTime * 2.0) * 0.5 + 0.5; // 0.0～1.0の周期的な値に置き換え
    float glow = -uv.y * fft * 2.0;
    
    float3 baseCol = float3(s, cos(t * 0.4), -sin(t * 0.24)) * 0.4 + 0.6;
    float3 col = baseCol * m + baseCol * glow;

    col *= 1.0 - dot(uv, uv);

    float tt = fmod(elapsedTime, 230.0);
    col *= S(0.0, 20.0, tt) * S(224.0, 200.0, tt);

    fragColor = float4(col, 1.0);
}


float4 main(VS_OUT pin) : SV_Target
{
    float4 fragColor = float4(0, 0, 0, 1);
    float2 fragCoord = pin.position.xy;
    mainImage(fragColor, fragCoord);
    return fragColor;
}