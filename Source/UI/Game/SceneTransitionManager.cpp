#include "pch.h"
#include "SceneTransitionManager.h"

#include "TransitionEffect.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Scene/Scene.h"


void SceneTransitionManager::Initialize()
{
    scaleTransitionEffect = std::make_shared<ScaleTransitionEffect>();
    scaleTransitionEffect->Initialize();

    fadeTransitionEffect = std::make_shared<FadeTransitionEffect>();
    fadeTransitionEffect->Initialize();
}

const char* ToString(SceneTransitionManager::State s)
{
    switch (s)
    {
    case SceneTransitionManager::State::Idle: return "Idle";
    case SceneTransitionManager::State::Closing: return "Closing";
    case SceneTransitionManager::State::ChangingScene: return "ChangingScene";
    case SceneTransitionManager::State::Opening: return "Opening";
    }
    return "Unknown";
}

void SceneTransitionManager::RequestTransition(const std::string& nextScene, const SceneTransitionParam& param, TransitionStyle style)
{
    if (state_ != State::Idle)
    {
        Logger::Warning((ToString(state_)));
        return;
    }

    // 入力を無効化する
    InputSystem::SetInputEnabled(false);

    // 音の再生を無効にする
    CoreAudio::SetMutedBySystem(true);
    CoreAudio::SetSystemPaused(true);

    currentStyle = style;

    nextScene_ = nextScene;
    this->param = param;

    if (style == TransitionStyle::Scale)
    {
        scaleTransitionEffect->OnSceneChanged();
        if (param.contains("stage"))
        {
            const auto& stage = param.at("stage");
            if (stage == "BOSS")
            {
                scaleTransitionEffect->SetTransitionTexture("BOSS");
            }
            //else if (stage=="FIRST")
            //{
            //    scaleTransitionEffect->SetTransitionTexture("FIRST");
            //}
            //else if (stage=="BOBBIN_FIRST")
            //{
            //    scaleTransitionEffect->SetTransitionTexture("BOBBIN_FIRST");
            //}
            //else if (stage=="REFLECT_WALL")
            //{
            //    scaleTransitionEffect->SetTransitionTexture("REFLECT_WALL");
            //}
            //else if (stage=="DIFFICULT")
            //{
            //    scaleTransitionEffect->SetTransitionTexture("DIFFICULT");
            //}
            else
            {
                scaleTransitionEffect->SetTransitionTexture("FIRST");
            }
        }
        scaleTransitionEffect->Start(TransitionDirection::Close);
    }
    else
    {
        fadeTransitionEffect->OnSceneChanged();
        fadeTransitionEffect->Start(TransitionDirection::Close);
    }
    state_ = State::Closing;
}

void SceneTransitionManager::Update(float deltaTime)
{
    if (!scaleTransitionEffect)
        return;
    bool finished = false;
    if (currentStyle == TransitionStyle::Scale)
    {
        scaleTransitionEffect->Update(deltaTime);
        finished = scaleTransitionEffect->IsFinished();
    }
    else
    {
        fadeTransitionEffect->Update(deltaTime);
        finished = fadeTransitionEffect->IsFinished();
    }

    switch (state_)
    {
    case State::Closing:
        if (scaleTransitionEffect->IsFinished())
        {
            state_ = State::ChangingScene;
            Scene::_transition(nextScene_, param);
            Logger::Log(U8("Closion を通った"));
        }
        break;
    case State::ChangingScene:
        // 新シーン生成後に呼ばれる想定
        break;
    case State::Opening:
        if (finished)
        {
            state_ = State::Idle;
            Logger::Log(U8("Opening を通った"));

            // 入力を有効化する
            InputSystem::SetInputEnabled(true);

            // 音の再生を有効にする
            CoreAudio::SetMutedBySystem(false);
            CoreAudio::SetSystemPaused(false);


            if (onOpeningFinished)
            {
                onOpeningFinished();
                onOpeningFinished = nullptr;
            }
            param.clear();
        }
        break;
    }
}

void SceneTransitionManager::SetOnOpeningFinished(const std::function<void()>& callback)
{
    onOpeningFinished = callback;
}


void SceneTransitionManager::NotifySceneChanged()
{
    if (state_ == State::ChangingScene)
    {
        if (currentStyle == TransitionStyle::Scale)
        {
            scaleTransitionEffect->OnSceneChanged();
            scaleTransitionEffect->Start(TransitionDirection::Open);
        }
        else
        {
            fadeTransitionEffect->OnSceneChanged();
            fadeTransitionEffect->Start(TransitionDirection::Open);
        }

        state_ = State::Opening;

    }
}
