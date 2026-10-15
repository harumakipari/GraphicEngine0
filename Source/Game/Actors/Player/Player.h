#pragma once
#include <memory>
#include <array>
#include "Game/Actors/Base/Character.h"

#include "Components/Controller/ControllerComponent.h"
#include "Components/Render/MeshComponent.h"

#include "Core/ActorManager.h"
#include "Graphics/Renderer/TrailRenderer.h"
#include "UI/Widgets/Widget.h"

class AudioSourceComponent;
class IInteractable;
class Enemy;
class GruxEnemy;

class Player :public Character
{
public:
    enum class DodgeDirection :uint8_t
    {
        Forward,
        Backward,
        Left,
        Right,
    };

    enum class HitReactionDirection :uint8_t
    {
        Front,
        Back,
        Left,
        Right,
    };

    enum class ActionType :uint8_t
    {
        None,
        Attack,
        Dodge,
    };

    enum class DamagePolicy :uint8_t
    {
        Normal,
        IgnoreDodgeIFrames,
    };

    enum class JustDodgeSlowPhase : uint8_t
    {
        Inactive,
        Hold,
        Return,
        RushHold,
    };

    struct ActionRequest
    {
        ActionType type = ActionType::None;
        float remainTime = 0.0f;
        DodgeDirection dodgeDirection = DodgeDirection::Forward;
        DirectX::XMFLOAT3 dodgeWorldDirection{};
        bool useDodgeWorldDirection = false;
    };

    // チュートリアルの操作UI
    enum class OperationGuideItem : uint8_t
    {
        L,
        LT,
        Y,
        X,
        R,
        Count,
    };

    struct OperationGuideItemState
    {
        std::shared_ptr<UIImageComponent> image;
        DirectX::XMFLOAT2 position{};
        DirectX::XMFLOAT2 baseScale{ 0.35f, 0.35f };
        bool visible = false;
        bool learned = false;
        float animationTimer = 0.0f;
    };

    enum class LocomotionMode :uint8_t
    {
        None,
        TPSWalk,
        TPSRun,
        LockOnBlendRun,
        Idle,
    };
public:
    explicit Player(const std::string& modelName) :Character(modelName)
    {
        mass = 50.0f;
        maxHp = 50;
        maxHp = 3;
        hp = maxHp;
    }

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void Finalize()override;

    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

    void OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event) override;

    void DrawAnimationEditorPreviewState(const AnimationNotifyState& state) override;

    // NotifyEnd が省略されたアニメーション切替でも、前のNotify由来状態を解除する。
    void OnAnimationChanged() override;

    // 入力・移動
    LocomotionMode GetLocomotionMode() const { return locomotionMode; }

    float GetPrisonOpeningSittingSampleTime() const { return prisonOpeningSittingSampleTime; }

    bool IsPrisonOpeningSittingPingPongForward() const { return prisonOpeningSittingPingPongForward; }

    bool IsPrisonOpeningUsingStandingPingPong() const { return prisonOpeningUseStandingPingPong; }

    // ブレンドスペースのアニメーションを使用するかの更新関数
    void UpdateLocomotionAnimation() override;

    void RestartLocomotionAnimation();

    // TPSモードの移動時の更新処理
    void UpdateTPSLocomotion();

    // ロックオンモードの移動時の更新処理
    void UpdateLockOnLocomotion();

    // カメラ・演出・イベント
    // イベントシーン開始時に呼ぶ処理
    void StartEvent();

    // イベントシーン終了時に呼ぶ処理
    void EndEvent();

    // Stops only the low-HP audiovisual presentation. HP is not modified.
    void StopLowHpPresentation();


    void SetLowHpPresentationCinematicSuppressed(bool suppressed);

    // Prison-opening movie only. This bypasses normal gameplay while retaining this Player actor.
    bool BeginPrisonOpeningSitting(const DirectX::XMFLOAT3& position, float yawDegrees);

    bool BeginPrisonOpeningStanding();

    void ConfigurePrisonOpeningSittingMotion(bool useStandingPingPong, float startTime,float endTime, float speed, float standingBlendTime);

    void EndPrisonOpeningSitting();

    bool BeginPrisonOpeningEyeClosed();

    void SetPrisonOpeningEyeClosedWeight(float weight);

    void EndPrisonOpeningEyeOverride();

    bool IsPrisonOpeningEyeOverrideActive() const;

    bool IsPrisonOpeningSitting() const { return prisonOpeningSitting; }

    bool IsPrisonOpeningStandingFinished() const;

    void LogPrisonOpeningTransitionDebug(const char* phase, int frame) const;

    // HUD・操作ガイド
    // Scene が表示方針を決定し、Player は所有するHUDへ反映する。
    // Battle HUD visibility is decided by GameScene; Player only owns its components.
    void SetHpBarVisible(bool visible);
    void SetGameplayHudVisible(bool visible);
    void SetOperationGuideHudVisible(bool visible);
    void SetOperationGuideItemVisible(OperationGuideItem item, bool visible);
    void SetOperationGuideItemLearned(OperationGuideItem item, bool learned = true);
    bool IsOperationGuideItemVisible(OperationGuideItem item) const;
    bool IsOperationGuideItemLearned(OperationGuideItem item) const;
    bool IsDodging() const { return stateMachine_ && std::string(stateMachine_->GetStateName()) == "Dodge"; }
    void SetMinimumHp(int value);
    void ClearMinimumHp();
    void RestoreHpToMaxWithUiAnimation();
    void BeginGameplayHudFadeOut();
    void SetGameplayHudFadeAlpha(float alpha);

    // 戦闘遷移・最終ヒット
    // Clears Player-owned transient combat state and restores full HP at the saved battle start.
    void ResetForBattleContinue(const Transform& battleStartTransform);
    void ForceDirectionImmediate(const DirectX::XMFLOAT3& direction);

    void BeginDeathEyeClose();
    void UpdateDeathEyeClose(float deathElapsedTime);
    void EndDeathEyeClose();
    void UpdateDeathVisualFade(float deathElapsedTime);

    void SetFinalHitCallback(std::function<void(GruxEnemy*, const DirectX::XMFLOAT3&)> callback)
    { finalHitCallback = std::move(callback); }
    bool BeginFinalHitWait();

    void SetDeathCameraStartCallback(std::function<void()> callback)
    {
        deathCameraStartCallback = std::move(callback);
    }
    void SetDeathCameraTransparencyDisabled(bool disabled)
    {
        deathCameraTransparencyDisabled = disabled;
    }

    // Stops residual attacks when the battle has ended without restoring HP.
    void StopBattleActions();
    // Releases the temporary phase-transition action lock without resetting HP or position.
    void ResumeBattleActionsAfterEvent();
    bool IsRushActiveForPhaseTransition() const
    {
        return stateMachine_ && std::string(stateMachine_->GetStateName()) == "Rush";
    }
    bool IsRushActive() const { return IsRushActiveForPhaseTransition(); }
    // Read-only Rush presentation state for camera synchronization.
    bool IsRushWindupActive() const { return rushWindupActive; }
    bool IsRushAttackActive() const { return rushAttackActive; }
    float GetRushWindupDuration() const { return rushWindupDuration; }
    void SetRushWindupActive(bool active) { rushWindupActive = active; }
    void SetRushAttackActive(bool active) { rushAttackActive = active; }
    bool IsJustDodgePresentationActive() const
    {
        return stateMachine_ && std::string(stateMachine_->GetStateName()) == "Dodge" && justDodgeSuccess;
    }
    std::shared_ptr<Enemy> GetRushTarget() const { return rushTarget.lock(); }

    // Shared by lethal hits in either phase: finish the active clip and suppress follow-ups.
    void RequestFinalHitRushFollowUpStop()
    {
        if (IsRushActiveForPhaseTransition())
            finalHitRushFollowUpStopRequested = true;
    }
    void RequestFinalHitVisualPoseLatch()
    {
        finalHitVisualPoseLatchRequested = true;
    }
    bool IsFinalHitVisualPoseLatchRequested() const
    {
        return finalHitVisualPoseLatchRequested;
    }
    bool IsPhase1LastHitAttackFinishedForTransition() const
    {
        const auto controller = GetBodyAnimationController();
        if (!controller || !controller->IsVisualPoseLatched() || !stateMachine_)
            return false;
        const std::string stateName = stateMachine_->GetStateName();
        return stateName != "Attack" && stateName != "Rush";
    }
    bool IsFinalHitRushFollowUpStopRequested() const
    {
        return finalHitRushFollowUpStopRequested;
    }
    void ClearFinalHitRushFollowUpStop()
    {
        finalHitRushFollowUpStopRequested = false;
    }
    bool IsFinalHitAttackDrainComplete() const;
    bool IsFinalHitAttackDrainActive() const { return finalHitAttackDrainActive; }
    bool IsFinalHitAttackDrainLatched() const { return finalHitAttackDrainCompleted; }
    void SetPhase2RushFinalHitDebugEnabled(bool enabled)
    {
        if (!enabled) phase2RushFinalHitDebugEventId = 0;
    }

    void SetPhase2RushFinalHitDebugEventId(uint64_t eventId) { phase2RushFinalHitDebugEventId = eventId; }
    uint64_t GetPhase2RushFinalHitDebugEventId() const { return phase2RushFinalHitDebugEventId; }
    void MarkFinalHitAttackDrainInterrupted();

    // Clears temporary combat visuals/actions without changing HP, transform,
    // or the current player state.
    void ClearTransientBattleActions();
    void ClearBattleVisualsForPhaseTransition();
    // Safely leaves gameplay actions before a cinematic changes the Player transform.
    void CancelGameplayActionsForCinematic();
    // Leaves the player in a deterministic, non-locomotion state before a cinematic.
    void NeutralizeForPhase2Cinematic();
    // Clears the player-owned sword trail history and sampling cache before a cinematic teleport.
    void ClearSwordTrailForPhaseTransition();
    bool IsSwordTrailActiveForPhaseTransition() const
    {
        return showTrail || !trail.trailPoints.empty();
    }
    bool IsLockOnGuideVisible() const { return lockOnGuideVisible; }

    // Enters the terminal, animation-playing state used after the boss is defeated.
    void EnterWinState();
    bool IsInWinState() const;

    // 描画・残像演出
    // 軌跡を描画する処理
    void RenderTrail(ID3D11DeviceContext* immediateContext);

    void CapturePlayerPoseGhost();
    void ResetPlayerPoseGhost();

    // カメラ連携
    // Focus開始時のForwardを設定する
    void SetFocusDirection(const DirectX::XMFLOAT3& focusDir)
    {
        this->focusDirection = focusDir;
    }

    // 回避方向を取得する
    DodgeDirection GetDodgeDirection()const { return dodgeDirection; }
    const DirectX::XMFLOAT3& GetDodgeWorldDirection() const { return dodgeWorldDirection; }
    bool UsesDodgeWorldDirection() const { return useDodgeWorldDirection; }
    // Resolves and stores the world-space direction for the current Dodge.
    void CaptureDodgeWorldDirectionSnapshot();

    void SetIsPlayerTransparency(const bool isTransparency) { moviePerform = !isTransparency; }

    // モード変更用関数
    void SetLocomotionMode(LocomotionMode mode);

    // デバッグ・ImGui
    void DrawImGuiDetails()override;

private:
    // 演出用の姿勢・Notify補助
    bool RebuildEyeClosePoseOverride();
    void ResetEyeCloseOverride();

    void HandleAnimationPlaySE(const AnimationNotifyEvent& event);

    DirectX::XMFLOAT3 GetHitBoxPoint(const std::shared_ptr<SceneComponent>& component, const DirectX::XMFLOAT3& localOffset) const;

    // 入力要求の内部処理
    // 入力処理をまとめる
    void CaptureActionRequest(float deltaTime);

    bool StoreActionRequest(ActionType type, float remainTime);

public:
    // 入力要求・移動
    // 保存済みの入力要求を現在の状態で実行可能なState遷移へ変換し、成功時だけ消費する。
    // 入力コマンドによってステートが変わるかどうか
    bool TryExecuteActionRequest();

    // 入力を消費する処理
    void ConsumeActionRequest(ActionType expectedType);

    // 戦闘・攻撃判定
    // 無敵判定を含め、成立した被弾だけを適用する
    bool TryTakeDamage(int damage, const DirectX::XMFLOAT3& attackerPosition,
        DamagePolicy policy = DamagePolicy::Normal);

    bool StartKnockBack(const DirectX::XMFLOAT3& direction);

    // True only while a Boss may begin a new attack against this Player.
    // Future GetUp grace time belongs inside this API.
    bool IsBossAttackTargetAvailable() const;

    void ClearActionRequest(const char* reason);

    HitReactionDirection GetHitReactionDirection() const { return hitReactionDirection; }

    // 攻撃開始時の処理
    void StartAttack();

    // ジャスト回避・Rush
    // ジャスト回避成功時の処理　ラッシュ受付期間開始
    void StartJustDodgeSuccess(const std::shared_ptr<Enemy>& enemy);

    bool CanAcceptInitialRushInput() const;
    bool LatchFinalHitVisualPoseAfterAnimationUpdate();
    bool CanShowInitialRushGuide() const;
    bool CanShowRushComboGuide() const;
    bool CanShowRushPrompt() const;
    bool IsRushOpportunityActive() const;
    void SetRushInputAcceptance(bool accepting, const char* endReason = nullptr);
    void SetRushInputDebugState(bool judgeSuccess, bool rushRequested);
    void LogInitialRushInputTrace(const char* event, const char* detail = nullptr);

    // Player/Bossの解除タイミングを独立して管理する。
    void BeginPlayerSlowReturn();
    void BeginBossSlowReturn(bool afterRush = false);
    void HoldBossSlowForRush();
    void ForceResetPlayerSlow();
    void ForceResetBossSlow();

    // Rush State owns the lifetime of this visual override.
    void SetRushWeaponVisual(bool enabled);

    int GetMaxRushAttackCount() const { return std::clamp(maxRushAttackCount, 1, 7); }

    // ジャスト回避を受け付けるかどうか
    bool GetJustDodgeWindow()const { return  justDodgeWindow; }
    // Checks the stored Dodge direction against an arbitrary attacker position.
    bool CanJustDodgeAgainst(const DirectX::XMFLOAT3& attackerPosition);
    void BeginDodgeDebug();
    void UpdateDodgeDebug(float deltaTime);
    void RecordNormalDodgeDebug();
    // カメラ・ボス戦連携
    // カメラの目の位置を取得する
    const std::shared_ptr<SceneComponent>& GetCameraEyeComponent() { return cameraEyeComponent; }

    // カメラの注視点の位置
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }

    void AcquireAttackTarget();
    void UpdateAttackTargetRotation(float deltaTime);
    void StopAttackTargetRotation() { attackRotationTracking = false; }
    void ClearAttackTarget();

    // ボス戦時かどうかを設定する
    void SetIsBossBattle(const bool isBossBattle) { this->isBossBattle = isBossBattle; }
    bool IsBossBattle() const { return isBossBattle; }

private:
    // ノックバック・移動更新
    void BeginKnockBackMovement();
    void UpdateKnockBackMovement(float deltaTime);
    void StopKnockBackForcedMove();
    void EndKnockBackMovement();

    // 動作更新処理
    void UpdateMovement();
    void UpdateWeaponVisualPresentation(float deltaTime);

    // HUD・操作ガイドの内部更新
    void UpdateRushPromptUI();
    void UpdateLockOnGuideUI();
    void HideAndResetLockOnGuideUI();
    void InitializeOperationGuideUI();
    void UpdateOperationGuideUI();
    void RefreshOperationGuideUIVisibility();

    // 戦闘計算・回避方向の補助
    float GetRushDamageMultiplier() const;
    int GetCurrentAttackDamage() const;

    // 回避の方向を決定する処理
    void DecideLockOnDodgeDirection();


private:
    DirectX::XMFLOAT3 knockBackDirection{ 0.0f, 0.0f, 1.0f };
    float knockBackInitialSpeed = 8.0f;
    float knockBackDuration = 0.45f;
    float knockBackElapsed = 0.0f;
    bool knockBackActive = false;
    bool knockBackForcedMoveActive = false;

    // インタラクト対象検索
    IInteractable* FindInteractable();

public:
    // ヒット中に当たった敵を記録する
    std::unordered_set<Actor*> hitActors;

    std::string currentAttackAnimation = "Primary_Attack_Fast_A";
    std::string startAttackAnimation = "Primary_Attack_Fast_A";    // コンボ開始のアニメーション

    bool comboQueued = false;   // コンボ攻撃がキューに入っているかどうか
    bool inputWindow = false;   // コンボ受付をするかどうか
    bool hitBox = false;   // 武器の当たり判定をつける
    float activeHitBoxRadius = 0.75f;
    DirectX::XMFLOAT3 activeHitBoxOffset{ 0.0f, 0.0f, 0.0f };
    bool transitionWindow = false;  // ステート遷移してもいいかどうか
    bool justDodgeWindow = false;  // ジャスト回避受付時間
    bool justDodgeSuccess = false; // ジャスト回避成功フラグ
    bool showTrail = false;     // 軌跡を出現させるかどうか
    float swordEmissivePower = 0.0f;    // 剣のエミッシブの力

    // アニメーション時にどれくらい移動するか
    std::vector<AnimationMotionWarp> animationMotionWarps;
    // 操作UI
    // These presentation states are independent of HUD-wide visibility so an
    // event can hide the HUD without losing tutorial progress.
    std::array<OperationGuideItemState,
        static_cast<size_t>(OperationGuideItem::Count)> operationGuideItems;
    bool operationGuideGameplayHudVisible = true;
    bool operationGuideEventVisible = true;
    float operationGuidePulseAmplitude = 0.185f;
    float operationGuidePulseSpeed = 3.5f; // radians / second


    bool invincibleWindow = false; // アニメーションによる無敵状態かどうか
    bool invincible = false; // 無敵状態かどうか


    float justDodgeMaxTowardAttackerDot = 0.25f;
    DirectX::XMFLOAT3 dodgeWorldDirectionSnapshot{};
    bool dodgeWorldDirectionSnapshotValid = false;
    DirectX::XMFLOAT3 directionToAttackerDebug{};
    float justDodgeDirectionDotDebug = 0.0f;
    bool justDodgeDirectionResultDebug = false;
    float dodgeDebugElapsed = 0.0f;
    bool normalDodgeRecordedThisDodge = false;
    bool justDodgeRecordedThisDodge = false;
    bool lastJustDodgeValid = false;
    std::string lastJustDodgeAttack = "None";
    float lastJustDodgeTime = 0.0f;
    float lastJustAnimationTime = 0.0f;
    float lastJustWindowStart = 0.0f;
    float lastJustWindowEnd = 0.0f;
    float lastJustWindowRatio = 0.0f;
    float lastJustBossHitBoxElapsed = -1.0f;
    float moveToEnemyInterval = 0.2f;  // ラッシュ後の敵までへのダッシュにかかる時間
    float rushApproachPoseBlendTime = 0.12f;
    float rushWindupDuration = 0.20f;
    bool rushWindupActive = false;
    bool rushAttackActive = false;

    // 通常攻撃でどれくらい攻撃を補正するか
    float normalEnemyRushSurfaceMargin = 0.15f;
    float motionWarpDesiredAttackSurfaceDistance = 0.5f;
    float attackRotationMaxCorrectionDegrees = 55.0f;
    float attackRotationSpeedDegrees = 240.0f;
    float attackAssistMaxDistance = 8.0f;
    std::weak_ptr<Enemy> attackTarget;
    float attackRotationStartYaw = 0.0f;
    bool attackRotationTracking = false;

    std::weak_ptr<Enemy> rushTarget; // ターゲットを選択

    enum class RushPromptAnimationPhase : uint8_t
    {
        Hidden,
        AppearGrow,
        AppearSettle,
        PulseGrow,
        PulseReturn,
    };

    bool rushInputAccepting = false;
    // Diagnostic-only clock and state for measuring the initial Just Dodge -> Rush input window.
    bool initialRushInputTraceActive = false;
    uint64_t initialRushInputTraceFrame = 0;
    float initialRushInputTraceGameTime = 0.0f;
    float initialRushInputTraceUnscaledTime = 0.0f;
    float initialRushInputTraceStartGameTime = 0.0f;
    float initialRushInputTraceStartUnscaledTime = 0.0f;
    bool finalHitRushFollowUpStopRequested = false;
    bool finalHitVisualPoseLatchRequested = false;
    bool finalHitAttackDrainActive = false;
    bool finalHitAttackDrainCompleted = false;
    bool finalHitAttackDrainFailed = false;
    uint64_t phase2RushFinalHitDebugEventId = 0;
    bool rushInputEndNotifyReceivedDebug = false;
    std::string rushInputEndReasonDebug = "None";
    bool rushJudgeSuccessDebug = false;
    bool rushRequestedDebug = false;
    float rushPromptAlpha = 0.0f;
    float rushPromptFadeInDuration = 0.10f;
    RushPromptAnimationPhase rushPromptAnimationPhase = RushPromptAnimationPhase::Hidden;
    float rushPromptAnimationTimer = 0.0f;
    bool rushPromptWasVisible = false;

    // ラッシュのUIの数値
    DirectX::XMFLOAT2 rushGuidePosition = { 1200.0f, 680.0f };
    DirectX::XMFLOAT2 rushButtonPosition = { 1200.0f, 600.0f };
    DirectX::XMFLOAT2 rushWordPosition = { 1197.0f, 532.0f };
    DirectX::XMFLOAT2 rushGuideSize = { 488.0f, 379.f };
    DirectX::XMFLOAT2 rushButtonSize = { 300.0f, 300.0f };
    DirectX::XMFLOAT2 rushWordSize = { 124.5f, 68.0f };
    DirectX::XMFLOAT2 rushGuideScale = { 0.32f, 0.32f };
    DirectX::XMFLOAT2 rushButtonBaseScale = { 0.32f, 0.32f };
    DirectX::XMFLOAT2 rushWordScale = { 1.0f, 1.0f };

    // 入力受付のコマンド
    ActionRequest bufferCommand{}; // 入力コマンド

    // Rush combat tuning values. Kept together for future runtime tuning.
    int normalAttackDamage = 2;
    float rushDamageMultiplier = 1.f;
    float finalRushDamageMultiplier = 2.5f;
    int maxRushAttackCount = 7;

    float normalAttackHitStopDuration = 0.05f; // ヒットストップの秒数
    float rushHitStopDuration = 0.03f;

public:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<InputComponent> inputComponent;
    std::shared_ptr<RotationComponent> rotationComponent;
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;

    std::shared_ptr<SceneComponent> swordRootComponent; // 剣の根元のコンポーネント
    std::shared_ptr<SceneComponent> swordMiddleComponent; // 剣の中間のコンポーネント
    std::shared_ptr<SceneComponent> swordTipComponent;  // 剣の先端のコンポーネント

    // 剣の描画用メッシュコンポーネント
    std::shared_ptr<SkeletalMeshComponent> swordMeshComponent;

    // 剣の残像用
    struct SwordGhost
    {
        std::shared_ptr<SkeletalMeshComponent> swordMeshComp;
        DirectX::XMFLOAT4X4 world = { };
        float alpha;
        bool isVisible = false;
    };
    std::array<SwordGhost, 8> ghosts;

    // Just Dodge成立時のPlayer固定Pose Ghost
    struct PlayerPoseGhost
    {
        std::shared_ptr<SkeletalMeshComponent> renderConstantsComponent;
        std::vector<InterleavedGltfModel::Node> nodes;
        DirectX::XMFLOAT4X4 world = {};
        float alpha = 0.0f;
        float elapsedTime = 0.0f;
        bool isVisible = false;
    };
    std::array<PlayerPoseGhost, 6> playerPoseGhosts;

    // 歩きのスピード
    float walkSpeed = 1.0f;
    // 走りのスピード
    float runSpeed = 5.15f;
    float forwardSpeedScale = 1.0f;
    float sideSpeedScale = 0.95f;
    float backwardSpeedScale = 1.0f;

    // ラッシュ時のUI
    std::shared_ptr<UIImageComponent> rushGuideImageComponent;
    std::shared_ptr<UIImageComponent> rushButtonImageComponent;
    std::shared_ptr<UIImageComponent> rushWordImageComponent;
    std::shared_ptr<UIImageComponent> lockOnGuideArrowImageComponent;
    std::shared_ptr<UIImageComponent> lockOnGuideButtonImageComponent;

    // ロックオンのガイドUIの調整値
    float lockOnGuideOffscreenElapsed = 0.0f;
    float lockOnGuidePulseElapsed = 0.0f;
    bool lockOnGuideVisible = false;
    float lockOnGuideDelay = 3.0f;
    float lockOnGuidePulsePeriod = 1.2f;
    float lockOnGuidePulseMinScale = 0.92f;
    DirectX::XMFLOAT2 lockOnGuideArrowSize{ 96.0f, 64.0f };
    DirectX::XMFLOAT2 lockOnGuideArrowBaseScale{ 0.5f, 0.7f };
    DirectX::XMFLOAT2 lockOnGuideButtonSize{ 96.0f, 96.0f };
    DirectX::XMFLOAT2 lockOnGuideButtonBaseScale{ 1.0f, 1.0f };
    float lockOnGuideButtonOffset = 75.0f;
    float lockOnGuideEdgeMargin = 130.0f;
    float lockOnGuideArrowRotationOffset = 180.0f;
    
private:
    void UpdateLowHpEffects();
    void TriggerLowHpPulse();
    void ResetLowHpEffects();

    int lowHpThreshold = 13;    // 瀕死判定HP
    bool lowHpActive = false;
    bool lowHpPresentationSuppressed = false;
    bool lowHpPresentationCinematicSuppressed = false;
    float heartbeatTimer = 0.0f;
    float heartbeatInterval = 3.0f;
    float lowHpPulseTimer = 0.0f;
    float lowHpPulseDuration = 0.4f;
    float lowHpPulseFlashAmount = 0.0f;
    float lowHpVignetteBaseAlpha = 0.12f;
    float lowHpVignettePulseAlpha = 0.32f;
    std::string heartbeatSEPath = "./Data/Sound/SE/player_heartbeat.wav";
    std::shared_ptr<UIImageComponent> lowHpVignetteImageComponent;

    void StartDamageFlash();
    void UpdateDamageFlash();
    void ApplyDamageFlash(float flashAmount);

    float damageFlashTimer = 0.0f;
    float damageFlashDuration = 0.25f;
    float damageFlashBodyTintStrength = 0.5f;
    float damageFlashRimStrength = 1.0f;
    DirectX::XMFLOAT3 damageFlashColor = { 0.85f, 0.08f, 0.05f };

    // 目を閉じる処理の変数
    float closedEyePoseTime = 4.045f;
    enum class EyeCloseOverrideOwner
    {
        None,
        ManualPreview,
        PrisonOpening,
        Death,
    };

    float closeEyeWeight = 0.0f;
    bool closeEyePreviewActive = false;
    EyeCloseOverrideOwner eyeCloseOverrideOwner = EyeCloseOverrideOwner::None;
    bool deathEyeCloseActive = false;
    float deathEyeCloseDelay = 2.506f;
    float deathEyeCloseDuration = 1.4f;
    float deathColorFadeDelay = 4.018f;
    float deathColorFadeDuration = 2.05f;
    float deathVisualFade = 0.0f;
    bool deathEyeCloseStarted = false;
    bool deathVisualFadeStarted = false;
    DirectX::XMFLOAT3 deathDeadEmissiveColor = { 0.35f, 0.35f, 0.35f };
    float playerAliveEmissionPower = 20.9f;


    DirectX::XMFLOAT3 prevSwordRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevSwordMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevSwordTipPos = { 0.0f,0.0f,0.0f };

    // 剣の残像の調整値
    float swordGhostElapsedTime = 0.0f;
    int swordGhostIndex = 0;
    float ghostInterval = 0.015f; // 残像を出す間隔
    float ghostFadeTime = 0.6f; // 残像が残る時間
    float swordGhostEmissive = 2.0f;    // 残像のemissiveColor
    bool battleActionsSuspended = false;
    DirectX::XMFLOAT3 swordGhostColor = { 0.0f,0.042f,0.253f };  // 残像の剣のベースカラー
    DirectX::XMFLOAT3 ghostInnerColor = { 1.0f,1.0f,1.0f }; // 残像の剣のベースカラー
    DirectX::XMFLOAT3 rushSwordColor = { 0.5f,0.8f,1.6f };
    DirectX::XMFLOAT3 activeGhostBaseColor{};
    DirectX::XMFLOAT3 activeGhostEdgeColor{};
    float ghostEdgeWidth = 1.0f; // 残像の輪郭
    DirectX::XMFLOAT4X4 prevSwordWorld; // 前回の姿勢
    bool isPrevSwordWorldValid = false;

    // playerのジャスト回避の残像の調整値
    float playerPoseGhostInitialAlpha = 0.4f;   // プレイヤーの残像の初回の透明度
    float playerPoseGhostLifetime = 0.81f;      // プレイヤーの残像のライフタイム
    float playerGhostSpawnInterval = 0.25f;
    DirectX::XMFLOAT3 playerPoseGhostColor = { 0.5f, 0.8f, 1.6f };
    float playerPoseGhostEmissive = 2.0f;
    DirectX::XMFLOAT3 playerPoseGhostEdgeColor = { 0.973f, 0.911f, 0.443f };
    DirectX::XMFLOAT3 playerPoseGhostInnerColor = { 1.0f, 1.0f, 1.0f };
    float playerPoseGhostEdgeWidth = 1.0f;
    float playerPoseGhostSpawnTimer = 0.0f;
    size_t nextPlayerPoseGhostIndex = 0;
    bool playerPoseGhostSpawnSequenceActive = false;

    void UpdatePlayerPoseGhost();
    void ApplyDitherTransparencyAlpha(float alpha);
    bool CapturePlayerPoseGhost(size_t ghostIndex);

    float weaponSphereRadius = 0.75f;    // 剣の球の当たり判定の半径

    // 剣の軌跡
    Trail trail;
    float trailRemainTime = 0.8f; // 残像が残る時間

    std::shared_ptr<UIGaugeFillComponent> hpDelayedFillUiComponent;
    std::shared_ptr<UIGaugeFillComponent> hpCurrentFillUiComponent;
    std::vector<std::shared_ptr<UICoreComponent>> hpBarUiComponents;
    struct GameplayHudFadeEntry
    {
        std::shared_ptr<UIImageComponent> component;
        CoreColor color;
    };
    std::vector<GameplayHudFadeEntry> gameplayHudFadeEntries;
    float delayedHp = 0.0f;
    float delayedHpDelayTimer = 0.0f;
    float delayedHpDelayDuration = 0.25f;
    float delayedHpFollowSpeed = 25.0f;
    int minimumHp = 0;
    bool hpRecoveryUiActive = false;
    float hpRecoveryUiStart = 0.0f;
    float hpRecoveryUiElapsed = 0.0f;
    float hpRecoveryUiDuration = 0.5f;
    CoreColor playerHpCurrentColor{ 0.302f, 0.565f, 0.644f, 1.0f };
    CoreColor playerHpDelayedColor{ 0.711f, 0.958f, 0.993f, 1.0f };

    // カメラの目の位置
    std::shared_ptr<SceneComponent> cameraEyeComponent;
    // カメラの注視点の位置
    std::shared_ptr<SceneComponent> cameraTargetComponent;
 
    // ボス戦時のオフセット
    float bossBattleCameraDistance = -3.0f;
    DirectX::XMFLOAT3 bossBattleCameraOffset = { -0.5f,2.0f,0.0f };

    // カメラの
    DirectX::XMFLOAT3 focusDirection = {};//　カメラのFocus開始時のForward
    bool isBossBattle = false;  // ボス戦状態かどうか

    // 回避方向
    DodgeDirection dodgeDirection = DodgeDirection::Backward;
    DirectX::XMFLOAT3 dodgeWorldDirection{};
    bool useDodgeWorldDirection = false;
    HitReactionDirection hitReactionDirection = HitReactionDirection::Front;
    // 歩き走りダッシュのステート管理
    LocomotionMode locomotionMode = LocomotionMode::Idle;


    JustDodgeSlowPhase playerSlowPhase = JustDodgeSlowPhase::Inactive;
    JustDodgeSlowPhase bossSlowPhase = JustDodgeSlowPhase::Inactive;
    float playerSlowHoldTimer = 0.0f;
    float bossSlowHoldTimer = 0.0f;
    float playerSlowReturnElapsed = 0.0f;
    float bossSlowReturnElapsed = 0.0f;

    // ジャスト回避時のスローの調整
    float playerSlowReturnStartScale = 1.0f;
    float bossSlowReturnStartScale = 1.0f;
    float activeBossSlowReturnDuration = 0.10f;
    float justDodgeTimeScale = 0.15f;    // ジャスト回避のタイムスケール
    float justDodgeSlowHoldDuration = 1.3f;     // ジャスト回避の時間
    float justDodgeSlowReturnDuration = 0.10f;
    float rushBossSlowScale = 0.10f;
    float rushBossReturnDuration = 0.10f;

    // プレイヤーの壁に近づいた時の透明度
    float transparencyStartRatio = 0.35f;
    float transparencyMinAlpha = 0.25f;  // 最小透明度
    float transparencyMaxAlpha = 1.f;  // 最大透明度

    // 演出中かどうか
    bool moviePerform = false;
    bool prisonOpeningSitting = false;
    bool prisonOpeningStanding = false;
    void UpdatePrisonOpeningStandingPingPong(float deltaTime);

    bool prisonOpeningUseStandingPingPong = false;
    float prisonOpeningSittingPingPongStartTime = 0.0f;
    float prisonOpeningSittingPingPongEndTime = 0.5f;
    float prisonOpeningSittingPingPongSpeed = 0.6f;
    float prisonOpeningSittingToStandingBlendTime = 0.15f;
    float prisonOpeningSittingPingPongPhase = 0.0f;
    float prisonOpeningSittingSampleTime = 0.0f;
    bool prisonOpeningSittingPingPongForward = true;
    bool prisonOpeningInputWasEnabled = true;
    DirectX::XMFLOAT3 prisonOpeningSittingPosition{};
    float prisonOpeningSittingYawDegrees = 0.0f;
    bool deathCameraTransparencyDisabled = false;
    bool finalHitWaiting = false;
    std::function<void(GruxEnemy*, const DirectX::XMFLOAT3&)> finalHitCallback;
    std::function<void()> deathCameraStartCallback;

    friend class PlayerStateBase;
    friend class PlayerKnockBackState;
    friend class PlayerDeathPendingState;
    friend class PlayerWinState;
};
