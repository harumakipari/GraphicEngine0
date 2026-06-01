#include "pch.h"
#include "TransitionEffect.h"

#include "Engine/Scene/Scene.h"


void ScaleTransitionEffect::Initialize()
{
    // 画像が小さくなるアニメーション
    //Audio::PlayOneShot(L"./Data/Sounds/SE/gameStart.wav", 1.5f);
    easingRunner = std::make_unique<EasingRunner>();

    float width = 1920.0f;
    float height = 1080.0f;

    float startScale = 220.0f;

    transitionTextures["TUTORIAL"] =
        std::make_shared<Sprite>(
            Graphics::GetDevice(),
            L"./Data/Textures/ScissorsUI/scene_change.png");

    transitionTextures["FIRST"] =
        std::make_shared<Sprite>(
            Graphics::GetDevice(),
            L"./Data/Textures/ScissorsUI/scene_change.png");

    transitionTextures["BOBBIN_FIRST"] =
        std::make_shared<Sprite>(
            Graphics::GetDevice(),
            L"./Data/Textures/ScissorsUI/scene_change.png");

    transitionTextures["REFLECT_WALL"] =
        std::make_shared<Sprite>(
            Graphics::GetDevice(),
            L"./Data/Textures/ScissorsUI/scene_change.png");

    transitionTextures["DIFFICULT"] =
        std::make_shared<Sprite>(
            Graphics::GetDevice(),
            L"./Data/Textures/ScissorsUI/scene_change.png");

    transitionTextures["BOSS"] =
        std::make_shared<Sprite>(
            Graphics::GetDevice(),
            L"./Data/Textures/ScissorsUI/scene_change_boss.png");

#if 0
    TestEasingHandler floatHandler;
    floatHandler.AddWait(0.5f);
    floatHandler.AddEasing(TestEaseType::OutSine, startScale, 1.0f, 2.5f);
    floatHandler.SetCompletedFunction([&]()
        {
            isFinishTransitionPerform = true;
        });
    PropertyAccessor<float> floatAccessor;
    floatAccessor.getter = [&]()->float { return 1.0f; };
    floatAccessor.setter = [&](float value) {
        float sizeX = width * value;
        float sizeY = height * value;
        spriteScale = value;
        };
    easingComponent->StartHandler(floatHandler, floatAccessor);
#endif // 0
    const auto scene = Scene::GetCurrentScene();

    sprite = std::make_shared<UISceneChangeComponent>("./Data/Textures/ScissorsUI/scene_change.png", "sceneChange");
    sprite->SetWorldPosition({ width * 0.5f, height * 0.5f });
    sprite->SetPivot({ 0.5f,0.5f });
    sprite->SetScale({ startScale,startScale });
    sprite->SetSize({ width, height });
    sprite->zOrder = 1000;
    //scene->GetUIManager()->Add(sprite);
}

void ScaleTransitionEffect::OnSceneChanged() const
{
    const auto scene = Scene::GetCurrentScene();
    scene->GetUIManager()->Add(sprite);
}

void ScaleTransitionEffect::Start(TransitionDirection dir)
{
    isFinishTransitionPerform = false;

    TestEasingHandler handler;
    float startScale = 160.0f;

    if (dir == TransitionDirection::Close)
    {
        spriteScale = startScale;
        handler.AddEasing(TestEaseType::OutSine, startScale, 1.0f, 2.f);
    }
    else // Open
    {
        startScale = 1.0f;
        handler.AddWait(0.1f);
        handler.AddEasing(TestEaseType::InSine, 1.0f, 220.0f, 0.8f);
    }

    handler.SetCompletedFunction([this]()
        {
            isFinishTransitionPerform = true;
        });

    PropertyAccessor<float> accessor;
    accessor.getter = [this]() { return spriteScale; };
    accessor.setter = [this](float v) { spriteScale = v; };

    easingRunner->StartHandler(handler, accessor);
}

void ScaleTransitionEffect::Update(float deltaTime)
{
    easingRunner->Tick(deltaTime);
    sprite->SetScale({ spriteScale,spriteScale });
}

void ScaleTransitionEffect::SetTransitionTexture(const std::string& stage)
{
    auto it = transitionTextures.find(stage);

    if (it != transitionTextures.end())
    {
        sprite->SetTexture(it->second);
    }
}

void FadeTransitionEffect::Initialize()
{
    easingRunner = std::make_unique<EasingRunner>();

    float width = 1920.0f;
    float height = 1080.0f;

    sprite = std::make_shared<UISceneChangeComponent>(
        "./Data/Textures/ScissorsUI/black.png",
        "fade");

    sprite->SetWorldPosition({ width * 0.5f, height * 0.5f });
    sprite->SetPivot({ 0.5f,0.5f });
    sprite->SetSize({ width,height });

    sprite->SetColor({ 0,0,0,0 }); // 黒 + α0

    sprite->zOrder = 5000;
}

void FadeTransitionEffect::OnSceneChanged() const
{
    auto scene = Scene::GetCurrentScene();
    scene->GetUIManager()->Add(sprite);
}

void FadeTransitionEffect::Start(TransitionDirection dir)
{
    isFinishTransitionPerform = false;

    TestEasingHandler handler;

    if (dir == TransitionDirection::Close)
    {
        // 透明 → 黒
        handler.AddEasing(
            TestEaseType::OutSine,
            0.0f,
            1.0f,
            1.0f);
    }
    else
    {
        // 黒 → 透明
        handler.AddEasing(
            TestEaseType::InSine,
            1.0f,
            0.0f,
            2.0f);
    }

    handler.SetCompletedFunction([this]()
        {
            isFinishTransitionPerform = true;
        });

    PropertyAccessor<float> accessor;

    accessor.getter = [this]()
        {
            return spriteAlpha;
        };

    accessor.setter = [this](float value)
        {
            spriteAlpha = value;
        };

    easingRunner->StartHandler(handler, accessor);
}

void FadeTransitionEffect::Update(float deltaTime)
{
    easingRunner->Tick(deltaTime);
      sprite->SetColor({
                1.0f,
                1.0f,
                1.0f,
                spriteAlpha
                });
}