#include "pch.h"
#include "Pause.h"

#include "SceneTransitionManager.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Utility/Time.h"
#include "Engine/Framework/Framework.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Scenes/GameScene.h"

#include <imgui.h>

namespace
{
    constexpr float PauseButtonBaseScale = 0.5f;
    constexpr const wchar_t* PauseMenuSound = L"./Data/Sound/SE/pause_menu.wav";
    constexpr const char* PauseButtonPushSound = "./Data/Sound/SE/button_push.wav";
    constexpr const char* PauseButtonSelectSound = "./Data/Sound/SE/button_select_move.wav";

    float MoveTowards(const float current, const float target, const float maxDelta)
    {
        if (current < target)
            return (std::min)(current + maxDelta, target);
        return (std::max)(current - maxDelta, target);
    }
}

void Pause::Initialize(const Transform& transform)
{
    const auto scene = GetOwnerScene();
    auto uiManager = scene->GetUIManager();

    pauseBackImage = std::make_shared<UIImageComponent>("./Data/Textures/UI/Pause/back.png", "back");
    pauseBackImage->SetWorldPosition({ 960, 540 }); pauseBackImage->SetPivot({ .5f,.5f });
    pauseBackImage->SetSize({ 1920, 1080 }); pauseBackImage->SetColor(DirectX::XMFLOAT4{ 0,0,0,.5f });
    pauseBackImage->SetVisible(false); pauseBackImage->zOrder = 95; uiManager->Add(pauseBackImage);

    pausePanel = std::make_shared<UIImageComponent>("./Data/Textures/UI/Pause/pause_panel.png", "pause_panel");
    pausePanel->SetWorldPosition({ 960, 540 });
    pausePanel->SetPivot({ .5f,.5f }); pausePanel->SetScale({ .5f,.5f });
    pausePanel->SetSize({ 1233, 1424 }); pausePanel->SetVisible(false); pausePanel->zOrder = 100; uiManager->Add(pausePanel);

    menuButton = std::make_shared<UIButtonComponent>("./Data/Textures/UI/Pause/menu.png", "menu");
    menuButton->SetWorldPosition({ 1828, 1007 }); menuButton->SetPivot({ .5f,.5f }); menuButton->SetSize({ 140,140 });
    menuButton->SetScale({ .8f,.8f }); menuButton->zOrder = 100; uiManager->Add(menuButton);
    menuButton->onClick = [this]() { OpenPause(); };

    const auto configureButton = [](const std::shared_ptr<UIButtonComponent>& button)
        {
            button->SetVisualColors(CoreColor(.65f, .65f, .65f, 1), CoreColor(.8f, .8f, .8f, 1),
                CoreColor(.8f, .8f, .8f, 1), CoreColor::White);
            button->SetVisible(false); button->SetEnable(false); button->zOrder = 105;
        };

    returnTitleButton = std::make_shared<UIButtonComponent>("./Data/Textures/UI/Pause/back_to_title.png", "back_to_title");
    returnTitleButton->SetWorldPosition({ 980,607 });
    returnTitleButton->SetPivot({ .5f,.5f });
    returnTitleButton->SetSize({ 733,78 });
    returnTitleButton->SetScale({ 0.5f,0.5f });
    configureButton(returnTitleButton);
    SetPauseButtonAction(returnTitleButton, [this]()
        {
            HideSelectionLines();
            Time::timeScale = 1.0f;
            const char* types[] = { "0", "1" };
            SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", "TitleScene"), std::make_pair("type", types[rand() % 2]), std::make_pair("fromScene","GameScene") });
        });
    uiManager->Add(returnTitleButton);

    battleActionButton = std::make_shared<UIButtonComponent>("./Data/Textures/UI/Pause/go_to_boss_room.png", "battle_action");
    battleActionButton->SetWorldPosition({ 978,751 });
    battleActionButton->SetPivot({ .5f,.5f });
    battleActionButton->SetSize({ 785,80 });
    battleActionButton->SetScale({ 0.5f,0.5f });

    configureButton(battleActionButton);
    SetPauseButtonAction(battleActionButton, [this]()
        {
            const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
            const bool isBossBattle = player && player->IsBossBattle();
            ClosePause();
            if (auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene()))
            {
                if (isBossBattle) gameScene->RequestRestartBossBattleFromPause();
                else gameScene->RequestBossEntryFromPause();
            }
        });
    uiManager->Add(battleActionButton);

    backToRoomButton = std::make_shared<UIButtonComponent>("./Data/Textures/UI/Pause/back_to_room.png", "back_to_room");
    backToRoomButton->SetWorldPosition({ 980, 751 });
    backToRoomButton->SetPivot({ .5f,.5f });
    backToRoomButton->SetSize({ 662,76 });
    backToRoomButton->SetScale({ 0.5f,0.5f });

    configureButton(backToRoomButton);
    SetPauseButtonAction(backToRoomButton, [this]()
        {
            ClosePause();
            if (auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene()))
                gameScene->RequestReturnToMainRoomStart();
        });
    uiManager->Add(backToRoomButton);

    quitGameButton = std::make_shared<UIButtonComponent>("./Data/Textures/UI/Pause/quit_game.png", "quit_game");
    quitGameButton->SetWorldPosition({ 980, 850 });
    quitGameButton->SetPivot({ .5f,.5f });
    quitGameButton->SetSize({ 472,92 });
    quitGameButton->SetScale({ 0.5f,0.5f });

    configureButton(quitGameButton);
    SetPauseButtonAction(quitGameButton, []()
        {
            Framework::RequestExit();
        });
    uiManager->Add(quitGameButton);
    uiManager->AddButton(returnTitleButton); uiManager->AddButton(backToRoomButton);
    uiManager->AddButton(battleActionButton); uiManager->AddButton(quitGameButton);

    selectionLineLeft = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/select_button_line.png", "PauseSelectLineLeft");
    selectionLineRight = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/select_button_line.png", "PauseSelectLineRight");
    for (const auto& line : { selectionLineLeft, selectionLineRight })
    {
        line->SetPivot({ .5f,.5f }); line->SetVisible(false); line->SetEnable(false); line->zOrder = 106; uiManager->Add(line);
    }
    stopUpdate = false;
}

bool Pause::CanOpenPause() const
{
    if (stopUpdate || state != PauseState::Playing || !InputSystem::IsInputEnabled())
        return false;

    const auto scene = GetOwnerScene();
    if (!scene || !scene->GetCameraManager())
        return false;
    const auto cameraManager = scene->GetCameraManager();
    if (cameraManager->IsUseMovie() || cameraManager->IsUseCinematic() || cameraManager->IsUseDebug())
        return false;

    const auto player = scene->GetActorManager()->GetActorOfType<Player>();
    return player && !player->IsPendingKill() && player->GetHp() > 0 && !player->IsInWinState();
}

void Pause::Update(float deltaTime)

{
    if (stopUpdate) return;

    const auto resumeFromPause = [this]()
        {
            CoreAudio::PlayOneShot(PauseMenuSound);
            ClosePause();
        };

    const bool canOpenPause = CanOpenPause();
    if (state == PauseState::Playing)
    {
        menuButton->SetVisible(canOpenPause);
        menuButton->SetEnable(canOpenPause);
    }

    if (InputSystem::GetInputState("Pause", InputStateMask::Trigger))
    {
        if (state == PauseState::Playing && canOpenPause) OpenPause();
        else if (state == PauseState::Paused) resumeFromPause();
    }
    if (state == PauseState::Paused && InputSystem::GetInputState("GamePadB", InputStateMask::Trigger)) resumeFromPause();
    if (state == PauseState::Paused)
    {
        ApplyButtonLayout();
        UpdatePauseButtonScales(Time::UnscaledDeltaTime());
        UpdateSelectionLines();
    }
}

void Pause::HidePauseMenu()
{
    stopUpdate = true; menuButton->SetEnable(false); menuButton->SetVisible(false); HideSelectionLines();
}

void Pause::OpenPause()
{
    if (!CanOpenPause()) return;
    CoreAudio::PlayOneShot(PauseMenuSound);
    pauseBackImage->SetVisible(true); pausePanel->SetVisible(true); pausePanel->SetEnable(true);
    returnTitleButton->SetVisible(true); returnTitleButton->SetEnable(true);
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    const bool isBossBattle = player && player->IsBossBattle();
    battleActionButton->SetTexture(std::make_shared<Sprite>(Graphics::GetDevice(), isBossBattle ? L"./Data/Textures/UI/Pause/restart_battle.png" : L"./Data/Textures/UI/Pause/go_to_boss_room.png"));
    battleActionButton->SetVisible(true); battleActionButton->SetEnable(true);
    backToRoomButton->SetVisible(isBossBattle); backToRoomButton->SetEnable(isBossBattle);
    quitGameButton->SetVisible(true); quitGameButton->SetEnable(true);
    menuButton->SetEnable(false); menuButton->SetVisible(false);
    state = PauseState::Paused; Time::timeScale = 0; CoreAudio::SetSystemPaused(true); GetOwnerScene()->SetPaused(true);
    ApplyButtonLayout();
    ResetPauseButtonScales();
    lastSelectedButton = nullptr; selectionLineAnimationProgress = 0;
    GetOwnerScene()->GetUIManager()->SetSelected(returnTitleButton.get()); UpdateSelectionLines();
}

void Pause::ClosePause()
{
    if (state != PauseState::Paused) return;
    pauseBackImage->SetVisible(false); pausePanel->SetVisible(false); pausePanel->SetEnable(false);
    returnTitleButton->SetEnable(false); returnTitleButton->SetVisible(false);
    battleActionButton->SetEnable(false); battleActionButton->SetVisible(false);
    backToRoomButton->SetEnable(false); backToRoomButton->SetVisible(false);
    quitGameButton->SetEnable(false); quitGameButton->SetVisible(false); HideSelectionLines();
    GetOwnerScene()->GetUIManager()->SetSelected(nullptr);
    ResetPauseButtonScales();
    CoreAudio::SetSystemPaused(false);
    Time::timeScale = 1;
    GetOwnerScene()->SetPaused(false); state = PauseState::Playing;
    //CoreAudio::PlayOneShot(L"./Data/Sound/SE/pause_menu.wav");
    menuButton->SetEnable(false); menuButton->SetVisible(false);
}

void Pause::HideSelectionLines()
{
    for (const auto& line : { selectionLineLeft, selectionLineRight }) { if (line) { line->SetVisible(false); line->SetEnable(false); } }
    lastSelectedButton = nullptr; selectionLineAnimationProgress = 0;
}

void Pause::UpdateSelectionLines()
{
    const auto* selected = GetOwnerScene()->GetUIManager()->GetSelectedButton();
    if ((selected != returnTitleButton.get() && selected != backToRoomButton.get() &&
        selected != battleActionButton.get() && selected != quitGameButton.get()) ||
        !selected->IsVisible() || !selected->IsEnabled()) {
        HideSelectionLines(); return;
    }
    if (selected != lastSelectedButton)
    {
        if (lastSelectedButton)
            CoreAudio::PlayOneShot(PauseButtonSelectSound);
        lastSelectedButton = const_cast<UIButtonComponent*>(selected);
        selectionLineAnimationProgress = 0;
    }
    const float lineRevealDuration = (std::max)(selectionLineAnimationDuration, FLT_EPSILON);
    selectionLineAnimationProgress = (std::min)(1.0f, selectionLineAnimationProgress + Time::UnscaledDeltaTime() / lineRevealDuration);
    const float buttonDisplayScale = GetPauseButtonDisplayScale(selected);
    const float lineWidth = selected->GetSize().x * selectionLineWidthRatio;
    const float buttonHalfWidth = selected->GetSize().x * buttonDisplayScale * .5f;
    const float lineHalfWidth = lineWidth * selectionLineScale.x * .5f;
    const float offset = buttonHalfWidth + lineHalfWidth + selectionLineGap;
    const auto position = selected->GetWorldPosition();
    selectionLineLeft->SetWorldPosition({ position.x - offset, position.y + selectionLineYOffset }); selectionLineRight->SetWorldPosition({ position.x + offset, position.y + selectionLineYOffset });
    for (const auto& line : { selectionLineLeft, selectionLineRight }) { line->SetSize({ lineWidth, selectionLineHeight }); line->SetScale({ selectionLineScale.x * selectionLineAnimationProgress, selectionLineScale.y }); line->SetVisible(true); line->SetEnable(true); }
}

void Pause::UpdatePauseButtonScales(float deltaTime)
{
    const auto uiManager = GetOwnerScene()->GetUIManager();
    const auto* selected = uiManager ? uiManager->GetSelectedButton() : nullptr;
    const float selectedScale = PauseButtonBaseScale * selectedButtonScale;
    const float duration = (std::max)(buttonScaleAnimationDuration, FLT_EPSILON);
    const float scaleSpeed = std::abs(selectedScale - PauseButtonBaseScale) / duration;
    const float maxDelta = scaleSpeed * (std::max)(0.0f, deltaTime);

    const auto updateScale = [&](const std::shared_ptr<UIButtonComponent>& button, float& displayScale)
        {
            if (!button || !button->IsVisible())
            {
                displayScale = PauseButtonBaseScale;
                if (button)
                    button->SetScale({ PauseButtonBaseScale, PauseButtonBaseScale });
                return;
            }

            const float targetScale = button.get() == selected
                ? selectedScale
                : PauseButtonBaseScale;
            displayScale = MoveTowards(displayScale, targetScale, maxDelta);
            button->SetScale({ displayScale, displayScale });
        };

    updateScale(returnTitleButton, returnTitleButtonDisplayScale);
    updateScale(battleActionButton, battleActionButtonDisplayScale);
    updateScale(backToRoomButton, backToRoomButtonDisplayScale);
    updateScale(quitGameButton, quitGameButtonDisplayScale);
}

void Pause::ResetPauseButtonScales()
{
    const auto resetScale = [](const std::shared_ptr<UIButtonComponent>& button, float& displayScale)
        {
            displayScale = PauseButtonBaseScale;
            if (button)
                button->SetScale({ PauseButtonBaseScale, PauseButtonBaseScale });
        };

    resetScale(returnTitleButton, returnTitleButtonDisplayScale);
    resetScale(battleActionButton, battleActionButtonDisplayScale);
    resetScale(backToRoomButton, backToRoomButtonDisplayScale);
    resetScale(quitGameButton, quitGameButtonDisplayScale);
}

void Pause::ApplyButtonLayout()
{
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    const bool isBossBattle = player && player->IsBossBattle();
    const int normalCount = isBossBattle ? 3 : 2;
    const float totalSpan = normalCount * buttonSpacing + quitExtraGap;
    const float firstY = menuCenterY - totalSpan * 0.5f;

    returnTitleButton->SetWorldPosition({ 980.0f, firstY });
    if (isBossBattle)
    {
        backToRoomButton->SetWorldPosition({ 980.0f, firstY + buttonSpacing });
        battleActionButton->SetWorldPosition({ 978.0f, firstY + buttonSpacing * 2.0f });
    }
    else
    {
        battleActionButton->SetWorldPosition({ 978.0f, firstY + buttonSpacing });
    }
    quitGameButton->SetWorldPosition({ 980.0f, firstY + normalCount * buttonSpacing + quitExtraGap });
}

float Pause::GetPauseButtonDisplayScale(const UIButtonComponent* button) const
{
    if (button == returnTitleButton.get()) return returnTitleButtonDisplayScale;
    if (button == battleActionButton.get()) return battleActionButtonDisplayScale;
    if (button == backToRoomButton.get()) return backToRoomButtonDisplayScale;
    if (button == quitGameButton.get()) return quitGameButtonDisplayScale;
    return PauseButtonBaseScale;
}

void Pause::SetPauseButtonAction(const std::shared_ptr<UIButtonComponent>& button, std::function<void()> action)
{
    if (!button)
        return;

    button->onClick = [this, action = std::move(action)]()
        {
            if (state != PauseState::Paused)
                return;
            CoreAudio::PlayOneShot(PauseButtonPushSound);
            if (action)
                action();
        };
}

void Pause::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::SeparatorText("Pause Selection Button");
    ImGui::DragFloat("Selected Button Scale", &selectedButtonScale, 0.005f, 1.0f, 1.25f, "%.3f");
    ImGui::DragFloat("Button Scale Animation Duration", &buttonScaleAnimationDuration, 0.005f, 0.01f, 1.0f, "%.3f sec");

    ImGui::SeparatorText("Pause Selection Line");
    ImGui::DragFloat("Line Width Ratio", &selectionLineWidthRatio, 0.01f, 0.05f, 2.0f, "%.3f");
    ImGui::DragFloat("Line Gap", &selectionLineGap, 1.0f, -200.0f, 200.0f, "%.1f");
    ImGui::DragFloat("Line Y Offset", &selectionLineYOffset, 1.0f, -200.0f, 200.0f, "%.1f");
    ImGui::DragFloat("Line Visual Scale X", &selectionLineScale.x, 0.005f, 0.01f, 2.0f, "%.3f");
    ImGui::DragFloat("Line Visual Scale Y", &selectionLineScale.y, 0.005f, 0.01f, 2.0f, "%.3f");
    ImGui::DragFloat("Line Reveal Duration", &selectionLineAnimationDuration, 0.005f, 0.01f, 1.0f, "%.3f sec");

    ImGui::SeparatorText("Pause Menu Layout");
    const bool layoutChanged =
        ImGui::DragFloat("Menu Center Y", &menuCenterY, 1.0f, 200.0f, 900.0f, "%.1f") |
        ImGui::DragFloat("Button Spacing", &buttonSpacing, 1.0f, 20.0f, 300.0f, "%.1f") |
        ImGui::DragFloat("Quit Extra Gap", &quitExtraGap, 1.0f, 0.0f, 300.0f, "%.1f");

    if (layoutChanged && state == PauseState::Paused)
    {
        ApplyButtonLayout();
        UpdateSelectionLines();
    }
#endif
}
