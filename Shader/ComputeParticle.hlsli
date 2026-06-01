
//パーティクルスレッド数
static const int NumParticleThread = 1024;

//生成パーティクル構造体
struct EmitParticleData
{
    float4 parameter; // x : パーティクル処理タイプ, y : 残り生存時間, z : 生存時間, w : 空き

    float4 position; // 生成座標
    float4 rotation; // 回転情報
    float4 scale; //拡縮情報 (xy:startSize, zw: endSize)

    float4 velocity; // 初速
    float4 acceleration; // 加速度
    
    float4 startColor; // 開始色情報
    float4 endColor; // 終了色情報
    
    float4 customData; //カスタムデータ
};

//パーティクル構造体
struct ParticleData
{
    float4 parameter; // x : パーティクル処理タイプ, y : 残り生存時間, z : 生存時間,　w : 空き

    float4 position; // 生成座標
    float4 rotation; // 回転情報
    float4 scale; //拡縮情報 (xy:startSize, zw: endSize)

    float4 velocity; // 初速
    float4 acceleration; // 加速度

    float4 texcoord; //  UV座標
    float4 color; // 色情報
    float4 startColor; // 開始色情報
    float4 endColor; // 終了色情報

    float4 customData; // カスタムデータ
};

//パーティクルヘッダー構造体
struct ParticleHeader
{
    uint alive; //生存フラグ
    uint particleIndex; //パーティクル番号
    float depth; //深度
    uint dummy;
};

//IndirectDataBufferへのアクセス用バイトオフセット
static const uint IndirectArgumentsNumCurrentParticle = 0;
static const uint IndirectArgumentsNumPreviousParticle = 4;
static const uint IndirectArgumentsNumDeadParticle = 8;
static const uint IndirectArgumentsNumEmitParticleDispatchIndirect = 12;

//DrawIndirect用構造体
struct DrawIndirect
{
    uint vertexCountPerInstance;
    uint instanceCount;
    uint startVertexLocation;
    uint startInstanceLocation;
};
static const uint IndirectArgumentsUpdateParticleDispatchIndirect = 24;
static const uint IndirectArgumentsNumEmitParticleIndex = 36;
static const uint IndirectArgumentsNumEmitPixelParticleIndex = 40;
static const uint IndirectArgumentsDrawIndirect = 44;

//=========================================================================================
//  汎用情報
cbuffer COMPUTE_PARTICLE_COMMON_CONSTANT_BUFFER : register(b10)
{
    uint2 textureSplitCount;
    uint systemNumParticles;
    uint totalEmitCount;
    
    uint maxEmitParticles;
    uint2 commonDummy;
};

//バイトニックソート情報
cbuffer COMPUTE_PARTICLE_BITONIC_SORT_CONSTANT_BUFFER : register(b11)
{
    uint increment;
    uint direction;
    uint sortDummy[2];
};
static const uint BitonicSortB2Thread = 256;
static const uint BitonicSortC2Thread = 512;

//=========================================================================================
//  頂点シェーダーからジオメトリシェーダーに転送する情報
struct GS_IN
{
    uint vertexId : VERTEX_ID;
};

//  ジオメトリシェーダーからピクセルシェーダーに転送する情報
struct PS_IN
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
    float emissive : TEXCOORD1;
};
