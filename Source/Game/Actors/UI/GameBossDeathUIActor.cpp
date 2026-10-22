#include "pch.h"
#include "GameBossDeathUIActor.h"
#include "Engine/Easing/Easing.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Utility/Time.h"
#include "UI/UIManager.h"

void GameBossDeathUIActor::Initialize(const Transform&)
{
    bossDeathFinishGuideImage = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/Rush/rush_x_a_b.png", "BossDeathFinishGuide");
    bossDeathFinishGuideImage->SetSize(bossDeathFinishGuideSize);
    bossDeathFinishGuideImage->SetScale(bossDeathFinishGuideScale);
    bossDeathFinishGuideImage->SetPivot({ 0.5f, 0.5f });
    bossDeathFinishGuideImage->SetVisible(false);
    GetOwnerScene()->GetUIManager()->Add(bossDeathFinishGuideImage);

    bossDeathFinishButtonImage = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/Rush/rush_y.png", "BossDeathFinishY");
    bossDeathFinishButtonImage->SetSize(bossDeathFinishButtonSize);
    bossDeathFinishButtonImage->SetScale({
        bossDeathFinishButtonBaseScale.x * 0.8f,
        bossDeathFinishButtonBaseScale.y * 0.8f });
    bossDeathFinishButtonImage->SetPivot({ 0.5f, 0.5f });
    bossDeathFinishButtonImage->SetVisible(false);
    GetOwnerScene()->GetUIManager()->Add(bossDeathFinishButtonImage);

    bossDeathFinishWordImage = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/Rush/finish_word.png", "BossDeathFinishWord");
    bossDeathFinishWordImage->SetSize(bossDeathFinishWordSize);
    bossDeathFinishWordImage->SetScale(bossDeathFinishWordScale);
    bossDeathFinishWordImage->SetPivot({ 0.5f, 0.5f });
    bossDeathFinishWordImage->SetVisible(false);
    GetOwnerScene()->GetUIManager()->Add(bossDeathFinishWordImage);
}

void GameBossDeathUIActor::SetFinishGuideVisible(const bool visible)
{
    bossDeathFinishUIVisible = visible;
    bossDeathFinishUIAlpha = 0.0f;
    bossDeathFinishUIPulseTimer = 0.0f;
    bossDeathFinishUIPulsePhase = visible
        ? BossDeathFinishPromptAnimationPhase::AppearGrow
        : BossDeathFinishPromptAnimationPhase::Hidden;

    const auto offsetPosition = [this](const DirectX::XMFLOAT2& base)
        {
            return DirectX::XMFLOAT2{
                base.x + bossDeathFinishUIPositionOffset.x,
                base.y + bossDeathFinishUIPositionOffset.y };
        };
    const CoreColor transparent{ 1.0f, 1.0f, 1.0f, 0.0f };

    if (bossDeathFinishGuideImage)
    {
        bossDeathFinishGuideImage->SetWorldPosition(
            offsetPosition(bossDeathFinishGuidePosition));
        bossDeathFinishGuideImage->SetScale(bossDeathFinishGuideScale);
        bossDeathFinishGuideImage->SetColor(transparent);
    }
    if (bossDeathFinishButtonImage)
    {
        bossDeathFinishButtonImage->SetWorldPosition(
            offsetPosition(bossDeathFinishButtonPosition));
        bossDeathFinishButtonImage->SetScale({
            bossDeathFinishButtonBaseScale.x * 0.8f,
            bossDeathFinishButtonBaseScale.y * 0.8f });
        bossDeathFinishButtonImage->SetColor(transparent);
    }
    if (bossDeathFinishWordImage)
    {
        bossDeathFinishWordImage->SetWorldPosition(
            offsetPosition(bossDeathFinishWordPosition));
        bossDeathFinishWordImage->SetScale({
            bossDeathFinishWordScale.x * 0.9f,
            bossDeathFinishWordScale.y * 0.9f });
        bossDeathFinishWordImage->SetColor(transparent);
    }

    if (bossDeathFinishGuideImage)
        bossDeathFinishGuideImage->SetVisible(visible);
    if (bossDeathFinishButtonImage)
        bossDeathFinishButtonImage->SetVisible(visible);
    if (bossDeathFinishWordImage)
        bossDeathFinishWordImage->SetVisible(visible);
}

void GameBossDeathUIActor::UpdatePresentation()
{
    if (!bossDeathFinishUIVisible || !bossDeathFinishGuideImage ||
        !bossDeathFinishButtonImage || !bossDeathFinishWordImage)
    {
        return;
    }

    const float deltaTime = Time::UnscaledDeltaTime();
    bossDeathFinishUIAlpha = bossDeathFinishUIFadeInDuration <= FLT_EPSILON
        ? 1.0f
        : std::clamp(bossDeathFinishUIAlpha +
            deltaTime / bossDeathFinishUIFadeInDuration, 0.0f, 1.0f);
    bossDeathFinishUIPulseTimer += deltaTime;

    float buttonScale = 1.0f;
    float wordScale = 1.0f;
    switch (bossDeathFinishUIPulsePhase)
    {
    case BossDeathFinishPromptAnimationPhase::Hidden:
        break;
    case BossDeathFinishPromptAnimationPhase::AppearGrow:
    {
        constexpr float duration = 0.10f;
        const float time = std::clamp(bossDeathFinishUIPulseTimer, 0.0f, duration);
        buttonScale = std::clamp(
            Easing::OutBack(time, duration, 1.0f, 1.15f, 0.8f), 0.8f, 1.15f);
        wordScale = std::clamp(
            Easing::OutBack(time, duration, 1.0f, 1.08f, 0.9f), 0.9f, 1.08f);
        if (bossDeathFinishUIPulseTimer >= duration)
        {
            bossDeathFinishUIPulsePhase =
                BossDeathFinishPromptAnimationPhase::AppearSettle;
            bossDeathFinishUIPulseTimer = 0.0f;
        }
        break;
    }
    case BossDeathFinishPromptAnimationPhase::AppearSettle:
    {
        constexpr float duration = 0.08f;
        const float time = std::clamp(bossDeathFinishUIPulseTimer, 0.0f, duration);
        buttonScale = Easing::OutQuad(time, duration, 1.0f, 1.15f);
        wordScale = Easing::OutQuad(time, duration, 1.0f, 1.08f);
        if (bossDeathFinishUIPulseTimer >= duration)
        {
            bossDeathFinishUIPulsePhase =
                BossDeathFinishPromptAnimationPhase::PulseGrow;
            bossDeathFinishUIPulseTimer = 0.0f;
        }
        break;
    }
    case BossDeathFinishPromptAnimationPhase::PulseGrow:
    {
        constexpr float duration = 0.25f;
        const float time = std::clamp(bossDeathFinishUIPulseTimer, 0.0f, duration);
        buttonScale = Easing::InOutSine(time, duration, 1.12f, 1.0f);
        wordScale = Easing::InOutSine(time, duration, 1.05f, 1.0f);
        if (bossDeathFinishUIPulseTimer >= duration)
        {
            bossDeathFinishUIPulsePhase =
                BossDeathFinishPromptAnimationPhase::PulseReturn;
            bossDeathFinishUIPulseTimer = 0.0f;
        }
        break;
    }
    case BossDeathFinishPromptAnimationPhase::PulseReturn:
    {
        constexpr float duration = 0.35f;
        const float time = std::clamp(bossDeathFinishUIPulseTimer, 0.0f, duration);
        buttonScale = Easing::OutQuad(time, duration, 1.0f, 1.12f);
        wordScale = Easing::OutQuad(time, duration, 1.0f, 1.05f);
        if (bossDeathFinishUIPulseTimer >= duration)
        {
            bossDeathFinishUIPulsePhase =
                BossDeathFinishPromptAnimationPhase::PulseGrow;
            bossDeathFinishUIPulseTimer = 0.0f;
        }
        break;
    }
    }

    const auto offsetPosition = [this](const DirectX::XMFLOAT2& base)
        {
            return DirectX::XMFLOAT2{
                base.x + bossDeathFinishUIPositionOffset.x,
                base.y + bossDeathFinishUIPositionOffset.y };
        };
    const CoreColor color{ 1.0f, 1.0f, 1.0f, bossDeathFinishUIAlpha };

    bossDeathFinishGuideImage->SetWorldPosition(
        offsetPosition(bossDeathFinishGuidePosition));
    bossDeathFinishGuideImage->SetSize(bossDeathFinishGuideSize);
    bossDeathFinishGuideImage->SetScale(bossDeathFinishGuideScale);
    bossDeathFinishGuideImage->SetColor(color);

    bossDeathFinishButtonImage->SetWorldPosition(
        offsetPosition(bossDeathFinishButtonPosition));
    bossDeathFinishButtonImage->SetSize(bossDeathFinishButtonSize);
    bossDeathFinishButtonImage->SetScale({
        bossDeathFinishButtonBaseScale.x * buttonScale,
        bossDeathFinishButtonBaseScale.y * buttonScale });
    bossDeathFinishButtonImage->SetColor(color);

    bossDeathFinishWordImage->SetWorldPosition(
        offsetPosition(bossDeathFinishWordPosition));
    bossDeathFinishWordImage->SetSize(bossDeathFinishWordSize);
    bossDeathFinishWordImage->SetScale({
        bossDeathFinishWordScale.x * wordScale,
        bossDeathFinishWordScale.y * wordScale });
    bossDeathFinishWordImage->SetColor(color);
}

void GameBossDeathUIActor::Finalize()
{
    SetFinishGuideVisible(false);
    for (const auto& image : { bossDeathFinishGuideImage, bossDeathFinishButtonImage, bossDeathFinishWordImage })
        if (image) image->MarkPendingKill();
    bossDeathFinishGuideImage.reset();
    bossDeathFinishButtonImage.reset();
    bossDeathFinishWordImage.reset();
}

void GameBossDeathUIActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
        ImGui::SeparatorText("Finish UI");
        ImGui::DragFloat2("Finish UI Position Offset",
            &bossDeathFinishUIPositionOffset.x, 1.0f);
        ImGui::DragFloat2("Finish Word Position",
            &bossDeathFinishWordPosition.x, 1.0f);
        ImGui::DragFloat2("Finish Guide Position",
            &bossDeathFinishGuidePosition.x, 1.0f);
        ImGui::DragFloat2("Finish Y Button Position",
            &bossDeathFinishButtonPosition.x, 1.0f);
        ImGui::DragFloat2("Finish Word Scale",
            &bossDeathFinishWordScale.x, 0.01f);
        ImGui::DragFloat2("Finish Guide Scale",
            &bossDeathFinishGuideScale.x, 0.01f);
        ImGui::DragFloat2("Finish Y Button Scale",
            &bossDeathFinishButtonBaseScale.x, 0.01f);
#endif
}
