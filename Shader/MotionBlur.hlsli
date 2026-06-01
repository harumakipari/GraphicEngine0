#ifndef __MOTION_BLUR_HLSLI__
#define __MOTION_BLUR_HLSLI__

#include "Constants.hlsli"
#include "GBuffer.hlsli"
#include "sprite.hlsli"

//  速度バッファをタイル化する目的で使用するブロックサイズ
//  ここはアプリ側と同期しておく必要がある
static const int TileSize = 20;

//  速度ベクトルをタイルサイズ分に拡大させる
float2 ReconstVelocity(float2 tv)
{
    return tv * TileSize;
}

#endif // __MOTION_BLUR_HLSLI__
