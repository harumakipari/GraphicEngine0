#pragma once
#include "Components/Easing/CoreEasingComponent.h"
#include "UI/Widgets/Widget.h"

enum class TransitionDirection :uint8_t
{
    Close,// ‰æ–Ê‚ð•¢‚¤
    Open, // ‰æ–Ê‚ðŠJ‚­
};

class ScaleTransitionEffect
{
public:
    void Initialize();

    void Start(TransitionDirection dir);

    void Update(float deltaTime);

    bool IsFinished() const { return isFinishTransitionPerform; }

    void SetTransitionTexture(const std::string& path);

    void OnSceneChanged() const;

private:
    std::shared_ptr<UISceneChangeComponent> sprite;
    std::shared_ptr<EasingRunner> easingRunner;
    float time = 0.0f;
    float spriteScale = 1.0f;
    bool isFinishTransitionPerform = false;

    std::unordered_map<std::string, std::shared_ptr<Sprite>> transitionTextures;
};


class FadeTransitionEffect
{
public:
    void Initialize();

    void Start(TransitionDirection dir);

    void Update(float deltaTime);

    bool IsFinished() const { return isFinishTransitionPerform; }

    void OnSceneChanged() const;
private:
    std::shared_ptr<UISceneChangeComponent> sprite;
    std::shared_ptr<EasingRunner> easingRunner;
    float time = 0.0f;
    float spriteAlpha = 1.0f;
    bool isFinishTransitionPerform = false;
};

