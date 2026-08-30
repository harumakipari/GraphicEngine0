#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Components/Effect/ParticleComponent.h"
#include "Core/Actor.h"
#include "Game/Actors/Base/Character.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "UI/Widgets/Widget.h"
#include "Animation/DangerArea.h"
#include "Game/Actors/Enemy/Boss/BossAITypes.h"
#include "Graphics/Renderer/TrailRenderer.h"
#include <array>

class GruxEnemy :public Enemy
{
public:
    explicit GruxEnemy(const std::string& actorName) :Enemy(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void RenderTrail(ID3D11DeviceContext* immediateContext);

    void DrawImGuiDetails() override;

    //当たった時の処理
    void TakeDamage(int damage);

    // ヒットエフェクトを生成する
    void SpawnHitEffect(DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 playerPos) const;

    void SpawnRushHitRing(const DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 playerPos) const;

    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

    void OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event) override;

    void DrawAnimationEditorPreviewState(const AnimationNotifyState& state) override;

    void OnAnimationChanged() override;

    // 攻撃開始時に始める処理
    void StartAttack();

    void DisableAttackHitBoxes();

    // Actionの選択結果を取得する
    BossActionType GetSelectedActionType() const
    {
        return selectedActionType;
    }

    uint64_t GetCurrentAttackSequenceId() const { return currentAttackSequenceId; }

    BossAIMode GetBossAIMode() const { return bossAIMode; }
    BossAttackType GetSelectedAttackType() const { return selectedAttackType; }

    bool TryStartIntent(BossIntentType intentType);
    bool SelectIntentByWeight();
    void ClearActiveIntent();
    void MarkIntentPositioningAttempted();
    void MarkIntentPositioningCompleted();
    void BeginIntentReevaluation();
    void MarkIntentAttackSelected();
    void OnSelectedActionStartedSuccessfully();
    void OnSelectedAttackCompletedSuccessfully();
    void OnSelectedActionStartFailed();
    void FailActiveIntent(const char* reason);
    bool InvalidateCloseCombatIntentForBack(const BossTargetContext& context);
    bool IsCombatRepositionActive() const;
    void CompleteCombatReposition();
    const std::optional<BossIntentType>& GetActiveIntent() const { return activeIntent; }
    bool WasIntentPositioningAttempted() const { return intentPositioningAttempted; }

    const std::optional<BossPositioningData>& GetSelectedPositioningData() const
    {
        return selectedPositioningData;
    }

    std::optional<BossAttackType> GetAttackTypeForAction(BossActionType actionType) const;
    bool PrepareAttackForSelectedAction();
    const BossPositioningData* GetPositioningDataForAction(BossActionType actionType) const;
    void BeginPositioning(const BossPositioningData& data);
    bool GetFixedPositioningTarget(DirectX::XMFLOAT3& outTarget) const
    {
        if (!fixedPositioningTargetValid)
            return false;
        outTarget = fixedPositioningTarget;
        return true;
    }

    float GetPositioningArrivalDistance() const { return positioningArrivalDistance; }
    float GetCombatRepositionSettleDuration() const { return combatRepositionSettleDuration; }
    void SetCombatRepositionSettleDebug(bool active, float remaining)
    {
        combatRepositionSettling = active;
        combatRepositionSettleRemaining = remaining;
        if (active && currentPositioningDebug.valid)
            currentPositioningDebug.endReason = "Settling";
    }
    void UpdatePositioningAnimation(float actualSpeed, float deltaTime);
    void EndPositioningAnimation();
    void UpdatePositioningMovement(const DirectX::XMFLOAT3& moveDirection,
        const DirectX::XMFLOAT3& facingDirection, float deltaTime);
    void UpdatePositioningDebug(float traveledDistance, float elapsedTime, float stuckTimer,
        float frameMovement, float actualSpeed, const DirectX::XMFLOAT3& requestedDirection);
    void FinishPositioningDebug(const std::string& reason);
    void StartSelectedActionCooldown();

    bool SelectAttackForCurrentMode();
    int GetAttackStageCount(BossAttackType type) const;
    bool PlayAttackStage(BossAttackType type, int stage);
    bool PlayAttackAnimationByName(const std::string& animationName);
    void BeginAdditionalAttackStage();
    void ClearJumpAttackMotionWarpOverride();
    bool BeginDashAttackMovement();
    bool UpdateDashAttackMovement(float deltaTime);
    void StopDashAttackMovement();
    bool BeginChargeAttackMovement();
    ChargeAttackEndReason UpdateChargeAttackMovement(float deltaTime);
    void StopChargeAttackMovement();
    float GetChargeWindupEndTime() const { return chargeWindupEndTime; }
    float GetStunDuration() const { return stunDuration; }
    bool IsDead() const { return hp <= 0; }
    const std::string& GetDeathAnimationName() const { return deathAnimationName; }
    void SetStunDebug(const char* phase, float elapsed)
    {
        stunPhaseDebug = phase ? phase : "None";
        stunElapsedDebug = elapsed;
    }
    void SetChargePhaseDebug(const char* phase) { chargePhaseDebug = phase ? phase : "None"; }
    void SetChargeWindupAnimationTimeDebug(float time) { chargeWindupAnimationTimeDebug = time; }
    float GetAttackInterval() const { return attackInterval; }
    float GetDashWindupDuration() const { return dashWindupDuration; }
    float GetJumpAttackTelegraphStartTime() const { return jumpAttackTelegraphStartTime; }
    float GetJumpAttackTelegraphEndTime() const { return jumpAttackTelegraphEndTime; }
    float GetRecoveryDurationForCurrentAttack() const;
    int GetDamageForCurrentAttack() const;
    bool IsTransitionWindowActive() const { return transitionWindow; }
    int GetCurrentAttackHitCount() const { return currentAttackHitCount; }
    bool WasCurrentAttackSequenceJustDodged() const { return !justDodgedActors.empty(); }
    float GetActiveHitBoxElapsedForDebug() const;
    std::string GetCurrentAttackNameForDebug() const;
    BossTargetContext BuildTargetContext() const;
    bool ShouldWaitForActiveIntentCooldown(const BossTargetContext& context) const;
    bool ShouldFailIntentForPositioningRetryLimit(const BossTargetContext& context) const;
    bool IsFacingPlayerForAttack(const BossTargetContext& context) const;
    bool PreparePendingAttackFacing(const BossTargetContext& context);
    bool HasPendingAttackFacing() const { return pendingAttackFacingValid; }
    const DirectX::XMFLOAT3& GetPendingAttackFacingDirection() const
    {
        return pendingAttackFacingDirection;
    }
    float GetPendingAttackFacingAngle() const;
    float GetPendingAttackFacingCompleteAngle() const
    {
        return pendingAttackFacingCompleteAngle;
    }
    bool ResumeSelectedAttackAfterTurn();
    bool IsCloseCombatAttackType(BossAttackType attackType) const;
    void SetAttackReadyReason(AttackReadyReason reason) { attackReadyReason = reason; }
    AttackReadyReason GetAttackReadyReason() const { return attackReadyReason; }
    float GetAttackReadyDuration() const
    {
        return attackReadyReason == AttackReadyReason::AfterTurn
            ? sideAttackReadyDuration
            : frontAttackReadyDuration;
    }
    void BeginAttackReadyDebug();
    void UpdateAttackReadyDebug(float elapsedTime);
    void EndAttackReadyDebug();
    bool PlayAttackReadySE();


    void ClearPendingAttackFacing();
    void StopAIMovement();
    bool RotateTowardsPlayer(const DirectX::XMFLOAT3& direction,
        float degreesPerSecond, float deltaTime, const char* debugSource);
    void BeginTurnRotationDebug(const char* fromState);
    void EndTurnRotationDebug(float duration);
    float GetTurnCompleteAngle() const { return turnCompleteAngle; }
    float GetTurnTimeout() const { return turnTimeout; }
    float GetTurnSpeed() const { return turnSpeed; }
    void SetLastAIDecision(const std::string& reason) { lastAIDecisionReason = reason; }

    // カメラの注視点の位置
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }

    // ボスの名前の演出を開始する
    void StartGruxNamePerform(float duration, float start = 0.0f, float end = 1.0f);


    // 抽選結果をmemberへ保存する関数
    bool SelectCombatAction();


private:
    // プレイヤーとの距離を取得する関数
    float GetDistanceToPlayer();

    // プレイヤーとの距離に応じた距離領域を取得する関数
    BossDistanceRegion GetDistanceRegion(float distance) const;

    // 現在の距離領域に基づいて、候補となる行動を選択する関数
    void UpdateActionCandidateFlags(const BossTargetContext& context);
    bool IsActionForCurrentIntent(BossActionType actionType, const BossTargetContext& context) const;
    const BossIntentData* GetActiveIntentData() const;
    BossIntentRangeStatus GetIntentRangeStatus(
        const BossIntentData& intentData, float distance) const;
    bool GetIntentAttackValidRange(float& outMinDistance, float& outMaxDistance) const;
    bool IsAttackPendingGoalAction(BossActionType actionType, const BossTargetContext& context) const;
    struct RepositionTargetEvaluation
    {
        DirectX::XMFLOAT3 desiredTarget{};
        DirectX::XMFLOAT3 clampedTarget{};
        float availableDistance = 0.0f;
        bool wasClamped = false;
        float clampDistance = 0.0f;
        bool sufficientlyMovable = false;
    };
    void EvaluateRepositionTargets(const BossTargetContext& context);
    void EvaluateClampedPositioningTarget(const DirectX::XMFLOAT3& startPosition,
        const DirectX::XMFLOAT3& desiredTarget, RepositionTargetEvaluation& outEvaluation) const;
    bool IsRepositionAction(BossActionType actionType) const;
    const RepositionTargetEvaluation& GetRepositionTargetEvaluation(BossActionType actionType) const;
    const char* GetRepositionFailureReason() const;

    // アクションが現在の距離(Region)で候補になるかを判定する関数
    bool IsActionCandidateForCurrentDistance(const BossActionData& actionData, BossDistanceRegion currentRegion) const;

    // Action候補とBase WeightからEffective Weightを更新する
    void UpdateActionEffectiveWeights();
    void UpdateActionCooldowns(float deltaTime);
    void UpdateIntentEffectiveWeights(const BossTargetContext& context);
    float GetIntentWeightForDistance(const BossIntentData& data, BossDistanceRegion region) const;
    float GetTotalIntentWeight() const;

    // ActionのEffective Weight合計を求める関数
    float GetTotalActionWeight() const;

    // Weightに基づいて行動を選択する関数。候補がない場合はstd::nulloptを返す
    std::optional<BossActionType> SelectActionByWeight();

    void ResetJustDodgeRecords(const char* reason);
    bool HasJustDodgedAttack(const Actor* actor) const;
    void ResetDangerArea();
    void CaptureInitialDangerObbSettings();
    AnimationNotifyState* GetSelectedDangerNotifyState();
    const AnimationNotifyState* GetSelectedDangerNotifyState() const;
    bool GetPlayerDangerOverlapForDebug(const DangerArea& area) const;
    void RefreshActiveDangerAreaFromNotify();
    void DrawDangerObbWorldDebug();
    void PrepareJumpAttackMotionWarpOverride();


    void RefreshActiveHitBoxesFromNotifyStates();
    // ボスの距離範囲のデバック描画
    void DrawBossAIDebugWorld(const BossTargetContext& context) const;
    void DrawPositioningDebugWorld() const;
    void BeginRotationDebugFrame();
    void FinishRotationDebugFrame();
    void RecordRotationDebugSource(const char* source,
        const DirectX::XMFLOAT3& targetDirection, float requestedTurnSpeed);
    void DrawRotationDebugWorld(const BossTargetContext& context) const;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    // 回転コンポーネントを追加
    std::shared_ptr<RotationComponent> rotationComponent;
    // キャラクタームーブコンポーネントを追加
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;

    // 左の武器の当たり判定のコンポーネント
    std::shared_ptr<CapsuleComponent> leftWeaponCollisionComp;
    // 右の武器の当たり判定のコンポーネント
    std::shared_ptr<CapsuleComponent> rightWeaponCollisionComp;
    std::string leftWeapon = "leftWeapon";
    std::string rightWeapon = "rightWeapon";
    std::string bothWeapon = "bothWeapon";

    std::shared_ptr<SceneComponent> weaponLeftRootComponent; // 左の武器の根元のコンポーネント
    std::shared_ptr<SceneComponent> weaponLeftMiddleComponent; // 左の武器の中間のコンポーネント
    std::shared_ptr<SceneComponent> weaponLeftTipComponent;  // 左の武器の先端のコンポーネント

    std::shared_ptr<SceneComponent> weaponRightRootComponent; // 右の武器の根元のコンポーネント
    std::shared_ptr<SceneComponent> weaponRightMiddleComponent; // 右の武器の中間のコンポーネント
    std::shared_ptr<SceneComponent> weaponRightTipComponent;  // 右の武器の先端のコンポーネント

    Trail leftWeaponTrail;
    Trail rightWeaponTrail;
    bool showLeftWeaponTrail = false;
    bool showRightWeaponTrail = false;

    // 軌跡の色
    DirectX::XMFLOAT3 bossTrailColor{ 1.0f, 1.0f, 1.0f };
    float bossTrailEmissiveStrength = 7.0f;
    float bossTrailLifetime = 0.8f;

    std::shared_ptr<ParticleComponent> hitSwordEffectComponent; // Existing normal hit effect
    std::shared_ptr<ParticleComponent> rushHitRingEffectComponent; // Rush World Ring effect
    std::shared_ptr<ParticleComponent> rushHitSparkEffectComponent; // Rush Spark effect

    std::shared_ptr<UIGaugeComponent> hpFrameUiComponent;   // HPバー

    bool rightHitBox = false;   // 右の剣の当たり判定
    bool leftHitBox = false;    // 左の剣の当たり判定
    bool isDangerWindow = false;

    // ヒット中に当たった敵を記録する
    std::unordered_set<Actor*> hitActors;
    // Players that successfully just-dodged the current attack sequence.
    std::unordered_set<const Actor*> justDodgedActors;
    uint64_t currentAttackSequenceId = 0;
    int currentAttackHitCount = 0;

    BossAIMode bossAIMode = BossAIMode::CombatAI;
    //BossAIMode bossAIMode = BossAIMode::DebugFixedAttack;
    BossAttackType debugFixedAttackType = BossAttackType::PrimaryAttackLA;

    std::optional<BossIntentType> activeIntent = std::nullopt;
    BossIntentStep activeIntentStep = BossIntentStep::Selecting;
    bool intentPositioningAttempted = false;
    bool intentPositioningCompleted = false;
    int intentPositioningAttemptCount = 0;
    static constexpr int maxIntentPositioningAttempts = 2;
    float dashAttackValidMinDistance = 6.0f;
    float dashAttackValidMaxDistance = 14.0f;
    std::string intentRepositionReason = "None";
    std::string intentLifecycleState = "None";
    std::string intentLifecycleTrace = "None";
    std::string intentLifecycleReason = "None";

    static constexpr int intentCount = 4;
    std::array<BossIntentData, intentCount> combatIntentData =
    { {
        { BossIntentType::CloseCombat, 60.0f, 30.0f, 25.0f, 4.0f, 5.5f, 0.25f },
        { BossIntentType::DashAttackPlan, 25.0f, 35.0f, 45.0f, 8.0f, 10.0f, 0.25f },
        { BossIntentType::JumpAttackPlan, 15.0f, 35.0f, 30.0f, 7.0f, 9.0f, 0.25f },
        { BossIntentType::CombatReposition, 10.0f, 12.0f, 10.0f, 0.0f, 100.0f, 0.0f },
    } };
    std::array<float, intentCount> combatIntentEffectiveWeights{};
    bool hasLastIntentRandomRoll = false;
    float lastIntentRandomRoll = 0.0f;
    float lastIntentRandomTotalWeight = 0.0f;
    std::array<float, intentCount> lastIntentRandomRangeBegin{};
    std::array<float, intentCount> lastIntentRandomRangeEnd{};
    std::array<float, intentCount> lastIntentRandomWeights{};
    std::optional<BossIntentType> lastSelectedIntent = std::nullopt;

    BossActionType selectedActionType = BossActionType::AttackLA;
    BossActionType lastActionType = BossActionType::AttackLA;
    BossAttackType selectedAttackType = BossAttackType::PrimaryAttackLA;
    std::optional<BossPositioningData> selectedPositioningData = std::nullopt;
    bool pendingAttackActionValid = false;
    bool pendingAttackFacingValid = false;
    DirectX::XMFLOAT3 pendingAttackFacingDirection{ 0.0f, 0.0f, 1.0f };
    float pendingAttackFacingCompleteAngle = 15.0f;

    BossAttackType lastAttackType = BossAttackType::PrimaryAttackLA;
    bool hasLastAttack = false;

    // 行動のデータを定義する配列。攻撃の種類、距離条件などを設定する。
    static constexpr int actionCount = 10;
    std::array<BossActionData, actionCount> combatActionData =
    {
        {
        { BossActionType::AttackLA, BossAttackType::PrimaryAttackLA ,BossDistanceRegion::Near,BossDistanceRegion::Near,30.0f,1.0f},
        { BossActionType::AttackRA, BossAttackType::PrimaryAttackRA,BossDistanceRegion::Near,BossDistanceRegion::Near,30.0f,1.0f},
        { BossActionType::FastCombo, BossAttackType::FastCombo ,BossDistanceRegion::Near,BossDistanceRegion::Near,40.0f,2.0f},
        { BossActionType::JumpAttack,BossAttackType::JumpAttack ,BossDistanceRegion::Middle,BossDistanceRegion::Middle,30.0f,3.0f},
        { BossActionType::DashAttack,BossAttackType::DashAttack,BossDistanceRegion::Middle,BossDistanceRegion::Far,40.0f,4.0f},
        { BossActionType::ChargeAttack,BossAttackType::ChargeAttack,BossDistanceRegion::Middle,BossDistanceRegion::Far,40.0f,4.0f},
        { BossActionType::Approach, std::nullopt,BossDistanceRegion::Near,BossDistanceRegion::Far,40.0f,0.5f},
        { BossActionType::Retreat, std::nullopt,BossDistanceRegion::Near,BossDistanceRegion::Far,40.0f,2.5f},
        { BossActionType::RepositionLeft, std::nullopt,BossDistanceRegion::Near,BossDistanceRegion::Far,50.0f,1.0f},
        { BossActionType::RepositionRight, std::nullopt,BossDistanceRegion::Near,BossDistanceRegion::Far,50.0f,1.0f},
    }
    };

    // 各行動が距離条件を満たしているかのフラグ　
    std::array<bool, actionCount> combatActionCandidateFlags{};
    std::array<BossActionCandidateReason, actionCount> combatActionCandidateReasons{};
    // 各行動の有効な重み。距離条件とRepeat条件を考慮した有効な重み
    std::array<float, actionCount> combatActionEffectiveWeights{};
    std::array<float, actionCount> combatActionCooldownRemaining{};

    bool showBossAIDebug = false;
    struct RotationSourceDebug
    {
        std::string source = "None";
        float targetYaw = 0.0f;
        float requestedTurnSpeed = 0.0f;
    };
    struct LastTurnDebug
    {
        bool valid = false;
        std::string fromState = "None";
        float startAngle = 0.0f;
        float startTargetYaw = 0.0f;
        float endAngle = 0.0f;
        float duration = 0.0f;
    };
    bool showRotationDebug = false;
    bool rotationDebugYawInitialized = false;
    float rotationDebugPreviousYaw = 0.0f;
    float rotationDebugCurrentYaw = 0.0f;
    float rotationDebugPlayerYaw = 0.0f;
    float rotationDebugActualYawDelta = 0.0f;
    float rotationDebugRequestedTurnSpeed = 0.0f;
    std::array<RotationSourceDebug, 8> rotationDebugSources{};
    int rotationDebugSourceCount = 0;
    DirectX::XMFLOAT3 rotationDebugTurnTargetDirection{};
    bool rotationDebugTurnTargetValid = false;
    LastTurnDebug lastTurnDebug{};
    float activeTurnDebugStartAngle = 0.0f;
    float activeTurnDebugTargetYaw = 0.0f;
    std::string activeTurnDebugFromState = "None";
    BossTargetContext aiDebugTargetContext{};
    bool attackFacingEvaluationValid = false;
    float attackFacingEvaluationTolerance = 0.0f;
    float attackFacingEvaluationAngle = 0.0f;
    bool attackFacingEvaluationRequired = false;
    bool lastResumedAttackValid = false;
    BossAttackType lastResumedAttackType = BossAttackType::PrimaryAttackLA;
    bool attackReadyActive = false;
    float attackReadyDebugTimer = 0.0f;
    BossAttackType attackReadyDebugType = BossAttackType::PrimaryAttackLA;
    bool attackReadySEFired = false;
    AttackReadyReason attackReadyReason = AttackReadyReason::Front;


    bool hasSelectedActionDebug = false;
    bool hasLastActionRandomRoll = false;
    float lastActionRandomRoll = 0.0f;
    float lastActionRandomTotalWeight = 0.0f;
    std::array<float, actionCount> lastActionRandomRangeBegin{};
    std::array<float, actionCount> lastActionRandomRangeEnd{};
    std::array<float, actionCount> lastActionRandomWeights{};

    // 攻撃ごとのデータを定義する配列。アニメーション名、距離条件、重みなどを設定する。
    std::array<BossAttackData, 6> combatAttackData =
    { {
        { BossAttackType::PrimaryAttackLA, "PrimaryAttack_LA", 0.0f, 5.0f, 1.0f, 1.25f, 5 },
        { BossAttackType::PrimaryAttackRA, "PrimaryAttack_RA", 0.0f, 5.0f, 1.0f, 1.30f, 5 },
        { BossAttackType::FastCombo, "FastCombo", 0.0f, 6.0f, 1.0f, 2.0f, 5 },
        { BossAttackType::JumpAttack, "PrimaryAttack_JumpAttack", 4.5f, 12.0f, 1.0f, 2.80f, 5 },
        { BossAttackType::DashAttack, "Stampede_0 > Stampede_Knockup_0", 6.0f, 100.0f, 1.0f, 1.20f, 7 },
        { BossAttackType::ChargeAttack, "Pre_FootSlide_0 > Stampede_0", 6.0f, 100.0f, 1.0f, 0.1f, 7 },
    } };

    // 既存のAttack選択用
    std::array<float, 5> combatEffectiveWeights{};  // 距離条件とRepeat条件を考慮した有効な重み
    std::array<bool, 5> combatCandidateFlags{}; // 各攻撃が距離条件を満たしているかのフラグ　主にImGuiで使用。
    float currentCombatPlayerDistance = 0.0f;   // Attack選択時点のプレイヤーとの距離。距離条件の判定に直接使用。
    float lastCombatSelectionDistance = 0.0f;   //  最後にAttack抽選を行ったときの距離。現在はImGui表示用として保持。
    float repeatWeightScale = 0.25f;    //  直前と同じAttackのWeightへ掛ける倍率。現在は0.25なので、同じ攻撃のWeightを25%まで下げる。

    std::array<BossPositioningData, 4> combatPositioningData =
    { {
        { BossActionType::Approach, BossPositioningDirection::TowardPlayer, 20.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TargetDistance, 0.0f },
        { BossActionType::Retreat, BossPositioningDirection::AwayFromPlayer, 7.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TravelDistance, 0.0f },
        { BossActionType::RepositionLeft, BossPositioningDirection::TowardPlayer, 3.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TravelDistance, 0.0f },
        { BossActionType::RepositionRight, BossPositioningDirection::TowardPlayer, 3.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TravelDistance, 0.0f },
    } };

    //  StateMachineのタイミングと方向制御
    float attackInterval = 0.1f;   // EnemyThinkStateへ入ってからAttack選択を開始するまでの待ち時間。
    float recoveryDuration = 0.5f;  //  攻撃終了後、EnemyRecoveryStateに滞在する時間。現在は全Attack共通の0.5秒。
    float attackFacingAngle = 35.0f;    // この角度以内なら攻撃可能とみなす

    float nearDistanceThreshold = 6.0f; // この距離以下は近距離とみなす
    float middleDistanceThreshold = 12.0f; // この距離以下は中距離とみなす

    float relativeFrontMaxAngle = 50.0f;
    float relativeBackMinAngle = 110.0f;
    const std::array<BossActionData, actionCount> initialCombatActionData = combatActionData;
    const std::array<BossIntentData, intentCount> initialCombatIntentData = combatIntentData;
    const std::array<BossAttackData, 6> initialCombatAttackData = combatAttackData;
    const float initialNearDistanceThreshold = nearDistanceThreshold;
    const float initialMiddleDistanceThreshold = middleDistanceThreshold;
    const float initialRelativeFrontMaxAngle = relativeFrontMaxAngle;
    const float initialRelativeBackMinAngle = relativeBackMinAngle;
    static constexpr float initialCombatRepositionMoveDistance = 10.0f;  // 横に練り歩く距離
    static constexpr float initialCombatRepositionMoveSpeed = 6.0f;
    static constexpr float initialCombatRepositionSettleDuration = 1.5f;    // 目的地に付いた後の待つ秒数

    static constexpr float initialFrontAttackReadyDuration = 1.0f;
    static constexpr float initialSideAttackReadyDuration = 1.5f;
    static constexpr float initialCloseCombatReadyFacingAngle = 7.5f;

    float turnSpeed = 480.0f;  // EnemyTurnStateでその場回転するときの速度
    float turnCompleteAngle = 15.0f;    // この角度以内なら回転完了とみなす
    float turnTimeout = 1.5f;   //   Turnがいつまでも完了しない場合の制限時間。現在は1.5秒でThinkへ戻る。
    std::string lastAIDecisionReason = "None";  // 最後にAIが行った判断理由を文字列で保存。ImGuiに表示。AIの動作確認用。
    float frontAttackReadyDuration = initialFrontAttackReadyDuration;
    float sideAttackReadyDuration = initialSideAttackReadyDuration;
    float closeCombatReadyFacingAngle = initialCloseCombatReadyFacingAngle;
    float currentFacingAngleBeforeReady = 0.0f;
    std::string attackReadySEName = "enemy_attack_ready1";
    float attackReadySEVolume = 1.0f;
    struct PositioningDebugSnapshot
    {
        bool valid = false;
        BossPositioningData data{};
        std::optional<BossIntentType> intent = std::nullopt;
        float preferredMin = 0.0f;
        float preferredMax = 0.0f;
        DirectX::XMFLOAT3 startBossPosition{};
        DirectX::XMFLOAT3 startPlayerPosition{};
        DirectX::XMFLOAT3 desiredTargetPosition{};
        DirectX::XMFLOAT3 clampedTargetPosition{};
        bool targetWasClamped = false;
        float targetClampDistance = 0.0f;
        float availableMoveDistance = 0.0f;
        float safetyMargin = 0.0f;
        float safeMinX = 0.0f;
        float safeMaxX = 0.0f;
        float safeMinZ = 0.0f;
        float safeMaxZ = 0.0f;
        DirectX::XMFLOAT3 currentPosition{};
        DirectX::XMFLOAT3 requestedMoveDirection{};
        float startPlayerDistance = 0.0f;
        float currentPlayerDistance = 0.0f;
        float plannedMoveDistance = 0.0f;
        float actualMoveDistance = 0.0f;
        float traveledPathDistance = 0.0f;
        float remainingDistance = 0.0f;
        float frameMovement = 0.0f;
        float actualSpeed = 0.0f;
        float elapsedTime = 0.0f;
        float stuckTimer = 0.0f;
        float inputMagnitude = 0.0f;
        std::string endReason = "None";
    };
    static constexpr float bossRoomMinX = -2.5f;
    static constexpr float bossRoomMaxX = 19.5f;
    static constexpr float bossRoomMinZ = 0.0f;
    static constexpr float bossRoomMaxZ = 21.5f;
    float bossRoomSafetyMargin = 0.5f;
    float positioningArrivalDistance = 0.3f;
    float minimumPositioningMoveDistance = 0.75f;
    float positioningMoveStartSpeedThreshold = 0.30f;
    float positioningMoveStopSpeedThreshold = 0.10f;
    float positioningMoveStopDelay = 0.15f;
    float positioningMoveStopTimer = 0.0f;
    float positioningAnimationActualSpeed = 0.0f;
    bool positioningAnimationMoving = false;
    DirectX::XMFLOAT3 fixedPositioningTarget{};
    bool fixedPositioningTargetValid = false;
    float combatRepositionBackWeight = 80.0f;
    float dashAttackPlanBackWeight = 35.0f;
    float jumpAttackPlanBackWeight = 35.0f;
    float combatRepositionMoveDistance = initialCombatRepositionMoveDistance;   // repositionの時に動く距離
    float combatRepositionMoveSpeed = initialCombatRepositionMoveSpeed;
    float combatRepositionSettleDuration = initialCombatRepositionSettleDuration;
    bool combatRepositionSettling = false;
    float combatRepositionSettleRemaining = 0.0f;
    const float initialCombatRepositionBackWeight = combatRepositionBackWeight;
    const float initialDashAttackPlanBackWeight = dashAttackPlanBackWeight;
    const float initialJumpAttackPlanBackWeight = jumpAttackPlanBackWeight;
    bool suppressCombatRepositionForNextIntentSelection = false;
    float postAttackCombatRepositionWeight = 45.0f;
    const float initialPostAttackCombatRepositionWeight =
        postAttackCombatRepositionWeight;
    bool postAttackCombatRepositionBoostPending = false;
    bool postAttackCombatRepositionBoostApplied = false;
    BossRepositionReason repositionReason = BossRepositionReason::Normal;
    BossRepositionDirection selectedRepositionDirection = BossRepositionDirection::None;
    RepositionTargetEvaluation leftRepositionTarget{};
    RepositionTargetEvaluation rightRepositionTarget{};
    bool repositionTargetsEvaluated = false;
    std::string repositionCompletionReason = "None";
    DirectX::XMFLOAT3 repositionDesiredTarget{};
    DirectX::XMFLOAT3 repositionClampedTarget{};
    float repositionLeftAvailableDistance = 0.0f;
    float repositionRightAvailableDistance = 0.0f;
    bool repositionSelectedTargetWasClamped = false;
    bool repositionNoSafeDirection = false;

    bool positioningDebugActive = true;
    bool positioningWorldDebug = true;
    PositioningDebugSnapshot currentPositioningDebug{};
    PositioningDebugSnapshot lastPositioningDebug{};
    BossPositioningData activePositioningDebugData{};
    float positioningDebugTraveledDistance = 0.0f;
    float positioningDebugElapsedTime = 0.0f;
    float positioningDebugStuckTimer = 0.0f;
    std::string positioningEndReason = "None";

    // FastComboの連続攻撃用
    bool transitionWindow = false;  //   Animation NotifyのTransitionWindowが現在有効かを表す。FastComboで次のコンボ段階へ進めるタイミングの判定に使用。

    // JumpAttackのMotionWarp用
    float maxJumpDistance = 12.5f;//  JumpAttackで実際に移動してよい最大距離。プレイヤーが遠くても12.5より長くは移動しない。
    float desiredAttackDistance = 0.1f; //  JumpAttack後にプレイヤーとの間へ残したい距離
    float jumpAttackTelegraphStartTime = 1.4f;  // 予備動作の開始アニメーション時間
    float jumpAttackTelegraphEndTime = 2.6f;
    float currentJumpPlayerDistance = 0.0f; //  JumpAttack開始時点のプレイヤーまでの距離
    float calculatedJumpDistance = 0.0f;    //  最終的にMotionWarpで移動する距離
    bool jumpMotionWarpOverrideActive = false;  // 通常のAnimation Notifyに設定された移動距離ではなく、JumpAttack用に計算した距離と方向を使用するかどうか
    DirectX::XMFLOAT3 jumpAttackStartPlayerPosition{};  //  JumpAttack開始時のプレイヤー位置
    DirectX::XMFLOAT3 jumpMotionWarpDirection{ 0.0f, 0.0f, 1.0f };  //  ボスからJumpAttack開始時のプレイヤー位置へ向かう正規化済み方向

    // DashAttack
    float dashWindupDuration = 1.40f;   // 予備動作の時間
    float dashAttackSpeed = 12.0f;
    float minDashAttackDistance = 4.0f;
    float maxDashAttackDistance = 16.0f;
    float desiredDashAttackDistance = 2.0f;
    float dashArrivalDistance = 0.35f;
    float dashAttackTimeout = 1.10f;
    float currentDashAttackPlayerDistance = 0.0f;
    float calculatedDashAttackDistance = 0.0f;
    float dashAttackElapsedTime = 0.0f;
    bool dashAttackMovementActive = false;
    DirectX::XMFLOAT3 dashAttackDirection{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 dashAttackStartPosition{};
    DirectX::XMFLOAT3 dashTargetPosition{};

    // ChargeAttack
    float chargeWindupEndTime = 2.90f;
    float chargeSpeed = 12.0f;
    float chargeSafetyTimeout = 8.0f;
    float chargeWallCastSafetyMargin = 0.10f;
    float chargeWallFacingThreshold = 0.70f;
    float chargeWallNormalYThreshold = 0.60f;
    float chargeWallCastRadiusScale = 0.80f;
    float chargeElapsedTime = 0.0f;
    bool chargeMovementActive = false;
    DirectX::XMFLOAT3 chargeDirection{ 0.0f, 0.0f, 1.0f };
    std::string chargePhaseDebug = "None";
    float chargeWindupAnimationTimeDebug = 0.0f;
    bool chargePlayerCastHitDebug = false;
    float chargePlayerHitDistanceDebug = 0.0f;
    std::string chargePlayerHitActorDebug = "None";
    std::string chargeSelectedHitDebug = "None";
    bool chargeWallCastHitDebug = false;
    float chargeWallFacingAmountDebug = 0.0f;
    DirectX::XMFLOAT3 chargeWallHitNormalDebug{};
    float chargeWallHitDistanceDebug = 0.0f;
    ChargeAttackEndReason chargeEndReasonDebug = ChargeAttackEndReason::None;

    // Wall Hit後の行動不能時間。Start/End Animation時間とは分離する。
    float stunDuration = 2.5f;
    std::string stunPhaseDebug = "None";
    float stunElapsedDebug = 0.0f;
    std::string deathAnimationName = "Death_A_0";

    bool isDeathPerform = false;
    float pitchBaseValue = 0.45f;

    // 左目の位置用コンポーネントを追加　暗闇で光る目の表現用
    std::shared_ptr<SceneComponent> leftEyeSceneComponent;
    // 右目の位置用コンポーネントを追加　暗闇で光る目の表現用
    std::shared_ptr<SceneComponent> rightEyeSceneComponent;

    // カメラの注視点の位置 
    std::shared_ptr<SceneComponent> cameraTargetComponent;
    // ボス戦時のオフセット
    float bossBattleCameraDistance = 0.0f;
    //float bossBattleCameraRightDistance = 2.5f;
    float bossBattleCameraRightDistance = 0.0f;
    DirectX::XMFLOAT3 bossBattleCameraOffset = { 0.0f,0.0f,0.0f };

    // 前フレームの左の武器
    DirectX::XMFLOAT3 prevWeaponLeftRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponLeftMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponLeftTipPos = { 0.0f,0.0f,0.0f };

    // 前フレームの右の武器
    DirectX::XMFLOAT3 prevWeaponRightRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponRightMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponRightTipPos = { 0.0f,0.0f,0.0f };

    float hitWeaponRadius = 0.8f;
    float activeLeftHitBoxRadius = 0.8f;
    float activeRightHitBoxRadius = 0.8f;
    std::vector<const AnimationNotifyState*> activeHitBoxNotifyStates;   // 武器の半径
    float enemyScale = 1.7f;    // 敵のスケール
    float hitEnemyEffectOffsetY = 2.2f;  // ヒットエフェクトのオフセットY
    float hitPlayerEffectOffsetY = 2.4f;  // ヒットエフェクトのオフセットY

    // 登場シーンのボス名前のUI
    std::shared_ptr<UIImageComponent> gruxNameImageComponent;
    std::unique_ptr<EasingRunner> easingRunner;
    float easingFactorAlpha = 0.0f;

    // ロックオンのイメージモデル
    std::shared_ptr<SkeletalMeshComponent> lockOnTargetMeshComponent;
    float lockOnOffset = 0.0f;  // プレイヤー側に押し出すオフセット
    float lockOnOffsetY = 1.65f;
    // ロックオンのイメージUI
    std::shared_ptr<UIImageComponent> lockOnTargetImageComponent;

    // ジャスト回避の矩形の範囲
    DirectX::XMFLOAT3 justDodgeAreaSize = { 0.0f,2.0f,0.0f };
    // ジャスト回避の矩形のオフセット
    DirectX::XMFLOAT3 justDodgeAreaOffset = { 0.0f,0.0f,0.0f };
    DangerArea dangerArea{};
    struct DangerObbInitialValue
    {
        DirectX::XMFLOAT3 centerOffset{};
        DirectX::XMFLOAT3 fullSize{};
    };
    std::unordered_map<uint64_t, DangerObbInitialValue> initialDangerObbSettings;
    std::unordered_map<uint64_t, DangerObbInitialValue> savedDangerObbSettings;
    std::string dangerObbSaveStatus = "Not saved this session";
    std::string dangerObbSavePath;
    bool dangerObbLastSaveSucceeded = false;
    size_t dangerObbSelectedClip = static_cast<size_t>(-1);
    size_t dangerObbSelectedStateIndex = 0;
    const AnimationNotifyState* activeDangerNotifyState = nullptr;
    bool dangerObbWorldDebug = false;

    float flashDuration = 0.8f;   // 何秒でフラッシュしなくなるか

    // アニメーション時にどれくらい移動するか
    std::vector<AnimationMotionWarp> animationMotionWarps;

    friend class GruxEnemyEyeActor;
};


class KnightActor : public Character
{
public:
    explicit KnightActor(const std::string& actorName) :Character(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<RotationComponent> rotationComponent;

};

class SavarogEnemy :public Character
{
public:
    explicit SavarogEnemy(const std::string& actorName) :Character(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<RotationComponent> rotationComponent;

};

class GracialEnemy :public Character
{
public:
    explicit GracialEnemy(const std::string& actorName) :Character(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<RotationComponent> rotationComponent;

};

