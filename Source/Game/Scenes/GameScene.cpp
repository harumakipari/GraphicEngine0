#include "pch.h"
#include "GameScene.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

#include "Components/Audio/AudioSourceComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Core/ActorManager.h"
#include "Engine/Debug/SceneEditor.h"
#include "Engine/Utility/Time.h"

#include "Game/Actors/Enemy/Boss/BossEnemy.h"
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

bool GameScene::Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
{
    PROFILE_FUNCTION();

    loadStageThread = std::thread([&]()
        {
            PROFILE_SCOPE("Load StageModel");
                stageAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStage0620/DarkStage.gltf",
                ModelTypes::ModelMode::StaticMesh, false, true);
            stageAsset->spawnPoints = stageAsset->model->spawnPoints;
        });
    loadStageAssetsThread = std::thread([&]()
        {
            PROFILE_SCOPE("Load StageAssetModel");
            stageCandelabraAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Candelabra/Candelabra.gltf", ModelTypes::ModelMode::StaticMesh, false, true);
            stageCandelabraAsset->spawnPoints = stageCandelabraAsset->model->spawnPoints;

            stageBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Brazier/Brazier.gltf", ModelTypes::ModelMode::StaticMesh, false, true);
            stageBrazierAsset->spawnPoints = stageBrazierAsset->model->spawnPoints;

            stageGroundBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/GroundBrazier/groundBrazier.gltf", ModelTypes::ModelMode::StaticMesh, false, true);
            stageGroundBrazierAsset->spawnPoints = stageGroundBrazierAsset->model->spawnPoints;

            stageMeltedWaxAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/MeltedWax/MeltedWax.gltf", ModelTypes::ModelMode::StaticMesh, false, true);
            stageMeltedWaxAsset->spawnPoints = stageMeltedWaxAsset->model->spawnPoints;

            stageStandingBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/StandingBrazier/StandingBrazier.gltf", ModelTypes::ModelMode::StaticMesh, false, true);
            stageStandingBrazierAsset->spawnPoints = stageStandingBrazierAsset->model->spawnPoints;

            stageCandleStandAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/CandleStand/CandleStand.gltf", ModelTypes::ModelMode::StaticMesh, false, true);
            stageCandleStandAsset->spawnPoints = stageCandleStandAsset->model->spawnPoints;

        });

    // ライトの方向と色を設定
    lightDirection = { 0.722f, -0.38f, -0.0211f, 0.9f };   // 上の窓からの光
    lightColor = { 1.0f, 0.8f, 1.0f, 2.6f };
    {
        //PROFILE_SCOPE("SceneBase Init");
        SceneBase::Initialize(device, width, height, props);
    }
    {
        //PROFILE_SCOPE("Physics Init");
        Physics::Instance().Initialize();
    }
    {
        PROFILE_SCOPE("SetUpActors Init");
        //アクターをセット
        SetUpActors();
    }



    //clothSimulate = std::make_unique<ClothSimulate>(device, "./Data/Models/Flag/Oden_Cloth_Noren_1.gltf");


    skyShaderConstantsBuffer = std::make_unique<ConstantBuffer<SkyShaderConstants>>(device);
    //HRESULT hr = CreatePsFromCSO(Graphics::GetDevice(), "./Shader/DarkStageSkyPS.cso", darkStageSkyPS.GetAddressOf());
    //HRESULT hr = CreatePsFromCSO(Graphics::GetDevice(), "./Shader/ShaderToySky2.cso", darkStageSkyPS.GetAddressOf());
    HRESULT hr = CreatePsFromCSO(Graphics::GetDevice(), "./Data/Shaders/ShaderToySkyPS.cso", darkStageSkyPS.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    RegisterRenderHook(RenderPass::Sky, [&](ID3D11DeviceContext* immediateContext)
        {
            ID3D11ShaderResourceView* shaderResourceViews[]
            {
                nullptr
            };
            fullscreenQuad->Blit(immediateContext, shaderResourceViews, 0, 1, darkStageSkyPS.Get());
        });

    // ここで布を描画する
    RegisterRenderHook(RenderPass::Mask, [&](ID3D11DeviceContext* immediateContext)
        {
            for (int i = 0; i < 5; i++)
            {
                if (const auto cloth = GetActorManager()->GetActorByName("cloth"))
                {
                    if (clothSimulate)
                    {
                        clothSimulate->Render(immediateContext, cloth->GetWorldTransform());
                    }
                }
            }
        });



    return true;
}

void GameScene::Start()
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

void GameScene::Update(float deltaTime)
{
    using namespace DirectX;
    SceneBase::Update(deltaTime);

    Physics::Instance().Update(Time::UnscaledDeltaTime());
    CollisionSystem::DetectAndResolveCollisions();
    CollisionSystem::ApplyPushAll();
    if (clothSimulate)
    {
        clothSimulate->Update(deltaTime);
    }

#if 0
    if (player && mainCameraComponent)
    {
        float followSpeed = 6.0f;
        const auto& forward = player->GetForward();
        float playerYaw = std::atan2f(forward.x, forward.z);
        //mainCameraComponent->yaw = (playerYaw);

        float delta = playerYaw - mainCameraComponent->yaw;
        delta = std::atan2f(std::sinf(delta), std::cosf(delta)); // -3.14 ~ 3.14
        mainCameraComponent->yaw += delta * followSpeed * deltaTime;

    }
#endif // 0

    //#ifdef _DEBUG
    if (InputSystem::GetInputState("Space", InputStateMask::Trigger))
    {
        const char* types[] = { "0", "1" };
        SceneTransitionManager::Instance().RequestTransition("GameScene");

        //Scene::_transition("LoadingScene", { std::make_pair("preload", "PuddingGameScene"), std::make_pair("type", types[rand() % 2]) });
    }
    //#endif // !_DEBUG
}

// 定数バッファの更新処理をシーンごとにカスタマイズできるようにするための仮想関数
void GameScene::UpdateConstants(ID3D11DeviceContext* immediateContext, float deltaTime)
{
    skyShaderConstantsBuffer->Activate(immediateContext, 12);
}

void GameScene::SetUpActors()
{
    Transform mainCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto mainCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<MainCamera>("mainCameraActor", mainCameraTr);
    mainCameraComponent = mainCameraActor->GetComponent<TPSCameraComponent>();
    {
        PROFILE_SCOPE("Create Player");
        Transform playerTr(DirectX::XMFLOAT3{ -15.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.07f,1.07f,1.07f });
        //Transform playerTr(DirectX::XMFLOAT3{ 0.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.07f,1.07f,1.07f });
        player = this->GetActorManager()->CreateAndRegisterActorWithTransform<Player>("player", playerTr);
        mainCameraActor->SetTarget(player->GetRootComponent());
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


    Transform clothTr(DirectX::XMFLOAT3{ 0.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto cloth = this->GetActorManager()->CreateAndRegisterActorWithTransform<Actor>("cloth", clothTr);


#if 0
    auto pauseActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<Pause>("pauseActor");
    pauseActor->SetRetrySceneName("SampleScene");
#endif // 0


#if 0
    Transform enemyTr(DirectX::XMFLOAT3{ -15.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto enemy = this->GetActorManager()->CreateAndRegisterActorWithTransform<SkeletonWarriorActor>("enemy", enemyTr);

    Transform KnightActorTR(DirectX::XMFLOAT3{ -15.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto KnightsActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<KnightActor>("KnightActor", KnightActorTR);
#endif // 0

    Transform GruxEnemyTr(DirectX::XMFLOAT3{ 7.69f,0.0f,11.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.3f,1.3f,1.3f });
    auto GruxEnemyActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<GruxEnemy>("GruxEnemy", GruxEnemyTr);

#if 0
    Transform dustParticleTr(DirectX::XMFLOAT3{ -27.0f,0.0f,11.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto dustParticleActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<Actor>("dustParticle", dustParticleTr);
    auto dustParticle = dustParticleActor->AddComponent<ParticleComponent>("dustComponent");
    dustParticle->Load("./Data/Effect/Files/DustEffect.json");
    ParticleComponent::AddSettings settings
    {
        .loop = true, // ループ再生
    };
    dustParticle->SetAddSettings(settings);
    dustParticle->Play();
#endif // 0

    loadStageThread.join();
    loadStageAssetsThread.join();
    {
        PROFILE_SCOPE("Create Stage");
        Transform stageTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        auto stage = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStage>("stage", stageTr); // 元のモデルの scale を 0.4f
        stage->SetModel(stageAsset, stageCandelabraAsset, stageBrazierAsset, stageGroundBrazierAsset, stageMeltedWaxAsset, stageStandingBrazierAsset,stageCandleStandAsset);
    }

    Transform doorTr(DirectX::XMFLOAT3{ -6.0f,0.0f,11.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto doorActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DoorLargeActor>("doorActor", doorTr);


    for (auto point : stageAsset->spawnPoints)
    {
#if 0
        if (point.name.rfind("Spawn_Door_Right", 0) == 0)
        {// 名前が "Spawn_Door_Right" で始まる場合、燭台を配置
            DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
            pos.x = -0.4f;
            //Transform doorTr{ pos,point.worldRotation,point.worldScale };
            Transform doorTr(DirectX::XMFLOAT3{ -6.0f,0.0f,11.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
            auto doorActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DoorLargeActor>("doorActor", doorTr);
        }

#endif // 0
        if (point.name.rfind("Spawn_SmallDoor", 0) == 0)
        {
            DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
            Transform smallDoorTr{ pos,point.worldRotation,point.worldScale };
            auto smallDoorActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DoorSmallActor>("smallDoorActor", smallDoorTr);
        }
    }

}

bool GameScene::Uninitialize(ID3D11Device* device)
{
    SceneBase::Uninitialize(device);
    Physics::Instance().Finalize();
    return true;
}

void GameScene::DrawGui()
{
#ifdef USE_IMGUI
    SceneBase::DrawGui();
#endif

}
