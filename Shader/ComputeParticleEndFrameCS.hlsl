#include "ComputeParticle.hlsli"

RWByteAddressBuffer indirectDataBuffer : register(u2);

[numthreads(1, 1, 1)]
void main()
{
    //死亡カウンターを取得＆初期化
    uint destroyCounter = indirectDataBuffer.Load(IndirectArgumentsNumDeadParticle);
    indirectDataBuffer.Store(IndirectArgumentsNumDeadParticle, 0);
    
    //現在のフレームでのパーティクル総数を再計算
    uint currentNumParticle = indirectDataBuffer.Load(IndirectArgumentsNumCurrentParticle);
    indirectDataBuffer.Store(IndirectArgumentsNumCurrentParticle, currentNumParticle - destroyCounter);
    
    //描画コール数をここで決める
    indirectDataBuffer.Store(IndirectArgumentsDrawIndirect, currentNumParticle - destroyCounter);
}