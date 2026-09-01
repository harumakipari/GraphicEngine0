#include "pch.h"
#include "GameScene.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

#include <tracy/Tracy.hpp>


#include "Components/Audio/AudioSourceComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Core/ActorManager.h"
#include "Engine/Camera/MovieCameraManagerActor.h"
#include "Engine/Debug/SceneEditor.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Camera/DarkGameCamera.h"

#include "Game/Actors/Player/Player.h"
#include "Game/Actors/Stage/Cloth.h"


#include "Physics/Physics.h"
#include "Game/DarkGame/DarkActors/DarkStage.h"
#include "Game/DarkGame/DarkActors/DarkStageChandelierActor.h"
#include "Game/DarkGame/DarkActors/DoorActor.h"
#include "Game/DarkGame/DarkActors/IceFragmentEffectActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemyEyeActor.h"
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
            // メインの部屋のモデル
            mainRoomAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStage0620/MainRoom.gltf",
                ModelTypes::ModelMode::StaticMesh, false, true);
            mainRoomAsset->spawnPoints = mainRoomAsset->model->spawnPoints;

            // ボスの部屋のモデル
            bossRoomAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStage0620/BossRoom.gltf",
                ModelTypes::ModelMode::StaticMesh, false, true);
            bossRoomAsset->spawnPoints = bossRoomAsset->model->spawnPoints;

            // ボスとメインの部屋の間モデル
            transitionAreaAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStage0620/TransitionArea.gltf",
                ModelTypes::ModelMode::StaticMesh, false, true);
            transitionAreaAsset->spawnPoints = transitionAreaAsset->model->spawnPoints;
        });
    loadStageAssetsThread = std::thread([&]()
        {
            PROFILE_SCOPE("Load StageAssetModel");
            //stageCandelabraAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Candelabra/Candelabra.gltf", ModelTypes::ModelMode::StaticMesh, false, true);
            stageCandelabraAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Candelabra/Candelabra.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageCandelabraAsset->spawnPoints = stageCandelabraAsset->model->spawnPoints;

            stageBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Brazier/Brazier.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageBrazierAsset->spawnPoints = stageBrazierAsset->model->spawnPoints;

            stageGroundBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/GroundBrazier/groundBrazier.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageGroundBrazierAsset->spawnPoints = stageGroundBrazierAsset->model->spawnPoints;

            stageMeltedWaxAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/MeltedWax/MeltedWax.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageMeltedWaxAsset->spawnPoints = stageMeltedWaxAsset->model->spawnPoints;

            stageStandingBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/StandingBrazier/StandingBrazier.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageStandingBrazierAsset->spawnPoints = stageStandingBrazierAsset->model->spawnPoints;

            stageCandleStandAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/CandleStand/CandleStand.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
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

    // クロスシミュレーション
    clothSimulate = std::make_unique<ClothSimulate>(device, "./Data/Models/Cloth/ClothModel.gltf");


    // ここで布を描画する
    RegisterRenderHook(RenderPass::Mask, [&](ID3D11DeviceContext* immediateContext)
        {
#if 0
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
#else
            if (darkClothActor)
            {
                darkClothActor->RenderCloth(immediateContext);
            }
#endif // 0
        });

    // ボスの部屋のラープのための初期化処理
    bossLerpEasing = std::make_unique<EasingRunner>();

    // ここで軌跡を描画する
    RegisterRenderHook(RenderPass::ForwardBlend, [&](ID3D11DeviceContext* immediateContext)
        {
            RenderState::BindBlendState(immediateContext, BLEND_STATE::ADD);
            player->RenderTrail(immediateContext);
            if (gruxEnemyActor)
                gruxEnemyActor->RenderTrail(immediateContext);
        });


    return true;
}

void GameScene::Start()
{
    battleFlowState = BattleFlowState::Intro;
    battleStartTransformsSaved = false;
    SetBattleHudVisible(false);
    // ゲームBGM
    gameBgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("GameBgmActor");
    gameBgmActor->SetSource(L"./Data/Sound/BGM/game_bgm.wav");
    gameBgmActor->SetLoop(true);
    gameBgmActor->SetBgm(true);
    gameBgmActor->Play();
    gameBgmActor->SetVolume(0.3f);

    // ボスBGM
    bossBgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("BossBgmActor");
    bossBgmActor->SetSource(L"./Data/Sound/BGM/boss_bgm.wav");
    bossBgmActor->SetLoop(true);
    bossBgmActor->SetBgm(true);
    //bossBgmActor->Play();
    bossBgmActor->SetVolume(0.02f);

#if 0
    cameraManager->ToggleCinematicCamera(this);

    SceneEditor::LoadPresetList(); // 更新
    std::string file = "WindowWall1.json";
    static SceneState savedState;
    SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
    savedState.Apply(Scene::GetCurrentScene());
#else
    // シーンプリセットを設定する
    SceneEditor::LoadPresetList(); // 更新
    std::string file = "newPreset.json";
    static SceneState savedState;
    SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
    savedState.ApplyScenePreset(Scene::GetCurrentScene());
#endif // 0

    // カメラをplayerの前方向を向くように変更
    darkCameraActor->RotateToPlayerForward();

    player->SetEulerRotation({ 0.0f,90.0f,0.0f });

    // シーンが切り替わった時に
    SceneTransitionManager::Instance().NotifySceneChanged();

}

void GameScene::Update(float deltaTime)
{
    using namespace DirectX;

    ZoneScopedN("Game Update");

    UpdateBattleFlow();

    // ボスの部屋のラープのため
    if (bossLerpEasing)
    {
        bossLerpEasing->Tick(deltaTime);
        if (startBossRoomLerp)
        {
            auto& shader = Scene::GetCurrentScene()->GetSceneSettings().sceneShaderConstants;
            float lerpFactor = std::lerp(startBossRoomLerpFactor, endBossRoomLerpFactor, bossLerpEasingFactor);
            shader.bossRoomLerpFactor = lerpFactor;
        }
    }

    // ライトのビューの焦点をプレイヤー位置に設定する (シャドウマップ用)
    if (player)
    {
        SetLightViewFocus(player->GetPosition());
    }
    // シネマカメラだったらまたはムービーカメラだったらプレイヤーを透明化しない
    if (cameraManager->IsUseCinematic() || cameraManager->IsUseMovie())
    {
        player->SetIsPlayerTransparency(false);
        player->operateUiComponent->SetVisible(false);
    }
    else
    {
        player->SetIsPlayerTransparency(true);
        player->operateUiComponent->SetVisible(true);
    }




    SceneBase::Update(deltaTime);

    // Gameplay時間へ同期し、HitStop中はPhysXだけが進むずれを防ぐ。
    if (deltaTime > FLT_EPSILON)
        Physics::Instance().Update(deltaTime);
    {
        ZoneScopedN("Collision Resolution");
        CollisionSystem::DetectAndResolveCollisions();
        CollisionSystem::ApplyPushAll();
    }
    if (clothSimulate)
    {
        clothSimulate->Update(deltaTime);
    }

    if (InputSystem::GetInputState("LockOn", InputStateMask::Press))
    {// 押している間ロックオンする
        ChangeCameraMode(TPSCameraController::CameraMode::BossBattle);
    }
    else
    {
        ChangeCameraMode(TPSCameraController::CameraMode::TPS);
    }


#ifdef USE_IMGUI


    if (enableLightGui || enableSceneGui)
    {
        InputSystem::SetCursorVisible(true);
    }
#if 0
    if (InputSystem::GetInputState("0", InputStateMask::Trigger))
    {
        SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", "TitleScene") }, TransitionStyle::Fade);
    }

    if (InputSystem::GetInputState("2", InputStateMask::Trigger))
    {
        auto& shader = Scene::GetCurrentScene()->GetSceneSettings().sceneShaderConstants;
        shader.enableFog = !shader.enableFog;
    }
    if (InputSystem::GetInputState("3", InputStateMask::Trigger))
    {
        auto& shader = Scene::GetCurrentScene()->GetSceneSettings().sceneShaderConstants;
        shader.enableBloom = !shader.enableBloom;
    }
    if (InputSystem::GetInputState("4", InputStateMask::Trigger))
    {
        enableLightGui = !enableLightGui;
        InputSystem::SetCursorVisible(enableLightGui);

    }
    if (InputSystem::GetInputState("5", InputStateMask::Trigger))
    {
        auto& shader = Scene::GetCurrentScene()->GetSceneSettings().sceneShaderConstants;
        shader.enableDof = !shader.enableDof;

        //lightManager->showLightRange = !lightManager->showLightRange;
        //showDebugLight = !showDebugLight;
    }
    if (InputSystem::GetInputState("6", InputStateMask::Trigger))
    {
        enableSceneGui = !enableSceneGui;
    }

    if (InputSystem::GetInputState("7", InputStateMask::Trigger))
    {

        // シーンプリセットを設定する
        SceneEditor::LoadPresetList(); // 更新
        std::string file = "Unadjusted.json";
        static SceneState savedState;
        SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
        savedState.ApplyScenePreset(Scene::GetCurrentScene());

    }


    if (InputSystem::GetInputState("8", InputStateMask::Trigger))
    {
        // シーンプリセットを設定する
        SceneEditor::LoadPresetList(); // 更新
        std::string file = "bossRoom.json";
        static SceneState savedState;
        SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
        savedState.ApplyScenePreset(Scene::GetCurrentScene());

        // BGMも元に戻す
        auto bgmActors = GetActorManager()->GetActorsOfType <BgmActor>();
        for (auto bgmActor : bgmActors)
        {
            if (bgmActor->GetName() == "BossBgmActor")
            {
                bgmActor->Stop();
            }
            if (bgmActor->GetName() == "GameBgmActor")
            {
                bgmActor->Play();
            }
        }

        if (auto cameraManager = GetCameraManager())
        {
            if (cameraManager->IsUseCinematic())
            {// すでにシネマカメラが使用だったら、
                cameraManager->ToggleCinematicCamera(this);
            }
        }

        if (gruxEnemyActor)
        {
            gruxEnemyActor->GetStateMachine()->ChangeState("EnemyThinkState");
        }

    }


    if (InputSystem::GetInputState("9", InputStateMask::Trigger))
    {
        // シーンプリセットを設定する
        SceneEditor::LoadPresetList(); // 更新
        std::string file = "newPreset.json";
        static SceneState savedState;
        SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
        savedState.ApplyScenePreset(Scene::GetCurrentScene());

        // BGMも元に戻す
        auto bgmActors = GetActorManager()->GetActorsOfType <BgmActor>();
        for (auto bgmActor : bgmActors)
        {
            if (bgmActor->GetName() == "BossBgmActor")
            {
                bgmActor->Stop();
            }
            if (bgmActor->GetName() == "GameBgmActor")
            {
                bgmActor->Play();
            }
        }

        if (auto cameraManager = GetCameraManager())
        {
            if (cameraManager->IsUseCinematic())
            {// すでにシネマカメラが使用だったら、
                cameraManager->ToggleCinematicCamera(this);
            }
        }

        if (gruxEnemyActor)
        {
            gruxEnemyActor->GetStateMachine()->ChangeState("EnemyThinkState");
        }

    }

    if (InputSystem::GetInputState("F2", InputStateMask::Trigger))
    {
        if (!cameraManager->IsUseCinematic())
        {// すでにシネマカメラが使用中でない場合のみ切り替え
            cameraManager->ToggleCinematicCamera(this);
        }

        // シーンプリセットを設定する
        SceneEditor::LoadPresetList(); // 更新
        std::string file = "0.json";
        static SceneState savedState;
        SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
        savedState.Apply(Scene::GetCurrentScene());

        // カメラを固定する
        cinemaCameraActor->SetUseDebugMode(false);
        cinemaCameraActor->SetEulerRotation({ 6.96f,-148.175f,11.522f });

    }
    // シーン変更
    if (InputSystem::GetInputState("F3", InputStateMask::Trigger))
    {
        if (!cameraManager->IsUseCinematic())
        {// すでにシネマカメラが使用中でない場合のみ切り替え
            cameraManager->ToggleCinematicCamera(this);
        }

        // シーンプリセットを設定する
        SceneEditor::LoadPresetList(); // 更新
        std::string file = "HallwayPlayerUpScene.json";
        static SceneState savedState;
        SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
        savedState.Apply(Scene::GetCurrentScene());
        // カメラを固定する
        cinemaCameraActor->SetUseDebugMode(false);


    }
    if (InputSystem::GetInputState("F4", InputStateMask::Trigger))
    {
        if (!cameraManager->IsUseCinematic())
        {// すでにシネマカメラが使用中でない場合のみ切り替え
            cameraManager->ToggleCinematicCamera(this);
        }

        // シーンプリセットを設定する
        SceneEditor::LoadPresetList(); // 更新
        std::string file = "title2.json";
        static SceneState savedState;
        SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
        savedState.Apply(Scene::GetCurrentScene());
        // カメラを固定する
        cinemaCameraActor->SetUseDebugMode(false);
    }
#endif // 0


    if (InputSystem::GetInputState("F9", InputStateMask::Trigger))
    {
        // シーンプリセットを設定する
        SceneEditor::LoadPresetList(); // 更新
        std::string file = "bossinroom.json";
        static SceneState savedState;
        SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
        savedState.ApplyScenePreset(Scene::GetCurrentScene());

        // BGMも元に戻す
        auto bgmActors = GetActorManager()->GetActorsOfType <BgmActor>();
        for (auto bgmActor : bgmActors)
        {
            if (bgmActor->GetName() == "BossBgmActor")
            {
                bgmActor->Play();
            }
            if (bgmActor->GetName() == "GameBgmActor")
            {
                bgmActor->Stop();
            }
        }

        if (auto cameraManager = GetCameraManager())
        {
            if (cameraManager->IsUseCinematic())
            {// すでにシネマカメラが使用だったら、
                cameraManager->ToggleCinematicCamera(this);
            }
        }

        if (gruxEnemyActor)
        {
            gruxEnemyActor->GetStateMachine()->ChangeState("EnemyIdleState");
        }
        player->SetIsBossBattle(true);
        StartBossBattle();
    }



#endif // !USE_IMGUI


}

// 定数バッファの更新処理をシーンごとにカスタマイズできるようにするための仮想関数
void GameScene::SetBattleHudVisible(const bool visible)
{
    if (player)
        player->SetHpBarVisible(visible);
    if (gruxEnemyActor)
        gruxEnemyActor->SetHpBarVisible(visible);
}

void GameScene::StartBossBattle()
{
    if (!player || !gruxEnemyActor)
        return;

    if (player->GetRootComponent() && gruxEnemyActor->GetRootComponent())
    {
        playerBattleStartTransform = player->GetRootComponent()->GetComponentWorldTransform();
        bossBattleStartTransform = gruxEnemyActor->GetRootComponent()->GetComponentWorldTransform();
        battleStartTransformsSaved = true;
    }

    player->SetIsBossBattle(true);
    battleFlowState = BattleFlowState::Playing;
    SetBattleHudVisible(true);
}

void GameScene::EnterPlayerDead()
{
    battleFlowState = BattleFlowState::PlayerDead;
    playerDeadElapsed = 0.0f;
    SetBattleHudVisible(false);

    if (gruxEnemyActor)
    {
        gruxEnemyActor->StopBattleActions();
        gruxEnemyActor->SetTimeScale(0.0f);
    }
}

void GameScene::ResetBattleForContinue()
{
    if (!battleStartTransformsSaved || !player || !gruxEnemyActor)
        return;

    Time::SetSlow(1.0f, 0.0f);

    if (cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(this);
    if (darkCameraActor)
        darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);

    player->ResetForBattleContinue(playerBattleStartTransform);
    gruxEnemyActor->ResetForBattleContinue(bossBattleStartTransform);

    if (darkCameraActor)
        darkCameraActor->RotateToPlayerForward();

    SetBattleHudVisible(true);
    battleFlowState = BattleFlowState::Playing;
}

void GameScene::EnterBossDead()
{
    battleFlowState = BattleFlowState::BossDead;
    SetBattleHudVisible(false);

    if (player)
        player->StopBattleActions();
    if (gruxEnemyActor)
        gruxEnemyActor->StopBattleActions();
}

void GameScene::UpdateBattleFlow()
{
    switch (battleFlowState)
    {
    case BattleFlowState::Intro:
        break;
    case BattleFlowState::Playing:
        if (gruxEnemyActor && gruxEnemyActor->GetHp() <= 0)
        {
            EnterBossDead();
        }
        else if (player && player->GetHp() <= 0)
        {
            EnterPlayerDead();
        }
        break;
    case BattleFlowState::PlayerDead:
        playerDeadElapsed += Time::UnscaledDeltaTime();
        if (playerDeadElapsed >= continueWaitDelay)
            battleFlowState = BattleFlowState::ContinueWait;
        break;
    case BattleFlowState::ContinueWait:
        if (InputSystem::GetInputState("GamePadA", InputStateMask::Trigger))
            battleFlowState = BattleFlowState::ResetForContinue;
        break;
    case BattleFlowState::ResetForContinue:
        ResetBattleForContinue();
        break;
    case BattleFlowState::BossDead:
        if (gruxEnemyActor && gruxEnemyActor->GetStateMachine() &&
            std::string(gruxEnemyActor->GetStateMachine()->GetStateName()) == "EnemyDeathState")
        {
            battleFlowState = BattleFlowState::Victory;
        }
        break;
    case BattleFlowState::Victory:
        // タイトル画面に戻る
        if (InputSystem::GetInputState("GamePadA", InputStateMask::Trigger))
        {
            SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", "TitleScene") }, TransitionStyle::Fade);
        }
        break;
    }
}
void GameScene::UpdateConstants(ID3D11DeviceContext* immediateContext, float deltaTime)
{
}

void GameScene::SetUpActors()
{
    {
        PROFILE_SCOPE("Create Player");
        Transform playerTr(DirectX::XMFLOAT3{ -13.537f,0.0f,10.757f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.07f,1.07f,1.07f });
        player = this->GetActorManager()->CreateAndRegisterActorWithTransform<Player>("Player", playerTr);
    }

    Transform debugCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto debugCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DebugCamera>("debugCam", debugCameraTr);
    cameraManager->SetDebugCamera(debugCameraActor);

    Transform cinemaCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    cinemaCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<CinemaCamera>("cinemaCam", cinemaCameraTr);
    cameraManager->SetCinematicCamera(cinemaCameraActor);

    Transform movieCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto movieCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<MovieCamera>("movieCam", movieCameraTr);
    cameraManager->SetMovieCamera(movieCameraActor);

    // MovieCameraManagerActor を作成し、MovieCameraComponent を設定
    auto movieCameraManagerActor = GetActorManager()->CreateAndRegisterActorWithTransform<MovieCameraManagerActor>("movieCameraManager", movieCameraTr);
    movieCameraManagerActor->SetMovieCameraComponent(movieCameraActor->GetMovieCameraComponent());

    Transform clothTr(DirectX::XMFLOAT3{ -13.537f,0.0f,10.757f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    darkClothActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr);


    Transform bossEyeTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto enemyEyeActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<GruxEnemyEyeActor>("GruxEnemyEyeActor", bossEyeTr);

    Transform iceTr(DirectX::XMFLOAT3{ -13.537f,0.0f,10.757f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto iceActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<IceFragmentEmitterActor>("IceFragmentEmitterActor", iceTr);


#if 0
    auto pauseActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<Pause>("pauseActor");
    pauseActor->SetRetrySceneName("SampleScene");
#endif // 0


    Transform GruxEnemyTr(DirectX::XMFLOAT3{ 7.69f,0.0f,11.0f }, DirectX::XMFLOAT3{ 0.0f,-90.0f,0.0f }, DirectX::XMFLOAT3{ 1.7f,1.7f,1.7f });
    gruxEnemyActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<GruxEnemy>("GruxEnemy", GruxEnemyTr);

    Transform darkCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    darkCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkCameraActor>("darkCameraActor", darkCameraTr);
    SetActiveCamera(darkCameraActor);
    float fovY = DirectX::XMConvertToRadians(35.0f);
    float aspect = 1280.f / 720.f;
    float nearZ = 0.1f;
    float farZ = 1000.f;
    darkCameraActor->SetPerspective(fovY, aspect, nearZ, farZ);
    darkCameraActor->InitSetYawAndPitch(DirectX::XMConvertToRadians(-20.0f), DirectX::XMConvertToRadians(0.0f));
    darkCameraActor->SetPlayerHead(player->GetCameraTargetComponent());
    darkCameraActor->SetEnemyHead(gruxEnemyActor->GetCameraTargetComponent());


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
        stage->SetModel(mainRoomAsset, transitionAreaAsset, bossRoomAsset, stageCandelabraAsset, stageBrazierAsset, stageGroundBrazierAsset, stageMeltedWaxAsset, stageStandingBrazierAsset, stageCandleStandAsset);
    }

    Transform doorTr(DirectX::XMFLOAT3{ -6.0f,0.0f,11.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto doorActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DoorLargeActor>("doorActor", doorTr);

    for (const auto& areaAsset : { mainRoomAsset, transitionAreaAsset, bossRoomAsset })
    {
        for (const auto& point : areaAsset->spawnPoints)
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

}

bool GameScene::Uninitialize(ID3D11Device* device)
{
    SceneBase::Uninitialize(device);
    Physics::Instance().Finalize();
    return true;
}

void GameScene::DrawGuiPlusAlpha()
{
#ifdef USE_IMGUI
    ImGui::Begin("GameScene");
    if (ImGui::Button(U8("ボスの部屋を明るくする")))
    {
        StartBossRoomLerp(0.0f, 1.0f, 3.0f);
    }
    if (ImGui::Button(U8("ボスの部屋を暗くする")))
    {
        StartBossRoomLerp(1.0f, 0.0f, 3.0f);
    }
    if (ImGui::Button(U8("Bossカメラ")))
    {
        ChangeCameraMode(TPSCameraController::CameraMode::BossBattle);
    }
    if (ImGui::Button(U8("TPSカメラ")))
    {
        ChangeCameraMode(TPSCameraController::CameraMode::TPS);
    }
    ImGui::End();
#endif

}


// ボスの部屋の色ラープ値を設定する
void GameScene::SetBossRoomLerpFactor(float lerpFactor)
{
    auto& shader = Scene::GetCurrentScene()->GetSceneSettings().sceneShaderConstants;
    shader.bossRoomLerpFactor = lerpFactor;
}

// ボスの目のみBloomをオンにする
void GameScene::SetEyeBloom(bool enable)
{
    auto& shader = Scene::GetCurrentScene()->GetSceneSettings().sceneShaderConstants;
    if (enable)
    {// オンにする
        shader.enableEyeBloom = true;
        shader.enableBloom = false;
    }
    else
    {// オフにする
        shader.enableEyeBloom = false;
        shader.enableBloom = true;
    }

}

// ボスの部屋の色のラープを開始する関数
void GameScene::StartBossRoomLerp(float startFactor, float endFactor, float duration, std::function<void()> finished)
{
    startBossRoomLerpFactor = startFactor;
    endBossRoomLerpFactor = endFactor;
    this->onFinished = finished;
    startBossRoomLerp = true;
    // ボスの部屋の色のラープを開始する
    {
        TestEasingHandler handler;

        handler.AddEasing(
            TestEaseType::InSine,
            0.0f,
            1.0f,
            duration
        );

        handler.SetCompletedFunction([this]()
            {
                startBossRoomLerp = false;
                if (onFinished)
                {
                    onFinished();
                }
                onFinished = nullptr;
                SetBossRoomLerpFactor(endBossRoomLerpFactor);
            });
        PropertyAccessor<float> accessor;

        accessor.getter = [this]() { return bossLerpEasingFactor; };
        accessor.setter = [this](float t)
            {
                bossLerpEasingFactor = t;
            };

        bossLerpEasing->StartHandler(handler, accessor);
    }
}

// カメラのモードを変更する
void GameScene::ChangeCameraMode(TPSCameraController::CameraMode cameraMode)
{
    switch (cameraMode)
    {
    case TPSCameraController::CameraMode::TPS:
        if (mainCameraActor)
        {
            mainCameraActor->SetLookTarget(player->GetCameraTargetComponent());
            mainCameraActor->SetCameraMode(TPSCameraController::CameraMode::TPS);
        }
        break;
    case TPSCameraController::CameraMode::BossBattle:
        // ボス戦
        if (mainCameraActor)
        {
            mainCameraActor->SetEye(player->GetCameraEyeComponent());
            mainCameraActor->SetLookTarget(gruxEnemyActor->GetCameraTargetComponent());
            mainCameraActor->SetCameraMode(TPSCameraController::CameraMode::BossBattle);
        }
        break;
    }
}
