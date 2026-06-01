#pragma once
#include <DirectXMath.h>

class Sprite;



// UV用の簡単なRect
struct SpriteUV
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 1.0f;
    float h = 1.0f;
};

class SpriteRenderer
{
public:
    // 一番基本（UIImage用）
    static void Draw(
        Sprite* sprite,
        const DirectX::XMFLOAT2& position,
        const DirectX::XMFLOAT2& size,
        const DirectX::XMFLOAT4& color = { 1,1,1,1 },
        float angle = 0.0f
    );

    // UV付き（Button / Gauge 用）
    static void Draw(
        Sprite* sprite,
        const DirectX::XMFLOAT2& position,
        const DirectX::XMFLOAT2& size,
        const DirectX::XMFLOAT4& color,
        const SpriteUV& uv,
        float angle = 0.0f,
        const DirectX::XMFLOAT2& pivot = { 0.0f, 0.0f },
        const DirectX::XMFLOAT2& scale = { 1.0f, 1.0f }
    );

    // 0~9の数字に対応するUVを計算して返す
    static SpriteUV CalcNumberUV(int number);


private:
};