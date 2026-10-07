#include "pch.h"
#include "Pause.h"

#include "SceneTransitionManager.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Utility/Time.h"
#include "Engine/Framework/Framework.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Scenes/GameScene.h"

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
    returnTitleButton->onClick = [this]()
        {
            if (state != PauseState::Paused) return;
            HideSelectionLines();
            CoreAudio::PlayOneShot(L"./Data/Sound/SE/button_push.wav");
            Time::timeScale = 1.0f;
            const char* types[] = { "0", "1" };
            SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", "TitleScene"), std::make_pair("type", types[rand() % 2]), std::make_pair("fromScene","GameScene") });
        };
    uiManager->Add(returnTitleButton);

    battleActionButton = std::make_shared<UIButtonComponent>("./Data/Textures/UI/Pause/go_to_boss_room.png", "battle_action");
    battleActionButton->SetWorldPosition({ 978,751 });
    battleActionButton->SetPivot({ .5f,.5f });
    battleActionButton->SetSize({ 785,80 });
    battleActionButton->SetScale({ 0.5f,0.5f });

    configureButton(battleActionButton);
    battleActionButton->onClick = [this]()
        {
            const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
            const bool isBossBattle = player && player->IsBossBattle();
            ClosePause();
            if (auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene()))
            {
                if (isBossBattle) gameScene->RequestRestartBossBattleFromPause();
                else gameScene->RequestBossEntryFromPause();
            }
        };
    uiManager->Add(battleActionButton);

    backToRoomButton = std::make_shared<UIButtonComponent>("./Data/Textures/UI/Pause/back_to_room.png", "back_to_room");
    backToRoomButton->SetWorldPosition({ 980, 751 });
    backToRoomButton->SetPivot({ .5f,.5f });
    backToRoomButton->SetSize({ 662,76 });
    backToRoomButton->SetScale({ 0.5f,0.5f });

    configureButton(backToRoomButton);
    backToRoomButton->onClick = [this]()
        {
            ClosePause();
            if (auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene()))
                gameScene->RequestReturnToMainRoomStart();
        };
    uiManager->Add(backToRoomButton);

    quitGameButton = std::make_shared<UIButtonComponent>("./Data/Textures/UI/Pause/quit_game.png", "quit_game");
    quitGameButton->SetWorldPosition({ 980, 850 });
    quitGameButton->SetPivot({ .5f,.5f });
    quitGameButton->SetSize({ 472,92 });
    quitGameButton->SetScale({ 0.5f,0.5f });

    configureButton(quitGameButton);
    quitGameButton->onClick = [this]()
        {
            if (state != PauseState::Paused) return;
            Framework::RequestExit();
        };
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

    const bool canOpenPause = CanOpenPause();
    if (state == PauseState::Playing)
    {
        menuButton->SetVisible(canOpenPause);
        menuButton->SetEnable(canOpenPause);
    }

    if (InputSystem::GetInputState("Pause", InputStateMask::Trigger))
    {
        if (state == PauseState::Playing && canOpenPause) OpenPause();
        else if (state == PauseState::Paused) ClosePause();
    }
    if (state == PauseState::Paused && InputSystem::GetInputState("GamePadB", InputStateMask::Trigger)) ClosePause();
    if (state == PauseState::Paused) UpdateSelectionLines();
}

void Pause::HidePauseMenu()
{
    stopUpdate = true; menuButton->SetEnable(false); menuButton->SetVisible(false); HideSelectionLines();
}

void Pause::OpenPause()
{
    if (!CanOpenPause()) return;
    CoreAudio::PlayOneShot(L"./Data/Sound/SE/pause_menu.wav");
    pauseBackImage->SetVisible(true); pausePanel->SetVisible(true); pausePanel->SetEnable(true);
    returnTitleButton->SetVisible(true); returnTitleButton->SetEnable(true);
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    const bool isBossBattle = player && player->IsBossBattle();
    battleActionButton->SetTexture(std::make_shared<Sprite>(Graphics::GetDevice(), isBossBattle ? L"./Data/Textures/UI/Pause/restart_battle.png" : L"./Data/Textures/UI/Pause/go_to_boss_room.png"));
    returnTitleButton->SetWorldPosition({ 980, 480 });
    backToRoomButton->SetWorldPosition({ 980, 600 });
    battleActionButton->SetWorldPosition({ 978, isBossBattle ? 720.0f : 600.0f });
    battleActionButton->SetVisible(true); battleActionButton->SetEnable(true);
    backToRoomButton->SetVisible(isBossBattle); backToRoomButton->SetEnable(isBossBattle);
    quitGameButton->SetVisible(true); quitGameButton->SetEnable(true);
    menuButton->SetEnable(false); menuButton->SetVisible(false);
    state = PauseState::Paused; Time::timeScale = 0; CoreAudio::SetSystemPaused(true); GetOwnerScene()->SetPaused(true);
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
    CoreAudio::SetSystemPaused(false); Time::timeScale = 1; GetOwnerScene()->SetPaused(false); state = PauseState::Playing;
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
    if (selected != lastSelectedButton) { lastSelectedButton = const_cast<UIButtonComponent*>(selected); selectionLineAnimationProgress = 0; }
    selectionLineAnimationProgress = (std::min)(1.0f, selectionLineAnimationProgress + Time::UnscaledDeltaTime() / selectionLineAnimationDuration);
    const float lineWidth = selected->GetSize().x * .5f;
    const float buttonHalfWidth = selected->GetSize().x * .5f;
    const float offset = buttonHalfWidth + lineWidth * .5f + selectionLineGap;
    const auto position = selected->GetWorldPosition();
    selectionLineLeft->SetWorldPosition({ position.x - offset, position.y }); selectionLineRight->SetWorldPosition({ position.x + offset, position.y });
    for (const auto& line : { selectionLineLeft, selectionLineRight }) { line->SetSize({ lineWidth,5 }); line->SetScale({ selectionLineScale.x * selectionLineAnimationProgress, selectionLineScale.y }); line->SetVisible(true); line->SetEnable(true); }
}