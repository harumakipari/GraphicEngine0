#include "pch.h"
#include "GruxDashAttackBT.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/State/StateMachine.h"

bool DashPlanAvailable::Judgment()
{
    const auto machine = owner->GetStateMachine();
    return owner->IsBattleAIActive() &&
        (!machine || std::strcmp(machine->GetStateName(), "EnemyStunState") != 0) &&
        owner->CanPlanDashAttack();
}

ActionBase::State CanPlanDashAttack::Run(float)
{
    const bool ready = owner->CanPlanDashAttack();
    owner->SetBehaviorTreeLastJudgment(ready ? "CanPlanDashAttack: true" : "CanPlanDashAttack: false");
    return ready ? State::Complete : State::Failed;
}

ActionBase::State PrepareDashSetupTarget::Run(float)
{
    return owner->PrepareDashAttackSetupTarget() ? State::Complete : State::Failed;
}

ActionBase::State FaceDashPlayerIfNeeded::Run(float deltaTime)
{
    const auto context = owner->BuildTargetContext();
    if (!context.valid || owner->IsDead())
        return State::Failed;
    owner->SetDashBTFacing();
    // Same face-complete policy as Jump; no FastCombo range dependency.
    if (context.absoluteAngleDegrees <= owner->GetTurnCompleteAngle())
        return State::Complete;
    owner->RotateTowardsPlayer(context.directionToPlayer, owner->GetTurnSpeed(), deltaTime, "BT_DashFacePlayer");
    return State::Run;
}

ActionBase::State CanExecuteDashAttack::Run(float)
{
    const bool ready = owner->CanExecuteDashAttack();
    owner->SetBehaviorTreeLastJudgment(ready ? "CanExecuteDashAttack: true" : "CanExecuteDashAttack: false");
    return ready ? State::Complete : State::Failed;
}

ActionBase::State StartDashAttack::Run(float)
{
    return owner->StartDashAttackTelegraph() ? State::Complete : State::Failed;
}

ActionBase::State ExecuteDashAttack::Run(float deltaTime)
{
    switch (owner->UpdateDashAttackBT(deltaTime))
    {
    case GruxEnemy::DashBTResult::Running: return State::Run;
    case GruxEnemy::DashBTResult::Complete: return State::Complete;
    default: return State::Failed;
    }
}

bool GruxEnemy::CanPlanDashAttack() const
{
    if (IsDead() || !BuildTargetContext().valid ||
        (stateMachine_ && std::strcmp(stateMachine_->GetStateName(), "EnemyDeathState") == 0))
        return false;
    // Cooldowns are indexed by Action, not by the independently ordered Attack table.
    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        if (combatActionData[i].type == BossActionType::DashAttack)
            return combatActionCooldownRemaining[i] <= 0.0f;
    }
    return false;
}

bool GruxEnemy::PrepareDashAttackSetupTarget()
{
    if (!CanPlanDashAttack())
        return false;
    dashBTPhase = DashBTPhase::Setup;
    dashBTPreviousAction = selectedActionType;
    selectedAttackType = BossAttackType::DashAttack;
    selectedActionType = BossActionType::DashAttack;
    SetBehaviorAttackResult(BehaviorAttackResult::None);
    StopAttackSetupMovement();
    ClearAttackSetupTarget();

    DirectX::XMFLOAT3 target{};
    float chosenDistance = 0.0f;
    int candidateCount = 0;
    if (!FindAttackSetupTarget(dashSetupDistanceMin, dashSetupDistanceMax,
        attackSetupCandidateAngleStep, attackSetupClampTolerance, dashSetupMinimumMoveDistance,
        target, chosenDistance, candidateCount))
    {
        CleanupDashAttackBT();
        return false;
    }
    // The only write of the setup destination for this plan. Movement never resamples it.
    attackSetupTarget = {};
    attackSetupTarget.valid = true;
    attackSetupTarget.targetPosition = target;
    // The shared mover compares per-frame displacement; avoid false stalls at high FPS.
    // This context value is Dash-only and does not change Jump's movement settings.
    attackSetupTarget.stuckMovementThreshold = 0.01f;
    const auto position = GetPosition();
    const float dx = target.x - position.x;
    const float dz = target.z - position.z;
    attackSetupChosenDistance = chosenDistance;
    attackSetupCandidateCount = candidateCount;
    attackSetupPlannedMoveDistance = std::sqrt(dx * dx + dz * dz);
    attackSetupRemainingDistance = attackSetupPlannedMoveDistance;
    return true;
}

bool GruxEnemy::CanExecuteDashAttack() const
{
    return !IsDead() && BuildTargetContext().valid &&
        dashBTPhase == DashBTPhase::Facing && !dashBTAttackStarted &&
        selectedAttackType == BossAttackType::DashAttack && attackSetupTarget.valid &&
        !attackSetupMovementActive && !dashAttackMovementActive &&
        characterMovementComponent && rotationComponent && GetBodyAnimationController();
}

bool GruxEnemy::StartDashAttackTelegraph()
{
    if (!CanExecuteDashAttack())
        return false;
    const auto controller = GetBodyAnimationController();
    for (const char* animation : { "Pre_Stampede_0", "Stampede_0", "Stampede_Knockup_0" })
    {
        if (!controller->GetAnimationAsset(animation))
            return false;
    }
    StopAttackSetupMovement();
    ClearAttackSetupTarget();
    StartAttack(); // Exactly once for the entire three-stage attack.
    dashBTAttackStarted = true;
    dashBTTelegraphElapsed = 0.0f;
    dashBTPhase = DashBTPhase::Telegraph;
    if (!PlayAttackStage(BossAttackType::DashAttack, 0))
        return false;
    CommitPendingCombatBagAttack();
    OnSelectedActionStartedSuccessfully();
    return true;
}

GruxEnemy::DashBTResult GruxEnemy::UpdateDashAttackBT(float deltaTime)
{
    const auto controller = GetBodyAnimationController();
    if (IsDead() || !controller || !dashBTAttackStarted)
        return DashBTResult::Failed;
    const float dt = (std::max)(0.0f, deltaTime);
    switch (dashBTPhase)
    {
    case DashBTPhase::Telegraph:
    {
        const auto context = BuildTargetContext();
        if (!context.valid)
            return DashBTResult::Failed;
        StopAIMovement();
        RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt, "BT_DashTelegraph");
        dashBTTelegraphElapsed += dt;
        if (dashBTTelegraphElapsed < GetDashWindupDuration())
            return DashBTResult::Running;
        BeginAdditionalAttackStage();
        // Stage 1 calls BeginDashAttackMovement once, sampling the latest target.
        if (!PlayAttackStage(BossAttackType::DashAttack, 1))
            return DashBTResult::Failed;
        dashBTPhase = DashBTPhase::Movement;
        return DashBTResult::Running;
    }
    case DashBTPhase::Movement:
    {
        const auto position = GetPosition();
        dashBTTraveledDistance = (position.x - dashAttackStartPosition.x) * dashAttackDirection.x +
            (position.z - dashAttackStartPosition.z) * dashAttackDirection.z;
        const bool finished = UpdateDashAttackMovement(dt, true);
        if (!finished && controller->IsPlayAnimation())
            return DashBTResult::Running;
        StopDashAttackMovement();
        BeginAdditionalAttackStage();
        if (!PlayAttackStage(BossAttackType::DashAttack, 2))
            return DashBTResult::Failed;
        dashBTPhase = DashBTPhase::Knockup;
        return DashBTResult::Running;
    }
    case DashBTPhase::Knockup:
        // Hit / Just Dodge never short-circuits the final attack animation.
        if (controller->IsPlayAnimation())
            return DashBTResult::Running;
        SetBehaviorAttackResult(WasCurrentAttackSequenceJustDodged()
            ? BehaviorAttackResult::JustDodged : BehaviorAttackResult::Success);
        OnSelectedAttackCompletedSuccessfully();
        FinishDashAttackBT();
        return DashBTResult::Complete;
    default:
        return DashBTResult::Failed;
    }
}

void GruxEnemy::FinishDashAttackBT()
{
    CleanupDashAttackBT();
    dashBTPhase = DashBTPhase::Recovery;
}

void GruxEnemy::CleanupDashAttackBT()
{
    if (!IsDashAttackBTActive())
        return;
    if (dashBTAttackStarted)
    {
        // Match EnemyAttackState::Exit timing, including interruption after StartAttack.
        StartSelectedActionCooldown();
        dashBTAttackStarted = false;
    }
    StopDashAttackMovement();
    ClearAttackSetupTarget();
    // Do not replace an externally started Death / Stun animation with positioning idle.
    const bool externallyInterrupted = IsDead() || (stateMachine_ &&
        (std::strcmp(stateMachine_->GetStateName(), "EnemyDeathState") == 0 ||
            std::strcmp(stateMachine_->GetStateName(), "EnemyStunState") == 0));
    if (!externallyInterrupted)
        EndPositioningAnimation();
    DisableAttackHitBoxes();
    dashBTPhase = DashBTPhase::None;
    dashBTTelegraphElapsed = 0.0f;
    dashBTTraveledDistance = 0.0f;
    dashAttackDirection = {};
    dashAttackStartPosition = {};
    dashTargetPosition = {};
    currentDashAttackPlayerDistance = 0.0f;
    calculatedDashAttackDistance = 0.0f;
    selectedActionType = dashBTPreviousAction;
}
