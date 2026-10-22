#include "pch.h"
#include "GameScene.h"
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
#include "Game/DarkGame/DarkActors/IceFragmentEffectActor.h"
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

    enum BossDeathShotIndex : size_t
    {
        BossDeathScream,
        BossDeathFall,
        BossDeathLanding,
        BossDeathFinish,
        BossDeathResult,
        BossDeathShotCount,
    };

    constexpr std::array<const char*, 2> Phase2CinematicPresetPaths = {
        "Data/Saves/ScenePresets/boss_recall.json",
        "Data/Saves/ScenePresets/boss_recall_player.json",
    };

    constexpr std::array<const char*, BossDeathShotCount> BossDeathPresetPaths = {
        "Data/Saves/ScenePresets/BossDeath_Scream.json",
        "Data/Saves/ScenePresets/BossDeath_Fall.json",
        "Data/Saves/ScenePresets/BossDeath_Landing.json",
        "Data/Saves/ScenePresets/BossDeath_Finish.json",
        "Data/Saves/ScenePresets/BossDeath_Result.json",
    };

    // Move the held sample into the requested interval before reflecting.
    // This is only a Death_B sample clock, not an animation blend.
    void AdvanceBossDeathHold(float& time, float& direction, float minTime,
        float maxTime, float rate, float entryRate, float deltaTime)
    {
        float remaining = (std::max)(deltaTime, 0.0f);
        if (time < minTime || time > maxTime)
        {
            const float target = time < minTime ? minTime : maxTime;
            direction = time < minTime ? 1.0f : -1.0f;
            if (entryRate <= 0.0f)
                return;
            const float entryDuration = std::abs(target - time) / entryRate;
            if (remaining < entryDuration)
            {
                time += direction * entryRate * remaining;
                return;
            }
            time = target;
            remaining -= entryDuration;
        }

        const float span = maxTime - minTime;
        if (span <= 0.0f || rate <= 0.0f)
            return;
        time += direction * std::fmod(rate * remaining, 2.0f * span);
        while (time > maxTime || time < minTime)
        {
            if (time > maxTime)
            {
                time = maxTime - (time - maxTime);
                direction = -1.0f;
            }
            else
            {
                time = minTime + (minTime - time);
                direction = 1.0f;
            }
        }
    }

    constexpr float RecallFirstSeTime = 1.4f;
    constexpr float RecallTimeSafetyMargin = 0.001f;
}

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
    judgeDodgeOperationTutorialState = backToRoomStartRequested ? JudgeDodgeOperationTutorialState::Completed : JudgeDodgeOperationTutorialState::Unseen;
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
        CreateLockOnTargetUI();
        CreateTutorialSkeletonUI();
        tutorialUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameTutorialUIActor>("TutorialUI");
        battleTimerUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameTimerUIActor>("BattleTimerUI");
        defeatUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameDefeatUIActor>("DefeatUI");
        defeatUIActor->SetSelectionCallback([this](int index) { ExecuteDeathResult(index); });
        LoadVictoryBestTime();
        victoryUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameVictoryUIActor>("VictoryUI");
        CreateBossDeathFadeUI();
        CreateBossDeathFinishUI();
        movieSkipUIActor = GetActorManager()->CreateAndRegisterActorWithTransform<GameMovieSkipUIActor>("MovieSkipUI");
        bossDeathShotsLoaded = LoadBossDeathShots();
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
    phase2TransitionElapsed = 0.0f;
    phase1BreakPending = false;
    phase2TransitionRequested = false;
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
        phase2TransformStarted = true;
        phase2TransformCompleted = true;
    }
    else
    {
        phase2TransformProgress = 0.0f;
        phase2TransformStarted = false;
        phase2TransformCompleted = false;
    }
    if (phase != BossPhase::Phase2)
    {
        phase2BgmStarted = false;
        phase2BgmCrossFadeActive = false;
        phase2BgmFadeElapsed = 0.0f;
        phase2BgmTriggerTime = -1.0f;
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
    phase2RecallActorPoseApplied = false;
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
    phase2TransitionRequested = true;
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
    phase2RecallActorPoseApplied = true;
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
                    phase2TransformStarted = true;
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
                if (recallAnimationTime >= phase2TransformStartTime)
                    phase2TransformStarted = true;
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
        if (isRecallAnimation && phase2TransformCompleted && !phase2BgmStarted)
        {
            BeginPhase2BgmCrossFade(controller->GetCurrentAnimationTime());
        }

        const bool recallFinished = !controller ||
            (isRecallAnimation && !controller->IsPlayAnimation());
        if (!recallFinished)
            break;

        // A shortened Recall must never leave the eventual Phase2 presentation partial.
        phase2TransformProgress = 1.0f;
        phase2TransformStarted = true;
        phase2TransformCompleted = true;
        if (!phase2BgmStarted)
            BeginPhase2BgmCrossFade(controller ? controller->GetCurrentAnimationTime() : phase2TransformEndTime);
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
    tutorialCompletedForBossBattle = false;
    if (player)
        player->SetMinimumHp(5);
    battleTimerUIActor->SetVisible(false);
    SetBattleHudVisible(false);
    // ゲームBGM
    gameBgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("GameBgmActor");
    gameBgmActor->SetSource(L"./Data/Sound/BGM/game_bgm1.wav");
    gameBgmActor->SetLoop(true);
    gameBgmActor->SetBgm(true);
    gameBgmActor->Play();
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

std::shared_ptr<BgmActor> GameScene::GetActiveBossBgmActor() const
{
    return phase2BgmStarted && phase2BgmActor ? phase2BgmActor : bossBgmActor;
}


void GameScene::BeginPhase2BgmCrossFade(const float recallAnimationTime)
{
    if (phase2BgmStarted)
        return;

    phase2BgmStarted = true;
    phase2BgmTriggerTime = recallAnimationTime;
    phase2BgmFadeElapsed = 0.0f;
    phase2BgmPhase1StartVolume = bossBgmActor ? bossBgmActor->GetVolume() : 0.0f;
    phase2BgmCrossFadeActive = bossBgmActor && phase2BgmActor;
    if (!phase2BgmActor)
        return;

    phase2BgmActor->Stop(false);
    phase2BgmActor->SetVolume(0.0f);
    phase2BgmActor->Play();
    if (!phase2BgmCrossFadeActive)
        phase2BgmActor->SetVolume(BossBgmVolume);
}

void GameScene::UpdatePhase2BgmCrossFade(const float deltaTime)
{
    if (!phase2BgmCrossFadeActive || !bossBgmActor || !phase2BgmActor)
        return;

    phase2BgmFadeElapsed += (std::max)(0.0f, deltaTime);
    const float fadeOut = (std::max)(phase2BgmFadeOutDuration, 0.001f);
    const float fadeIn = (std::max)(phase2BgmFadeInDuration, 0.001f);
    const float outT = std::clamp(phase2BgmFadeElapsed / fadeOut, 0.0f, 1.0f);
    const float inT = std::clamp(phase2BgmFadeElapsed / fadeIn, 0.0f, 1.0f);
    bossBgmActor->SetVolume(std::lerp(phase2BgmPhase1StartVolume, 0.0f, outT));
    phase2BgmActor->SetVolume(std::lerp(0.0f, BossBgmVolume, inT));
    if (outT >= 1.0f)
        bossBgmActor->Stop(false);
    if (outT >= 1.0f && inT >= 1.0f)
        phase2BgmCrossFadeActive = false;
}

void GameScene::ResetBossBattleBgm(const BossPhase phase, const bool play)
{
    phase2BgmCrossFadeActive = false;
    phase2BgmFadeElapsed = 0.0f;
    phase2BgmStarted = phase == BossPhase::Phase2;
    phase2BgmTriggerTime = -1.0f;

    const auto activeBgm = GetActiveBossBgmActor();
    const auto stopInactiveBgm = [activeBgm](const std::shared_ptr<BgmActor>& bgm)
        {
            if (bgm && bgm != activeBgm)
            {
                bgm->Stop(false);
                bgm->SetVolume(BossBgmVolume);
            }
        };
    stopInactiveBgm(bossBgmActor);
    stopInactiveBgm(phase2BgmActor);

    if (activeBgm)
    {
        activeBgm->SetVolume(BossBgmVolume);
        if (play && phase == BossPhase::Phase1)
        {
            Logger::Log(Logger::LogCategory::Gameplay,
                "[BossBGM][RestartFromBeginning] caller=GameScene::ResetBossBattleBgm");
            activeBgm->RestartFromBeginning();
        }
        else if (play)
            activeBgm->Play();
        else
            activeBgm->Stop(false);
    }
    if (gameBgmActor)
        gameBgmActor->Stop();
}

void GameScene::BeginBossBattleBgmFadeOut()
{
    const auto activeBgm = GetActiveBossBgmActor();
    if (bossBgmFading || !activeBgm) return;
    bossBgmFadeStartVolume = activeBgm->GetVolume();
    bossBgmFadeElapsed = 0.0f;
    bossBgmFading = true;
}

void GameScene::BeginGameBgmFadeOut()
{
    if (battleFlowState != BattleFlowState::Intro || gameBgmFading || !gameBgmActor) return;
    gameBgmFadeStartVolume = gameBgmActor->GetVolume();
    gameBgmFadeElapsed = 0.0f;
    gameBgmFading = true;
}

void GameScene::PlayBossBattleBgm()
{
    if (bossBgmActor)
        bossBgmActor->Play();
}

void GameScene::UpdateGameBgmFadeOut(const float deltaTime)
{
    if (!gameBgmFading || !gameBgmActor) return;
    gameBgmFadeElapsed += (std::max)(0.0f, deltaTime);
    const float duration = (std::max)(gameBgmFadeDuration, 0.001f);
    const float t = std::clamp(gameBgmFadeElapsed / duration, 0.0f, 1.0f);
    gameBgmActor->SetVolume(std::lerp(gameBgmFadeStartVolume, 0.0f, t));
    if (t >= 1.0f)
    {
        gameBgmActor->Stop(false);
        gameBgmFading = false;
    }
}

void GameScene::BeginPlayerDeathBgmFadeOut()
{
    const auto activeBgm = GetActiveBossBgmActor();
    if (playerBgmFading || !activeBgm) return;
    phase2BgmCrossFadeActive = false;
    bossBgmFadeStartVolume = activeBgm->GetVolume();
    bossBgmFadeElapsed = 0.0f;
    playerBgmFading = true;
}

void GameScene::UpdateDeathBgmFade(const float deltaTime)
{
    const auto activeBgm = GetActiveBossBgmActor();
    if ((!bossBgmFading && !playerBgmFading) || !activeBgm) return;
    const float duration = playerBgmFading ? playerDeathBgmFadeTime : bossDeathBgmFadeTime;
    bossBgmFadeElapsed += deltaTime;
    const float t = duration > 0.0f ? std::clamp(bossBgmFadeElapsed / duration, 0.0f, 1.0f) : 1.0f;
    activeBgm->SetVolume(std::lerp(bossBgmFadeStartVolume, 0.0f, t));
    if (t >= 1.0f)
    {
        activeBgm->Stop(false);
        bossBgmFading = false;
        playerBgmFading = false;
    }
}

void GameScene::PlayBossDeathSecondBgm() { if (!bossDeathSecondBgmPlayed && bossDeathSecondBgmActor) { bossDeathSecondBgmPlayed = true; bossDeathSecondBgmActor->Play(); } }
void GameScene::PlayPlayerDeathBgm() { if (!playerDeathBgmPlayed && playerDeathBgmActor) { playerDeathBgmPlayed = true; playerDeathBgmActor->Play(); } }
void GameScene::ResetDeathBgmState(const BossPhase restartPhase)
{
    if (bossDeathSecondBgmActor) bossDeathSecondBgmActor->Stop(false);
    if (playerDeathBgmActor) playerDeathBgmActor->Stop(false);
    bossDeathSecondBgmPlayed = false;
    playerDeathBgmPlayed = false;
    bossBgmFading = false;
    playerBgmFading = false;
    bossBgmFadeElapsed = 0.0f;
    ResetBossBattleBgm(restartPhase, true);
}

void GameScene::CreateLockOnTargetUI()
{
    const auto createImage = [this](const char* texturePath, const char* name)
        {
            auto image = std::make_shared<UIImageComponent>(texturePath, name);
            image->SetPivot({ 0.5f, 0.5f });
            image->SetSize({ lockOnTargetUISize, lockOnTargetUISize });
            image->SetVisible(false);
            GetUIManager()->Add(image);
            return image;
        };

    lockOnTargetCenterImageComponent = createImage("./Data/Textures/UI/LockOn/lock_on_center.png", "LockOnTargetCenter");
    lockOnTargetTopImageComponent = createImage("./Data/Textures/UI/LockOn/lock_on_top.png", "LockOnTargetTop");
    lockOnTargetBottomImageComponent = createImage("./Data/Textures/UI/LockOn/lock_on_bottom.png", "LockOnTargetBottom");
    lockOnTargetLeftImageComponent = createImage("./Data/Textures/UI/LockOn/lock_on_left.png", "LockOnTargetLeft");
    lockOnTargetRightImageComponent = createImage("./Data/Textures/UI/LockOn/lock_on_right.png", "LockOnTargetRight");
}

void GameScene::CreateTutorialSkeletonUI()
{
    const auto uiManager = GetUIManager();
    if (tutorialPassiveSkeletonActor) tutorialPassiveSkeletonActor->CreateHpBarUI(uiManager, "TutorialPassiveSkeletonHp");
    if (tutorialDodgeSkeletonActor) tutorialDodgeSkeletonActor->CreateHpBarUI(uiManager, "TutorialDodgeSkeletonHp");
}

void GameScene::StartJudgeDodgeOperationTutorial()
{
    if (judgeDodgeOperationTutorialState != JudgeDodgeOperationTutorialState::Unseen || IsPaused()) return;
    judgeDodgeOperationTutorialState = JudgeDodgeOperationTutorialState::Showing;
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
    if (judgeDodgeOperationTutorialState != JudgeDodgeOperationTutorialState::Showing) return;
    CoreAudio::PlayOneShot("./Data/Sound/SE/button_push.wav", 0.5f);
    tutorialUIActor->SetExplanationVisible(false);
    tutorialDodgeGuideRingFinished = false;
    tutorialDodgeGuideRingElapsed = 0.0f;
    const bool ringStartedInAttackRange = tutorialDodgeSkeletonActor && player &&
        tutorialDodgeSkeletonActor->IsPlayerWithinAttackRange(*player);
    tutorialDodgeGuideRingStartedInAttackRange = ringStartedInAttackRange;
    tutorialDodgeGuideRingPreviousAttackRange = ringStartedInAttackRange;
    tutorialDodgeGuideRingActive = true;
    judgeDodgeOperationTutorialState = JudgeDodgeOperationTutorialState::Completed;
    if (judgeDodgeOperationTutorialOwnsPause) { SetPaused(false); Time::timeScale = judgeDodgeOperationTutorialPreviousTimeScale; judgeDodgeOperationTutorialOwnsPause = false; }
}

void GameScene::UpdateTutorialDodgeGuideRing()
{
    if (!tutorialUIActor) return;
    const bool invalidSkeleton = !tutorialDodgeSkeletonActor ||
        tutorialDodgeSkeletonActor->IsDead() ||
        tutorialDodgeSkeletonActor->IsPendingKill() ||
        (player && player->IsBossBattle());
    if (bossRoomLockOnScopeActive || tutorialBossEntryPending || tutorialCompletedForBossBattle)
    {
        tutorialDodgeGuideRingActive = false;
        tutorialDodgeGuideRingFinished = true;
        tutorialUIActor->HideRing();
        return;
    }
    if (tutorialDodgeGuideRingForceVisible)
    {
        if (invalidSkeleton)
            tutorialUIActor->HideRing();
        else
            tutorialUIActor->UpdateRing(tutorialDodgeSkeletonActor->GetPosition(), Time::UnscaledDeltaTime());
        return;
    }

    if (!tutorialDodgeGuideRingActive || tutorialDodgeGuideRingFinished) return;
    if (invalidSkeleton)
    {
        tutorialDodgeGuideRingActive = false; tutorialDodgeGuideRingFinished = true; tutorialUIActor->HideRing(); return;
    }
    tutorialDodgeGuideRingElapsed += Time::UnscaledDeltaTime();
    const bool attackRangeEnteredAfterRingShown = !tutorialDodgeGuideRingPreviousAttackRange && tutorialDodgeSkeletonAttackRange;
    const bool startedInRangeGraceExpired = tutorialDodgeGuideRingStartedInAttackRange &&
        tutorialDodgeGuideRingElapsed >= tutorialDodgeGuideRingRangeGraceDuration;
    tutorialDodgeGuideRingPreviousAttackRange = tutorialDodgeSkeletonAttackRange;
    if (attackRangeEnteredAfterRingShown || startedInRangeGraceExpired)
    {
        tutorialDodgeGuideRingActive = false;
        tutorialDodgeGuideRingFinished = true;
        tutorialUIActor->HideRing();
        return;
    }
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
    if (tutorialCompletedForBossBattle)
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

    const SkeletonHpBarSettings hpSettings{
        tutorialSkeletonHpDisplayDistance, tutorialSkeletonHpNearDistance, tutorialSkeletonHpFarDistance,
        tutorialSkeletonHpNearScaleMultiplier, tutorialSkeletonHpFarScaleMultiplier,
        tutorialSkeletonHpBarWorldOffset, tutorialSkeletonHpBarScale,
        tutorialSkeletonUseFixedScreenPosition, tutorialSkeletonHpFixedScreenPosition };
    if (tutorialPassiveSkeletonActor)
        tutorialPassiveSkeletonActor->UpdateHpBar(*darkCameraActor, player->GetPosition(), hpSettings);
    if (tutorialDodgeSkeletonActor)
        tutorialDodgeSkeletonActor->UpdateHpBar(*darkCameraActor, player->GetPosition(), hpSettings);
    const bool ltVisible = tutorialPassiveSkeletonLockOnCandidate &&
        player->IsOperationGuideItemVisible(Player::OperationGuideItem::LT) &&
        !player->IsOperationGuideItemLearned(Player::OperationGuideItem::LT);
    const bool passiveSkeletonAttackRange = tutorialPassiveSkeletonActor &&
        !tutorialPassiveSkeletonActor->IsDead() &&
        tutorialPassiveSkeletonActor->IsPlayerWithinAttackRange(*player);
    const bool yVisible = passiveSkeletonAttackRange &&
        player->IsOperationGuideItemVisible(Player::OperationGuideItem::Y) &&
        !player->IsOperationGuideItemLearned(Player::OperationGuideItem::Y);
    const bool xVisible = tutorialDodgeGuideActivated && !tutorialDodgeSkeletonJustDodged &&
        player->IsOperationGuideItemVisible(Player::OperationGuideItem::X);

    tutorialUIActor->UpdatePrompts(*darkCameraActor, tutorialPassiveSkeletonActor, tutorialDodgeSkeletonActor,
        ltVisible, yVisible, xVisible, tutorialSkeletonUseFixedScreenPosition);
}
void GameScene::HideLockOnTargetUI()
{
    const std::array<std::shared_ptr<UIImageComponent>, 5> images =
    {
        lockOnTargetCenterImageComponent, lockOnTargetTopImageComponent,
        lockOnTargetBottomImageComponent, lockOnTargetLeftImageComponent,
        lockOnTargetRightImageComponent
    };
    for (const auto& image : images)
    {
        if (image)
            image->SetVisible(false);
    }
    lockOnTargetComponent.reset();
    lockOnTargetUIAnimationPhase = LockOnTargetUIAnimationPhase::Hidden;
    lockOnTargetUIAnimationElapsed = 0.0f;
    lockOnTargetUIRotationDegree = 0.0f;
}

void GameScene::UpdateLockOnTargetUI(float deltaTime)
{
    if (!lockOnTargetCenterImageComponent)
        return;

    if (!darkCameraActor ||
        darkCameraActor->GetMovementMode() != DarkCameraActor::CameraMode::LockOn ||
        !darkCameraActor->HasValidLockOnTarget())
    {
        HideLockOnTargetUI();
        return;
    }

    const auto target = darkCameraActor->GetEnemyHead();
    if (!target)
    {
        HideLockOnTargetUI();
        return;
    }

    if (lockOnTargetComponent.lock() != target)
    {
        lockOnTargetComponent = target;
        lockOnTargetUIAnimationPhase = LockOnTargetUIAnimationPhase::Gathering;
        lockOnTargetUIAnimationElapsed = 0.0f;
        lockOnTargetUIRotationDegree = 0.0f;
    }

    const float safeDeltaTime = (std::max)(deltaTime, 0.0f);
    switch (lockOnTargetUIAnimationPhase)
    {
    case LockOnTargetUIAnimationPhase::Gathering:
        lockOnTargetUIAnimationElapsed += safeDeltaTime;
        if (lockOnTargetUIAnimationElapsed >= (std::max)(lockOnTargetUIGatherDuration, 0.0f))
        {
            lockOnTargetUIAnimationPhase = LockOnTargetUIAnimationPhase::Hold;
            lockOnTargetUIAnimationElapsed = 0.0f;
        }
        break;
    case LockOnTargetUIAnimationPhase::Hold:
        lockOnTargetUIAnimationElapsed += safeDeltaTime;
        if (lockOnTargetUIAnimationElapsed >= (std::max)(lockOnTargetUIHoldDuration, 0.0f))
        {
            lockOnTargetUIAnimationPhase = LockOnTargetUIAnimationPhase::Rotating;
            lockOnTargetUIAnimationElapsed = 0.0f;
        }
        break;
    case LockOnTargetUIAnimationPhase::Rotating:
        lockOnTargetUIRotationDegree += lockOnTargetUIRotationSpeedDegree * safeDeltaTime;
        if (lockOnTargetUIRotationDegree >= 360.0f || lockOnTargetUIRotationDegree <= -360.0f)
            lockOnTargetUIRotationDegree = std::fmod(lockOnTargetUIRotationDegree, 360.0f);
        break;
    default:
        break;
    }

    const auto projection = darkCameraActor->ProjectWorldPositionForUI(
        target->GetComponentLocation());
    if (!projection.valid || !projection.inFront || !projection.insideViewport)
    {
        const std::array<std::shared_ptr<UIImageComponent>, 5> images =
        {
            lockOnTargetCenterImageComponent, lockOnTargetTopImageComponent,
            lockOnTargetBottomImageComponent, lockOnTargetLeftImageComponent,
            lockOnTargetRightImageComponent
        };
        for (const auto& image : images)
        {
            if (image)
                image->SetVisible(false);
        }
        return;
    }

    float targetOverallScale = skeletonLockOnTargetUIOverallScale;
    DirectX::XMFLOAT2 targetOverallOffset = skeletonLockOnTargetUIOverallOffset;
    if (dynamic_cast<GruxEnemy*>(target->GetOwner()))
    {
        targetOverallScale = gruxLockOnTargetUIOverallScale;
        targetOverallOffset = gruxLockOnTargetUIOverallOffset;
    }
    const float overallScale = std::clamp(targetOverallScale, 0.01f, 2.0f);
    const float scaledImageSize = lockOnTargetUISize * overallScale;
    DirectX::XMFLOAT2 center = ConvertScreenToUI(projection.screenPosition);
    center.x += targetOverallOffset.x;
    center.y += targetOverallOffset.y;
    float gatherRemaining = 0.0f;
    if (lockOnTargetUIAnimationPhase == LockOnTargetUIAnimationPhase::Gathering)
    {
        const float duration = (std::max)(lockOnTargetUIGatherDuration, 0.0001f);
        const float t = std::clamp(lockOnTargetUIAnimationElapsed / duration, 0.0f, 1.0f);
        const float easeOutCubic = 1.0f - std::pow(1.0f - t, 3.0f);
        gatherRemaining = lockOnTargetUIStartOffset * (1.0f - easeOutCubic);
    }
    const float ringAngle = lockOnTargetUIAnimationPhase == LockOnTargetUIAnimationPhase::Rotating
        ? lockOnTargetUIRotationDegree : 0.0f;

    lockOnTargetCenterImageComponent->SetSize({ scaledImageSize, scaledImageSize });
    lockOnTargetCenterImageComponent->SetWorldPosition(center);
    lockOnTargetCenterImageComponent->SetWorldAngleDegree(0.0f);
    lockOnTargetCenterImageComponent->SetVisible(true);

    const std::array<std::shared_ptr<UIImageComponent>, 4> rings =
    {
        lockOnTargetTopImageComponent, lockOnTargetBottomImageComponent,
        lockOnTargetLeftImageComponent, lockOnTargetRightImageComponent
    };
    const std::array<DirectX::XMFLOAT2, 4> directions =
    {
        DirectX::XMFLOAT2{ 0.0f, -1.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f },
        DirectX::XMFLOAT2{ -1.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f }
    };
    const std::array<DirectX::XMFLOAT2, 4> finalOffsets =
    {
        lockOnTargetUITopFinalOffset, lockOnTargetUIBottomFinalOffset,
        lockOnTargetUILeftFinalOffset, lockOnTargetUIRightFinalOffset
    };
    const float radians = DirectX::XMConvertToRadians(ringAngle);
    const float cosAngle = std::cos(radians);
    const float sinAngle = std::sin(radians);
    for (size_t i = 0; i < rings.size(); ++i)
    {
        const auto& ring = rings[i];
        if (!ring)
            continue;

        ring->SetSize({ scaledImageSize, scaledImageSize });
        DirectX::XMFLOAT2 offset =
        {
            (finalOffsets[i].x + directions[i].x * gatherRemaining) * overallScale,
            (finalOffsets[i].y + directions[i].y * gatherRemaining) * overallScale
        };
        if (lockOnTargetUIAnimationPhase == LockOnTargetUIAnimationPhase::Rotating)
        {
            // UI Y grows downward, so positive degrees rotate clockwise visually.
            offset =
            {
                offset.x * cosAngle - offset.y * sinAngle,
                offset.x * sinAngle + offset.y * cosAngle
            };
        }
        ring->SetWorldPosition({ center.x + offset.x, center.y + offset.y });
        ring->SetWorldAngleDegree(ringAngle);
        ring->SetVisible(true);
    }
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
        Logger::Log(Logger::LogCategory::Gameplay, std::format(
            "[LockOn][Selected] target={} distanceXZ={:.3f}",
            selectedEnemy->GetName(), std::sqrt(selectedDistanceSq)));
        return LockOnTargetSelectionResult::Selected;
    }

    darkCameraActor->ClearEnemyHead();
    lockOnSelectedEnemy.reset();
    Logger::Log(Logger::LogCategory::Gameplay, "[LockOn][Selected] no valid target");
    return LockOnTargetSelectionResult::NoCandidate;
}

void GameScene::CompleteTutorialForBossBattle()
{
    if (tutorialCompletedForBossBattle)
        return;

    tutorialCompletedForBossBattle = true;
    HideTutorialSkeletonUI();
    tutorialPassiveSkeletonLockOnCandidate = false;
    tutorialPassiveSkeletonYHpBaseline = -1;
    tutorialPassiveSkeletonYHpBaselineCaptured = false;
    tutorialDodgeSkeletonYHpBaseline = -1;
    tutorialDodgeSkeletonYHpBaselineCaptured = false;
    tutorialDodgeSkeletonAttackRange = false;
    tutorialDodgeGuideActivated = false;
    tutorialDodgeSkeletonJustDodged = false;

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
    tutorialPassiveSkeletonLockOnCandidate = false;
    if (tutorialCompletedForBossBattle)
    {
        tutorialDodgeSkeletonAttackRange = false;
        return;
    }
    if (!player || !tutorialPassiveSkeletonActor)
    {
        tutorialDodgeSkeletonAttackRange = false;
        if (player)
            player->SetOperationGuideItemVisible(Player::OperationGuideItem::X, false);
        return;
    }

    const auto revealYAndCaptureTutorialSkeletonHp = [this]()
        {
            if (player->IsOperationGuideItemVisible(Player::OperationGuideItem::Y))
                return;

            player->SetOperationGuideItemVisible(Player::OperationGuideItem::Y, true);
            tutorialPassiveSkeletonYHpBaseline = tutorialPassiveSkeletonActor->GetHp();
            tutorialPassiveSkeletonYHpBaselineCaptured = true;
            if (tutorialDodgeSkeletonActor)
            {
                tutorialDodgeSkeletonYHpBaseline = tutorialDodgeSkeletonActor->GetHp();
                tutorialDodgeSkeletonYHpBaselineCaptured = true;
            }
        };

    std::shared_ptr<SceneComponent> target;
    float distanceSq = FLT_MAX;
    float screenDistanceSq = FLT_MAX;
    tutorialPassiveSkeletonLockOnCandidate = EvaluateLockOnCandidate(
        tutorialPassiveSkeletonActor, target, distanceSq, screenDistanceSq);
    if (tutorialPassiveSkeletonLockOnCandidate)
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
    if (selectionResult == LockOnTargetSelectionResult::Selected && selectedTutorialSkeleton &&
        player->IsOperationGuideItemVisible(Player::OperationGuideItem::LT))
    {
        player->SetOperationGuideItemLearned(Player::OperationGuideItem::LT);
        revealYAndCaptureTutorialSkeletonHp();
    }

    if (player->IsOperationGuideItemVisible(Player::OperationGuideItem::Y) &&
        !player->IsOperationGuideItemLearned(Player::OperationGuideItem::Y))
    {
        const auto detectTutorialSkeletonHit = [](const std::shared_ptr<SkeletonWarriorActor>& skeleton,
            int& baselineHp, bool& baselineCaptured)
            {
                if (!skeleton)
                    return false;

                const int currentHp = skeleton->GetHp();
                if (!baselineCaptured)
                {
                    baselineHp = currentHp;
                    baselineCaptured = true;
                    return false;
                }
                return currentHp < baselineHp;
            };

        if (detectTutorialSkeletonHit(tutorialPassiveSkeletonActor,
            tutorialPassiveSkeletonYHpBaseline, tutorialPassiveSkeletonYHpBaselineCaptured) ||
            detectTutorialSkeletonHit(tutorialDodgeSkeletonActor,
                tutorialDodgeSkeletonYHpBaseline, tutorialDodgeSkeletonYHpBaselineCaptured))
        {
            player->SetOperationGuideItemLearned(Player::OperationGuideItem::Y);
        }
    }

    const bool tutorialDodgeSkeletonAvailable = tutorialDodgeSkeletonActor &&
        tutorialDodgeSkeletonActor->IsAlive() &&
        tutorialDodgeSkeletonActor->IsActive() &&
        !tutorialDodgeSkeletonActor->IsDead() &&
        !tutorialDodgeSkeletonActor->IsPendingKill();
    tutorialDodgeSkeletonAttackRange = tutorialDodgeSkeletonAvailable &&
        tutorialDodgeSkeletonActor->IsPlayerWithinAttackRange(*player);
    if (tutorialDodgeSkeletonAttackRange && !tutorialDodgeGuideActivated)
    {
        tutorialDodgeGuideActivated = true;
        revealYAndCaptureTutorialSkeletonHp();
    }
    // Visibility follows the current combat range; learned remains independent.
    player->SetOperationGuideItemVisible(Player::OperationGuideItem::X, tutorialDodgeSkeletonAttackRange);

    if (tutorialDodgeGuideActivated && !tutorialDodgeSkeletonJustDodged &&
        player->IsJustDodgePresentationActive() &&
        player->GetRushTarget().get() == tutorialDodgeSkeletonActor.get())
    {
        // Rush target is assigned only by Player::StartJustDodgeSuccess().
        tutorialDodgeSkeletonJustDodged = true;
    }

    if (tutorialDodgeGuideActivated &&
        player->IsOperationGuideItemVisible(Player::OperationGuideItem::X) &&
        !player->IsOperationGuideItemLearned(Player::OperationGuideItem::X) &&
        player->IsDodging())
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
    UpdateGameBgmFadeOut(Time::UnscaledDeltaTime());
    UpdateDeathBgmFade(Time::UnscaledDeltaTime());
    UpdatePhase2BgmCrossFade(Time::UnscaledDeltaTime());

    if (judgeDodgeOperationTutorialState == JudgeDodgeOperationTutorialState::Showing)
    {
        tutorialUIActor->UpdateExplanationLayout();
        if (InputSystem::GetInputState("A", InputStateMask::Trigger) || InputSystem::GetInputState("Enter", InputStateMask::Trigger))
            CloseJudgeDodgeOperationTutorial();
        return;
    }
    const bool mainRoomTutorialActive = !tutorialCompletedForBossBattle &&
        !bossRoomLockOnScopeActive && !tutorialBossEntryPending &&
        player && !player->IsBossBattle();
    if (mainRoomTutorialActive)
    {

        if (judgeDodgeOperationTutorialState == JudgeDodgeOperationTutorialState::Unseen && player && player->GetPosition().x > -34.0f && !IsPaused() && InputSystem::IsInputEnabled() && !cameraManager->IsUseMovie() && !cameraManager->IsUseCinematic())
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
        HideLockOnTargetUI();
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


    // StartBossBattle is also used as a battle reset entry point.  If a prior
    // Phase2 track survived to here, explicitly return audio ownership to Phase1.
    const bool hadPhase2BgmRuntime = phase2BgmStarted || phase2BgmCrossFadeActive;
    ResetBossPhaseRuntime(BossPhase::Phase1);
    if (hadPhase2BgmRuntime)
        ResetBossBattleBgm(BossPhase::Phase1, true);
    else
        PlayBossBattleBgm();
    if (!gameBgmFading && gameBgmActor)
        gameBgmActor->Stop(false);
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
    // Resume is idempotent and also handles repeated StartBossBattle calls.
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
    BeginPlayerDeathBgmFadeOut();
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
        PlayPlayerDeathBgm();
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
    ResetDeathBgmState(resumePhase);
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
            // Release only the Rush-owned Boss slow; Rush::Exit keeps its existing cleanup.
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
    // Diagnostic only: retain the lethal-hit callback route until EnterBossDead.
    bossDeathLastHpZeroDetectionSource = "FinalHit callback";
    DisableCinematicCameraDebugInput();
    using namespace DirectX;
    // +Z forward / +X right, matching Character::UpdateDirectionVectors and Grux AI.
    XMFLOAT3 direction{ source.x - boss->GetPosition().x, 0.0f,
        source.z - boss->GetPosition().z };
    XMFLOAT3 local{};
    XMStoreFloat3(&local, XMVector3Rotate(XMLoadFloat3(&direction),
        XMQuaternionInverse(XMLoadFloat4(&boss->GetQuaternionRotation()))));
    if (local.x * local.x + local.z * local.z < FLT_EPSILON)
        finalHitDirection = "Front";
    else if (std::abs(local.z) > std::abs(local.x))
        finalHitDirection = local.z > 0.0f ? "Front" : "Back";
    else
        finalHitDirection = local.x >= 0.0f ? "Right" : "Left";
    finalHitReaction = "HitReact_" + finalHitDirection;
    finalHitSlowScale = std::isfinite(finalHitSlowScale)
        ? std::clamp(finalHitSlowScale, 0.01f, 1.0f) : 0.20f;
    finalHitSlowDuration = std::isfinite(finalHitSlowDuration)
        ? std::clamp(finalHitSlowDuration, 0.01f, 5.0f) : 0.30f;
    finalHitAftermathDuration = std::isfinite(finalHitAftermathDuration)
        ? std::clamp(finalHitAftermathDuration, 0.0f, 2.0f) : 0.15f;
    finalHitRecoveryElapsed = 0.0f;
    finalHitRecoveryDurationActive = 0.0f;
    finalHitRecoveryProgress = 0.0f;
    recoveryStartScale = finalHitSlowScale;
    currentRecoveryScale = finalHitSlowScale;
    finalHitReactionCutReached = false;
    finalHitTimer = finalHitSlowDuration;
    finalHitPending = true; // Do not consume time from before this frame's hit.
    battleFlowState = BattleFlowState::FinalHitSlow;
    finalBattleTime = battleElapsedTime + Time::UnscaledDeltaTime();
    finalBattleTimeSaved = true;
    if (player)
        phase2FinalHitAttackWaitRequired = player->BeginFinalHitWait();
    boss->BeginFinalHitReaction(finalHitReaction);
    // Replace the ordinary hit stop. GameScene owns the unscaled hold/recovery
    // timers so Time::Tick cannot snap to 1 before recovery starts.
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

    // HP can reach zero through a route without the lethal-hit callback. In
    // that case there is no owned boss reaction to await, but an active Player
    // attack still receives the same natural-completion drain.
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
    const auto& gameplayShader = GetSceneSettings().sceneShaderConstants;
    bossDeathGameplayDof = {
        gameplayShader.focusDistance,
        gameplayShader.dofNearRange,
        gameplayShader.dofRange,
        gameplayShader.dofBlurStrength,
        gameplayShader.enableBlur,
        gameplayShader.enableDof,
    };
    bossDeathGameplayDofCaptured = true;

    battleFlowState = BattleFlowState::BossDead;
    battleTimerUIActor->SetVisible(false);
    SetBattleHudVisible(false);
    bossDeathWalkStopStartPosition = {};
    bossDeathWalkStopFinishPosition = {};
    bossDeathWalkStopStartRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bossDeathWalkStopFinishRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bossDeathPhase = BossDeathPhase::FadeOut;
    bossDeathPhaseElapsed = 0.0f;
    bossDeathRecallPromptTime = bossDeathRecallPromptMinTime;
    bossDeathRecallPromptDirection = 1.0f;
    bossDeathFinishHoldTime = bossDeathFinishHoldMinTime;
    bossDeathFinishHoldDirection = 1.0f;
    bossDeathFwdPromptTime = bossDeathFwdPromptMinTime;
    bossDeathFwdPromptDirection = 1.0f;
    bossDeathScreamHoldTime = bossDeathScreamHoldMin;
    bossDeathScreamHoldDirection = 1.0f;
    bossDeathFinishInputEnabled = false;
    StopBossDeathGroanLoop();
    if (bossDeathVoiceAudio)
    {
        bossDeathVoiceAudio->Stop(false);
        bossDeathVoiceAudio.reset();
    }
    SetBossDeathFinishUIVisible(false);
    SetBossDeathFadeAlpha(0.0f);

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
    ResetDeathBgmState(BossPhase::Phase1);
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
    finalHitDirection = "None";
    finalHitReaction.clear();
    Time::SetSlow(1.0f, 0.0f);
    StopBossDeathGroanLoop();
    if (bossDeathVoiceAudio)
    {
        bossDeathVoiceAudio->Stop(false);
        bossDeathVoiceAudio.reset();
    }

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

    // Cut to the camera's current pose to cancel both arbitrary-pose blending
    // and camera-path playback before returning render ownership to gameplay.
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
    if (bossDeathGameplayDofCaptured)
        ApplyBossDeathDof(bossDeathGameplayDof);
    bossDeathGameplayDofCaptured = false;

    bossDeathWalkStopStartPosition = {};
    bossDeathWalkStopFinishPosition = {};
    bossDeathWalkStopStartRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bossDeathWalkStopFinishRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bossDeathPhase = BossDeathPhase::FadeOut;
    bossDeathPhaseElapsed = 0.0f;
    bossDeathRecallPromptTime = bossDeathRecallPromptMinTime;
    bossDeathRecallPromptDirection = 1.0f;
    bossDeathFinishHoldTime = bossDeathFinishHoldMinTime;
    bossDeathFinishHoldDirection = 1.0f;
    bossDeathFwdPromptTime = bossDeathFwdPromptMinTime;
    bossDeathFwdPromptDirection = 1.0f;
    bossDeathScreamHoldTime = bossDeathScreamHoldMin;
    bossDeathScreamHoldDirection = 1.0f;
    bossDeathApproachStartRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bossDeathFinishInputEnabled = false;
    SetBossDeathFinishUIVisible(false);
    SetBossDeathFadeAlpha(0.0f);

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

bool GameScene::LoadBossDeathShots()
{
    try
    {
        for (size_t shotIndex = 0; shotIndex < BossDeathPresetPaths.size(); ++shotIndex)
        {
            SceneState sceneState{};
            SceneEditor::LoadSceneState(BossDeathPresetPaths[shotIndex], sceneState);

            const auto findActor = [&sceneState](const char* actorName)
                {
                    return std::find_if(sceneState.actorStates.begin(), sceneState.actorStates.end(),
                        [actorName](const ActorTransformState& actor)
                        {
                            return actor.name == actorName;
                        });
                };

            const auto playerState = findActor("Player");
            const auto bossState = findActor("GruxEnemy");
            if (playerState == sceneState.actorStates.end() ||
                bossState == sceneState.actorStates.end())
            {
                const std::string message = std::format(
                    "Boss death preset is missing Player or GruxEnemy: {}",
                    BossDeathPresetPaths[shotIndex]);
                Logger::Error(Logger::LogCategory::System, message.c_str());
                return false;
            }

            auto& shot = bossDeathShots[shotIndex];
            shot.camera.position = sceneState.camera.position;
            shot.camera.rotation = sceneState.camera.rotation;
            shot.camera.fov = sceneState.camera.fov;
            shot.player = { playerState->position, playerState->rotation };
            shot.boss = { bossState->position, bossState->rotation };

            const auto& shader = sceneState.shader;
            shot.dof = {
                shader.focusDistance,
                shader.dofNearRange,
                shader.dofRange,
                shader.dofBlurStrength,
                shader.enableBlur,
                shader.enableDof,
            };
        }

        // The editable approach start defaults to the staged Fall player pose.
        //bossDeathApproachStartPosition =
        //    bossDeathShots[BossDeathFall].player.position;
    }
    catch (const std::exception& exception)
    {
        const std::string message = std::format(
            "Failed to load boss death presets: {}", exception.what());
        Logger::Error(Logger::LogCategory::System, message.c_str());
        return false;
    }

    return true;
}


GameMovieSkipUIActor::MovieSkipType GameScene::GetActiveMovieSkipType()
{
    if (battleFlowState == BattleFlowState::BossDead &&bossDeathPhase < BossDeathPhase::RecallPingPong)
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

void GameScene::CreateBossDeathFinishUI()
{
    bossDeathFinishGuideImage = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/Rush/rush_x_a_b.png", "BossDeathFinishGuide");
    bossDeathFinishGuideImage->SetSize(bossDeathFinishGuideSize);
    bossDeathFinishGuideImage->SetScale(bossDeathFinishGuideScale);
    bossDeathFinishGuideImage->SetPivot({ 0.5f, 0.5f });
    bossDeathFinishGuideImage->SetVisible(false);
    GetUIManager()->Add(bossDeathFinishGuideImage);

    bossDeathFinishButtonImage = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/Rush/rush_y.png", "BossDeathFinishY");
    bossDeathFinishButtonImage->SetSize(bossDeathFinishButtonSize);
    bossDeathFinishButtonImage->SetScale({
        bossDeathFinishButtonBaseScale.x * 0.8f,
        bossDeathFinishButtonBaseScale.y * 0.8f });
    bossDeathFinishButtonImage->SetPivot({ 0.5f, 0.5f });
    bossDeathFinishButtonImage->SetVisible(false);
    GetUIManager()->Add(bossDeathFinishButtonImage);

    bossDeathFinishWordImage = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/Rush/finish_word.png", "BossDeathFinishWord");
    bossDeathFinishWordImage->SetSize(bossDeathFinishWordSize);
    bossDeathFinishWordImage->SetScale(bossDeathFinishWordScale);
    bossDeathFinishWordImage->SetPivot({ 0.5f, 0.5f });
    bossDeathFinishWordImage->SetVisible(false);
    GetUIManager()->Add(bossDeathFinishWordImage);
}

void GameScene::SetBossDeathFinishUIVisible(const bool visible)
{
    bossDeathFinishUIVisible = visible;
    bossDeathFinishUIAlpha = 0.0f;
    bossDeathFinishUIPulseTimer = 0.0f;
    bossDeathFinishUIPulsePhase = visible
        ? BossDeathFinishPromptAnimationPhase::AppearGrow
        : BossDeathFinishPromptAnimationPhase::Hidden;

    const auto offsetPosition = [this](const DirectX::XMFLOAT2& base)
        {
            return DirectX::XMFLOAT2{
                base.x + bossDeathFinishUIPositionOffset.x,
                base.y + bossDeathFinishUIPositionOffset.y };
        };
    const CoreColor transparent{ 1.0f, 1.0f, 1.0f, 0.0f };

    if (bossDeathFinishGuideImage)
    {
        bossDeathFinishGuideImage->SetWorldPosition(
            offsetPosition(bossDeathFinishGuidePosition));
        bossDeathFinishGuideImage->SetScale(bossDeathFinishGuideScale);
        bossDeathFinishGuideImage->SetColor(transparent);
    }
    if (bossDeathFinishButtonImage)
    {
        bossDeathFinishButtonImage->SetWorldPosition(
            offsetPosition(bossDeathFinishButtonPosition));
        bossDeathFinishButtonImage->SetScale({
            bossDeathFinishButtonBaseScale.x * 0.8f,
            bossDeathFinishButtonBaseScale.y * 0.8f });
        bossDeathFinishButtonImage->SetColor(transparent);
    }
    if (bossDeathFinishWordImage)
    {
        bossDeathFinishWordImage->SetWorldPosition(
            offsetPosition(bossDeathFinishWordPosition));
        bossDeathFinishWordImage->SetScale({
            bossDeathFinishWordScale.x * 0.9f,
            bossDeathFinishWordScale.y * 0.9f });
        bossDeathFinishWordImage->SetColor(transparent);
    }

    if (bossDeathFinishGuideImage)
        bossDeathFinishGuideImage->SetVisible(visible);
    if (bossDeathFinishButtonImage)
        bossDeathFinishButtonImage->SetVisible(visible);
    if (bossDeathFinishWordImage)
        bossDeathFinishWordImage->SetVisible(visible);
}

void GameScene::UpdateBossDeathFinishUI()
{
    if (!bossDeathFinishUIVisible || !bossDeathFinishGuideImage ||
        !bossDeathFinishButtonImage || !bossDeathFinishWordImage)
    {
        return;
    }

    const float deltaTime = Time::UnscaledDeltaTime();
    bossDeathFinishUIAlpha = bossDeathFinishUIFadeInDuration <= FLT_EPSILON
        ? 1.0f
        : std::clamp(bossDeathFinishUIAlpha +
            deltaTime / bossDeathFinishUIFadeInDuration, 0.0f, 1.0f);
    bossDeathFinishUIPulseTimer += deltaTime;

    float buttonScale = 1.0f;
    float wordScale = 1.0f;
    switch (bossDeathFinishUIPulsePhase)
    {
    case BossDeathFinishPromptAnimationPhase::Hidden:
        break;
    case BossDeathFinishPromptAnimationPhase::AppearGrow:
    {
        constexpr float duration = 0.10f;
        const float time = std::clamp(bossDeathFinishUIPulseTimer, 0.0f, duration);
        buttonScale = std::clamp(
            Easing::OutBack(time, duration, 1.0f, 1.15f, 0.8f), 0.8f, 1.15f);
        wordScale = std::clamp(
            Easing::OutBack(time, duration, 1.0f, 1.08f, 0.9f), 0.9f, 1.08f);
        if (bossDeathFinishUIPulseTimer >= duration)
        {
            bossDeathFinishUIPulsePhase =
                BossDeathFinishPromptAnimationPhase::AppearSettle;
            bossDeathFinishUIPulseTimer = 0.0f;
        }
        break;
    }
    case BossDeathFinishPromptAnimationPhase::AppearSettle:
    {
        constexpr float duration = 0.08f;
        const float time = std::clamp(bossDeathFinishUIPulseTimer, 0.0f, duration);
        buttonScale = Easing::OutQuad(time, duration, 1.0f, 1.15f);
        wordScale = Easing::OutQuad(time, duration, 1.0f, 1.08f);
        if (bossDeathFinishUIPulseTimer >= duration)
        {
            bossDeathFinishUIPulsePhase =
                BossDeathFinishPromptAnimationPhase::PulseGrow;
            bossDeathFinishUIPulseTimer = 0.0f;
        }
        break;
    }
    case BossDeathFinishPromptAnimationPhase::PulseGrow:
    {
        constexpr float duration = 0.25f;
        const float time = std::clamp(bossDeathFinishUIPulseTimer, 0.0f, duration);
        buttonScale = Easing::InOutSine(time, duration, 1.12f, 1.0f);
        wordScale = Easing::InOutSine(time, duration, 1.05f, 1.0f);
        if (bossDeathFinishUIPulseTimer >= duration)
        {
            bossDeathFinishUIPulsePhase =
                BossDeathFinishPromptAnimationPhase::PulseReturn;
            bossDeathFinishUIPulseTimer = 0.0f;
        }
        break;
    }
    case BossDeathFinishPromptAnimationPhase::PulseReturn:
    {
        constexpr float duration = 0.35f;
        const float time = std::clamp(bossDeathFinishUIPulseTimer, 0.0f, duration);
        buttonScale = Easing::OutQuad(time, duration, 1.0f, 1.12f);
        wordScale = Easing::OutQuad(time, duration, 1.0f, 1.05f);
        if (bossDeathFinishUIPulseTimer >= duration)
        {
            bossDeathFinishUIPulsePhase =
                BossDeathFinishPromptAnimationPhase::PulseGrow;
            bossDeathFinishUIPulseTimer = 0.0f;
        }
        break;
    }
    }

    const auto offsetPosition = [this](const DirectX::XMFLOAT2& base)
        {
            return DirectX::XMFLOAT2{
                base.x + bossDeathFinishUIPositionOffset.x,
                base.y + bossDeathFinishUIPositionOffset.y };
        };
    const CoreColor color{ 1.0f, 1.0f, 1.0f, bossDeathFinishUIAlpha };

    bossDeathFinishGuideImage->SetWorldPosition(
        offsetPosition(bossDeathFinishGuidePosition));
    bossDeathFinishGuideImage->SetSize(bossDeathFinishGuideSize);
    bossDeathFinishGuideImage->SetScale(bossDeathFinishGuideScale);
    bossDeathFinishGuideImage->SetColor(color);

    bossDeathFinishButtonImage->SetWorldPosition(
        offsetPosition(bossDeathFinishButtonPosition));
    bossDeathFinishButtonImage->SetSize(bossDeathFinishButtonSize);
    bossDeathFinishButtonImage->SetScale({
        bossDeathFinishButtonBaseScale.x * buttonScale,
        bossDeathFinishButtonBaseScale.y * buttonScale });
    bossDeathFinishButtonImage->SetColor(color);

    bossDeathFinishWordImage->SetWorldPosition(
        offsetPosition(bossDeathFinishWordPosition));
    bossDeathFinishWordImage->SetSize(bossDeathFinishWordSize);
    bossDeathFinishWordImage->SetScale({
        bossDeathFinishWordScale.x * wordScale,
        bossDeathFinishWordScale.y * wordScale });
    bossDeathFinishWordImage->SetColor(color);
}

void GameScene::StartBossDeathGroanLoop()
{
    StopBossDeathGroanLoop();
    //bossDeathGroanAudio = std::make_shared<CoreStandaloneAudioSource>(L"./Data/Sound/SE/boss_death_grown.wav");
    bossDeathGroanAudio = std::make_shared<CoreStandaloneAudioSource>(L"./Data/Sound/SE/enemy_groan.wav");
    bossDeathGroanAudio->SetVolume(0.05f);
    bossDeathGroanAudio->Play(true);
}

void GameScene::StopBossDeathGroanLoop()
{
    if (!bossDeathGroanAudio)
        return;

    bossDeathGroanAudio->Stop(false);
    bossDeathGroanAudio.reset();
}

void GameScene::UpdateBossDeathPromptLoop(const float deltaTime)
{
    AdvanceBossDeathHold(bossDeathFwdPromptTime, bossDeathFwdPromptDirection,
        bossDeathFwdPromptMinTime, bossDeathFwdPromptMaxTime,
        bossDeathFwdPromptPlaybackRate, bossDeathFwdPlaybackRate, deltaTime);

    if (gruxEnemyActor)
    {
        if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
        {
            controller->HoldAnimationPose("Death_B_0", bossDeathFwdPromptTime);
        }
    }
}

void GameScene::UpdateBossDeathScreamHold(const float deltaTime)
{
    if (!gruxEnemyActor)
        return;
    const auto mesh = gruxEnemyActor->GetSkeletalMeshComponent();
    if (!mesh || !mesh->IsVisible())
        return; // The captured particles no longer follow the animated mesh.

    AdvanceBossDeathHold(bossDeathScreamHoldTime, bossDeathScreamHoldDirection,
        bossDeathScreamHoldMin, bossDeathScreamHoldMax,
        bossDeathScreamHoldRate, bossDeathScreamHoldRate, deltaTime);
    if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
        controller->HoldAnimationPose("Death_B_0", bossDeathScreamHoldTime);
}

void GameScene::SetBossDeathFadeAlpha(const float alpha)
{
    if (!bossDeathFadeOverlay)
        return;

    const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
    bossDeathFadeOverlay->SetColor(CoreColor{ 0.0f, 0.0f, 0.0f, clampedAlpha });
    bossDeathFadeOverlay->SetVisible(clampedAlpha > 0.0f);
}

void GameScene::ApplyBossDeathDof(const BossDeathDofState& dof)
{
    auto& shader = GetSceneSettings().sceneShaderConstants;
    shader.focusDistance = dof.focusDistance;
    shader.dofNearRange = dof.nearRange;
    shader.dofRange = dof.range;
    shader.dofBlurStrength = dof.blurStrength;
    shader.enableBlur = dof.enableBlur;
    shader.enableDof = dof.enableDof;
}

void GameScene::CutToBossDeathShot(const size_t shotIndex)
{
    if (shotIndex >= bossDeathShots.size() || !cinemaCameraActor)
        return;

    if (auto* camera = dynamic_cast<CinematicCameraComponent*>(
        cinemaCameraActor->GetCameraComponent()))
    {
        camera->CutToPose(bossDeathShots[shotIndex].camera);
        ApplyBossDeathDof(bossDeathShots[shotIndex].dof);
    }
}

void GameScene::SkipBossDeathToFinishPrompt()
{
    if (battleFlowState != BattleFlowState::BossDead ||
        bossDeathPhase >= BossDeathPhase::RecallPingPong)
    {
        return;
    }

    if (!bossDeathShotsLoaded || !player || !gruxEnemyActor || !cinemaCameraActor)
    {
        Logger::Warning(Logger::LogCategory::System,
            "Boss death skip ignored because finish-prompt staging is unavailable");
        return;
    }

    ClampBossDeathPreviewTuning();
    DisableCinematicCameraDebugInput();
    gruxEnemyActor->StopBossDeathCinematicOneShots();
    if (bossDeathVoiceAudio)
    {
        bossDeathVoiceAudio->Stop(false);
        bossDeathVoiceAudio.reset();
    }
    BeginBossBattleBgmFadeOut();
    PlayBossDeathSecondBgm();
    if (!bossDeathGroanAudio)
        StartBossDeathGroanLoop();
    SetBossDeathFadeAlpha(0.0f);

    bossDeathPhaseElapsed = 0.0f;
    bossDeathRecallPromptTime = bossDeathRecallPromptMinTime;
    bossDeathRecallPromptDirection = 1.0f;
    bossDeathFwdPromptTime = bossDeathFwdPromptMinTime;
    bossDeathFwdPromptDirection = 1.0f;

    if (const auto playerCapsule = std::dynamic_pointer_cast<ShapeComponent>(
        player->FindComponentByName("capsuleComponent")))
    {
        playerCapsule->DisableCollision();
    }
    if (const auto bossCapsule = std::dynamic_pointer_cast<ShapeComponent>(
        gruxEnemyActor->FindComponentByName("enemyCapsuleComponent")))
    {
        bossCapsule->DisableCollision();
    }

    const auto& finishPlayerPose = bossDeathShots[BossDeathFinish].player;
    DirectX::XMFLOAT3 finishPlayerPosition = finishPlayerPose.position;
    finishPlayerPosition.x += bossDeathFinishPlayerPositionOffset.x;
    finishPlayerPosition.y += bossDeathFinishPlayerPositionOffset.y;
    finishPlayerPosition.z += bossDeathFinishPlayerPositionOffset.z;
    player->ClearTransientBattleActions();
    player->SetPosition(finishPlayerPosition);
    player->SetQuaternionRotation(finishPlayerPose.rotation);
    player->UpdateAllComponentTransforms();
    if (const auto controller = player->GetBodyAnimationController())
    {
        controller->ReleaseLatchedVisualPose();
        controller->ReleaseHeldAnimationPose();
        controller->ResetAnimationRate();
        player->PlayBodyAnimation("Result_Recall", false, true, 0.0f, true);
        if (!controller->SetPlaybackRange(0.0f, bossDeathRecallPromptMinTime) ||
            !controller->HoldAnimationPose("Result_Recall", bossDeathRecallPromptTime))
        {
            Logger::Error(Logger::LogCategory::System,
                "Failed to hold Result_Recall for boss death skip");
        }
    }

    const auto& landingBossPose = bossDeathShots[BossDeathLanding].boss;
    DirectX::XMFLOAT3 finishBossPosition = landingBossPose.position;
    finishBossPosition.x += bossDeathFwdPositionOffset.x;
    finishBossPosition.y += bossDeathFwdPositionOffset.y;
    finishBossPosition.z += bossDeathFwdPositionOffset.z;
    gruxEnemyActor->StopBattleActions();
    gruxEnemyActor->AbortBehaviorTreeForDeath();
    gruxEnemyActor->EndFinalHitReaction();
    gruxEnemyActor->SetPosition(finishBossPosition);
    gruxEnemyActor->SetQuaternionRotation(landingBossPose.rotation);
    gruxEnemyActor->UpdateAllComponentTransforms();
    if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
    {
        controller->ReleaseHeldAnimationPose();
        controller->ResetAnimationRate();
        gruxEnemyActor->PlayBodyAnimation("Death_B_0", false, true, 0.0f, true,
            "GameScene::SkipBossDeathToFinishPrompt");
        if (!controller->SetPlaybackRange(bossDeathFwdStartTime, bossDeathFwdEndTime) ||
            !controller->HoldAnimationPose("Death_B_0", bossDeathFwdPromptTime))
        {
            Logger::Error(Logger::LogCategory::System,
                "Failed to hold Death_B_0 for boss death skip");
        }
    }

    if (cameraManager->IsUseDebug())
        cameraManager->ToggleCamera(this);
    if (cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(this);
    if (!cameraManager->IsUseCinematic())
        cameraManager->ToggleCinematicCamera(this);
    CutToBossDeathShot(BossDeathFinish);
    gruxEnemyActor->StopBossDeathCinematicOneShots();

    bossDeathPhase = BossDeathPhase::RecallPingPong;
    bossDeathFinishInputEnabled = true;
    SetBossDeathFinishUIVisible(true);
    movieSkipUIActor->ResetPresentation();
}
bool GameScene::SetupBossDeathCinematic()
{
    if (!bossDeathShotsLoaded || !player || !gruxEnemyActor || !cinemaCameraActor)
        return false;

    // SetupCinematic is entered only after FadeOut has set the overlay to full
    // black. Release the combat pose latch before staging the Player or cameras.
    if (const auto controller = player->GetBodyAnimationController())
        controller->ReleaseLatchedVisualPose();

    if (const auto playerCapsule = std::dynamic_pointer_cast<ShapeComponent>(
        player->FindComponentByName("capsuleComponent")))
    {
        playerCapsule->DisableCollision();
    }
    if (const auto bossCapsule = std::dynamic_pointer_cast<ShapeComponent>(
        gruxEnemyActor->FindComponentByName("enemyCapsuleComponent")))
    {
        bossCapsule->DisableCollision();
    }

    // Boss uses the Scream staging transform. Player starts from the shared
    // Fall/Landing transform and approaches the Finish transform later.
    const auto& screamShot = bossDeathShots[BossDeathScream];
    const auto& fallShot = bossDeathShots[BossDeathFall];
    player->SetPosition(fallShot.player.position);
    player->SetQuaternionRotation(fallShot.player.rotation);
    player->UpdateAllComponentTransforms();
    gruxEnemyActor->SetPosition(screamShot.boss.position);
    gruxEnemyActor->SetQuaternionRotation(screamShot.boss.rotation);
    gruxEnemyActor->UpdateAllComponentTransforms();

    if (cameraManager->IsUseDebug())
        cameraManager->ToggleCamera(this);
    if (cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(this);
    if (!cameraManager->IsUseCinematic())
        cameraManager->ToggleCinematicCamera(this);

    // ToggleCinematicCamera copies the gameplay pose first. Overwrite every pose
    // field and DOF in this same update so that copied pose is never rendered.
    CutToBossDeathShot(BossDeathScream);
    const auto controller = gruxEnemyActor->GetBodyAnimationController();
    if (controller)
        controller->SetAnimationRate(bossDeathRoarPlaybackRate);
    gruxEnemyActor->PlayBodyAnimation("Ultimate_Roar_0", false, true, 0.1f, true,
        "GameScene::SetupBossDeathCinematic");
    if (!controller || !controller->SetPlaybackRange(
        bossDeathRoarStartTime, bossDeathRoarEndTime))
    {
        Logger::Error(Logger::LogCategory::System,
            "Failed to set Ultimate_Roar_0 playback range for boss death preview");
    }
    return true;
}

void GameScene::ClampBossDeathPreviewTuning()
{
    bossDeathFallToLandingBlendDuration =
        (std::max)(bossDeathFallToLandingBlendDuration, 0.0f);
    bossDeathRoarPlaybackRate = std::clamp(
        bossDeathRoarPlaybackRate, 0.1f, 2.0f);
    bossDeathStunPlaybackRate = std::clamp(
        bossDeathStunPlaybackRate, 0.1f, 2.0f);
    bossDeathFwdPlaybackRate = std::clamp(
        bossDeathFwdPlaybackRate, 0.1f, 2.0f);
    bossDeathPlayerApproachDuration =
        (std::max)(bossDeathPlayerApproachDuration, 0.0f);
    bossDeathWalkStopStartDistance = std::isfinite(bossDeathWalkStopStartDistance)
        ? std::clamp(bossDeathWalkStopStartDistance, 0.0f, 2.0f) : 0.35f;
    bossDeathRecallPromptPlaybackRate =
        (std::max)(bossDeathRecallPromptPlaybackRate, 0.0f);
    bossDeathFwdPromptPlaybackRate =
        (std::max)(bossDeathFwdPromptPlaybackRate, 0.0f);
    bossDeathStunToDeathBlend = std::isfinite(bossDeathStunToDeathBlend)
        ? std::clamp(bossDeathStunToDeathBlend, 0.0f, 1.0f) : 0.25f;
    bossDeathScreamHoldRate = std::isfinite(bossDeathScreamHoldRate)
        ? std::clamp(bossDeathScreamHoldRate, 0.0f, 2.0f) : 0.30f;
    bossDeathHuskDelay = (std::max)(bossDeathHuskDelay, 0.0f);

    bossDeathFinishHoldPlaybackRate = std::isfinite(bossDeathFinishHoldPlaybackRate)
        ? std::clamp(bossDeathFinishHoldPlaybackRate, 0.0f, 3.0f) : 0.07f;
    if (player)
    {
        if (const auto controller = player->GetBodyAnimationController())
        {
            const float duration = controller->GetAnimationLength("Result_Recall");
            if (std::isfinite(duration) && duration >= 0.0f)
            {
                const float margin = (std::min)(0.001f, duration);
                bossDeathFinishHoldMinTime = std::clamp(
                    std::isfinite(bossDeathFinishHoldMinTime) ? bossDeathFinishHoldMinTime : 1.58f,
                    0.0f, duration - margin);
                bossDeathFinishHoldMaxTime = std::clamp(
                    std::isfinite(bossDeathFinishHoldMaxTime) ? bossDeathFinishHoldMaxTime : 1.70f,
                    bossDeathFinishHoldMinTime + margin, duration);
                bossDeathFinishHoldTime = std::clamp(bossDeathFinishHoldTime,
                    bossDeathFinishHoldMinTime, bossDeathFinishHoldMaxTime);
            }
        }
    }

    const float recallMaxLimit = RecallFirstSeTime - RecallTimeSafetyMargin;
    bossDeathRecallPromptMinTime = std::clamp(
        bossDeathRecallPromptMinTime,
        0.0f,
        recallMaxLimit - RecallTimeSafetyMargin);
    bossDeathRecallPromptMaxTime = std::clamp(
        bossDeathRecallPromptMaxTime,
        bossDeathRecallPromptMinTime + RecallTimeSafetyMargin,
        recallMaxLimit);

    if (gruxEnemyActor)
    {
        if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
        {
            const auto clampPlaybackRange = [](float& start, float& end,
                const float duration)
                {
                    constexpr float Margin = 0.001f;
                    if (!std::isfinite(duration) || duration <= Margin)
                    {
                        start = 0.0f;
                        end = (std::max)(duration, 0.0f);
                        return;
                    }

                    start = std::clamp(start, 0.0f, duration - Margin);
                    end = std::clamp(end, start + Margin, duration);
                };

            const float roarDuration =
                controller->GetAnimationLength("Ultimate_Roar_0");
            clampPlaybackRange(
                bossDeathRoarStartTime, bossDeathRoarEndTime, roarDuration);

            const float deathFwdDuration = controller->GetAnimationLength("Death_B_0");
            clampPlaybackRange(
                bossDeathFwdStartTime, bossDeathFwdEndTime, deathFwdDuration);
            clampPlaybackRange(
                bossDeathFwdPromptMinTime,
                bossDeathFwdPromptMaxTime,
                deathFwdDuration);
            if (!std::isfinite(bossDeathScreamHoldMin))
                bossDeathScreamHoldMin = 1.50f;
            if (!std::isfinite(bossDeathScreamHoldMax))
                bossDeathScreamHoldMax = 1.58f;
            clampPlaybackRange(bossDeathScreamHoldMin, bossDeathScreamHoldMax, deathFwdDuration);
        }
    }
}

void GameScene::UpdateBossDeathCinematic()
{
    const float deltaTime = Time::UnscaledDeltaTime();
    ClampBossDeathPreviewTuning();
    bossDeathPhaseElapsed += deltaTime;

    // Keep the boss held until capture hides its mesh, including a pending capture.
    if (bossDeathPhase == BossDeathPhase::HuskDelay ||
        bossDeathPhase == BossDeathPhase::HuskPreview)
        UpdateBossDeathScreamHold(deltaTime);

    if (bossDeathPhase == BossDeathPhase::HuskDelay ||
        bossDeathPhase == BossDeathPhase::HuskPreview)
    {
        const float span = bossDeathFinishHoldMaxTime - bossDeathFinishHoldMinTime;
        if (span > 0.0f)
        {
            // Remove whole round trips, then reflect overshoot as in Recall Prompt.
            const float advance = std::fmod(
                bossDeathFinishHoldPlaybackRate * (std::max)(deltaTime, 0.0f), 2.0f * span);
            bossDeathFinishHoldTime += bossDeathFinishHoldDirection * advance;
            while (bossDeathFinishHoldTime > bossDeathFinishHoldMaxTime ||
                bossDeathFinishHoldTime < bossDeathFinishHoldMinTime)
            {
                if (bossDeathFinishHoldTime > bossDeathFinishHoldMaxTime)
                {
                    bossDeathFinishHoldTime = bossDeathFinishHoldMaxTime -
                        (bossDeathFinishHoldTime - bossDeathFinishHoldMaxTime);
                    bossDeathFinishHoldDirection = -1.0f;
                }
                else
                {
                    bossDeathFinishHoldTime = bossDeathFinishHoldMinTime +
                        (bossDeathFinishHoldMinTime - bossDeathFinishHoldTime);
                    bossDeathFinishHoldDirection = 1.0f;
                }
            }
        }
        else
        {
            bossDeathFinishHoldTime = bossDeathFinishHoldMinTime;
        }
        if (player)
        {
            if (const auto controller = player->GetBodyAnimationController())
                controller->HoldAnimationPose("Result_Recall", bossDeathFinishHoldTime);
        }
    }

    switch (bossDeathPhase)
    {
    case BossDeathPhase::FadeOut:
    {
        const float duration = (std::max)(bossDeathFadeOutDuration, FLT_EPSILON);
        SetBossDeathFadeAlpha(bossDeathPhaseElapsed / duration);
        if (bossDeathPhaseElapsed >= duration)
        {
            SetBossDeathFadeAlpha(1.0f);
            bossDeathPhase = BossDeathPhase::SetupCinematic;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::SetupCinematic:
        SetBossDeathFadeAlpha(1.0f);
        if (SetupBossDeathCinematic())
        {
            bossDeathPhase = BossDeathPhase::FadeInScream;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;

    case BossDeathPhase::FadeInScream:
    {
        const float duration = (std::max)(bossDeathFadeInDuration, FLT_EPSILON);
        SetBossDeathFadeAlpha(1.0f - bossDeathPhaseElapsed / duration);
        if (bossDeathPhaseElapsed >= duration)
        {
            SetBossDeathFadeAlpha(0.0f);
            bossDeathPhase = BossDeathPhase::DeathScream;
            BeginBossBattleBgmFadeOut();
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::DeathScream:
    {
        const auto controller = gruxEnemyActor ? gruxEnemyActor->GetBodyAnimationController()
            : nullptr;
        if (controller &&
            controller->GetCurrentAnimationName() == "Ultimate_Roar_0" &&
            !controller->IsPlayAnimation())
        {
            CutToBossDeathShot(BossDeathFall);
            controller->ResetAnimationRate();
            controller->SetAnimationRate(bossDeathStunPlaybackRate);
            gruxEnemyActor->PlayBodyAnimation(
                "Result_Down_Start", false, true, 0.1f, true);
            bossDeathPhase = BossDeathPhase::DeathFall;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::DeathFall:
    {
        const auto controller = gruxEnemyActor
            ? gruxEnemyActor->GetBodyAnimationController()
            : nullptr;
        if (controller &&
            controller->GetCurrentAnimationName() == "Result_Down_Start" &&
            !controller->IsPlayAnimation())
        {
            controller->ResetAnimationRate();
            if (auto* camera = dynamic_cast<CinematicCameraComponent*>(
                cinemaCameraActor->GetCameraComponent()))
            {
                camera->BlendToPose(
                    bossDeathShots[BossDeathLanding].camera,
                    bossDeathFallToLandingBlendDuration);
            }
            ApplyBossDeathDof(bossDeathShots[BossDeathLanding].dof);

            const auto& landingBossPose = bossDeathShots[BossDeathLanding].boss;
            DirectX::XMFLOAT3 deathFwdPosition = landingBossPose.position;
            deathFwdPosition.x += bossDeathFwdPositionOffset.x;
            deathFwdPosition.y += bossDeathFwdPositionOffset.y;
            deathFwdPosition.z += bossDeathFwdPositionOffset.z;
            gruxEnemyActor->SetPosition(deathFwdPosition);
            gruxEnemyActor->SetQuaternionRotation(landingBossPose.rotation);
            gruxEnemyActor->UpdateAllComponentTransforms();

            controller->SetAnimationRate(bossDeathFwdPlaybackRate);
            gruxEnemyActor->PlayBodyAnimation(
                "Death_B_0", false, true, bossDeathStunToDeathBlend, true);
            if (!controller->SetPlaybackRange(
                bossDeathFwdStartTime, bossDeathFwdEndTime))
            {
                Logger::Error(Logger::LogCategory::System,
                    "Failed to set Death_Fwd playback range for boss death preview");
            }
            bossDeathPhase = BossDeathPhase::DeathLanding;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::DeathLanding:
    {
        const auto controller = gruxEnemyActor
            ? gruxEnemyActor->GetBodyAnimationController()
            : nullptr;
        if (controller &&
            controller->GetCurrentAnimationName() == "Death_B_0" &&
            controller->GetCurrentAnimationTime() >= bossDeathFwdEndTime)
        {
            controller->ResetAnimationRate();
            // Own the endpoint pose first; walking never restarts the boss clip.
            bossDeathFwdPromptTime = controller->GetCurrentAnimationTime();
            bossDeathFwdPromptDirection =
                bossDeathFwdPromptTime >= bossDeathFwdPromptMaxTime ? -1.0f : 1.0f;
            if (!controller->HoldAnimationPose("Death_B_0", bossDeathFwdPromptTime))
            {
                break;
            }
            PlayBossDeathSecondBgm();
            StartBossDeathGroanLoop();
            player->SetPosition(bossDeathApproachStartPosition);
            player->UpdateAllComponentTransforms();
            bossDeathApproachStartRotation = player->GetQuaternionRotation();
            player->PlayBodyAnimation("Walk_Fwd", true, true, 0.2f, true);
            bossDeathPhase = BossDeathPhase::PlayerApproach;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::PlayerApproach:
    {
        UpdateBossDeathPromptLoop(deltaTime);
        const float duration = (std::max)(
            bossDeathPlayerApproachDuration, FLT_EPSILON);
        const float t = std::clamp(bossDeathPhaseElapsed / duration, 0.0f, 1.0f);
        float smoothT = t * t * (3.0f - 2.0f * t);
        const auto& finishPose = bossDeathShots[BossDeathFinish].player;
        DirectX::XMFLOAT3 adjustedFinishPosition = finishPose.position;
        adjustedFinishPosition.x += bossDeathFinishPlayerPositionOffset.x;
        adjustedFinishPosition.y += bossDeathFinishPlayerPositionOffset.y;
        adjustedFinishPosition.z += bossDeathFinishPlayerPositionOffset.z;

        const float approachDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(
            DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&adjustedFinishPosition),
                DirectX::XMLoadFloat3(&bossDeathApproachStartPosition))));
        const float stopStartT = approachDistance > FLT_EPSILON
            ? 1.0f - (std::min)(bossDeathWalkStopStartDistance / approachDistance, 1.0f)
            : 0.0f;
        // Clamp at the threshold even when a long frame crosses it.
        const bool startStop = smoothT >= stopStartT;
        smoothT = (std::min)(smoothT, stopStartT);
        DirectX::XMFLOAT3 position{};
        position.x = std::lerp(
            bossDeathApproachStartPosition.x, adjustedFinishPosition.x, smoothT);
        position.y = std::lerp(
            bossDeathApproachStartPosition.y, adjustedFinishPosition.y, smoothT);
        position.z = std::lerp(
            bossDeathApproachStartPosition.z, adjustedFinishPosition.z, smoothT);
        DirectX::XMFLOAT4 rotation{};
        DirectX::XMStoreFloat4(&rotation, DirectX::XMQuaternionNormalize(
            DirectX::XMQuaternionSlerp(
                DirectX::XMLoadFloat4(&bossDeathApproachStartRotation),
                DirectX::XMLoadFloat4(&finishPose.rotation), smoothT)));

        player->SetPosition(position);
        player->SetQuaternionRotation(rotation);
        player->UpdateAllComponentTransforms();

        if (startStop)
        {
            bossDeathWalkStopStartPosition = player->GetPosition();
            bossDeathWalkStopFinishPosition = adjustedFinishPosition;
            bossDeathWalkStopStartRotation = player->GetQuaternionRotation();
            bossDeathWalkStopFinishRotation = finishPose.rotation;
            if (const auto controller = player->GetBodyAnimationController())
                controller->ResetAnimationRate();
            // Player root motion is disabled; only this scene moves the actor.
            player->PlayBodyAnimation("Walk_Fwd_Stop_Fast", false, true, 0.1f, true);
            bossDeathPhase = BossDeathPhase::PlayerWalkStop;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::PlayerWalkStop:
    {
        UpdateBossDeathPromptLoop(deltaTime);
        const auto controller = player->GetBodyAnimationController();
        if (!controller || controller->GetCurrentAnimationName() != "Walk_Fwd_Stop_Fast")
            break;

        const float duration = (std::max)(controller->GetAnimationLength("Walk_Fwd_Stop_Fast"), FLT_EPSILON);
        const float t = std::clamp(controller->GetCurrentAnimationTime() / duration, 0.0f, 1.0f);
        const float easeOut = 1.0f - (1.0f - t) * (1.0f - t);
        DirectX::XMFLOAT3 position{};
        DirectX::XMStoreFloat3(&position, DirectX::XMVectorLerp(
            DirectX::XMLoadFloat3(&bossDeathWalkStopStartPosition),
            DirectX::XMLoadFloat3(&bossDeathWalkStopFinishPosition), easeOut));
        DirectX::XMFLOAT4 rotation{};
        DirectX::XMStoreFloat4(&rotation, DirectX::XMQuaternionNormalize(DirectX::XMQuaternionSlerp(
            DirectX::XMLoadFloat4(&bossDeathWalkStopStartRotation),
            DirectX::XMLoadFloat4(&bossDeathWalkStopFinishRotation), easeOut)));
        player->SetPosition(position);
        player->SetQuaternionRotation(rotation);
        player->UpdateAllComponentTransforms();

        if (!controller->IsPlayAnimation())
        {
            player->SetPosition(bossDeathWalkStopFinishPosition);
            player->SetQuaternionRotation(bossDeathWalkStopFinishRotation);
            player->UpdateAllComponentTransforms();
            CutToBossDeathShot(BossDeathFinish);
            player->PlayBodyAnimation("Result_Recall", false, true, 0.15f, true);
            if (const auto controller = player->GetBodyAnimationController();
                !controller || !controller->SetPlaybackRange(
                    0.0f, bossDeathRecallPromptMinTime))
            {
                Logger::Error(Logger::LogCategory::System,
                    "Failed to set Result_Recall playback range for boss death preview");
            }
            bossDeathPhase = BossDeathPhase::RecallLeadIn;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::RecallLeadIn:
    {
        UpdateBossDeathPromptLoop(deltaTime);
        const auto controller = player
            ? player->GetBodyAnimationController()
            : nullptr;
        if (controller &&
            controller->GetCurrentAnimationName() == "Result_Recall" &&
            !controller->IsPlayAnimation())
        {
            // Recall's runtime ShowTrail state starts at 0.24 sec but its end at
            // 1.4 sec is intentionally never reached. Clear only transient
            // battle presentation before pose-only sampling begins.
            player->ClearTransientBattleActions();
            bossDeathRecallPromptTime = bossDeathRecallPromptMinTime;
            bossDeathRecallPromptDirection = 1.0f;
            if (controller->HoldAnimationPose(
                "Result_Recall", bossDeathRecallPromptTime))
            {
                bossDeathPhase = BossDeathPhase::RecallPingPong;
                bossDeathPhaseElapsed = 0.0f;
                bossDeathFinishInputEnabled = true;
                SetBossDeathFinishUIVisible(true);
            }
        }
        break;
    }

    case BossDeathPhase::RecallPingPong:
    {
        UpdateBossDeathPromptLoop(deltaTime);
        UpdateBossDeathFinishUI();
        bossDeathRecallPromptTime +=
            bossDeathRecallPromptDirection *
            bossDeathRecallPromptPlaybackRate * deltaTime;

        // Reflect overshoot at both endpoints. The loop also handles a large
        // debug-frame delta crossing the very short range more than once.
        while (bossDeathRecallPromptTime > bossDeathRecallPromptMaxTime ||
            bossDeathRecallPromptTime < bossDeathRecallPromptMinTime)
        {
            if (bossDeathRecallPromptTime > bossDeathRecallPromptMaxTime)
            {
                bossDeathRecallPromptTime =
                    bossDeathRecallPromptMaxTime -
                    (bossDeathRecallPromptTime - bossDeathRecallPromptMaxTime);
                bossDeathRecallPromptDirection = -1.0f;
            }
            else
            {
                bossDeathRecallPromptTime =
                    bossDeathRecallPromptMinTime +
                    (bossDeathRecallPromptMinTime - bossDeathRecallPromptTime);
                bossDeathRecallPromptDirection = 1.0f;
            }
        }

        if (const auto controller = player->GetBodyAnimationController())
        {
            controller->HoldAnimationPose(
                "Result_Recall", bossDeathRecallPromptTime);
        }

        const bool finishTriggered = bossDeathFinishInputEnabled &&
            InputSystem::GetInputState(
                "Attack", InputStateMask::Trigger, DeviceFlags::GamePadOnly);
        if (finishTriggered)
        {
            bossDeathFinishInputEnabled = false;
            SetBossDeathFinishUIVisible(false);
            if (const auto controller = player->GetBodyAnimationController())
            {
                const float resumeTime = bossDeathRecallPromptTime;
                controller->ReleaseHeldAnimationPose();
                player->PlayBodyAnimation(
                    "Result_Recall", false, false, 0.0f, true);
                if (!controller->SetPlaybackRange(
                    resumeTime, bossDeathRecallFinishHitTime))
                {
                    Logger::Error(Logger::LogCategory::System,
                        "Failed to resume Result_Recall for boss finish preview");
                }
            }
            player->ClearTransientBattleActions();
            bossDeathPhase = BossDeathPhase::FinishTriggered;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::FinishTriggered:
    {
        UpdateBossDeathPromptLoop(deltaTime);
        const auto controller = player
            ? player->GetBodyAnimationController()
            : nullptr;
        if (controller &&
            controller->GetCurrentAnimationName() == "Result_Recall" &&
            controller->GetCurrentAnimationTime() >= bossDeathRecallFinishHitTime)
        {
            StopBossDeathGroanLoop();
            // Keep pose ownership and enter the narrower interval continuously.
            bossDeathScreamHoldTime = bossDeathFwdPromptTime;
            bossDeathScreamHoldDirection = bossDeathFwdPromptDirection;
            // ボス死亡断末魔
            if (bossDeathVoiceAudio)
                bossDeathVoiceAudio->Stop(false);
            bossDeathVoiceAudio = CoreAudio::PlayOneShot("./Data/Sound/SE/boss_death_voice1.wav", 3.0f);
            // コントローラー振動
            InputSystem::SetVibration(0.9f, 0.2f);

            bossDeathFinishHoldTime = bossDeathFinishHoldMinTime;
            bossDeathFinishHoldDirection = 1.0f;
            controller->ResetAnimationRate();
            controller->HoldAnimationPose("Result_Recall", bossDeathFinishHoldTime);
            bossDeathPhase = BossDeathPhase::HuskDelay;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::HuskDelay:
        if (bossDeathPhaseElapsed >= bossDeathHuskDelay)
        {
            if (gruxEnemyActor)
                gruxEnemyActor->RequestBeginHuskParticle();
            bossDeathPhase = BossDeathPhase::HuskPreview;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;

    case BossDeathPhase::HuskPreview:
        if (IsHuskComplete())
            EnterVictoryResult();
        break;
    }
}

void GameScene::LoadVictoryBestTime()
{
    // Same JSON representation as scene saves; only the boss clear record is stored.
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
            // Keep the previous record intact until the complete new JSON is written.
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
    const GameVictoryUIActor::VictoryRank victoryRank = victoryClearTime <= s ? GameVictoryUIActor::VictoryRank::S : victoryClearTime <= a ? GameVictoryUIActor::VictoryRank::A :victoryClearTime <= b ? GameVictoryUIActor::VictoryRank::B : GameVictoryUIActor::VictoryRank::C;
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
    if (battleFlowState != BattleFlowState::BossDead || !bossDeathShotsLoaded ||
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
    camera->BlendToPose(bossDeathShots[BossDeathResult].camera, blendDuration);
    ApplyBossDeathDof(bossDeathShots[BossDeathResult].dof);
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
    // Independent wall clock for diagnostics only; retain the final sample after the wait.
    if (battleFlowState == BattleFlowState::FinalHitSlow ||
        battleFlowState == BattleFlowState::FinalHitSlowRecovery ||
        battleFlowState == BattleFlowState::FinalHitAftermath)
    {
        const auto controller = gruxEnemyActor ? gruxEnemyActor->GetBodyAnimationController() : nullptr;
        if (controller && !finalHitReaction.empty() &&
            controller->GetCurrentAnimationName() == finalHitReaction)
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
            // A valid reaction continues at normal time if its configured cut
            // lies beyond the slow/recovery/aftermath window. Only a missing or
            // replaced reaction takes the anomaly escape.
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
        // Result UIButton callbacks own the selection and confirmation.
        deathPresentationElapsed += Time::UnscaledDeltaTime();
        UpdateDeathResultPresentation();
        break;
    case BattleFlowState::ResetForContinue:
        ResetBattleForContinue();
        break;
    case BattleFlowState::BossDead:
        UpdateBossDeathCinematic();
        break;
    case BattleFlowState::Victory:
        UpdateVictoryResult();
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


    Transform iceTr(DirectX::XMFLOAT3{ -13.537f,0.0f,10.757f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto iceActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<IceFragmentEmitterActor>("IceFragmentEmitterActor", iceTr);

    Transform debrisTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    this->GetActorManager()->CreateAndRegisterActorWithTransform<ModelDebrisEmitterActor>(
        "ModelDebrisEmitterActor", debrisTr);

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
    if (bossDeathSecondBgmActor) bossDeathSecondBgmActor->Stop(false);
    if (playerDeathBgmActor) playerDeathBgmActor->Stop(false);
    SceneBase::Uninitialize(device);
    Physics::Instance().Finalize();
    return true;
}

void GameScene::DrawGuiPlusAlpha()
{
#ifdef USE_IMGUI
    ImGui::Begin("GameScene");

    ImGui::SeparatorText("Tutorial Guide Ring");
    ImGui::Checkbox("Tutorial Guide Ring Force Visible", &tutorialDodgeGuideRingForceVisible);
    tutorialUIActor->DrawTutorialSettings();

    ImGui::SeparatorText("Tutorial Camera Profile Trigger");
    ImGui::DragFloat3("Corridor Exit Trigger Center", &tutorialCameraProfileTriggerCenter.x,
        0.05f, -100.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat3("Corridor Exit Trigger Size", &tutorialCameraProfileTriggerSize.x,
        0.05f, 0.05f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("Show Corridor Exit Trigger", &showTutorialCameraProfileTriggerDebug);
    ImGui::SeparatorText("Prison Opening Sitting");
    ImGui::DragFloat3("Sitting Position", &prisonOpeningSittingPosition.x,
        0.01f, -100.0f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Sitting Rotation Yaw", &prisonOpeningSittingYawDegrees,
        0.1f, -360.0f, 360.0f, "%.3f deg");
    ImGui::SeparatorText("Tutorial Skeleton UI");
    ImGui::DragFloat("HP Display Distance", &tutorialSkeletonHpDisplayDistance, 0.1f, 0.0f, 100.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Near Distance", &tutorialSkeletonHpNearDistance, 0.1f, 0.0f, 100.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Far Distance", &tutorialSkeletonHpFarDistance, 0.1f, 0.0f, 100.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Near Scale Multiplier", &tutorialSkeletonHpNearScaleMultiplier, 0.01f, 0.0f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Far Scale Multiplier", &tutorialSkeletonHpFarScaleMultiplier, 0.01f, 0.0f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("HP World Offset X", &tutorialSkeletonHpBarWorldOffset.x, 0.01f, -10.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("HP World Offset Y", &tutorialSkeletonHpBarWorldOffset.y, 0.01f, -10.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("HP World Offset Z", &tutorialSkeletonHpBarWorldOffset.z, 0.01f, -10.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("HP Bar Scale", &tutorialSkeletonHpBarScale, 0.01f, 0.05f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    tutorialUIActor->DrawPromptSettings();
    ImGui::Checkbox("Use Fixed Screen Position", &tutorialSkeletonUseFixedScreenPosition);
    ImGui::DragFloat2("HP Fixed Screen Position X / Y", &tutorialSkeletonHpFixedScreenPosition.x, 0.5f, -500.0f, 2420.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragInt(U8("Phase1 MaxHP"), &phase1MaxHp, 1.0f, 1, 500);
    ImGui::DragInt(U8("Phase2 MaxHP"), &phase2MaxHp, 1.0f, 1, 500);
    ImGui::DragFloat(U8("Phase2 FadeOut時間"), &phase2RecallFadeOutDuration,
        0.01f, 0.0f, 2.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(U8("Phase2 FadeIn時間"), &phase2RecallFadeInDuration,
        0.01f, 0.0f, 2.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    const float phase2FadeOutRemaining = phase2TransitionStep == Phase2TransitionStep::RecallFadeOut
        ? (std::max)(0.0f, phase2RecallFadeOutDuration - phase2StepElapsed) : 0.0f;
    const float phase2FadeInRemaining = phase2TransitionStep == Phase2TransitionStep::RecallFadeIn
        ? (std::max)(0.0f, phase2RecallFadeInDuration - phase2StepElapsed) : 0.0f;
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
        if (const auto& skeletalMesh = gruxEnemyActor->GetSkeletalMeshComponent())
        {
            DirectX::XMFLOAT3 phase2HairTint = skeletalMesh->GetPhase2HairTint();
            if (ImGui::ColorEdit3("Phase2 Hair Tint RGB", &phase2HairTint.x))
                skeletalMesh->SetPhase2HairTint(phase2HairTint);
        }
    }

    ImGui::DragFloat("Phase2 BGM Fade Out Duration", &phase2BgmFadeOutDuration,
        0.01f, 0.0f, 5.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Phase2 BGM Fade In Duration", &phase2BgmFadeInDuration,
        0.01f, 0.0f, 5.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    const float emotePreWaitElapsed = phase2TransitionStep == Phase2TransitionStep::PlayerEmotePreWait
        ? phase2StepElapsed : 0.0f;
    const float emotePostWaitElapsed = phase2TransitionStep == Phase2TransitionStep::PlayerEmotePostWait
        ? phase2StepElapsed : 0.0f;
    ImGui::DragFloat(U8("Phase2 Emote\u524DWait"), &phase2PlayerEmotePreWaitDuration,
        0.01f, 0.0f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(U8("Player Emote EndTime"), &phase2PlayerEmoteEndTime,
        0.001f, 0.001f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(U8("Phase2 Emote\u5F8CWait"), &phase2PlayerEmotePostWaitDuration,
        0.01f, 0.0f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    const bool playerLockOnActive = darkCameraActor &&
        (darkCameraActor->GetCameraMode() == DarkCameraActor::CameraMode::LockOn ||
            darkCameraActor->GetMovementMode() == DarkCameraActor::CameraMode::LockOn);
    static constexpr const char* darkCameraModeNames[] =
    { "TPS", "Focus", "LockOn", "Death" };
    const char* playerStateName = player && player->GetStateMachine()
        ? player->GetStateMachine()->GetStateName() : "None";
    ImGui::SeparatorText("LockOn Target UI");
    ImGui::DragFloat("LockOn Gather Start Offset", &lockOnTargetUIStartOffset, 0.5f, 0.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("LockOn Gather Duration", &lockOnTargetUIGatherDuration, 0.01f, 0.0f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("LockOn Hold Duration", &lockOnTargetUIHoldDuration, 0.01f, 0.0f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("LockOn Ring Rotation Speed", &lockOnTargetUIRotationSpeedDegree, 1.0f, -360.0f, 360.0f, "%.1f deg/sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SeparatorText("LockOn Target Overall Transform");
    ImGui::DragFloat("Skeleton LockOn Overall Scale", &skeletonLockOnTargetUIOverallScale, 0.01f, 0.01f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Skeleton LockOn Overall Offset X / Y", &skeletonLockOnTargetUIOverallOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Grux LockOn Overall Scale", &gruxLockOnTargetUIOverallScale, 0.01f, 0.01f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Grux LockOn Overall Offset X / Y", &gruxLockOnTargetUIOverallOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextDisabled("Final Offset: +Y is down in UI screen coordinates.");
    ImGui::DragFloat2("Top Final Offset X / Y", &lockOnTargetUITopFinalOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Bottom Final Offset X / Y", &lockOnTargetUIBottomFinalOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Left Final Offset X / Y", &lockOnTargetUILeftFinalOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Right Final Offset X / Y", &lockOnTargetUIRightFinalOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    const bool phase2BossIdleMaintained = phase2BossController &&
        (phase2TransitionStep == Phase2TransitionStep::PlayerEmotePreWait ||
            phase2TransitionStep == Phase2TransitionStep::PlayerEmote ||
            phase2TransitionStep == Phase2TransitionStep::PlayerEmotePostWait ||
            phase2TransitionStep == Phase2TransitionStep::ReturnToTps) &&
        phase2BossController->GetCurrentAnimationName() == "TravelMode_Idle_0";
    ImGui::SeparatorText("Phase2 Boss HP Preview");
    if (gruxEnemyActor)
        gruxEnemyActor->DrawPhase2HpBarPreviewGui();
    ImGui::SeparatorText("BGM Debug");
    ImGui::DragFloat("Game BGM Fade Duration", &gameBgmFadeDuration,
        0.01f, 0.0f, 5.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::Button(U8("ボスの部屋を明るくする")))
    {
        StartBossRoomLerp(0.0f, 1.0f, 3.0f);
    }
    if (ImGui::Button(U8("ボスの部屋を暗くする")))
    {
        StartBossRoomLerp(1.0f, 0.0f, 3.0f);
    }
    ImGui::Separator();
    ImGui::Text("Boss Battle Timer");
    ImGui::Text("Battle Time: %.3f", battleElapsedTime);
    ImGui::Text("Final Battle Time: %.3f%s", finalBattleTime,
        finalBattleTimeSaved ? "" : " (not saved)");
    battleTimerUIActor->SetElapsedTime(battleElapsedTime, numberTexWidth, numberTexHeight);
    ImGui::Separator();
    if (ImGui::TreeNode("Boss Death Cinematic Preview"))
    {
        static constexpr std::array<const char*, 13> phaseNames = {
            "FadeOut", "SetupCinematic", "FadeInScream",
            "DeathScream", "DeathFall", "DeathLanding",
            "PlayerApproach", "PlayerWalkStop", "RecallLeadIn", "RecallPingPong",
            "FinishTriggered", "HuskDelay", "HuskPreview"
        };
        ImGui::SeparatorText("Boss Death Flow Debug (read only)");
        static constexpr const char* battleFlowDebugNames[] = {
            "Intro", "Playing", "FinalHitSlow", "FinalHitSlowRecovery", "FinalHitAftermath", "PlayerDead",
            "ContinueWait", "ResetForContinue", "BossDead", "Victory"
        };
        const auto flowName = [&](BattleFlowState state)
            {
                const size_t index = static_cast<size_t>(state);
                return index < std::size(battleFlowDebugNames) ? battleFlowDebugNames[index] : "Unknown";
            };
        const size_t phaseIndex = static_cast<size_t>(bossDeathPhase);
        const char* phaseName = phaseIndex < phaseNames.size() ? phaseNames[phaseIndex] : "Unknown";
        const auto bossController = gruxEnemyActor
            ? gruxEnemyActor->GetBodyAnimationController() : nullptr;
        const char* actualAnimation = bossController
            ? bossController->GetCurrentAnimationName().c_str() : "None";
        const char* expectedAnimation = "None";
        bool animationCompletionMet = false;
        switch (bossDeathPhase)
        {
        case BossDeathPhase::DeathScream:
            expectedAnimation = "Ultimate_Roar_0";
            animationCompletionMet = bossController &&
                bossController->GetCurrentAnimationName() == expectedAnimation &&
                !bossController->IsPlayAnimation();
            break;
        case BossDeathPhase::DeathFall:
            expectedAnimation = "Result_Down_Start";
            animationCompletionMet = bossController &&
                bossController->GetCurrentAnimationName() == expectedAnimation &&
                !bossController->IsPlayAnimation();
            break;
        case BossDeathPhase::DeathLanding:
            expectedAnimation = "Death_B_0";
            animationCompletionMet = bossController &&
                bossController->GetCurrentAnimationName() == expectedAnimation &&
                bossController->GetCurrentAnimationTime() >= bossDeathFwdEndTime;
            break;
        case BossDeathPhase::SetupCinematic:
            animationCompletionMet = bossDeathShotsLoaded && player && gruxEnemyActor && cinemaCameraActor;
            break;
        case BossDeathPhase::HuskPreview:
            animationCompletionMet = IsHuskComplete();
            break;
        default:
            animationCompletionMet = true;
            break;
        }
        const bool animationMatch = std::string(expectedAnimation) == "None" ||
            (bossController && bossController->GetCurrentAnimationName() == expectedAnimation);

        std::string setupFailureReason;
        const auto addSetupFailure = [&](const char* reason)
            {
                if (!setupFailureReason.empty()) setupFailureReason += ", ";
                setupFailureReason += reason;
            };
        if (!bossDeathShotsLoaded) addSetupFailure("BossDeathPresetMissing");
        if (!player) addSetupFailure("PlayerMissing");
        if (!gruxEnemyActor) addSetupFailure("GruxMissing");
        if (!cinemaCameraActor) addSetupFailure("CinematicCameraMissing");
        if (setupFailureReason.empty()) setupFailureReason = "None";

        std::string huskWaitReason;
        if (!huskParticles) huskWaitReason = "HuskParticlesMissing";
        else if (bossDeathPhase != BossDeathPhase::HuskDelay &&
            bossDeathPhase != BossDeathPhase::HuskPreview) huskWaitReason = "PhaseNotHusk";
        else if (bossDeathPhase == BossDeathPhase::HuskDelay) huskWaitReason = "HuskDelay";
        else if (gruxHuskCaptureRequested || gruxHuskPreviewCaptureRequested) huskWaitReason = "CapturePending";
        else if (!gruxHuskPlaybackActive) huskWaitReason = "PlaybackNotStarted";
        else if (!IsHuskComplete()) huskWaitReason = "WaitingCompletion";
        else huskWaitReason = "None";

        const char* stopReason = "None";
        if (battleFlowState != BattleFlowState::BossDead) stopReason = "NotInBossDead";
        else if (bossDeathPhase == BossDeathPhase::SetupCinematic && setupFailureReason != "None") stopReason = "SetupCinematicWaiting";
        else if (bossDeathPhase == BossDeathPhase::DeathScream && !animationCompletionMet) stopReason = "WaitingDeathScreamAnimation";
        else if (bossDeathPhase == BossDeathPhase::DeathFall && !animationCompletionMet) stopReason = "WaitingDeathFallAnimation";
        else if (bossDeathPhase == BossDeathPhase::DeathLanding && !animationCompletionMet) stopReason = "WaitingDeathLandingAnimation";
        else if (bossDeathPhase == BossDeathPhase::HuskPreview && !IsHuskComplete()) stopReason = "WaitingHusk";

        ImGui::Separator();
        ImGui::DragFloat("Final Hit Slow Scale", &finalHitSlowScale, 0.01f, 0.01f, 1.0f);
        ImGui::DragFloat("Final Hit Slow Duration", &finalHitSlowDuration, 0.01f, 0.01f, 2.0f);
        ImGui::DragFloat("Final Hit Slow Recovery Duration", &finalHitSlowRecoveryDuration,
            0.01f, 0.0f, 1.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Final Hit Aftermath Duration", &finalHitAftermathDuration, 0.01f, 0.0f, 2.0f);
        if (gruxEnemyActor)
        {
            if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
            {
                const std::string reactionName = finalHitReaction.empty() ? "HitReact_Front" : finalHitReaction;
                const float duration = (std::max)(0.0f, controller->GetAnimationLength(reactionName));
                ImGui::DragFloat("Final Hit Reaction Cut Time", &finalHitReactionCutTime,
                    0.001f, 0.0f, duration, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
            }
        }
        ImGui::Separator();
        ImGui::SeparatorText("Victory Camera Intro");
        static constexpr const char* victoryPhaseNames[] = { "None", "CameraBlend", "WinEmote", "ResultDelay", "ShowButtons", "Interactive" };
        ImGui::Text("Victory Result Phase: %s", victoryPhaseNames[static_cast<size_t>(victoryResultPhase)]);
        ImGui::DragFloat("Result Camera Blend Duration", &victoryCameraBlendDuration,
            0.01f, 0.0f, 2.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
        const auto* resultCamera = cinemaCameraActor
            ? dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()) : nullptr;
        ImGui::DragFloat("Result Recall To Idle Blend Duration", &resultRecallToIdleBlendDuration,
            0.01f, 0.0f, 0.5f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Result UI Trigger Animation Time", &resultUITriggerAnimationTime,
            0.01f, 0.0f, victoryEmoteEndTime, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Victory S Limit", &victoryRankSLimit, 1.0f, 0.0f, 5999.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Victory A Limit", &victoryRankALimit, 1.0f, 0.0f, 5999.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Victory B Limit", &victoryRankBLimit, 1.0f, 0.0f, 5999.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
        victoryRankALimit = (std::max)(victoryRankSLimit, victoryRankALimit);
        victoryRankBLimit = (std::max)(victoryRankALimit, victoryRankBLimit);
        ImGui::TextDisabled("Rank limits apply to the next Victory.");
        victoryUIActor->SetNumberTextureSize(numberTexWidth, numberTexHeight);
        ImGui::DragFloat("Result Delay", &victoryResultDelayDuration,
            0.01f, 0.0f, 2.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat(U8("暗転時間"), &bossDeathFadeOutDuration, 0.01f, 0.01f, 5.0f);
        ImGui::DragFloat(U8("暗転解除時間"), &bossDeathFadeInDuration, 0.01f, 0.01f, 5.0f);

        ImGui::SeparatorText("Scream");
        ImGui::DragFloat("Ultimate Roar Start Time",
            &bossDeathRoarStartTime, 0.001f, 0.0f, 10.0f);
        ImGui::DragFloat("Ultimate Roar End Time",
            &bossDeathRoarEndTime, 0.001f, 0.0f, 10.0f);
        ImGui::DragFloat("Ultimate Roar Playback Rate",
            &bossDeathRoarPlaybackRate, 0.01f, 0.1f, 2.0f);

        ImGui::SeparatorText("Fall / Landing");
        ImGui::DragFloat("Stun Playback Rate",
            &bossDeathStunPlaybackRate, 0.01f, 0.1f, 2.0f);
        ImGui::DragFloat("Boss Death Stun To Death Blend",
            &bossDeathStunToDeathBlend, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Boss Death Scream Hold Min",
            &bossDeathScreamHoldMin, 0.001f, 0.0f, 10.0f);
        ImGui::DragFloat("Boss Death Scream Hold Max",
            &bossDeathScreamHoldMax, 0.001f, 0.0f, 10.0f);
        ImGui::DragFloat("Boss Death Scream Hold Rate",
            &bossDeathScreamHoldRate, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Fall To Landing Blend Duration",
            &bossDeathFallToLandingBlendDuration, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Death Fwd Start Time",
            &bossDeathFwdStartTime, 0.001f, 0.0f, 10.0f);
        ImGui::DragFloat("Death Fwd End Time",
            &bossDeathFwdEndTime, 0.001f, 0.0f, 10.0f);
        ImGui::DragFloat("Death Fwd Playback Rate",
            &bossDeathFwdPlaybackRate, 0.01f, 0.1f, 2.0f);
        ImGui::DragFloat("Death Fwd Prompt Min Time",
            &bossDeathFwdPromptMinTime, 0.001f, 0.0f, 10.0f);
        ImGui::DragFloat("Death Fwd Prompt Max Time",
            &bossDeathFwdPromptMaxTime, 0.001f, 0.0f, 10.0f);
        ImGui::DragFloat("Death Fwd Prompt Playback Rate",
            &bossDeathFwdPromptPlaybackRate, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat3("Death Fwd Boss Position Offset",
            &bossDeathFwdPositionOffset.x, 0.01f);

        ImGui::SeparatorText("Player Approach");
        ImGui::DragFloat3("Player Approach Start Position",
            &bossDeathApproachStartPosition.x, 0.01f);
        ImGui::DragFloat("Player Approach Duration",
            &bossDeathPlayerApproachDuration, 0.01f, 0.0f, 10.0f);

        ImGui::DragFloat("Boss Death Walk Stop Start Distance",
            &bossDeathWalkStopStartDistance, 0.01f, 0.0f, 2.0f, "%.2f m");

        ImGui::SeparatorText("Finish");
        ImGui::DragFloat3("Finish Player Position Offset",
            &bossDeathFinishPlayerPositionOffset.x, 0.01f);

        ImGui::SeparatorText("Finish UI");
        ImGui::DragFloat2("Finish UI Position Offset",
            &bossDeathFinishUIPositionOffset.x, 1.0f);
        ImGui::DragFloat2("Finish Word Position",
            &bossDeathFinishWordPosition.x, 1.0f);
        ImGui::DragFloat2("Finish Guide Position",
            &bossDeathFinishGuidePosition.x, 1.0f);
        ImGui::DragFloat2("Finish Y Button Position",
            &bossDeathFinishButtonPosition.x, 1.0f);
        ImGui::DragFloat2("Finish Word Scale",
            &bossDeathFinishWordScale.x, 0.01f);
        ImGui::DragFloat2("Finish Guide Scale",
            &bossDeathFinishGuideScale.x, 0.01f);
        ImGui::DragFloat2("Finish Y Button Scale",
            &bossDeathFinishButtonBaseScale.x, 0.01f);
        ImGui::DragFloat("Husk Start Delay",
            &bossDeathHuskDelay, 0.01f, 0.0f, 5.0f);

        ImGui::SeparatorText("Recall");
        ImGui::DragFloat("Recall Prompt Min Time",
            &bossDeathRecallPromptMinTime, 0.001f, 0.0f, RecallFirstSeTime);
        ImGui::DragFloat("Recall Prompt Max Time",
            &bossDeathRecallPromptMaxTime, 0.001f, 0.0f, RecallFirstSeTime);
        ImGui::DragFloat("Recall Prompt Playback Rate",
            &bossDeathRecallPromptPlaybackRate, 0.01f, 0.0f, 3.0f);
        ClampBossDeathPreviewTuning();
        ImGui::DragFloat("Finish Hold Min Time",
            &bossDeathFinishHoldMinTime, 0.001f);
        ImGui::DragFloat("Finish Hold Max Time",
            &bossDeathFinishHoldMaxTime, 0.001f);
        ImGui::DragFloat("Finish Hold Playback Rate",
            &bossDeathFinishHoldPlaybackRate, 0.01f, 0.0f, 3.0f);
        ClampBossDeathPreviewTuning();
        ImGui::Text("Finish Hold Time: %.3f", bossDeathFinishHoldTime);
        ImGui::Text("Recall Prompt Time: %.3f", bossDeathRecallPromptTime);
        ImGui::TreePop();
    }
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

    ImGui::End();

    if (darkCameraActor && darkCameraActor->IsNormalEnemyLockOnScreenRadiusDebugEnabled() && player)
    {
        float viewportX = 0.0f;
        float viewportY = 0.0f;
        float viewportWidth = 0.0f;
        float viewportHeight = 0.0f;
        Graphics::GetViewport(viewportX, viewportY, viewportWidth, viewportHeight);
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        drawList->PushClipRect(
            ImVec2(viewportX, viewportY),
            ImVec2(viewportX + viewportWidth, viewportY + viewportHeight), true);

        const float maxDistance = darkCameraActor->GetLockOnTargetSelectionMaxDistance();
        const float maxDistanceSq = maxDistance * maxDistance;
        for (const auto& enemy : GetActorManager()->GetActorsOfType<Enemy>())
        {
            if (!enemy || std::dynamic_pointer_cast<GruxEnemy>(enemy))
                continue;

            const auto target = std::dynamic_pointer_cast<SceneComponent>(
                enemy->FindComponentByName("cameraTargetComponent"));
            if (!target)
                continue;

            DirectX::XMFLOAT3 delta = MathHelper::Subtract(enemy->GetPosition(), player->GetPosition());
            delta.y = 0.0f;
            const float distanceSq = delta.x * delta.x + delta.z * delta.z;
            const auto projection = darkCameraActor->ProjectWorldPositionForUI(
                target->GetComponentLocation());
            const bool insideScreenRadius = projection.valid && projection.inFront &&
                projection.insideViewport &&
                darkCameraActor->IsNormalEnemyLockOnProjectionWithinScreenRadius(projection);
            const bool candidate = !enemy->IsDefeated() && !enemy->IsPendingKill() &&
                !bossRoomLockOnScopeActive && distanceSq <= maxDistanceSq && insideScreenRadius;
            const ImU32 color = candidate
                ? IM_COL32(80, 255, 110, 255)
                : insideScreenRadius ? IM_COL32(255, 180, 50, 255)
                : IM_COL32(255, 80, 80, 255);

            if (projection.valid && projection.inFront)
            {
                const ImVec2 position{ projection.screenPosition.x, projection.screenPosition.y };
                drawList->AddCircle(position, 7.0f, color, 16, 2.0f);
                drawList->AddLine(
                    ImVec2(position.x - 10.0f, position.y),
                    ImVec2(position.x + 10.0f, position.y), color, 1.5f);
                drawList->AddLine(
                    ImVec2(position.x, position.y - 10.0f),
                    ImVec2(position.x, position.y + 10.0f), color, 1.5f);
                const char* status = candidate ? "Candidate"
                    : insideScreenRadius ? "Other filter" : "Out of range";
                const std::string label = enemy->GetName() + " [" + status + "]";
                drawList->AddText(ImVec2(position.x + 12.0f, position.y - 8.0f), color, label.c_str());
            }
        }
        drawList->PopClipRect();
    }
#endif

    if (darkCameraActor && darkCameraActor->IsNormalEnemyLockOnScreenRadiusDebugEnabled() && player)
    {
        const DirectX::XMFLOAT3 playerPosition = player->GetPosition();
        const float radius = darkCameraActor->GetLockOnTargetSelectionMaxDistance();
        constexpr int segmentCount = 48;
        constexpr float twoPi = DirectX::XM_2PI;
        const DirectX::XMFLOAT4 rangeColor{ 0.25f, 0.85f, 1.0f, 1.0f };
        for (int segment = 0; segment < segmentCount; ++segment)
        {
            const float firstAngle = twoPi * static_cast<float>(segment) / segmentCount;
            const float secondAngle = twoPi * static_cast<float>(segment + 1) / segmentCount;
            DirectX::XMFLOAT3 first{
                playerPosition.x + std::cos(firstAngle) * radius,
                playerPosition.y + 0.05f,
                playerPosition.z + std::sin(firstAngle) * radius };
            DirectX::XMFLOAT3 second{
                playerPosition.x + std::cos(secondAngle) * radius,
                playerPosition.y + 0.05f,
                playerPosition.z + std::sin(secondAngle) * radius };
            DebugRender::DrawLine(first, second, rangeColor, 0.0f, true);
        }
        DebugRender::DrawSphere(
            { playerPosition.x, playerPosition.y + 0.05f, playerPosition.z },
            0.12f, rangeColor, 0.0f, true);
    }

    if (showTutorialCameraProfileTriggerDebug)
    {
        DebugRender::DrawBox(tutorialCameraProfileTriggerCenter, tutorialCameraProfileTriggerSize,
            tutorialCameraProfileTriggerFired
            ? DirectX::XMFLOAT4{ 0.2f, 0.9f, 0.35f, 1.0f }
            : DirectX::XMFLOAT4{ 1.0f, 0.75f, 0.15f, 1.0f },
            0.0f, true);
    }
}


// ボスの部屋の色ラープ値を設定する
void GameScene::SetBossRoomLerpFactor(float lerpFactor)
{
    auto& shader = Scene::GetCurrentScene()->GetSceneSettings().sceneShaderConstants;
    shader.bossRoomLerpFactor = lerpFactor;
}

// ボスの目のみBloomをオンにする

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
