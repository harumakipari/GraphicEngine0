#include "pch.h"
#include "Widget.h"

#include <imgui.h>

#include "Engine/Scene/Scene.h"
#include "Physics/CollisionFunction.h"


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

        //localPosition.x = worldPosition.x - parent->worldPosition.x;
        //localPosition.y = worldPosition.y - parent->worldPosition.y;

        //localAngle = worldAngle - parent->worldAngle;
    }
}

inline XMFLOAT2 Rotate2D(const XMFLOAT2& v, float rad)
{
    float c = cosf(rad);
    float s = sinf(rad);
    return {
        v.x * c - v.y * s,
        v.x * s + v.y * c
    };
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

void UIButtonComponent::Update(float dt)
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

void UIButtonComponent::UpdateScale(float deltaTime)
{
    currentScale = std::lerp(currentScale, targetScale, deltaTime * scaleSpeed);

    scale = { currentScale,currentScale };

}


