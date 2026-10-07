#pragma once
#include "Core/Actor.h"
#include "UI/Widgets/Widget.h"

class Pause :public Actor
{
public:
    enum class PauseState :uint8_t
    {
        Playing,
        Paused,
    };

public:
    explicit Pause(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;
    void DrawImGuiDetails() override;

    // ���g���C����V�[���̖��O��ݒ肷��
    void SetRetrySceneName(const std::string& sceneName) { retrySceneName = sceneName; }

    // �|�[�Y��ʂ�B��
    void HidePauseMenu();

private:
    // �|�[�Y��ʂ�J���Ƃ��̏���
    void OpenPause();

    // �|�[�Y��ʂ���鎞�̏���
    void ClosePause();
    bool CanOpenPause() const;
    void UpdateSelectionLines();
    void HideSelectionLines();
    void UpdatePauseButtonScales(float deltaTime);
    void ResetPauseButtonScales();
    void ApplyButtonLayout();
    float GetPauseButtonDisplayScale(const UIButtonComponent* button) const;
    void SetPauseButtonAction(const std::shared_ptr<UIButtonComponent>& button, std::function<void()> action);


private:
    std::shared_ptr<UIImageComponent> pauseBackImage; //�|�[�Y���̔w�i
    std::shared_ptr<UIImageComponent> pausePanel;
    std::shared_ptr<UIButtonComponent> menuButton;
    std::shared_ptr<UIButtonComponent> returnTitleButton;
    std::shared_ptr<UIButtonComponent> returnMainRoomButton;
    std::shared_ptr<UIButtonComponent> battleActionButton;
    std::shared_ptr<UIButtonComponent> backToRoomButton;
    std::shared_ptr<UIButtonComponent> quitGameButton;
    std::shared_ptr<UIImageComponent> selectionLineLeft;
    std::shared_ptr<UIImageComponent> selectionLineRight;
    UIButtonComponent* lastSelectedButton = nullptr;

    float selectionLineAnimationProgress = 0.0f;
    float selectionLineWidthRatio = 0.87f;
    float selectionLineHeight = 5.0f;
    float selectionLineYOffset = 0.0f;
    DirectX::XMFLOAT2 selectionLineScale{ 0.12f, 0.4f };
    float selectionLineGap = 15.0f;
    float selectionLineAnimationDuration = 0.15f;

    float selectedButtonScale = 1.16f;
    float buttonScaleAnimationDuration = 0.12f;
    float menuCenterY = 567.0f;
    float buttonSpacing = 80.0f;
    float quitExtraGap = 0.0f;
    float returnTitleButtonDisplayScale = 0.5f;
    float battleActionButtonDisplayScale = 0.5f;
    float backToRoomButtonDisplayScale = 0.5f;
    float quitGameButtonDisplayScale = 0.5f;

    PauseState state = PauseState::Playing;
    bool stopUpdate = true;

    std::string retrySceneName = "MainScene";
};
