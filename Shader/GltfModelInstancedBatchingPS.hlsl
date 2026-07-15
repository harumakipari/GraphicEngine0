#include "GltfModel.hlsli"
#include "ModelType.hlsli"
#include "Sampler.hlsli"

#define BASE_COLOR_TEXTURE 0 
#define METALLIC_ROUGHNESS_TEXTURE 1 
#define NORMAL_TEXTURE 2 
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4 
Texture2D<float4> materialTextures[5] : register(t1);
Texture2D shadowMap : register(t15);

GBUFFER_PS_OUT main(INSTANCE_VS_OUT pin, bool isFrontFace : SV_IsFrontFace)
{
    GBUFFER_PS_OUT pout;

    // インスタンス時は定数バッファを個別に送れていないので、
    int instanceObjectType = pin.instancePlusParameter.y;

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

        if (instanceObjectType == OBJECT_PLAYER || instanceObjectType == OBJECT_ENEMY)
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

    instanceObjectType = OBJECT_FURNITURE;  // ここで家具にする
    pout.gBuffer3Normal = float4(N.xyz, instanceObjectType); // world space

    float2 velocity = CalculateUvSpaceVelocity(pin.currentClipPosition, pin.previousClipPosition);
    pout.velocity = float4(velocity, 0, 1);

    if (instanceObjectType == OBJECT_STAGE || instanceObjectType == OBJECT_DOOR || instanceObjectType == OBJECT_FURNITURE)
    { // 影の値を入れる
        const float shadow_depth_bias = 0.001;
        float4 light_view_position = mul(pin.wPosition, lightViewProjection); // World to Clip space
        light_view_position = light_view_position / light_view_position.w; // Clip to NDC
        float2 light_view_texcoord = 0;
	    // NDC to Texture coordinate
        light_view_texcoord.x = light_view_position.x * +0.5 + 0.5;
        light_view_texcoord.y = light_view_position.y * -0.5 + 0.5;
        float depth = saturate(light_view_position.z - shadow_depth_bias);
        float shadow_factor = 1.0f;
        shadow_factor = shadowMap.SampleCmpLevelZero(comparisionSamplerState, light_view_texcoord, depth).x;
        pout.velocity.w = shadow_factor;
    }

    pout.albedo = baseColorFactor;

    pout.position = pin.wPosition; // world space 

    pout.emissive = float4(emissiveFactor, GBUFFER_FLAG_NORMAL); // wの値 : スカイマップ１それ以外０    2: emissiveFlagとして使用

    pout.material = float4(metallicFactor, roughnessFactor, occlusionFactor, materialType /*マテリアルタイプ*/);
    
    return pout;
}
