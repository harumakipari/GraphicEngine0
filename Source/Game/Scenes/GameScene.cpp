#include "pch.h"
#include "GameScene.h"
#include "Game/DarkGame/GameBossDeathController.h"
#include "Game/DarkGame/GamePlayerDeathController.h"
#include "Game/DarkGame/GameVictoryController.h"
#include "Game/DarkGame/GamePhase2TransitionController.h"
#include "UI/Game/SceneTransitionManager.h"
#include <json.hpp>
#include <fstream>
#include <filesystem>

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

#include <tracy/Tracy.hpp>

#include "Components/Audio/AudioSourceComponent.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Camera/MovieCameraManagerActor.h"
#include "Engine/Debug/SceneEditor.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Camera/DarkGameCamera.h"

#include "Core/ActorManager.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Actors/UI/GameTimerUIActor.h"
#include "Game/Actors/UI/GameDefeatUIActor.h"
#include "Game/Actors/UI/GameVictoryUIActor.h"
#include "Game/Actors/UI/GameMovieSkipUIActor.h"
#include "Game/Actors/Stage/Cloth.h"


#include "Game/DarkGame/DarkActors/DarkStage.h"
#include "Game/DarkGame/DarkActors/DarkStageChandelierActor.h"
#include "Game/DarkGame/DarkActors/DoorActor.h"
#include "Game/DarkGame/DarkActors/ModelDebrisEmitterActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/SkeletonWarriorEnemy.h"
#include "Physics/Physics.h"
#include "Physics/CollisionFunction.h"

#include "Physics/CollisionSystem.h"
#include "UI/UIManager.h"
#include "UI/Game/Pause.h"



bool GameScene::Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
{
    PROFILE_FUNCTION();

    // 牢屋のムービーなどのフラグを初期化する
    prisonOpeningMovieStartRequested = false;
    backToRoomStartRequested = false;

    // タイトルからかゲームシーンからかによってムービーを再生するかを決める
    if (const auto entryIt = props.find("gameEntry"); entryIt != props.end())
    {
        prisonOpeningMovieStartRequested = entryIt->second == "NewGame";
        backToRoomStartRequested = entryIt->second == "BackToRoom";
    }

    // チュートリアル
    tutorialController.Initialize(backToRoomStartRequested);
    judgeDodgeOperationTutorialOwnsPause = false;


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
        stageCandelabraAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Candelabra/Candelabra.gltf", ModelTypes::ModelMode::InstancedStaticMesh, true, true);
        stageCandelabraAsset->spawnPoints = stageCandelabraAsset->model->spawnPoints;

        stageBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Brazier/Brazier.gltf", ModelTypes::ModelMode::InstancedStaticMesh, true, true);
        stageBrazierAsset->spawnPoints = stageBrazierAsset->model->spawnPoints;

        stageGroundBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/GroundBrazier/groundBrazier.gltf", ModelTypes::ModelMode::InstancedStaticMesh, true, true);
        stageGroundBrazierAsset->spawnPoints = stageGroundBrazierAsset->model->spawnPoints;

        stageMeltedWaxAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/MeltedWax/MeltedWax.gltf", ModelTypes::ModelMode::InstancedStaticMesh, true, true);
        stageMeltedWaxAsset->spawnPoints = stageMeltedWaxAsset->model->spawnPoints;

        stageStandingBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/StandingBrazier/StandingBrazier.gltf", ModelTypes::ModelMode::InstancedStaticMesh, true, true);
        stageStandingBrazierAsset->spawnPoints = stageStandingBrazierAsset->model->spawnPoints;

        stageCandleStandAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/CandleStand/CandleStand.gltf", ModelTypes::ModelMode::InstancedStaticMesh, true, true);
        stageCandleStandAsset->spawnPoints = stageCandleStandAsset->model->spawnPoints;

    });


    SceneBase::Initialize(device, width, height, props);
    Physics::Instance().Initialize();

    {
        PROFILE_SCOPE("SetUpActors Init");
        //アクターをセット
        SetUpActors();
        CreateController();
        CreateUI();
    }

    // ボスの部屋のラープのための初期化処理
    bossLerpEasing = std::make_unique<EasingRunner>();


    // ここで布を描画する処理を追加
    RegisterRenderHook(RenderPass::Mask, [&](ID3D11DeviceContext* immediateContext)
    {
        for (auto darkClothActor : darkClothActors)
        {
            if (darkClothActor)
            {
                darkClothActor->RenderCloth(immediateContext);
            }
        }
    });


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
    tutorialBossEntryPending = false;
    tutorialController.ResetForSceneStart();
    if (player)
        player->SetMinimumHp(5);    // チュートリアルのためHPの下限制限を設ける

    battleTimerUIActor->SetVisible(false);
    SetBattleHudVisible(false);

    // ゲームBGM
    gameBgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("GameBgmActor");
    gameBgmActor->SetSource(L"./Data/Sound/BGM/game_bgm1.wav");
    gameBgmActor->SetLoop(true);
    gameBgmActor->SetBgm(true);
    bgmDirector.SetGameActor(gameBgmActor);
    bgmDirector.PlayGameBgm();
    gameBgmActor->SetVolume(0.3f);

    // ボスBGM
    bossBgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("BossBgmActor");
    bossBgmActor->SetSource(L"./Data/Sound/BGM/boss_phase1_bgm1.wav");
    bossBgmActor->SetLoop(true);
    bossBgmActor->SetBgm(true);
    bossBgmActor->SetVolume(bossBgmVolume);

    // ボス第二形態のBGM
    phase2BgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("BossPhase2BgmActor");
    phase2BgmActor->SetSource(L"./Data/Sound/BGM/boss_phase2_bgm1.wav");
    phase2BgmActor->SetLoop(true);
    phase2BgmActor->SetBgm(true);
    phase2BgmActor->SetVolume(0.0f);

    // ボス死亡時のBGM
    bossDeathBgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("BossDeathSecondBgmActor");
    bossDeathBgmActor->SetSource(L"./Data/Sound/BGM/boss_death_second_bgm.wav");
    bossDeathBgmActor->SetLoop(true);
    bossDeathBgmActor->SetBgm(true);
    bossDeathBgmActor->SetVolume(0.45f);

    // プレイヤ＾死亡時のBGM
    playerDeathBgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("PlayerDeathBgmActor");
    playerDeathBgmActor->SetSource(L"./Data/Sound/BGM/player_death_bgm_1.wav");
    playerDeathBgmActor->SetLoop(true);
    playerDeathBgmActor->SetBgm(true);
    playerDeathBgmActor->SetVolume(0.35f);

    // BGMディレクターのBGMを設定
    bgmDirector.SetActors(gameBgmActor, bossBgmActor, phase2BgmActor, bossDeathBgmActor, playerDeathBgmActor);

    // シーンプリセットを設定する
    SceneEditor::LoadPresetList(); // 更新
    std::string file = "newPreset.json";
    static SceneState savedState;
    SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
    savedState.ApplyScenePreset(Scene::GetCurrentScene());

    // カメラをplayerの前方向を向くように変更
    darkCameraActor->RotateToPlayerForward();

    
    if (backToRoomStartRequested)
    {
        if (const auto jailDoor = GetActorManager()->GetActorOfType<DoorJailActor>())
            jailDoor->SetOpenCompletedSilently();
    }

    if (prisonOpeningMovieStartRequested)
    {
        if (const auto movieManager = GetActorManager()->GetActorOfType<MovieCameraManagerActor>())
            movieManager->PreparePrisonOpeningMovieFirstFrame();
    }

    // シーンが切り替わった時に
    SceneTransitionManager::Instance().NotifySceneChanged();

    if (prisonOpeningMovieStartRequested)
    {
        SceneTransitionManager::Instance().SetOnOpeningFinished([]()
        {
            auto* gameScene = dynamic_cast<GameScene*>(Scene::GetCurrentScene());
            if (!gameScene || !gameScene->prisonOpeningMovieStartRequested)
                return;

            gameScene->prisonOpeningMovieStartRequested = false;
            if (!InputSystem::IsInputEnabled())
                return;

            if (const auto movieManager = gameScene->GetActorManager()->GetActorOfType<MovieCameraManagerActor>())
            {
                if (!movieManager->PlayPrisonOpeningMovie())
                    movieManager->CancelPreparedPrisonOpeningMovie();
            }
        });
    }
}

void GameScene::Update(float deltaTime)
{
    using namespace DirectX;

    ZoneScopedN("Game Update");
    bgmDirector.UpdateGameBgmFadeOut(Time::UnscaledDeltaTime());
    bgmDirector.UpdateDeathBgmFade(Time::UnscaledDeltaTime());
    bgmDirector.UpdatePhase2BgmCrossFade(Time::UnscaledDeltaTime(), bossBgmVolume);

    if (tutorialController.GetExplanationState() == GameTutorialController::ExplanationState::Showing)
    {
        tutorialUIActor->UpdateExplanationLayout();
        if (InputSystem::GetInputState("A", InputStateMask::Trigger) || InputSystem::GetInputState("Enter", InputStateMask::Trigger))
            CloseJudgeDodgeOperationTutorial();
        return;
    }
    const bool mainRoomTutorialActive = !tutorialController.IsCompletedForBossBattle() &&
        !bossRoomLockOnScopeActive && !tutorialBossEntryPending &&
        player && !player->IsBossBattle();
    if (mainRoomTutorialActive)
    {

        if (tutorialController.GetExplanationState() == GameTutorialController::ExplanationState::Unseen && player && player->GetPosition().x > -34.0f && !IsPaused() && InputSystem::IsInputEnabled() && !cameraManager->IsUseMovie() && !cameraManager->IsUseCinematic())
        {
            StartJudgeDodgeOperationTutorial();
            return;
        }
    }

    playerDeathController->UpdateFrameData(*defeatUIActor, numberTexWidth, numberTexHeight, deathResultMoveSeVolume);
    UpdateBattleFlow();
    victoryUIActor->UpdateButtonLayout();
    defeatUIActor->UpdateLayout();

    // ボスの部屋の明るさラープのため
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
    UpdateTutorialCameraProfileTrigger();

    const bool isCinematicOrMovie = cameraManager->IsUseCinematic() || cameraManager->IsUseMovie();
    player->SetLowHpPresentationCinematicSuppressed(isCinematicOrMovie);
    const bool playerInputAvailable = InputSystem::IsInputEnabled() &&
        !cameraManager->IsUseDebug() && !isCinematicOrMovie;
    if (!player->IsBossBattle())
        player->SetHpBarVisible(playerInputAvailable);

    // シネマカメラだったらまたはムービーカメラだったらプレイヤーを透明化しない
    if (isCinematicOrMovie)
    {
        player->SetIsPlayerTransparency(false);
        player->SetOperationGuideHudVisible(false);
    }
    else
    {
        player->SetIsPlayerTransparency(true);
        player->SetOperationGuideHudVisible(playerInputAvailable);
    }


    const auto lockOnSelectionResult = UpdateLockOnTargetSelection();
    UpdateOperationGuideTutorial(lockOnSelectionResult);
    UpdateTutorialDodgeGuideRing();

    SceneBase::Update(deltaTime);
    UpdateMovieSkip();
    UpdateTutorialSkeletonUI();
    UpdateLockOnTargetUI(deltaTime);

    // Gameplay時間へ同期し、HitStop中はPhysXだけが進むずれを防ぐ。
    if (deltaTime > FLT_EPSILON)
        Physics::Instance().Update(deltaTime);
    {
        ZoneScopedN("Collision Resolution");
        CollisionSystem::DetectAndResolveCollisions();
        CollisionSystem::ApplyPushAll();
    }

#ifdef USE_IMGUI


    if (enableLightGui || enableSceneGui)
    {
        InputSystem::SetCursorVisible(true);
    }

#if 0

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

#endif // 0




#endif // !USE_IMGUI


}
bool GameScene::Uninitialize(ID3D11Device* device)
{
    bgmDirector.StopDeathBgm();
    bgmDirector.ClearActors();
    if (player)
        player->SetDeathCameraStartCallback(nullptr);
    if (defeatUIActor)
        defeatUIActor->SetSelectionCallback(nullptr);
    SceneBase::Uninitialize(device);
    Physics::Instance().Finalize();
    return true;
}

void GameScene::DrawGuiPlusAlpha()
{
#ifdef USE_IMGUI
    ImGui::Begin("GameScene");

    DrawTutorialDebugGui();
    DrawPhase2DebugGui();
    DrawBgmAndBattleTimerDebugGui();
    DrawBossDeathDebugGui();
    DrawPlayerDeathDebugGui();

    ImGui::End();
#endif
}
void GameScene::SetUpActors()
{
    Transform mainRoomStartPlayerTransform{ DirectX::XMFLOAT3{ -13.537f,0.0f,10.757f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.07f,1.07f,1.07f } };
    player = this->GetActorManager()->CreateAndRegisterActorWithTransform<Player>("Player", mainRoomStartPlayerTransform);
    player->SetFinalHitCallback([this](GruxEnemy* boss, const DirectX::XMFLOAT3& source)
    {
        OnPlayerFinalHit(boss, source);
    });

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

    // 瓦礫エフェクトアクター生成
    Transform debrisTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    this->GetActorManager()->CreateAndRegisterActorWithTransform<ModelDebrisEmitterActor>("ModelDebrisEmitterActor", debrisTr);

    // 布の生成
    {
        // ボスの部屋　奥の壁
        Transform clothTr(DirectX::XMFLOAT3{ 21.7f,6.8f,18.757f }, DirectX::XMFLOAT3{ 0.0f,-90.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        darkClothActors[0] = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr);

        Transform clothTr1(DirectX::XMFLOAT3{ 21.7f,6.8f,2.957f }, DirectX::XMFLOAT3{ 0.0f,-90.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        darkClothActors[1] = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr1);

        // ボスの部屋　左の壁
        Transform clothTr2(DirectX::XMFLOAT3{ -2.8f,6.8f,24.657f }, DirectX::XMFLOAT3{ 0.0f,-180.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        darkClothActors[2] = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr2);

        Transform clothTr3(DirectX::XMFLOAT3{ 18.9f,6.8f,24.657f }, DirectX::XMFLOAT3{ 0.0f,-180.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        darkClothActors[3] = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr3);

        // ボスの部屋　右の壁
        Transform clothTr4(DirectX::XMFLOAT3{ -2.8f,6.8f,-2.54f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        darkClothActors[4] = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr4);

        Transform clothTr5(DirectX::XMFLOAT3{ 18.9f,6.8f,-2.54f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        darkClothActors[5] = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr5);

        // メインの部屋　ドア横
        Transform clothTr6(DirectX::XMFLOAT3{ -6.8f,5.7f,6.5f }, DirectX::XMFLOAT3{ 0.0f,-90.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        darkClothActors[6] = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr6);

        Transform clothTr7(DirectX::XMFLOAT3{ -6.8f,5.7f,15.3f }, DirectX::XMFLOAT3{ 0.0f,-90.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        darkClothActors[7] = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr7);

        // メインの部屋　柱
        Transform clothTr8(DirectX::XMFLOAT3{ -22.0f,5.4f,9.5f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        darkClothActors[8] = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr8);

        Transform clothTr9(DirectX::XMFLOAT3{ -22.0f,5.4f,12.4f }, DirectX::XMFLOAT3{ 0.0f,-180.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        darkClothActors[9] = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkClothActor>("cloth", clothTr9);
    }


#if 0
    {// メインの部屋の絵画を生成する
        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
        Transform paintingTr{ pos,point.worldRotation,point.worldScale };
        auto paintingActor = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStagePaintingActor>("PaintingActor", paintingTr);
    }
    // 牢屋の部屋の絵画
    {
        Transform paintingTr{ DirectX::XMFLOAT3{-67.42f,4.2f,17.26f},DirectX::XMFLOAT3{0.0f,180.0f,0.0f},DirectX::XMFLOAT3{0.81f,1.0f,0.81f } };
        auto paintingActor = GetActorManager()->CreateAndRegisterActorWithTransform<DarkStagePaintingActor>("JailPaintingActor", paintingTr);
        paintingActor->SetType(DarkStagePaintingActor::PaintType::JailRoom);
    }

    // メインの部の絵画
    {
        Transform paintingMainRoomTr{ DirectX::XMFLOAT3{-18.1f,3.5f,16.9f},DirectX::XMFLOAT3{0.0f,180.0f,0.0f},DirectX::XMFLOAT3{1.0f,1.0f,1.0f } };
        auto paintingActor = GetActorManager()->CreateAndRegisterActorWithTransform<DarkStagePaintingActor>("MainPaintingActor", paintingMainRoomTr);
        paintingActor->SetType(DarkStagePaintingActor::PaintType::MainRoom);

    }

    // 通路の絵画
    {
        Transform paintingTr{ DirectX::XMFLOAT3{-28.f,4.262f,-2.796f},DirectX::XMFLOAT3{0.0f,0.0f,0.0f},DirectX::XMFLOAT3{0.69f,0.69f,0.69f } };
        auto paintingActor = GetActorManager()->CreateAndRegisterActorWithTransform<DarkStagePaintingActor>("HallWayPaintingActor", paintingTr);
        paintingActor->SetType(DarkStagePaintingActor::PaintType::Hallway);
    }

#endif // 0
    // ボス生成
    Transform GruxEnemyTr(DirectX::XMFLOAT3{ 7.69f,0.0f,11.0f }, DirectX::XMFLOAT3{ 0.0f,-90.0f,0.0f }, DirectX::XMFLOAT3{ 1.7f,1.7f,1.7f });
    gruxEnemyActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<GruxEnemy>("GruxEnemy", GruxEnemyTr);
    if (gruxEnemyActor)
        gruxEnemyActor->PauseBattleAI();
    gruxEnemyActor->SetCinematicDeathAnimationOwnedExternally(true);


    // メインの部屋にチュートリアル用の骸骨を追加。
    {
        // 立っているままの骸骨
        Transform passiveTransform(DirectX::XMFLOAT3{ -39.5f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,-40.0f,0.0f }, DirectX::XMFLOAT3{ 1.07f,1.07f,1.07f });
        tutorialPassiveSkeletonActor = GetActorManager()->CreateAndRegisterActorWithTransform<SkeletonWarriorActor>("TutorialPassiveSkeleton", passiveTransform);
        tutorialPassiveSkeletonActor->SetMaxHp(2);
        tutorialPassiveSkeletonActor->SetTutorialPassive(true);
        tutorialPassiveSkeletonActor->EnableTutorialPassiveStandIdle();
        tutorialPassiveSkeletonActor->SetLockOnCameraProfile(EnemyLockOnCameraProfile::Compact);
        tutorialPassiveSkeletonActor->SetSwordVisible(false);

        // ジャスト回避の骸骨
        Transform dodgeTransform(DirectX::XMFLOAT3{ -15.1f,-0.3f,10.75f }, DirectX::XMFLOAT3{ 0.0f,-140.0f,0.0f }, DirectX::XMFLOAT3{ 1.3f,1.3f,1.3f });
        tutorialDodgeSkeletonActor = GetActorManager()->CreateAndRegisterActorWithTransform<SkeletonWarriorActor>("TutorialDodgeSkeleton", dodgeTransform);
        tutorialDodgeSkeletonActor->SetMaxHp(17);
        tutorialDodgeSkeletonActor->SetTutorialAttackHighlightEnabled(true);
        tutorialDodgeSkeletonActor->SetLockOnCameraProfile(EnemyLockOnCameraProfile::Standard);

    }

    // メインカメラを生成
    Transform darkCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    darkCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkCameraActor>("darkCameraActor", darkCameraTr);
    SetActiveCamera(darkCameraActor);
    float fovY = DirectX::XMConvertToRadians(35.0f);
    float aspect = 1920.f / 1080.f;
    float nearZ = 0.1f;
    float farZ = 1000.f;
    darkCameraActor->SetPerspective(fovY, aspect, nearZ, farZ);
    darkCameraActor->InitSetYawAndPitch(DirectX::XMConvertToRadians(-20.0f), DirectX::XMConvertToRadians(0.0f));
    darkCameraActor->SetPlayerHead(player->GetCameraTargetComponent());
    darkCameraActor->SetEnemyHead(gruxEnemyActor->GetCameraTargetComponent());


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
            if (point.name.rfind("Spawn_SmallDoor", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform smallDoorTr{ pos,point.worldRotation,point.worldScale };
                auto smallDoorActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DoorSmallActor>("smallDoorActor", smallDoorTr);
            }
        }
    }

    // ポーズアクターを生成
    this->GetActorManager()->CreateAndRegisterActorWithTransform<Pause>("pauseActor");
}

void GameScene::EnterBossRoomLockOnScope(const bool fromTutorialDoorMovie)
{
    if (fromTutorialDoorMovie)
        tutorialBossEntryPending = true;
    if (bossRoomLockOnScopeActive)
        return;

    bossRoomLockOnScopeActive = true;
    lockOnRequiresReleaseAfterBossRoomEntry = true;
    lockOnInputHeldLastFrame = false;
    lockOnTargetSelectedForHeldInput = false;

    if (!darkCameraActor)
        return;

    const auto target = darkCameraActor->GetEnemyHead();
    const auto targetOwner = target ? target->GetOwner() : nullptr;
    if (targetOwner && !dynamic_cast<GruxEnemy*>(targetOwner))
    {
        darkCameraActor->ClearEnemyHead();
        darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);
        lockOnTargetUIActor->HideMarker();
    }
}

void GameScene::PrepareTutorialForBossEntry()
{
    if (player)
        player->CancelGameplayActionsForCinematic();
    if (tutorialPassiveSkeletonActor)
        tutorialPassiveSkeletonActor->SetTutorialPassive(true);
    if (tutorialDodgeSkeletonActor)
        tutorialDodgeSkeletonActor->SetTutorialPassive(true);
    HideTutorialSkeletonUI();
    if (const auto doorActor = GetActorManager()->GetActorOfType<DoorLargeActor>())
        doorActor->CompleteInteraction();
}


bool GameScene::BeginPrisonOpeningMoviePlayerSitting() const
{
    if (player)
    {
        constexpr DirectX::XMFLOAT3 prisonOpeningSittingPosition{ -66.919f, -0.078f, 11.892f };
        constexpr float prisonOpeningSittingYawDegrees = 107.03f;
        return player->BeginPrisonOpeningSitting(prisonOpeningSittingPosition, prisonOpeningSittingYawDegrees);
    }
    return false;
}

void GameScene::EndPrisonOpeningMoviePlayerSitting() const
{
    if (player)
        player->EndPrisonOpeningSitting();
}

void GameScene::BeginGameBgmFadeOut()
{
    if (battleFlowState != BattleFlowState::Intro) return;
    bgmDirector.BeginGameFade();
}
void GameScene::PlayBossBattleBgm()
{
    bgmDirector.PlayBossBattleBgm();
}
void GameScene::StartBossBattle()
{
    EnterBossRoomLockOnScope(false);
    victoryUIActor->ResetPresentation();
    victoryController->ResetPresentation();
    ResetHuskCompletionTracking();
    DisableCinematicCameraDebugInput();
    if (!player || !gruxEnemyActor)
        return;

    const bool isInitialTutorialBossEntry = tutorialBossEntryPending;
    tutorialBossEntryPending = false;
    if (tutorialPassiveSkeletonActor)
        tutorialPassiveSkeletonActor->DestroySpawnedBodyParts();
    if (tutorialDodgeSkeletonActor)
        tutorialDodgeSkeletonActor->DestroySpawnedBodyParts();


    const bool hadPhase2BgmRuntime = bgmDirector.HasPhase2BgmRuntime();
    ResetBossPhaseRuntime(BossPhase::Phase1);
    if (hadPhase2BgmRuntime)
        bgmDirector.ResetBossBattleBgm(GameBgmDirector::BossBgmPhase::Phase1, true, bossBgmVolume);
    else
        PlayBossBattleBgm();
    bgmDirector.StopGameBgmIfNotFading();
    ApplyBossPhaseHp(BossPhase::Phase1);
    CaptureContinueBossCheckpoint();
    player->EndEvent();
    player->SetMinimumHp(0);    // ボス戦闘なので制限を設けない
    if (isInitialTutorialBossEntry)
    {
        CompleteTutorialForBossBattle();
        player->RestoreHpToMaxWithUiAnimation();
    }

    if (player->GetRootComponent() && gruxEnemyActor->GetRootComponent())
    {
        playerBattleStartTransform = player->GetRootComponent()->GetComponentWorldTransform();
        bossBattleStartTransform = gruxEnemyActor->GetRootComponent()->GetComponentWorldTransform();
        battleStartTransformsSaved = true;
    }

    player->SetIsBossBattle(true);
    battleElapsedTime = 0.0f;
    finalBattleTime = 0.0f;
    finalBattleTimeSaved = false;
    battleFlowState = BattleFlowState::Playing;
    gruxEnemyActor->ResumeBattleAI();
    battleTimerUIActor->SetVisible(true);
    battleTimerUIActor->SetElapsedTime(battleElapsedTime, numberTexWidth, numberTexHeight);
    SetBattleHudVisible(true);
    player->SetGameplayHudVisible(true);
    // 入力を受け付ける
    InputSystem::SetInputEnabled(true);
}
void GameScene::SkipBossDeathToFinishPrompt()
{
    if (battleFlowState == BattleFlowState::BossDead &&
        bossDeathController->SkipToFinishPrompt(GameBossDeathController::Context{ this, player.get(), gruxEnemyActor.get(), cinemaCameraActor.get(), cameraManager.get(), bossDeathUIActor.get(), bossDeathFadeOverlay.get(), &bgmDirector, &GetSceneSettings().sceneShaderConstants }))
        movieSkipUIActor->ResetPresentation();
}

void GameScene::NotifyPhase2BossRoar()
{
    if (bossPhase != BossPhase::TransitionToPhase2 ||
        phase2TransitionStep != Phase2TransitionStep::BossRecall ||
        phase2BossRoarHpBarStarted || !gruxEnemyActor)
        return;

    phase2BossRoarHpBarStarted = true;
    gruxEnemyActor->BeginPhase2HpBarPresentation(phase2MaxHp);
}
bool GameScene::RequestBossEntryFromPause()
{
    if (!player || player->IsBossBattle() || bossRoomLockOnScopeActive || !cameraManager || cameraManager->IsUseMovie() || cameraManager->IsUseCinematic()) return false;
    const auto movieManager = GetActorManager()->GetActorOfType<MovieCameraManagerActor>();
    if (!movieManager) return false;
    movieManager->PlayDoorMovie();
    return true;
}

bool GameScene::RequestRestartBossBattleFromPause()
{
    if (!player || !player->IsBossBattle() || !gruxEnemyActor || !battleStartTransformsSaved) return false;
    RestartBossBattle();
    return true;
}

void GameScene::RequestReturnToMainRoomStart()
{
    SceneTransitionManager::Instance().RequestTransition(
        "LoadingScene",
        { std::make_pair("preload", "GameScene"), std::make_pair("gameEntry", "BackToRoom") },
        TransitionStyle::Fade);
}

GameScene::LockOnTargetSelectionResult GameScene::UpdateLockOnTargetSelection()
{
    if (!darkCameraActor || !player || battleFlowState == BattleFlowState::BossDead ||
        bossPhase == BossPhase::TransitionToPhase2)
    {
        lockOnInputHeldLastFrame = false;
        lockOnTargetSelectedForHeldInput = false;
        lockOnSelectedEnemy.reset();
        return LockOnTargetSelectionResult::NoCandidate;
    }

    const bool lockOnHeld = InputSystem::GetInputState("LockOn", InputStateMask::Press);
    if (!lockOnHeld)
    {
        lockOnInputHeldLastFrame = false;
        lockOnTargetSelectedForHeldInput = false;
        lockOnRequiresReleaseAfterBossRoomEntry = false;
        lockOnSelectedEnemy.reset();
        return LockOnTargetSelectionResult::NoCandidate;
    }

    if (lockOnRequiresReleaseAfterBossRoomEntry)
        return LockOnTargetSelectionResult::Invalidated;

    if (lockOnInputHeldLastFrame)
    {
        if (!lockOnTargetSelectedForHeldInput)
            return LockOnTargetSelectionResult::NoCandidate;

        bool shouldRelease = !darkCameraActor->HasValidLockOnTarget();
        if (!shouldRelease)
        {
            // 選択済みの敵を取得
            const auto enemy = lockOnSelectedEnemy.lock();

            if (!enemy)
            {
                shouldRelease = true;
            }
            else
            {
                // ボス戦中のGruxは距離制限を適用しない
                const bool isGrux = std::dynamic_pointer_cast<GruxEnemy>(enemy) != nullptr;
                const bool ignoreDistance = isGrux && player->IsBossBattle();
                if (!ignoreDistance)
                {
                    // プレイヤーと敵のXZ平面距離を計算
                    DirectX::XMFLOAT3 delta = MathHelper::Subtract(enemy->GetPosition(), player->GetPosition());
                    delta.y = 0.0f;
                    const float distanceSq = MathHelper::LengthSq(delta);
                    constexpr float releaseDistance = 11.0f;
                    if (distanceSq > releaseDistance * releaseDistance)
                    {
                        shouldRelease = true;
                    }
                }
            }
        }
        if (shouldRelease)
        {
            darkCameraActor->ClearEnemyHead();
            darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);

            lockOnTargetSelectedForHeldInput = false;
            lockOnSelectedEnemy.reset();

            return LockOnTargetSelectionResult::Invalidated;
        }


        if (!darkCameraActor->HasValidLockOnTarget())
        {
            darkCameraActor->ClearEnemyHead();
            darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);
            lockOnTargetSelectedForHeldInput = false;
            lockOnSelectedEnemy.reset();
            return LockOnTargetSelectionResult::Invalidated;
        }
        return LockOnTargetSelectionResult::Selected;
    }

    lockOnInputHeldLastFrame = true;
    lockOnTargetSelectedForHeldInput = false;
    const auto enemies = GetActorManager()->GetActorsOfType<Enemy>();
    std::shared_ptr<Enemy> selectedEnemy;
    std::shared_ptr<SceneComponent> selectedTarget;
    float selectedDistanceSq = FLT_MAX;
    float selectedScreenDistanceSq = FLT_MAX;
    constexpr float distanceTieEpsilonSq = 0.01f;
    for (const auto& enemy : enemies)
    {
        std::shared_ptr<SceneComponent> target;
        float distanceSq = FLT_MAX;
        float screenDistanceSq = FLT_MAX;
        if (!EvaluateLockOnCandidate(enemy, target, distanceSq, screenDistanceSq))
            continue;
        const bool closer = distanceSq < selectedDistanceSq - distanceTieEpsilonSq;
        const bool tiedButMoreCentral = std::abs(distanceSq - selectedDistanceSq) <= distanceTieEpsilonSq && screenDistanceSq < selectedScreenDistanceSq;
        if (closer || tiedButMoreCentral)
        {
            selectedEnemy = enemy;
            selectedTarget = target;
            selectedDistanceSq = distanceSq;
            selectedScreenDistanceSq = screenDistanceSq;
        }
    }

    if (selectedTarget)
    {
        darkCameraActor->SetEnemyHead(selectedTarget);
        lockOnTargetSelectedForHeldInput = true;
        lockOnSelectedEnemy = selectedEnemy;
        return LockOnTargetSelectionResult::Selected;
    }

    darkCameraActor->ClearEnemyHead();
    lockOnSelectedEnemy.reset();
    return LockOnTargetSelectionResult::NoCandidate;
}

bool GameScene::IsBossDeathFinishPromptActive() const
{
    return battleFlowState == BattleFlowState::BossDead && bossDeathController &&
        bossDeathController->IsFinishPromptActive();
}

bool GameScene::IsBossDeathCinematicAudioActive() const
{
    return battleFlowState == BattleFlowState::BossDead && bossDeathController &&
        bossDeathController->IsCinematicAudioActive();
}

// コントローラー生成
void GameScene::CreateController()
{
    // Player勝利時のアニメーションなどの遷移処理
    victoryController = std::make_unique<GameVictoryController>();
    victoryController->LoadBestTime();
    // ボス死亡時
    bossDeathController = std::make_unique<GameBossDeathController>();
    playerDeathController = std::make_unique<GamePlayerDeathController>();
    bossDeathController->LoadBossDeathShots();
    phase2TransitionController = std::make_unique<GamePhase2TransitionController>();
    phase2TransitionController->LoadShots();
}

// ゲームUIを生成
void GameScene::CreateUI()
{
    const auto uiManager = GetUIManager();

    // ボス死亡時のフェード画像生成
    bossDeathFadeOverlay = std::make_shared<UIImageComponent>("BossDeathFadeOverlay");
    bossDeathFadeOverlay->SetSize({ 1920.0f, 1080.0f });
    bossDeathFadeOverlay->SetWorldPosition({ 0.0f, 0.0f });
    bossDeathFadeOverlay->SetPivot({ 0.0f, 0.0f });
    bossDeathFadeOverlay->SetColor(CoreColor{ 0.0f, 0.0f, 0.0f, 0.0f });
    bossDeathFadeOverlay->SetVisible(false);
    bossDeathFadeOverlay->SetEnable(false);
    bossDeathFadeOverlay->zOrder = 1000;
    uiManager->Add(bossDeathFadeOverlay);

    // クリアUIを作成
    victoryUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameVictoryUIActor>("VictoryUI");
    // ロックオンUIを作成
    lockOnTargetUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameLockOnTargetUIActor>("LockOnTargetUI");
    // チュートリアルUIを作成
    tutorialUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameTutorialUIActor>("TutorialUI");
    // 制限時間UIを作成
    battleTimerUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameTimerUIActor>("BattleTimerUI");
    // Player死亡時のUIを作成
    defeatUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameDefeatUIActor>("DefeatUI");
    defeatUIActor->SetSelectionCallback([this](int index) { ExecuteDeathResult(index); });
    // boss死亡時のUIを作成
    bossDeathUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameBossDeathUIActor>("BossDeathUI");
    // ムービースキップUIを作成
    movieSkipUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameMovieSkipUIActor>("MovieSkipUI");
}

void GameScene::UpdateTutorialCameraProfileTrigger()
{
    if (!player || !darkCameraActor || tutorialCameraProfileTriggerFired)
        return;

    const DirectX::XMFLOAT3 position = player->GetPosition();
    const DirectX::XMFLOAT3 halfSize{
        tutorialCameraProfileTriggerSize.x * 0.5f,
        tutorialCameraProfileTriggerSize.y * 0.5f,
        tutorialCameraProfileTriggerSize.z * 0.5f };
    const bool inside =
        std::abs(position.x - tutorialCameraProfileTriggerCenter.x) <= halfSize.x &&
        std::abs(position.y - tutorialCameraProfileTriggerCenter.y) <= halfSize.y &&
        std::abs(position.z - tutorialCameraProfileTriggerCenter.z) <= halfSize.z;
    if (!inside)
        return;

    tutorialCameraProfileTriggerFired = true;
    darkCameraActor->SetTpsCameraProfile(DarkCameraActor::TpsCameraProfile::Standard);
}
bool GameScene::EvaluateLockOnCandidate(const std::shared_ptr<Enemy>& enemy, std::shared_ptr<SceneComponent>& outTarget, float& outDistanceSq, float& outScreenDistanceSq) const
{
    outTarget.reset();
    outDistanceSq = FLT_MAX;
    outScreenDistanceSq = FLT_MAX;
    if (!darkCameraActor || !player || !enemy || enemy->IsDefeated() || enemy->IsPendingKill() ||
        battleFlowState == BattleFlowState::BossDead || bossPhase == BossPhase::TransitionToPhase2)
        return false;

    const auto target = std::dynamic_pointer_cast<SceneComponent>(enemy->FindComponentByName("cameraTargetComponent"));
    if (!target)
        return false;

    const bool isGrux = std::dynamic_pointer_cast<GruxEnemy>(enemy) != nullptr;
    if (bossRoomLockOnScopeActive != isGrux)
        return false;

    DirectX::XMFLOAT3 delta = MathHelper::Subtract(enemy->GetPosition(), player->GetPosition());
    delta.y = 0.0f;
    const float distanceSq = delta.x * delta.x + delta.z * delta.z;
    const float maxDistance = darkCameraActor->GetLockOnTargetSelectionMaxDistance();
    if (!isGrux && distanceSq > maxDistance * maxDistance)
        return false;

    const auto projection = darkCameraActor->ProjectWorldPositionForUI(target->GetComponentLocation());
    float screenDistanceSq = FLT_MAX;
    if (!isGrux)
    {
        if (!projection.valid || !projection.inFront || !projection.insideViewport ||
            !darkCameraActor->IsNormalEnemyLockOnProjectionWithinScreenRadius(projection))
            return false;
        screenDistanceSq = projection.ndc.x * projection.ndc.x + projection.ndc.y * projection.ndc.y;
    }
    else if (projection.valid && projection.inFront && projection.insideViewport)
    {
        screenDistanceSq = projection.ndc.x * projection.ndc.x + projection.ndc.y * projection.ndc.y;
    }

    outTarget = target;
    outDistanceSq = distanceSq;
    outScreenDistanceSq = screenDistanceSq;
    return true;
}
void GameScene::UpdateOperationGuideTutorial(const LockOnTargetSelectionResult selectionResult)
{
    const auto update = tutorialController.BeginGuideUpdate(
        player != nullptr, tutorialPassiveSkeletonActor != nullptr);
    if (update == GameTutorialController::GuideUpdate::HideX)
        player->SetOperationGuideItemVisible(Player::OperationGuideItem::X, false);
    if (update != GameTutorialController::GuideUpdate::Proceed)
        return;

    const auto revealYAndCaptureTutorialSkeletonHp = [this]()
    {
        if (!tutorialController.ShouldRevealY(
            player->IsOperationGuideItemVisible(Player::OperationGuideItem::Y)))
            return;

        player->SetOperationGuideItemVisible(Player::OperationGuideItem::Y, true);
        tutorialController.CaptureYBaseline(false, tutorialPassiveSkeletonActor->GetHp());
        if (tutorialDodgeSkeletonActor)
            tutorialController.CaptureYBaseline(true, tutorialDodgeSkeletonActor->GetHp());
    };

    std::shared_ptr<SceneComponent> target;
    float distanceSq = FLT_MAX;
    float screenDistanceSq = FLT_MAX;
    const bool candidate = EvaluateLockOnCandidate(
        tutorialPassiveSkeletonActor, target, distanceSq, screenDistanceSq);
    if (tutorialController.SetPassiveLockOnCandidate(candidate))
        player->SetOperationGuideItemVisible(Player::OperationGuideItem::LT, true);

    // Reuse the Skeleton combat range; no tutorial-only distance is introduced.
    if (!tutorialPassiveSkeletonActor->IsDead() &&
        tutorialPassiveSkeletonActor->IsPlayerWithinAttackRange(*player))
    {
        revealYAndCaptureTutorialSkeletonHp();
    }

    const auto selectedEnemy = lockOnSelectedEnemy.lock();
    const bool selectedTutorialSkeleton = selectedEnemy &&
    (selectedEnemy.get() == tutorialPassiveSkeletonActor.get() ||
        (tutorialDodgeSkeletonActor && selectedEnemy.get() == tutorialDodgeSkeletonActor.get()));
    const bool selected = selectionResult == LockOnTargetSelectionResult::Selected;
    const bool ltVisible = selected && selectedTutorialSkeleton &&
        player->IsOperationGuideItemVisible(Player::OperationGuideItem::LT);
    if (tutorialController.ShouldLearnLT(selected, selectedTutorialSkeleton, ltVisible))
    {
        player->SetOperationGuideItemLearned(Player::OperationGuideItem::LT);
        revealYAndCaptureTutorialSkeletonHp();
    }

    const bool yVisible = player->IsOperationGuideItemVisible(Player::OperationGuideItem::Y);
    const bool yLearned = yVisible && player->IsOperationGuideItemLearned(Player::OperationGuideItem::Y);
    if (tutorialController.ShouldCheckYDamage(yVisible, yLearned))
    {
        if (tutorialController.DetectYDamage(false, tutorialPassiveSkeletonActor->GetHp()) ||
            (tutorialDodgeSkeletonActor &&
                tutorialController.DetectYDamage(true, tutorialDodgeSkeletonActor->GetHp())))
        {
            player->SetOperationGuideItemLearned(Player::OperationGuideItem::Y);
        }
    }

    const bool tutorialDodgeSkeletonAvailable = tutorialDodgeSkeletonActor &&
        tutorialDodgeSkeletonActor->IsAlive() &&
        tutorialDodgeSkeletonActor->IsActive() &&
        !tutorialDodgeSkeletonActor->IsDead() &&
        !tutorialDodgeSkeletonActor->IsPendingKill();
    const bool dodgeAttackRange = tutorialDodgeSkeletonAvailable &&
        tutorialDodgeSkeletonActor->IsPlayerWithinAttackRange(*player);
    if (tutorialController.SetDodgeAttackRange(dodgeAttackRange))
        revealYAndCaptureTutorialSkeletonHp();

    // Visibility follows the current combat range; learned remains independent.
    player->SetOperationGuideItemVisible(Player::OperationGuideItem::X, tutorialController.IsDodgeAttackRange());

    if (tutorialController.IsDodgeGuideActivated() && !tutorialController.HasDodgeSkeletonJustDodged() &&
        player->IsJustDodgePresentationActive() &&
        player->GetRushTarget().get() == tutorialDodgeSkeletonActor.get())
    {
        // Rush target is assigned only by Player::StartJustDodgeSuccess().
        tutorialController.RecordJustDodge(true);
    }

    const bool xVisible = tutorialController.IsDodgeGuideActivated() &&
        player->IsOperationGuideItemVisible(Player::OperationGuideItem::X);
    const bool xLearned = xVisible && player->IsOperationGuideItemLearned(Player::OperationGuideItem::X);
    const bool dodging = xVisible && !xLearned && player->IsDodging();
    if (tutorialController.ShouldLearnX(xVisible, xLearned, dodging))
    {
        player->SetOperationGuideItemLearned(Player::OperationGuideItem::X);
    }
}

void GameScene::CompleteTutorialForBossBattle()
{
    if (!tutorialController.CompleteForBossBattle())
        return;
    HideTutorialSkeletonUI();
    tutorialController.ResetGuideProgress();

    if (!player)
        return;

    const auto completeOperationGuide = [this](const Player::OperationGuideItem item)
    {
        player->SetOperationGuideItemVisible(item, true);
        player->SetOperationGuideItemLearned(item);
    };
    completeOperationGuide(Player::OperationGuideItem::L);
    completeOperationGuide(Player::OperationGuideItem::LT);
    completeOperationGuide(Player::OperationGuideItem::Y);
    completeOperationGuide(Player::OperationGuideItem::X);
    completeOperationGuide(Player::OperationGuideItem::R);
    // Keep item completion state, but hide the tutorial HUD throughout the boss battle.
    player->SetOperationGuideHudVisible(false);
}

void GameScene::UpdateLockOnTargetUI(float deltaTime) const
{
    if (!darkCameraActor ||
        darkCameraActor->GetMovementMode() != DarkCameraActor::CameraMode::LockOn ||
        !darkCameraActor->HasValidLockOnTarget())
    {
        lockOnTargetUIActor->HideMarker();
        return;
    }
    const auto target = darkCameraActor->GetEnemyHead();
    if (!target)
    {
        lockOnTargetUIActor->HideMarker();
        return;
    }
    lockOnTargetUIActor->UpdateMarkerPresentation(*darkCameraActor, target, target->GetComponentLocation(),
                                                  dynamic_cast<GruxEnemy*>(target->GetOwner()) != nullptr, deltaTime);
}

void GameScene::StartJudgeDodgeOperationTutorial()
{
    if (!tutorialController.BeginExplanation(IsPaused())) return;
    CoreAudio::PlayOneShot("./Data/Sound/SE/new_mission_start.wav");
    judgeDodgeOperationTutorialPreviousTimeScale = Time::timeScale;
    judgeDodgeOperationTutorialOwnsPause = true;
    SetPaused(true);
    Time::timeScale = 0.0f;
    tutorialUIActor->UpdateExplanationLayout();
    tutorialUIActor->SetExplanationVisible(true);
}
void GameScene::CloseJudgeDodgeOperationTutorial()
{
    if (tutorialController.GetExplanationState() != GameTutorialController::ExplanationState::Showing) return;
    CoreAudio::PlayOneShot("./Data/Sound/SE/button_push.wav", 0.5f);
    tutorialUIActor->SetExplanationVisible(false);
    const bool ringStartedInAttackRange = tutorialDodgeSkeletonActor && player &&
        tutorialDodgeSkeletonActor->IsPlayerWithinAttackRange(*player);
    tutorialController.CloseExplanation(ringStartedInAttackRange);
    if (judgeDodgeOperationTutorialOwnsPause)
    {
        SetPaused(false);
        Time::timeScale = judgeDodgeOperationTutorialPreviousTimeScale;
        judgeDodgeOperationTutorialOwnsPause = false;
    }
}
void GameScene::UpdateTutorialDodgeGuideRing()
{
    if (!tutorialUIActor) return;
    const bool invalidSkeleton = !tutorialDodgeSkeletonActor ||
        tutorialDodgeSkeletonActor->IsDead() ||
        tutorialDodgeSkeletonActor->IsPendingKill() ||
        (player && player->IsBossBattle());
    const auto action = tutorialController.UpdateRing(
        bossRoomLockOnScopeActive || tutorialBossEntryPending || tutorialController.IsCompletedForBossBattle(),
        invalidSkeleton, tutorialController.IsDodgeAttackRange(), Time::UnscaledDeltaTime());
    if (action == GameTutorialController::RingAction::Hide)
        tutorialUIActor->HideRing();
    else if (action == GameTutorialController::RingAction::Update)
        tutorialUIActor->UpdateRing(tutorialDodgeSkeletonActor->GetPosition(), Time::UnscaledDeltaTime());
}

void GameScene::UpdateTutorialSkeletonUI()
{
    if (tutorialController.IsCompletedForBossBattle())
    {
        HideTutorialSkeletonUI();
        return;
    }
    const auto activeCameraManager = GetCameraManager();
    const bool cameraRestricted = !activeCameraManager || activeCameraManager->IsUseDebug() ||
        activeCameraManager->IsUseCinematic() || activeCameraManager->IsUseMovie();
    const bool uiAllowed = player && player->GetHp() > 0 && !player->IsPendingKill() && darkCameraActor &&
        InputSystem::IsInputEnabled() && !cameraRestricted;
    if (!uiAllowed)
    {
        HideTutorialSkeletonUI();
        return;
    }

    if (tutorialPassiveSkeletonActor)
        tutorialPassiveSkeletonActor->UpdateHpBar(*darkCameraActor, player->GetPosition());
    if (tutorialDodgeSkeletonActor)
        tutorialDodgeSkeletonActor->UpdateHpBar(*darkCameraActor, player->GetPosition());
    const bool ltVisible = tutorialController.IsPassiveLockOnCandidate() &&
        player->IsOperationGuideItemVisible(Player::OperationGuideItem::LT) &&
        !player->IsOperationGuideItemLearned(Player::OperationGuideItem::LT);
    const bool passiveSkeletonAttackRange = tutorialPassiveSkeletonActor &&
        !tutorialPassiveSkeletonActor->IsDead() &&
        tutorialPassiveSkeletonActor->IsPlayerWithinAttackRange(*player);
    const bool yVisible = passiveSkeletonAttackRange &&
        player->IsOperationGuideItemVisible(Player::OperationGuideItem::Y) &&
        !player->IsOperationGuideItemLearned(Player::OperationGuideItem::Y);
    const bool xVisible = tutorialController.IsDodgeGuideActivated() && !tutorialController.HasDodgeSkeletonJustDodged() &&
        player->IsOperationGuideItemVisible(Player::OperationGuideItem::X);

    tutorialUIActor->UpdatePrompts(*darkCameraActor, tutorialPassiveSkeletonActor, tutorialDodgeSkeletonActor,
                                   ltVisible, yVisible, xVisible);
}

void GameScene::HideTutorialSkeletonUI() const
{
    if (tutorialPassiveSkeletonActor) tutorialPassiveSkeletonActor->HideHpBar();
    if (tutorialDodgeSkeletonActor) tutorialDodgeSkeletonActor->HideHpBar();
    if (tutorialUIActor) tutorialUIActor->HidePrompts();
}

void GameScene::UpdateBattleFlow()
{
    if (battleFlowState == BattleFlowState::FinalHitSlow ||
        battleFlowState == BattleFlowState::FinalHitSlowRecovery ||
        battleFlowState == BattleFlowState::FinalHitAftermath)
    {
        const auto controller = gruxEnemyActor ? gruxEnemyActor->GetBodyAnimationController() : nullptr;
        if (controller && !finalHitReaction.empty() &&
            controller->GetCurrentAnimationName() == finalHitReaction &&
            !controller->IsPlayAnimation())
        {
            if (!finalHitReactionCutReached)
            {
                finalHitReactionCutReached = true;
                phase2FinalHitReactionComplete = true;
                gruxEnemyActor->EndFinalHitReaction();
            }
        }
    }
    if (phase2FinalHitGateActive && IsPhase2FinalHitGateComplete())
    {
        EnterBossDead();
        return;
    }
    switch (battleFlowState)
    {
    case BattleFlowState::Intro:
        break;
    case BattleFlowState::Playing:
        if (bossPhase == BossPhase::TransitionToPhase2)
        {
            UpdatePhase2Transition();
            break;
        }
        if (phase1BreakPending)
        {
            UpdatePhase1FinalHitTimeSequence();
            UpdatePhase1BreakPending();
            battleTimerUIActor->SetElapsedTime(battleElapsedTime, numberTexWidth, numberTexHeight);
            break;
        }
        if (!IsPaused())
        {
            float timerDelta = Time::UnscaledDeltaTime();
            if (!Time::IsHitStopActive() && player && gruxEnemyActor)
            {
                const float slowScale = (std::min)(player->GetTimeScale(), gruxEnemyActor->GetTimeScale());
                if (std::isfinite(slowScale))
                    timerDelta *= std::clamp(slowScale, 0.0f, 1.0f);
            }
            battleElapsedTime += timerDelta;
        }
        if (gruxEnemyActor && gruxEnemyActor->GetHp() <= 0 && bossPhase == BossPhase::Phase1)
        {
            BeginPhase1BreakPending();
        }
        else if (gruxEnemyActor && gruxEnemyActor->GetHp() <= 0 && bossPhase == BossPhase::Phase2)
        {
            if (!finalBattleTimeSaved)
            {
                finalBattleTime = battleElapsedTime;
                finalBattleTimeSaved = true;
            }
            BeginPhase2FinalHitHpFallback();
        }
        else if (player && player->GetHp() <= 0)
        {
            CaptureContinueBossCheckpoint();
            EnterPlayerDead();
        }
        battleTimerUIActor->SetElapsedTime(battleElapsedTime, numberTexWidth, numberTexHeight);
        break;
    case BattleFlowState::FinalHitSlow:
        if (finalHitPending)
        {
            finalHitPending = false;
            break;
        }
        finalHitTimer -= Time::UnscaledDeltaTime();
        if (finalHitTimer <= 0.0f)
        {
            recoveryStartScale = Time::timeScale;
            currentRecoveryScale = recoveryStartScale;
            finalHitRecoveryElapsed = 0.0f;
            finalHitRecoveryProgress = 0.0f;
            finalHitRecoveryDurationActive = std::isfinite(finalHitSlowRecoveryDuration)
                                                 ? std::clamp(finalHitSlowRecoveryDuration, 0.0f, 1.0f) : 0.30f;
            finalHitTimer = 0.0f;
            Time::SetSlow(recoveryStartScale, 0.0f);
            battleFlowState = BattleFlowState::FinalHitSlowRecovery;
            if (finalHitRecoveryDurationActive <= 0.0f)
            {
                finalHitRecoveryProgress = 1.0f;
                currentRecoveryScale = 1.0f;
                Time::SetSlow(1.0f, 0.0f);
                finalHitTimer = finalHitAftermathDuration;
                battleFlowState = BattleFlowState::FinalHitAftermath;
            }
        }
        break;
    case BattleFlowState::FinalHitSlowRecovery:
        {
            finalHitRecoveryElapsed = (std::min)(finalHitRecoveryDurationActive,
                                                 finalHitRecoveryElapsed + Time::UnscaledDeltaTime());
            finalHitRecoveryProgress = finalHitRecoveryDurationActive > 0.0f
                                           ? std::clamp(finalHitRecoveryElapsed / finalHitRecoveryDurationActive, 0.0f, 1.0f)
                                           : 1.0f;
            const float t = finalHitRecoveryProgress;
            const float smoothT = t * t * (3.0f - 2.0f * t);
            currentRecoveryScale = std::lerp(recoveryStartScale, 1.0f, smoothT);
            Time::SetSlow(currentRecoveryScale, 0.0f);
            if (finalHitRecoveryProgress >= 1.0f)
            {
                currentRecoveryScale = 1.0f;
                Time::SetSlow(1.0f, 0.0f);
                finalHitTimer = finalHitAftermathDuration;
                battleFlowState = BattleFlowState::FinalHitAftermath;
            }
            break;
        }
    case BattleFlowState::FinalHitAftermath:
        finalHitTimer -= Time::UnscaledDeltaTime();
        if (finalHitTimer <= 0.0f)
        {
            const auto controller = gruxEnemyActor
                                        ? gruxEnemyActor->GetBodyAnimationController() : nullptr;
            if (!phase2FinalHitReactionComplete &&
                (!controller || finalHitReaction.empty() ||
                    controller->GetCurrentAnimationName() != finalHitReaction))
            {
                phase2FinalHitReactionComplete = true;
            }
            finalHitTimer = 0.0f;
        }
        break;
    case BattleFlowState::PlayerDead:
        if (playerDeathController->Update(Time::UnscaledDeltaTime(), true, player.get(), gruxEnemyActor.get(),
                                          *defeatUIActor, bgmDirector, cinemaCameraActor.get(), numberTexWidth, numberTexHeight, deathResultMoveSeVolume))
            battleFlowState = BattleFlowState::ContinueWait;
        break;
    case BattleFlowState::ContinueWait:
        playerDeathController->Update(Time::UnscaledDeltaTime(), false, player.get(), gruxEnemyActor.get(),
                                      *defeatUIActor, bgmDirector, cinemaCameraActor.get(), numberTexWidth, numberTexHeight, deathResultMoveSeVolume);
        break;
    case BattleFlowState::ResetForContinue:
        ResetBattleForContinue();
        break;
    case BattleFlowState::BossDead:
        if (bossDeathController->Update(GameBossDeathController::Context{ this, player.get(), gruxEnemyActor.get(), cinemaCameraActor.get(), cameraManager.get(), bossDeathUIActor.get(), bossDeathFadeOverlay.get(), &bgmDirector, &GetSceneSettings().sceneShaderConstants }, Time::UnscaledDeltaTime(), IsHuskComplete()))
            EnterVictoryResult();
        break;
    case BattleFlowState::Victory:
        UpdateVictoryResult();
        break;
    }
}


void GameScene::OnPlayerFinalHit(GruxEnemy* boss, const DirectX::XMFLOAT3& source)
{
    if (battleFlowState != BattleFlowState::Playing || boss != gruxEnemyActor.get())
        return;
    CoreAudio::PlayOneShot("./Data/Sound/SE/boss_last_hit.wav", 3.0f);
    if (bossPhase == BossPhase::Phase1)
    {
        StartPhase1FinalHitTimeSequence();
        BeginPhase1BreakPending();
        if (player)
        {
            const bool rushWasActive = player->IsRushActiveForPhaseTransition();
            player->RequestFinalHitVisualPoseLatch();
            player->RequestFinalHitRushFollowUpStop();
            if (rushWasActive)
                player->BeginBossSlowReturn(true);
        }
        boss->BeginPhase1LastHitReaction(source);
        return;
    }
    if (bossPhase == BossPhase::TransitionToPhase2)
        return;

    phase2FinalHitGateActive = true;
    phase2FinalHitReactionComplete = false;
    phase2FinalHitAttackWaitRequired = false;
    DisableCinematicCameraDebugInput();
    using namespace DirectX;
    XMFLOAT3 direction{ source.x - boss->GetPosition().x, 0.0f,
        source.z - boss->GetPosition().z };
    XMFLOAT3 local{};
    XMStoreFloat3(&local, XMVector3Rotate(XMLoadFloat3(&direction), XMQuaternionInverse(XMLoadFloat4(&boss->GetQuaternionRotation()))));

    std::string finalHitDirection = "Front";
    if (local.x * local.x + local.z * local.z < FLT_EPSILON)
        finalHitDirection = "Front";
    else if (std::abs(local.z) > std::abs(local.x))
        finalHitDirection = local.z > 0.0f ? "Front" : "Back";
    else
        finalHitDirection = local.x >= 0.0f ? "Right" : "Left";
    finalHitReaction = "HitReact_" + finalHitDirection;
    finalHitSlowScale = std::isfinite(finalHitSlowScale) ? std::clamp(finalHitSlowScale, 0.01f, 1.0f) : 0.20f;
    finalHitSlowDuration = std::isfinite(finalHitSlowDuration) ? std::clamp(finalHitSlowDuration, 0.01f, 5.0f) : 0.30f;
    finalHitAftermathDuration = std::isfinite(finalHitAftermathDuration) ? std::clamp(finalHitAftermathDuration, 0.0f, 2.0f) : 0.15f;
    finalHitRecoveryElapsed = 0.0f;
    finalHitRecoveryDurationActive = 0.0f;
    finalHitRecoveryProgress = 0.0f;
    recoveryStartScale = finalHitSlowScale;
    currentRecoveryScale = finalHitSlowScale;
    finalHitReactionCutReached = false;
    finalHitTimer = finalHitSlowDuration;
    finalHitPending = true;
    battleFlowState = BattleFlowState::FinalHitSlow;
    finalBattleTime = battleElapsedTime + Time::UnscaledDeltaTime();
    finalBattleTimeSaved = true;
    if (player)
        phase2FinalHitAttackWaitRequired = player->BeginFinalHitWait();
    boss->BeginFinalHitReaction(finalHitReaction);
    Time::SetSlow(finalHitSlowScale, finalHitSlowDuration);
}

void GameScene::EnterPlayerDead()
{
    DisableCinematicCameraDebugInput();
    battleFlowState = BattleFlowState::PlayerDead;
    bgmDirector.BeginPlayerDeathFade();
    battleTimerUIActor->SetVisible(false);
    playerDeathController->StartPresentation(battleElapsedTime, *defeatUIActor);
    defeatUIActor->ResetPresentation();
    InputSystem::SetInputEnabled(false);
    playerDeathController->ResetDeathCameraStart();
    playerDeathController->BeginHudFade(player.get(), gruxEnemyActor.get());

    if (gruxEnemyActor)
        gruxEnemyActor->PauseBattleAI();

    playerDeathController->StageDeathActors(player.get(), gruxEnemyActor.get());

    playerDeathController->PrepareDeathActor(player.get(), darkCameraActor.get(), [this]()
    {
        if (battleFlowState == BattleFlowState::PlayerDead)
            playerDeathController->StartDeathCamera(darkCameraActor.get(), gruxEnemyActor);
    });
}

void GameScene::EnterBossDead()
{
    if (bossPhase == BossPhase::Phase2)
    {
        if (!phase2FinalHitGateActive)
        {
            BeginPhase2FinalHitHpFallback();
            return;
        }
        if (!IsPhase2FinalHitGateComplete())
            return;
        phase2FinalHitGateActive = false;
        phase2FinalHitReactionComplete = false;
        phase2FinalHitAttackWaitRequired = false;
    }

    DisableCinematicCameraDebugInput();
    Time::SetSlow(1.0f, 0.0f);
    finalHitPending = false;
    finalHitTimer = 0.0f;
    if (gruxEnemyActor) gruxEnemyActor->EndFinalHitReaction();
    bossDeathController->CaptureGameplayDof(GetSceneSettings().sceneShaderConstants);

    battleFlowState = BattleFlowState::BossDead;
    battleTimerUIActor->SetVisible(false);
    SetBattleHudVisible(false);
    bossDeathController->Start(*bossDeathUIActor, bossDeathFadeOverlay.get());

    if (player)
    {
        player->StopLowHpPresentation();
        player->EnterWinState();
    }
    if (gruxEnemyActor)
    {
        gruxEnemyActor->StopBattleActions();
        if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
            controller->ResetAnimationRate();
    }
    if (darkCameraActor)
    {
        darkCameraActor->CancelOffscreenAttackAssist();
        darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);
    }
}

// 定数バッファの更新処理をシーンごとにカスタマイズできるようにするための仮想関数
void GameScene::SetBattleHudVisible(const bool visible) const
{
    if (player)
        player->SetHpBarVisible(visible);
    if (gruxEnemyActor)
        gruxEnemyActor->SetHpBarVisible(visible);
}

void GameScene::BeginPhase2FinalHitHpFallback()
{
    if (phase2FinalHitGateActive)
        return;

    phase2FinalHitGateActive = true;
    phase2FinalHitReactionComplete = true;
    phase2FinalHitAttackWaitRequired = player && player->BeginFinalHitWait();
}

bool GameScene::IsPhase2FinalHitGateComplete() const
{
    if (!phase2FinalHitGateActive)
        return false;
    const bool playerAttackComplete = !phase2FinalHitAttackWaitRequired ||
        !player || player->IsFinalHitAttackDrainComplete();
    return phase2FinalHitReactionComplete && playerAttackComplete;
}
void GameScene::BeginPhase2Transition()
{
    if (bossPhase != BossPhase::Phase1 || !player || !gruxEnemyActor)
        return;

    phase1FinalHitFadePending = true;
    InputSystem::SetInputEnabled(false);
    Time::SetSlow(1.0f, 0.0f);
    BeginPhase2Cinematic();
}

void GameScene::CompletePhase2TransitionAtBlack()
{
    ResetBossPhaseRuntime(BossPhase::TransitionToPhase2);
    if (gruxEnemyActor)
        gruxEnemyActor->BeginHpBarFadeOut();
    finalHitPending = false;
    InputSystem::SetInputEnabled(false);
    Time::SetSlow(1.0f, 0.0f);
    player->StartEvent();
    player->StopBattleActions();
    player->SetGameplayHudVisible(false);
    battleTimerUIActor->SetVisible(false);
    player->NeutralizeForPhase2Cinematic();
    gruxEnemyActor->PauseBattleAIForPhaseTransition();
    if (darkCameraActor)
    {
        darkCameraActor->CancelOffscreenAttackAssist();
        darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);
        darkCameraActor->SnapToTpsDirection(player->GetForward());
    }
    SetupPhase2RecallCinematic();
    phase1FinalHitFadePending = false;
    phase2RecallFadeAlpha = 1.0f;
    SetBossDeathFadeAlpha(1.0f);
    phase2TransitionStep = Phase2TransitionStep::RecallFadeIn;
    phase2StepElapsed = 0.0f;
}

void GameScene::UpdatePhase2Transition()
{
    UpdatePhase2Cinematic();
}
void GameScene::ResetBossPhaseRuntime(const BossPhase phase)
{
    if (auto* manager = GetCameraManager()) manager->ClearCameraShake();
    bossPhase = phase;
    phase1BreakPending = false;
    phase2BossRoarHpBarStarted = false;
    phase1BreakWaitingForRush = false;
    phase1FinalHitFadePending = false;
    phase1FinalHitTimePhase = Phase1FinalHitTimePhase::None;
    phase1FinalHitTimeElapsed = 0.0f;
    phase2FinalHitGateActive = false;
    phase2FinalHitReactionComplete = false;
    phase2FinalHitAttackWaitRequired = false;
    phase2TransitionStep = Phase2TransitionStep::None;
    phase2StepElapsed = 0.0f;
    phase2RecallFadeAlpha = 0.0f;
    SetBossDeathFadeAlpha(0.0f);
    phase2TransitionController->ResetTransform(phase == BossPhase::Phase2);
    if (phase != BossPhase::Phase2)
    {
        bgmDirector.ResetPhase2Runtime();
    }
    if (gruxEnemyActor)
    {
        gruxEnemyActor->SetPhase2CinematicAnimationOwnedExternally(false);
        gruxEnemyActor->SetHpBarFadeAlpha(1.0f);
        if (phase != BossPhase::TransitionToPhase2)
            gruxEnemyActor->SetHpBarVisualProfileForBattlePhase(phase == BossPhase::Phase2);
        phase2TransitionController->ApplyTransform(gruxEnemyActor.get());
    }
    phase2TransitionController->ResetPlayerEmote(player.get());
    if (player)
        player->ClearSwordTrailForPhaseTransition();
    if (darkCameraActor)
    {
        darkCameraActor->CancelExternalBlend();
        darkCameraActor->CancelOffscreenAttackAssist();
        darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);
    }
    if (phase == BossPhase::Phase1 || phase == BossPhase::Phase2)
        continueCheckpointPhase = phase;
}

void GameScene::ApplyBossPhaseHp(const BossPhase phase) const
{
    if (!gruxEnemyActor) return;
    switch (phase)
    {
    case BossPhase::Phase1:
        gruxEnemyActor->SetBattleHp(phase1MaxHp, phase1MaxHp);
        break;
    case BossPhase::Phase2:
        gruxEnemyActor->SetBattleHp(phase2MaxHp, phase2MaxHp);
        break;
    case BossPhase::TransitionToPhase2:
        break;
    }
}

void GameScene::BeginPhase1BreakPending()
{
    if (bossPhase != BossPhase::Phase1 || phase1BreakPending || !gruxEnemyActor)
        return;

    phase1BreakPending = true;
    InputSystem::SetInputEnabled(false);
    phase1BreakWaitingForRush = player && player->IsRushActiveForPhaseTransition();
    gruxEnemyActor->DisableAttackHitBoxes();
}

void GameScene::UpdatePhase1BreakPending()
{
    if (!phase1BreakPending)
        return;

    if (phase1FinalHitFadePending)
    {
        UpdatePhase2Transition();
        return;
    }

    phase1BreakWaitingForRush = player && player->IsRushActiveForPhaseTransition();
    if (phase1BreakWaitingForRush || !player ||
        !player->IsPhase1LastHitAttackFinishedForTransition() ||
        !gruxEnemyActor || !gruxEnemyActor->IsDelayedHpSettled())
        return;

    BeginPhase2Transition();
}

void GameScene::StartPhase1FinalHitTimeSequence()
{
    phase1FinalHitTimePhase = Phase1FinalHitTimePhase::HitStop;
    phase1FinalHitTimeElapsed = 0.0f;
    Time::SetSlow(0.0f, 0.3f);
}

void GameScene::UpdatePhase1FinalHitTimeSequence()
{
    if (phase1FinalHitTimePhase != Phase1FinalHitTimePhase::HitStop)
        return;

    const float frameUnscaledDelta = Time::UnscaledDeltaTime();
    const float configuredDuration = (std::max)(0.0f, phase1FinalHitHitStopDuration);
    phase1FinalHitTimeElapsed += frameUnscaledDelta;
    if (phase1FinalHitTimeElapsed < configuredDuration)
        return;

    phase1FinalHitTimePhase = Phase1FinalHitTimePhase::Slow;
    const float slowScale = std::clamp(phase1FinalHitSlowScale, 0.01f, 1.0f);
    Time::SetSlow(slowScale, 0.0f);
}


void GameScene::BeginPhase2Cinematic()
{
    phase2TransitionStep = Phase2TransitionStep::RecallFadeOut;
    phase2StepElapsed = 0.0f;
    phase2RecallFadeAlpha = 0.0f;
    phase2TransitionController->ClearPlayerEmote();
    SetBossDeathFadeAlpha(0.0f);
}

void GameScene::SetupPhase2RecallCinematic() const
{
    if (player)
    {
        if (const auto controller = player->GetBodyAnimationController())
            controller->ReleaseLatchedVisualPose();
        player->NeutralizeForPhase2Cinematic();
    }
    if (gruxEnemyActor)
    {
        gruxEnemyActor->ClearDamageVisualsForPhaseTransition();
    }

    phase2TransitionController->ApplyRecallActorPose(player.get(), gruxEnemyActor.get());

    if (cameraManager->IsUseDebug()) cameraManager->ToggleCamera(this);
    if (cameraManager->IsUseMovie()) cameraManager->ToggleMovieCamera(this);
    if (!cameraManager->IsUseCinematic()) cameraManager->ToggleCinematicCamera(this);
    phase2TransitionController->CutToShot(0, cinemaCameraActor.get());

    if (gruxEnemyActor)
        gruxEnemyActor->ForcePhase2CinematicIdleImmediate();
    if (player)
        player->PlayBodyAnimation("Idle", true, true, 0.0f, true);

}

void GameScene::UpdatePhase2Cinematic()
{
    if (bossPhase != BossPhase::TransitionToPhase2 && !phase1FinalHitFadePending)
        return;

    const bool keepPhase2BossIdle =
        phase2TransitionStep == Phase2TransitionStep::RecallFadeIn ||
        phase2TransitionStep == Phase2TransitionStep::RecallPreWait ||
        phase2TransitionStep == Phase2TransitionStep::PlayerEmotePreWait ||
        phase2TransitionStep == Phase2TransitionStep::PlayerEmote ||
        phase2TransitionStep == Phase2TransitionStep::PlayerEmotePostWait ||
        phase2TransitionStep == Phase2TransitionStep::ReturnToTps;
    if (keepPhase2BossIdle && gruxEnemyActor)
    {
        const auto controller = gruxEnemyActor->GetBodyAnimationController();
        if (controller && controller->GetCurrentAnimationName() != "TravelMode_Idle_0")
        {
            gruxEnemyActor->ForcePhase2CinematicIdleImmediate();
        }
    }
    InputSystem::SetInputEnabled(false);

    switch (phase2TransitionStep)
    {
    case Phase2TransitionStep::RecallFadeOut:
        {
            const float duration = std::clamp(phase2RecallFadeOutDuration, 0.0f, 2.0f);
            phase2StepElapsed = duration <= 0.001f ? duration :
                                    (std::min)(duration, phase2StepElapsed + Time::UnscaledDeltaTime());
            phase2RecallFadeAlpha = duration <= 0.001f ? 1.0f :
                                        std::clamp(phase2StepElapsed / duration, 0.0f, 1.0f);
            SetBossDeathFadeAlpha(phase2RecallFadeAlpha);
            if (gruxEnemyActor)
                gruxEnemyActor->SetHpBarFadeAlpha(1.0f - phase2RecallFadeAlpha);
            if (phase2RecallFadeAlpha < 1.0f)
                break;

            CompletePhase2TransitionAtBlack();
            break;
        }

    case Phase2TransitionStep::RecallFadeIn:
        {
            const float duration = std::clamp(phase2RecallFadeInDuration, 0.0f, 2.0f);
            phase2StepElapsed = duration <= 0.001f ? duration :
                                    (std::min)(duration, phase2StepElapsed + Time::UnscaledDeltaTime());
            phase2RecallFadeAlpha = duration <= 0.001f ? 0.0f :
                                        1.0f - std::clamp(phase2StepElapsed / duration, 0.0f, 1.0f);
            SetBossDeathFadeAlpha(phase2RecallFadeAlpha);
            if (phase2RecallFadeAlpha > 0.0f)
                break;

            phase2RecallFadeAlpha = 0.0f;
            SetBossDeathFadeAlpha(0.0f);
            phase2TransitionStep = Phase2TransitionStep::RecallPreWait;
            phase2StepElapsed = 0.0f;
            break;
        }
    case Phase2TransitionStep::RecallPreWait:
        {
            if (const auto controller = gruxEnemyActor ? gruxEnemyActor->GetBodyAnimationController() : nullptr)
                controller->OnUpdate(Time::UnscaledDeltaTime());

            const float waitDuration = std::clamp(phase2RecallPreWaitDuration, 0.0f, 3.0f);
            phase2StepElapsed = (std::min)(waitDuration,
                                           phase2StepElapsed + Time::UnscaledDeltaTime());
            if (phase2StepElapsed < waitDuration)
                break;

            if (gruxEnemyActor)
                gruxEnemyActor->PlayPhase2CinematicBodyAnimation("PrimaryAttack_Recall", false, true, 0.1f, true);
            phase2TransitionStep = Phase2TransitionStep::BossRecall;
            phase2StepElapsed = 0.0f;
            break;
        }

    case Phase2TransitionStep::BossRecall:
        {
            if (!phase2TransitionController->UpdateRecall(gruxEnemyActor.get(), bgmDirector, bossBgmVolume))
                break;
            if (gruxEnemyActor)
                gruxEnemyActor->ForcePhase2CinematicIdleImmediate();

            if (player && gruxEnemyActor)
            {
                DirectX::XMFLOAT3 toBoss = MathHelper::Subtract(gruxEnemyActor->GetPosition(), player->GetPosition());
                toBoss.y = 0.0f;
                if (MathHelper::Length(toBoss) > FLT_EPSILON)
                {
                    player->ForceDirectionImmediate(MathHelper::Normalize(toBoss));
                    player->UpdateAllComponentTransforms();
                }
            }
            phase2TransitionStep = Phase2TransitionStep::PlayerEmotePreWait;
            phase2StepElapsed = 0.0f;
            phase2TransitionController->CutToShot(1, cinemaCameraActor.get());
            phase2TransitionController->ClearPlayerEmote();
            if (player)
                player->PlayBodyAnimation("Idle", true, true, 0.1f, true);
            break;
        }

    case Phase2TransitionStep::PlayerEmotePreWait:
        {
            if (const auto controller = player ? player->GetBodyAnimationController() : nullptr)
                controller->OnUpdate(Time::UnscaledDeltaTime());

            const float waitDuration = std::clamp(phase2PlayerEmotePreWaitDuration, 0.0f, 3.0f);
            phase2StepElapsed = (std::min)(waitDuration,
                                           phase2StepElapsed + Time::UnscaledDeltaTime());
            if (phase2StepElapsed < waitDuration)
                break;

            phase2TransitionController->BeginPlayerEmote(player.get());
            phase2TransitionStep = Phase2TransitionStep::PlayerEmote;
            phase2StepElapsed = 0.0f;
            break;
        }

    case Phase2TransitionStep::PlayerEmote:
        {
            if (!phase2TransitionController->UpdatePlayerEmote(player.get()))
                break;

            phase2TransitionStep = Phase2TransitionStep::PlayerEmotePostWait;
            phase2StepElapsed = 0.0f;
            break;
        }

    case Phase2TransitionStep::PlayerEmotePostWait:
        {
            // No controller update or animation request while the Emote_Win endpoint pose is held.
            const float waitDuration = std::clamp(phase2PlayerEmotePostWaitDuration, 0.0f, 3.0f);
            phase2StepElapsed = (std::min)(waitDuration,
                                           phase2StepElapsed + Time::UnscaledDeltaTime());
            if (phase2StepElapsed < waitDuration)
                break;

            // Starting Idle releases SetPlaybackRange only after the hold has completed.
            phase2TransitionController->FinishPlayerEmote(player.get());
            BeginPhase2TpsReturnBlend();
            break;
        }

    case Phase2TransitionStep::ReturnToTps:
    case Phase2TransitionStep::None:
        break;
    }
}

void GameScene::BeginPhase2TpsReturnBlend()
{
    InputSystem::SetInputEnabled(false);
    phase2TransitionStep = Phase2TransitionStep::ReturnToTps;
    auto finish = [this]()
    {
        phase2TransitionStep = Phase2TransitionStep::None;
        bossPhase = BossPhase::Phase2;
        if (gruxEnemyActor)
            gruxEnemyActor->CompletePhase2HpBarPresentation();
        ApplyBossPhaseHp(BossPhase::Phase2);
        CaptureContinueBossCheckpoint();
        if (player)
        {
            player->ResumeBattleActionsAfterEvent();
            player->EndEvent();
        }
        if (gruxEnemyActor)
        {
            gruxEnemyActor->ResetTimeScale();
            gruxEnemyActor->ResumeBattleAI();
        }
        battleTimerUIActor->SetVisible(true);
        battleTimerUIActor->SetElapsedTime(battleElapsedTime, numberTexWidth, numberTexHeight);
        SetBattleHudVisible(true);
        if (player)
            player->SetGameplayHudVisible(true);
        InputSystem::SetInputEnabled(true);
    };

    if (!darkCameraActor || !cinemaCameraActor)
    {
        finish();
        return;
    }

    DarkCameraActor::CameraPose start{};
    start.eye = cinemaCameraActor->GetPosition();
    const DirectX::XMFLOAT3 forward = cinemaCameraActor->GetForward();
    start.target = MathHelper::Add(start.eye, MathHelper::Multiply(forward, 5.0f));
    start.yaw = std::atan2(forward.x, forward.z);
    start.pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
    const auto target = darkCameraActor->CreateFocusPose();
    if (cameraManager->IsUseCinematic()) cameraManager->ToggleCinematicCamera(this);
    darkCameraActor->StartExternalBlend(start, target, phase2TpsReturnBlendDuration, finish);
}

void GameScene::CaptureContinueBossCheckpoint()
{
    if (!gruxEnemyActor ||
        (bossPhase != BossPhase::Phase1 && bossPhase != BossPhase::Phase2))
        return;

    continueCheckpointPhase = bossPhase;
    continueCheckpointBossHp = gruxEnemyActor->GetHp();
    continueCheckpointBossMaxHp = gruxEnemyActor->GetMaxHp();
}

void GameScene::ResetBattleForContinue()
{
    DisableCinematicCameraDebugInput();
    if (!battleStartTransformsSaved || !player || !gruxEnemyActor)
        return;

    playerDeathController->ResetPresentationElapsed();
    defeatUIActor->ResetPresentation();

    Time::SetSlow(1.0f, 0.0f);

    if (cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(this);
    if (cameraManager->IsUseCinematic())
        cameraManager->ToggleCinematicCamera(this);
    if (darkCameraActor)
    {
        darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);
        darkCameraActor->ClearDeathTarget();
    }

    const BossPhase resumePhase = continueCheckpointPhase;
    const auto resumeBgmPhase = resumePhase == BossPhase::Phase2 ? GameBgmDirector::BossBgmPhase::Phase2 :
                                    resumePhase == BossPhase::Phase1 ? GameBgmDirector::BossBgmPhase::Phase1 : GameBgmDirector::BossBgmPhase::Transition;
    bgmDirector.ResetDeathBgmState(resumeBgmPhase, bossBgmVolume);
    ResetBossPhaseRuntime(resumePhase);
    player->ResetForBattleContinue(playerBattleStartTransform);
    player->EndEvent();
    player->SetDeathCameraStartCallback(nullptr);
    player->SetDeathCameraTransparencyDisabled(false);
    gruxEnemyActor->ResetForBattleContinue(bossBattleStartTransform);
    phase2TransitionController->ApplyTransform(gruxEnemyActor.get());
    gruxEnemyActor->SetBattleHp(continueCheckpointBossHp, continueCheckpointBossMaxHp);
    ResetBattleFacingAndCamera();
    gruxEnemyActor->ResumeBattleAI();

    SetBattleHudVisible(true);
    player->SetGameplayHudVisible(true);
    battleTimerUIActor->SetVisible(true);
    battleTimerUIActor->SetElapsedTime(battleElapsedTime, numberTexWidth, numberTexHeight);
    // 入力を受け付ける
    InputSystem::SetInputEnabled(true);
    battleFlowState = BattleFlowState::Playing;
}


void GameScene::ResetBattleFacingAndCamera() const
{
    if (!player || !gruxEnemyActor)
        return;

    DirectX::XMFLOAT3 toBoss = MathHelper::Subtract(
        gruxEnemyActor->GetPosition(), player->GetPosition());
    toBoss.y = 0.0f;
    if (MathHelper::Length(toBoss) <= FLT_EPSILON)
        return;

    toBoss = MathHelper::Normalize(toBoss);
    player->ForceDirectionImmediate(toBoss);
    player->UpdateAllComponentTransforms();
    if (darkCameraActor)
        darkCameraActor->SnapToTpsDirection(toBoss);
}

void GameScene::RestartBossBattle()
{
    if (darkCameraActor)
        darkCameraActor->ClearDeathTarget();
    bgmDirector.ResetDeathBgmState(GameBgmDirector::BossBgmPhase::Phase1, bossBgmVolume);
    victoryUIActor->ResetPresentation();
    victoryController->ResetPresentation();
    ResetHuskCompletionTracking();
    DisableCinematicCameraDebugInput();
    playerDeathController->ResetPresentationElapsed();
    defeatUIActor->ResetPresentation();
    GetUIManager()->SetSelected(nullptr);
    player->SetDeathCameraStartCallback(nullptr);
    player->SetDeathCameraTransparencyDisabled(false);
    finalHitPending = false;
    finalHitTimer = 0.0f;
    finalHitRecoveryElapsed = 0.0f;
    finalHitRecoveryDurationActive = 0.0f;
    finalHitRecoveryProgress = 0.0f;
    recoveryStartScale = 1.0f;
    currentRecoveryScale = 1.0f;
    finalHitReactionCutReached = false;
    finalHitReaction.clear();
    Time::SetSlow(1.0f, 0.0f);
    bossDeathController->StopCinematicAudio();

    if (const auto controller = player->GetBodyAnimationController())
    {
        controller->ReleaseHeldAnimationPose();
        controller->ResetAnimationRate();
    }
    if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
    {
        controller->ReleaseHeldAnimationPose();
        controller->ResetAnimationRate();
    }

    ResetBossPhaseRuntime(BossPhase::Phase1);
    player->ClearTransientBattleActions();
    player->ResetForBattleContinue(playerBattleStartTransform);
    player->EndEvent();
    gruxEnemyActor->ResetForBattleRestart(bossBattleStartTransform);
    phase2TransitionController->ApplyTransform(gruxEnemyActor.get());
    ApplyBossPhaseHp(BossPhase::Phase1);
    CaptureContinueBossCheckpoint();
    ResetBattleFacingAndCamera();
    gruxEnemyActor->ResumeBattleAI();

    if (const auto playerCapsule = std::dynamic_pointer_cast<ShapeComponent>(
        player->FindComponentByName("capsuleComponent")))
    {
        playerCapsule->EnableCollision();
    }
    if (const auto bossCapsule = std::dynamic_pointer_cast<ShapeComponent>(
        gruxEnemyActor->FindComponentByName("enemyCapsuleComponent")))
    {
        bossCapsule->EnableCollision();
    }
    if (const auto bossMesh = gruxEnemyActor->GetSkeletalMeshComponent())
        bossMesh->SetIsVisible(true);

    if (cinemaCameraActor)
    {
        if (auto* camera = dynamic_cast<CinematicCameraComponent*>(
            cinemaCameraActor->GetCameraComponent()))
        {
            CinematicCameraComponent::CameraPose currentPose{};
            currentPose.position = cinemaCameraActor->GetPosition();
            currentPose.rotation = cinemaCameraActor->GetQuaternionRotation();
            currentPose.fov = camera->GetFov();
            camera->CutToPose(currentPose);
        }
    }
    if (cameraManager->IsUseCinematic())
        cameraManager->ToggleCinematicCamera(this);
    if (cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(this);
    if (cameraManager->IsUseDebug())
        cameraManager->ToggleCamera(this);
    bossDeathController->Reset(GetSceneSettings().sceneShaderConstants, *bossDeathUIActor, bossDeathFadeOverlay.get());

    gruxHuskCaptured = false;
    gruxHuskCaptureRequested = false;
    gruxHuskPreviewCaptureRequested = false;
    gruxHuskPlaybackActive = false;
    gruxHuskBackupValid = false;
    gruxHuskDeathProgress = 0.0f;
    if (huskParticles)
    {
        huskParticles->particle_data.death_progress = 0.0f;
        huskParticles->particle_data.particle_count = 0;
    }

    StartBossBattle();
}

void GameScene::ExecuteDeathResult(const int index)
{
    if (!playerDeathController->AcceptSelection(*defeatUIActor, deathResultConfirmSeVolume)) return;
    if (index == 0) ResetBattleForContinue();
    else if (index == 1) RestartBossBattle();
    else SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", "TitleScene") }, TransitionStyle::Fade);
}

void GameScene::ExecuteVictoryResult(const int selection)
{
    if (!victoryController->CanAcceptSelection(*victoryUIActor)) return;
    const bool restartAllowed = selection != 0 || (battleStartTransformsSaved && player && gruxEnemyActor);
    if (!victoryController->AcceptSelection(selection, restartAllowed, *victoryUIActor, deathResultConfirmSeVolume)) return;
    if (selection == 0)
        RestartBossBattle();
    else
        SceneTransitionManager::Instance().RequestTransition("LoadingScene",
                                                             { std::make_pair("preload", "TitleScene") }, TransitionStyle::Fade);
}

void GameScene::EnterVictoryResult()
{
    if (battleFlowState != BattleFlowState::BossDead || !bossDeathController->AreShotsLoaded() ||
        !cinemaCameraActor || !dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()))
        return;

    victoryUIActor->ResetPresentation();
    GetUIManager()->SetNavigationEnabled(false);
    GetUIManager()->SetSelected(nullptr);

    defeatUIActor->ResetPresentation();
    victoryController->CaptureResult(finalBattleTime, finalBattleTimeSaved, *victoryUIActor);

    DisableCinematicCameraDebugInput();
    Time::SetSlow(1.0f, 0.0f);

    if (cameraManager->IsUseDebug()) cameraManager->ToggleCamera(this);
    if (cameraManager->IsUseMovie()) cameraManager->ToggleMovieCamera(this);
    if (!cameraManager->IsUseCinematic()) cameraManager->ToggleCinematicCamera(this);
    auto* camera = dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent());
    const float blendDuration = victoryController->GetVictoryCameraBlend();
    bossDeathController->BlendToResult(*camera, GetSceneSettings().sceneShaderConstants, blendDuration);
    // playerの勝利アニメーションを再生する
    victoryController->PreparePlayer(player.get());
    victoryController->BeginPresentation();
    battleFlowState = BattleFlowState::Victory;
}

void GameScene::UpdateVictoryResult()
{
    DisableCinematicCameraDebugInput();
    const int selection = victoryController->UpdateUntilSelection(player.get(), cinemaCameraActor.get(),
                                                                  *victoryUIActor, deathResultMoveSeVolume);
    if (selection >= 0)
        ExecuteVictoryResult(selection);
    victoryController->FinishUpdate(*victoryUIActor, numberTexWidth, numberTexHeight);
}

void GameScene::DisableCinematicCameraDebugInput() const
{
    if (!cinemaCameraActor) return;
    if (auto* camera = dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()))
        camera->SetDebugInputEnabled(false);
}

void GameScene::UpdateMovieSkip()
{
    const GameMovieSkipUIActor::MovieSkipType activeMovieSkipType = GetActiveMovieSkipType();
    movieSkipUIActor->SetActiveMovieType(activeMovieSkipType);
    movieSkipUIActor->UpdatePresentation();
    if (movieSkipUIActor->ConsumeSkipRequest())
    {
        if (activeMovieSkipType == GameMovieSkipUIActor::MovieSkipType::PrisonOpening || activeMovieSkipType == GameMovieSkipUIActor::MovieSkipType::BossEntry)
        {
            if (const auto movieManager = GetActorManager()->GetActorOfType<MovieCameraManagerActor>())
            {
                if (activeMovieSkipType == GameMovieSkipUIActor::MovieSkipType::PrisonOpening)
                    movieManager->SkipPrisonOpeningToGameplay();
                else
                    movieManager->SkipBossEntryToBattle();
            }
            movieSkipUIActor->ResetPresentation();
        }
        else if (activeMovieSkipType == GameMovieSkipUIActor::MovieSkipType::BossDeath)
        {
            SkipBossDeathToFinishPrompt();
        }
    }
}

GameMovieSkipUIActor::MovieSkipType GameScene::GetActiveMovieSkipType()
{
    if (battleFlowState == BattleFlowState::BossDead && bossDeathController->CanSkip())
    {
        return GameMovieSkipUIActor::MovieSkipType::BossDeath;
    }

    const auto movieManager = GetActorManager()->GetActorOfType<MovieCameraManagerActor>();
    if (!movieManager)
        return GameMovieSkipUIActor::MovieSkipType::None;
    if (movieManager->IsPrisonOpeningMovieActive())
        return GameMovieSkipUIActor::MovieSkipType::PrisonOpening;
    if (movieManager->IsBossEntryMovieActive())
        return GameMovieSkipUIActor::MovieSkipType::BossEntry;
    return GameMovieSkipUIActor::MovieSkipType::None;
}

void GameScene::SetBossDeathFadeAlpha(const float alpha) const
{
    if (!bossDeathFadeOverlay)
        return;

    const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
    bossDeathFadeOverlay->SetColor(CoreColor{ 0.0f, 0.0f, 0.0f, clampedAlpha });
    bossDeathFadeOverlay->SetVisible(clampedAlpha > 0.0f);
}

#ifdef USE_IMGUI
void GameScene::DrawTutorialDebugGui()
{
    ImGui::SeparatorText("Tutorial Camera Profile Trigger");
    ImGui::DragFloat3("Corridor Exit Trigger Center", &tutorialCameraProfileTriggerCenter.x,
        0.05f, -100.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat3("Corridor Exit Trigger Size", &tutorialCameraProfileTriggerSize.x,
        0.05f, 0.05f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
}

void GameScene::DrawPhase2DebugGui()
{
    ImGui::DragInt(U8("Phase1 MaxHP"), &phase1MaxHp, 1.0f, 1, 500);
    ImGui::DragInt(U8("Phase2 MaxHP"), &phase2MaxHp, 1.0f, 1, 500);
    ImGui::DragFloat(U8("Phase2 FadeOut時間"), &phase2RecallFadeOutDuration,
        0.01f, 0.0f, 2.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(U8("Phase2 FadeIn時間"), &phase2RecallFadeInDuration,
        0.01f, 0.0f, 2.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(U8("Phase2 Recall\u524DWait"), &phase2RecallPreWaitDuration,
        0.01f, 0.0f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    float phase1BossScale = gruxEnemyActor ? gruxEnemyActor->GetPhase1BossScale() : 1.7f;
    float phase2BossScale = gruxEnemyActor ? gruxEnemyActor->GetPhase2BossScale() : 2.1f;
    const bool phase1ScaleChanged = ImGui::DragFloat(U8("Phase1 Boss Scale"), &phase1BossScale,
        0.01f, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    const bool phase2ScaleChanged = ImGui::DragFloat(U8("Phase2 Boss Scale"), &phase2BossScale,
        0.01f, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    if (gruxEnemyActor && (phase1ScaleChanged || phase2ScaleChanged))
        gruxEnemyActor->SetPhaseScaleRange(phase1BossScale, phase2BossScale);
    phase2TransitionController->DrawRecallDebugGui(gruxEnemyActor.get());
    if (gruxEnemyActor)
    {
        float phase2MaskMinYOffset = gruxEnemyActor->GetPhase2MaskMinYOffset();
        float phase2MaskMaxYOffset = gruxEnemyActor->GetPhase2MaskMaxYOffset();
        float phase2MaskSoftness = gruxEnemyActor->GetPhase2MaskSoftness();
        const bool phase2MaskOffsetChanged =
            ImGui::DragFloat(U8("Phase2 Mask MinY Offset"), &phase2MaskMinYOffset,
                0.01f, -2.0f, 5.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp) |
            ImGui::DragFloat(U8("Phase2 Mask MaxY Offset"), &phase2MaskMaxYOffset,
                0.01f, -2.0f, 5.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        if (phase2MaskOffsetChanged)
            gruxEnemyActor->SetPhase2MaskOffsets(phase2MaskMinYOffset, phase2MaskMaxYOffset);
        if (ImGui::DragFloat(U8("Phase2 Mask Softness"), &phase2MaskSoftness,
            0.005f, 0.0f, 0.3f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
        {
            gruxEnemyActor->SetPhase2MaskSoftness(phase2MaskSoftness);
        }
    }

    bgmDirector.DrawPhase2DebugGui();
    ImGui::DragFloat(U8("Phase2 Emote\u524DWait"), &phase2PlayerEmotePreWaitDuration,
        0.01f, 0.0f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    phase2TransitionController->DrawPlayerEmoteDebugGui();
    ImGui::DragFloat(U8("Phase2 Emote\u5F8CWait"), &phase2PlayerEmotePostWaitDuration,
        0.01f, 0.0f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
}

void GameScene::DrawBgmAndBattleTimerDebugGui()
{
    bgmDirector.DrawGameBgmDebugGui();
    ImGui::Separator();
    ImGui::Text("Boss Battle Timer");
    ImGui::Text("Battle Time: %.3f", battleElapsedTime);
    ImGui::Text("Final Battle Time: %.3f%s", finalBattleTime,
        finalBattleTimeSaved ? "" : " (not saved)");
    battleTimerUIActor->SetElapsedTime(battleElapsedTime, numberTexWidth, numberTexHeight);
    ImGui::Separator();
}

void GameScene::DrawBossDeathDebugGui()
{
    if (ImGui::TreeNode("Boss Death Cinematic Preview"))
    {
        ImGui::Separator();
        ImGui::DragFloat("Final Hit Slow Scale", &finalHitSlowScale, 0.01f, 0.01f, 1.0f);
        ImGui::DragFloat("Final Hit Slow Duration", &finalHitSlowDuration, 0.01f, 0.01f, 2.0f);
        ImGui::DragFloat("Final Hit Slow Recovery Duration", &finalHitSlowRecoveryDuration,
            0.01f, 0.0f, 1.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Final Hit Aftermath Duration", &finalHitAftermathDuration, 0.01f, 0.0f, 2.0f);

        ImGui::Separator();
        ImGui::SeparatorText("Victory Camera Intro");
        victoryController->DrawCameraIntroDebugGui();
        victoryController->DrawAnimationDebugGui();
        victoryController->DrawRankDebugGui();
        victoryUIActor->SetNumberTextureSize(numberTexWidth, numberTexHeight);
        victoryController->DrawDelayDebugGui();
        bossDeathController->DrawDebugGui(player.get(), gruxEnemyActor.get());
        ImGui::TreePop();
    }
}

void GameScene::DrawPlayerDeathDebugGui()
{
    ImGui::Separator();
    ImGui::Text("Death Presentation");
    playerDeathController->DrawDebugGui(deathResultMoveSeVolume, deathResultConfirmSeVolume,
        numberTexWidth, numberTexHeight, battleElapsedTime);
}

#endif


// ボスの部屋の色ラープ値を設定する
void GameScene::SetBossRoomLerpFactor(float lerpFactor)
{
    auto& shader = Scene::GetCurrentScene()->GetSceneSettings().sceneShaderConstants;
    shader.bossRoomLerpFactor = lerpFactor;
}


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

// ボスの部屋の色のラープを開始する関数
void GameScene::CancelBossRoomLerp()
{
    startBossRoomLerp = false;
    onFinished = nullptr;
    if (bossLerpEasing)
        bossLerpEasing->Clear();
}
