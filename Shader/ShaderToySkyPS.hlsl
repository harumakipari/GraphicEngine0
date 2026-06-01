struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

#include "Constants.hlsli"


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


void mainImage(out float4 fragColor, in float2 fragCoord)
{
#if 0
	//get coords and direction
    float2 uv = fragCoord.xy / iResolution.xy - .5;
    uv.y *= iResolution.y / iResolution.x;
    float3 dir = float3(uv * zoom, 1.);
    float time = iTime * speed + .25;

	//mouse rotation
    float a1 = .5 + iMouse.x / iResolution.x * 2.;
    float a2 = .8 + iMouse.y / iResolution.y * 2.;
    float2x2 rot1 = float2x2(cos(a1), sin(a1), -sin(a1), cos(a1));
    float2x2 rot2 = float2x2(cos(a2), sin(a2), -sin(a2), cos(a2));
    dir.xz *= rot1;
    dir.xy *= rot2;
    float3 from = float3(1., .5, 0.5);
    from += float3(time * 2., time, -2.);
    from.xz *= rot1;
    from.xy *= rot2;
#else
    float2 uv = fragCoord.xy / iResolution.xy - 0.5;
    uv.y *= iResolution.y / iResolution.x;
    float3 dir = float3(uv * zoom, 1.0);
    float time = elapsedTime * speed + 0.25;
    float2 iMouse = float2(0.1, 0.1); // マウス位置を初期化

// マウス回転
    float a1 = 0.5 + iMouse.x / iResolution.x * 2.0;
    float a2 = 0.8 + iMouse.y / iResolution.y * 2.0;
    float2x2 rot1 = float2x2(cos(a1), sin(a1), -sin(a1), cos(a1));
    float2x2 rot2 = float2x2(cos(a2), sin(a2), -sin(a2), cos(a2));

    dir.xz = mul(rot1, dir.xz);
    dir.xy = mul(rot2, dir.xy);

// 視点 from に time を適用
    float3 from = float3(1.0, 0.5, 0.5);
    from += float3(time * 2.0, time, -2.0);
    from.xz = mul(rot1, from.xz);
    from.xy = mul(rot2, from.xy);
#endif
	
	//volumetric rendering
    float s = 0.1, fade = 1.;
    float3 v = float3(0, 0, 0);
    for (int r = 0; r < volsteps; r++)
    {
        float3 p = from + s * dir * .5;
        p = abs(float3(tile, tile, tile) - fmod(p, float3(tile * 2.0, tile * 2.0, tile * 2.0)));
        //p = abs(float3(tile) - mod(p, float3(tile * 2.))); // tiling fold
        float pa, a = pa = 0.;
        for (int i = 0; i < iterations; i++)
        {
            p = abs(p) / dot(p, p) - formuparam; // the magic formula
            a += abs(length(p) - pa); // absolute sum of average change
            pa = length(p);
        }
        float dm = max(0., darkmatter - a * a * .001); //dark matter
        a *= a * a; // add contrast
        if (r > 6)
            fade *= 1. - dm; // dark matter, don't render near
		//v+=vec3(dm,dm*.5,0.);
        v += fade;
        v += float3(s, s * s, s * s * s * s) * a * brightness * fade; // coloring based on distance
        fade *= distfading; // distance fading
        s += stepsize;
    }
    v = lerp(float3(length(v), length(v), length(v)), v, saturation);

    //v = mix(float3(length(v)), v, saturation); //color adjust
    //fragColor = float4(v * .01, 1.);
    fragColor = float4(v * 0.0005, 1.0);

}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 fragColor = 0;
    float2 fragCoord = pin.position.xy;

    mainImage(fragColor, fragCoord);
    return fragColor;
}