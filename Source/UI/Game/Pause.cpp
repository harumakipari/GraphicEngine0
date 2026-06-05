#include "pch.h"
#include "Pause.h"

#include "SceneTransitionManager.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Utility/Time.h"

void Pause::Initialize(const Transform& transform)
{
    const auto scene = GetOwnerScene();

    auto uiManager = scene->GetUIManager();

    pauseBackImage = std::make_shared<UIImageComponent>("./Data/Textures/ScissorsUI/back.png", "back");
    pauseBackImage->SetWorldPosition({ 1920 * 0.5f, 1080 * 0.5f });
    pauseBackImage->SetPivot({ 0.5f,0.5f });
    pauseBackImage->SetScale({ 1.0f,1.0f });
    pauseBackImage->SetSize({ 1920, 1080 });
    pauseBackImage->SetColor(DirectX::XMFLOAT4{ 0.24f,0.08f,0.127f,0.5f });
    pauseBackImage->SetVisible(false);
    pauseBackImage->zOrder = 95; // 奥
    uiManager->Add(pauseBackImage);

    pausePanel = std::make_shared<UIImageComponent>("./Data/Textures/ScissorsUI/pause_panel.png", "pause_panel");
    pausePanel->SetWorldPosition({ 1920 * 0.5f, 1080 * 0.5f });
    pausePanel->SetPivot({ 0.5f,0.5f });
    pausePanel->SetScale({ 1.0f,1.0f });
    pausePanel->SetSize({ 1033, 860 });
    pausePanel->SetVisible(false);
    pausePanel->zOrder = 100; // 手前に描画する
    uiManager->Add(pausePanel);

    // メニューボタン
    {
        menuButton = std::make_shared<UIButtonComponent>("./Data/Textures/ScissorsUI/menu.png", "menu");
        menuButton->SetWorldPosition({ 100, 85 });
        menuButton->SetPivot({ 0.5f,0.5f });
        menuButton->SetSize({ 140, 140 });
        menuButton->zOrder = 100; // 手前に描画する
        uiManager->Add(menuButton);
        menuButton->onClick = [&]()
            {
                OpenPause();
            };
    }

    // ゲームへ戻る
    closeButton = std::make_shared<UIButtonComponent>("./Data/Textures/ScissorsUI/back_to_game.png", "back_to_game");
    closeButton->SetWorldPosition({ 979, 463 });
    closeButton->SetPivot({ 0.5f,0.5f });
    closeButton->SetSize({ 391, 123 });
    closeButton->SetVisible(false);
    closeButton->SetEnable(false);
    closeButton->zOrder = 105; // 手前に描画する

    closeButton->onClick = [&]()
        {
            ClosePause();
        };
    uiManager->Add(closeButton);

    returnTitleButton = std::make_shared<UIButtonComponent>("./Data/Textures/ScissorsUI/back_to_title.png", "back_to_title");
    returnTitleButton->SetWorldPosition({ 980, 607 });
    returnTitleButton->SetPivot({ 0.5f,0.5f });
    returnTitleButton->SetSize({ 391, 123 });
    returnTitleButton->SetVisible(false);
    returnTitleButton->SetEnable(false);
    returnTitleButton->zOrder = 105; // 手前に描画する
    returnTitleButton->onClick = [&]()
        {
            if (state != PauseState::Paused)
            {// カウントダウンが何回も起こるのを防ぐため
                return;
            }


            CoreAudio::PlayOneShot(L"./Data/Sound/SE1/push_button.wav");
            Time::timeScale = 1.0f;

            const char* types[] = { "0", "1" };
            SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", "TitleScene"), std::make_pair("type", types[rand() % 2]), std::make_pair("fromScene","GameScene")  });

        };

    GetOwnerScene()->GetUIManager()->Add(returnTitleButton);

    retryButton = std::make_shared<UIButtonComponent>("./Data/Textures/ScissorsUI/retry.png", "retry");
    retryButton->SetWorldPosition({ 978, 751 });
    retryButton->SetPivot({ 0.5f,0.5f });
    retryButton->SetSize({ 391, 123 });
    retryButton->SetVisible(false);
    retryButton->SetEnable(false);
    retryButton->zOrder = 105; // 手前に描画する
    retryButton->onClick = [&]()
        {
            if (state != PauseState::Paused)
            {// カウントダウンが何回も起こるのを防ぐため
                return;
            }

            CoreAudio::PlayOneShot(L"./Data/Sound/SE1/push_button.wav");
            Time::timeScale = 1.0f;

            const char* types[] = { "0", "1" };

            SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", retrySceneName), std::make_pair("type", types[rand() % 2]),  std::make_pair("fromScene","GameScene")  });

            // Scene::_transition("LoadingScene", { std::make_pair("preload",retrySceneName), std::make_pair("type", types[rand() % 2]) });

        };

    GetOwnerScene()->GetUIManager()->Add(retryButton);

    for (int i = 0; i < 3; i++)
    {
        const int num = 3 - i; // 3,2,1
        std::string filename = "./Data/Textures/ScissorsUI/CountDown_" + std::to_string(num) + ".png";

        countDownImages[i] = std::make_shared<UIImageComponent>(filename, "countDown_" + std::to_string(num));

        countDownImages[i]->SetWorldPosition({ 967, 490 });
        countDownImages[i]->SetPivot({ 0.5f,0.5f });
        countDownImages[i]->SetScale({ 4.f,4.f });
        countDownImages[i]->SetSize({ 200, 200 });
        countDownImages[i]->SetVisible(false);
        countDownImages[i]->zOrder = 110; // 手前に描画する

        scene->GetUIManager()->Add(countDownImages[i]);
    }

    uiManager->AddButton(closeButton);
    uiManager->AddButton(returnTitleButton);
    uiManager->AddButton(retryButton);

    stopUpdate = false;


    //pausePanel = std::make_shared<UIImageComponent>("./Data/Textures/UI/CountDown_1.png", "pause_panel");
    //pausePanel->SetVisible(false);
    //scene->GetUIManager()->Add(pausePanel);

}

void Pause::Update(float deltaTime)
{
    if (stopUpdate)
    {
        return;
    }

    // ポーズ切り替え
    if (InputSystem::GetInputState("Pause", InputStateMask::Trigger))
    {
        if (state == PauseState::Playing)
        {
            OpenPause();
        }
        else if (state == PauseState::Paused)
        {
            ClosePause();
        }
    }

    // カウントダウン処理（既存）
    if (state != PauseState::ResumeCountdown)
        return;

    countdownTime -= Time::UnscaledDeltaTime();

    const int current = static_cast<int>(ceil(countdownTime)); // 3,2,1

    // 全部消す
    for (auto& img : countDownImages)
    {
        img->SetVisible(false);
        img->SetEnable(false);
    }

    if (current >= 1 && current <= 3)
    {
        int index = 3 - current; // 3→0, 2→1, 1→2
        countDownImages[index]->SetVisible(true);
        countDownImages[index]->SetEnable(true);

        float t = countdownTime - floor(countdownTime);
        float scale = std::lerp(4.5f, 3.f, t);
        countDownImages[index]->SetScale({ scale, scale });

        float alpha = std::lerp(0.0f, 1.0f, t);
        countDownImages[index]->SetColor(XMFLOAT4{ 1,1,1, alpha });

        if (current != lastCountdownNumber)
        {
            CoreAudio::PlayOneShot(L"./Data/Sound/SE1/pause_countDown_se.wav", 3.0f);
            lastCountdownNumber = current;
        }
    }


    if (countdownTime <= 0.0f)
    {
        // 全部消す
        for (auto& img : countDownImages)
        {
            img->SetVisible(false);
            img->SetEnable(false);
        }

        // 音の再生を有効にする
        CoreAudio::SetSystemPaused(false);

        Time::timeScale = 1.0f;
        GetOwnerScene()->SetPaused(false);
        state = PauseState::Playing;
        lastCountdownNumber = -1;
        menuButton->SetEnable(true);
        menuButton->SetVisible(true);

    }
}

// ポーズ画面を隠す
void Pause::HidePauseMenu()
{
    Logger::Log("stop update false");
    stopUpdate = true;
    menuButton->SetEnable(false);
    menuButton->SetVisible(false);
}

// ポーズ画面を開くときの処理
void Pause::OpenPause()
{
    CoreAudio::PlayOneShot(L"./Data/Sound/SE1/escape_se.wav");

    pauseBackImage->SetVisible(true);

    pausePanel->SetVisible(true);
    pausePanel->SetEnable(true);

    closeButton->SetVisible(true);
    closeButton->SetEnable(true);

    returnTitleButton->SetVisible(true);
    returnTitleButton->SetEnable(true);

    retryButton->SetVisible(true);
    retryButton->SetEnable(true);

    menuButton->SetEnable(false);
    menuButton->SetVisible(false);

    state = PauseState::Paused;

    Time::timeScale = 0.0f;

    // 音の再生を無効にする
    CoreAudio::SetSystemPaused(true);

    GetOwnerScene()->SetPaused(true);

    // 初期選択　UI　ゲームパッドの時の最初に選択するUI
    GetOwnerScene()->GetUIManager()->SetSelected(closeButton.get());
}

// ポーズ画面を閉じる時の処理
void Pause::ClosePause()
{
    if (state != PauseState::Paused)
    {// カウントダウンが何回も起こるのを防ぐため
        return;
    }



    //CoreAudio::PlayOneShot(L"./Data/Sound/SE/push_button.wav");
    pauseBackImage->SetVisible(false);
    pausePanel->SetVisible(false);
    pausePanel->SetEnable(false);
    closeButton->SetEnable(false);
    closeButton->SetVisible(false);
    returnTitleButton->SetEnable(false);
    returnTitleButton->SetVisible(false);
    retryButton->SetEnable(false);
    retryButton->SetVisible(false);
    state = PauseState::ResumeCountdown;
    countdownTime = 3.0f;
}