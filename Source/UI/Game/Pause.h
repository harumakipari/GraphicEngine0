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
        ResumeCountdown,
    };

public:
    explicit Pause(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    // リトライするシーンの名前を設定する
    void SetRetrySceneName(const std::string& sceneName) { retrySceneName = sceneName; }

    // ポーズ画面を隠す
    void HidePauseMenu();

private:
    // ポーズ画面を開くときの処理
    void OpenPause();

    // ポーズ画面を閉じる時の処理
    void ClosePause();


private:
    std::shared_ptr<UIImageComponent> pauseBackImage; //ポーズ中の背景
    std::shared_ptr<UIImageComponent> pausePanel;
    std::shared_ptr<UIButtonComponent> menuButton;
    std::shared_ptr<UIButtonComponent> returnTitleButton;
    std::shared_ptr<UIButtonComponent> returnMainRoomButton;
    std::shared_ptr<UIButtonComponent> restartBattleButton;


    std::array<std::shared_ptr<UIImageComponent>, 3> countDownImages;
    PauseState state = PauseState::Playing;
    float countdownTime = 3.0f;
    int lastCountdownNumber = -1;
    bool stopUpdate = true;

    std::string retrySceneName = "MainScene";
};
