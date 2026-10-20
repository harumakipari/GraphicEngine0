#pragma once
#include "Core/Actor.h"
#include "Core/CoreColor.h"

class UIImageComponent;
class UIButtonComponent;

class GameDefeatUIActor : public Actor
{
public:
    explicit GameDefeatUIActor(const std::string& actorName) : Actor(actorName) {}
    void Initialize(const Transform& transform) override;

    void DrawImGuiDetails()override;


    void SetSelectionCallback(std::function<void(int)> callback) { selectionCallback = std::move(callback); }
    void SetFrameData(float elapsed, float overlayStart, float defeatedStart, float battleTimeStart, float buttonsStart, float lineStart, float texWidth, float texHeight, float moveVolume);
    void SetBattleTime(float battleTime);
    void SetVisible(bool visible);
    bool IsVisible() const { return deathResultVisible; }
    bool IsInputEnabled() const { return deathResultInputEnabled; }
    void EnableInput();
    void ResetPresentation();
    void UpdatePresentation();
    void UpdateLayout();

private:
    void UpdateDeathResultBattleTimeValue();
    void UpdateDeathResultBattleTimeLayout();
    void UpdateDeathResultMenu();

private:
    std::shared_ptr<UIImageComponent> deathResultDarkOverlay;
    std::shared_ptr<UIImageComponent> deathResultDefeated;
    std::shared_ptr<UIImageComponent> deathResultBattleTime;


    std::function<void(int)> selectionCallback;

    float deathAttemptTime = 0.0f;
    float deathPresentationElapsed = 0.0f;
    float deathOverlayStartTime = 4.6f;
    float deathDefeatedStartTime = 5.0f;
    float deathBattleTimeStartTime = 5.1f;
    float deathButtonsStartTime = 5.2f;
    float deathSelectLineStartTime = 4.4f;
    float numberTexWidth = 198.0f;
    float numberTexHeight = 300.0f;
    float deathResultMoveSeVolume = 0.15f;
    float deathOverlayFadeDuration = 0.4f;
    float deathOverlayMaxAlpha = 0.25f;
    float deathDefeatedFadeDuration = 0.40f;
    float deathBattleTimeFadeDuration = 0.30f;
    float deathButtonsFadeDuration = 0.30f;
    bool deathResultVisible = false;
    bool deathResultInputEnabled = false;
    int deathResultSelection = 0;
    std::array<std::shared_ptr<UIButtonComponent>, 3> deathResultButtons{};
    DirectX::XMFLOAT2 deathResultDefeatedPosition{ 960.0f, 530.0f };
    DirectX::XMFLOAT2 deathResultDefeatedScale{ 1.0f, 1.0f };
    DirectX::XMFLOAT2 deathResultBattleTimePosition{ 770.0f, 594.0f };
    DirectX::XMFLOAT2 deathResultBattleTimeScale{ 0.5f, 0.5f };
    DirectX::XMFLOAT2 deathResultButtonStartPosition{ 1011.0f, 734.0f };
    float deathResultButtonHorizontalSpacing = 100.0f;
    DirectX::XMFLOAT2 deathResultButtonScale{ 0.4f, 0.4f };
    std::array<std::shared_ptr<UIImageComponent>, 8> deathResultTimeDigits{};
    std::array<int, 8> deathResultTimeValues{};
    DirectX::XMFLOAT2 deathResultTimePosition{ 1160.0f, 588.0f };
    DirectX::XMFLOAT2 deathResultTimeNumberScale{ 0.35f, 0.35f };
    DirectX::XMFLOAT2 deathResultColonOffset{ -8.0f, 0.0f };
    DirectX::XMFLOAT2 deathResultColonScale{ 3.5f, 2.2f };
    DirectX::XMFLOAT2 deathResultDotOffset{ -15.0f, 0.0f };
    DirectX::XMFLOAT2 deathResultDotScale{ 3.5f, 2.2f };
    float deathResultTimeNumberSpacing = 30.0f;
    float deathResultMinuteSecondSpacing = -18.0f;
    float deathResultSecondCentisecondSpacing = -26.0f;
    std::shared_ptr<UIImageComponent> deathResultSelectLineLeft;
    std::shared_ptr<UIImageComponent> deathResultSelectLineRight;
    std::array<float, 3> deathResultSelectLineDistances = { -21.0f,-39.0f,-39.0f };
    DirectX::XMFLOAT2 deathResultSelectLineScale{ 0.12f, 0.4f };
    float deathResultSelectLineAnimDuration = 0.15f;
    float deathResultSelectLineAnimProgress = 0.0f;
    float deathResultSelectedScale = 1.2f;
    CoreColor deathResultSelectedColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    CoreColor deathResultUnselectedColor{ 0.5f, 0.5f, 0.5f, 1.0f };
};
