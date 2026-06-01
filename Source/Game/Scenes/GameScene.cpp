#include "pch.h"
#include "GameScene.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

#include "Components/Audio/CoreAudioSourceComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Core/ActorManager.h"
#include "Engine/Utility/Time.h"

#include "Game/Actors/Dessert/Pudding.h"
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
            //stageAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStage_0327_1/DarkStage.gltf",
            //stageAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStage0414/DarkStage.gltf",
            stageAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStage0601/DarkStage.gltf",
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
        });

    // ライトの方向と色を設定
    lightDirection = { -0.15f, -0.483f, 0.786f, 0.9f };   // 横の窓からの光
    lightDirection = { 0.03f, -0.15f, 0.23f, 0.9f };   // 横の窓からの光
    //lightDirection = { 0.382f, -0.882f, 0.112f, 0.0f };   // 上の窓からの光
    //lightDirection = { 0.545f, -0.86f, -0.526f, 0.0f };   // 上の窓からの光

    //lightDirection = { 0.9f, -0.64f, -0.058f, 0.9f };   // 上の窓からの光

    //lightDirection = { 1.0f, -1.0f, -0.008f, 0.9f };   // 上の窓からの光
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


    clothSimulate = std::make_unique<ClothSimulate>(device, "./Data/Models/Flag/Oden_Cloth_Noren_1.gltf");


    skyShaderConstantsBuffer = std::make_unique<ConstantBuffer<SkyShaderConstants>>(device);
    //HRESULT hr = CreatePsFromCSO(Graphics::GetDevice(), "./Shader/DarkStageSkyPS.cso", darkStageSkyPS.GetAddressOf());
    //HRESULT hr = CreatePsFromCSO(Graphics::GetDevice(), "./Shader/ShaderToySky2.cso", darkStageSkyPS.GetAddressOf());
    HRESULT hr = CreatePsFromCSO(Graphics::GetDevice(), "./Shader/ShaderToySkyPS.cso", darkStageSkyPS.GetAddressOf());
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
                    clothSimulate->Render(immediateContext, cloth->GetWorldTransform());
                }
            }
        });



    return true;
}

void GameScene::Start()
{
    auto audioActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<Actor>("Audio");
    auto audioComp = audioActor->AddComponent<CoreAudioSourceComponent>("audioSource");
    audioComp->SetSource(L"./Data/Sound/BGM/game.wav");
    audioComp->SetLoop(true);
    audioComp->Play();
    //audioComp->SetVolume(0.2f);
#if 0

    std::shared_ptr<UIImageComponent> image = std::make_shared<UIImageComponent>("./Data/Textures/UI/icon_chara.png", "image");
    image->SetWorldPosition({ 50, 50 });
    image->SetSize({ 200, 200 });

    uiManager->Add(image);


    std::shared_ptr<UIButtonComponent> button = std::make_shared<UIButtonComponent>("./Data/Textures/UI/icon_chara.png", "button");
    button->SetWorldPosition({ 300, 50 });
    button->SetSize({ 200, 80 });

    uiManager->Add(button);

    std::shared_ptr<UIGaugeComponent> gauge = std::make_shared<UIGaugeComponent>("./Data/Textures/UI/boss_hp_frame.png", "./Data/Textures/UI/boss_hp.png", "gauge");
    gauge->SetWorldPosition({ 50, 300 });
    gauge->SetSize({ 300, 40 });

    uiManager->Add(gauge);

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

    clothSimulate->Update(deltaTime);


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
        SceneTransitionManager::Instance().RequestTransition("SampleScene");

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
        Transform playerTr(DirectX::XMFLOAT3{ 0.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.07f,1.07f,1.07f });
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


    //Transform paintingTr(DirectX::XMFLOAT3{ -29.9f,2.8f,2.5f }, DirectX::XMFLOAT3{ 0.0f,90.0f,0.0f }, DirectX::XMFLOAT3{ 0.38f,0.38f,0.38f });
    //auto paintingActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStagePaintingActor>("painting", paintingTr);

#if 0
    Transform torchSconceTr(DirectX::XMFLOAT3{ -5.75f,1.9f,13.2f }, DirectX::XMFLOAT3{ 0.0f,-180.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto torchSconceActor = GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageTorchSconceActor>("TorchSconce", torchSconceTr);
#endif // 0


#if 0
    auto pauseActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<Pause>("pauseActor");
    pauseActor->SetRetrySceneName("SampleScene");
#endif // 0

#if 0
    {
        PROFILE_SCOPE("Create Enemy");
        Transform bossTr(DirectX::XMFLOAT3{ -10.7f,-0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 2.0f,2.0f,2.0f });
        auto boss = this->GetActorManager()->CreateAndRegisterActorWithTransform<BossEnemy>("boss", bossTr);
    }

#endif // 0
    Transform enemyTr(DirectX::XMFLOAT3{ -15.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto enemy = this->GetActorManager()->CreateAndRegisterActorWithTransform<SkeletonWarriorActor>("enemy", enemyTr);

    Transform KnightActorTR(DirectX::XMFLOAT3{ -15.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto KnightsActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<KnightActor>("KnightActor", KnightActorTR);

#if 1
    Transform GruxEnemyTr(DirectX::XMFLOAT3{ -18.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 2.0f,2.0f,2.0f });
    auto GruxEnemyActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<GruxEnemy>("GruxEnemy", GruxEnemyTr);

#endif // 0



    //Transform savarogEnemyTr(DirectX::XMFLOAT3{ -10.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    //auto savarogEnemy = this->GetActorManager()->CreateAndRegisterActorWithTransform<SavarogEnemy>("SavarogEnemy", savarogEnemyTr);

    //Transform gracialEnemyTr(DirectX::XMFLOAT3{ -10.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,10.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    //auto gracialEnemy = this->GetActorManager()->CreateAndRegisterActorWithTransform<GracialEnemy>("gracialEnemy", gracialEnemyTr);

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
        stage->SetModel(stageAsset, stageCandelabraAsset, stageBrazierAsset, stageGroundBrazierAsset, stageMeltedWaxAsset, stageStandingBrazierAsset);
    }

    for (auto point : stageAsset->spawnPoints)
    {
        if (point.name.rfind("Spawn_Door_Right", 0) == 0)
        {// 名前が "Spawn_Door_Right" で始まる場合、燭台を配置
            DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
            pos.x = -0.4f;
            Transform doorTr{ pos,point.worldRotation,point.worldScale };
            //Transform doorTr(DirectX::XMFLOAT3{ -6.0f,0.0f,11.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
            auto doorActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DoorLargeActor>("doorActor", doorTr);
        }
        else if (point.name.rfind("Spawn_SmallDoor", 0) == 0)
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
    ImGui::Begin("SkyShader");

    ImGui::ColorEdit3("topColor", &skyShaderConstantsBuffer->data.topColor.x);
    ImGui::ColorEdit3("bottomColor", &skyShaderConstantsBuffer->data.bottomColor.x);
    ImGui::ColorEdit3("sunColor", &skyShaderConstantsBuffer->data.sunColor.x);
    ImGui::ColorEdit3("cloudColor", &skyShaderConstantsBuffer->data.cloudColor.x);
    ImGui::DragFloat("cloudThreshold", &skyShaderConstantsBuffer->data.cloudThreshold, 0.01f);
    ImGui::DragFloat("sunSize", &skyShaderConstantsBuffer->data.sunSize, 0.1f);
    ImGui::DragFloat("cloudIntensity", &skyShaderConstantsBuffer->data.cloudIntensity, 0.1f);
    ImGui::DragFloat("scrollSpeed", &skyShaderConstantsBuffer->data.scrollSpeed, 0.01f);

    ImGui::DragFloat("starScale", &skyShaderConstantsBuffer->data.starScale, 0.01f);
    ImGui::DragFloat2("starOffset", &skyShaderConstantsBuffer->data.starOffset.x, 0.1f);
    ImGui::DragFloat("starIntensity", &skyShaderConstantsBuffer->data.starIntensity, 0.1f);

    ImGui::ColorEdit3("moonColor", &skyShaderConstantsBuffer->data.moonColor.x);
    ImGui::DragFloat("moonRadius", &skyShaderConstantsBuffer->data.moonRadius, 0.01f);

    ImGui::DragFloat2("moonPos", &skyShaderConstantsBuffer->data.moonPos.x, 0.1f);
    ImGui::DragFloat2("moonOffset", &skyShaderConstantsBuffer->data.moonOffset.x, 0.01f);

    ImGui::ColorEdit3("startAuroraColor", &skyShaderConstantsBuffer->data.startAuroraColor.x);
    ImGui::DragFloat("value", &skyShaderConstantsBuffer->data.value, 0.01f);

    ImGui::ColorEdit3("endAuroraColor", &skyShaderConstantsBuffer->data.endAuroraColor.x);
    ImGui::DragFloat("value1", &skyShaderConstantsBuffer->data.value1, 0.01f);

    ImGui::End();
#endif

}
