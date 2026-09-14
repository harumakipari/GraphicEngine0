#include "GltfModel.hlsli"
#include "Sampler.hlsli"
#include "Common.hlsli"

#define BASE_COLOR_TEXTURE 0 
#define METALLIC_ROUGHNESS_TEXTURE 1 
#define NORMAL_TEXTURE 2 
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4 
Texture2D<float4> materialTextures[5] : register(t1);
Texture2D<float4> phase2BaseColorTexture : register(t6);

cbuffer PHASE2_TEXTURE_BLEND_CONSTANT_BUFFER : register(b6)
{
    float phase2TransformProgress;
    float phase2WorldMinY;
    float phase2WorldMaxY;
    float phase2MaskSoftness;
    int phase2TextureDebugMode;
    float3 phase2TextureBlendPadding;
}

GBUFFER_PS_OUT main(VS_OUT pin, bool isFrontFace : SV_IsFrontFace)
{
    GBUFFER_PS_OUT pout;

    const float GAMMA = 2.2;

    const MaterialConstants m = materials[material];

    float4 baseColorFactor = m.pbrMetallicRoughness.baseColorFactor;
    float3 phase2RawRgb = 0.0;
    float3 phase2LinearRgb = 0.0;
    float3 phase2LinearAlbedo = 0.0;
    float3 blendedAlbedoBeforeFlash = baseColorFactor.rgb;
    const int baseColorTexture = m.pbrMetallicRoughness.basecolorTexture.index;
    if (baseColorTexture > -1)
    {
        float4 sampled = materialTextures[BASE_COLOR_TEXTURE].Sample(samplerStates[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        float4 phase2Sampled = phase2BaseColorTexture.Sample(samplerStates[ANISOTROPIC], pin.texcoord);
        phase2RawRgb = phase2Sampled.rgb;
        phase2Sampled.rgb = pow(phase2Sampled.rgb, GAMMA);
        phase2LinearRgb = phase2Sampled.rgb;
        phase2LinearAlbedo = baseColorFactor.rgb * phase2Sampled.rgb;
        const float normalizedHeight = saturate(
            (pin.wPosition.y - phase2WorldMinY) /
            max(phase2WorldMaxY - phase2WorldMinY, 0.0001f));
        const float softness = max(phase2MaskSoftness, 0.00001f);
        // Move a soft boundary upward. Extending it past both ends guarantees
        // Progress 0 = Phase1 and Progress 1 = Phase2 across the whole torso.
        const float boundary = lerp(-softness, 1.0f + softness,
            saturate(phase2TransformProgress));
        const float phase2Weight = 1.0f - smoothstep(
            boundary - softness, boundary + softness, normalizedHeight);
        sampled = lerp(sampled, phase2Sampled, phase2Weight);
        baseColorFactor *= sampled;
        blendedAlbedoBeforeFlash = baseColorFactor.rgb;
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

    if (materialType == MATERIAL_METALLIC)
    {
        emissiveFactor += float3(1.0, 0.8, 0.2) *chargePower ;
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

    float2 velocity = CalculateUvSpaceVelocity(pin.currentClipPosition, pin.previousClipPosition);
    pout.velocity = float4(velocity, 0, 1);

    // ヒットフラッシュ時
    float3 finalAlbedo = baseColorFactor.rgb;
    // ヒットフラッシュ
    float flash = pow(flashValue, 2.0);
    // 色調整・時間調整
    finalAlbedo = lerp(finalAlbedo, float3(1.0,0, 0), flash);

    // Modes 1-3 intentionally bypass deferred lighting and scene post-lighting effects.
    // Tone mapping remains active, so comparison with Mode 0 isolates the lighting path.
    if (phase2TextureDebugMode != 0)
    {
        // Mode 1 is decoded here so the FinalPS display encode reproduces the raw sRGB texture.
        float3 debugAlbedo = phase2LinearRgb;
        if (phase2TextureDebugMode == 2)
            debugAlbedo = phase2LinearAlbedo;
        else if (phase2TextureDebugMode == 3)
            debugAlbedo = blendedAlbedoBeforeFlash;

        pout.albedo = float4(debugAlbedo, baseColorFactor.a);
        pout.gBuffer3Normal.w = OBJECT_NO_LIGHTING;
    }
    else
    {
        pout.albedo = float4(finalAlbedo, baseColorFactor.a);
    }

    pout.position = pin.wPosition; // world space 

    pout.emissive = float4(emissiveFactor, GBUFFER_FLAG_NORMAL); // wの値 : スカイマップ１それ以外０    2: emissiveFlagとして使用

    if (materialType == MATERIAL_EYE)
    {
        float luminance = dot(baseColorFactor.rgb, float3(0.3, 0.59, 0.11));

        float2 uv = pin.texcoord;
        float dist = distance(uv, float2(0.5, 0.5));

        float maskColor = 1.0 - step(0.1, luminance);
        float maskCenter = 1.0 - smoothstep(0.1, 0.2, dist);

        float mask = maskColor * maskCenter;
        emissiveFactor = mask * float3(cpuColor.rgb) * emissionPower;
        pout.emissive = float4(emissiveFactor, GBUFFER_FLAG_NORMAL); // wの値 : スカイマップ１それ以外０    2: emissiveFlagとして使用
    }

    pout.material = float4(metallicFactor, roughnessFactor, occlusionFactor, materialType /*マテリアルタイプ*/);
    
    return pout;
}