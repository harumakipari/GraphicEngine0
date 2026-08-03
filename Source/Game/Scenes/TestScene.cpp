#include "pch.h"
#include "TestScene.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

#include "Components/Audio/AudioSourceComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Core/ActorManager.h"
#include "Engine/Debug/SceneEditor.h"
#include "Engine/Utility/Time.h"

#include "Game/Actors/Player/Player.h"
#include "Game/Actors/Stage/Cloth.h"


#include "Physics/Physics.h"
#include "Game/DarkGame/DarkActors/DarkStage.h"
#include "Game/DarkGame/DarkActors/DarkStageChandelierActor.h"
#include "Game/DarkGame/DarkActors/DoorActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/SkeletonWarriorEnemy.h"

#include "Physics/CollisionSystem.h"
#include "UI/UIManager.h"
#include "UI/Game/Pause.h"

bool TestScene::Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
{
    PROFILE_FUNCTION();

    // ライトの方向と色を設定
    lightDirection = { 0.722f, -0.38f, -0.0211f, 0.9f };   // 上の窓からの光
    lightColor = { 1.0f, 0.8f, 1.0f, 2.6f };
    {
        SceneBase::Initialize(device, width, height, props);
    }
    {
        Physics::Instance().Initialize();
    }
    {
        PROFILE_SCOPE("SetUpActors Init");
        //アクターをセット
        SetUpActors();
    }

    return true;
}

void TestScene::Start()
{
    auto audioActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<Actor>("Audio");
    auto audioComp = audioActor->AddComponent<AudioSourceComponent>("audioSource");
    audioComp->SetSource(L"./Data/Sound/BGM/game_bgm.wav");
    audioComp->SetLoop(true);
    audioComp->Play();
    audioComp->SetVolume(1.2f);
#if 0
    cameraManager->ToggleCinematicCamera(this);

    SceneEditor::LoadPresetList(); // 更新
    std::string file = "WindowWall1.json";
    static SceneState savedState;
    SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
    savedState.Apply(Scene::GetCurrentScene());
#else
    //// シーンプリセットを設定する
    //SceneEditor::LoadPresetList(); // 更新
    //std::string file = "newPreset.json";
    //static SceneState savedState;
    //SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
    //savedState.ApplyScenePreset(Scene::GetCurrentScene());
#endif // 0

    // シーンが切り替わった時に
    SceneTransitionManager::Instance().NotifySceneChanged();

}

void TestScene::Update(float deltaTime)
{
    using namespace DirectX;
    SceneBase::Update(deltaTime);

    Physics::Instance().Update(Time::UnscaledDeltaTime());
    CollisionSystem::DetectAndResolveCollisions();
    CollisionSystem::ApplyPushAll();

    if (InputSystem::GetInputState("Space", InputStateMask::Trigger))
    {
        const char* types[] = { "0", "1" };
        SceneTransitionManager::Instance().RequestTransition("GameScene");
    }
}

void TestScene::SetUpActors()
{
    Transform mainCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto mainCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<MainCamera>("mainCameraActor", mainCameraTr);
    {
        PROFILE_SCOPE("Create Player");
        Transform playerTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.07f,1.07f,1.07f });
        auto player = this->GetActorManager()->CreateAndRegisterActorWithTransform<Player>("player", playerTr);
        mainCameraActor->SetLookTarget(player->GetRootComponent());
        player->characterMovementComponent->SetUseGravity(false);
    }
    SetActiveCamera(mainCameraActor);
    Logger::Log(U8("sampleシーンのカメラ設定される。"));

    Transform debugCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto debugCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DebugCamera>("debugCam", debugCameraTr);
    cameraManager->SetDebugCamera(debugCameraActor);

    Transform cinemaCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto cinemaCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<CinemaCamera>("cinemaCam", cinemaCameraTr);
    cameraManager->SetCinematicCamera(cinemaCameraActor);

    Transform movieCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto movieCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<MovieCamera>("movieCam", movieCameraTr);
    cameraManager->SetMovieCamera(movieCameraActor);

    Transform GruxEnemyTr(DirectX::XMFLOAT3{ 1.69f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.3f,1.3f,1.3f });
    auto GruxEnemyActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<GruxEnemy>("GruxEnemy", GruxEnemyTr);

    Transform stageTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto stage = this->GetActorManager()->CreateAndRegisterActorWithTransform<Actor>("stage", stageTr);
    auto staticMeshComponent = stage->AddComponent<class StaticMeshComponent>("staticMeshComponent");
    staticMeshComponent->SetModel("./Data/Models/Stage/ExampleStage.gltf", true);
}

bool TestScene::Uninitialize(ID3D11Device* device)
{
    SceneBase::Uninitialize(device);
    Physics::Instance().Finalize();
    return true;
}

void TestScene::DrawGui()
{
#ifdef USE_IMGUI
    SceneBase::DrawGui();
#endif

}