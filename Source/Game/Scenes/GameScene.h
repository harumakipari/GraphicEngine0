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
#include "Game/DarkGame/GamePhase2TransitionController.h"
#include "Game/DarkGame/GamePlayerDeathController.h"
#include "Game/DarkGame/GameVictoryController.h"
#include "Game/DarkGame/DarkActors/DarkClothActor.h"
#include "Game/DarkGame/DarkActors/DarkStageAsset.h"

#include "UI/Widgets/Widget.h"

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

    enum class LockOnTargetSelectionResult : uint8_t
    {
        Selected,
        NoCandidate,
        Invalidated,
    };

private:
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

public:
    GameScene() {}
    ~GameScene()override {}

    bool Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override;
    void Start() override;
    void Update(float deltaTime) override;
    bool Uninitialize(ID3D11Device* device) override;
    void DrawGuiPlusAlpha() override;
    void SetUpActors()override;

    static inline Scene::Autoenrollment<GameScene> _autoenrollment;

    // ムービー・カメラ
    void SetBossRoomLerpFactor(float lerpFactor);
    void StartBossRoomLerp(float startFactor, float endFactor, float duration, std::function<void()> finished = nullptr);
    void CancelBossRoomLerp();
    void EnterBossRoomLockOnScope(bool fromTutorialDoorMovie = true);
    void PrepareTutorialForBossEntry();
    bool BeginPrisonOpeningMoviePlayerSitting() const;
    void EndPrisonOpeningMoviePlayerSitting() const;

    // 戦闘・BGM
    void BeginGameBgmFadeOut();
    void PlayBossBattleBgm();
    void StartBossBattle();
    void SkipBossDeathToFinishPrompt();
    void NotifyPhase2BossRoar();

    // Pause・シーン遷移
    bool RequestBossEntryFromPause();
    bool RequestRestartBossBattleFromPause();
    void RequestReturnToMainRoomStart();

    // 戦闘状態
    LockOnTargetSelectionResult UpdateLockOnTargetSelection();
    BossPhase GetBossPhase() const { return bossPhase; }
    int GetPhase2MaxHpForPresentation() const { return phase2MaxHp; }
    bool IsBossPhaseTransitionActive() const { return bossPhase == BossPhase::TransitionToPhase2; }
    bool IsPhase1BreakPending() const { return phase1BreakPending; }
    bool IsBossInFinalPhase() const { return bossPhase == BossPhase::Phase2; }
    bool IsBossDeathFinishPromptActive() const;
    bool IsBossDeathCinematicAudioActive() const;

private:
    // コントローラー生成
    void CreateController();

    // ゲームUIを生成
    void CreateUI();

    // Tutorial・Lock-on
    void UpdateTutorialCameraProfileTrigger();
    bool EvaluateLockOnCandidate(const std::shared_ptr<Enemy>& enemy,
        std::shared_ptr<SceneComponent>& outTarget, float& outDistanceSq,
        float& outScreenDistanceSq) const;
    void UpdateOperationGuideTutorial(LockOnTargetSelectionResult selectionResult);
    void CompleteTutorialForBossBattle();
    void UpdateLockOnTargetUI(float deltaTime) const;
    void StartJudgeDodgeOperationTutorial();
    void CloseJudgeDodgeOperationTutorial();
    void UpdateTutorialDodgeGuideRing();
    void UpdateTutorialSkeletonUI();
    void HideTutorialSkeletonUI() const;

    // 戦闘進行・最終ヒット
    void UpdateBattleFlow();
    void OnPlayerFinalHit(GruxEnemy* boss, const DirectX::XMFLOAT3& source);
    void EnterPlayerDead();
    void EnterBossDead();
    void SetBattleHudVisible(bool visible) const;
    void BeginPhase2FinalHitHpFallback();
    bool IsPhase2FinalHitGateComplete() const;

    // Phase1 / Phase2移行
    void BeginPhase2Transition();
    void CompletePhase2TransitionAtBlack();
    void UpdatePhase2Transition();
    void ResetBossPhaseRuntime(BossPhase phase);
    void ApplyBossPhaseHp(BossPhase phase) const;   // フェーズごとにボスのHPを設定する
    void BeginPhase1BreakPending();
    void UpdatePhase1BreakPending();
    void StartPhase1FinalHitTimeSequence();
    void UpdatePhase1FinalHitTimeSequence();
    void BeginPhase2Cinematic();
    void SetupPhase2RecallCinematic() const;
    void UpdatePhase2Cinematic();
    void BeginPhase2TpsReturnBlend();

    // Continue・Restart・Result
    void CaptureContinueBossCheckpoint();
    void ResetBattleForContinue();
    void ResetBattleFacingAndCamera() const;
    void RestartBossBattle();
    void ExecuteDeathResult(int index);
    void ExecuteVictoryResult(int selection);
    void EnterVictoryResult();  // Player勝利リザルトの処理を開始する
    void UpdateVictoryResult();

    // Movie Skip・Fade・Cinematic Camera
    void DisableCinematicCameraDebugInput() const;
    void UpdateMovieSkip();
    GameMovieSkipUIActor::MovieSkipType GetActiveMovieSkipType();
    void SetBossDeathFadeAlpha(float alpha) const;

    // Debug・ImGui
#ifdef USE_IMGUI
    void DrawTutorialDebugGui();
    void DrawPhase2DebugGui();
    void DrawBgmAndBattleTimerDebugGui();
    void DrawBossDeathDebugGui();
    void DrawPlayerDeathDebugGui();
#endif
    // UI Actor（Victory Result）
    std::shared_ptr<GameVictoryUIActor> victoryUIActor;

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

    // Actor参照（Player・Boss）    
    std::shared_ptr<Player> player;
    std::shared_ptr<GruxEnemy> gruxEnemyActor;

    // UI Actor（Lock-on）   
    std::shared_ptr<GameLockOnTargetUIActor> lockOnTargetUIActor;
    bool lockOnInputHeldLastFrame = false;
    bool lockOnTargetSelectedForHeldInput = false;

    bool bossRoomLockOnScopeActive = false;
    bool lockOnRequiresReleaseAfterBossRoomEntry = false;

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
    float phase2StepElapsed = 0.0f;
    float phase2RecallFadeOutDuration = 1.5f;
    float phase2RecallFadeInDuration = 1.0f;
    float phase2RecallFadeAlpha = 0.0f;
    float phase2RecallPreWaitDuration = 0.01f;
    float phase2PlayerEmotePreWaitDuration = 0.5f;
    float phase2PlayerEmotePostWaitDuration = 0.5f;
    float phase2TpsReturnBlendDuration = 1.0f;
    Transform playerBattleStartTransform{};
    Transform bossBattleStartTransform{};
    bool battleStartTransformsSaved = false;
    bool tutorialBossEntryPending = false;
    float battleElapsedTime = 0.0f;
    float finalBattleTime = 0.0f;
    bool finalBattleTimeSaved = false;
    // Controller・Manager（戦闘演出・リザルト）   
    std::unique_ptr<GameBossDeathController> bossDeathController;
    std::unique_ptr<GamePlayerDeathController> playerDeathController;
    std::unique_ptr<GameVictoryController> victoryController;
    std::unique_ptr<GamePhase2TransitionController> phase2TransitionController;
    // UI Actor・UI部品（死亡・リザルト）
    std::shared_ptr<GameMovieSkipUIActor> movieSkipUIActor;
    std::shared_ptr<GameBossDeathUIActor> bossDeathUIActor;
    std::shared_ptr<UIImageComponent> bossDeathFadeOverlay;

    std::shared_ptr<GameTimerUIActor> battleTimerUIActor;
    std::shared_ptr<GameDefeatUIActor> defeatUIActor;
    float deathResultMoveSeVolume = 0.15f;
    float deathResultConfirmSeVolume = 0.1f;
    float numberTexWidth = 198.f;
    float numberTexHeight = 300.f;


    // Camera Actor参照
    std::shared_ptr<CinemaCamera> cinemaCameraActor;
    std::shared_ptr<DarkCameraActor> darkCameraActor;

    bool tutorialCameraProfileTriggerFired = false;// チュートリアルカメラを使うか
    bool prisonOpeningMovieStartRequested = false; //牢屋ムービーを再生するか



    DirectX::XMFLOAT3 tutorialCameraProfileTriggerCenter{ -25.0f, 1.0f, 10.75f };
    DirectX::XMFLOAT3 tutorialCameraProfileTriggerSize{ 2.0f, 3.0f, 8.0f };
    bool backToRoomStartRequested = false;
    bool judgeDodgeOperationTutorialOwnsPause = false;
    float judgeDodgeOperationTutorialPreviousTimeScale = 1.0f;
    // Tutorial Actor・UI参照
    GameTutorialController tutorialController;  // チュートリアルコントローラー
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
    std::shared_ptr<BgmActor> bossDeathBgmActor;
    std::shared_ptr<BgmActor> playerDeathBgmActor;
    const float bossBgmVolume = 0.2f;

    // 布アクター
    std::array<std::shared_ptr<DarkClothActor>, 10> darkClothActors;
};
