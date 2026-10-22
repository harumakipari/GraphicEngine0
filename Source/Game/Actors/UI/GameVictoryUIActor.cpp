#include "pch.h"
#include "GameVictoryUIActor.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Utility/Time.h"
#include "Engine/Audio/Audio.h"
#include "UI/UIManager.h"

namespace
{
    // Shared visual feedback; each result owns its selection range and tuning.
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

void GameVictoryUIActor::UpdateContents()
{
    const auto apply = [this](const std::shared_ptr<UIImageComponent>& image, size_t layout, bool show)
        {
            if (!image) return;
            image->SetWorldPosition(victoryUIPositions[layout]);
            image->SetScale({ victoryUIScales[layout], victoryUIScales[layout] });
            image->SetVisible(resultUITriggered && show);
            image->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, resultBackgroundAlpha });
        };
    apply(victoryLabels[0], 0, true);
    apply(victoryLabels[1], 2, true);
    apply(victoryLabels[2], 4, victoryIsNewRecord);
    for (size_t i = 0; i < victoryRankImages.size(); ++i)
        apply(victoryRankImages[i], 5, i == static_cast<size_t>(victoryRank));
    const double times[] = { victoryClearTime, victoryDisplayedBestTime };
    for (size_t row = 0; row < victoryTimeDigits.size(); ++row)
    {
        // Match Defeat's atlas and rounded centiseconds, but always keep both minute digits.
        const int total = static_cast<int>(std::clamp(std::floor(times[row] * 100.0 + 0.5), 0.0, 599999.0));
        const int minutes = total / 6000, seconds = total / 100 % 60, fraction = total % 100;
        const int values[] = { minutes / 10, minutes % 10, -1, seconds / 10, seconds % 10, -2, fraction / 10, fraction % 10 };
        const size_t layout = row == 0 ? 1 : 3;
        const float gaps[] = { resultTimeDigitSpacing, resultTimeMinuteColonSpacing,
            resultTimeColonSecondSpacing, resultTimeDigitSpacing, resultTimeSecondDotSpacing,
            resultTimeDotMillisecondSpacing, resultTimeDigitSpacing };
        std::array<float, 8> scales{};
        std::array<float, 8> widths{};
        float totalWidth = 0.0f;
        for (size_t i = 0; i < widths.size(); ++i)
        {
            scales[i] = i == 2 ? resultTimeColonScale : i == 5 ? resultTimeDotScale : victoryUIScales[1];
            // Match creation sizes. Spacing is in unscaled UI pixels.
            widths[i] = (values[i] < 0 ? 48.0f : 96.0f) * scales[i];
            totalWidth += widths[i];
            if (i < widths.size() - 1) totalWidth += gaps[i];
        }
        // Preserve each row's center anchor as the layout changes.
        float x = victoryUIPositions[layout].x - totalWidth * 0.5f;
        for (size_t i = 0; i < victoryTimeDigits[row].size(); ++i)
        {
            auto& digit = victoryTimeDigits[row][i];
            apply(digit, layout, true);
            if (digit)
            {
                const float yOffset = i == 2 ? resultTimeColonYOffset : i == 5 ? resultTimeDotYOffset : 0.0f;
                digit->SetScale({ scales[i], scales[i] });
                digit->SetWorldPosition({ x + widths[i] * 0.5f, victoryUIPositions[layout].y + yOffset });
                if (values[i] >= 0)
                    digit->SetUV({ values[i] * numberTexWidth, 0.0f, numberTexWidth, numberTexHeight });
            }
            // Next left edge = previous scaled right edge + spacing.
            x += widths[i];
            if (i < widths.size() - 1) x += gaps[i];
        }
    }
}

void GameVictoryUIActor::Initialize(const Transform& transform)
{
    victoryResultBackground = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/clear_result_back.png", "VictoryResultBackground");
    victoryResultBackground->SetSize({ 1155.0f, 1080.0f });
    victoryResultBackground->SetWorldPosition({ 0.0f, 0.0f });
    victoryResultBackground->SetPivot({ 0.0f, 0.0f });
    victoryResultBackground->zOrder = 20;
    GetOwnerScene()->GetUIManager()->Add(victoryResultBackground);
    const char* labelPaths[] = { "clear_time.png", "best_time.png", "new_record.png" };
    const DirectX::XMFLOAT2 labelSizes[] = { { 587.0f, 174.0f }, { 495.0f, 174.0f }, { 535.0f, 111.0f } };
    auto create = [this](const std::string& path, const std::string& name, DirectX::XMFLOAT2 size)
        {
            auto image = std::make_shared<UIImageComponent>(path, name);
            image->SetSize(size);
            image->SetPivot({ 0.5f, 0.5f });
            image->zOrder = 21;
            image->SetVisible(false);
            image->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, 0.0f });
            GetOwnerScene()->GetUIManager()->Add(image);
            return image;
        };
    for (size_t i = 0; i < victoryLabels.size(); ++i)
        victoryLabels[i] = create(std::string("./Data/Textures/UI/Result/") + labelPaths[i],
            "VictoryLabel" + std::to_string(i), labelSizes[i]);
    const char* rankPaths[] = { "rank_s.png", "rank_a.png", "rank_b.png", "rank_c.png" };
    for (size_t i = 0; i < victoryRankImages.size(); ++i)
        victoryRankImages[i] = create(std::string("./Data/Textures/UI/Result/") + rankPaths[i],
            "VictoryRank" + std::to_string(i), { 650.0f, 650.0f });
    for (size_t row = 0; row < victoryTimeDigits.size(); ++row)
        for (size_t i = 0; i < victoryTimeDigits[row].size(); ++i)
        {
            const bool punctuation = i == 2 || i == 5;
            const char* path = i == 2 ? "timer_colon.png" : i == 5 ? "timer_dot.png" : "number.png";
            victoryTimeDigits[row][i] = create(std::string("./Data/Textures/UI/") + path,
                "VictoryTime" + std::to_string(row) + "_" + std::to_string(i),
                { punctuation ? 48.0f : 96.0f, 128.0f });
        }
    victoryButtons[0] = create("./Data/Textures/UI/Result/restart_battle_button.png",
        "VictoryRestartButton", { 668.0f, 75.0f });
    victoryButtons[1] = create("./Data/Textures/UI/Result/return_title_button.png",
        "VictoryTitleButton", { 730.0f, 71.0f });
    for (size_t i = 0; i < victorySelectLines.size(); ++i)
        victorySelectLines[i] = create("./Data/Textures/UI/Result/select_button_line.png",
            i == 0 ? "VictorySelectLineLeft" : "VictorySelectLineRight", { 113.0f, 5.0f });
    ResetPresentation();
}

void GameVictoryUIActor::ResetPresentation()
{
    ResetButtons();
    victoryIsNewRecord = false;
    victoryRank = VictoryRank::C;
    victoryClearTime = 0.0;
    victoryDisplayedBestTime = 0.0;
    resultUITriggered = false;
    resultBackgroundFadeStarted = false;
    resultBackgroundFadeElapsed = 0.0f;
    resultBackgroundAlpha = 0.0f;
    if (victoryResultBackground)
    {
        victoryResultBackground->SetVisible(false);
        victoryResultBackground->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, 0.0f });
    }
    UpdateContents();
}

void GameVictoryUIActor::UpdateBackground()
{
    if (!resultBackgroundFadeStarted) return;
    const float duration = std::isfinite(resultBackgroundFadeDuration)
        ? std::clamp(resultBackgroundFadeDuration, 0.0f, 1.0f) : 0.25f;
    resultBackgroundFadeElapsed = (std::min)(duration,
        resultBackgroundFadeElapsed + (std::max)(0.0f, Time::UnscaledDeltaTime()));
    resultBackgroundAlpha = duration > 0.0f
        ? std::clamp(resultBackgroundFadeElapsed / duration, 0.0f, 1.0f) : 1.0f;
    if (victoryResultBackground)
        victoryResultBackground->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, resultBackgroundAlpha });
    UpdateContents();
}

void GameVictoryUIActor::ResetButtons()
{
    victoryButtonsVisible = false;
    victoryButtonAlpha = 0.0f;
    victoryButtonFadeTimer = 0.0f;
    victoryButtonStickDelay = 0.0f;
    victorySelectedButton = 0;
    victoryButtonInputEnabled = false;
    victoryButtonInputArmed = false;
    victoryButtonsShowing = false;
    GetOwnerScene()->GetUIManager()->SetNavigationEnabled(true);
    UpdateButtonLayout();
}

void GameVictoryUIActor::UpdateButtonLayout()
{
    for (size_t i = 0; i < victoryButtons.size(); ++i)
    {
        if (!victoryButtons[i]) continue;
        victoryButtons[i]->SetWorldPosition(victoryButtonPositions[i]);
        const CoreColor normal{ 1.0f, 1.0f, 1.0f, 1.0f };
        const CoreColor dimmed{ victoryUnselectedButtonBrightness, victoryUnselectedButtonBrightness,
            victoryUnselectedButtonBrightness, 1.0f };
        if (victoryButtonFeedbackEnabled)
            ApplyResultButtonFeedback(*victoryButtons[i], { victoryButtonScales[i], victoryButtonScales[i] },
                i == static_cast<size_t>(victorySelectedButton), victorySelectedButtonScale,
                victoryUnselectedButtonScale, normal, dimmed, victoryButtonAlpha);
        else
        {
            victoryButtons[i]->SetScale({ victoryButtonScales[i], victoryButtonScales[i] });
            victoryButtons[i]->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, victoryButtonAlpha });
        }
        victoryButtons[i]->SetVisible(victoryButtonsVisible);
    }
    const auto& selectedButton = victoryButtons[victorySelectedButton];
    const auto& buttonPosition = victoryButtonPositions[victorySelectedButton];
    const float buttonWidth = selectedButton ? selectedButton->GetSize().x *
        victoryButtonScales[victorySelectedButton] * victorySelectedButtonScale : 0.0f;
    const float lineWidth = victorySelectLines[0] ? victorySelectLines[0]->GetSize().x * victorySelectLineScale.x : 0.0f;
    const float lineY = buttonPosition.y + victorySelectLineOffset.y;
    const float lineDistance = buttonWidth * 0.5f + victorySelectLineOffset.x;
    for (size_t i = 0; i < victorySelectLines.size(); ++i)
    {
        const auto& line = victorySelectLines[i];
        if (!line) continue;
        line->SetScale(victorySelectLineScale);
        line->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, victoryButtonAlpha });
        line->SetVisible(victoryButtonsVisible && (victoryButtonsShowing || victoryButtonInputEnabled));
        const float side = i == 0 ? -1.0f : 1.0f;
        line->SetWorldPosition({ buttonPosition.x + side * (lineDistance + lineWidth * 0.5f), lineY });
    }
}

int GameVictoryUIActor::UpdateInput(float moveVolume)
{
    if (!victoryButtonInputEnabled) return -1;
    // Use the same actions and stick threshold as UIManager, scoped to these two images.
    // Require neutral input after entering Interactive, then accept fresh triggers only.
    const float stickX = InputSystem::GetLeftStick().x;
    if (!victoryButtonInputArmed)
    {
        if (!InputSystem::GetInputState("UISubmit", InputStateMask::Press) &&
            !InputSystem::GetInputState("UILeft", InputStateMask::Press) &&
            !InputSystem::GetInputState("UIRight", InputStateMask::Press) &&
            !InputSystem::GetInputState("Left", InputStateMask::Press) &&
            !InputSystem::GetInputState("Right", InputStateMask::Press) && std::abs(stickX) <= 0.6f)
            victoryButtonInputArmed = true;
        return -1;
    }
    victoryButtonStickDelay = (std::max)(0.0f, victoryButtonStickDelay - Time::UnscaledDeltaTime());
    int direction = 0;
    if (InputSystem::GetInputState("UILeft", InputStateMask::Trigger) ||
        InputSystem::GetInputState("Left", InputStateMask::Trigger)) direction = -1;
    else if (InputSystem::GetInputState("UIRight", InputStateMask::Trigger) ||
        InputSystem::GetInputState("Right", InputStateMask::Trigger)) direction = 1;
    else if (victoryButtonStickDelay <= 0.0f)
        direction = stickX > 0.6f ? 1 : stickX < -0.6f ? -1 : 0;
    if (direction != 0)
    {
        const int next = std::clamp(victorySelectedButton + direction, 0, 1);
        ChangeResultSelection(victorySelectedButton, next, moveVolume);
        victoryButtonStickDelay = 0.2f;
    }
    if (InputSystem::GetInputState("UISubmit", InputStateMask::Trigger)) return victorySelectedButton;
    return -1;
}


void GameVictoryUIActor::SetResultData(double clearTime, double bestTime, bool newRecord, VictoryRank rank)
{
    victoryClearTime = clearTime;
    victoryDisplayedBestTime = bestTime;
    victoryIsNewRecord = newRecord;
    victoryRank = rank;
}

void GameVictoryUIActor::SetNumberTextureSize(float width, float height)
{
    numberTexWidth = width;
    numberTexHeight = height;
}

void GameVictoryUIActor::StartBackgroundFade()
{
    resultUITriggered = true;
    resultBackgroundFadeStarted = true;
    resultBackgroundFadeElapsed = 0.0f;
    if (victoryResultBackground) victoryResultBackground->SetVisible(true);
}

void GameVictoryUIActor::ShowButtons()
{
    victoryButtonsVisible = true;
    victoryButtonsShowing = true;
    victoryButtonFadeTimer = 0.0f;
}

bool GameVictoryUIActor::AdvanceButtonFade()
{
    const float duration = std::isfinite(victoryButtonFadeDuration)
        ? std::clamp(victoryButtonFadeDuration, 0.0f, 1.0f) : 0.25f;
    victoryButtonFadeTimer = (std::min)(duration,
        victoryButtonFadeTimer + (std::max)(0.0f, Time::UnscaledDeltaTime()));
    victoryButtonAlpha = duration > 0.0f ? victoryButtonFadeTimer / duration : 1.0f;
    return victoryButtonFadeTimer >= duration;
}

void GameVictoryUIActor::EnableInput()
{
    victoryButtonsShowing = false;
    victoryButtonFeedbackEnabled = true;
    victoryButtonInputEnabled = true;
    victoryButtonInputArmed = false;
}

void GameVictoryUIActor::DisableInput()
{
    victoryButtonInputEnabled = false;
    victoryButtonInputArmed = false;
}

void GameVictoryUIActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    const char* layoutNames[] = { "Clear Time Label", "Clear Time Digits", "Best Time Label", "Best Time Digits", "New Record", "Rank" };
    for (size_t i = 0; i < victoryUIPositions.size(); ++i)
    {
        ImGui::PushID(layoutNames[i]);
        ImGui::TextUnformatted(layoutNames[i]);
        ImGui::DragFloat2("Position", &victoryUIPositions[i].x, 1.0f);
        if (i != 1 && i != 3)
            ImGui::DragFloat("Scale", &victoryUIScales[i], 0.01f, 0.01f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::PopID();
    }
    ImGui::SeparatorText("Victory Result Buttons");
    ImGui::DragFloat("Victory Button Fade Duration", &victoryButtonFadeDuration,
        0.01f, 0.0f, 1.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Restart Button Position", &victoryButtonPositions[0].x, 1.0f);
    ImGui::DragFloat("Restart Button Scale", &victoryButtonScales[0], 0.01f,
        0.01f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Return Title Button Position", &victoryButtonPositions[1].x, 1.0f);
    ImGui::DragFloat("Return Title Button Scale", &victoryButtonScales[1], 0.01f,
        0.01f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Victory Selected Button Scale", &victorySelectedButtonScale, 0.01f,
        0.5f, 1.5f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Victory Unselected Button Scale", &victoryUnselectedButtonScale, 0.01f,
        0.5f, 1.5f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Victory Unselected Button Brightness", &victoryUnselectedButtonBrightness, 0.01f,
        0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextDisabled("Scale multipliers use each button's base scale. Line offset is from its left edge.");
    ImGui::DragFloat("Victory Select Line Horizontal Offset", &victorySelectLineOffset.x, 1.0f, 0.0f, 300.0f);
    ImGui::DragFloat("Victory Select Line Y Offset", &victorySelectLineOffset.y, 1.0f, -200.0f, 200.0f);
    ImGui::DragFloat2("Victory Select Line Scale X/Y", &victorySelectLineScale.x, 0.01f,
        0.01f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    UpdateButtonLayout();
    ImGui::SeparatorText("Result Time Layout");
    ImGui::TextDisabled("Shared by Clear / Best Time. Spacing and Y offsets are UI pixels.");
    ImGui::DragFloat("Result Time Digit Scale", &victoryUIScales[1], 0.01f, 0.01f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Result Time Digit Spacing", &resultTimeDigitSpacing, 0.1f, -30.0f, 30.0f, "%.1f px", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Result Time Minute Colon Spacing", &resultTimeMinuteColonSpacing, 0.1f, -30.0f, 30.0f, "%.1f px", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Result Time Colon Second Spacing", &resultTimeColonSecondSpacing, 0.1f, -30.0f, 30.0f, "%.1f px", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Result Time Second Dot Spacing", &resultTimeSecondDotSpacing, 0.1f, -30.0f, 30.0f, "%.1f px", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Result Time Dot Millisecond Spacing", &resultTimeDotMillisecondSpacing, 0.1f, -30.0f, 30.0f, "%.1f px", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Result Time Colon Scale", &resultTimeColonScale, 0.01f, 0.01f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Result Time Colon Y Offset", &resultTimeColonYOffset, 0.1f, -100.0f, 100.0f, "%.1f px", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Result Time Dot Scale", &resultTimeDotScale, 0.01f, 0.01f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Result Time Dot Y Offset", &resultTimeDotYOffset, 0.1f, -100.0f, 100.0f, "%.1f px", ImGuiSliderFlags_AlwaysClamp);
    UpdateContents();
    ImGui::DragFloat("Result Background Fade Duration", &resultBackgroundFadeDuration,
        0.01f, 0.0f, 1.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
#endif
}
