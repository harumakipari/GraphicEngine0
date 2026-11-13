#include "pch.h"
#include "Widget.h"

#include <imgui.h>

#include "Engine/Scene/Scene.h"
#include "Physics/CollisionFunction.h"

inline XMFLOAT2 Rotate2D(const XMFLOAT2& v, const float rad)
{
    float c = cosf(rad);
    float s = sinf(rad);
    return {
        v.x * c - v.y * s,
        v.x * s + v.y * c
    };
}

void UICoreComponent::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Name: %s", name.c_str());
    ImGui::Checkbox("Visible", &visible);
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::DragFloat2("worldPosition", &worldPosition.x, 1.0f);
    ImGui::DragFloat2("localPosition", &localPosition.x, 1.0f);
    ImGui::DragFloat2("Size", &size.x, 1.0f);
    ImGui::DragFloat2("Scale", &scale.x, 0.2f);
    ImGui::SliderFloat("worldAngle", &worldAngle, 0.0f, 360.0f);
    ImGui::SliderFloat2("pivot", &pivot.x, 0.0f, 1.0f);

#endif
}

void UICoreComponent::UpdateTransform()
{
    if (parent)
    {
        // 親の回転をラジアンで取得
        float parentRad = DirectX::XMConvertToRadians(parent->worldAngle);

        // localPosition を回転
        XMFLOAT2 rotatedLocal = Rotate2D(localPosition, parentRad);

        // 平行移動
        worldPosition.x = parent->worldPosition.x + rotatedLocal.x;
        worldPosition.y = parent->worldPosition.y + rotatedLocal.y;

        // 角度は加算
        worldAngle = parent->worldAngle + localAngle;
    }

    for (auto* child : children)
    {
        child->UpdateTransform();
    }
}

void UICoreComponent::SetParent(UICoreComponent* newParent)
{
    if (parent == newParent) return;

    // 既存の親から外す
    if (parent)
    {
        auto& siblings = parent->children;
        siblings.erase(
            std::remove(siblings.begin(), siblings.end(), this),
            siblings.end()
        );
    }

    parent = newParent;

    if (parent)
    {
        parent->children.push_back(this);
    }
}

void UIImageComponent::Draw(ID3D11DeviceContext* immediateContext)
{
    SpriteRenderer::Draw(texture.get(), worldPosition, size, color, uv, worldAngle, pivot, scale);
}

void UIButtonComponent::Update(const float dt)
{
    if (InputSystem::IsGamepadConnected())
    {// ゲームパッドをつないでいたら
        // マウス処理しない（重要）
        UpdateVisual();
        if (useHoverScale)
        {
            UpdateScale(dt);
        }
        return;
    }


    DirectX::XMFLOAT2 cursor = InputSystem::GetMousePositionScreen();
    if (!InputSystem::GetMousePositionUI(cursor))
    {
        state = UIButtonState::Normal;
        return;
    }
    bool inside = IsInside(cursor);

    if (inside)
    {
        Scene::GetCurrentScene()->GetUIManager()->SetMouseCaptured(true);

        InputSystem::isUIUsingMouse = true;


        // マウスカーソルを取得
        if (InputSystem::GetInputState("MouseLeft"))
        {// 左ボタンを押している間
            state = UIButtonState::Pressed;
        }
        else
        {
            if (state == UIButtonState::Pressed &&
                InputSystem::GetInputState("MouseLeft", InputStateMask::Release))
            {
                OnClick();
            }
            state = UIButtonState::Hovered;
        }
    }
    else
    {
        state = UIButtonState::Normal;
    }

    UpdateVisual();

    if (useHoverScale)
    {
        UpdateScale(dt);
    }
}



bool UIButtonComponent::IsInside(const DirectX::XMFLOAT2& p) const
{
    float left = worldPosition.x - size.x * pivot.x;
    float top = worldPosition.y - size.y * pivot.y;

    float right = left + size.x;
    float bottom = top + size.y;

    return p.x >= left && p.x <= right &&
        p.y >= top && p.y <= bottom;
}

void UIButtonComponent::UpdateVisual()
{
    switch (state)
    {
    case UIButtonState::Normal:
        color = normalColor;
        targetScale = normalScale;
        break;
    case UIButtonState::Hovered:
        color = hoveredColor;
        targetScale = hoverScale;
        break;
    case UIButtonState::Pressed:
        targetScale = pressScale;
        color = CoreColor(0.8f, 0.8f, 0.8f, 1);
        break;
    case UIButtonState::Selected:
        color = hoveredColor;
        targetScale = hoverScale;
        break;
    }
}

void UIButtonComponent::UpdateScale(const float deltaTime)
{
    currentScale = std::lerp(currentScale, targetScale, deltaTime * scaleSpeed);

    scale = { currentScale,currentScale };

}


void UIGaugeFillComponent::Draw(ID3D11DeviceContext* immediateContext)
{
    if (value <= 0.0f)
        return;

    XMFLOAT2 drawSize = size;
    SpriteUV drawUv = uv;

    if (horizontal)
    {
        drawSize.x *= value;
        drawUv.w *= value;
    }
    else
    {
        drawSize.y *= value;
        drawUv.h *= value;
    }

    SpriteRenderer::Draw(texture.get(), worldPosition, drawSize, color, drawUv, worldAngle, pivot, scale);
}

void UIGaugeComponent::Draw(ID3D11DeviceContext* immediateContext)
{
    XMFLOAT2 drawSize = gaugeFillSize;

    if (horizontal)
        drawSize.x *= value;
    else
        drawSize.y *= value;

    SpriteRenderer::Draw(texture.get(), { worldPosition.x + gaugeOffset.x,worldPosition.y + gaugeOffset.y }, drawSize, gaugeFillColor, uv, worldAngle, pivot, scale);

    SpriteRenderer::Draw(frameTexture.get(), worldPosition, size, gaugeFrameColor, { uv.x,uv.y,size.x,size.y }, worldAngle, pivot, scale);
}

void UISceneChangeComponent::DrawSceneChangeSprite(ID3D11DeviceContext* immediateContext)
{
    SpriteRenderer::Draw(texture.get(), worldPosition, size, color, uv, worldAngle, pivot, scale);
}
