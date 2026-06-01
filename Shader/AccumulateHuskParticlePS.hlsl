#include "Constants.hlsli"
#include "GltfModel.hlsli"
#include "Lights.hlsli"

#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
SamplerState samplerStates[3] : register(s0);
Texture2D textureMaps[4] : register(t0);

struct PARTICLE
{
    float4 color;
    float3 position;
    float3 normal;
    float3 velocity;
    float age;
    int state;
};
AppendStructuredBuffer<PARTICLE> particleBuffer : register(u1);

void main(VS_OUT pin)
{
    float4 color = textureMaps[0].Sample(samplerStates[ANISOTROPIC], pin.texcoord);
    float alpha = color.a;
    
    const float GAMMA = 2.3;
    color.rgb = pow(color.rgb, GAMMA);
    
    float3 N = normalize(pin.wNormal.xyz);
    
    float3 T = normalize(pin.wTangent.xyz);
    float sigma = pin.wTangent.w;
    T = normalize(T - dot(N, T));
    float3 B = normalize(cross(N, T) * sigma);
    
    float4 normal = textureMaps[1].Sample(samplerStates[LINEAR], pin.texcoord);
    normal = (normal * 2.0) - 1.0;
    N = normalize((normal.x * T) + (normal.y * B) + (normal.z * N));
    
    float3 L = normalize(-lightDirection.xyz);
    float3 diffuse = color.rgb * max(0, dot(N, L));
    float3 V = normalize(cameraPositon.xyz - pin.wPosition.xyz);
    float3 specular = pow(max(0, dot(N, normalize(V + L))), 128);
    float3 ambient = color.rgb * 0.2;
    
    //PARTICLE p;
    //p.gbuffer3Color = float4(max(0, ambient + diffuse + specular), alpha) * pin.gbuffer3Color;
    //p.position = pin.wPosition.xyz;
    //p.gbuffer1Normal = N.xyz;
    //p.velocity = 0;
    //p.age = 0;
    //p.state = 0;
    //particleBuffer.Append(p);
}