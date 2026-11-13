#pragma once
#include "UI/Widgets/Widget.h"

class UIManager
{
public:
    void Update(float deltaTime);

    void Draw(ID3D11DeviceContext* immediateContext) const;

    void DrawImGui();

    void DrawFont(ID3D11DeviceContext* immediateContext) const;

    void DrawSceneChangeSprite(ID3D11DeviceContext* immediateContext) const;

    void Add(const std::shared_ptr<UICoreComponent>& ui)
    {
        if (!ui) return;
        pendingAdd.push_back(ui);
    }

    // コントローラー対応するボタンを追加
    void AddButton(const std::shared_ptr<UIButtonComponent>& ui)
    {
        if (!ui) return;
        buttons.push_back(ui);
    }

    void Cleanup();

    void Clear()
    {
        rootComponents.clear();
    }

    void SetAllUIActive(bool visible, bool enabled);
    bool IsMouseCaptured() const { return mouseCaptured; }
    void SetMouseCaptured(const bool isCaptured) { mouseCaptured = isCaptured; }
    void SetNavigationEnabled(const bool isEnabled) { navigationEnabled = isEnabled; }
    void SetSelected(UIButtonComponent* button);
    UIButtonComponent* GetSelectedButton() const { return selectedButton; }
private:
    // ゲームパッドでUIを操作
    void HandleGamepadUI(float deltaTime);
    // 選択切り替え処理
    void MoveSelection(const int direction);
private:
    std::vector<std::shared_ptr<UICoreComponent>> rootComponents;
    std::vector<std::shared_ptr<UICoreComponent>> pendingAdd; // 追加待ち

    UIButtonComponent* selectedButton = nullptr; // 現在選択されているボタン（コントローラー、キーボード操作用）
    std::vector<std::shared_ptr<UIButtonComponent>> buttons;

    bool navigationEnabled = true;
    bool visible = true;
    bool enabled = true;
    bool mouseCaptured = false;
};
