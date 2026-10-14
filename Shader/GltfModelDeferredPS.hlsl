#include "GltfModel.hlsli"
#include "Sampler.hlsli"
#include "Common.hlsli"
#include "ShaderFunctions.hlsli"

#define BASE_COLOR_TEXTURE 0 
#define METALLIC_ROUGHNESS_TEXTURE 1 
#define NORMAL_TEXTURE 2 
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4 
Texture2D<float4> materialTextures[5] : register(t1);

Texture2D shadowMap : register(t15);
Texture2D clothOpacityTexture : register(t16);

cbuffer CLOTH_DEBUG_CONSTANT_BUFFER : register(b12)
{
    uint clothDebugMode;
    float3 clothDebugPadding;
}
static const int MATERIAL_CLOTH_DEBUG = 99;

cbuffer CLOTH_ALPHA_TEST_CONSTANT_BUFFER : register(b13)
{
    uint clothAlphaTestEnabled;
    float clothAlphaThreshold;
    float2 clothAlphaTestPadding;
}
// Cloth-only GBuffer material IDs. They preserve the selected mode through
// full-screen passes, after the cloth-local b12 constant buffer is unbound.
static const int MATERIAL_CLOTH_NO_SHADOW = MATERIAL_CLOTH_DEBUG + 4;
static const int MATERIAL_CLOTH_DIFFUSE_ONLY = MATERIAL_CLOTH_DEBUG + 5;
static const int MATERIAL_CLOTH_AMBIENT_ONLY = MATERIAL_CLOTH_DEBUG + 6;
static const int MATERIAL_CLOTH_NORMAL_DOT_LIGHT = MATERIAL_CLOTH_DEBUG + 7;
static const int MATERIAL_CLOTH_TWO_SIDED_DIFFUSE = MATERIAL_CLOTH_DEBUG + 8;
static const int MATERIAL_CLOTH_ABS_NORMAL_DOT_LIGHT = MATERIAL_CLOTH_DEBUG + 9;
static const int MATERIAL_CLOTH_DIRECT_DIFFUSE_TERM = MATERIAL_CLOTH_DEBUG + 10;
static const int MATERIAL_CLOTH_DIFFUSE_AMBIENT_TERM = MATERIAL_CLOTH_DEBUG + 11;
static const int MATERIAL_CLOTH_SIMULATION_NORMAL = MATERIAL_CLOTH_DEBUG + 15;
static const int MATERIAL_CLOTH_GEOMETRIC_NORMAL = MATERIAL_CLOTH_DEBUG + 16;
static const int MATERIAL_CLOTH_NORMAL_DIFFERENCE = MATERIAL_CLOTH_DEBUG + 17;

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
    // The opacity mask is bound only by ClothSimulate at t16.  Keep this
    // branch material-type guarded so ordinary glTF draws never read t16.
    if (materialType == MATERIAL_CLOTH_DEBUG && clothAlphaTestEnabled != 0u)
    {
        const float clothOpacity = clothOpacityTexture.Sample(samplerStates[LINEAR], pin.texcoord).r;
        clip(clothOpacity - clothAlphaThreshold);
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

    // This is the normal produced by ClothSimulateVS: source glTF normal,
    // per-vertex constraint quaternion, then the cloth world matrix.
    const float3 simulationFrameNormal = normalize(pin.wNormal.xyz);

    // Position derivatives are constant within each rasterized triangle.  The
    // SV_IsFrontFace correction retains the triangle winding sign so that a
    // reversed winding can be distinguished from a merely tilted normal.
    const float3 positionDx = ddx(pin.wPosition.xyz);
    const float3 positionDy = ddy(pin.wPosition.xyz);
    const float3 geometricNormal = normalize(cross(positionDy, positionDx)) * (isFrontFace ? 1.0f : -1.0f);

    float3 N = simulationFrameNormal;
    float3 T = hasTangent ? normalize(pin.wTangent.xyz) : float3(1, 0, 0.0001);
    float sigma = hasTangent ? pin.wTangent.w : 1.0;
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T) * sigma);

    // TODO:アンチエイリアス対策にラフネスを下げる処理を追加
    //roughnessFactor = SpecularAntiAliasing(roughnessFactor, N, 0.05f);


    //背面については、接線方向の基底ベクトルは符号が反転する。
    if (isFrontFace == false)
    {
        T = -T;
        B = -B;
        N = -N;
    }
    
    const int normalTexture = m.normalTexture.index;
    if (normalTexture > -1 && !(materialType == MATERIAL_CLOTH_DEBUG && clothDebugMode == 3u))
    {
        float4 sampled = materialTextures[NORMAL_TEXTURE].Sample(samplerStates[LINEAR], pin.texcoord);
        float3 normalFactor = sampled.xyz;
        normalFactor = (normalFactor * 2.0) - 1.0;
        normalFactor = normalize(normalFactor * float3(m.normalTexture.scale, m.normalTexture.scale, 1.0));
        N = normalize((normalFactor.x * T) + (normalFactor.y * B) + (normalFactor.z * N));
    }

    // Cloth-only diagnostic pixels use the existing OBJECT_NO_LIGHTING path.
    // This bypasses Deferred Lighting and FinalPS emissive addition without
    // changing the render treatment of any other model.
    if (materialType == MATERIAL_CLOTH_DEBUG && (clothDebugMode == 1u || clothDebugMode == 2u || clothDebugMode == 15u || clothDebugMode == 16u || clothDebugMode == 17u))
    {
        float3 debugColor = clothDebugMode == 1u ? baseColorFactor.rgb : (N * 0.5f + 0.5f);
        if (clothDebugMode == 16u)
        {
            // Signed geometric normal from the simulated triangle positions.
            debugColor = geometricNormal * 0.5f + 0.5f;
        }
        else if (clothDebugMode == 17u)
        {
            // Neutral gray is the unsigned angular error: 0 = parallel or
            // anti-parallel, 1 = perpendicular.  Red means the two normals
            // are anti-parallel, exposing a winding/sign mismatch separately.
            const float normalDot = dot(simulationFrameNormal, geometricNormal);
            const float unsignedAngle = acos(saturate(abs(normalDot))) * (2.0f / 3.14159265f);
            debugColor = normalDot < 0.0f
                ? float3(1.0f, unsignedAngle * 0.25f, unsignedAngle * 0.25f)
                : float3(unsignedAngle, unsignedAngle, unsignedAngle);
        }
        pout.gBuffer3Normal = float4(N.xyz, OBJECT_NO_LIGHTING);
        pout.velocity = float4(CalculateUvSpaceVelocity(pin.currentClipPosition, pin.previousClipPosition), 0, 1);
        pout.albedo = float4(debugColor, baseColorFactor.a);
        pout.position = pin.wPosition;
        pout.emissive = float4(0, 0, 0, GBUFFER_FLAG_NORMAL);
        pout.material = float4(metallicFactor, roughnessFactor, occlusionFactor, materialType + clothDebugMode);
        return pout;
    }
    // Per-material rim values use the component-owned runtime material override.
    // Materials without an override keep their default zero power and are unchanged.
    const float3 viewDirection = normalize(cameraPosition.xyz - pin.wPosition.xyz);
    const float rimFactor = pow(saturate(1.0f - dot(N, viewDirection)), 3.0f);
    emissiveFactor += m.rimColor * (m.rimPower * rimFactor);

    // Cloth uses its dedicated material type and does not bind the generic
    // object-type constant buffer. Preserve materialType for Cloth diagnostics,
    // while carrying the existing Furniture classification through the GBuffer.
    const int gBufferObjectType = materialType == MATERIAL_CLOTH_DEBUG
        ? OBJECT_FURNITURE
        : objectType;
    pout.gBuffer3Normal = float4(N.xyz, gBufferObjectType); // world space

    float2 velocity = CalculateUvSpaceVelocity(pin.currentClipPosition, pin.previousClipPosition);
    pout.velocity = float4(velocity, 0, 1);

    if (objectType == OBJECT_STAGE || objectType == OBJECT_DOOR || objectType == OBJECT_FURNITURE)
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

    // Deferred Lighting and PostEffectPS are full-screen passes and cannot
    // read b12, so retain the Cloth mode in the GBuffer material type.
    const int gBufferMaterialType = materialType == MATERIAL_CLOTH_DEBUG
        ? materialType + clothDebugMode
        : materialType;
    pout.material = float4(metallicFactor, roughnessFactor, occlusionFactor, gBufferMaterialType /*マテリアルタイプ*/);
    
    return pout;
}