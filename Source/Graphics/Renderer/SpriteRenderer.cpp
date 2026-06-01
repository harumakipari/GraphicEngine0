#include "pch.h"
#include "SpriteRenderer.h"

#include "Graphics/Core/Graphics.h"
#include "Graphics/Sprite/Sprite.h"


void SpriteRenderer::Draw(
    Sprite* sprite,
    const DirectX::XMFLOAT2& position,
    const DirectX::XMFLOAT2& size,
    const DirectX::XMFLOAT4& color,
    float angle
)
{
    if (!sprite) return;

    sprite->Render(
        Graphics::GetDeviceContext(),
        position.x,
        position.y,
        size.x,
        size.y,
        color.x,
        color.y,
        color.z,
        color.w,
        angle
    );
}

void SpriteRenderer::Draw(
    Sprite* sprite,
    const DirectX::XMFLOAT2& position,
    const DirectX::XMFLOAT2& size,
    const DirectX::XMFLOAT4& color,
    const SpriteUV& uv,
    float angle,
    const DirectX::XMFLOAT2& pivot,
    const DirectX::XMFLOAT2& scale
)
{
    if (!sprite ) return;

    sprite->Render(
        Graphics::GetDeviceContext(),
        position.x,
        position.y,
        size.x,
        size.y,
        color,
        angle,
        uv.x,
        uv.y,
        uv.w,
        uv.h,
        scale.x,
        scale.y,
        pivot.x,
        pivot.y
    );
}

// 0~9‚Ì”š‚É‘Î‰‚·‚éUV‚ğŒvZ‚µ‚Ä•Ô‚·
SpriteUV SpriteRenderer::CalcNumberUV(int number)
{
    number = std::clamp(number, 0, 9);

    const float digitCount = 10.0f;
    const float uWidth = 1.0f / digitCount;

    SpriteUV uv;
    uv.x = uWidth * number;
    uv.y = 0.0f;
    uv.w = uWidth;
    uv.h = 1.0f;

    return uv;
}
