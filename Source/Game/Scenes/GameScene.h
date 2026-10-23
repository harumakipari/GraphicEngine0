#pragma once

#include "Engine/Scene/Scene.h"

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <array>

#include "Engine/Scene/SceneBase.h"
#include "Game/Actors/BgmActor.h"
#include "Game/DarkGame/GameBgmDirector.h"
#include "Game/DarkGame/GameTutorialController.h"
#include "Game/Actors/Camera/DarkGameCamera.h"



#include "Game/Actors/Camera/LoadingCamera.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Actors/UI/GameBossDeathUIActor.h"
#include "Game/Actors/UI/GameDefeatUIActor.h"
#include "Game/Actors/UI/GameMovieSkipUIActor.h"
#include "Game/Actors/UI/GameTimerUIActor.h"
#include "Game/Actors/UI/GameLockOnTargetUIActor.h"
#include "Game/Actors/UI/GameTutorialUIActor.h"
#include "Game/Actors/UI/GameVictoryUIActor.h"
#include "Game/DarkGame/GameBossDeathController.h"
#include "Game/DarkGame/GamePlayerDeathController.h"
#include "Game/DarkGame/GameVictoryController.h"
#include "Game/DarkGame/DarkActors/DarkClothActor.h"
#include "Game/DarkGame/DarkActors/DarkStageAsset.h"

#include "UI/Widgets/Widget.h"

#include "PBD/PBDSystem.h"

class GruxEnemy;
class Enemy;
class SkeletonWarriorActor;
class StaticMeshComponent;
class SceneComponent;

class GameScene : public SceneBase
{
public:
    enum class BossPhase : uint8_t
    {
        Phase1,
        TransitionToPhase2,
        Phase2,
    };

    enum class BattleFlowState : uint8_t
    {
        Intro,
        Playing,
        FinalHitSlow,
        FinalHitSlowRecovery,
        FinalHitAftermath,
        PlayerDead,
        ContinueWait,
        ResetForContinue,
        BossDead,
        Victory,
    };

public:
    GameScene() {}
    ~GameScene() override;

    bool Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override;

    void Start() override;

    void Update(float deltaTime) override;

    bool Uninitialize(ID3D11Device* device) override;

    void DrawGuiPlusAlpha() override;

    void SetUpActors()override;

    static inline Scene::Autoenrollment<GameScene> _autoenrollment;

    void SetBossRoomLerpFactor(float lerpFactor);
    void StartBossRoomLerp(float startFactor, float endFactor, float duration, std::function<void()> finished = nullptr);
    void CancelBossRoomLerp();

    void SetDeathPresentationCueCallback(std::function<void(GamePlayerDeathController::Cue)> callback);

    void BeginGameBgmFadeOut();
    void PlayBossBattleBgm();

    // Called when the boss-room door movie begins, before the boss-introduction blend completes.
    void EnterBossRoomLockOnScope(bool fromTutorialDoorMovie = true);

    // Called when the existing boss-introduction camera blend has completed.
    void StartBossBattle();
    void SkipBossDeathToFinishPrompt();
    bool IsBossDeathFinishPromptActive() const;
    bool IsBossDeathCinematicAudioActive() const;

    void PrepareTutorialForBossEntry();
    bool RequestBossEntryFromPause();
    bool RequestRestartBossBattleFromPause();
    void RequestReturnToMainRoomStart();
    // Runtime prison-opening entry only; MovieCameraComponent debug playback does not call these.
    bool BeginPrisonOpeningMoviePlayerSitting();
    void EndPrisonOpeningMoviePlayerSitting();

    enum class LockOnTargetSelectionResult : uint8_t
    {
        Selected,
        NoCandidate,
        Invalidated,
    };
    LockOnTargetSelectionResult UpdateLockOnTargetSelection();
    // Read-only labels consumed by Grux animation-request diagnostics.
    BossPhase GetBossPhase() const { return bossPhase; }
    int GetBossCurrentHp() const;
    int GetBossMaxHp() const;
    int GetPhase2MaxHpForPresentation() const { return phase2MaxHp; }
    void NotifyPhase2BossRoar();
    bool IsBossPhaseTransitionActive() const { return bossPhase == BossPhase::TransitionToPhase2; }
    bool IsPhase1BreakPending() const { return phase1BreakPending; }
    bool IsBossInFinalPhase() const { return bossPhase == BossPhase::Phase2; }
private:
#ifdef USE_IMGUI
    void DrawTutorialDebugGui();
    void DrawPhase2DebugGui();
    void DrawBgmAndBattleTimerDebugGui();
    void DrawBossDeathDebugGui();
    void DrawPlayerDeathDebugGui();
#endif
    void UpdateTutorialCameraProfileTrigger();
    bool EvaluateLockOnCandidate(const std::shared_ptr<Enemy>& enemy,
        std::shared_ptr<SceneComponent>& outTarget, float& outDistanceSq,
        float& outScreenDistanceSq) const;
    void UpdateOperationGuideTutorial(LockOnTargetSelectionResult selectionResult);
    void CompleteTutorialForBossBattle();
    void UpdateLockOnTargetUI(float deltaTime);
    void StartJudgeDodgeOperationTutorial();
    void CloseJudgeDodgeOperationTutorial();
    void UpdateTutorialDodgeGuideRing();
    void SpawnTutorialSkeletons();
    void CreateTutorialSkeletonUI();
    void UpdateTutorialSkeletonUI();
    void HideTutorialSkeletonUI();
    enum class Phase1FinalHitTimePhase : uint8_t
    {
        None,
        HitStop,
        Slow,
    };

    enum class Phase2TransitionStep : uint8_t
    {
        None,
        RecallFadeOut,
        RecallFadeIn,
        RecallPreWait,
        BossRecall,
        PlayerEmotePreWait,
        PlayerEmote,
        PlayerEmotePostWait,
        ReturnToTps,
    };

    struct CinematicActorPose
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct Phase2CinematicShot
    {
        CinematicCameraComponent::CameraPose camera{};
        CinematicActorPose player{};
        CinematicActorPose boss{};
    };

    void UpdateBattleFlow();
    void BeginPhase2Transition();
    void CompletePhase2TransitionAtBlack();
    void UpdatePhase2Transition();
    void ResetBossPhaseRuntime(BossPhase phase);
    void ApplyBossPhaseHp(BossPhase phase);
    void CaptureContinueBossCheckpoint();
    void BeginPhase1BreakPending();
    void UpdatePhase1BreakPending();
    void StartPhase1FinalHitTimeSequence();
    void UpdatePhase1FinalHitTimeSequence();
    bool LoadPhase2CinematicShots();
    void BeginPhase2Cinematic();
    void SetupPhase2RecallCinematic();
    void UpdatePhase2Cinematic();
    void ApplyPhase2RecallActorPose();
    void CutToPhase2Shot(size_t shotIndex);
    void BeginPhase2TpsReturnBlend();
    void SetBattleHudVisible(bool visible);
    void DisableCinematicCameraDebugInput();
    void EnterPlayerDead();
    void ResetBattleForContinue();
    void ResetBattleFacingAndCamera();
    void EnterBossDead();
    void BeginPhase2FinalHitHpFallback();
    bool IsPhase2FinalHitGateComplete() const;
    void RestartBossBattle();
    void UpdateMovieSkip();
    GameMovieSkipUIActor::MovieSkipType GetActiveMovieSkipType();
    void CreateBossDeathFadeUI();
    void SetBossDeathFadeAlpha(float alpha);
    void ExecuteVictoryResult(int selection);
    std::shared_ptr<GameVictoryUIActor> victoryUIActor;
    void EnterVictoryResult();
    void UpdateVictoryResult();
    void ExecuteDeathResult(int index);

    std::shared_ptr<StageAsset> mainRoomAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> bossRoomAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> transitionAreaAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageCandelabraAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageGroundBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageMeltedWaxAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageStandingBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageCandleStandAsset = std::make_shared<StageAsset>();

    std::thread loadStageThread;
    std::thread loadStageAssetsThread;

    std::unique_ptr<PBD::System> pbd;


    std::shared_ptr<InterleavedGltfModel> model;

    std::shared_ptr<Player> player;

    std::shared_ptr<GruxEnemy> gruxEnemyActor;

    std::shared_ptr<GameLockOnTargetUIActor> lockOnTargetUIActor;
    bool lockOnInputHeldLastFrame = false;
    bool lockOnTargetSelectedForHeldInput = false;

    bool bossRoomLockOnScopeActive = false;
    bool lockOnRequiresReleaseAfterBossRoomEntry = false;

    void OnPlayerFinalHit(GruxEnemy* boss, const DirectX::XMFLOAT3& source);

    float finalHitSlowScale = 0.05f;
    float finalHitSlowDuration = 2.0f;
    float finalHitAftermathDuration = 1.f;
    float finalHitSlowRecoveryDuration = 0.30f;
    float finalHitRecoveryElapsed = 0.0f;
    float finalHitRecoveryDurationActive = 0.0f;
    float finalHitRecoveryProgress = 0.0f;
    float recoveryStartScale = 1.0f;
    float currentRecoveryScale = 1.0f;
    float finalHitTimer = 0.0f;
    bool finalHitPending = false;
    bool phase2FinalHitGateActive = false;
    bool phase2FinalHitReactionComplete = false;
    bool phase2FinalHitAttackWaitRequired = false;

    bool finalHitReactionCutReached = false;
    std::string finalHitReaction;
    BattleFlowState battleFlowState = BattleFlowState::Intro;
    BossPhase bossPhase = BossPhase::Phase1;
    BossPhase continueCheckpointPhase = BossPhase::Phase1;
    int continueCheckpointBossHp = 30;
    int continueCheckpointBossMaxHp = 30;
    int phase1MaxHp = 30;
    int phase2MaxHp = 55;
    bool phase1BreakPending = false;
    bool phase2BossRoarHpBarStarted = false;
    bool phase1BreakWaitingForRush = false;
    bool phase1FinalHitFadePending = false;
    Phase1FinalHitTimePhase phase1FinalHitTimePhase = Phase1FinalHitTimePhase::None;
    float phase1FinalHitTimeElapsed = 0.0f;
    float phase1FinalHitHitStopDuration = 0.12f;
    float phase1FinalHitSlowScale = 0.15f;
    Phase2TransitionStep phase2TransitionStep = Phase2TransitionStep::None;
    std::array<Phase2CinematicShot, 2> phase2CinematicShots{};
    bool phase2CinematicShotsLoaded = false;
    float phase2StepElapsed = 0.0f;
    float phase2RecallFadeOutDuration = 1.5f;
    float phase2RecallFadeInDuration = 1.0f;
    float phase2RecallFadeAlpha = 0.0f;
    float phase2RecallPreWaitDuration = 0.01f;
    float phase2TransformStartTime = 5.3f;
    float phase2TransformEndTime = 8.0f;
    float phase2TransformProgress = 0.0f;
    bool phase2TransformCompleted = false;
    float phase2PlayerEmotePreWaitDuration = 0.5f;
    float phase2PlayerEmoteEndTime = 0.515f;
    float phase2PlayerEmotePostWaitDuration = 0.5f;
    bool phase2PlayerEmoteActive = false;
    bool phase2PlayerEmoteEndPoseHeld = false;
    float phase2TpsReturnBlendDuration = 1.0f;
    Transform playerBattleStartTransform{};
    Transform bossBattleStartTransform{};
    bool battleStartTransformsSaved = false;
    bool tutorialBossEntryPending = false;
    float battleElapsedTime = 0.0f;
    float finalBattleTime = 0.0f;
    bool finalBattleTimeSaved = false;
    std::unique_ptr<GameBossDeathController> bossDeathController;
    std::unique_ptr<GamePlayerDeathController> playerDeathController;
    std::unique_ptr<GameVictoryController> victoryController;
    std::shared_ptr<GameMovieSkipUIActor> movieSkipUIActor;
    std::shared_ptr<GameBossDeathUIActor> bossDeathUIActor;
    std::shared_ptr<UIImageComponent> bossDeathFadeOverlay;

    std::shared_ptr<GameTimerUIActor> battleTimerUIActor;
    std::shared_ptr<GameDefeatUIActor> defeatUIActor;
    float deathResultMoveSeVolume = 0.15f;
    float deathResultConfirmSeVolume = 0.1f;
    float numberTexWidth = 198.f;
    float numberTexHeight = 300.f;


    std::shared_ptr<CinemaCamera> cinemaCameraActor;

    std::shared_ptr<DarkCameraActor> darkCameraActor;

    // A scene-owned, non-physical volume: it has no collider and only changes
    // the camera profile after the player crosses the tutorial corridor exit.
    DirectX::XMFLOAT3 tutorialCameraProfileTriggerCenter{ -25.0f, 1.0f, 10.75f };
    DirectX::XMFLOAT3 tutorialCameraProfileTriggerSize{ 2.0f, 3.0f, 8.0f };
    bool tutorialCameraProfileTriggerFired = false;
    DirectX::XMFLOAT3 prisonOpeningSittingPosition{ -66.919f, -0.078f, 11.892f };
    // Authored from the common Player rotation in open_movie1/2/3/5/6/7 ScenePresets.
    float prisonOpeningSittingYawDegrees = 107.031867f;
    Transform mainRoomStartPlayerTransform{ DirectX::XMFLOAT3{ -13.537f,0.0f,10.757f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.07f,1.07f,1.07f } };
    bool prisonOpeningMovieStartRequested = false;
    bool backToRoomStartRequested = false;
    GameTutorialController tutorialController;
    bool judgeDodgeOperationTutorialOwnsPause = false;
    float judgeDodgeOperationTutorialPreviousTimeScale = 1.0f;
    std::shared_ptr<GameTutorialUIActor> tutorialUIActor;
    std::shared_ptr<SkeletonWarriorActor> tutorialPassiveSkeletonActor;
    std::shared_ptr<SkeletonWarriorActor> tutorialDodgeSkeletonActor;
    std::weak_ptr<Enemy> lockOnSelectedEnemy;
    std::unique_ptr<EasingRunner> bossLerpEasing;
    float bossLerpEasingFactor = 0.0f;
    bool startBossRoomLerp = false;
    std::function<void()> onFinished;
    float startBossRoomLerpFactor = 0.0f;
    float endBossRoomLerpFactor = 1.0f;

    GameBgmDirector bgmDirector;
    std::shared_ptr<BgmActor> gameBgmActor;
    std::shared_ptr<BgmActor> bossBgmActor;
    std::shared_ptr<BgmActor> phase2BgmActor;
    std::shared_ptr<BgmActor> bossDeathSecondBgmActor;
    std::shared_ptr<BgmActor> playerDeathBgmActor;
    const float bossBgmVolume = 0.2f;

    // 布アクター
    std::array<std::shared_ptr<DarkClothActor>, 10> darkClothActors;
};
