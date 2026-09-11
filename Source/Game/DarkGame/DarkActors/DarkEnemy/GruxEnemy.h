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

#include "Engine/Audio/Audio.h"
#include "Game/DarkGame/BehaviorTree/BehaviorData.h"

class Player;
class BehaviorTree;
class NodeBase;

class GruxEnemy :public Enemy
{
public:
    explicit GruxEnemy(const std::string& actorName) :Enemy(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void RenderTrail(ID3D11DeviceContext* immediateContext);

    void DrawImGuiDetails() override;

    //�����������̏���
    void TakeDamage(int damage);

    // Battle HUD visibility is decided by GameScene; Grux only owns its components.
    void SetHpBarVisible(bool visible);
    void BeginHpBarFadeOut();
    void SetHpBarFadeAlpha(float alpha);

    // Stops combat immediately while preserving HP and the current death animation.
    void StopBattleActions();
    void BeginFinalHitReaction(const std::string& animationName);
    void EndFinalHitReaction();

    // Suspends battle decisions while allowing the Actor and its animations to update.
    void PauseBattleAI();
    void SetDirectionImmediate(const DirectX::XMFLOAT3& direction);
    void ResumeBattleAI();
    bool IsBattleAIActive() const { return battleAIActive; }

    // Clears Grux-owned transient combat state while preserving HP.
    void ResetForBattleContinue(const Transform& battleStartTransform);
    void ResetForBattleRestart(const Transform& battleStartTransform);

    void BeginRushHpDisplay();
    void EndRushHpDisplay();

    // �q�b�g�G�t�F�N�g�𐶐�����
    void SpawnHitEffect(DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 playerPos) const;

    void SpawnRushHitRing(const DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 playerPos) const;

    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

    void OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event) override;

    void DrawAnimationEditorPreviewState(const AnimationNotifyState& state) override;

    void OnAnimationChanged() override;

    // �U���J�n���Ɏn�߂鏈��
    void StartAttack();

    void DisableAttackHitBoxes();

    // Action�̑I����ʂ�擾����
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
    void RequestJumpAttackCameraAssist();
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
    bool CanPlanDashAttack() const;
    bool PrepareDashAttackSetupTarget();
    bool CanExecuteDashAttack() const;
    bool StartDashAttackTelegraph();
    enum class DashBTPhase { None, Setup, Facing, Telegraph, Movement, Knockup, Recovery };
    enum class DashBTResult { Running, Complete, Failed };
    DashBTResult UpdateDashAttackBT(float deltaTime);
    void CleanupDashAttackBT();
    void FinishDashAttackBT();
    bool IsDashAttackBTActive() const { return dashBTPhase != DashBTPhase::None; }
    void SetDashBTFacing() { dashBTPhase = DashBTPhase::Facing; }
    bool BeginDashAttackMovement();
    bool UpdateDashAttackMovement(float deltaTime, bool keepLockedDirection = false);
    void StopDashAttackMovement();
    enum class ChargeBTPhase { None, Setup, Facing, Telegraph, Charging, Result, Stun, RecoveryPending, Recovery, RecoveryPostconditions };
    enum class ChargeBTStunPhase { None, Start, Loop, End };
    enum class ChargeBTStepResult { Running, Complete, Failed };
    bool CanPlanChargeAttack() const;
    bool PrepareChargeAttackSetupTarget();
    ChargeBTStepResult UpdateChargeAttackFacingBT(float deltaTime);
    bool CanExecuteChargeAttack() const;
    void StartChargeAttackBT();
    bool BeginSingleChargeBT();
    ChargeBTStepResult UpdateSingleChargeBT(float deltaTime);
    ChargeBTStepResult ResolveChargeResultBT(float deltaTime);
    bool BeginChargeStunBT();
    ChargeBTStepResult UpdateChargeStunBT(float deltaTime);
    void FinishChargeAttackBT();
    void FailChargeAttackBT();
    bool IsChargeAttackBTActive() const { return chargeBT.phase != ChargeBTPhase::None; }
    ChargeBTPhase GetChargeBTPhase() const { return chargeBT.phase; }
    ChargeAttackEndReason GetChargeBTResult() const { return chargeBT.result; }
    void BeginChargeRecoveryBT() { chargeBT.phase = ChargeBTPhase::Recovery; }
    void MarkChargeRecoveryTimerFinishedBT() { chargeBT.phase = ChargeBTPhase::RecoveryPostconditions; }
    ChargeBTStepResult FinishChargeRecoveryBT();
    bool ShouldAbortChargeAttackBT() const;
    void CleanupChargeAttackBT();
    void DrawChargeAttackBTDebug();
    bool BeginChargeAttackMovement();
    ChargeAttackEndReason UpdateChargeAttackMovement(float deltaTime);
    void StopChargeAttackMovement();
    float GetChargeWindupEndTime() const { return chargeWindupEndTime; }
    float GetStunDuration() const { return stunDuration; }
    bool IsDead() const { return hp <= 0; }
    void SetCinematicDeathAnimationOwnedExternally(const bool owned)
    {
        cinematicDeathAnimationOwnedExternally = owned;
    }
    bool IsCinematicDeathAnimationOwnedExternally() const
    {
        return cinematicDeathAnimationOwnedExternally;
    }
    bool ConsumeBeginHuskParticleRequest()
    {
        const bool requested = beginHuskParticleRequest;
        beginHuskParticleRequest = false;
        return requested;
    }
    void RequestBeginHuskParticle()
    {
        beginHuskParticleRequest = true;
        CoreAudio::PlayOneShot("./Data/Sound/SE/boss_dead_dissolve.wav");
    }
    const std::shared_ptr<SkeletalMeshComponent>& GetSkeletalMeshComponent() const
    {
        return skeletalMeshComponent;
    }
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
    float GetJumpAttackDesiredStartDistance() const { return jumpDesiredStartDistance; }
    float GetJumpSetupDistanceMin() const { return jumpSetupDistanceMin; }
    float GetJumpSetupDistanceMax() const { return jumpSetupDistanceMax; }
    float GetRecoveryDurationForCurrentAttack() const;
    void SetNextRecoveryDuration(float duration, const char* source);
    float ConsumeNextRecoveryDuration();
    void SetPendingChargeRecoveryResult(ChargeAttackEndReason reason);
    ChargeAttackEndReason GetPendingChargeRecoveryResult() const
    {
        return pendingChargeRecoveryResult;
    }
    ChargeAttackEndReason ConsumePendingChargeRecoveryResult();
    void RequestCombatRepositionIntent();
    void UpdateRecoveryDebug(float elapsed, float duration)
    {
        recoveryElapsedDebug = elapsed;
        currentRecoveryDurationDebug = duration;
    }
    float GetChargePlayerHitRecoveryDuration() const
    {
        return chargePlayerHitRecoveryDuration;
    }
    float GetChargeJustDodgeRecoveryDuration() const
    {
        return chargeJustDodgeRecoveryDuration;
    }
    float GetPostStunRecoveryDuration() const
    {
        return postStunRecoveryDuration;
    }
    int GetDamageForCurrentAttack() const;
    bool IsTransitionWindowActive() const { return transitionWindow; }
    int GetCurrentAttackHitCount() const { return currentAttackHitCount; }
    bool WasCurrentAttackSequenceJustDodged() const { return !justDodgedActors.empty(); }
    float GetActiveHitBoxElapsedForDebug() const;
    std::string GetCurrentAttackNameForDebug() const;
    enum class RoarStage { None, Telegraph, Shockwave };
    bool AreAttackBehaviorsDisabledForDebug() const { return disableAttackBehaviorsForDebug; }
    bool CanPlanRoar() const;
    bool CanPlanRetreat() const;
    bool EvaluateRoarPlanForDebug() const;
    std::string GetRoarRejectReasonForDebug() const;
    bool IsRoarExecutionAllowed() const;
    bool IsRoarBTActive() const { return roarBT.stage != RoarStage::None; }
    bool BeginRoarBT();
    int UpdateRoarBT(float dt);
    void ApplyRoarShockwave();
    void CleanupRoarBT(const char* status);
    void TickRoarLifecycle(float dt);
    void DrawRoarBTDebug();
    float GetRoarRenderFootOffset() const;
    BossTargetContext BuildTargetContext() const;
    void RefreshFastComboTargetContext(int stage);
    struct PositioningTargetContext { bool valid=false; DirectX::XMFLOAT3 targetPosition{}; float arrivalTolerance=0.3f; float timeout=3.0f; float maxMoveDistance=20.0f; float moveSpeed=6.0f; float stuckMovementThreshold=0.1f; float stuckTimeThreshold=0.5f; };
    struct PositioningTargetRuntime { DirectX::XMFLOAT3 previousPosition{}; float elapsed=0.0f; float traveledDistance=0.0f; float remainingDistance=0.0f; float stuckTime=0.0f; bool movementActive=false; };
    enum class PositioningMoveResult { None, Running, Arrived, Timeout, Stuck, InvalidTarget, MaxDistanceReached };
    using AttackSetupTargetContext = PositioningTargetContext;
    using AttackSetupMoveResult = PositioningMoveResult;
    bool CanPlanJumpAttack() const;
    bool PrepareJumpAttackSetupTarget();
    bool FindAttackSetupTarget(float minDistance, float maxDistance, float angleStep, float clampTolerance, float minimumMoveDistance, DirectX::XMFLOAT3& outTarget, float& outDistance, int& outCandidateCount) const;
    bool HasAttackSetupTarget() const { return attackSetupTarget.valid; }
    const AttackSetupTargetContext& GetAttackSetupTarget() const { return attackSetupTarget; }
    void ClearAttackSetupTarget();
    void BeginAttackSetupMovement();
    AttackSetupMoveResult UpdateAttackSetupMovement(float deltaTime);
    void StopAttackSetupMovement();
    bool PrepareRetreatTarget();
    PositioningMoveResult UpdateRetreatMovement(float deltaTime);
    void FinishRetreatMovement(bool arrived);
    void RecordRetreatMoveResult(PositioningMoveResult result);
    float GetAttackSetupRemainingDistance() const { return attackSetupRemainingDistance; }
    float GetAttackSetupElapsedTime() const { return attackSetupElapsedTime; }
    float GetAttackSetupPlannedMoveDistance() const { return attackSetupPlannedMoveDistance; }
    float GetJumpSetupMinimumMoveDistance() const { return jumpSetupMinimumMoveDistance; }
    bool StartJumpAttackTelegraph();
    bool UpdateJumpAttackTelegraph(float deltaTime);
    bool StartJumpAttackExecution();
    const BossTargetContext& GetFastComboTargetContext() const { return fastComboTargetContext; }
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

    // �J�����̒����_�̈ʒu
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }

    // �{�X�̖��O�̉��o��J�n����
    void StartGruxNamePerform(float duration, float start = 0.0f, float end = 1.0f);

    // ���I���ʂ�member�֕ۑ�����֐�
    bool SelectCombatAction();
    // FastCombo�̎��s�ہE�������
    // FastCombo��U�����Ƃ��đI��ł��邩
    bool CanPlanFastCombo() const;
    void StartFastComboApproachRetryCooldown();
    bool IsFastComboApproachRetryCooldownActive() const;
    // ����FastCombo����s�ł����Ԃ�
    bool CanExecuteFastCombo() const;
    // Player��FastCombo�̍U���͈͓�ɂ��邩
    bool IsFastComboInRange() const;
    // Player���U���\�Ȑ��ʊp�x��ɂ��邩
    bool IsPlayerInFastComboFacingRange(const BossTargetContext& context) const;
    bool IsPlayerInFastComboFaceCompleteRange(const BossTargetContext& context) const;

    // FastCombo�J�n�O�̐ڋߏ���
    // FastCombo�p�̐ڋߏ�����J�n
    void BeginFastComboApproach();
    // �ڋߏ�����X�V���A������������Ԃ�
    bool UpdateFastComboApproach(float deltaTime);

    // BehaviorTree������s����U���̐ݒ�
    void SetSelectedAttackForBehaviorTree(BossAttackType type)
    {
        selectedAttackType = type;
    }

    // BehaviorTree�Ŏ��s�����U���̏I������
    enum class BehaviorAttackResult
    {
        None,        // ���ʖ��m��
        Success,     // �U������I��
        JustDodged   // Player�̃W���X�g���ɂ�蒆�f
    };

    void SetBehaviorAttackResult(BehaviorAttackResult result)
    {
        behaviorAttackResult = result;
    }

    // BehaviorTree�p�̍U���I����Recovery�̏�����J�n
    void BeginRecovery() const;

    // BehaviorTree�p�̍U���I����Recovery���Ԃ�擾
    float GetSelectedAttackRecoveryDuration() const { return GetRecoveryDurationForCurrentAttack(); }
    // BehaviorTree�p�̍U���������Ԃ�擾
    float GetBehaviorPrepareDuration() const { return fastComboPrepareDuration; }
    float GetFastComboApproachMaxDuration() const { return fastComboApproachMaxDuration; }
    float GetFastComboApproachRetryCooldown() const { return fastComboApproachRetryCooldown; }
    // BehaviorTree�p�̑ҋ@���Ԃ�擾
    float GetBehaviorIdleDuration() const { return behaviorIdleDuration; }

    // BehaviorTree�̍X�V
    void UpdateBehaviorTree(float deltaTime);

    // FastCombo��BehaviorTree����ɂ��邩
    void SetBehaviorTreeFastComboEnabled(bool enabled)
    {
        behaviorTreeFastComboEnabled = enabled;
    }

    bool IsBehaviorTreeFastComboEnabled() const
    {
        return behaviorTreeFastComboEnabled;
    }

    struct CloseCombatSettings { float minRange=0.0f; float executeMaxRange=6.0f; float planMaxRange=10.0f; float facingLimitDegrees=35.0f; float faceCompleteAngleDegrees=7.0f; };
    const CloseCombatSettings& GetCloseCombatSettings() const { return closeCombatSettings; }
    float GetInterStageFaceCompleteAngle() const { return interStageFaceCompleteAngle; }
    float GetInterStageMaxFacingAngle() const { return interStageMaxFacingAngle; }
    float GetInterStageFaceDelay() const { return interStageFaceDelay; }
    enum class FastComboRuntimeState { Attack, InterStageDelay, InterStageFacing };
    void SetFastComboRuntimeState(FastComboRuntimeState state) { fastComboRuntimeState = state; }
    FastComboRuntimeState GetFastComboRuntimeState() const { return fastComboRuntimeState; }
    int GetFastComboRuntimeStage() const { return fastComboRuntimeStage; }
    void SetFastComboRuntimeStage(int stage) { fastComboRuntimeStage = stage; }
    const std::string& GetBehaviorTreeCurrentNode() const { return behaviorTreeCurrentNode; }
    const std::string& GetBehaviorTreePreviousNode() const { return behaviorTreePreviousNode; }
    const std::string& GetBehaviorTreeLastResult() const { return behaviorTreeLastResult; }
    const std::string& GetBehaviorTreeLastJudgment() const { return behaviorTreeLastJudgment; }
    void SetBehaviorTreeLastJudgment(const std::string& value) { behaviorTreeLastJudgment = value; }
private:
    bool finalHitReactionActive = false;
    bool finalHitReactionHeld = false;
    // �v���C���[�Ƃ̋�����擾����֐�
    float GetDistanceToPlayer();

    // �v���C���[�Ƃ̋����ɉ����������̈��擾����֐�
    BossDistanceRegion GetDistanceRegion(float distance) const;

    // ���݂̋����̈�Ɋ�Â��āA���ƂȂ�s����I�����֐�
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

    // �A�N�V���������݂̋���(Region)�Ō��ɂȂ邩�𔻒肷��֐�
    bool IsActionCandidateForCurrentDistance(const BossActionData& actionData, BossDistanceRegion currentRegion) const;

    // Action����Base Weight����Effective Weight��X�V����
    void UpdateActionEffectiveWeights();
    bool IsCombatAttackAction(BossActionType actionType) const;
    bool IsAttackActionForIntent(BossActionType actionType, BossIntentType intentType) const;
    float GetRecentAttackPenalty(BossActionType actionType) const;
    float GetIntentRecentAttackPenalty(BossIntentType intentType) const;
    void UpdateActionCooldowns(float deltaTime);
    void UpdateIntentEffectiveWeights(const BossTargetContext& context);
    float GetIntentWeightForDistance(const BossIntentData& data, BossDistanceRegion region) const;
    float GetTotalIntentWeight() const;

    // Action��Effective Weight���v����߂�֐�
    float GetTotalActionWeight() const;

    // Weight�Ɋ�Â��čs����I�����֐��B��₪�Ȃ��ꍇ��std::nullopt��Ԃ�
    std::optional<BossActionType> SelectActionByWeight();

    void ResetJustDodgeRecords(const char* reason);
    bool HasJustDodgedAttack(const Actor* actor) const;
    bool TryStartJustDodgeSuccess(Player* player);
    void ResetDangerArea();
    void CaptureInitialDangerObbSettings();
    AnimationNotifyState* GetSelectedDangerNotifyState();
    const AnimationNotifyState* GetSelectedDangerNotifyState() const;
    bool GetPlayerDangerOverlapForDebug(const DangerArea& area) const;
    void RefreshActiveDangerAreaFromNotify();
    void DrawDangerObbWorldDebug();
    void PrepareJumpAttackMotionWarpOverride();

    void RefreshActiveHitBoxesFromNotifyStates();
    // �{�X�̋����͈͂̃f�o�b�N�`��
    void DrawBossAIDebugWorld(const BossTargetContext& context) const;
    void DrawPositioningDebugWorld() const;
    void DrawRetreatDebugWorld() const;
    PositioningMoveResult UpdatePositioningTargetMovement(PositioningTargetContext& target,
        PositioningTargetRuntime& runtime, float deltaTime, const char* debugSource);
    void ClearPositioningTarget(PositioningTargetContext& target, PositioningTargetRuntime& runtime);
    void BeginRotationDebugFrame();
    void FinishRotationDebugFrame();
    void RecordRotationDebugSource(const char* source, const DirectX::XMFLOAT3& targetDirection, float requestedTurnSpeed);
    void DrawRotationDebugWorld(const BossTargetContext& context) const;

    // �W�����v�U���̌�ɒ��n�̎��̃G�t�F�N�g�𐶐�����
    void SpawnGroundImpactEffect() const;

    //�ǂɓ����������̃G�t�F�N�g�𐶐�����
    void SpawnWallImpactEffect(const DirectX::XMFLOAT3& impactPosition, const DirectX::XMFLOAT3& wallNormal) const;

    // ���퓯�m�̉ΉԂ̃G�t�F�N�g�𐶐�����
    void SpawnWeaponClashEffect() const;

    // ������n�ʂɎC�鎞�̃G�t�F�N�g�𐶐�����
    void SpawnLeftFootScrapeEffect()const;

    // �E����n�ʂɎC�鎞�̃G�t�F�N�g�𐶐�����
    void SpawnRightFootScrapeEffect()const;

    // �n�ʂɓ|�ꂽ�Ƃ��̃G�t�F�N�g�𐶐�����
    void SpawnGroundDownEffect()const;

private:
    // �`��p�R���|�[�l���g��ǉ�
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    // ��]�R���|�[�l���g��ǉ�
    std::shared_ptr<RotationComponent> rotationComponent;
    // �L�����N�^�[���[�u�R���|�[�l���g��ǉ�
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;

    // ���̕���̓����蔻��̃R���|�[�l���g
    std::shared_ptr<CapsuleComponent> enemyCapsuleComponent;
    std::shared_ptr<CapsuleComponent> leftWeaponCollisionComp;
    // �E�̕���̓����蔻��̃R���|�[�l���g
    std::shared_ptr<CapsuleComponent> rightWeaponCollisionComp;
    std::string leftWeapon = "leftWeapon";
    std::string rightWeapon = "rightWeapon";
    std::string bothWeapon = "bothWeapon";

    std::shared_ptr<SceneComponent> weaponLeftRootComponent; // ���̕���̍����̃R���|�[�l���g
    std::shared_ptr<SceneComponent> weaponLeftMiddleComponent; // ���̕���̒��Ԃ̃R���|�[�l���g
    std::shared_ptr<SceneComponent> weaponLeftTipComponent;  // ���̕���̐�[�̃R���|�[�l���g

    std::shared_ptr<SceneComponent> weaponRightRootComponent; // �E�̕���̍����̃R���|�[�l���g
    std::shared_ptr<SceneComponent> weaponRightMiddleComponent; // �E�̕���̒��Ԃ̃R���|�[�l���g
    std::shared_ptr<SceneComponent> weaponRightTipComponent;  // �E�̕���̐�[�̃R���|�[�l���g

    std::shared_ptr<SceneComponent> beltComponent;  // �x���g�̃R���|�[�l���g

    std::shared_ptr<SceneComponent> leftFootComponent;      // �����̃R���|�[�l���g
    std::shared_ptr<SceneComponent> rightFootComponent;     // �E���̃R���|�[�l���g

    Trail leftWeaponTrail;
    Trail rightWeaponTrail;
    bool showLeftWeaponTrail = false;
    bool showRightWeaponTrail = false;

    // �O�Ղ̐F
    DirectX::XMFLOAT3 bossTrailColor{ 0.0f, 0.13f, 0.002f };
    float bossTrailEmissiveStrength = 7.0f;
    float bossTrailLifetime = 0.8f;

    std::shared_ptr<ParticleComponent> hitSwordEffectComponent; // Existing normal hit effect
    std::shared_ptr<ParticleComponent> rushHitRingEffectComponent; // Rush World Ring effect
    std::shared_ptr<ParticleComponent> rushHitSparkEffectComponent; // Rush Spark effect
    std::shared_ptr<ParticleComponent> groundDustEffectComponent;
    std::shared_ptr<ParticleComponent> wallImpactDustEffectComponent;
    std::shared_ptr<ParticleComponent> wallImpactFlashEffectComponent;
    std::shared_ptr<ParticleComponent> metalSparkEffectComponent;
    std::shared_ptr<ParticleComponent> footScrapeEffectComponent;   // �����̃G�t�F�N�g

    std::shared_ptr<UIGaugeFillComponent> hpDelayedFillUiComponent;
    std::shared_ptr<UIGaugeFillComponent> hpCurrentFillUiComponent;   // HP�o�[
    std::vector<std::shared_ptr<UICoreComponent>> hpBarUiComponents;
    struct HpBarFadeEntry
    {
        std::shared_ptr<UIImageComponent> component;
        CoreColor color;
    };
    std::vector<HpBarFadeEntry> hpBarFadeEntries;
    bool rushHpDisplayActive = false;
    bool useRushDelayedHpFollowSpeed = false;
    float delayedHp = 0.0f;
    float delayedHpDelayTimer = 0.0f;
    float delayedHpDelayDuration = 0.25f;
    float delayedHpFollowSpeed = 8.95f; // HP�o�[���ǂꂭ�炢�x�����邩
    float delayedHpRushFollowSpeed = 12.0f; // HP�o�[���ǂꂭ�炢Rush���ɒx�����邩
    CoreColor bossHpCurrentColor{ 0.55f, 0.08f, 0.06f, 1.0f };
    CoreColor bossHpDelayedColor{ 0.95f, 0.72f, 0.38f, 1.0f };

    bool rightHitBox = false;   // �E�̌��̓����蔻��
    bool leftHitBox = false;    // ���̌��̓����蔻��
    bool isDangerWindow = false;

    // �q�b�g���ɓ��������G��L�^����
    std::unordered_set<Actor*> hitActors;
    // Players that successfully just-dodged the current attack sequence.
    std::unordered_set<const Actor*> justDodgedActors;
    uint64_t currentAttackSequenceId = 0;
    int currentAttackHitCount = 0;

    bool battleAIActive = true; // �{�X�o�g��AI��g�p���邩�ǂ����@false�Ȃ�idle�̂܂�
    //BossAIMode bossAIMode = BossAIMode::CombatAI;         // �{�X��AI���[�h
    BossAIMode bossAIMode = BossAIMode::DebugFixedAttack;   // �{�X��AI���[�h�@�f�o�b�N�p
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
    // �ӎv����̋������Ƃɂ��d��
    std::array<BossIntentData, intentCount> combatIntentData =
    { {
        { BossIntentType::CloseCombat, 60.0f, 30.0f, 25.0f, 4.0f, 5.5f, 0.25f },
        { BossIntentType::DashAttackPlan, 25.0f, 35.0f, 45.0f, 8.0f, 10.0f, 0.25f },
        { BossIntentType::JumpAttackPlan, 15.0f, 35.0f, 30.0f, 7.0f, 9.0f, 0.25f },
        { BossIntentType::CombatReposition, 3.0f, 12.0f, 10.0f, 0.0f, 100.0f, 0.0f },
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
    std::optional<BossActionType> lastStartedCombatAttack = std::nullopt;
    std::optional<BossActionType> secondLastStartedCombatAttack = std::nullopt;
    static constexpr float initialRecentAttackPenaltyLast = 0.2f;          // ���߂̍U���̉����銄��
    static constexpr float initialRecentAttackPenaltySecond = 0.45f;        // �Q��̍U���̉����銄��
    float recentAttackPenaltyLast = initialRecentAttackPenaltyLast;
    float recentAttackPenaltySecond = initialRecentAttackPenaltySecond;
    BossAttackType selectedAttackType = BossAttackType::PrimaryAttackLA;
    std::optional<BossPositioningData> selectedPositioningData = std::nullopt;
    bool pendingAttackActionValid = false;
    bool pendingAttackFacingValid = false;
    DirectX::XMFLOAT3 pendingAttackFacingDirection{ 0.0f, 0.0f, 1.0f };
    float pendingAttackFacingCompleteAngle = 15.0f;

    BossAttackType lastAttackType = BossAttackType::PrimaryAttackLA;
    bool hasLastAttack = false;

    // �s���̃f�[�^���`����z��B�U���̎�ށA��������Ȃǂ�ݒ肷��B
    static constexpr int actionCount = 10;
    std::array<BossActionData, actionCount> combatActionData =
    {
        {
        { BossActionType::AttackLA, BossAttackType::PrimaryAttackLA ,BossDistanceRegion::Near,BossDistanceRegion::Near,20.0f,1.0f},
        { BossActionType::AttackRA, BossAttackType::PrimaryAttackRA,BossDistanceRegion::Near,BossDistanceRegion::Near,20.0f,1.0f},
        { BossActionType::FastCombo, BossAttackType::FastCombo ,BossDistanceRegion::Near,BossDistanceRegion::Near,100.0f,2.0f},
        { BossActionType::JumpAttack,BossAttackType::JumpAttack ,BossDistanceRegion::Middle,BossDistanceRegion::Middle,30.0f,3.0f},
        { BossActionType::DashAttack,BossAttackType::DashAttack,BossDistanceRegion::Middle,BossDistanceRegion::Far,40.0f,4.0f},
        { BossActionType::ChargeAttack,BossAttackType::ChargeAttack,BossDistanceRegion::Middle,BossDistanceRegion::Far,40.0f,4.0f},
        { BossActionType::Approach, std::nullopt,BossDistanceRegion::Near,BossDistanceRegion::Far,40.0f,0.5f},
        { BossActionType::Retreat, std::nullopt,BossDistanceRegion::Near,BossDistanceRegion::Far,40.0f,2.5f},
        { BossActionType::RepositionLeft, std::nullopt,BossDistanceRegion::Near,BossDistanceRegion::Far,50.0f,1.0f},
        { BossActionType::RepositionRight, std::nullopt,BossDistanceRegion::Near,BossDistanceRegion::Far,50.0f,1.0f},
    }
    };

    // �e�s������������𖞂����Ă��邩�̃t���O
    std::array<bool, actionCount> combatActionCandidateFlags{};
    std::array<BossActionCandidateReason, actionCount> combatActionCandidateReasons{};
    // �e�s���̗L���ȏd�݁B���������Repeat�����l�������L���ȏd��
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
    bool disableAttackBehaviorsForDebug = false;
    float defensiveTooCloseDistance = 5.2f;
    float roarRadius = 5.0f;
    float roarHeightTolerance = 2.0f;
    float roarLevelStartFootOffset = -0.30f;
    float roarLevelStartFootOffsetEndTime = 0.30f;
    float roarPreStampedeStartTime = 3.4f;  // ��K�J�n����
    float roarPreStampedeEndTime = 7.1f;    // ��K�I������
    float roarCooldownDuration = 30.0f;
    float roarCooldownRemaining = 0.0f;
    float retreatDistanceMin = 5.0f;    // 退避距離 最小
    float retreatDistanceMax = 12.0f;// 退避距離　最大
    float retreatMinimumMoveDistance = 5.0f;
    float retreatMoveSpeed = 6.0f;
    float retreatRetryCooldownDuration = 0.5f;
    float retreatRetryCooldownRemaining = 0.0f;
    enum class RetreatCandidateRejectReason { None, Clamp, DistanceIncrease, MinimumMoveDistance, CapsuleCast, Other };
    struct RetreatCandidateDebug { DirectX::XMFLOAT3 position{}; RetreatCandidateRejectReason rejectReason=RetreatCandidateRejectReason::None; bool pathBlocked=false; bool accepted=false; };
    struct RetreatRuntime
    {
        DirectX::XMFLOAT3 playerSnapshot{};
        DirectX::XMFLOAT3 gruxSnapshot{};
        float startingPlayerDistance=0.0f;
        float plannedPlayerDistance=0.0f;
        float plannedMoveDistance=0.0f;
        int candidateCount=0;
        int clampRejectCount=0;
        int distanceIncreaseRejectCount=0;
        int minimumMoveRejectCount=0;
        int capsuleCastRejectCount=0;
        int otherRejectCount=0;
        bool lastCapsuleCastHit=false;
        bool lastCapsuleCastWorldStatic=false;
        bool lastCapsuleCastWorldProps=false;
        bool lastCapsuleCastConvex=false;
        DirectX::XMFLOAT3 lastCapsuleHitPosition{};
        float lastCapsuleHitDistance=0.0f;
        DirectX::XMFLOAT3 capsulePoint1{};
        DirectX::XMFLOAT3 capsulePoint2{};
        float capsuleRadius=0.0f;
        float capsuleSegmentLength=0.0f;
        DirectX::XMFLOAT3 collisionComponentPosition{};
        DirectX::XMFLOAT3 collisionComponentScale{};
        float collisionConfiguredRadius=0.0f;
        float collisionConfiguredHeight=0.0f;
        float collisionOffsetY=0.0f;
        float collisionPhysXHalfHeight=0.0f;
        DirectX::XMFLOAT3 collisionPhysXPoint1{};
        DirectX::XMFLOAT3 collisionPhysXPoint2{};
        float collisionPhysXMinY=0.0f;
        float collisionPhysXMaxY=0.0f;
        float retreatCastMinY=0.0f;
        float retreatCastMaxY=0.0f;
        bool retreatSweepUsesSphere=false;
        bool retreatSweepInitialOverlap=false;
        bool retreatSweepHasNormal=false;
        DirectX::XMFLOAT3 retreatSweepHitNormal{};
        float retreatSweepPenetrationDepth=0.0f;
        std::string retreatSweepHitActor="None";
        std::string retreatSweepHitComponent="None";
        int retreatSweepIgnoredSupportContactCount=0;
        bool retreatSweepPathBlocked=false;
        DirectX::XMFLOAT3 retreatSweepBlockingNormal{};
        float retreatSweepBlockingDistance=0.0f;
        std::string retreatSweepBlockingActor="None";
        std::string retreatSweepBlockingComponent="None";
        std::string failureReason="None";
        std::vector<RetreatCandidateDebug> candidates;
    } retreatRuntime;
    PositioningTargetContext retreatTarget{};
    PositioningTargetRuntime retreatMovementRuntime{};
    PositioningTargetRuntime attackSetupRuntime{};
    bool retreatWorldDebug = true;
    DirectX::XMFLOAT3 retreatDebugTargetPosition{};
    float retreatRemainingDistance = 0.0f;
    PositioningMoveResult retreatLastMoveResult = PositioningMoveResult::None;
    std::string retreatCompleteReason = "None";

    struct RoarRuntime
    {
        RoarStage stage = RoarStage::None;
        bool shockwaveFired = false;
        bool hitPlayer = false;
        float elapsed = 0.0f;
        float stalled = 0.0f;
        float previousTime = 0.0f;
        float endTime = 0.0f;
    } roarBT;
    std::string roarBTStatus = "Idle";
    mutable bool roarPlanDebugEvaluated = false;
    mutable bool roarPlanDebugLastResult = false;
    mutable std::string roarPlanDebugLastRejectReason;
    mutable std::string roarPlanDebugLastNode;
    BossTargetContext fastComboTargetContext{};
    std::array<BossTargetContext, 3> fastComboStageTargetContexts{};
    int fastComboTargetStage = -1;
    float interStageFaceCompleteAngle = 10.0f;
    float interStageMaxFacingAngle = 70.0f;
    float interStageFaceDelay = 0.25f;
    FastComboRuntimeState fastComboRuntimeState = FastComboRuntimeState::Attack;
    int fastComboRuntimeStage = -1;
    AttackSetupTargetContext attackSetupTarget{};
    DirectX::XMFLOAT3 attackSetupPreviousPosition{};
    float attackSetupElapsedTime=0.0f;
    float attackSetupTraveledDistance=0.0f;
    float attackSetupRemainingDistance=0.0f;
    float attackSetupStuckTime=0.0f;
    bool attackSetupMovementActive=false;
    float attackSetupChosenDistance=0.0f;
    float attackSetupPlannedMoveDistance=0.0f;
    int attackSetupCandidateCount=0;
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

    // �U�����Ƃ̃f�[�^���`����z��B�A�j���[�V�������A��������A�d�݁A�U���ʂȂǂ�ݒ肷��B
    std::array<BossAttackData, 6> combatAttackData =
    { {
        { BossAttackType::PrimaryAttackLA, "PrimaryAttack_LA", 0.0f, 5.0f, 1.0f, 1.25f, 5 },
        { BossAttackType::PrimaryAttackRA, "PrimaryAttack_RA", 0.0f, 5.0f, 1.0f, 1.30f, 5 },
        { BossAttackType::FastCombo, "FastCombo", 0.0f, 6.0f, 1.0f, 2.0f, 5 },
        { BossAttackType::JumpAttack, "PrimaryAttack_JumpAttack", 4.5f, 12.0f, 1.0f, 2.80f, 5 },
        { BossAttackType::DashAttack, "Stampede_0 > Stampede_Knockup_0", 6.0f, 100.0f, 1.0f, 1.20f, 7 },
        { BossAttackType::ChargeAttack, "Pre_FootSlide_0 > Stampede_0", 6.0f, 100.0f, 1.0f, 0.1f, 13 },
    } };

    // ������Attack�I��p
    std::array<float, 5> combatEffectiveWeights{};  // ���������Repeat�����l�������L���ȏd��
    std::array<bool, 5> combatCandidateFlags{}; // �e�U������������𖞂����Ă��邩�̃t���O�@���ImGui�Ŏg�p�B
    float currentCombatPlayerDistance = 0.0f;   // Attack�I����_�̃v���C���[�Ƃ̋����B��������̔���ɒ��ڎg�p�B
    float lastCombatSelectionDistance = 0.0f;   //  �Ō��Attack���I��s�����Ƃ��̋����B���݂�ImGui�\���p�Ƃ��ĕێ��B
    float repeatWeightScale = 0.25f;    //  ���O�Ɠ���Attack��Weight�֊|����{���B���݂�0.25�Ȃ̂ŁA�����U����Weight��25%�܂ŉ�����B

    std::array<BossPositioningData, 4> combatPositioningData =
    { {
        { BossActionType::Approach, BossPositioningDirection::TowardPlayer, 20.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TargetDistance, 0.0f },
        { BossActionType::Retreat, BossPositioningDirection::AwayFromPlayer, 7.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TravelDistance, 0.0f },
        { BossActionType::RepositionLeft, BossPositioningDirection::TowardPlayer, 3.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TravelDistance, 0.0f },
        { BossActionType::RepositionRight, BossPositioningDirection::TowardPlayer, 3.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TravelDistance, 0.0f },
    } };

    //  StateMachine�̃^�C�~���O�ƕ�������
    float attackInterval = 0.1f;   // EnemyThinkState�֓����Ă���Attack�I���J�n����܂ł̑҂����ԁB
    float recoveryDuration = 0.5f;  //  �U���I����AEnemyRecoveryState�ɑ؍݂��鎞�ԁB���݂͑SAttack���ʂ�0.5�b�B
    float attackFacingAngle = 35.0f;    // ���̊p�x�ȓ�Ȃ�U���\�Ƃ݂Ȃ�

    float nearDistanceThreshold = 6.0f; // ���̋����ȉ��͋ߋ����Ƃ݂Ȃ�
    float middleDistanceThreshold = 12.0f; // ���̋����ȉ��͒������Ƃ݂Ȃ�

    float relativeFrontMaxAngle = 50.0f;
    float relativeBackMinAngle = 110.0f;
    const std::array<BossActionData, actionCount> initialCombatActionData = combatActionData;
    const std::array<BossIntentData, intentCount> initialCombatIntentData = combatIntentData;
    const std::array<BossAttackData, 6> initialCombatAttackData = combatAttackData;
    const float initialNearDistanceThreshold = nearDistanceThreshold;
    const float initialMiddleDistanceThreshold = middleDistanceThreshold;
    const float initialRelativeFrontMaxAngle = relativeFrontMaxAngle;
    const float initialRelativeBackMinAngle = relativeBackMinAngle;
    static constexpr float initialCombatRepositionMoveDistance = 10.0f;  // ���ɗ���������
    static constexpr float initialCombatRepositionMoveSpeed = 6.0f;
    static constexpr float initialCombatRepositionSettleDuration = 1.5f;    // �ړI�n�ɕt������̑҂b��

    static constexpr float initialFrontAttackReadyDuration = 1.0f;
    static constexpr float initialSideAttackReadyDuration = 1.5f;
    static constexpr float initialCloseCombatReadyFacingAngle = 7.5f;

    float turnSpeed = 480.0f;  // EnemyTurnState�ł��̏��]����Ƃ��̑��x
    float turnCompleteAngle = 15.0f;    // ���̊p�x�ȓ�Ȃ��]�����Ƃ݂Ȃ�
    float turnTimeout = 1.5f;   //   Turn�����܂ł�������Ȃ��ꍇ�̐������ԁB���݂�1.5�b��Think�֖߂�B
    std::string lastAIDecisionReason = "None";  // �Ō��AI���s�������f���R�𕶎���ŕۑ��BImGui�ɕ\���BAI�̓���m�F�p�B
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
    float combatRepositionMoveDistance = initialCombatRepositionMoveDistance;   // reposition�̎��ɓ�������
    float combatRepositionMoveSpeed = initialCombatRepositionMoveSpeed;
    float combatRepositionSettleDuration = initialCombatRepositionSettleDuration;
    bool combatRepositionSettling = false;
    float combatRepositionSettleRemaining = 0.0f;
    const float initialCombatRepositionBackWeight = combatRepositionBackWeight;
    const float initialDashAttackPlanBackWeight = dashAttackPlanBackWeight;
    const float initialJumpAttackPlanBackWeight = jumpAttackPlanBackWeight;
    bool suppressCombatRepositionForNextIntentSelection = false;
    bool combatRepositionIntentPending = false;
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

    // FastCombo�̘A���U���p
    bool transitionWindow = false;      // Animation Notify��TransitionWindow�����ݗL������\���BFastCombo�Ŏ��̃R���{�i�K�֐i�߂�^�C�~���O�̔���Ɏg�p�B

    // JumpAttack��MotionWarp�p
    float maxJumpDistance = 12.5f;      // JumpAttack�Ŏ��ۂɈړ����Ă悢�ő勗���B�v���C���[�������Ă�12.5��蒷���͈ړ����Ȃ��B
    float desiredAttackDistance = 0.1f;
    float jumpDesiredStartDistance = 4.5f;
    float jumpSetupDistanceMin = 6.5f;
    float jumpSetupDistanceMax = 8.5f;
    float jumpSetupMinimumMoveDistance = 5.0f;  // JumpAttack�ōŒ���ړ�����
    float attackSetupCandidateAngleStep = 30.0f;
    float attackSetupClampTolerance = 0.75f;    //  JumpAttack��Ƀv���C���[�Ƃ̊Ԃ֎c����������
    float jumpAttackTelegraphStartTime = 1.4f;  // �\������̊J�n�A�j���[�V��������
    float jumpAttackTelegraphEndTime = 2.6f;
    float currentJumpPlayerDistance = 0.0f; //  JumpAttack�J�n���_�̃v���C���[�܂ł̋���
    float calculatedJumpDistance = 0.0f;    //  �ŏI�I��MotionWarp�ňړ����鋗��
    bool jumpAttackExecutionStartCalledDebug = false;
    bool jumpMotionWarpOverrideActive = false;  // �ʏ��Animation Notify�ɐݒ肳�ꂽ�ړ������ł͂Ȃ��AJumpAttack�p�Ɍv�Z���������ƕ�����g�p���邩�ǂ���
    DirectX::XMFLOAT3 jumpAttackStartPlayerPosition{};  //  JumpAttack�J�n���̃v���C���[�ʒu
    DirectX::XMFLOAT3 jumpMotionWarpDirection{ 0.0f, 0.0f, 1.0f };  //  �{�X����JumpAttack�J�n���̃v���C���[�ʒu�֌��������K���ςݕ���

    // DashAttack
    float dashSetupDistanceMin = 8.0f;
    float dashSetupDistanceMax = 10.0f;
    float dashSetupMinimumMoveDistance = 5.0f;  // �_�b�V���ړ��Œ዗��
    DashBTPhase dashBTPhase = DashBTPhase::None;
    bool dashBTAttackStarted = false;
    BossActionType dashBTPreviousAction = BossActionType::AttackLA;
    float dashBTTelegraphElapsed = 0.0f;
    float dashBTTraveledDistance = 0.0f;
    float dashWindupDuration = 1.40f;   // �\������̎���
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
    bool IsChargeBTExecutionAllowed() const;
    bool UpdateChargeAnimationWatchdogBT(float animationTime, float deltaTime);
    struct ChargeBTRuntime
    {
        ChargeBTPhase phase = ChargeBTPhase::None;
        ChargeBTStunPhase stunPhase = ChargeBTStunPhase::None;
        ChargeAttackEndReason result = ChargeAttackEndReason::None;
        bool failed = false;
        bool attackStarted = false;
        bool cooldownStarted = false;
        BossActionType previousAction = BossActionType::AttackLA;
        std::string initialStateName;
        float facingElapsed = 0.0f;
        float animationElapsed = 0.0f;
        float animationStalled = 0.0f;
        float previousAnimationTime = 0.0f;
        float stunElapsed = 0.0f;
        float recoveryDuration = 0.0f;
    } chargeBT;
    float chargeSetupDistanceMin = 8.0f;
    float chargeSetupDistanceMax = 10.0f;
    float chargeSetupMinimumMoveDistance = 3.0f;
    float chargeWindupEndTime = 2.90f;
    float chargeSpeed = 12.0f;
    float chargeSafetyTimeout = 8.0f;
    float chargeWallCastSafetyMargin = 0.10f;
    float chargeWallFacingThreshold = 0.70f;
    float chargeWallNormalYThreshold = 0.60f;
    float chargeWallCastRadiusScale = 0.80f;
    float chargeElapsedTime = 0.0f;
    bool chargeMovementActive = false;
    bool chargeDangerWindowActive = false;
    bool chargeJustDodgeSuccessDebug = false;
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
    ChargeAttackEndReason pendingChargeRecoveryResult = ChargeAttackEndReason::None;

    // Wall Hit��̍s���s�\���ԁBStart/End Animation���ԂƂ͕�������B
    float stunDuration = 2.5f;
    std::string stunPhaseDebug = "None";
    float stunElapsedDebug = 0.0f;
    std::string deathAnimationName = "Death_A_0";
    bool cinematicDeathAnimationOwnedExternally = false;

    // Recovery�ւ̑J�ڌ�����x�����㏑���ł���Duration�B
    float chargePlayerHitRecoveryDuration = 0.8f;   // ��Dash�U����Player�ɓ�����������recovery����
    float chargeJustDodgeRecoveryDuration = 1.0f;   // ��Dash�U���̃W���X�g�����ꂽ����recovery����
    float postStunRecoveryDuration = 0.1f;  // ��Dash�U���̕ǂɓ�����������recovery����
    std::optional<float> nextRecoveryDuration;
    std::string nextRecoverySource = "Default";
    float currentRecoveryDurationDebug = 0.0f;
    float recoveryElapsedDebug = 0.0f;
    std::string recoverySourceDebug = "Default";

    bool isDeathPerform = false;
    bool beginHuskParticleRequest = false;
    float pitchBaseValue = 0.45f;

    // ���ڂ̈ʒu�p�R���|�[�l���g��ǉ��@�ÈłŌ���ڂ̕\���p
    std::shared_ptr<SceneComponent> leftEyeSceneComponent;
    // �E�ڂ̈ʒu�p�R���|�[�l���g��ǉ��@�ÈłŌ���ڂ̕\���p
    std::shared_ptr<SceneComponent> rightEyeSceneComponent;

    // �J�����̒����_�̈ʒu
    std::shared_ptr<SceneComponent> cameraTargetComponent;
    // �{�X�펞�̃I�t�Z�b�g
    float bossBattleCameraDistance = 0.0f;
    //float bossBattleCameraRightDistance = 2.5f;
    float bossBattleCameraRightDistance = 0.0f;
    DirectX::XMFLOAT3 bossBattleCameraOffset = { 0.0f,0.0f,0.0f };

    // �O�t���[���̍��̕���
    DirectX::XMFLOAT3 prevWeaponLeftRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponLeftMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponLeftTipPos = { 0.0f,0.0f,0.0f };

    // �O�t���[���̉E�̕���
    DirectX::XMFLOAT3 prevWeaponRightRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponRightMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponRightTipPos = { 0.0f,0.0f,0.0f };

    float hitWeaponRadius = 0.8f;
    float activeLeftHitBoxRadius = 0.8f;
    float activeRightHitBoxRadius = 0.8f;
    std::vector<const AnimationNotifyState*> activeHitBoxNotifyStates;   // ����̔��a
    float enemyScale = 1.7f;    // �G�̃X�P�[��
    float hitEnemyEffectOffsetY = 2.2f;  // �q�b�g�G�t�F�N�g�̃I�t�Z�b�gY
    float hitPlayerEffectOffsetY = 2.4f;  // �q�b�g�G�t�F�N�g�̃I�t�Z�b�gY

    // �o��V�[���̃{�X���O��UI
    std::shared_ptr<UIImageComponent> gruxNameImageComponent;
    std::unique_ptr<EasingRunner> easingRunner;
    float easingFactorAlpha = 0.0f;

    // ���b�N�I���̃C���[�W���f��
    std::shared_ptr<SkeletalMeshComponent> lockOnTargetMeshComponent;
    float lockOnOffset = 0.0f;  // �v���C���[���ɉ����o���I�t�Z�b�g
    float lockOnOffsetY = 1.65f;
    // ���b�N�I���̃C���[�WUI
    std::shared_ptr<UIImageComponent> lockOnTargetImageComponent;

    // �W���X�g���̋�`�͈̔�
    DirectX::XMFLOAT3 justDodgeAreaSize = { 0.0f,2.0f,0.0f };
    // �W���X�g���̋�`�̃I�t�Z�b�g
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

    float damageFlashDuration = 0.20f;
    float damageFlashStartValue = 0.60f;
    float hitVoiceCooldown = 0.50f;
    float hitVoiceCooldownTimer = 0.0f;
    int lastHitVoiceIndex = -1;

    // �A�j���[�V�������ɂǂꂭ�炢�ړ����邩
    std::vector<AnimationMotionWarp> animationMotionWarps;

    friend class GruxEnemyEyeActor;

    // �r�w�C�r�A�c���[
    std::unique_ptr<BehaviorTree>	aiTree = nullptr;
    std::unique_ptr<BehaviorData>	behaviorData = nullptr;
    NodeBase* activeNode = nullptr;
    bool behaviorTreeFastComboEnabled = false;
    CloseCombatSettings closeCombatSettings;
    bool showCloseCombatDebugRange = false;
    std::string behaviorTreeCurrentNode = "None";
    std::string behaviorTreePreviousNode = "None";
    std::string behaviorTreeLastResult = "None";
    std::string behaviorTreeLastJudgment = "None";
    BehaviorAttackResult behaviorAttackResult = BehaviorAttackResult::None;
    bool behaviorApproachActive = false;
    float fastComboPrepareDuration = 0.5f;
    float fastComboApproachMaxDuration = 1.5f;
    float fastComboApproachRetryCooldown = 2.0f;
    float fastComboApproachRetryRemaining = 0.0f;
    float behaviorIdleDuration = 1.0f;// �ҋ@����

};

class KnightActor : public Character
{
public:
    explicit KnightActor(const std::string& actorName) :Character(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

private:
    // �`��p�R���|�[�l���g��ǉ�
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
    // �`��p�R���|�[�l���g��ǉ�
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
    // �`��p�R���|�[�l���g��ǉ�
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<RotationComponent> rotationComponent;

};


