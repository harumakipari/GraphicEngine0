#include "ComputeParticle.hlsli"
#include "Constants.hlsli"
#include "Sampler.hlsli"

//拡大行列生成
float4x4 MatrixScaling(float3 scale)
{
    float4x4 m;
    m._11 = scale.x;
    m._12 = 0.0f;
    m._13 = 0.0f;
    m._14 = 0.0f;
	
    m._21 = 0.0f;
    m._22 = scale.y;
    m._23 = 0.0f;
    m._24 = 0.0f;
	
    m._31 = 0.0f;
    m._32 = 0.0f;
    m._33 = scale.z;
    m._34 = 0.0f;
    
    m._41 = 0.0f;
    m._42 = 0.0f;
    m._43 = 0.0f;
    m._44 = 1.0f;
    return m;
}

//回転行列生成
float4x4 MatrixRotationRollPitchYaw(float3 rotation)
{
    float cp, sp;
    float cy, sy;
    float cr, sr;
    sincos(rotation.x, sp, cp);
    sincos(rotation.y, sy, cy);
    sincos(rotation.z, sr, cr);
    
    float4x4 m;
    m._11 = cr * cy + sr * sp * sy;
    m._12 = sr * cp;
    m._13 = sr * sp * cy - cr * sy;
    m._14 = 0.0f;
	
    m._21 = cr * sp * sy - sr * cy;
    m._22 = cr * cp;
    m._23 = sr * sy + cr * sp * cy;
    m._24 = 0.0f;
	
    m._31 = cp * sy;
    m._32 = -sp;
    m._33 = cp * cy;
    m._34 = 0.0f;
    
    m._41 = 0.0f;
    m._42 = 0.0f;
    m._43 = 0.0f;
    m._44 = 1.0f;
    return m;
}

//移動行列生成
float4x4 MatrixTranslation(float3 translation)
{
    float4x4 m;
    m._11 = 1.0f;
    m._12 = 0.0f;
    m._13 = 0.0f;
    m._14 = 0.0f;
	
    m._21 = 0.0f;
    m._22 = 1.0f;
    m._23 = 0.0f;
    m._24 = 0.0f;
	
    m._31 = 0.0f;
    m._32 = 0.0f;
    m._33 = 1.0f;
    m._34 = 0.0f;
    
    m._41 = translation.x;
    m._42 = translation.y;
    m._43 = translation.z;
    m._44 = 1.0f;
    return m;
}

//パーティクルバッファ
StructuredBuffer<ParticleData> particleDataBuffer : register(t0);
StructuredBuffer<ParticleHeader> particleHeaderBuffer : register(t1);

// カーブテクスチャ
Texture1DArray curveTextures : register(t10);

[maxvertexcount(4)]
void main(point GS_IN gin[1], inout TriangleStream<PS_IN> output)
{
    uint vertexId = gin[0].vertexId;
    
    //ヘッダーのParticleIndexがそのままパーティクルデータバッファのindexになる。
    bool isAlive = particleHeaderBuffer[vertexId].alive != 0;
    uint particleIndex = particleHeaderBuffer[vertexId].particleIndex;
    
    //残り生存時間と誕生時の生存時間から割合を計算する。
    float lifeTimeRate = !isAlive ? 0 : particleDataBuffer[particleIndex].parameter.y / particleDataBuffer[particleIndex].parameter.z;
    
    int curveIndex = (int) particleDataBuffer[particleIndex].customData.y;
    float t = lifeTimeRate;
    // カーブサンプリング
    float curveValue = curveTextures.SampleLevel(samplerStates[LINEAR_CLAMP], float2(t, curveIndex), 0);

    //生存していない場合はスケールを０にしておく
#if 0
    float2 size = !isAlive ? float2(0, 0) : lerp(particleDataBuffer[particleIndex].scale.zw, particleDataBuffer[particleIndex].scale.xy, lifeTimeRate);
#else

    float2 baseSize = particleDataBuffer[particleIndex].scale.xy;

    // カーブで倍率
    float2 size = baseSize * curveValue;

#endif

    //ビルボード行列生成（ビュー行列の逆行列でいい。ただし移動値はいらない）
    //TODO: InverseView行列を定数バッファに設定する。
    float4x4 billbordMatrix = invView;
    billbordMatrix._41_42_43 = float3(0, 0, 0);
    billbordMatrix._44 = 1.0f;
    
    //ワールド行列生成
    float4x4 scaleMatrix = MatrixScaling(float3(size, 1));

    //回転行列生成
    float4x4 rotationMatrix = (float4x4) 0;
    
    //描画モードによって回転行列を変更
    switch ((int) (particleDataBuffer[particleIndex].parameter.x + 0.5f))
    {
        case 0: // Billboard
            rotationMatrix = mul(billbordMatrix, MatrixRotationRollPitchYaw(particleDataBuffer[particleIndex].rotation.xyz));
            break;
        
        case 1: // StretchedBillboard
        {
                float3 velocity = particleDataBuffer[particleIndex].velocity;
                float speed = length(velocity);
                float3 dir = speed > 1e-5f ? normalize(velocity) : float3(0, 0, 1);

                float3 camRight = normalize(invView._m00_m01_m02);
                float3 camUp = normalize(cross(dir, camRight));
                camRight = normalize(cross(camUp, dir));

                rotationMatrix = float4x4(
            float4(camRight, 0),
            float4(camUp, 0),
            float4(dir, 0),
            float4(0, 0, 0, 1)
            );
                break;
            }
        case 2: // FixedRotation
            rotationMatrix = MatrixRotationRollPitchYaw(particleDataBuffer[particleIndex].rotation.xyz);
            break;
    }
    float4x4 translationMatrix = MatrixTranslation(particleDataBuffer[particleIndex].position.xyz);
    float4x4 worldMatrix = mul(mul(scaleMatrix, rotationMatrix), translationMatrix);
    float4x4 worldViewProjectionMatrix = mul(worldMatrix, viewProjection);
    
    //各種情報取得
    float4 texcoord = particleDataBuffer[particleIndex].texcoord;
    float4 color = particleDataBuffer[particleIndex].color;

    float emissive = particleDataBuffer[particleIndex].customData.x;

    //頂点生成
    static const float4 vertexPositions[4] =
    {
        float4(-0.5f, -0.5f, 0, 1),
        float4(+0.5f, -0.5f, 0, 1),
        float4(-0.5f, +0.5f, 0, 1),
        float4(+0.5f, +0.5f, 0, 1),
    };
    static const float2 vertexTexcoords[4] =
    {
        float2(0, 1),
        float2(1, 1),
        float2(0, 0),
        float2(1, 0),
    };
    for (uint i = 0; i < 4; i++)
    {
        PS_IN element;
        element.position = mul(vertexPositions[i], worldViewProjectionMatrix);
        element.texcoord = texcoord.xy + texcoord.zw * vertexTexcoords[i];
        element.color = color;
        element.emissive = emissive;
        output.Append(element);
    }
    output.RestartStrip();
}