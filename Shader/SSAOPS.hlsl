#include "Sampler.hlsli"
#include "Constants.hlsli"
#include "Common.hlsli"

Texture2D depthTexture : register(t0);
Texture2D<float4> sceneNormalTexture : register(t1);
StructuredBuffer<float3> kernelPoints : register(t2);
StructuredBuffer<float3> noise : register(t3);

#define ALCHEMY_AO

cbuffer SSAO_CONSTANTS_BUFFER : register(b5)
{
    float radius;
    float bias;
    float power;
    float pad;
}


float4 main(float4 svPosition : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    float3 normal = sceneNormalTexture.SampleLevel(samplerStates[LINEAR_BORDER_BLACK], texcoord, 0).xyz; // world空間
    normal = mul(float4(normal, 0), view).xyz; //　world空間 -> view空間

    float depth = depthTexture.SampleLevel(samplerStates[LINEAR_BORDER_BLACK], texcoord, 0);
#if 1
    if (depth > 0.9999999)
    {
        discard;
    }
#endif	
    float4 ndc;
    ndc.xy = UvToNdc(texcoord);
    ndc.z = depth;
    ndc.w = 1.0;
	
	// ndc -> view空間　
    float4 position = mul(ndc, inverseProjection);
    position /= position.w;
	
	// TBNは tangent 空間から view 空間への変換行列
    float3 randomVec = noise[(svPosition.x % 4) + 4 * (svPosition.y % 4)]; // ランダムな回転
    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);
	
    const int kernelSize = 64;
	
    float occlusion = 0.0; // 累積されるオクルージョン値
    for (int kernel = 0; kernel < kernelSize; ++kernel)
    {
        float3 samplePosition = mul(kernelPoints[kernel], TBN); // tangent 空間 -> view 空間
        samplePosition = position.xyz + samplePosition * radius; // サンプル位置を中心から半径内に配置
		
		// 射線上にあるビュー空間とシーン空間の交点を見つける。
        float4 intersection = mul(float4(samplePosition, 1.0), projection); // view空間 -> clip空間
        intersection /= intersection.w; // clip空間 -> ndc空間
        intersection.z = depthTexture.SampleLevel(samplerStates[LINEAR_BORDER_WHITE], NdcToUv(intersection).xy, 0);
        intersection = mul(intersection, inverseProjection); // from ndc to view-space
        intersection /= intersection.w; // perspective divide
		
		// 遮蔽率の推定
        float3 v = intersection.xyz - position.xyz;
        const float beta = bias; // バイアス距離
        const float epsilon = 0.001; //　ゼロ除算を防止するための小さな値
        occlusion += max(0, dot(normal, v) - position.z * beta) / (dot(v, v) + epsilon);
    }
	
    const float sigma = 0.3; // 累積された遮蔽値をどれくらいの強さで最終的な影に反映させるか
    occlusion = max(0.0, 1.0 - (2.0 * sigma * occlusion / kernelSize));
	
    return power > 0.0 ? pow(occlusion, power) : 1.0; 
}