#include "pch.h"
#include "SampleScene.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "imgui.h"
#endif

#include "Game/Actors/Player/TestPlayer.h"
#include "Components/Audio/AudioSourceComponent.h"
#include "Graphics/Core/Graphics.h"
#include "Graphics/Core/RenderState.h"
#include "Engine/Input/InputSystem.h"
#include "Core/ActorManager.h"
#include "Engine/Utility/Time.h"

#include "Game/Actors/Camera/LoadingCamera.h"
#include "Game/Actors/Enemy/Boss/BossEnemy.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Actors/Stage/ElasticBuilding.h"
#include "Game/Actors/Stage/Cloth.h"


#include "Physics/Physics.h"
#include "Game/DarkGame/DarkActors/DarkStage.h"
#include "Game/DarkGame/DarkActors/DarkStageCandelabraActor.h"
#include "Game/DarkGame/DarkActors/DarkStageChandelierActor.h"

#include "Game/DarkGame/DarkActors/DarkEnemy/SkeletonWarriorEnemy.h"

#include "Physics/CollisionSystem.h"
#include "UI/UIManager.h"
#include "UI/Game/Pause.h"

bool SampleScene::Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
{
    SceneBase::Initialize(device, width, height, props);

    Physics::Instance().Initialize();

    //アクターをセット
    SetUpActors();

#if 0
    morphModel = std::make_unique<MorphModel>(device, "./Data/Models/Morph/morphSphere.gltf");

    RegisterRenderHook(RenderPass::Opaque, [&](ID3D11DeviceContext* immediateContext)
        {
            if (const auto cloth = GetActorManager()->GetActorByName("cloth"))
            {
                morphModel->Render(immediateContext, cloth->GetWorldTransform(), {}, MorphModel::RenderPass::All);
            }
        });
#endif // 0

    RegisterRenderHook(RenderPass::Opaque, [&](ID3D11DeviceContext* immediateContext)
        {
            if (const auto cloth = GetActorManager()->GetActorByName("pauseActor"))
            {
                //shapeMatchingModel->Render(immediateContext, cloth->GetWorldTransform(), {}, ShapeMatchingModel::RenderPass::Opaque);
            }
        });

    return true;
}

void SampleScene::Start()
{


}

void SampleScene::Update(float deltaTime)
{
    using namespace DirectX;
    SceneBase::Update(deltaTime);
    //shapeMatchingModel->Update(deltaTime);

    Physics::Instance().Update(Time::UnscaledDeltaTime());
    CollisionSystem::DetectAndResolveCollisions();
    CollisionSystem::ApplyPushAll();

    //#ifdef _DEBUG
    if (InputSystem::GetInputState("Space", InputStateMask::Trigger))
    {
        const char* types[] = { "0", "1" };
        Scene::_transition("LoadingScene", { std::make_pair("preload", "SampleScene"), std::make_pair("type", types[rand() % 2]) });
    }
    //#endif // !_DEBUG
}

void SampleScene::SetUpActors()
{
    auto mainCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<MainCamera>("mainCameraActor");
    auto mainCameraComponent = mainCameraActor->GetComponent<TPSCameraComponent>();

    Transform enemyTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,180.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto enemy = this->GetActorManager()->CreateAndRegisterActorWithTransform<Actor>("enemy", enemyTr);

    mainCameraActor->SetTarget(enemy->GetRootComponent());
    SetActiveCamera(mainCameraActor);
    Logger::Log(U8("morphシーンのカメラ設定される。"));

    Transform stageTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto stage = this->GetActorManager()->CreateAndRegisterActorWithTransform<Stage>("stage", stageTr);

    auto debugCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DebugCamera>("debugCam");
    debugCameraActor->SetPosition({ 0.0f,10.0f,-20.0f });

    //building->AddComponent<StaticMeshComponent>("cloth")->SetModel("./Data/Models/ClothFlag/pole.gltf");

    Transform buildTr2(DirectX::XMFLOAT3{ -3.0f,0.45f,3.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 0.8f,0.8f,0.8f });
    auto pauseActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<Pause>("pauseActor", buildTr2);

    Transform playerTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto player = this->GetActorManager()->CreateAndRegisterActorWithTransform<Player>("player", playerTr);

#if 1
    Transform testPlayerTr(DirectX::XMFLOAT3{ 3.0f,0.0f,0.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto testPlayer = this->GetActorManager()->CreateAndRegisterActorWithTransform<TestPlayer>("testPlayer", testPlayerTr);
#endif // 1

    std::shared_ptr<StageAsset> stageCandelabraAsset = std::make_shared<StageAsset>();
    stageCandelabraAsset->model = std::make_shared<InterleavedGltfModel>(Graphics::GetDevice(), "./Data/Models/DarkStageAssets/Candelabra/Candelabra.gltf", ModelTypes::ModelMode::StaticMesh, false, true);
    stageCandelabraAsset->spawnPoints = stageCandelabraAsset->model->spawnPoints;

    Transform chandelierTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,180.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto chandelier = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageCandelabraActor>("chandelier", chandelierTr);
    chandelier->SetModel(stageCandelabraAsset);


    //Transform chandelierTr(DirectX::XMFLOAT3{ 0.0f,3.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,180.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    //auto chandelier = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageChandelierActor>("chandelier", chandelierTr);

    cameraManager->SetDebugCamera(debugCameraActor);
}

bool SampleScene::Uninitialize(ID3D11Device* device)
{
    SceneBase::Uninitialize(device);
    Physics::Instance().Finalize();
    return true;
}

void SampleScene::DrawGui()
{
    SceneBase::DrawGui();
}
