#pragma once
#include "TransitionEffect.h"

enum class TransitionStyle : uint8_t
{
    Scale,
    Fade
};

class SceneTransitionManager
{
public:
    using SceneTransitionParam = std::unordered_map<std::string, std::string>;

    static SceneTransitionManager& Instance()
    {
        static SceneTransitionManager instance;
        return instance;
    }

    void Initialize();

    void RequestTransition(
        const std::string& nextScene,
        const SceneTransitionParam& param = {}, TransitionStyle style = TransitionStyle::Fade
    );

    void Update(float deltaTime);

    // シーンが切り替わった時に呼ばれる シーン遷移の演出を入れる
    void NotifySceneChanged(); 

    // シーンが切り替わり演出が終了した後に呼ぶ関数
    void SetOnOpeningFinished(const std::function<void()>& callback);

    void Draw(){}

    const SceneTransitionParam& GetParams() const
    {
        return param;
    }

    enum class State :uint8_t
    {
        Idle,
        Closing,
        ChangingScene,
        Opening
    };

private:
    std::function<void()> onOpeningFinished;

    State state_ = State::ChangingScene;

    std::string nextScene_;
    SceneTransitionParam param;

    TransitionStyle currentStyle = TransitionStyle::Fade;

    std::shared_ptr<FadeTransitionEffect> fadeTransitionEffect;
    std::shared_ptr<ScaleTransitionEffect> scaleTransitionEffect;
};
