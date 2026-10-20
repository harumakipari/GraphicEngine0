#pragma once
#include "Core/Actor.h"

enum class MovieSkipType : uint8_t
{
    None,
    PrisonOpening,
    BossEntry,
    BossDeath,
};

class UIImageComponent;

class GameMovieSkipUIActor : public Actor
{
public:
    explicit GameMovieSkipUIActor(const std::string& actorName) : Actor(actorName) {}

    void Initialize(const Transform& transform) override;

    void DrawImGuiDetails()override;

    // 今の流れているムービータイプを設定する
    void SetActiveMovieType(MovieSkipType type);

    void UpdatePresentation();
    bool ConsumeSkipRequest();
    void ResetPresentation();

private:
    MovieSkipType movieSkipType = MovieSkipType::None;
    bool movieSkipPromptVisible = false;
    bool movieSkipConfirmed = false;
    bool skipRequest = false;
    bool waitingForButtonsReleased = false;
    float movieSkipPromptTimer = 0.0f;
    float movieSkipPromptFade = 0.0f;
    float movieSkipPromptTimeout = 2.0f;    // 何秒だけスキップボタンを表示するか
    float movieSkipPromptFadeDuration = 0.15f;
    DirectX::XMFLOAT2 movieSkipPromptRightBottomOffset{ 56.0f, 48.0f };
    float movieSkipPromptScale = 0.36f;
    std::shared_ptr<UIImageComponent> movieSkipPromptImage;
};
