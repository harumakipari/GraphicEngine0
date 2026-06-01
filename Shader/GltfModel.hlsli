struct VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord : TEXCOORD;
    uint4 joints[2] : JOINTS;
    float4 weights[2] : WEIGHTS;
};

struct BATCH_VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord : TEXCOORD;
};

struct MORPH_VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord : TEXCOORD;
    uint4 joints[2] : JOINTS;
    float4 weights[2] : WEIGHTS;
    float4 morphPosition[4] : MORPH_POSITION;
    float4 morphNormal[4] : MORPH_NORMAL;
};

struct INSTANCE_VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord : TEXCOORD;
    row_major float4x4 instance_matrix : INSTANCE_MATRIX;

    float4 instanceColor : INSTANCE_COLOR;
    float4 instanceEmissive : INSTANCE_EMISSIVE;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 wPosition : POSITION;
    float4 wNormal : NORMAL;
    float4 wTangent : TANGENT;
    float2 texcoord : TEXCOORD;
};

struct INSTANCE_VS_OUT
{
    float4 position : SV_POSITION;
    float4 wPosition : POSITION;
    float4 wNormal : NORMAL;
    float4 wTangent : TANGENT;
    float2 texcoord : TEXCOORD;
    float4 instanceColor : INSTANCE_COLOR;
    float4 instanceEmissive : INSTANCE_EMISSIVE;
};

cbuffer PRIMITIVE_CONSTANT_BUFFER : register(b0)
{
    row_major float4x4 world;
    
    int material;
    bool hasTangent;
    int skin;
    int materialType;
    
    row_major float4x4 inverseTransposeWorld;
}


cbuffer PLUS_ALPHA_CONSTANT_BUFFER : register(b5)
{
    float4 cpuColor; // 色をCPU側で指定する用　（ダメージ当たったときとか）

    float modelHueShift; // 色相調整
    float modelSaturation; // 彩度調整
    float modelBrightness; // 明度調整
    float modelDissolve; // ディゾルブ用

    float4 morphWeights;

    float modelContrast;//コントラスト調整
    float emissionPower; // 自己発光の強さ
    float flashValue; //　白くフラッシュする値
    int objectType; // オブジェクトの種類 0:通常 1:プレイヤーとか
}


#include "Constants.hlsli"

struct TextureInfo
{
    int index;
    int texcoord;
};

struct NormalTextureInfo
{
    int index;
    int texcoord;
    float scale;
};

struct OcclusionTextureInfo
{
    int index;
    int texcoord;
    float strength;
};

struct PbrMetallicRoughness
{
    float4 baseColorFactor;
    TextureInfo basecolorTexture;
    float metallicFactor;
    float roughnessFactor;
    TextureInfo metallicRoughnessTexture;
};

struct MaterialConstants
{
    float3 emissiveFactor; // length 3. default [0, 0, 0]
    int alphaMode; // "OPAQUE" : 0, "MASK" : 1, "BLEND" : 2 
    float alphaCutoff; // default 0.5
    bool doubleSided; // default false;
    
    PbrMetallicRoughness pbrMetallicRoughness;
    
    NormalTextureInfo normalTexture;
    OcclusionTextureInfo occlusionTexture;
    TextureInfo emissiveTexture;
};

StructuredBuffer<MaterialConstants> materials : register(t0);

//PRIMITIVE_JOINT_CONSTANTS定数バッファを定義
static const uint PRIMITIVE_MAX_JOINTS = 512;
cbuffer PRIMITIVE_JOINT_CONSTANTS : register(b2)
{
    row_major float4x4 jointMatrices[PRIMITIVE_MAX_JOINTS];
}


// MULTIPLE_RENDER_TARGETS
struct PS_OUT
{
    float4 normal : SV_TARGET1;
    float4 color : SV_TARGET3;
    float4 position : SV_TARGET4; // world position
};

#include "GBuffer.hlsli"

//struct GBUFFER_PS_OUT
//{
//    float4 gbuffer1Normal : SV_TARGET1; // Normal (Octahedral) + Roughness + Metallic
//    float4 gbuffer2Color : SV_TARGET2; // BaseColor (RGB) +　occlusion
//    float4 position : SV_TARGET3; // world position 
//    float4 emissive : SV_TARGET4; // w:何かを書き込んでいたら０にするそれ以外は１　スカイマップなどの時に使用 
//};