#include "pch.h"


#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

#include "PuddingGameScene.h"

#include "Components/Audio/CoreAudioSourceComponent.h"
#include "Graphics/Core/Graphics.h"
#include "Engine/Input/InputSystem.h"
#include "Core/ActorManager.h"

#include "Game/Actors/Camera/LoadingCamera.h"
#include "Game/Actors/Dessert/Pudding.h"
#include "Game/Actors/Dessert/TargetPudding.h"
#include "Game/Actors/Enemy/Boss/BossEnemy.h"
#include "Game/Actors/Stage/Cloth.h"


#include "Physics/Physics.h"
#include "Game/DarkGame/DarkActors/DarkStage.h"


#include "Physics/CollisionSystem.h"
#include "UI/UIManager.h"


bool PuddingGameScene::Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
{
    SceneBase::Initialize(device, width, height, props);

    Physics::Instance().Initialize();

    //アクターをセット
    SetUpActors();
    return true;
}

void PuddingGameScene::Start()
{
    auto audioActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<Actor>("Audio");
    auto audioComp = audioActor->AddComponent<CoreAudioSourceComponent>("audioSource");
    audioComp->SetSource(L"./Data/Sound/BGM/title.wav");
    audioComp->SetLoop(true);
    audioComp->Play();
    audioComp->SetVolume(0.2f);

    std::shared_ptr<UIImageComponent> image = std::make_shared<UIImageComponent>("image");
    image->SetWorldPosition({ 50, 50 });
    image->SetSize({ 200, 200 });

    uiManager->Add(image);

    std::shared_ptr<UIButtonComponent> button = std::make_shared<UIButtonComponent>("./Data/Textures/UI/start_button.png", "button");
    button->SetWorldPosition({ 300, 50 });
    button->SetSize({ 200, 80 });

    uiManager->Add(button);

    std::shared_ptr<UIGaugeComponent> gauge = std::make_shared<UIGaugeComponent>("gauge");
    gauge->SetWorldPosition({ 50, 300 });
    gauge->SetSize({ 300, 40 });

    uiManager->Add(gauge);

    // ボタンでゲージ減らす
    button->onClick = [gauge]()
        {
            Logger::Log(u8"ボタンButton Clicked!");
            static float  value = 1.0f;
            const char* types[] = { "0", "1" };
            //Scene::_transition("LoadingScene", { std::make_pair("preload", "SampleScene"), std::make_pair("type", types[rand() % 2]) });
            SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", "SampleScene"), std::make_pair("type", types[rand() % 2]) });

            value -= 0.1f;
            if (value < 0.0f)
                value = 0.0f;
            gauge->SetValue(value, 1.0f);
        };


    // シーンが切り替わった時に
    SceneTransitionManager::Instance().NotifySceneChanged();
}

void PuddingGameScene::Update(float deltaTime)
{
    using namespace DirectX;
    SceneBase::Update(deltaTime);

    Physics::Instance().Update(deltaTime);

    CollisionSystem::DetectAndResolveCollisions();
    CollisionSystem::ApplyPushAll();

#ifdef _DEBUG
    if (InputSystem::GetInputState("Space", InputStateMask::Trigger))
    {
        const char* types[] = { "0", "1" };
        //SceneTransitionManager::Instance().RequestTransition("SampleScene");

        Scene::_transition("LoadingScene", { std::make_pair("preload", "SampleScene"), std::make_pair("type", types[rand() % 2]) });
    }
#endif // !_DEBUG
}

void PuddingGameScene::SetUpActors()
{
    auto mainCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<MainCamera>("mainCameraActor");
    auto mainCameraComponent = mainCameraActor->GetComponent<TPSCameraComponent>();

    //Transform stageTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    //auto stage = this->GetActorManager()->CreateAndRegisterActorWithTransform<Stage>("stage", stageTr); // 元のモデルの scale を 0.4f

    auto debugCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DebugCamera>("debugCam");
    debugCameraActor->SetPosition({ 0.0f,10.0f,-20.0f });

    //Transform buildTr(DirectX::XMFLOAT3{ -5.0f,0.0f,3.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 0.8f,0.8f,0.8f });
    //auto building = this->GetActorManager()->CreateAndRegisterActorWithTransform<Actor>("jerry", buildTr);
    //auto elasticComp = building->AddComponent<ElasticMeshComponent>("jerry");
    //elasticComp->SetModel("./Data/Models/pink_pudding/scene.gltf");
    //elasticComp->Initialize();


    //Transform targetPuddingTr(DirectX::XMFLOAT3{ 0.0f,0.0f,13.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 1.8f,1.8f,1.8f });
    //auto targetPudding = this->GetActorManager()->CreateAndRegisterActorWithTransform<TargetPudding>("targetPudding", targetPuddingTr);


    //CameraManager::SetGameCamera(mainCameraActor);
    SetActiveCamera(mainCameraActor);
    Logger::Log(U8("PuddingGameシーンのカメラ設定される。"));

    cameraManager->SetDebugCamera(debugCameraActor);

    //mainCameraComponent->target = (building->GetRootComponent());
    //mainCameraComponent->pitch = DirectX::XMConvertToRadians(.0f);

}

bool PuddingGameScene::Uninitialize(ID3D11Device* device)
{
    SceneBase::Uninitialize(device);
    Physics::Instance().Finalize();
    return true;
}

void PuddingGameScene::Render(ID3D11DeviceContext* immediateContext, float deltaTime)
{
    SceneBase::Render(immediateContext, deltaTime);
}

void PuddingGameScene::DrawGui()
{
    SceneBase::DrawGui();
}
