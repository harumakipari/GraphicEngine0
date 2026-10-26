#include "pch.h"
#include "GameDefeatUIActor.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Utility/Time.h"
#include "UI/UIManager.h"

namespace
{
    void ApplyResultButtonFeedback(UIImageComponent& button, const DirectX::XMFLOAT2& baseScale,
        bool selected, float selectedScale, float unselectedScale,
        const CoreColor& selectedColor, const CoreColor& unselectedColor, float alpha)
    {
        const float scale = selected ? selectedScale : unselectedScale;
        button.SetScale({ baseScale.x * scale, baseScale.y * scale });
        CoreColor color = selected ? selectedColor : unselectedColor;
        color.a *= alpha;
        button.SetColor(color);
    }

    bool ChangeResultSelection(int& selection, int next, float volume)
    {
        if (selection == next) return false;
        selection = next;
        CoreAudio::PlayOneShot("./Data/Sound/SE/button_select_move.wav", volume);
        return true;
    }

}

void GameDefeatUIActor::Initialize(const Transform& transform)
{
    auto uiManager = GetOwnerScene()->GetUIManager();
    deathResultDarkOverlay = std::make_shared<UIImageComponent>("DeathResultDarkOverlay");
    deathResultDarkOverlay->SetSize({ 1920.0f, 1080.0f });
    deathResultDarkOverlay->SetWorldPosition({ 0.0f, 0.0f });
    deathResultDarkOverlay->SetPivot({ 0.0f, 0.0f });
    deathResultDarkOverlay->SetColor(CoreColor{ 0.0f, 0.0f, 0.0f, 0.0f });
    deathResultDarkOverlay->zOrder = 20;
    uiManager->Add(deathResultDarkOverlay);

    deathResultDefeated = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/Defeat.png", "DeathResultDefeated");
    deathResultDefeated->SetSize({ 1920.0f, 610.0f });
    deathResultDefeated->SetWorldPosition(deathResultDefeatedPosition);
    deathResultDefeated->SetPivot({ 0.5f, 0.5f });
    deathResultDefeated->zOrder = 21;
    uiManager->Add(deathResultDefeated);

    deathResultBattleTime = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/battle_time.png", "DeathResultBattleTime");
    deathResultBattleTime->SetWorldPosition(deathResultBattleTimePosition);
    deathResultBattleTime->SetPivot({ 0.5f, 0.5f });
    deathResultBattleTime->SetSize({ 600.0f, 175.0f });
    deathResultBattleTime->SetScale({ 0.4f,0.4f });
    deathResultBattleTime->zOrder = 21;
    uiManager->Add(deathResultBattleTime);

    const int timeDigits[] = { 0, 1, -1, 4, 2, -2, 3, 6 };
    for (int i = 0; i < 8; ++i)
    {
        const bool colon = timeDigits[i] == -1;
        const bool dot = timeDigits[i] == -2;
        const std::string name = "DeathResultTime" + std::to_string(i);
        const std::string path = colon ? "./Data/Textures/UI/timer_colon.png" :
            (dot ? "./Data/Textures/UI/timer_dot.png" : "./Data/Textures/UI/number.png");
        deathResultTimeDigits[i] = std::make_shared<UIImageComponent>(path, name);
        deathResultTimeDigits[i]->SetSize(colon || dot ? DirectX::XMFLOAT2{ 48.0f, 128.0f } : DirectX::XMFLOAT2{ 96.0f, 128.0f });
        deathResultTimeDigits[i]->SetPivot({ 0.5f, 0.5f });
        deathResultTimeDigits[i]->SetScale(deathResultTimeNumberScale);
        deathResultTimeDigits[i]->SetVisible(false);
        deathResultTimeDigits[i]->zOrder = 21;
        uiManager->Add(deathResultTimeDigits[i]);
    }

    UpdateDeathResultBattleTimeValue();
    const char* paths[] = { "continue_button.png", "restart_battle_button.png", "return_title_button.png" };
    const char* names[] = { "DeathResultContinue", "DeathResultRestart", "DeathResultTitle" };
    const float buttonWidths[] = { 448.0f, 667.0f, 730.0f };
    const float buttonHeight[] = { 70.0f,75.0f, 71.0f };
    for (int i = 0; i < 3; ++i)
    {
        deathResultButtons[i] = std::make_shared<UIButtonComponent>(std::string("./Data/Textures/UI/Result/") + paths[i], names[i]);
        deathResultButtons[i]->SetSize({ buttonWidths[i], buttonHeight[i] });
        deathResultButtons[i]->SetScale(deathResultButtonScale);
        deathResultButtons[i]->SetUseHoverScale(false);
        deathResultButtons[i]->SetPivot({ 0.5f, 0.5f });
        deathResultButtons[i]->SetVisible(false);
        deathResultButtons[i]->SetEnable(false);
        const int index = i;
        const std::weak_ptr<GameDefeatUIActor> weakSelf = std::static_pointer_cast<GameDefeatUIActor>(shared_from_this());
        deathResultButtons[i]->onClick = [weakSelf, index]()
            {
                if (const auto self = weakSelf.lock(); self && self->deathResultInputEnabled && self->selectionCallback)
                    self->selectionCallback(index);
            };
        deathResultButtons[i]->zOrder = 21;
        uiManager->Add(deathResultButtons[i]);
        uiManager->AddButton(deathResultButtons[i]);
    }

    deathResultSelectLineLeft = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/select_button_line.png", "DeathResultSelectLineLeft");
    deathResultSelectLineRight = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/select_button_line.png", "DeathResultSelectLineRight");
    for (const auto& line : { deathResultSelectLineLeft, deathResultSelectLineRight })
    {
        line->SetVisible(false);
        uiManager->Add(line);
    }
    deathResultSelectLineLeft->SetPivot({ 1.0f,0.5f });
    deathResultSelectLineRight->SetPivot({ 0.0f,0.5f });
    deathResultSelectLineLeft->zOrder = 22;
    deathResultSelectLineRight->zOrder = 22;
    SetVisible(false);
}

void GameDefeatUIActor::Finalize()
{
    SetSelectionCallback(nullptr);
}


void GameDefeatUIActor::SetVisible(const bool visible)
{
    deathResultVisible = visible;
    if (!visible) deathResultSelectLineAnimProgress = 0.0f;
    if (deathResultDarkOverlay) deathResultDarkOverlay->SetVisible(visible);
    if (deathResultDefeated) deathResultDefeated->SetVisible(visible);
    if (deathResultBattleTime) deathResultBattleTime->SetVisible(visible);
    for (auto& digit : deathResultTimeDigits)
        if (digit) digit->SetVisible(visible);
    for (auto& button : deathResultButtons)
    {
        if (button) { button->SetVisible(visible); button->SetEnable(visible && deathResultInputEnabled); }
    }
    if (deathResultSelectLineLeft) deathResultSelectLineLeft->SetVisible(visible);
    if (deathResultSelectLineRight) deathResultSelectLineRight->SetVisible(visible);
    if (visible)
    {
        UpdateDeathResultBattleTimeValue();
        UpdateLayout();
    }
}

void GameDefeatUIActor::UpdateDeathResultBattleTimeValue()
{
    const int totalCentiseconds = std::clamp(static_cast<int>(std::floor(deathAttemptTime * 100.0f + 0.5f)), 0, 99 * 60 * 100 + 59 * 100 + 99);
    const int minutes = totalCentiseconds / 6000;
    const int seconds = (totalCentiseconds / 100) % 60;
    const int centiseconds = totalCentiseconds % 100;
    deathResultTimeValues = { minutes / 10, minutes % 10, -1, seconds / 10, seconds % 10, -2, centiseconds / 10, centiseconds % 10 };
}

void GameDefeatUIActor::UpdateDeathResultBattleTimeLayout()
{
    const int minutes = deathResultTimeValues[0] * 10 + deathResultTimeValues[1];
    std::array<float, 8> layoutPositions{};
    int visibleIndex = 0;
    int firstVisible = -1;
    int lastVisible = -1;

    for (int i = 0; i < 8; ++i)
    {
        if (i == 0 && minutes < 10) continue;
        float groupSpacing = 0.0f;
        if (i >= 3) groupSpacing += deathResultMinuteSecondSpacing;
        if (i >= 6) groupSpacing += deathResultSecondCentisecondSpacing;
        layoutPositions[i] = visibleIndex++ * deathResultTimeNumberSpacing + groupSpacing;
        if (firstVisible < 0) firstVisible = i;
        lastVisible = i;
    }
    const float layoutCenter = firstVisible >= 0
        ? (layoutPositions[firstVisible] + layoutPositions[lastVisible]) * 0.5f
        : 0.0f;

    for (int i = 0; i < 8; ++i)
    {
        auto& digit = deathResultTimeDigits[i];
        if (!digit) continue;
        const bool show = !(i == 0 && minutes < 10);
        digit->SetVisible(deathResultVisible && show);
        if (!show) continue;
        DirectX::XMFLOAT2 position{ deathResultTimePosition.x + layoutPositions[i] - layoutCenter, deathResultTimePosition.y };
        DirectX::XMFLOAT2 scale = deathResultTimeNumberScale;
        if (deathResultTimeValues[i] == -1)
        {
            position.x += deathResultColonOffset.x;
            position.y += deathResultColonOffset.y;
            scale.x *= deathResultColonScale.x;
            scale.y *= deathResultColonScale.y;
        }
        else if (deathResultTimeValues[i] == -2)
        {
            position.x += deathResultDotOffset.x;
            position.y += deathResultDotOffset.y;
            scale.x *= deathResultDotScale.x;
            scale.y *= deathResultDotScale.y;
        }
        digit->SetWorldPosition(position);
        digit->SetScale(scale);
        if (deathResultTimeValues[i] >= 0)
            digit->SetUV({ deathResultTimeValues[i] * numberTexWidth, 0.0f, numberTexWidth, numberTexHeight });
    }
}

void GameDefeatUIActor::UpdateLayout()
{
    if (!deathResultVisible) return;
    if (deathResultDefeated)
    {
        deathResultDefeated->SetWorldPosition(deathResultDefeatedPosition);
        deathResultDefeated->SetScale(deathResultDefeatedScale);
    }
    if (deathResultBattleTime)
    {
        deathResultBattleTime->SetWorldPosition(deathResultBattleTimePosition);
        deathResultBattleTime->SetScale(deathResultBattleTimeScale);
    }
    UpdateDeathResultBattleTimeLayout();
    UpdateDeathResultMenu();
}
void GameDefeatUIActor::UpdateDeathResultMenu()
{
    if (!deathResultVisible) return;
    float totalWidth = 0.0f;
    for (const auto& button : deathResultButtons)
        if (button) totalWidth += button->GetSize().x * deathResultButtonScale.x;
    totalWidth += deathResultButtonHorizontalSpacing * 2.0f;
    float cursorX = deathResultButtonStartPosition.x - totalWidth * 0.5f;
    for (const auto& button : deathResultButtons)
    {
        if (!button) continue;
        const float width = button->GetSize().x * deathResultButtonScale.x;
        button->SetScale(deathResultButtonScale);
        button->SetWorldPosition({ cursorX + width * 0.5f, deathResultButtonStartPosition.y });
        cursorX += width + deathResultButtonHorizontalSpacing;
    }

    auto* uiManager = GetOwnerScene()->GetUIManager();
    if (!deathResultInputEnabled)
    {
        if (deathResultSelection != 0)
        {
            deathResultSelection = 0;
            deathResultSelectLineAnimProgress = 0.0f;
        }
        if (deathResultButtons[0] &&
            uiManager->GetSelectedButton() != deathResultButtons[0].get())
        {
            uiManager->SetSelected(deathResultButtons[0].get());
        }
    }
    // UIManager owns D-Pad/Analog selection after Result input is enabled.
    else if (auto* selected = uiManager->GetSelectedButton())
    {
        for (int i = 0; i < static_cast<int>(deathResultButtons.size()); ++i)
        {
            if (deathResultButtons[i].get() == selected && i != deathResultSelection)
            {
                if (ChangeResultSelection(deathResultSelection, i, deathResultMoveSeVolume))
                    deathResultSelectLineAnimProgress = 0.0f;
            }
        }
    }
    const float buttonsAlpha = std::clamp((deathPresentationElapsed - deathButtonsStartTime) /
        (std::max)(0.001f, deathButtonsFadeDuration), 0.0f, 1.0f);
    for (int i = 0; i < static_cast<int>(deathResultButtons.size()); ++i)
    {
        auto& button = deathResultButtons[i];
        if (!button) continue;
        const bool selected = i == deathResultSelection;
        ApplyResultButtonFeedback(*button, deathResultButtonScale, selected,
            deathResultSelectedScale, 1.0f, deathResultSelectedColor, deathResultUnselectedColor, buttonsAlpha);
    }

    if (deathResultButtons[deathResultSelection])
    {
        const auto button = deathResultButtons[deathResultSelection];
        const auto buttonPos = button->GetWorldPosition();
        const auto buttonSize = button->GetSize();
        const float buttonWidth = buttonSize.x * deathResultButtonScale.x;
        const float lineWidth = buttonWidth * 0.5f;
        const float lineY = buttonPos.y;
        for (const auto& line : { deathResultSelectLineLeft, deathResultSelectLineRight })
        {
            if (!line) continue;
            line->SetSize({ lineWidth, 5.0f });
            line->SetScale({ deathResultSelectLineScale.x * deathResultSelectLineAnimProgress, deathResultSelectLineScale.y });
            line->SetWorldPosition({ buttonPos.x, lineY });
        }
        if (deathResultSelectLineLeft)
            deathResultSelectLineLeft->SetWorldPosition({ buttonPos.x - buttonWidth * 0.5f - deathResultSelectLineDistances[deathResultSelection] - lineWidth * 0.5f, lineY });
        if (deathResultSelectLineRight)
            deathResultSelectLineRight->SetWorldPosition({ buttonPos.x + buttonWidth * 0.5f + deathResultSelectLineDistances[deathResultSelection] + lineWidth * 0.5f, lineY });
        if (deathPresentationElapsed >= deathSelectLineStartTime)
            deathResultSelectLineAnimProgress = (std::min)(1.0f, deathResultSelectLineAnimProgress + Time::UnscaledDeltaTime() / (std::max)(0.001f, deathResultSelectLineAnimDuration));
    }
}


void GameDefeatUIActor::SetFrameData(float elapsed, float overlayStart, float defeatedStart, float battleTimeStart, float buttonsStart, float lineStart, float texWidth, float texHeight, float moveVolume)
{
    deathPresentationElapsed = elapsed;
    deathOverlayStartTime = overlayStart;
    deathDefeatedStartTime = defeatedStart;
    deathBattleTimeStartTime = battleTimeStart;
    deathButtonsStartTime = buttonsStart;
    deathSelectLineStartTime = lineStart;
    numberTexWidth = texWidth;
    numberTexHeight = texHeight;
    deathResultMoveSeVolume = moveVolume;
}

void GameDefeatUIActor::SetBattleTime(float battleTime)
{
    deathAttemptTime = battleTime;
    UpdateDeathResultBattleTimeValue();
}

void GameDefeatUIActor::EnableInput()
{
    deathResultSelection = 0;
    deathResultSelectLineAnimProgress = 0.0f;
    if (deathResultButtons[0]) GetOwnerScene()->GetUIManager()->SetSelected(deathResultButtons[0].get());
    deathResultInputEnabled = true;
    SetVisible(true);
}

void GameDefeatUIActor::ResetPresentation()
{
    deathResultInputEnabled = false;
    deathResultSelection = 0;
    deathResultSelectLineAnimProgress = 0.0f;
    SetVisible(false);
}

void GameDefeatUIActor::UpdatePresentation()
{
    const auto fade = [this](const float start, const float duration)
        {
            return std::clamp((deathPresentationElapsed - start) / (std::max)(0.001f, duration), 0.0f, 1.0f);
        };
    const float overlayAlpha = fade(deathOverlayStartTime, deathOverlayFadeDuration);
    const float defeatedAlpha = fade(deathDefeatedStartTime, deathDefeatedFadeDuration);
    const float battleTimeAlpha = fade(deathBattleTimeStartTime, deathBattleTimeFadeDuration);
    const float lineAlpha = fade(deathSelectLineStartTime, deathResultSelectLineAnimDuration);
    if (deathResultDarkOverlay) deathResultDarkOverlay->SetColor(CoreColor{ 0.0f, 0.0f, 0.0f, deathOverlayMaxAlpha * overlayAlpha });
    if (deathResultDefeated) deathResultDefeated->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, defeatedAlpha });
    if (deathResultBattleTime) deathResultBattleTime->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, battleTimeAlpha });
    for (auto& digit : deathResultTimeDigits) if (digit) digit->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, battleTimeAlpha });
    for (auto& line : { deathResultSelectLineLeft, deathResultSelectLineRight }) if (line) line->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, lineAlpha });

}

void GameDefeatUIActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::DragFloat(U8("背景暗転 フェード時間"), &deathOverlayFadeDuration, 0.01f, 0.01f, 5.0f);
    ImGui::SliderFloat(U8("背景暗転 最大濃度"), &deathOverlayMaxAlpha, 0.0f, 1.0f);
    ImGui::DragFloat(U8("DEFEATED フェード時間"), &deathDefeatedFadeDuration, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat(U8("Battle Time フェード時間"), &deathBattleTimeFadeDuration, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat(U8("ボタン フェード時間"), &deathButtonsFadeDuration, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat2("Result Defeated Position", &deathResultDefeatedPosition.x, 1.0f);
    ImGui::DragFloat2("Result Defeated Scale", &deathResultDefeatedScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat2("Result BattleTime Position", &deathResultBattleTimePosition.x, 1.0f);
    ImGui::DragFloat2("Result BattleTime Scale", &deathResultBattleTimeScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat2("Result Button Start Position", &deathResultButtonStartPosition.x, 1.0f);
    ImGui::DragFloat("Result Button Horizontal Spacing", &deathResultButtonHorizontalSpacing, 1.0f, 0.0f, 500.0f);
    ImGui::DragFloat2("Result Button Scale", &deathResultButtonScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat(U8("選択中ボタン 拡大率"), &deathResultSelectedScale, 0.01f, 1.0f, 2.0f);
    ImGui::ColorEdit4(U8("選択中ボタン 色"), &deathResultSelectedColor.r);
    ImGui::ColorEdit4(U8("非選択ボタン 色"), &deathResultUnselectedColor.r);
    ImGui::DragFloat("Select Line Distance From Continue Button", &deathResultSelectLineDistances[0], 1.0f, -200.0f, 200.0f);
    ImGui::DragFloat("Select Line Distance From Restart Button", &deathResultSelectLineDistances[1], 1.0f, -200.0f, 200.0f);
    ImGui::DragFloat("Select Line Distance From Title Button", &deathResultSelectLineDistances[2], 1.0f, -200.0f, 200.0f);
    ImGui::DragFloat2("Result Select Line Scale", &deathResultSelectLineScale.x, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Result Select Line Animation Duration", &deathResultSelectLineAnimDuration, 0.01f, 0.01f, 1.0f);
    ImGui::DragFloat2("Battle Time Number Position", &deathResultTimePosition.x, 1.0f);
    ImGui::DragFloat2("Battle Time Number Scale", &deathResultTimeNumberScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat2("Battle Time Colon Offset", &deathResultColonOffset.x, 1.0f);
    ImGui::DragFloat2("Battle Time Colon Scale", &deathResultColonScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat2("Battle Time Dot Offset", &deathResultDotOffset.x, 1.0f);
    ImGui::DragFloat2("Battle Time Dot Scale", &deathResultDotScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat("Battle Time Number Spacing", &deathResultTimeNumberSpacing, 1.0f, 1.0f, 200.0f);
    ImGui::DragFloat(U8("Battle Time 分・秒 間隔"), &deathResultMinuteSecondSpacing, 1.0f, -200.0f, 500.0f);
    ImGui::DragFloat(U8("Battle Time 秒・小数 間隔"), &deathResultSecondCentisecondSpacing, 1.0f, -200.0f, 500.0f);
#endif
}
