#pragma once
#include "Core/Actor.h"

class UIImageComponent;

class GameVictoryUIActor : public Actor
{
public:
    enum class VictoryRank :uint8_t
    {
        S, A, B, C
    };
public:
    explicit GameVictoryUIActor(const std::string& actorName) : Actor(actorName) {}
    void Initialize(const Transform& transform) override;

    void DrawImGuiDetails() override;


    void SetResultData(double clearTime, double bestTime, bool newRecord, VictoryRank rank);
    void SetNumberTextureSize(float width, float height);
    void ResetPresentation();
    void StartBackgroundFade();
    bool IsBackgroundStarted() const { return resultUITriggered; }
    void UpdateBackground();
    void ShowButtons();
    bool AdvanceButtonFade();
    void EnableInput();
    void DisableInput();
    int UpdateInput(float moveVolume);
    bool IsInputEnabled() const { return victoryButtonInputEnabled; }
    void UpdateButtonLayout();

private:
    void UpdateContents();
    void ResetButtons();
    std::shared_ptr<UIImageComponent> victoryResultBackground;
    double victoryClearTime = 0.0;
    double victoryDisplayedBestTime = 0.0;
    bool victoryIsNewRecord = false;
    VictoryRank victoryRank = VictoryRank::C;
    float numberTexWidth = 198.0f;
    float numberTexHeight = 300.0f;
    bool victoryButtonsShowing = false;
    float resultBackgroundFadeDuration = 0.25f;
    float resultBackgroundFadeElapsed = 0.0f;
    float resultBackgroundAlpha = 0.0f;
    bool resultUITriggered = false;
    bool resultBackgroundFadeStarted = false;
    std::array<std::shared_ptr<UIImageComponent>, 2> victoryButtons{};
    std::array<std::shared_ptr<UIImageComponent>, 2> victorySelectLines{};
    std::array<DirectX::XMFLOAT2, 2> victoryButtonPositions{ { { 355.0f, 840.0f }, { 820.0f, 840.0f } } };
    std::array<float, 2> victoryButtonScales{ { 0.45f, 0.45f } };
    DirectX::XMFLOAT2 victorySelectLineOffset{ 6.0f, 0.0f };
    DirectX::XMFLOAT2 victorySelectLineScale{ 0.29f, 0.3f };
    float victorySelectedButtonScale = 0.97f;
    float victoryUnselectedButtonScale = 0.83f;
    float victoryUnselectedButtonBrightness = 0.60f;
    float victoryButtonFadeDuration = 0.25f;
    float victoryButtonFadeTimer = 0.0f;
    float victoryButtonAlpha = 0.0f;
    float victoryButtonStickDelay = 0.0f;
    int victorySelectedButton = 0;
    bool victoryButtonsVisible = false;
    bool victoryButtonInputEnabled = false;
    bool victoryButtonInputArmed = false;
    bool victoryButtonFeedbackEnabled = false;
    std::array<std::shared_ptr<UIImageComponent>, 3> victoryLabels{};
    std::array<std::shared_ptr<UIImageComponent>, 4> victoryRankImages{};
    std::array<std::array<std::shared_ptr<UIImageComponent>, 8>, 2> victoryTimeDigits{};
    // Clear label, clear digits, best label, best digits, new record, rank.
    std::array<DirectX::XMFLOAT2, 6> victoryUIPositions
    { {
        { 420.0f, 620.0f }, { 737.0f, 609.0f }, { 416.0f, 701.0f },
        { 737.0f, 693.0f }, { 997.0f, 610.0f }, { 586.0f, 360.0f }
    }
    };
    std::array<float, 6> victoryUIScales{ { 0.37f, 0.33f, 0.39f, 0.33f, 0.46f, 0.45f } };
    // victoryUIScales[1] is the shared digit scale; slot 3 is unused.
    float resultTimeDigitSpacing = -3.4f;
    float resultTimeMinuteColonSpacing = -10.2f;
    float resultTimeColonSecondSpacing = -12.6f;
    float resultTimeSecondDotSpacing = -17.9f;
    float resultTimeDotMillisecondSpacing = -18.3f;
    float resultTimeColonScale = 0.73f;
    float resultTimeColonYOffset = 0.0f;
    float resultTimeDotScale = 0.76f;
    float resultTimeDotYOffset = 0.0f;
};
