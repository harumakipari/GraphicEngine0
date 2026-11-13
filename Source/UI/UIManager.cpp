#include "pch.h"
#include "UIManager.h"

#include <imgui.h>
#include "FontManager.h"
#include "Engine/Utility/Time.h"


void UIManager::Update(const float deltaTime)
{
    if (!enabled) return;

    // コントローラー用のUIを操作  
    if (navigationEnabled) HandleGamepadUI(Time::UnscaledDeltaTime());

    if (!pendingAdd.empty())
    {
        for (auto& ui : pendingAdd)
        {
            rootComponents.push_back(ui);
        }
        pendingAdd.clear();
    }

    mouseCaptured = false;
    for (auto& ui : rootComponents)
    {
        if (!ui) continue;
        if (ui->IsEnabled())
        {
            ui->UpdateTransform();
            ui->Update(deltaTime);
        }
    }

    // 削除処理
    Cleanup();
}

void UIManager::Draw(ID3D11DeviceContext* immediateContext)const
{
    if (!visible) return;

    // 描画順にソート zOrderが大きいほど後に描画される
    std::vector<std::shared_ptr<UICoreComponent>> sortedComponents = rootComponents;
    std::sort(sortedComponents.begin(), sortedComponents.end(),
              [](const std::shared_ptr<UICoreComponent>& a, const std::shared_ptr<UICoreComponent>& b)
              {
                  return a->zOrder < b->zOrder;
              });

    for (auto& ui : sortedComponents)
    {
        if (ui->IsVisible())
        {
            ui->Draw(immediateContext);
        }
    }
}

void UIManager::DrawImGui()
{

    if (ImGui::Begin("UI Manager"))
    {
        ImGui::Checkbox("Visible", &visible);
        ImGui::Checkbox("Enabled", &enabled);

        ImGui::Separator();

        for (size_t i = 0; i < rootComponents.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            rootComponents[i]->DrawImGui();
            ImGui::PopID();
        }

        if (ImGui::Button("Clear All"))
        {
            Clear();
        }
    }
    ImGui::End();
}

void UIManager::DrawFont(ID3D11DeviceContext* immediateContext)const
{
    if (!visible) return;

    // 描画順にソート zOrderが大きいほど後に描画される
    std::vector<std::shared_ptr<UICoreComponent>> sortedComponents = rootComponents;
    std::sort(sortedComponents.begin(), sortedComponents.end(),
              [](const std::shared_ptr<UICoreComponent>& a, const std::shared_ptr<UICoreComponent>& b)
              {
                  return a->zOrder < b->zOrder;
              });


    FontManager::GetUIFont()->Begin(immediateContext);
    for (auto& ui : sortedComponents)
    {
        if (ui->IsVisible())
        {
            ui->DrawTexts(immediateContext);
        }
    }
    FontManager::GetUIFont()->End(immediateContext);

}

void UIManager::DrawSceneChangeSprite(ID3D11DeviceContext* immediateContext)const
{
    if (!visible) return;

    // 描画順にソート zOrderが大きいほど後に描画される
    std::vector<std::shared_ptr<UICoreComponent>> sortedComponents = rootComponents;
    std::sort(sortedComponents.begin(), sortedComponents.end(),
              [](const std::shared_ptr<UICoreComponent>& a, const std::shared_ptr<UICoreComponent>& b)
              {
                  return a->zOrder < b->zOrder;
              });


    FontManager::GetUIFont()->Begin(immediateContext);
    for (auto& ui : sortedComponents)
    {
        if (ui->IsVisible())
        {
            ui->DrawSceneChangeSprite(immediateContext);
        }
    }
    FontManager::GetUIFont()->End(immediateContext);

}

void UIManager::Cleanup()
{
    rootComponents.erase(
        std::remove_if(
            rootComponents.begin(),
            rootComponents.end(),
            [](const std::shared_ptr<UICoreComponent>& ui)
            {
                return ui->IsPendingKill();
            }
        ),
        rootComponents.end()
    );
}

void UIManager::SetAllUIActive(const bool visible, const bool enabled)
{
    this->visible = visible;
    this->enabled = enabled;

    for (auto& ui : rootComponents)
    {
        if (auto sceneChange = std::dynamic_pointer_cast<UISceneChangeComponent>(ui))
        {
            continue;
        }
        if (!ui) continue;

        ui->SetVisible(visible);
        ui->SetEnable(enabled);
    }
}

void UIManager::SetSelected(UIButtonComponent* button)
{
    if (selectedButton)
        selectedButton->state = UIButtonState::Normal;

    selectedButton = button;

    if (selectedButton)
        selectedButton->state = UIButtonState::Selected;
}

void UIManager::HandleGamepadUI(const float deltaTime)
{
    static float stickDelay = 0.0f;
    stickDelay -= deltaTime;

    bool moved = false;

    // D-Pad入力（優先・1回だけ）
    if (InputSystem::GetInputState("UIUp", InputStateMask::Trigger) ||
        InputSystem::GetInputState("UILeft", InputStateMask::Trigger))
    {
        MoveSelection(-1);
        stickDelay = 0.2f;
        moved = true;
    }
    else if (InputSystem::GetInputState("UIDown", InputStateMask::Trigger) ||
        InputSystem::GetInputState("UIRight", InputStateMask::Trigger))
    {
        MoveSelection(1);
        stickDelay = 0.2f;
        moved = true;
    }

    // スティック入力（D-Pad優先）
    if (!moved && stickDelay <= 0.0f)
    {
        const auto stick = InputSystem::GetLeftStick();

        if (stick.y > 0.6f || stick.x < -0.6f)
        {
            MoveSelection(-1);
            stickDelay = 0.2f;
        }
        else if (stick.y < -0.6f || stick.x > 0.6f)
        {
            MoveSelection(1);
            stickDelay = 0.2f;
        }
    }

    // 決定ボタン
    if (InputSystem::GetInputState("UISubmit", InputStateMask::Trigger))
    {
        if (selectedButton)
        {
            selectedButton->OnClick();
        }
    }
}

// 選択切り替え処理
void UIManager::MoveSelection(const int direction)
{
    std::vector<UIButtonComponent*> activeButtons;
    activeButtons.reserve(buttons.size());
    for (const auto& button : buttons)
    {
        if (button && button->IsVisible() && button->IsEnabled())
            activeButtons.push_back(button.get());
    }

    if (activeButtons.empty())
        return;

    int index = 0;
    for (int i = 0; i < static_cast<int>(activeButtons.size()); ++i)
    {
        if (activeButtons[i] == selectedButton)
        {
            index = i;
            break;
        }
    }

    index = std::clamp(index + direction, 0, static_cast<int>(activeButtons.size()) - 1);
    SetSelected(activeButtons[index]);
}
