#include "pch.h"
#include "UIManager.h"
#include <imgui.h>

#include "FontManager.h"
#include "Engine/Utility/Time.h"


void UIManager::Update(float deltaTime)
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

    //FontManager::GetUIFont()->Begin(immediateContext);

    for (auto& ui : sortedComponents)
    {
        if (ui->IsVisible())
        {
            ui->Draw(immediateContext);
        }
    }

    //RenderState::BindBlendState(immediateContext, BLEND_STATE::ALPHA);
    //RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
    //RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);

    //FontManager::GetUIFont()->End(immediateContext);
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
            if (ui->IsVisible())
            {
                ui->DrawTexts(immediateContext);
                //ui->ClearDirty();
            }
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
            if (ui->IsVisible())
            {
                ui->DrawSceneChangeSprite(immediateContext);
                //ui->ClearDirty();
            }
        }
    }
    FontManager::GetUIFont()->End(immediateContext);

}

void UIManager::DrawImGUi()
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

void UIManager::SetAllUIActive(bool visible, bool enabled)
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

void UIManager::SetSelected(UIButtonComponent* btn)
{
    if (selectedButton)
        selectedButton->state = UIButtonState::Normal;

    selectedButton = btn;

    if (selectedButton)
        selectedButton->state = UIButtonState::Selected;
}

void UIManager::HandleGamepadUI(float deltaTime)
{
    static float stickDelay = 0.0f;
    stickDelay -= deltaTime;

    bool moved = false;

    // =========================
    // D-Pad入力（優先・1回だけ）
    // =========================
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

    // =========================
    // スティック入力（D-Pad優先）
    // =========================
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

    // =========================
    // 決定ボタン
    // =========================
    if (InputSystem::GetInputState("UISubmit", InputStateMask::Trigger))
    {
        if (selectedButton)
        {
            selectedButton->OnClick();
        }
    }
}

// 選択切り替え処理
void UIManager::MoveSelection(int dir)
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

    index = std::clamp(index + dir, 0, static_cast<int>(activeButtons.size()) - 1);
    SetSelected(activeButtons[index]);
}