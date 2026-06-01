#include "GltfModel.hlsli"
#include "Sampler.hlsli"
#include "Common.hlsli"
#include "ModelType.hlsli"


#define BASE_COLOR_TEXTURE 0 
#define METALLIC_ROUGHNESS_TEXTURE 1 
#define NORMAL_TEXTURE 2 
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4 
Texture2D<float4> materialTextures[5] : register(t1);


GBUFFER_PS_OUT main(VS_OUT pin, bool isFrontFace : SV_IsFrontFace)
{
    GBUFFER_PS_OUT pout;

    const float GAMMA = 2.2;

    const MaterialConstants m = materials[material];

    float4 baseColorFactor = m.pbrMetallicRoughness.baseColorFactor;
    const int baseColorTexture = m.pbrMetallicRoughness.basecolorTexture.index;
    if (baseColorTexture > -1)
    {
        float4 sampled = materialTextures[BASE_COLOR_TEXTURE].Sample(samplerStates[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        baseColorFactor *= sampled;
    }
    if (m.alphaMode == 0 /*OPAQUE*/)
    {
        baseColorFactor.a = 1.0;
    }
    else if (m.alphaMode == 1 /*MASK*/ || m.alphaMode == 2 /*BLEND*/)
    {
        clip(baseColorFactor.a - m.alphaCutoff);
    }
    
    float3 emissiveFactor = m.emissiveFactor;
    const int emissiveTexture = m.emissiveTexture.index;
    if (emissiveTexture > -1)
    {
        float4 sampled = materialTextures[EMISSIVE_TEXTURE].Sample(samplerStates[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        emissiveFactor *= sampled.rgb;

        if (objectType == OBJECT_PLAYER || objectType == OBJECT_ENEMY)
        { // playerの時はエミッシブを強めに出す
            emissiveFactor *= emissionPower;
        }


    }
    
    float roughnessFactor = m.pbrMetallicRoughness.roughnessFactor;
    float metallicFactor = m.pbrMetallicRoughness.metallicFactor;
    const int metallicRoughnessTexture = m.pbrMetallicRoughness.metallicRoughnessTexture.index;
    if (metallicRoughnessTexture > -1)
    {
        float4 sampled = materialTextures[METALLIC_ROUGHNESS_TEXTURE].Sample(samplerStates[LINEAR], pin.texcoord);
        //roughnessFactor = 1.0;
        roughnessFactor *= sampled.g;
        metallicFactor *= sampled.b;
    }

    if (objectType == OBJECT_DOOR)
    { // ドアの時だけラフネスを上げて、メタリックを下げる
        if (metallicFactor < 0.1) // 木
        {
            roughnessFactor = max(roughnessFactor, 0.6);
        }
    }
    
    float occlusionFactor = 1.0;
    const int occlusionTexture = m.occlusionTexture.index;
    if (occlusionTexture > -1)
    {
        float4 sampled = materialTextures[OCCLUSION_TEXTURE].Sample(samplerStates[LINEAR], pin.texcoord);
        occlusionFactor *= sampled.r;
    }
    const float occlusionStrength = m.occlusionTexture.strength;

    float3 N = normalize(pin.wNormal.xyz);
    float3 T = hasTangent ? normalize(pin.wTangent.xyz) : float3(1, 0, 0.0001);
    float sigma = hasTangent ? pin.wTangent.w : 1.0;
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T) * sigma);
    
    //背面については、接線方向の基底ベクトルは符号が反転する。
    if (isFrontFace == false)
    {
        T = -T;
        B = -B;
        N = -N;
    }
    
    const int normalTexture = m.normalTexture.index;
    if (normalTexture > -1)
    {
        float4 sampled = materialTextures[NORMAL_TEXTURE].Sample(samplerStates[LINEAR], pin.texcoord);
        float3 normalFactor = sampled.xyz;
        normalFactor = (normalFactor * 2.0) - 1.0;
        normalFactor = normalize(normalFactor * float3(m.normalTexture.scale, m.normalTexture.scale, 1.0));
        N = normalize((normalFactor.x * T) + (normalFactor.y * B) + (normalFactor.z * N));
    }

    pout.gBuffer3Normal = float4(N.xyz, objectType); // world space

    //pout.gbuffer1.xy = EncodeOctahedralNormal(N);


    pout.albedo = baseColorFactor;

    pout.position = pin.wPosition; // world space 

    pout.emissive = float4(emissiveFactor, 0); // wの値 : スカイマップ１それ以外０    2: emissiveFlagとして使用

    if (materialType == MATERIAL_EYE)
    {
        float luminance = dot(baseColorFactor.rgb, float3(0.3, 0.59, 0.11));

        float2 uv = pin.texcoord;
        float dist = distance(uv, float2(0.5, 0.5));

        float maskColor = 1.0 - step(0.1, luminance);
        float maskCenter = 1.0 - smoothstep(0.1, 0.2, dist);

        float mask = maskColor * maskCenter;
        emissiveFactor = mask * float3(cpuColor.rgb) * emissionPower; 
        pout.emissive = float4(emissiveFactor, 0); // wの値 : スカイマップ１それ以外０    2: emissiveFlagとして使用
    }

    pout.material = float4(metallicFactor, roughnessFactor, occlusionFactor, materialType /*マテリアルタイプ*/);
    
    return pout;
}