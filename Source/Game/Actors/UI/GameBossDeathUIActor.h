#pragma once
#include "Core/Actor.h"

class UIImageComponent;

class GameBossDeathUIActor : public Actor
{
public:
    explicit GameBossDeathUIActor(const std::string& name) : Actor(name) {}
    void Initialize(const Transform& transform) override;
    void Finalize() override;
    void SetFinishGuideVisible(bool visible);
    void UpdatePresentation();
    void DrawImGuiDetails()override;

private:
    enum class BossDeathFinishPromptAnimationPhase : uint8_t
    {
        Hidden,
        AppearGrow,
        AppearSettle,
        PulseGrow,
        PulseReturn,
    };

    DirectX::XMFLOAT2 bossDeathFinishUIPositionOffset{ 7.0f,-134.0f };
    bool bossDeathFinishUIVisible = false;
    float bossDeathFinishUIAlpha = 0.0f;
    float bossDeathFinishUIFadeInDuration = 0.10f;
    float bossDeathFinishUIPulseTimer = 0.0f;
    BossDeathFinishPromptAnimationPhase bossDeathFinishUIPulsePhase = BossDeathFinishPromptAnimationPhase::Hidden;
    DirectX::XMFLOAT2 bossDeathFinishGuidePosition{ 1200.0f, 674.0f };
    DirectX::XMFLOAT2 bossDeathFinishButtonPosition{ 1200.0f, 600.0f };
    DirectX::XMFLOAT2 bossDeathFinishWordPosition{ 1202.0f, 542.0f };
    DirectX::XMFLOAT2 bossDeathFinishGuideSize{ 488.0f, 379.0f };
    DirectX::XMFLOAT2 bossDeathFinishButtonSize{ 300.0f, 300.0f };
    DirectX::XMFLOAT2 bossDeathFinishWordSize{ 124.5f, 68.0f };
    DirectX::XMFLOAT2 bossDeathFinishGuideScale{ 0.32f, 0.32f };
    DirectX::XMFLOAT2 bossDeathFinishButtonBaseScale{ 0.32f, 0.32f };
    DirectX::XMFLOAT2 bossDeathFinishWordScale{ 1.05f, 0.78f };
    std::shared_ptr<UIImageComponent> bossDeathFinishGuideImage;
    std::shared_ptr<UIImageComponent> bossDeathFinishButtonImage;
    std::shared_ptr<UIImageComponent> bossDeathFinishWordImage;
};
