#include "ComputeParticle.hlsli"
#include "Constants.hlsli"

RWStructuredBuffer<ParticleData> particleDataBuffer : register(u0); //パーティクル管理バッファ
AppendStructuredBuffer<uint> particleUnusedBuffer : register(u1); //パーティクル番号管理バッファ（末尾への追加専用）
RWByteAddressBuffer indirectDataBuffer : register(u2); // インダイレクト用バッファ
RWStructuredBuffer<ParticleHeader> particleHeaderBuffer : register(u3); //パーティクルヘッダー管理バッファ

[numthreads(NumParticleThread, 1, 1)]
void main( uint3 dispatchThreadId : SV_DispatchThreadID )
{
    uint headerIndex = dispatchThreadId.x;
    
    ParticleHeader header = particleHeaderBuffer[headerIndex];
    
    uint dataIndex = header.particleIndex;
    
    //有効フラグが立っているものだけ処理
    if (header.alive == 0)
        return;
    
    ParticleData data = particleDataBuffer[dataIndex];
    
    //経過時間分減少させる
    data.parameter.y -= deltaTime;
    if (data.parameter.y < 0)
    {
        //寿命が尽きたら未使用リストに追加
        header.alive = 0;
        particleUnusedBuffer.Append(dataIndex);
        
        //　ヘッダー情報更新
        particleHeaderBuffer[headerIndex] = header;
        particleDataBuffer[dataIndex] = data;
        
        //死亡数をカウントする
        indirectDataBuffer.InterlockedAdd(IndirectArgumentsNumDeadParticle, 1);
        return;
    }
    
    
    switch ((int) (data.parameter.x + 0.5f))
    {
        default:
        {
            //速度更新
                data.velocity.xyz += data.acceleration.xyz * deltaTime;
        
            //位置更新
                data.position.xyz += data.velocity.xyz * deltaTime;
        
            // ライフタイム比率算出(z:寿命、y:残り寿命)
                float lifeRatio = (data.parameter.z - data.parameter.y) / data.parameter.z;
                uint frameCount = textureSplitCount.x * textureSplitCount.y;
            //切り取り座標を算出
                uint type = min((uint) (lifeRatio * frameCount), frameCount - 1); // clamping
            
                float w = 1.0 / textureSplitCount.x;
                float h = 1.0 / textureSplitCount.y;
                float2 uv = float2((type % textureSplitCount.x) * w, (type / textureSplitCount.x) * h);
                data.texcoord.xy = uv;
                data.texcoord.zw = float2(w, h);
        
            // 開始色から終了色へ線形補間
                data.color = lerp(data.startColor, data.endColor, lifeRatio);
                break;
            }
    }
        
    //深度ソート値算出
    header.depth = mul(float4(data.position.xyz, 1), viewProjection).w;

    //更新情報を格納
    particleHeaderBuffer[headerIndex] = header;
    particleDataBuffer[dataIndex] = data;
}