#pragma once

#include "Engine/Scene/Scene.h"

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <array>

#include "Core/ActorManager.h"
#include "Engine/Scene/SceneBase.h"
#include "Game/Actors/BgmActor.h"
#include "Game/DarkGame/GameBgmDirector.h"
#include "Game/Actors/Camera/DarkGameCamera.h"


#include "Graphics/Renderer/SceneRenderer.h"

#include "Game/Actors/Camera/LoadingCamera.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Actors/Stage/ClothSimulate.h"
#include "Game/Actors/UI/GameBossDeathUIActor.h"
#include "Game/Actors/UI/GameDefeatUIActor.h"
#include "Game/Actors/UI/GameMovieSkipUIActor.h"
#include "Game/Actors/UI/GameTimerUIActor.h"
#include "Game/Actors/UI/GameLockOnTargetUIActor.h"
#include "Game/Actors/UI/GameTutorialUIActor.h"
#include "Game/Actors/UI/GameVictoryUIActor.h"
#include "Game/DarkGame/DarkActors/DarkClothActor.h"
#include "Game/DarkGame/DarkActors/DarkStageAsset.h"

#include "UI/Widgets/Widget.h"

#include "PBD/PBDSystem.h"

class GruxEnemy;
class Enemy;
class SkeletonWarriorActor;
class StaticMeshComponent;
class CoreStandaloneAudioSource;
class SceneComponent;

class GameScene : public SceneBase
{
    enum class DeathStagingArea : uint8_t
    {
        Center, Front, Back, Left, Right, FrontLeft, FrontRight, BackLeft, BackRight
    };

    struct DeathStagingAreaSettings
    {
        float bossDistance = 4.5f;
    };

public:
    // Boss health remains phase-local for future phase-specific UI.
    enum class BossPhase : uint8_t
    {
        Phase1,
        TransitionToPhase2,
        Phase2,
    };

    enum class DeathPresentationCue : uint8_t
    {
        GameplayHudFade,
        OverlayFade,
        DefeatedFade,
        BattleTimeFade,
        ButtonsFade,
    };

    void SetDeathPresentationCueCallback(std::function<void(DeathPresentationCue)> callback)
    {
        deathPresentationCueCallback = std::move(callback);
    }

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


    void BeginGameBgmFadeOut();
    void PlayBossBattleBgm();

    // Called when the boss-room door movie begins, before the boss-introduction blend completes.
    void EnterBossRoomLockOnScope(bool fromTutorialDoorMovie = true);

    // Called when the existing boss-introduction camera blend has completed.
    void StartBossBattle();
    void SkipBossDeathToFinishPrompt();
    bool IsBossDeathFinishPromptActive() const
    {
        return battleFlowState == BattleFlowState::BossDead &&
            bossDeathPhase >= BossDeathPhase::RecallPingPong;
    }
    bool IsBossDeathCinematicAudioActive() const
    {
        return battleFlowState == BattleFlowState::BossDead &&
            bossDeathPhase < BossDeathPhase::RecallPingPong;
    }

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

    enum class BossDeathPhase : uint8_t
    {
        FadeOut,
        SetupCinematic,
        FadeInScream,
        DeathScream,
        DeathFall,
        DeathLanding,
        PlayerApproach,
        PlayerWalkStop,
        RecallLeadIn,
        RecallPingPong,
        FinishTriggered,
        HuskDelay,
        HuskPreview,
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

    struct BossDeathDofState
    {
        float focusDistance = 0.0f;
        float nearRange = 0.0f;
        float range = 0.0f;
        float blurStrength = 0.0f;
        int enableBlur = 0;
        int enableDof = 0;
    };

    struct BossDeathShotState
    {
        CinematicCameraComponent::CameraPose camera{};
        CinematicActorPose player{};
        CinematicActorPose boss{};
        BossDeathDofState dof{};
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
    void StageDeathActors();
    DeathStagingArea DetermineDeathStagingArea(const DirectX::XMFLOAT3& originalPlayerPosition) const;
    void OnPlayerDeathCameraStart();
    void ResetBattleForContinue();
    void ResetBattleFacingAndCamera();
    void EnterBossDead();
    void BeginPhase2FinalHitHpFallback();
    bool IsPhase2FinalHitGateComplete() const;
    void RestartBossBattle();
    bool LoadBossDeathShots();
    GameMovieSkipUIActor::MovieSkipType GetActiveMovieSkipType();
    void CreateBossDeathFadeUI();
    void SetBossDeathFadeAlpha(float alpha);
    void StartBossDeathGroanLoop();
    void StopBossDeathGroanLoop();
    void UpdateBossDeathPromptLoop(float deltaTime);
    void UpdateBossDeathScreamHold(float deltaTime);
    void UpdateBossDeathCinematic();
    bool SetupBossDeathCinematic();
    void ApplyBossDeathDof(const BossDeathDofState& dof);
    void CutToBossDeathShot(size_t shotIndex);
    void ClampBossDeathPreviewTuning();
    enum class VictoryResultPhase { None, CameraBlend, WinEmote, ResultDelay, ShowButtons, Interactive };
    void ExecuteVictoryResult(int selection);
    std::shared_ptr<GameVictoryUIActor> victoryUIActor;
    void EnterVictoryResult();
    void UpdateVictoryResult();
    void LoadVictoryBestTime();
    void CaptureVictoryResult();
    bool victoryBestTimeValid = false;
    double victoryBestTime = 0.0;
    float victoryRankSLimit = 90.0f;
    float victoryRankALimit = 120.0f;
    float victoryRankBLimit = 180.0f;
    float resultRecallToIdleBlendDuration = 0.26f;
    // Weapon bone drops rapidly between 1.13 and 1.20 seconds in Emote_Slice_0.
    float resultUITriggerAnimationTime = 1.17f;
    VictoryResultPhase victoryResultPhase = VictoryResultPhase::None;
    float victoryResultDelayElapsed = 0.0f;
    float victoryResultDelayDuration = 0.70f;
    float victoryCameraBlendDuration = 0.60f;
    static constexpr float victoryEmoteEndTime = 1.48f;
    void ExecuteDeathResult(int index);
    void UpdateDeathResultPresentation();
    void ResetDeathPresentationCues();
    void FireDeathPresentationCue(DeathPresentationCue cue, bool& fired);

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
    // Selection happens once per held-input start; Rush keeps its own target.
    bool lockOnInputHeldLastFrame = false;
    bool lockOnTargetSelectedForHeldInput = false;

    // The door movie marks the gameplay boundary between corridor enemies and Grux.
    // An input held through that transition must not acquire Grux automatically.
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
    // Transition is not checkpointable; Continue uses only stable combat phases.
    BossPhase continueCheckpointPhase = BossPhase::Phase1;
    int continueCheckpointBossHp = 30;
    int continueCheckpointBossMaxHp = 30;
    // Tuning values apply at battle reset and at the Phase1 -> Phase2 handoff.
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
    bool phase2TpsReturnBlendActive = false;
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
    std::string phase2CurrentShot = "None";
    Transform playerBattleStartTransform{};
    Transform bossBattleStartTransform{};
    bool battleStartTransformsSaved = false;
    bool tutorialBossEntryPending = false;
    bool tutorialCompletedForBossBattle = false;
    bool deathCameraStartRequested = false;
    float playerDeadElapsed = 0.0f;
    float battleElapsedTime = 0.0f;
    float finalBattleTime = 0.0f;
    bool finalBattleTimeSaved = false;
    std::array<BossDeathShotState, 5> bossDeathShots{};
    bool bossDeathShotsLoaded = false;
    BossDeathDofState bossDeathGameplayDof{};
    bool bossDeathGameplayDofCaptured = false;
    BossDeathPhase bossDeathPhase = BossDeathPhase::FadeOut;
    float bossDeathPhaseElapsed = 0.0f;
    // Diagnostic-only death-flow event history. It never drives game logic.
    int bossDeathEnterCallCount = 0;
    BattleFlowState bossDeathLastEnterPreviousFlow = BattleFlowState::Intro;
    std::string bossDeathLastHpZeroDetectionSource = "Other";
    float bossDeathFadeOutDuration = 1.6f;
    float bossDeathFadeInDuration = 1.6f;
    float bossDeathRoarStartTime = 0.17f;
    float bossDeathRoarEndTime = 1.176f;
    float bossDeathRoarPlaybackRate = 0.71f;
    float bossDeathStunPlaybackRate = 0.5f;
    float bossDeathStunToDeathBlend = 0.25f;
    float bossDeathFallToLandingBlendDuration = 1.0f;
    float bossDeathFwdStartTime = 0.498f;
    float bossDeathFwdEndTime = 2.233f;
    float bossDeathFwdPlaybackRate = 0.44f;
    float bossDeathFwdPromptMinTime = 1.38f;
    float bossDeathFwdPromptMaxTime = 1.625f;
    float bossDeathFwdPromptTime = 1.55f;
    float bossDeathFwdPromptDirection = 1.0f;
    float bossDeathFwdPromptPlaybackRate = 0.10f;
    float bossDeathScreamHoldMin = 1.474f;
    float bossDeathScreamHoldMax = 1.588f;
    float bossDeathScreamHoldRate = 0.30f;
    float bossDeathScreamHoldTime = 1.50f;
    float bossDeathScreamHoldDirection = 1.0f;
    DirectX::XMFLOAT3 bossDeathFwdPositionOffset{ 0.6f,0.0f,0.0f };
    float bossDeathPlayerApproachDuration = 2.7f;
    float bossDeathWalkStopStartDistance = 0.35f;
    DirectX::XMFLOAT3 bossDeathWalkStopStartPosition{};
    DirectX::XMFLOAT3 bossDeathWalkStopFinishPosition{};
    DirectX::XMFLOAT4 bossDeathWalkStopStartRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT4 bossDeathWalkStopFinishRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 bossDeathApproachStartPosition{ 5.025f,-0.1f,11.723f };
    DirectX::XMFLOAT4 bossDeathApproachStartRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 bossDeathFinishPlayerPositionOffset{ -0.9f,0.0f,0.0f };
    float bossDeathRecallPromptMinTime = 1.155f;
    float bossDeathRecallPromptMaxTime = 1.225f;
    float bossDeathRecallPromptTime = 1.163f;
    float bossDeathRecallPromptDirection = 1.0f;
    float bossDeathRecallPromptPlaybackRate = 0.07f;
    float bossDeathFinishHoldMinTime = 1.72f;
    float bossDeathFinishHoldMaxTime = 1.875f;
    float bossDeathFinishHoldPlaybackRate = 0.07f;
    float bossDeathFinishHoldTime = 1.58f;
    float bossDeathFinishHoldDirection = 1.0f;
    float bossDeathRecallFinishHitTime = 1.875f;
    float bossDeathHuskDelay = 1.4f;
    bool bossDeathFinishInputEnabled = false;
    std::shared_ptr<GameMovieSkipUIActor> movieSkipUIActor;
    std::shared_ptr<GameBossDeathUIActor> bossDeathUIActor;
    std::shared_ptr<UIImageComponent> bossDeathFadeOverlay;

    std::shared_ptr<CoreStandaloneAudioSource> bossDeathGroanAudio;
    std::shared_ptr<CoreStandaloneAudioSource> bossDeathVoiceAudio;
    std::shared_ptr<GameTimerUIActor> battleTimerUIActor;
    float deathAttemptTime = 0.0f;
    float deathPresentationElapsed = 0.0f;
    float deathHudFadeDuration = 0.60f;
    float deathOverlayStartTime = 4.6f;
    float deathDefeatedStartTime = 5.0f;
    float deathBattleTimeStartTime = 5.1f;
    float deathButtonsStartTime = 5.2f;
    float deathSelectLineStartTime = 4.40f;
    float deathResultInputEnableTime = 5.2f;
    std::shared_ptr<GameDefeatUIActor> defeatUIActor;
    float deathResultMoveSeVolume = 0.15f;
    float deathResultConfirmSeVolume = 0.1f;
    bool deathHudFadeCueFired = false;
    bool deathOverlayCueFired = false;
    bool deathDefeatedCueFired = false;
    bool deathBattleTimeCueFired = false;
    bool deathButtonsCueFired = false;
    std::function<void(DeathPresentationCue)> deathPresentationCueCallback;
    float numberTexWidth = 198.f;
    float numberTexHeight = 300.f;


    // Death staging bounds in world XZ coordinates.
    float deathStagingMinPlayerX = 0.0f;
    float deathStagingMaxPlayerX = 17.425f;
    float deathStagingMinPlayerZ = 1.0f;
    float deathStagingMaxPlayerZ = 20.45f;
    float deathStagingWallInsetX = 0.8f;
    float deathStagingWallInsetZ = 0.8f;
    std::array<DeathStagingAreaSettings, 9> deathStagingAreaSettings{};


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
    enum class JudgeDodgeOperationTutorialState : uint8_t { Unseen, Showing, Completed };
    JudgeDodgeOperationTutorialState judgeDodgeOperationTutorialState = JudgeDodgeOperationTutorialState::Unseen;
    bool judgeDodgeOperationTutorialOwnsPause = false;
    float judgeDodgeOperationTutorialPreviousTimeScale = 1.0f;
    std::shared_ptr<GameTutorialUIActor> tutorialUIActor;
    bool tutorialDodgeGuideRingActive = false;
    bool tutorialDodgeGuideRingFinished = false;
    float tutorialDodgeGuideRingElapsed = 0.0f;
    float tutorialDodgeGuideRingRangeGraceDuration = 0.25f;
    bool tutorialDodgeGuideRingStartedInAttackRange = false;
    bool tutorialDodgeGuideRingPreviousAttackRange = false;
    std::shared_ptr<SkeletonWarriorActor> tutorialPassiveSkeletonActor;
    bool tutorialPassiveSkeletonLockOnCandidate = false;
    int tutorialPassiveSkeletonYHpBaseline = -1;
    bool tutorialPassiveSkeletonYHpBaselineCaptured = false;
    int tutorialDodgeSkeletonYHpBaseline = -1;
    bool tutorialDodgeSkeletonYHpBaselineCaptured = false;
    std::shared_ptr<SkeletonWarriorActor> tutorialDodgeSkeletonActor;
    bool tutorialDodgeSkeletonAttackRange = false;
    bool tutorialDodgeGuideActivated = false;
    bool tutorialDodgeSkeletonJustDodged = false;
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

    // 布アクター
    std::array<std::shared_ptr<DarkClothActor>, 10> darkClothActors;
};
