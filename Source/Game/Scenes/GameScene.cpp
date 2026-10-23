#include "pch.h"
#include "GameScene.h"
#include "Game/DarkGame/GameBossDeathController.h"
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
#include "Engine/Audio/Audio.h"
#include "Core/ActorManager.h"
#include "Engine/Camera/MovieCameraManagerActor.h"
#include "Engine/Debug/SceneEditor.h"
#include "Engine/Debug/DebugRender.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Camera/DarkGameCamera.h"

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

namespace
{
    constexpr float BossBgmVolume = 0.2f;

    constexpr std::array<const char*, 2> Phase2CinematicPresetPaths = {
        "Data/Saves/ScenePresets/boss_recall.json",
        "Data/Saves/ScenePresets/boss_recall_player.json",
    };

}

GameScene::GameScene() = default;
GameScene::~GameScene() = default;

bool GameScene::Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
{
    PROFILE_FUNCTION();

    prisonOpeningMovieStartRequested = false;
    backToRoomStartRequested = false;
    if (const auto entryIt = props.find("gameEntry"); entryIt != props.end())
    {
        prisonOpeningMovieStartRequested = entryIt->second == "NewGame";
        backToRoomStartRequested = entryIt->second == "BackToRoom";
    }
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
            //stageCandelabraAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Candelabra/Candelabra.gltf", ModelTypes::ModelMode::StaticMesh, false, true);
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
        lockOnTargetUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameLockOnTargetUIActor>("LockOnTargetUI");
        CreateTutorialSkeletonUI();
        tutorialUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameTutorialUIActor>("TutorialUI");
        battleTimerUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameTimerUIActor>("BattleTimerUI");
        defeatUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameDefeatUIActor>("DefeatUI");
        defeatUIActor->SetSelectionCallback([this](int index) { ExecuteDeathResult(index); });
        LoadVictoryBestTime();
        victoryUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameVictoryUIActor>("VictoryUI");
        CreateBossDeathFadeUI();
        bossDeathUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameBossDeathUIActor>("BossDeathUI");
        movieSkipUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameMovieSkipUIActor>("MovieSkipUI");
        bossDeathController = std::make_unique<GameBossDeathController>();
        bossDeathController->LoadBossDeathShots();
        phase2CinematicShotsLoaded = LoadPhase2CinematicShots();
    }



    // ここで布を描画する
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


int GameScene::GetBossCurrentHp() const
{
    return gruxEnemyActor ? gruxEnemyActor->GetHp() : 0;
}

int GameScene::GetBossMaxHp() const
{
    return gruxEnemyActor ? gruxEnemyActor->GetMaxHp() : 0;
}

void GameScene::ApplyBossPhaseHp(const BossPhase phase)
{
    if (!gruxEnemyActor) return;
    switch (phase)
    {
    case BossPhase::Phase1: gruxEnemyActor->SetBattleHp(phase1MaxHp, phase1MaxHp); break;
    case BossPhase::Phase2: gruxEnemyActor->SetBattleHp(phase2MaxHp, phase2MaxHp); break;
    case BossPhase::TransitionToPhase2: break;
    }
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
    if (phase == BossPhase::Phase2)
    {
        phase2TransformProgress = 1.0f;
        phase2TransformCompleted = true;
    }
    else
    {
        phase2TransformProgress = 0.0f;
        phase2TransformCompleted = false;
    }
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
        gruxEnemyActor->SetPhaseTransformProgress(phase2TransformProgress);
        gruxEnemyActor->HideLockOnVisualsForPhaseTransition();
    }
    if ((phase2PlayerEmoteActive || phase2PlayerEmoteEndPoseHeld) && player)
        player->PlayBodyAnimation("Idle", true, true, 0.0f, true);
    phase2PlayerEmoteActive = false;
    phase2PlayerEmoteEndPoseHeld = false;
    if (player)
        player->ClearSwordTrailForPhaseTransition();
    phase2TpsReturnBlendActive = false;
    phase2CurrentShot = "None";
    if (darkCameraActor)
    {
        darkCameraActor->CancelExternalBlend();
        darkCameraActor->CancelOffscreenAttackAssist();
        darkCameraActor->SetRequestMode(DarkCameraActor::CameraMode::TPS);
    }
    // Future Phase2 movie/color/scale/attack/weapon-drop runtime resets belong here.
    if (phase == BossPhase::Phase1 || phase == BossPhase::Phase2)
        continueCheckpointPhase = phase;
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
void GameScene::BeginPhase1BreakPending()
{
    if (bossPhase != BossPhase::Phase1 || phase1BreakPending || !gruxEnemyActor)
        return;

    phase1BreakPending = true;
    InputSystem::SetInputEnabled(false);
    phase1BreakWaitingForRush = player && player->IsRushActiveForPhaseTransition();
    // Existing Rush/reaction animation is allowed to finish, but it cannot deal
    // damage or open a new danger window while the phase break is pending.
    gruxEnemyActor->DisableAttackHitBoxes();
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

bool GameScene::LoadPhase2CinematicShots()
{
    try
    {
        for (size_t shotIndex = 0; shotIndex < Phase2CinematicPresetPaths.size(); ++shotIndex)
        {
            SceneState sceneState{};
            SceneEditor::LoadSceneState(Phase2CinematicPresetPaths[shotIndex], sceneState);
            const auto findActor = [&sceneState](const char* actorName)
                {
                    return std::find_if(sceneState.actorStates.begin(), sceneState.actorStates.end(),
                        [actorName](const ActorTransformState& actor) { return actor.name == actorName; });
                };
            const auto playerState = findActor("Player");
            const auto bossState = findActor("GruxEnemy");
            if (playerState == sceneState.actorStates.end() || bossState == sceneState.actorStates.end())
                return false;

            auto& shot = phase2CinematicShots[shotIndex];
            shot.camera = { sceneState.camera.position, sceneState.camera.rotation, sceneState.camera.fov };
            shot.player = { playerState->position, playerState->rotation };
            shot.boss = { bossState->position, bossState->rotation };
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        Logger::Error(Logger::LogCategory::System,
            std::format("Failed to load Phase2 cinematic presets: {}", exception.what()).c_str());
        return false;
    }
}

void GameScene::ApplyPhase2RecallActorPose()
{
    if (!player || !gruxEnemyActor || !phase2CinematicShotsLoaded)
        return;

    const auto& shot = phase2CinematicShots[0];
    gruxEnemyActor->SetPosition(shot.boss.position);
    gruxEnemyActor->SetQuaternionRotation(shot.boss.rotation);
    player->SetPosition(shot.player.position);
    player->SetQuaternionRotation(shot.player.rotation);
    // StopBattleActions has already cleared forced movement and velocity.  Sync
    // both component hierarchies immediately so no stale rotation restores pose.
    gruxEnemyActor->UpdateAllComponentTransforms();
    player->UpdateAllComponentTransforms();
}

void GameScene::CutToPhase2Shot(const size_t shotIndex)
{
    if (shotIndex >= phase2CinematicShots.size() || !cinemaCameraActor)
        return;
    if (auto* camera = dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()))
        camera->CutToPose(phase2CinematicShots[shotIndex].camera);
}

void GameScene::BeginPhase2Cinematic()
{
    // Keep gameplay visible during the fade-out.  The cinematic pose and camera
    // are installed only after the screen has become completely black.
    phase2TransitionStep = Phase2TransitionStep::RecallFadeOut;
    phase2StepElapsed = 0.0f;
    phase2RecallFadeAlpha = 0.0f;
    phase2PlayerEmoteActive = false;
    phase2PlayerEmoteEndPoseHeld = false;
    phase2CurrentShot = "Gameplay Fade Out";
    SetBossDeathFadeAlpha(0.0f);
}

void GameScene::SetupPhase2RecallCinematic()
{
    if (player)
    {
        // A second visual cleanup under black prevents any one-frame residue
        // between the initial combat abort and the actor teleport.
        if (const auto controller = player->GetBodyAnimationController())
            controller->ReleaseLatchedVisualPose();
        player->NeutralizeForPhase2Cinematic();
    }
    if (gruxEnemyActor)
    {
        gruxEnemyActor->ClearDamageVisualsForPhaseTransition();
        gruxEnemyActor->HideLockOnVisualsForPhaseTransition();
    }

    ApplyPhase2RecallActorPose();

    if (cameraManager->IsUseDebug()) cameraManager->ToggleCamera(this);
    if (cameraManager->IsUseMovie()) cameraManager->ToggleMovieCamera(this);
    if (!cameraManager->IsUseCinematic()) cameraManager->ToggleCinematicCamera(this);
    CutToPhase2Shot(0);

    if (gruxEnemyActor)
        gruxEnemyActor->ForcePhase2CinematicIdleImmediate(
            "GameScene::Phase2RecallFadeSetupImmediate");
    if (player)
        player->PlayBodyAnimation("Idle", true, true, 0.0f, true);

    phase2CurrentShot = "boss_recall";
}
void GameScene::BeginPhase2Transition()
{
    if (bossPhase != BossPhase::Phase1 || !player || !gruxEnemyActor)
        return;

    // Keep the completed Player/Boss presentation untouched while RecallFadeOut begins.
    phase1FinalHitFadePending = true;
    InputSystem::SetInputEnabled(false);
    Time::SetSlow(1.0f, 0.0f);
    BeginPhase2Cinematic();
}
void GameScene::CompletePhase2TransitionAtBlack()
{
    // All stateful/visual cleanup is intentionally deferred until alpha reaches 1.
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
    gruxEnemyActor->HideLockOnVisualsForPhaseTransition();
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
void GameScene::BeginPhase2TpsReturnBlend()
{
    InputSystem::SetInputEnabled(false);
    phase2TransitionStep = Phase2TransitionStep::ReturnToTps;
    phase2TpsReturnBlendActive = true;
    phase2CurrentShot = "TPS Return Blend";

    auto finish = [this]()
        {
            phase2TpsReturnBlendActive = false;
            phase2TransitionStep = Phase2TransitionStep::None;
            phase2CurrentShot = "None";
            bossPhase = BossPhase::Phase2;
            if (gruxEnemyActor)
                gruxEnemyActor->CompletePhase2HpBarPresentation();
            ApplyBossPhaseHp(BossPhase::Phase2); // Single, UI-hookable Phase2 HP handoff point.
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
            gruxEnemyActor->ForcePhase2CinematicIdleImmediate(
                "GameScene::Phase2PlayerShotIdleGuard");
        }
    }
    // Transition ownership is exclusive until the TPS blend completion callback.
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
            gruxEnemyActor->PlayBodyAnimation("PrimaryAttack_Recall", false, true, 0.1f, true,
                "GameScene::Phase2BossRecall");
        phase2TransitionStep = Phase2TransitionStep::BossRecall;
        phase2StepElapsed = 0.0f;
        break;
    }

    case Phase2TransitionStep::BossRecall:
    {
        const auto controller = gruxEnemyActor ? gruxEnemyActor->GetBodyAnimationController() : nullptr;
        if (controller)
            controller->OnUpdate(Time::UnscaledDeltaTime());

        const bool isRecallAnimation = controller &&
            controller->GetCurrentAnimationName() == "PrimaryAttack_Recall";
        if (isRecallAnimation && !phase2TransformCompleted)
        {
            const float recallAnimationTime = controller->GetCurrentAnimationTime();
            const float transformDuration = phase2TransformEndTime - phase2TransformStartTime;
            if (transformDuration <= 0.001f)
            {
                if (recallAnimationTime >= phase2TransformStartTime)
                {
                    phase2TransformProgress = 1.0f;
                    phase2TransformCompleted = true;
                }
                else
                {
                    phase2TransformProgress = 0.0f;
                }
            }
            else
            {
                phase2TransformProgress = std::clamp(
                    (recallAnimationTime - phase2TransformStartTime) / transformDuration,
                    0.0f, 1.0f);
                if (recallAnimationTime >= phase2TransformEndTime)
                {
                    phase2TransformProgress = 1.0f;
                    phase2TransformCompleted = true;
                }
            }
        }
        else if (isRecallAnimation)
        {
            // Completion is one-shot; ImGui tuning must not regress it during this Recall.
            phase2TransformProgress = 1.0f;
        }

        if (isRecallAnimation && gruxEnemyActor)
        {
            gruxEnemyActor->SetPhaseTransformProgress(phase2TransformProgress);
        }
        if (isRecallAnimation && phase2TransformCompleted && !bgmDirector.IsPhase2Started())
        {
            bgmDirector.BeginPhase2CrossFade(controller->GetCurrentAnimationTime(), BossBgmVolume);
        }

        const bool recallFinished = !controller ||
            (isRecallAnimation && !controller->IsPlayAnimation());
        if (!recallFinished)
            break;

        // A shortened Recall must never leave the eventual Phase2 presentation partial.
        phase2TransformProgress = 1.0f;
        phase2TransformCompleted = true;
        if (!bgmDirector.IsPhase2Started())
            bgmDirector.BeginPhase2CrossFade(controller ? controller->GetCurrentAnimationTime() : phase2TransformEndTime, BossBgmVolume);
        if (gruxEnemyActor)
            gruxEnemyActor->SetPhaseTransformProgress(phase2TransformProgress);
        if (gruxEnemyActor)
            gruxEnemyActor->ForcePhase2CinematicIdleImmediate(
                "GameScene::Phase2BossRecallCompleteImmediate");


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
        phase2CurrentShot = "boss_recall_player";
        CutToPhase2Shot(1);
        phase2PlayerEmoteActive = false;
        phase2PlayerEmoteEndPoseHeld = false;
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

        phase2PlayerEmoteActive = false;
        phase2PlayerEmoteEndPoseHeld = false;
        if (const auto controller = player ? player->GetBodyAnimationController() : nullptr)
        {
            player->PlayBodyAnimation("Emote_Phase2", false, true, 0.2f, true);
            phase2PlayerEmoteActive = controller->SetPlaybackRange(
                0.0f, std::clamp(phase2PlayerEmoteEndTime, 0.0f,
                    controller->GetAnimationLength("Emote_Phase2")));
        }
        phase2TransitionStep = Phase2TransitionStep::PlayerEmote;
        phase2StepElapsed = 0.0f;
        break;
    }

    case Phase2TransitionStep::PlayerEmote:
    {
        const auto controller = player ? player->GetBodyAnimationController() : nullptr;
        if (controller)
            controller->OnUpdate(Time::UnscaledDeltaTime());

        const bool emoteFinished = !controller || !phase2PlayerEmoteActive ||
            (controller->GetCurrentAnimationName() == "Emote_Phase2" && !controller->IsPlayAnimation());
        if (!emoteFinished)
            break;

        // Do not request Idle here: the playback-range endpoint is the held pose.
        phase2PlayerEmoteActive = false;
        phase2PlayerEmoteEndPoseHeld = controller &&
            controller->GetCurrentAnimationName() == "Emote_Phase2";
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
        if (player)
            player->PlayBodyAnimation("Idle", true, true, 0.2f, true);
        phase2PlayerEmoteEndPoseHeld = false;
        BeginPhase2TpsReturnBlend();
        break;
    }

    case Phase2TransitionStep::ReturnToTps:
    case Phase2TransitionStep::None:
        break;
    }
}
void GameScene::UpdatePhase2Transition()
{
    UpdatePhase2Cinematic();
}

void GameScene::Start()
{
    battleFlowState = BattleFlowState::Intro;
    battleStartTransformsSaved = false;
    tutorialBossEntryPending = false;
    tutorialController.ResetForSceneStart();
    if (player)
        player->SetMinimumHp(5);
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
    //bossBgmActor->Play();
    bossBgmActor->SetVolume(BossBgmVolume);

    phase2BgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("BossPhase2BgmActor");
    phase2BgmActor->SetSource(L"./Data/Sound/BGM/boss_phase2_bgm1.wav");
    phase2BgmActor->SetLoop(true);
    phase2BgmActor->SetBgm(true);
    phase2BgmActor->SetVolume(0.0f);

    bossDeathSecondBgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("BossDeathSecondBgmActor");
    bossDeathSecondBgmActor->SetSource(L"./Data/Sound/BGM/boss_death_second_bgm.wav");
    bossDeathSecondBgmActor->SetLoop(true);
    bossDeathSecondBgmActor->SetBgm(true);
    bossDeathSecondBgmActor->SetVolume(0.45f);

    playerDeathBgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("PlayerDeathBgmActor");
    playerDeathBgmActor->SetSource(L"./Data/Sound/BGM/player_death_bgm_1.wav");
    playerDeathBgmActor->SetLoop(true);
    playerDeathBgmActor->SetBgm(true);
    playerDeathBgmActor->SetVolume(0.35f);
    bgmDirector.SetActors(gameBgmActor, bossBgmActor, phase2BgmActor, bossDeathSecondBgmActor, playerDeathBgmActor);

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
                // Resolve the active scene at callback time so no destroyed GameScene is captured.
                auto* gameScene = dynamic_cast<GameScene*>(Scene::GetCurrentScene());
                if (!gameScene || !gameScene->prisonOpeningMovieStartRequested)
                    return;

                // This request is one-shot even if playback cannot begin.
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

void GameScene::BeginGameBgmFadeOut()
{
    if (battleFlowState != BattleFlowState::Intro) return;
    bgmDirector.BeginGameFade();
}

void GameScene::PlayBossBattleBgm()
{
    bgmDirector.PlayBossBattleBgm();
}

void GameScene::CreateTutorialSkeletonUI()
{
    const auto uiManager = GetUIManager();
    if (tutorialPassiveSkeletonActor) tutorialPassiveSkeletonActor->CreateHpBarUI(uiManager, "TutorialPassiveSkeletonHp");
    if (tutorialDodgeSkeletonActor) tutorialDodgeSkeletonActor->CreateHpBarUI(uiManager, "TutorialDodgeSkeletonHp");
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
    if (judgeDodgeOperationTutorialOwnsPause) { SetPaused(false); Time::timeScale = judgeDodgeOperationTutorialPreviousTimeScale; judgeDodgeOperationTutorialOwnsPause = false; }
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

void GameScene::HideTutorialSkeletonUI()
{
    if (tutorialPassiveSkeletonActor) tutorialPassiveSkeletonActor->HideHpBar();
    if (tutorialDodgeSkeletonActor) tutorialDodgeSkeletonActor->HideHpBar();
    if (tutorialUIActor) tutorialUIActor->HidePrompts();
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
void GameScene::UpdateLockOnTargetUI(float deltaTime)
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
void GameScene::Update(float deltaTime)
{
    using namespace DirectX;

    ZoneScopedN("Game Update");
    bgmDirector.UpdateGameBgmFadeOut(Time::UnscaledDeltaTime());
    bgmDirector.UpdateDeathBgmFade(Time::UnscaledDeltaTime());
    bgmDirector.UpdatePhase2BgmCrossFade(Time::UnscaledDeltaTime(), BossBgmVolume);

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

    defeatUIActor->SetFrameData(deathPresentationElapsed, deathOverlayStartTime, deathDefeatedStartTime,
        deathBattleTimeStartTime, deathButtonsStartTime, deathSelectLineStartTime,
        numberTexWidth, numberTexHeight, deathResultMoveSeVolume);
    UpdateBattleFlow();
    victoryUIActor->UpdateButtonLayout();
    defeatUIActor->UpdateLayout();

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

#endif // 0




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

void GameScene::DisableCinematicCameraDebugInput()
{
    if (!cinemaCameraActor) return;
    if (auto* camera = dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()))
        camera->SetDebugInputEnabled(false);
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

void GameScene::SpawnTutorialSkeletons()
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

void GameScene::RequestReturnToMainRoomStart()
{
    SceneTransitionManager::Instance().RequestTransition(
        "LoadingScene",
        { std::make_pair("preload", "GameScene"), std::make_pair("gameEntry", "BackToRoom") },
        TransitionStyle::Fade);
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

bool GameScene::BeginPrisonOpeningMoviePlayerSitting()
{
    return player && player->BeginPrisonOpeningSitting(
        prisonOpeningSittingPosition, prisonOpeningSittingYawDegrees);
}

void GameScene::EndPrisonOpeningMoviePlayerSitting()
{
    if (player)
        player->EndPrisonOpeningSitting();
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
void GameScene::StartBossBattle()
{
    EnterBossRoomLockOnScope(false);
    victoryUIActor->ResetPresentation();
    victoryResultPhase = VictoryResultPhase::None;
    victoryResultDelayElapsed = 0.0f;
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
        bgmDirector.ResetBossBattleBgm(GameBgmDirector::BossBgmPhase::Phase1, true, BossBgmVolume);
    else
        PlayBossBattleBgm();
    bgmDirector.StopGameBgmIfNotFading();
    ApplyBossPhaseHp(BossPhase::Phase1);
    CaptureContinueBossCheckpoint();
    player->EndEvent();
    player->ClearMinimumHp();
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

void GameScene::EnterPlayerDead()
{
    DisableCinematicCameraDebugInput();
    battleFlowState = BattleFlowState::PlayerDead;
    bgmDirector.BeginPlayerDeathFade();
    battleTimerUIActor->SetVisible(false);
    playerDeadElapsed = 0.0f;
    deathPresentationElapsed = 0.0f;
    deathAttemptTime = battleElapsedTime;
    defeatUIActor->SetBattleTime(deathAttemptTime);
    ResetDeathPresentationCues();
    defeatUIActor->ResetPresentation();
    InputSystem::SetInputEnabled(false);
    deathCameraStartRequested = false;
    if (player)
        player->BeginGameplayHudFadeOut();
    if (gruxEnemyActor)
        gruxEnemyActor->BeginHpBarFadeOut();
    FireDeathPresentationCue(DeathPresentationCue::GameplayHudFade, deathHudFadeCueFired);

    if (gruxEnemyActor)
        gruxEnemyActor->PauseBattleAI();

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

    const float stagingMinX = (std::min)(deathStagingMinPlayerX, deathStagingMaxPlayerX);
    const float stagingMaxX = (std::max)(deathStagingMinPlayerX, deathStagingMaxPlayerX);
    const float stagingMinZ = (std::min)(deathStagingMinPlayerZ, deathStagingMaxPlayerZ);
    const float stagingMaxZ = (std::max)(deathStagingMinPlayerZ, deathStagingMaxPlayerZ);
    const float wallInsetX = std::clamp(deathStagingWallInsetX, 0.0f, (stagingMaxX - stagingMinX) * 0.5f);
    const float wallInsetZ = std::clamp(deathStagingWallInsetZ, 0.0f, (stagingMaxZ - stagingMinZ) * 0.5f);
    const float safeMinX = stagingMinX + wallInsetX;
    const float safeMaxX = stagingMaxX - wallInsetX;
    const float safeMinZ = stagingMinZ + wallInsetZ;
    const float safeMaxZ = stagingMaxZ - wallInsetZ;

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

    defeatUIActor->SetFrameData(deathPresentationElapsed, deathOverlayStartTime, deathDefeatedStartTime,
        deathBattleTimeStartTime, deathButtonsStartTime, deathSelectLineStartTime,
        numberTexWidth, numberTexHeight, deathResultMoveSeVolume);
    if (!defeatUIActor->IsVisible())
    {
        bgmDirector.PlayPlayerDeathBgm();
        DisableCinematicCameraDebugInput();
        defeatUIActor->SetVisible(true);
    }
    defeatUIActor->UpdatePresentation();

    if (deathPresentationElapsed >= deathOverlayStartTime) FireDeathPresentationCue(DeathPresentationCue::OverlayFade, deathOverlayCueFired);
    if (deathPresentationElapsed >= deathDefeatedStartTime) FireDeathPresentationCue(DeathPresentationCue::DefeatedFade, deathDefeatedCueFired);
    if (deathPresentationElapsed >= deathBattleTimeStartTime) FireDeathPresentationCue(DeathPresentationCue::BattleTimeFade, deathBattleTimeCueFired);
    if (deathPresentationElapsed >= deathButtonsStartTime) FireDeathPresentationCue(DeathPresentationCue::ButtonsFade, deathButtonsCueFired);
}

void GameScene::ExecuteDeathResult(const int index)
{
    if (!defeatUIActor->IsInputEnabled()) return;
    CoreAudio::PlayOneShot("./Data/Sound/SE/button_push.wav", deathResultConfirmSeVolume);
    defeatUIActor->ResetPresentation();
    if (index == 0) ResetBattleForContinue();
    else if (index == 1) RestartBossBattle();
    else SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", "TitleScene") }, TransitionStyle::Fade);
}
void GameScene::OnPlayerDeathCameraStart()
{
    if (deathCameraStartRequested || battleFlowState != BattleFlowState::PlayerDead)
        return;

    deathCameraStartRequested = true;
    if (darkCameraActor)
        darkCameraActor->StartDeathMode(gruxEnemyActor, nullptr);
}

void GameScene::ResetBattleFacingAndCamera()
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

void GameScene::ResetBattleForContinue()
{
    DisableCinematicCameraDebugInput();
    if (!battleStartTransformsSaved || !player || !gruxEnemyActor)
        return;

    deathPresentationElapsed = 0.0f;
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
    bgmDirector.ResetDeathBgmState(resumeBgmPhase, BossBgmVolume);
    ResetBossPhaseRuntime(resumePhase);
    player->ResetForBattleContinue(playerBattleStartTransform);
    player->EndEvent();
    player->SetDeathCameraStartCallback(nullptr);
    player->SetDeathCameraTransparencyDisabled(false);
    gruxEnemyActor->ResetForBattleContinue(bossBattleStartTransform);
    gruxEnemyActor->SetPhaseTransformProgress(phase2TransformProgress);
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
    bossDeathLastHpZeroDetectionSource = "FinalHit callback";
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

bool GameScene::IsPhase2FinalHitGateComplete() const
{
    if (!phase2FinalHitGateActive)
        return false;
    const bool playerAttackComplete = !phase2FinalHitAttackWaitRequired ||
        !player || player->IsFinalHitAttackDrainComplete();
    return phase2FinalHitReactionComplete && playerAttackComplete;
}

void GameScene::BeginPhase2FinalHitHpFallback()
{
    if (phase2FinalHitGateActive)
        return;

    phase2FinalHitGateActive = true;
    phase2FinalHitReactionComplete = true;
    phase2FinalHitAttackWaitRequired = player && player->BeginFinalHitWait();
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

    ++bossDeathEnterCallCount;
    bossDeathLastEnterPreviousFlow = battleFlowState;
    DisableCinematicCameraDebugInput();
    Time::SetSlow(1.0f, 0.0f);
    finalHitPending = false;
    finalHitTimer = 0.0f;
    if (gruxEnemyActor) gruxEnemyActor->EndFinalHitReaction();
    bossDeathController->CaptureGameplayDof(GetSceneSettings().sceneShaderConstants);

    battleFlowState = BattleFlowState::BossDead;
    battleTimerUIActor->SetVisible(false);
    SetBattleHudVisible(false);
    bossDeathController->Start(*bossDeathUIActor, *bossDeathFadeOverlay);

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


void GameScene::RestartBossBattle()
{
    if (darkCameraActor)
        darkCameraActor->ClearDeathTarget();
    bgmDirector.ResetDeathBgmState(GameBgmDirector::BossBgmPhase::Phase1, BossBgmVolume);
    victoryUIActor->ResetPresentation();
    victoryResultPhase = VictoryResultPhase::None;
    victoryResultDelayElapsed = 0.0f;
    ResetHuskCompletionTracking();
    DisableCinematicCameraDebugInput();
    deathPresentationElapsed = 0.0f;
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
    gruxEnemyActor->SetPhaseTransformProgress(phase2TransformProgress);
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
    bossDeathController->Reset(GetSceneSettings().sceneShaderConstants, *bossDeathUIActor, *bossDeathFadeOverlay);

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

void GameScene::SkipBossDeathToFinishPrompt()
{
    if (battleFlowState == BattleFlowState::BossDead &&
        bossDeathController->SkipToFinishPrompt(GameBossDeathController::Context{ this, player.get(), gruxEnemyActor.get(), cinemaCameraActor.get(), cameraManager.get(), bossDeathUIActor.get(), bossDeathFadeOverlay.get(), &bgmDirector, &GetSceneSettings().sceneShaderConstants }))
        movieSkipUIActor->ResetPresentation();
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

void GameScene::CreateBossDeathFadeUI()
{
    bossDeathFadeOverlay = std::make_shared<UIImageComponent>("BossDeathFadeOverlay");
    bossDeathFadeOverlay->SetSize({ 1920.0f, 1080.0f });
    bossDeathFadeOverlay->SetWorldPosition({ 0.0f, 0.0f });
    bossDeathFadeOverlay->SetPivot({ 0.0f, 0.0f });
    bossDeathFadeOverlay->SetColor(CoreColor{ 0.0f, 0.0f, 0.0f, 0.0f });
    bossDeathFadeOverlay->SetVisible(false);
    bossDeathFadeOverlay->SetEnable(false);
    bossDeathFadeOverlay->zOrder = 1000;
    GetUIManager()->Add(bossDeathFadeOverlay);
}

void GameScene::SetBossDeathFadeAlpha(const float alpha)
{
    if (!bossDeathFadeOverlay)
        return;

    const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
    bossDeathFadeOverlay->SetColor(CoreColor{ 0.0f, 0.0f, 0.0f, clampedAlpha });
    bossDeathFadeOverlay->SetVisible(clampedAlpha > 0.0f);
}

void GameScene::LoadVictoryBestTime()
{
    const char* path = "Data/Saves/BossBestTime.json";
    try
    {
        if (!std::filesystem::exists(path)) return;
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Cannot open boss best time");
        nlohmann::json data;
        input >> data;
        const double value = data.at("bossBestTimeSeconds").get<double>();
        if (!std::isfinite(value) || value < 0.0)
            throw std::runtime_error("Invalid boss best time");
        victoryBestTime = value;
        victoryBestTimeValid = true;
    }
    catch (const std::exception& error)
    {
        Logger::Warning(Logger::LogCategory::System,
            (std::string("Boss best time load failed: ") + error.what()).c_str());
    }
}

void GameScene::CaptureVictoryResult()
{
    const bool validClear = finalBattleTimeSaved && std::isfinite(finalBattleTime) && finalBattleTime >= 0.0f;
    const double victoryClearTime = validClear ? static_cast<double>(finalBattleTime) : 0.0;
    const bool victoryIsNewRecord = validClear && (!victoryBestTimeValid || victoryClearTime < victoryBestTime);
    if (victoryIsNewRecord)
    {
        victoryBestTime = victoryClearTime;
        victoryBestTimeValid = true;
        try
        {
            std::filesystem::create_directories("Data/Saves");
            const char* temporaryPath = "Data/Saves/BossBestTime.json.tmp";
            {
                std::ofstream output(temporaryPath, std::ios::trunc);
                output.exceptions(std::ios::badbit | std::ios::failbit);
                output << nlohmann::json{ { "bossBestTimeSeconds", victoryBestTime } }.dump(4);
                output.close();
            }
            if (!MoveFileExA(temporaryPath, "Data/Saves/BossBestTime.json",
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Cannot replace boss best time save");
        }
        catch (const std::exception& error)
        {
            Logger::Warning(Logger::LogCategory::System,
                (std::string("Boss best time save failed: ") + error.what()).c_str());
        }
    }
    const double victoryDisplayedBestTime = victoryBestTimeValid ? victoryBestTime : 0.0;
    const float s = std::isfinite(victoryRankSLimit) ? (std::max)(0.0f, victoryRankSLimit) : 60.0f;
    const float a = std::isfinite(victoryRankALimit) ? (std::max)(s, victoryRankALimit) : (std::max)(s, 90.0f);
    const float b = std::isfinite(victoryRankBLimit) ? (std::max)(a, victoryRankBLimit) : (std::max)(a, 120.0f);
    const GameVictoryUIActor::VictoryRank victoryRank = victoryClearTime <= s ? GameVictoryUIActor::VictoryRank::S : victoryClearTime <= a ? GameVictoryUIActor::VictoryRank::A : victoryClearTime <= b ? GameVictoryUIActor::VictoryRank::B : GameVictoryUIActor::VictoryRank::C;
    victoryUIActor->SetResultData(victoryClearTime, victoryDisplayedBestTime, victoryIsNewRecord, victoryRank);
}

void GameScene::ExecuteVictoryResult(const int selection)
{
    if (!victoryUIActor->IsInputEnabled() || victoryResultPhase != VictoryResultPhase::Interactive) return;
    if (selection == 0 && (!battleStartTransformsSaved || !player || !gruxEnemyActor)) return;
    victoryUIActor->DisableInput();
    CoreAudio::PlayOneShot("./Data/Sound/SE/button_push.wav", deathResultConfirmSeVolume);
    if (selection == 0)
        RestartBossBattle();
    else
        SceneTransitionManager::Instance().RequestTransition("LoadingScene",
            { std::make_pair("preload", "TitleScene") }, TransitionStyle::Fade);
}

void GameScene::EnterVictoryResult()
{
    if (battleFlowState != BattleFlowState::BossDead || !bossDeathController->AreShotsLoaded() ||
        !cinemaCameraActor ||
        !dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()))
        return;

    victoryUIActor->ResetPresentation();
    GetUIManager()->SetNavigationEnabled(false);
    GetUIManager()->SetSelected(nullptr);
    defeatUIActor->ResetPresentation();
    CaptureVictoryResult();
    DisableCinematicCameraDebugInput();
    Time::SetSlow(1.0f, 0.0f);
    if (cameraManager->IsUseDebug()) cameraManager->ToggleCamera(this);
    if (cameraManager->IsUseMovie()) cameraManager->ToggleMovieCamera(this);
    if (!cameraManager->IsUseCinematic()) cameraManager->ToggleCinematicCamera(this);
    auto* camera = dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent());
    const float blendDuration = std::isfinite(victoryCameraBlendDuration)
        ? std::clamp(victoryCameraBlendDuration, 0.0f, 2.0f) : 0.60f;
    bossDeathController->BlendToResult(*camera, GetSceneSettings().sceneShaderConstants, blendDuration);
    if (const auto controller = player ? player->GetBodyAnimationController() : nullptr)
    {
        controller->ReleaseHeldAnimationPose(true);
        controller->ResetAnimationRate();
        const float idleBlend = std::isfinite(resultRecallToIdleBlendDuration)
            ? std::clamp(resultRecallToIdleBlendDuration, 0.0f, 0.5f) : 0.15f;
        player->PlayBodyAnimation("Idle", true, idleBlend > 0.0f, idleBlend, true);
    }
    battleFlowState = BattleFlowState::Victory;
    victoryResultDelayElapsed = 0.0f;
    victoryResultPhase = VictoryResultPhase::CameraBlend;
}

void GameScene::UpdateVictoryResult()
{
    DisableCinematicCameraDebugInput();
    const auto controller = player ? player->GetBodyAnimationController() : nullptr;
    auto* camera = cinemaCameraActor
        ? dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()) : nullptr;
    switch (victoryResultPhase)
    {
    case VictoryResultPhase::CameraBlend:
        if (camera && !camera->IsBlending() && controller)
        {
            controller->ResetAnimationRate();
            player->PlayBodyAnimation("Emote_Win", false, true, 0.2f, true);
            controller->SetPlaybackRange(0.0f, victoryEmoteEndTime);
            victoryResultPhase = VictoryResultPhase::WinEmote;
        }
        break;
    case VictoryResultPhase::WinEmote:
        if (!victoryUIActor->IsBackgroundStarted() && controller &&
            controller->GetCurrentAnimationName() == "Emote_Win")
        {
            const float triggerTime = std::isfinite(resultUITriggerAnimationTime)
                ? std::clamp(resultUITriggerAnimationTime, 0.0f, victoryEmoteEndTime) : 1.17f;
            if (controller->GetCurrentAnimationTime() >= triggerTime)
            {
                victoryUIActor->StartBackgroundFade();
            }
        }
        if (controller && controller->GetCurrentAnimationName() == "Emote_Win" &&
            controller->GetCurrentAnimationTime() >= victoryEmoteEndTime)
        {
            player->PlayBodyAnimation("Idle", true, true, 0.2f, true);
            victoryResultDelayElapsed = 0.0f;
            victoryResultPhase = VictoryResultPhase::ResultDelay;
        }
        break;
    case VictoryResultPhase::ResultDelay:
    {
        const float duration = std::isfinite(victoryResultDelayDuration)
            ? std::clamp(victoryResultDelayDuration, 0.0f, 2.0f) : 0.70f;
        victoryResultDelayElapsed = (std::min)(duration,
            victoryResultDelayElapsed + (std::max)(0.0f, Time::UnscaledDeltaTime()));
        if (victoryResultDelayElapsed >= duration)
        {
            victoryUIActor->ShowButtons();
            victoryResultPhase = VictoryResultPhase::ShowButtons;
        }
        break;
    }
    case VictoryResultPhase::ShowButtons:
    {
        if (victoryUIActor->AdvanceButtonFade())
        {
            victoryResultPhase = VictoryResultPhase::Interactive;
            victoryUIActor->EnableInput();
        }
        break;
    }
    case VictoryResultPhase::Interactive:
        if (const int selection = victoryUIActor->UpdateInput(deathResultMoveSeVolume); selection >= 0)
            ExecuteVictoryResult(selection);
        break;
    default:
        break;
    }
    victoryUIActor->SetNumberTextureSize(numberTexWidth, numberTexHeight);
    victoryUIActor->UpdateBackground();
    victoryUIActor->UpdateButtonLayout();
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
            // Diagnostic only: this is the direct HP<=0 fallback route.
            bossDeathLastHpZeroDetectionSource = "UpdateBattleFlow fallback";
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
        playerDeadElapsed += Time::UnscaledDeltaTime();
        deathPresentationElapsed += Time::UnscaledDeltaTime();
        UpdateDeathResultPresentation();
        if (!defeatUIActor->IsInputEnabled() && deathPresentationElapsed >= deathResultInputEnableTime)
        {
            DisableCinematicCameraDebugInput();
            defeatUIActor->EnableInput();
            InputSystem::SetInputEnabled(true);
            battleFlowState = BattleFlowState::ContinueWait;
        }
        break;
    case BattleFlowState::ContinueWait:
        deathPresentationElapsed += Time::UnscaledDeltaTime();
        UpdateDeathResultPresentation();
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

void GameScene::SetUpActors()
{
    {
        PROFILE_SCOPE("Create Player");
        Transform playerTr = mainRoomStartPlayerTransform;
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

    Transform debrisTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    this->GetActorManager()->CreateAndRegisterActorWithTransform<ModelDebrisEmitterActor>(
        "ModelDebrisEmitterActor", debrisTr);

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
#endif // 0
#if 0
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

    Transform GruxEnemyTr(DirectX::XMFLOAT3{ 7.69f,0.0f,11.0f }, DirectX::XMFLOAT3{ 0.0f,-90.0f,0.0f }, DirectX::XMFLOAT3{ 1.7f,1.7f,1.7f });
    gruxEnemyActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<GruxEnemy>("GruxEnemy", GruxEnemyTr);
    // Battle AI remains paused throughout the pre-battle approach and intro movie.
    if (gruxEnemyActor)
        gruxEnemyActor->PauseBattleAI();
    // Only this scene's boss-death flow owns the death animation sequence.
    gruxEnemyActor->SetCinematicDeathAnimationOwnedExternally(true);
    player->SetFinalHitCallback([this](GruxEnemy* boss, const DirectX::XMFLOAT3& source)
        { OnPlayerFinalHit(boss, source); });

    // メインの部屋にチュートリアル用の骸骨を追加。
    SpawnTutorialSkeletons();

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

bool GameScene::Uninitialize(ID3D11Device* device)
{
    bgmDirector.StopDeathBgm();
    bgmDirector.ClearActors();
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

#ifdef USE_IMGUI
void GameScene::DrawTutorialDebugGui()
{
    ImGui::SeparatorText("Tutorial Camera Profile Trigger");
    ImGui::DragFloat3("Corridor Exit Trigger Center", &tutorialCameraProfileTriggerCenter.x,
        0.05f, -100.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat3("Corridor Exit Trigger Size", &tutorialCameraProfileTriggerSize.x,
        0.05f, 0.05f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SeparatorText("Prison Opening Sitting");
    ImGui::DragFloat3("Sitting Position", &prisonOpeningSittingPosition.x,
        0.01f, -100.0f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Sitting Rotation Yaw", &prisonOpeningSittingYawDegrees,
        0.1f, -360.0f, 360.0f, "%.3f deg");
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
    const float recallPreWaitElapsed = phase2TransitionStep == Phase2TransitionStep::RecallPreWait
        ? phase2StepElapsed : 0.0f;
    const float recallPreWaitRemaining = (std::max)(0.0f,
        phase2RecallPreWaitDuration - recallPreWaitElapsed);
    const auto phase2BossController = gruxEnemyActor
        ? gruxEnemyActor->GetBodyAnimationController() : nullptr;
    const auto phase2PlayerController = player ? player->GetBodyAnimationController() : nullptr;
    float phase1BossScale = gruxEnemyActor ? gruxEnemyActor->GetPhase1BossScale() : 1.7f;
    float phase2BossScale = gruxEnemyActor ? gruxEnemyActor->GetPhase2BossScale() : 2.1f;
    const bool phase1ScaleChanged = ImGui::DragFloat(U8("Phase1 Boss Scale"), &phase1BossScale,
        0.01f, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    const bool phase2ScaleChanged = ImGui::DragFloat(U8("Phase2 Boss Scale"), &phase2BossScale,
        0.01f, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    if (gruxEnemyActor && (phase1ScaleChanged || phase2ScaleChanged))
        gruxEnemyActor->SetPhaseScaleRange(phase1BossScale, phase2BossScale);
    ImGui::DragFloat(U8("Phase2 Transform Start Time"), &phase2TransformStartTime,
        0.01f, 0.0f, 15.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(U8("Phase2 Transform End Time"), &phase2TransformEndTime,
        0.01f, 0.0f, 15.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::DragFloat(U8("Phase2 Transform Progress"), &phase2TransformProgress,
        0.01f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp) && gruxEnemyActor)
    {
        gruxEnemyActor->SetPhaseTransformProgress(phase2TransformProgress);
    }
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
    ImGui::DragFloat(U8("Player Emote EndTime"), &phase2PlayerEmoteEndTime,
        0.001f, 0.001f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(U8("Phase2 Emote\u5F8CWait"), &phase2PlayerEmotePostWaitDuration,
        0.01f, 0.0f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SeparatorText("Phase2 Boss HP Preview");
    if (gruxEnemyActor)
        gruxEnemyActor->DrawPhase2HpBarPreviewGui();
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
        ImGui::DragFloat("Result Camera Blend Duration", &victoryCameraBlendDuration,
            0.01f, 0.0f, 2.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Result Recall To Idle Blend Duration", &resultRecallToIdleBlendDuration,
            0.01f, 0.0f, 0.5f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Result UI Trigger Animation Time", &resultUITriggerAnimationTime,
            0.01f, 0.0f, victoryEmoteEndTime, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Victory S Limit", &victoryRankSLimit, 1.0f, 0.0f, 5999.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Victory A Limit", &victoryRankALimit, 1.0f, 0.0f, 5999.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Victory B Limit", &victoryRankBLimit, 1.0f, 0.0f, 5999.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
        victoryRankALimit = (std::max)(victoryRankSLimit, victoryRankALimit);
        victoryRankBLimit = (std::max)(victoryRankALimit, victoryRankBLimit);
        victoryUIActor->SetNumberTextureSize(numberTexWidth, numberTexHeight);
        ImGui::DragFloat("Result Delay", &victoryResultDelayDuration,
            0.01f, 0.0f, 2.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
        bossDeathController->DrawDebugGui(player.get(), gruxEnemyActor.get());
        ImGui::TreePop();
    }
}

void GameScene::DrawPlayerDeathDebugGui()
{
    ImGui::Separator();
    ImGui::Text("Death Presentation");
    ImGui::DragFloat(U8("HUDフェードアウト時間"), &deathHudFadeDuration, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat(U8("背景暗転 開始時間"), &deathOverlayStartTime, 0.01f, 0.0f, 30.0f);

    ImGui::DragFloat(U8("DEFEATED 表示開始時間"), &deathDefeatedStartTime, 0.01f, 0.0f, 30.0f);

    ImGui::DragFloat(U8("Battle Time 表示開始時間"), &deathBattleTimeStartTime, 0.01f, 0.0f, 30.0f);

    ImGui::DragFloat(U8("ボタン 表示開始時間"), &deathButtonsStartTime, 0.01f, 0.0f, 30.0f);

    ImGui::DragFloat(U8("選択ライン 表示開始時間"), &deathSelectLineStartTime, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat(U8("操作受付 開始時間"), &deathResultInputEnableTime, 0.01f, 0.0f, 30.0f);
    ImGui::SliderFloat("Result Move SE Volume", &deathResultMoveSeVolume, 0.0f, 2.0f);
    ImGui::SliderFloat("Result Confirm SE Volume", &deathResultConfirmSeVolume, 0.0f, 2.0f);
    ImGui::DragFloat("numberTexWidth", &numberTexWidth, 1.0f, 1.0f, 200.0f);
    ImGui::DragFloat("numberTexHeight", &numberTexHeight, 1.0f, 1.0f, 300.0f);

    if (ImGui::TreeNode("Death Staging"))
    {
        ImGui::DragFloat("Min Player X", &deathStagingMinPlayerX, 0.01f);
        ImGui::DragFloat("Max Player X", &deathStagingMaxPlayerX, 0.01f);
        ImGui::DragFloat("Min Player Z", &deathStagingMinPlayerZ, 0.01f);
        ImGui::DragFloat("Max Player Z", &deathStagingMaxPlayerZ, 0.01f);
        ImGui::DragFloat("Wall Inset X", &deathStagingWallInsetX, 0.01f, 0.0f, 3.0f);
        ImGui::DragFloat("Wall Inset Z", &deathStagingWallInsetZ, 0.01f, 0.0f, 3.0f);
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
}

#endif


// ボスの部屋の色ラープ値を設定する
void GameScene::SetBossRoomLerpFactor(float lerpFactor)
{
    auto& shader = Scene::GetCurrentScene()->GetSceneSettings().sceneShaderConstants;
    shader.bossRoomLerpFactor = lerpFactor;
}


// ボスの部屋の色のラープを開始する関数
void GameScene::CancelBossRoomLerp()
{
    startBossRoomLerp = false;
    onFinished = nullptr;
    if (bossLerpEasing)
        bossLerpEasing->Clear();
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
