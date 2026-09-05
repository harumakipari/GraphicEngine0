#include "pch.h"
#include "GameScene.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

#include <tracy/Tracy.hpp>


#include "Components/Audio/AudioSourceComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Audio/Audio.h"
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
#include "Game/DarkGame/DarkActors/ModelDebrisEmitterActor.h"
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
        CreateDeathResultUI();
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
    UpdateDeathResultUILayout();

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
        player->operateUiComponent->SetVisible(battleFlowState == BattleFlowState::Playing);
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
    battleElapsedTime = 0.0f;
    battleFlowState = BattleFlowState::Playing;
    SetBattleHudVisible(true);
    player->SetGameplayHudVisible(true);
    // 入力を受け付ける
    InputSystem::SetInputEnabled(true);
}

void GameScene::EnterPlayerDead()
{
    battleFlowState = BattleFlowState::PlayerDead;
    playerDeadElapsed = 0.0f;
    deathPresentationElapsed = 0.0f;
    deathAttemptTime = battleElapsedTime;
    deathResultVisible = false;
    deathResultInputEnabled = false;
    deathResultSelection = 0;
    ResetDeathPresentationCues();
    SetDeathResultVisible(false);
    InputSystem::SetInputEnabled(false);
    deathCameraStartRequested = false;
    if (player)
        player->BeginGameplayHudFadeOut();
    if (gruxEnemyActor)
        gruxEnemyActor->BeginHpBarFadeOut();
    FireDeathPresentationCue(DeathPresentationCue::GameplayHudFade, deathHudFadeCueFired);

    if (gruxEnemyActor)
        gruxEnemyActor->PauseBattleAI();

    // プレイヤーが死亡した時のプレイヤーとボスの位置を調整
    StageDeathActors();

    if (darkCameraActor && player)
    {
        player->SetDeathCameraTransparencyDisabled(true);
        player->SetDeathCameraStartCallback([this]()
            {
                OnPlayerDeathCameraStart();
            });
        if (auto stateMachine = player->GetStateMachine())
            stateMachine->ChangeState("Death");
    }
    else if (player && player->GetStateMachine())
    {
        //カメラがない場合は、プレーヤーの死亡ステートに遷移
        player->SetDeathCameraTransparencyDisabled(true);
        player->GetStateMachine()->ChangeState("Death");
    }
}

GameScene::DeathStagingArea GameScene::DetermineDeathStagingArea(
    const DirectX::XMFLOAT3& originalPlayerPosition) const
{
    const bool front = originalPlayerPosition.x < deathStagingMinPlayerX;
    const bool back = originalPlayerPosition.x > deathStagingMaxPlayerX;
    const bool right = originalPlayerPosition.z < deathStagingMinPlayerZ;
    const bool left = originalPlayerPosition.z > deathStagingMaxPlayerZ;

    if (front && left) return DeathStagingArea::FrontLeft;
    if (front && right) return DeathStagingArea::FrontRight;
    if (back && left) return DeathStagingArea::BackLeft;
    if (back && right) return DeathStagingArea::BackRight;
    if (front) return DeathStagingArea::Front;
    if (back) return DeathStagingArea::Back;
    if (left) return DeathStagingArea::Left;
    if (right) return DeathStagingArea::Right;
    return DeathStagingArea::Center;
}

void GameScene::StageDeathActors()
{
    if (!player || !gruxEnemyActor)
        return;

    const DirectX::XMFLOAT3 originalPlayerPosition = player->GetPosition();
    const DeathStagingArea stagingArea = DetermineDeathStagingArea(originalPlayerPosition);
    const float targetBossDistance = deathStagingAreaSettings[static_cast<size_t>(stagingArea)].bossDistance;

    float safeMinX = deathStagingMinPlayerX;
    float safeMaxX = deathStagingMaxPlayerX;
    float safeMinZ = deathStagingMinPlayerZ;
    float safeMaxZ = deathStagingMaxPlayerZ;
    switch (stagingArea)
    {
    case DeathStagingArea::FrontLeft:
        safeMinX += deathStagingCornerInsetX;
        safeMaxZ -= deathStagingCornerInsetZ;
        break;
    case DeathStagingArea::FrontRight:
        safeMinX += deathStagingCornerInsetX;
        safeMinZ += deathStagingCornerInsetZ;
        break;
    case DeathStagingArea::BackLeft:
        safeMaxX -= deathStagingCornerInsetX;
        safeMaxZ -= deathStagingCornerInsetZ;
        break;
    case DeathStagingArea::BackRight:
        safeMaxX -= deathStagingCornerInsetX;
        safeMinZ += deathStagingCornerInsetZ;
        break;
    default:
        break;
    }

    DirectX::XMFLOAT3 safePlayerPosition = originalPlayerPosition;
    safePlayerPosition.x = std::clamp(safePlayerPosition.x, safeMinX, safeMaxX);
    safePlayerPosition.z = std::clamp(safePlayerPosition.z, safeMinZ, safeMaxZ);

    const DirectX::XMFLOAT3 stagingOffset{
        safePlayerPosition.x - originalPlayerPosition.x,
        0.0f,
        safePlayerPosition.z - originalPlayerPosition.z };
    const DirectX::XMFLOAT3 originalBossPosition = gruxEnemyActor->GetPosition();
    DirectX::XMFLOAT3 stagedBossPosition{
        originalBossPosition.x + stagingOffset.x,
        originalBossPosition.y,
        originalBossPosition.z + stagingOffset.z };

    if (std::abs(stagingOffset.x) > FLT_EPSILON ||
        std::abs(stagingOffset.z) > FLT_EPSILON)
    {
        player->SetPosition(safePlayerPosition);
    }

    DirectX::XMFLOAT3 playerToBoss = MathHelper::Subtract(
        stagedBossPosition, safePlayerPosition);
    playerToBoss.y = 0.0f;
    DirectX::XMFLOAT3 direction = playerToBoss;
    if (MathHelper::Length(direction) > FLT_EPSILON)
    {
        direction = MathHelper::Normalize(direction);
    }
    else
    {
        direction = player->GetForward();
        direction.y = 0.0f;
        if (MathHelper::Length(direction) <= FLT_EPSILON)
            direction = { 0.0f, 0.0f, 1.0f };
        else
            direction = MathHelper::Normalize(direction);
    }

    stagedBossPosition.x = safePlayerPosition.x + direction.x * targetBossDistance;
    stagedBossPosition.z = safePlayerPosition.z + direction.z * targetBossDistance;
    if (std::abs(stagedBossPosition.x - originalBossPosition.x) > FLT_EPSILON ||
        std::abs(stagedBossPosition.z - originalBossPosition.z) > FLT_EPSILON)
    {
        gruxEnemyActor->SetPosition(stagedBossPosition);
    }

    DirectX::XMFLOAT3 bossToPlayer = MathHelper::Subtract(
        safePlayerPosition, stagedBossPosition);
    bossToPlayer.y = 0.0f;
    if (MathHelper::Length(bossToPlayer) <= FLT_EPSILON)
    {
        bossToPlayer = player->GetForward();
        bossToPlayer.x *= -1.0f;
        bossToPlayer.y = 0.0f;
        bossToPlayer.z *= -1.0f;
    }
    gruxEnemyActor->SetDirectionImmediate(bossToPlayer);
}

void GameScene::CreateDeathResultUI()
{
    auto uiManager = GetUIManager();
    deathResultDarkOverlay = std::make_shared<UIImageComponent>("DeathResultDarkOverlay");
    deathResultDarkOverlay->SetSize({ 1920.0f, 1080.0f });
    deathResultDarkOverlay->SetWorldPosition({ 0.0f, 0.0f });
    deathResultDarkOverlay->SetPivot({ 0.0f, 0.0f });
    deathResultDarkOverlay->SetColor(CoreColor{ 0.0f, 0.0f, 0.0f, 0.0f });
    deathResultDarkOverlay->zOrder = 20;
    uiManager->Add(deathResultDarkOverlay);

    deathResultDefeated = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/Defeat.png", "DeathResultDefeated");
    deathResultDefeated->SetSize({ 1920.0f, 610.0f });
    deathResultDefeated->SetWorldPosition(deathResultDefeatedPosition);
    deathResultDefeated->SetPivot({ 0.5f, 0.5f });
    deathResultDefeated->zOrder = 21;
    uiManager->Add(deathResultDefeated);

    deathResultBattleTime = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/battle_time.png", "DeathResultBattleTime");
    deathResultBattleTime->SetWorldPosition(deathResultBattleTimePosition);
    deathResultBattleTime->SetPivot({ 0.5f, 0.5f });
    deathResultBattleTime->SetSize({ 600.0f, 175.0f });
    deathResultBattleTime->SetScale({ 0.4f,0.4f });
    deathResultBattleTime->zOrder = 21;
    uiManager->Add(deathResultBattleTime);

    const int timeDigits[] = { 0, 1, -1, 4, 2, -2, 3, 6 };
    for (int i = 0; i < 8; ++i)
    {
        const bool colon = timeDigits[i] == -1;
        const bool dot = timeDigits[i] == -2;
        const std::string name = "DeathResultTime" + std::to_string(i);
        const std::string path = colon ? "./Data/Textures/UI/timer_colon.png" :
            (dot ? "./Data/Textures/UI/timer_dot.png" : "./Data/Textures/UI/number.png");
        deathResultTimeDigits[i] = std::make_shared<UIImageComponent>(path, name);
        deathResultTimeDigits[i]->SetSize(colon || dot ? DirectX::XMFLOAT2{ 48.0f, 128.0f } : DirectX::XMFLOAT2{ 96.0f, 128.0f });
        deathResultTimeDigits[i]->SetPivot({ 0.5f, 0.5f });
        deathResultTimeDigits[i]->SetScale(deathResultTimeNumberScale);
        deathResultTimeDigits[i]->SetVisible(false);
        deathResultTimeDigits[i]->zOrder = 21;
        uiManager->Add(deathResultTimeDigits[i]);
    }

    UpdateDeathResultBattleTimeValue();
    const char* paths[] = { "continue_button.png", "restart_battle_button.png", "return_title_button.png" };
    const char* names[] = { "DeathResultContinue", "DeathResultRestart", "DeathResultTitle" };
    const float buttonWidths[] = { 448.0f, 667.0f, 730.0f };
    const float buttonHeight[] = { 70.0f,75.0f, 71.0f};
    for (int i = 0; i < 3; ++i)
    {
        deathResultButtons[i] = std::make_shared<UIButtonComponent>(std::string("./Data/Textures/UI/Result/") + paths[i], names[i]);
        deathResultButtons[i]->SetSize({ buttonWidths[i], buttonHeight[i] });
        deathResultButtons[i]->SetScale(deathResultButtonScale);
        deathResultButtons[i]->SetUseHoverScale(false);
        deathResultButtons[i]->SetPivot({ 0.5f, 0.5f });
        deathResultButtons[i]->SetVisible(false);
        deathResultButtons[i]->SetEnable(false);
        const int index = i;
        deathResultButtons[i]->onClick = [this, index]() { ExecuteDeathResult(index); };
        deathResultButtons[i]->zOrder = 21;
        uiManager->Add(deathResultButtons[i]);
        uiManager->AddButton(deathResultButtons[i]);
    }

    deathResultSelectLineLeft = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/select_button_line.png", "DeathResultSelectLineLeft");
    deathResultSelectLineRight = std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/select_button_line.png", "DeathResultSelectLineRight");
    for (const auto& line : { deathResultSelectLineLeft, deathResultSelectLineRight })
    {
        line->SetVisible(false);
        uiManager->Add(line);
    }
    deathResultSelectLineLeft->SetPivot({ 1.0f,0.5f });
    deathResultSelectLineRight->SetPivot({ 0.0f,0.5f });
    deathResultSelectLineLeft->zOrder = 22;
    deathResultSelectLineRight->zOrder = 22;
    SetDeathResultVisible(false);
}

void GameScene::SetDeathResultVisible(const bool visible)
{
    deathResultVisible = visible;
    if (!visible) deathResultSelectLineAnimProgress = 0.0f;
    if (deathResultDarkOverlay) deathResultDarkOverlay->SetVisible(visible);
    if (deathResultDefeated) deathResultDefeated->SetVisible(visible);
    if (deathResultBattleTime) deathResultBattleTime->SetVisible(visible);
    for (auto& digit : deathResultTimeDigits)
        if (digit) digit->SetVisible(visible);
    for (auto& button : deathResultButtons)
    {
        if (button) { button->SetVisible(visible); button->SetEnable(visible && deathResultInputEnabled); }
    }
    if (deathResultSelectLineLeft) deathResultSelectLineLeft->SetVisible(visible);
    if (deathResultSelectLineRight) deathResultSelectLineRight->SetVisible(visible);
    if (visible)
    {
        UpdateDeathResultBattleTimeValue();
        UpdateDeathResultUILayout();
    }
}

void GameScene::SelectDeathResult(const int index)
{
    const int next = std::clamp(index, 0, 2);
    if (next == deathResultSelection && deathResultSelectLineLeft && deathResultSelectLineLeft->IsVisible()) return;
    deathResultSelection = next;
    deathResultSelectLineAnimProgress = 0.0f;
    if (deathResultButtons[deathResultSelection])
        GetUIManager()->SetSelected(deathResultButtons[deathResultSelection].get());
}

void GameScene::UpdateDeathResultBattleTimeValue()
{
    const int totalCentiseconds = std::clamp(static_cast<int>(std::floor(deathAttemptTime * 100.0f + 0.5f)), 0, 99 * 60 * 100 + 59 * 100 + 99);
    const int minutes = totalCentiseconds / 6000;
    const int seconds = (totalCentiseconds / 100) % 60;
    const int centiseconds = totalCentiseconds % 100;
    deathResultTimeValues = { minutes / 10, minutes % 10, -1, seconds / 10, seconds % 10, -2, centiseconds / 10, centiseconds % 10 };
}

void GameScene::UpdateDeathResultBattleTimeLayout()
{
    const int minutes = deathResultTimeValues[0] * 10 + deathResultTimeValues[1];
    std::array<float, 8> layoutPositions{};
    int visibleIndex = 0;
    int firstVisible = -1;
    int lastVisible = -1;

    for (int i = 0; i < 8; ++i)
    {
        if (i == 0 && minutes < 10) continue;
        float groupSpacing = 0.0f;
        if (i >= 3) groupSpacing += deathResultMinuteSecondSpacing;
        if (i >= 6) groupSpacing += deathResultSecondCentisecondSpacing;
        layoutPositions[i] = visibleIndex++ * deathResultTimeNumberSpacing + groupSpacing;
        if (firstVisible < 0) firstVisible = i;
        lastVisible = i;
    }
    const float layoutCenter = firstVisible >= 0
        ? (layoutPositions[firstVisible] + layoutPositions[lastVisible]) * 0.5f
        : 0.0f;

    for (int i = 0; i < 8; ++i)
    {
        auto& digit = deathResultTimeDigits[i];
        if (!digit) continue;
        const bool show = !(i == 0 && minutes < 10);
        digit->SetVisible(deathResultVisible && show);
        if (!show) continue;
        DirectX::XMFLOAT2 position{ deathResultTimePosition.x + layoutPositions[i] - layoutCenter, deathResultTimePosition.y };
        DirectX::XMFLOAT2 scale = deathResultTimeNumberScale;
        if (deathResultTimeValues[i] == -1)
        {
            position.x += deathResultColonOffset.x;
            position.y += deathResultColonOffset.y;
            scale.x *= deathResultColonScale.x;
            scale.y *= deathResultColonScale.y;
        }
        else if (deathResultTimeValues[i] == -2)
        {
            position.x += deathResultDotOffset.x;
            position.y += deathResultDotOffset.y;
            scale.x *= deathResultDotScale.x;
            scale.y *= deathResultDotScale.y;
        }
        digit->SetWorldPosition(position);
        digit->SetScale(scale);
        if (deathResultTimeValues[i] >= 0)
            digit->SetUV({ deathResultTimeValues[i] * numberTexWidth, 0.0f, numberTexWidth, numberTexHeight });
    }
}

void GameScene::UpdateDeathResultUILayout()
{
    if (!deathResultVisible) return;
    if (deathResultDefeated)
    {
        deathResultDefeated->SetWorldPosition(deathResultDefeatedPosition);
        deathResultDefeated->SetScale(deathResultDefeatedScale);
    }
    if (deathResultBattleTime)
    {
        deathResultBattleTime->SetWorldPosition(deathResultBattleTimePosition);
        deathResultBattleTime->SetScale(deathResultBattleTimeScale);
    }
    UpdateDeathResultBattleTimeLayout();
    UpdateDeathResultMenu();
}
void GameScene::UpdateDeathResultMenu()
{
    if (!deathResultVisible) return;
    float totalWidth = 0.0f;
    for (const auto& button : deathResultButtons)
        if (button) totalWidth += button->GetSize().x * deathResultButtonScale.x;
    totalWidth += deathResultButtonHorizontalSpacing * 2.0f;
    float cursorX = deathResultButtonStartPosition.x - totalWidth * 0.5f;
    for (const auto& button : deathResultButtons)
    {
        if (!button) continue;
        const float width = button->GetSize().x * deathResultButtonScale.x;
        button->SetScale(deathResultButtonScale);
        button->SetWorldPosition({ cursorX + width * 0.5f, deathResultButtonStartPosition.y });
        cursorX += width + deathResultButtonHorizontalSpacing;
    }

    // UIManager owns D-Pad/Analog selection; mirror its current Result button.
    if (auto* selected = GetUIManager()->GetSelectedButton())
    {
        for (int i = 0; i < static_cast<int>(deathResultButtons.size()); ++i)
        {
            if (deathResultButtons[i].get() == selected && i != deathResultSelection)
            {
                deathResultSelection = i;
                deathResultSelectLineAnimProgress = 0.0f;
                CoreAudio::PlayOneShot("./Data/Sound/SE/button_select_move.wav", deathResultMoveSeVolume);
            }
        }
    }
    const float buttonsAlpha = std::clamp((deathPresentationElapsed - deathButtonsStartTime) /
        (std::max)(0.001f, deathButtonsFadeDuration), 0.0f, 1.0f);
    for (int i = 0; i < static_cast<int>(deathResultButtons.size()); ++i)
    {
        auto& button = deathResultButtons[i];
        if (!button) continue;
        const bool selected = i == deathResultSelection;
        CoreColor color = selected ? deathResultSelectedColor : deathResultUnselectedColor;
        color.a *= buttonsAlpha;
        button->SetColor(color);
        if (selected)
            button->SetScale({ deathResultButtonScale.x * deathResultSelectedScale, deathResultButtonScale.y * deathResultSelectedScale });
    }

    if (deathResultButtons[deathResultSelection])
    {
        const auto button = deathResultButtons[deathResultSelection];
        const auto buttonPos = button->GetWorldPosition();
        const auto buttonSize = button->GetSize();
        const float buttonWidth = buttonSize.x * deathResultButtonScale.x;
        const float lineWidth = buttonWidth * 0.5f;
        const float lineY = buttonPos.y;
        for (const auto& line : { deathResultSelectLineLeft, deathResultSelectLineRight })
        {
            if (!line) continue;
            line->SetSize({ lineWidth, 5.0f });
            line->SetScale({ deathResultSelectLineScale.x * deathResultSelectLineAnimProgress, deathResultSelectLineScale.y });
            line->SetWorldPosition({ buttonPos.x, lineY });
        }
        if (deathResultSelectLineLeft)
            deathResultSelectLineLeft->SetWorldPosition({ buttonPos.x - buttonWidth * 0.5f - deathResultSelectLineDistances[deathResultSelection] - lineWidth * 0.5f, lineY });
        if (deathResultSelectLineRight)
            deathResultSelectLineRight->SetWorldPosition({ buttonPos.x + buttonWidth * 0.5f + deathResultSelectLineDistances[deathResultSelection] + lineWidth * 0.5f, lineY });
        if (deathPresentationElapsed >= deathSelectLineStartTime)
            deathResultSelectLineAnimProgress = (std::min)(1.0f, deathResultSelectLineAnimProgress + Time::UnscaledDeltaTime() / (std::max)(0.001f, deathResultSelectLineAnimDuration));
    }
}

void GameScene::ResetDeathPresentationCues()
{
    deathHudFadeCueFired = deathOverlayCueFired = deathDefeatedCueFired = false;
    deathBattleTimeCueFired = deathButtonsCueFired = false;
}

void GameScene::FireDeathPresentationCue(const DeathPresentationCue cue, bool& fired)
{
    if (fired) return;
    fired = true;
    if (deathPresentationCueCallback) deathPresentationCueCallback(cue);
}

void GameScene::UpdateDeathResultPresentation()
{
    const auto fade = [this](const float start, const float duration)
    {
        return std::clamp((deathPresentationElapsed - start) / (std::max)(0.001f, duration), 0.0f, 1.0f);
    };
    const float hudAlpha = 1.0f - fade(0.0f, deathHudFadeDuration);
    if (player) player->SetGameplayHudFadeAlpha(hudAlpha);
    if (gruxEnemyActor) gruxEnemyActor->SetHpBarFadeAlpha(hudAlpha);

    if (!deathResultVisible)
    {
        deathResultVisible = true;
        SetDeathResultVisible(true);
        deathResultSelection = 0;
        deathResultSelectLineAnimProgress = 0.0f;
        if (deathResultButtons[0]) GetUIManager()->SetSelected(deathResultButtons[0].get());
    }

    const float overlayAlpha = fade(deathOverlayStartTime, deathOverlayFadeDuration);
    const float defeatedAlpha = fade(deathDefeatedStartTime, deathDefeatedFadeDuration);
    const float battleTimeAlpha = fade(deathBattleTimeStartTime, deathBattleTimeFadeDuration);
    const float lineAlpha = fade(deathSelectLineStartTime, deathResultSelectLineAnimDuration);
    if (deathResultDarkOverlay) deathResultDarkOverlay->SetColor(CoreColor{ 0.0f, 0.0f, 0.0f, deathOverlayMaxAlpha * overlayAlpha });
    if (deathResultDefeated) deathResultDefeated->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, defeatedAlpha });
    if (deathResultBattleTime) deathResultBattleTime->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, battleTimeAlpha });
    for (auto& digit : deathResultTimeDigits) if (digit) digit->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, battleTimeAlpha });
    for (auto& line : { deathResultSelectLineLeft, deathResultSelectLineRight }) if (line) line->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, lineAlpha });

    if (deathPresentationElapsed >= deathOverlayStartTime) FireDeathPresentationCue(DeathPresentationCue::OverlayFade, deathOverlayCueFired);
    if (deathPresentationElapsed >= deathDefeatedStartTime) FireDeathPresentationCue(DeathPresentationCue::DefeatedFade, deathDefeatedCueFired);
    if (deathPresentationElapsed >= deathBattleTimeStartTime) FireDeathPresentationCue(DeathPresentationCue::BattleTimeFade, deathBattleTimeCueFired);
    if (deathPresentationElapsed >= deathButtonsStartTime) FireDeathPresentationCue(DeathPresentationCue::ButtonsFade, deathButtonsCueFired);
}

void GameScene::ExecuteDeathResult(const int index)
{
    if (!deathResultInputEnabled) return;
    CoreAudio::PlayOneShot("./Data/Sound/SE/button_push.wav", deathResultConfirmSeVolume);
    deathResultInputEnabled = false;
    deathResultSelection = 0;
    deathResultSelectLineAnimProgress = 0.0f;
    SetDeathResultVisible(false);
    if (index == 0) ResetBattleForContinue();
    else if (index == 1) { battleElapsedTime = 0.0f; if (gruxEnemyActor) gruxEnemyActor->ResetForBattleRestart(bossBattleStartTransform); if (player) player->ResetForBattleContinue(playerBattleStartTransform); if (gruxEnemyActor) gruxEnemyActor->ResumeBattleAI(); StartBossBattle(); }
    else SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", "TitleScene") }, TransitionStyle::Fade);
}
void GameScene::OnPlayerDeathCameraStart()
{
    if (deathCameraStartRequested || battleFlowState != BattleFlowState::PlayerDead)
        return;

    deathCameraStartRequested = true;
    if (darkCameraActor)
        darkCameraActor->StartDeathMode(nullptr);
}

void GameScene::ResetBattleForContinue()
{
    if (!battleStartTransformsSaved || !player || !gruxEnemyActor)
        return;

    deathPresentationElapsed = 0.0f;
    deathResultVisible = false;
    deathResultInputEnabled = false;
    deathResultSelection = 0;
    SetDeathResultVisible(false);

    Time::SetSlow(1.0f, 0.0f);

    if (cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(this);
    if (darkCameraActor)
        darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);

    player->ResetForBattleContinue(playerBattleStartTransform);
    player->SetDeathCameraStartCallback(nullptr);
    player->SetDeathCameraTransparencyDisabled(false);
    gruxEnemyActor->ResetForBattleContinue(bossBattleStartTransform);
    gruxEnemyActor->ResumeBattleAI();

    if (darkCameraActor)
        darkCameraActor->RotateToPlayerForward();

    SetBattleHudVisible(true);
    player->SetGameplayHudVisible(true);
    // 入力を受け付ける
    InputSystem::SetInputEnabled(true);
    battleFlowState = BattleFlowState::Playing;
}

void GameScene::EnterBossDead()
{
    battleFlowState = BattleFlowState::BossDead;
    SetBattleHudVisible(false);

    if (player)
        player->EnterWinState();
    if (gruxEnemyActor)
        gruxEnemyActor->StopBattleActions();
    if (darkCameraActor)
    {
        darkCameraActor->CancelOffscreenAttackAssist();
        darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);
    }
}

void GameScene::UpdateBattleFlow()
{
    switch (battleFlowState)
    {
    case BattleFlowState::Intro:
        break;
    case BattleFlowState::Playing:
        battleElapsedTime += Time::UnscaledDeltaTime();
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
        deathPresentationElapsed += Time::UnscaledDeltaTime();
        UpdateDeathResultPresentation();
        if (!deathResultInputEnabled && deathPresentationElapsed >= deathResultInputEnableTime)
        {
            deathResultInputEnabled = true;
            SetDeathResultVisible(true);
            InputSystem::SetInputEnabled(true);
            battleFlowState = BattleFlowState::ContinueWait;
        }
        break;
    case BattleFlowState::ContinueWait:
        // Result UIButton callbacks own the selection and confirmation.
        deathPresentationElapsed += Time::UnscaledDeltaTime();
        UpdateDeathResultPresentation();
        break;
    case BattleFlowState::ResetForContinue:
        ResetBattleForContinue();
        break;
    case BattleFlowState::BossDead:
        if (gruxEnemyActor && gruxEnemyActor->GetStateMachine() &&
            std::string(gruxEnemyActor->GetStateMachine()->GetStateName()) == "EnemyDeathState" &&
            gruxEnemyActor->GetBodyAnimationController() &&
            !gruxEnemyActor->GetBodyAnimationController()->IsPlayAnimation())
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

    Transform debrisTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    this->GetActorManager()->CreateAndRegisterActorWithTransform<ModelDebrisEmitterActor>(
        "ModelDebrisEmitterActor", debrisTr);


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
    ImGui::Separator();
    ImGui::Text("Death Presentation");
    ImGui::DragFloat(U8("HUDフェードアウト時間"), &deathHudFadeDuration, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat(U8("背景暗転 開始時間"), &deathOverlayStartTime, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat(U8("背景暗転 フェード時間"), &deathOverlayFadeDuration, 0.01f, 0.01f, 5.0f);
    ImGui::SliderFloat(U8("背景暗転 最大濃度"), &deathOverlayMaxAlpha, 0.0f, 1.0f);

    ImGui::DragFloat(U8("DEFEATED 表示開始時間"), &deathDefeatedStartTime, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat(U8("DEFEATED フェード時間"), &deathDefeatedFadeDuration, 0.01f, 0.01f, 5.0f);

    ImGui::DragFloat(U8("Battle Time 表示開始時間"), &deathBattleTimeStartTime, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat(U8("Battle Time フェード時間"), &deathBattleTimeFadeDuration, 0.01f, 0.01f, 5.0f);

    ImGui::DragFloat(U8("ボタン 表示開始時間"), &deathButtonsStartTime, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat(U8("ボタン フェード時間"), &deathButtonsFadeDuration, 0.01f, 0.01f, 5.0f);

    ImGui::DragFloat(U8("選択ライン 表示開始時間"), &deathSelectLineStartTime, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat(U8("操作受付 開始時間"), &deathResultInputEnableTime, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat2("Result Defeated Position", &deathResultDefeatedPosition.x, 1.0f);
    ImGui::DragFloat2("Result Defeated Scale", &deathResultDefeatedScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat2("Result BattleTime Position", &deathResultBattleTimePosition.x, 1.0f);
    ImGui::DragFloat2("Result BattleTime Scale", &deathResultBattleTimeScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat2("Result Button Start Position", &deathResultButtonStartPosition.x, 1.0f);
    ImGui::DragFloat("Result Button Horizontal Spacing", &deathResultButtonHorizontalSpacing, 1.0f, 0.0f, 500.0f);
    ImGui::DragFloat2("Result Button Scale", &deathResultButtonScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat(U8("選択中ボタン 拡大率"), &deathResultSelectedScale, 0.01f, 1.0f, 2.0f);
    ImGui::ColorEdit4(U8("選択中ボタン 色"), &deathResultSelectedColor.r);
    ImGui::ColorEdit4(U8("非選択ボタン 色"), &deathResultUnselectedColor.r);
    ImGui::SliderFloat("Result Move SE Volume", &deathResultMoveSeVolume, 0.0f, 2.0f);
    ImGui::SliderFloat("Result Confirm SE Volume", &deathResultConfirmSeVolume, 0.0f, 2.0f);
    ImGui::DragFloat("Select Line Distance From Continue Button", &deathResultSelectLineDistances[0], 1.0f, -200.0f, 200.0f);
    ImGui::DragFloat("Select Line Distance From Restart Button", &deathResultSelectLineDistances[1], 1.0f, -200.0f, 200.0f);
    ImGui::DragFloat("Select Line Distance From Title Button", &deathResultSelectLineDistances[2], 1.0f, -200.0f, 200.0f);
    ImGui::DragFloat2("Result Select Line Scale", &deathResultSelectLineScale.x, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Result Select Line Animation Duration", &deathResultSelectLineAnimDuration, 0.01f, 0.01f, 1.0f);
    ImGui::DragFloat2("Battle Time Number Position", &deathResultTimePosition.x, 1.0f);
    ImGui::DragFloat2("Battle Time Number Scale", &deathResultTimeNumberScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat2("Battle Time Colon Offset", &deathResultColonOffset.x, 1.0f);
    ImGui::DragFloat2("Battle Time Colon Scale", &deathResultColonScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat2("Battle Time Dot Offset", &deathResultDotOffset.x, 1.0f);
    ImGui::DragFloat2("Battle Time Dot Scale", &deathResultDotScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat("Battle Time Number Spacing", &deathResultTimeNumberSpacing, 1.0f, 1.0f, 200.0f);
    ImGui::DragFloat(U8("Battle Time 分・秒 間隔"), &deathResultMinuteSecondSpacing, 1.0f, -200.0f, 500.0f);
    ImGui::DragFloat(U8("Battle Time 秒・小数 間隔"), &deathResultSecondCentisecondSpacing, 1.0f, -200.0f, 500.0f);
    ImGui::DragFloat("numberTexWidth", &numberTexWidth, 1.0f, 1.0f, 200.0f);
    ImGui::DragFloat("numberTexHeight", &numberTexHeight, 1.0f, 1.0f, 300.0f);

    if (ImGui::TreeNode("Death Staging"))
    {
        ImGui::DragFloat("Min Player X", &deathStagingMinPlayerX, 0.01f);
        ImGui::DragFloat("Max Player X", &deathStagingMaxPlayerX, 0.01f);
        ImGui::DragFloat("Min Player Z", &deathStagingMinPlayerZ, 0.01f);
        ImGui::DragFloat("Max Player Z", &deathStagingMaxPlayerZ, 0.01f);
        ImGui::DragFloat("Corner Inset X", &deathStagingCornerInsetX, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Corner Inset Z", &deathStagingCornerInsetZ, 0.01f, 0.0f, 10.0f);
        static constexpr const char* areaNames[] =
        { "Center", "Front", "Back", "Left", "Right", "FrontLeft", "FrontRight", "BackLeft", "BackRight" };
        for (size_t i = 0; i < deathStagingAreaSettings.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            ImGui::DragFloat(areaNames[i], &deathStagingAreaSettings[i].bossDistance, 0.01f, 0.0f, 20.0f);
            ImGui::PopID();
        }
        ImGui::TreePop();
        ImGui::Text("deathAttemptTime : %.3f", deathAttemptTime);
        ImGui::Text("battleElapsedTime : %.3f", battleElapsedTime);


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
