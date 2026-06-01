#include "FullScreenQuad.hlsli"

#define POINT 0
#define LINEAR 1
#define ANISOTROPHIC 2
SamplerState samplerStates[3] : register(s0);
Texture2D textureMaps[4] : register(t0);


cbuffer SHADER_CONSTANT_BUFFER : register(b2)
{
    //‹P“x
    float luminanceThreshold;
    float gaussianSigma;
    float bloomIntenssity;
    float exposure;
}


float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = textureMaps[0].Sample(samplerStates[ANISOTROPHIC], pin.texcoord);
    float alpha = color.a;
    color.rgb = step(luminanceThreshold, dot(color.rgb, float3(0.299, 0.587, 0.114))) * color.rgb;
    //color.rgb = step(0.8, dot(color.rgb, float3(0.299, 0.587, 0.114))) * color.rgb;
    return float4(color.rgb, alpha);
}

