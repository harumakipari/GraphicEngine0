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


void UITextPopup::Update(float dt)
{
    easingRunner->Tick(dt);
}
UIArrowComponent::UIArrowComponent(const std::string& filename, const std::string& name) :UIImageComponent(filename, name)
{
    UIImageComponent(filename, name);
    texture = std::make_shared<Sprite>(Graphics::GetDevice(), std::wstring(filename.begin(), filename.end()).c_str());
    uv.w = texture->GetTextureSize().x;
    uv.h = texture->GetTextureSize().y;

    //this->SetVisible(false);

#if 0
    auto uiManager = Scene::GetCurrentScene()->GetUIManager();

    for (int i = 0; i < 10; i++)
    {
        auto rect = std::make_shared<UIImageComponent>("./Data/Textures/ScissorsUI/arrow_rect.png", "arrow_rect");
        rect->SetSize({ 230, 122 });
        rect->SetPivot({ 0.0f,0.5f });
        uiManager->Add(rect);
        bodyRects.push_back(rect);
    }

    head = std::make_shared<UIImageComponent>("./Data/Textures/ScissorsUI/arrow_triangle.png", "arrow_triangle");
    head->SetSize({ 58, 68 });
    head->SetPivot({ 0.0f,0.5f });
    uiManager->Add(head);


#endif // 0
}

void UIArrowComponent::SetEnd(DirectX::XMFLOAT3 endPos)
{
    this->endPos = endPos;
    // 目的地のスクリーン座標
    XMFLOAT2 uiTargetPos = WorldToUI(endPos);
    // 開始の位置のスクリーン座標
    XMFLOAT2 uiStartPos = WorldToUI(startPos);

    float distance = MathHelper::DistanceFloat2(uiTargetPos, uiStartPos);

    float arrowSizeX = size.x;
    float uiScale = abs(distance) / arrowSizeX;
    SetScale({ uiScale,1.0f });

    //　方向ベクトル
    DirectX::XMFLOAT2 dir = MathHelper::SubtractFloat2(uiTargetPos, uiStartPos);
    // 正規化
    if (distance > 0.0001f)
    {
        dir.x /= distance;
        dir.y /= distance;
    }

    float angle = atan2f(dir.y, dir.x);
    float degree = XMConvertToDegrees(angle);

#if 0
    int segmentLength = 230;    // レクトの横の長さ
    int segmentCount = static_cast<int>(distance / segmentLength);
    for (int i = 0; i < bodyRects.size(); i++)
    {
        if (i < segmentCount)
        {
            float offset = i * segmentLength;

            XMFLOAT2 pos =
            {
                uiStartPos.x + dir.x * offset,
                uiStartPos.y + dir.y * offset
            };

            bodyRects[i]->SetWorldPosition(pos);

            bodyRects[i]->SetWorldAngleDegree(degree);

            bodyRects[i]->SetVisible(true);
        }
        else
        {
            bodyRects[i]->SetVisible(false);
        }
    }
    head->SetWorldPosition(uiTargetPos);
    head->SetWorldAngleDegree(degree);
    head->SetVisible(true);
#endif

    SetWorldAngleDegree(degree);

    SetWorldPosition(uiStartPos);
}
