#include "pch.h"
#include "GruxChargeAttackBT.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Actors/Player/Player.h"
#include "Game/State/StateMachine.h"
#include "Engine/Scene/Scene.h"
#include "Core/ActorManager.h"
#include "Game/Scenes/GameScene.h"

namespace
{
    ActionBase::State ToActionState(GruxEnemy::ChargeBTStepResult result)
    {
        switch (result)
        {
        case GruxEnemy::ChargeBTStepResult::Running: return ActionBase::State::Run;
        case GruxEnemy::ChargeBTStepResult::Complete: return ActionBase::State::Complete;
        default: return ActionBase::State::Failed;
        }
    }
}

bool CanPlanChargeAttack::Judgment()
{
    const bool ready = owner->CanPlanChargeAttack();
    owner->SetBehaviorTreeLastJudgment(ready ? "CanPlanChargeAttack: true" : "CanPlanChargeAttack: false");
    return ready;
}
ActionBase::State PrepareChargeSetupTarget::Run(float)
{
    return owner->PrepareChargeAttackSetupTarget() ? State::Complete : State::Failed;
}
ActionBase::State FaceChargePlayerIfNeeded::Run(float dt)
{
    return ToActionState(owner->UpdateChargeAttackFacingBT(dt));
}
ActionBase::State CanExecuteChargeAttack::Run(float)
{
    const bool ready = owner->CanExecuteChargeAttack();
    owner->SetBehaviorTreeLastJudgment(ready ? "CanExecuteChargeAttack: true" : "CanExecuteChargeAttack: false");
    if (!ready)
        owner->FailChargeAttackBT();
    // A committed setup with a lost target/controller resolves through normal recovery.
    return owner->IsChargeAttackBTActive() ? State::Complete : State::Failed;
}
ActionBase::State StartChargeAttack::Run(float)
{
    owner->StartChargeAttackBT();
    return owner->IsChargeAttackBTActive() ? State::Complete : State::Failed;
}
ActionBase::State ExecuteChargeAttack::Run(float dt)
{
    return ToActionState(owner->UpdateChargeAttackBT(dt));
}
ActionBase::State ResolveChargeResult::Run(float dt)
{
    return ToActionState(owner->ResolveChargeResultBT(dt));
}
ActionBase::State ExecuteChargeRecovery::Run(float dt)
{
    if (!owner->IsChargeAttackBTActive())
    {
        timerAction.Reset();
        return State::Failed;
    }
    if (owner->GetChargeBTPhase() == GruxEnemy::ChargeBTPhase::RecoveryPending)
    {
        timerAction.Reset(); // Also resets an interrupted recovery before StartAttack was called.
        owner->BeginChargeRecoveryBT();
    }
    if (owner->GetChargeBTPhase() == GruxEnemy::ChargeBTPhase::Recovery)
    {
        if (timerAction.Run(dt) == State::Run)
            return State::Run;
        owner->MarkChargeRecoveryTimerFinishedBT();
    }
    return ToActionState(owner->FinishChargeRecoveryBT());
}

bool GruxEnemy::IsChargeBTExecutionAllowed() const
{
    return !IsDead() && battleAIActive && !finalHitReactionActive && !finalHitReactionHeld &&
        !IsAnimationEditorPreviewActive() &&
        (!stateMachine_ || (std::strcmp(stateMachine_->GetStateName(), "EnemyDeathState") != 0 &&
            std::strcmp(stateMachine_->GetStateName(), "EnemyStunState") != 0));
}

bool GruxEnemy::CanPlanChargeAttack() const
{
    if (!IsChargeBTExecutionAllowed() || !BuildTargetContext().valid ||
        IsChargeAttackBTActive() || IsDashAttackBTActive() || chargeMovementActive || dashAttackMovementActive ||
        !characterMovementComponent || !rotationComponent || !GetBodyAnimationController())
        return false;
    for (size_t i = 0; i < combatActionData.size(); ++i)
        if (combatActionData[i].type == BossActionType::ChargeAttack)
            return forceBehaviorTreeCharge || combatActionCooldownRemaining[i] <= 0.0f;
    return false;
}

bool GruxEnemy::PrepareChargeAttackSetupTarget()
{
    if (!CanPlanChargeAttack())
        return false;
    chargeBT = {};
    chargeBT.phase = ChargeBTPhase::Setup;
    chargeBT.previousAction = selectedActionType;
    chargeBT.initialStateName = stateMachine_ ? stateMachine_->GetStateName() : "";
    selectedActionType = BossActionType::ChargeAttack;
    selectedAttackType = BossAttackType::ChargeAttack;
    SetBehaviorAttackResult(BehaviorAttackResult::None);
    StopAttackSetupMovement();
    ClearAttackSetupTarget();
    nextRecoveryDuration.reset();
    pendingChargeRecoveryResult = ChargeAttackEndReason::None;
    DirectX::XMFLOAT3 target{};
    float chosenDistance = 0.0f;
    int candidateCount = 0;
    const float chargeCastRadius = (std::max)(0.05f, radius * chargeWallCastRadiusScale);
    constexpr float chargeSetupExtraClearance = 0.05f;
    const float chargeSetupBoundaryMargin = (std::max)(
        bossRoomSafetyMargin, chargeCastRadius + chargeWallCastSafetyMargin + chargeSetupExtraClearance);
    if (!FindAttackSetupTarget(chargeSetupDistanceMin, chargeSetupDistanceMax,
        attackSetupCandidateAngleStep, attackSetupClampTolerance, chargeSetupMinimumMoveDistance,
        target, chosenDistance, candidateCount, chargeSetupBoundaryMargin))
    {
        CleanupChargeAttackBT();
        return false;
    }
    // The only setup destination write for this plan. Shared movement never resamples it.
    attackSetupTarget = {};
    attackSetupTarget.valid = true;
    attackSetupTarget.targetPosition = target;
    attackSetupTarget.stuckMovementThreshold = 0.01f; // Same per-frame tolerance as Dash's context.
    const auto position = GetPosition();
    const float dx = target.x - position.x, dz = target.z - position.z;
    attackSetupChosenDistance = chosenDistance;
    attackSetupCandidateCount = candidateCount;
    attackSetupPlannedMoveDistance = std::sqrt(dx * dx + dz * dz);
    attackSetupRemainingDistance = attackSetupPlannedMoveDistance;
    return true;
}

GruxEnemy::ChargeBTStepResult GruxEnemy::UpdateChargeAttackFacingBT(float dt)
{
    if (!IsChargeAttackBTActive()) return ChargeBTStepResult::Failed;
    chargeBT.phase = ChargeBTPhase::Facing;
    const auto context = BuildTargetContext();
    if (!context.valid || !rotationComponent)
    {
        FailChargeAttackBT();
        return ChargeBTStepResult::Complete;
    }
    if (context.absoluteAngleDegrees <= GetTurnCompleteAngle())
        return ChargeBTStepResult::Complete;
    chargeBT.facingElapsed += (std::max)(0.0f, dt);
    if (chargeBT.facingElapsed >= (std::max)(0.1f, GetTurnTimeout()))
    {
        FailChargeAttackBT();
        return ChargeBTStepResult::Complete;
    }
    RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt, "BT_ChargeFacePlayer");
    return ChargeBTStepResult::Running;
}

bool GruxEnemy::CanExecuteChargeAttack() const
{
    return IsChargeBTExecutionAllowed() && BuildTargetContext().valid &&
        chargeBT.phase == ChargeBTPhase::Facing && !chargeBT.attackStarted &&
        attackSetupTarget.valid && !attackSetupMovementActive && !chargeMovementActive &&
        selectedAttackType == BossAttackType::ChargeAttack &&
        characterMovementComponent && rotationComponent && GetBodyAnimationController();
}

void GruxEnemy::FailChargeAttackBT()
{
    if (!IsChargeAttackBTActive()) return;
    StopChargeAttackMovement();
    DisableAttackHitBoxes();
    ClearAttackSetupTarget();
    chargeBT.failed = true;
    chargeBT.result = ChargeAttackEndReason::None; // Keep failure distinct without changing the legacy enum.
    chargeBT.phase = ChargeBTPhase::Result;
    SetChargePhaseDebug("Failed");
}

void GruxEnemy::StartChargeAttackBT()
{
    if (chargeBT.phase == ChargeBTPhase::Result) return;
    if (!CanExecuteChargeAttack()) { FailChargeAttackBT(); return; }
    const auto controller = GetBodyAnimationController();
    if (!controller->GetAnimationAsset("Pre_FootSlide_0") || !controller->GetAnimationAsset("Stampede_0"))
    {
        FailChargeAttackBT();
        return;
    }
    StopAttackSetupMovement();
    ClearAttackSetupTarget();
    StartAttack(); // Once per whole attack, never once per leg.
    chargeBT.attackStarted = true;
    chargeBT.tripleChargeActive = IsPhase2ChargeActive();
    chargeBT.triplePhase = chargeBT.tripleChargeActive
        ? TripleChargePhase::InitialWindup : TripleChargePhase::None;
    chargeBT.tripleChargeIndex = 0;
    chargeBT.tripleChargeTransitionElapsed = 0.0f;
    chargeBT.tripleChargeLegElapsed = 0.0f;
    chargeBT.tripleChargeLegDistance = 0.0f;
    chargeBT.tripleFinalWallHit = false;
    chargeBT.tripleEarlyWallHit = false;
    chargeBT.tripleCurrentWallClearance = 0.0f;
    chargeBT.tripleWallTurnCandidate = false;
    chargeBT.tripleWallTurnTriggered = false;
    chargeBT.activeStunDuration = GetStunDuration();
    chargeBT.phase = ChargeBTPhase::Telegraph;
    chargeBT.animationElapsed = 0.0f;
    chargeBT.animationStalled = 0.0f;
    chargeBT.previousAnimationTime = 0.0f;
    chargeDirectionLocked = false;
    chargeDirection = {};
    SetPendingChargeRecoveryResult(ChargeAttackEndReason::None);
    SetChargePhaseDebug("Windup");
    PlayBodyAnimation("Pre_FootSlide_0", false, true, 0.1f, true);
    OnSelectedActionStartedSuccessfully();
}

bool GruxEnemy::BeginSingleChargeBT()
{
    // Existing entry owns the direction snapshot, timer, danger flag, animation and velocity.
    if (!BeginChargeAttackMovement()) return false;
    chargeBT.result = ChargeAttackEndReason::None;
    chargeBT.phase = ChargeBTPhase::Charging;
    SetChargePhaseDebug("Charging");
    return true;
}

bool GruxEnemy::IsPhase2ChargeActive()
{
    const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
    return scene && scene->IsBossInFinalPhase();
}

bool GruxEnemy::BeginTripleChargeLeg()
{
    if (!BeginChargeAttackMovement())
        return false;
    chargeBT.result = ChargeAttackEndReason::None;
    const int index = std::clamp(chargeBT.tripleChargeIndex, 0, 2);
    chargeBT.tripleChargeDirections[index] = chargeDirection;
    chargeBT.tripleChargeStartPositions[index] = GetPosition();
    chargeBT.tripleChargeLegElapsed = 0.0f;
    chargeBT.tripleChargeLegDistance = 0.0f;
    chargeBT.tripleCurrentWallClearance = 0.0f;
    chargeBT.tripleWallTurnCandidate = false;
    chargeBT.tripleWallTurnTriggered = false;
    chargeBT.triplePhase = TripleChargePhase::Charging;
    chargeBT.phase = ChargeBTPhase::Charging;
    SetChargePhaseDebug(index == 0 ? "TripleCharge1" : index == 1 ? "TripleCharge2" : "TripleCharge3");
    return true;
}

bool GruxEnemy::BeginTripleChargeTransition()
{
    if (chargeBT.tripleChargeIndex > 2)
        return false;
    StopChargeAttackMovement();
    chargeBT.triplePhase = TripleChargePhase::InterChargeTransition;
    chargeBT.tripleChargeTransitionElapsed = 0.0f;
    chargeBT.tripleWallTurnTriggered = false;
    SetChargePhaseDebug("TripleTransition");
    return true;
}

GruxEnemy::ChargeBTStepResult GruxEnemy::UpdateChargeAttackBT(float dt)
{
    return chargeBT.tripleChargeActive ? UpdateTripleChargeBT(dt) : UpdateSingleChargeBT(dt);
}

GruxEnemy::ChargeBTStepResult GruxEnemy::UpdateTripleChargeBT(float dt)
{
    if (!IsChargeAttackBTActive()) return ChargeBTStepResult::Failed;
    if (chargeBT.phase == ChargeBTPhase::Result) return ChargeBTStepResult::Complete;
    const auto controller = GetBodyAnimationController();
    if (!controller || !characterMovementComponent || !rotationComponent)
    {
        FailChargeAttackBT();
        chargeBT.triplePhase = TripleChargePhase::Aborted;
        return ChargeBTStepResult::Complete;
    }

    if (chargeBT.phase == ChargeBTPhase::Telegraph)
    {
        const auto context = BuildTargetContext();
        if (!context.valid || controller->GetCurrentAnimationName() != "Pre_FootSlide_0")
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        const float animationTime = controller->GetCurrentAnimationTime();
        SetChargeWindupAnimationTimeDebug(animationTime);
        if (animationTime < chargeDirectionLockTime)
            RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt, "BT_TripleChargeWindup");
        else if (!chargeDirectionLocked && !LockChargeDirectionToPlayer())
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        if (animationTime >= GetChargeWindupEndTime())
        {
            if (!BeginTripleChargeLeg())
            {
                FailChargeAttackBT();
                chargeBT.triplePhase = TripleChargePhase::Aborted;
                return ChargeBTStepResult::Complete;
            }
            return ChargeBTStepResult::Running;
        }
        if (!controller->IsPlayAnimation() || !UpdateChargeAnimationWatchdogBT(animationTime, dt))
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        return ChargeBTStepResult::Running;
    }

    if (chargeBT.triplePhase == TripleChargePhase::InterChargeTransition)
    {
        const auto context = BuildTargetContext();
        if (!context.valid)
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        chargeBT.tripleChargeTransitionElapsed += (std::max)(0.0f, dt);
        RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt, "BT_TripleChargeTransition");
        if (chargeBT.tripleChargeTransitionElapsed < chargeBT.tripleChargeTransitionDuration)
            return ChargeBTStepResult::Running;
        chargeDirectionLocked = false;
        if (!LockChargeDirectionToPlayer() || !BeginTripleChargeLeg())
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        return ChargeBTStepResult::Running;
    }

    if (chargeBT.phase != ChargeBTPhase::Charging || chargeBT.triplePhase != TripleChargePhase::Charging)
        return ChargeBTStepResult::Failed;
    if (controller->GetCurrentAnimationName() != "Stampede_0")
    {
        FailChargeAttackBT();
        chargeBT.triplePhase = TripleChargePhase::Aborted;
        return ChargeBTStepResult::Complete;
    }

    chargeBT.tripleChargeLegElapsed += (std::max)(0.0f, dt);
    const auto currentPosition = GetPosition();
    const auto startPosition = chargeBT.tripleChargeStartPositions[std::clamp(chargeBT.tripleChargeIndex, 0, 2)];
    const float dx = currentPosition.x - startPosition.x;
    const float dz = currentPosition.z - startPosition.z;
    chargeBT.tripleChargeLegDistance = std::sqrt(dx * dx + dz * dz);
    const bool finalLeg = chargeBT.tripleChargeIndex >= 2;
    chargeBT.result = UpdateChargeAttackMovement(dt, !finalLeg);
    if (chargeBT.result == ChargeAttackEndReason::None)
    {
        if (!finalLeg && (chargeBT.tripleChargeLegDistance >= chargeBT.tripleChargeLegMaxDistance ||
            chargeBT.tripleChargeLegElapsed >= chargeBT.tripleChargeLegMaxDuration))
        {
            chargeBT.result = ChargeAttackEndReason::LegComplete;
            chargeBT.tripleWallTurnTriggered = false;
            chargeSelectedHitDebug = "LegCompleteSafety";
            StopChargeAttackMovement();
        }
        else
            return ChargeBTStepResult::Running;
    }

    if (chargeBT.result == ChargeAttackEndReason::LegComplete)
    {
        ++chargeBT.tripleChargeIndex;
        if (!BeginTripleChargeTransition())
        {
            chargeBT.triplePhase = TripleChargePhase::Completed;
            chargeBT.phase = ChargeBTPhase::Result;
            chargeBT.result = ChargeAttackEndReason::SafetyTimeout;
            chargeBT.failed = true;
            return ChargeBTStepResult::Complete;
        }
        return ChargeBTStepResult::Running;
    }

    DisableAttackHitBoxes();
    if (chargeBT.tripleChargeIndex < 2 && chargeBT.result == ChargeAttackEndReason::WallHit)
    {
        chargeBT.tripleEarlyWallHit = true;
        chargeBT.triplePhase = TripleChargePhase::Aborted;
    }
    else if (chargeBT.tripleChargeIndex >= 2 && chargeBT.result == ChargeAttackEndReason::WallHit)
    {
        chargeBT.tripleFinalWallHit = true;
        chargeBT.triplePhase = TripleChargePhase::Completed;
    }
    else
        chargeBT.triplePhase = TripleChargePhase::Aborted;
    chargeBT.phase = ChargeBTPhase::Result;
    return ChargeBTStepResult::Complete;
}

bool GruxEnemy::UpdateChargeAnimationWatchdogBT(float animationTime, float dt)
{
    const float safeDt = (std::max)(0.0f, dt);
    chargeBT.animationElapsed += safeDt;
    if (animationTime > chargeBT.previousAnimationTime + 0.00001f)
        chargeBT.animationStalled = 0.0f;
    else
        chargeBT.animationStalled += safeDt;
    chargeBT.previousAnimationTime = animationTime;
    return chargeBT.animationStalled < 1.0f && chargeBT.animationElapsed < 10.0f;
}

GruxEnemy::ChargeBTStepResult GruxEnemy::UpdateSingleChargeBT(float dt)
{
    if (!IsChargeAttackBTActive()) return ChargeBTStepResult::Failed;
    if (chargeBT.phase == ChargeBTPhase::Result) return ChargeBTStepResult::Complete;
    const auto controller = GetBodyAnimationController();
    if (!controller || !characterMovementComponent || !rotationComponent)
    {
        FailChargeAttackBT();
        return ChargeBTStepResult::Complete;
    }
    if (chargeBT.phase == ChargeBTPhase::Telegraph)
    {
        const auto context = BuildTargetContext();
        if (!context.valid || controller->GetCurrentAnimationName() != "Pre_FootSlide_0")
        {
            FailChargeAttackBT();
            return ChargeBTStepResult::Complete;
        }
        const float animationTime = controller->GetCurrentAnimationTime();
        SetChargeWindupAnimationTimeDebug(animationTime);
        if (animationTime < chargeDirectionLockTime)
            RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt, "BT_ChargeWindup");
        else if (!chargeDirectionLocked && !LockChargeDirectionToPlayer())
        {
            FailChargeAttackBT();
            return ChargeBTStepResult::Complete;
        }
        if (animationTime >= GetChargeWindupEndTime())
        {
            if (!BeginSingleChargeBT()) FailChargeAttackBT();
            return chargeBT.failed ? ChargeBTStepResult::Complete : ChargeBTStepResult::Running;
        }
        if (!controller->IsPlayAnimation() || !UpdateChargeAnimationWatchdogBT(animationTime, dt))
        {
            FailChargeAttackBT();
            return ChargeBTStepResult::Complete;
        }
        return ChargeBTStepResult::Running;
    }
    if (chargeBT.phase != ChargeBTPhase::Charging) return ChargeBTStepResult::Failed;
    if (controller->GetCurrentAnimationName() != "Stampede_0")
    {
        FailChargeAttackBT();
        return ChargeBTStepResult::Complete;
    }
    // A single leg reports only its outcome. No Stun, recovery policy or cooldown here.
    chargeBT.result = UpdateChargeAttackMovement(dt);
    if (chargeBT.result == ChargeAttackEndReason::None) return ChargeBTStepResult::Running;
    DisableAttackHitBoxes();
    chargeBT.phase = ChargeBTPhase::Result;
    return ChargeBTStepResult::Complete;
}

bool GruxEnemy::BeginChargeStunBT()
{
    const auto controller = GetBodyAnimationController();
    if (!controller) return false;
    for (const char* name : {"Knock_Down_Start", "Knock_Down_Loop", "Knock_Down_End"})
        if (!controller->GetAnimationAsset(name)) return false;
    StopChargeAttackMovement();
    DisableAttackHitBoxes();
    chargeBT.phase = ChargeBTPhase::Stun;
    chargeBT.activeStunDuration = chargeBT.tripleFinalWallHit
        ? GetStunDuration() * chargeBT.tripleWallStunDurationMultiplier
        : GetStunDuration();
    chargeBT.stunPhase = ChargeBTStunPhase::Start;
    chargeBT.stunElapsed = 0.0f;
    chargeBT.animationElapsed = chargeBT.animationStalled = chargeBT.previousAnimationTime = 0.0f;
    SetStunDebug("Start", 0.0f);
    PlayBodyAnimation("Knock_Down_Start", false, true, 0.1f, true);
    return true;
}

GruxEnemy::ChargeBTStepResult GruxEnemy::UpdateChargeStunBT(float dt)
{
    const auto controller = GetBodyAnimationController();
    if (!controller) return ChargeBTStepResult::Failed;
    const char* expected = chargeBT.stunPhase == ChargeBTStunPhase::Start ? "Knock_Down_Start" :
        chargeBT.stunPhase == ChargeBTStunPhase::Loop ? "Knock_Down_Loop" : "Knock_Down_End";
    if (controller->GetCurrentAnimationName() != expected) return ChargeBTStepResult::Failed;
    if (chargeBT.stunPhase == ChargeBTStunPhase::Loop)
    {
        chargeBT.stunElapsed += (std::max)(0.0f, dt);
        SetStunDebug("Loop", chargeBT.stunElapsed);
        const float stunDuration = chargeBT.activeStunDuration > 0.0f
            ? chargeBT.activeStunDuration : GetStunDuration();
        if (chargeBT.stunElapsed < stunDuration) return ChargeBTStepResult::Running;
        chargeBT.stunPhase = ChargeBTStunPhase::End;
        chargeBT.animationElapsed = chargeBT.animationStalled = chargeBT.previousAnimationTime = 0.0f;
        SetStunDebug("End", chargeBT.stunElapsed);
        PlayBodyAnimation("Knock_Down_End", false, true, 0.1f, true);
        return ChargeBTStepResult::Running;
    }
    if (controller->IsPlayAnimation())
        return UpdateChargeAnimationWatchdogBT(controller->GetCurrentAnimationTime(), dt)
            ? ChargeBTStepResult::Running : ChargeBTStepResult::Failed;
    if (chargeBT.stunPhase == ChargeBTStunPhase::End) return ChargeBTStepResult::Complete;
    chargeBT.stunPhase = ChargeBTStunPhase::Loop;
    chargeBT.stunElapsed = 0.0f;
    SetStunDebug("Loop", 0.0f);
    PlayBodyAnimation("Knock_Down_Loop", true, true, 0.1f, true);
    return ChargeBTStepResult::Running;
}

void GruxEnemy::FinishChargeAttackBT()
{
    // Whole-attack boundary. Phase2 can defer this until its final leg/result is resolved.
    if (chargeBT.attackStarted && !chargeBT.cooldownStarted)
    {
        const auto savedAction = selectedActionType;
        selectedActionType = BossActionType::ChargeAttack;
        StartSelectedActionCooldown();
        selectedActionType = savedAction;
        chargeBT.cooldownStarted = true;
    }
    StopChargeAttackMovement();
    DisableAttackHitBoxes();
    ClearAttackSetupTarget();
}

GruxEnemy::ChargeBTStepResult GruxEnemy::ResolveChargeResultBT(float dt)
{
    if (!IsChargeAttackBTActive()) return ChargeBTStepResult::Failed;
    if (chargeBT.phase == ChargeBTPhase::Stun)
    {
        const auto status = UpdateChargeStunBT(dt);
        if (status == ChargeBTStepResult::Running) return status;
        if (status == ChargeBTStepResult::Failed) chargeBT.failed = true;
        chargeBT.stunPhase = ChargeBTStunPhase::None;
        SetStunDebug("None", 0.0f);
        chargeBT.recoveryDuration = status == ChargeBTStepResult::Complete
            ? GetPostStunRecoveryDuration() : GetSelectedAttackRecoveryDuration();
        SetNextRecoveryDuration(chargeBT.recoveryDuration, status == ChargeBTStepResult::Complete ? "PostStun" : "ChargeFailed");
    }
    else if (chargeBT.phase == ChargeBTPhase::Result)
    {
        // Phase1 policy. The single-charge executor does not know this mapping.
        if (!chargeBT.failed && chargeBT.result == ChargeAttackEndReason::WallHit)
        {
            OnSelectedAttackCompletedSuccessfully();
            if (BeginChargeStunBT()) return ChargeBTStepResult::Running;
            chargeBT.failed = true;
        }
        chargeBT.recoveryDuration = GetSelectedAttackRecoveryDuration();
        const char* source = chargeBT.failed ? "ChargeFailed" : "Default";
        if (!chargeBT.failed && chargeBT.result == ChargeAttackEndReason::PlayerHit)
        {
            chargeBT.recoveryDuration = GetChargePlayerHitRecoveryDuration();
            source = "ChargePlayerHit";
        }
        else if (!chargeBT.failed && chargeBT.result == ChargeAttackEndReason::JustDodge)
        {
            chargeBT.recoveryDuration = GetChargeJustDodgeRecoveryDuration();
            source = "ChargeJustDodge";
        }
        SetPendingChargeRecoveryResult(chargeBT.failed ? ChargeAttackEndReason::None : chargeBT.result);
        SetNextRecoveryDuration(chargeBT.recoveryDuration, source);
    }
    else return ChargeBTStepResult::Failed;
    FinishChargeAttackBT();
    chargeBT.phase = ChargeBTPhase::RecoveryPending;
    return ChargeBTStepResult::Complete;
}

GruxEnemy::ChargeBTStepResult GruxEnemy::FinishChargeRecoveryBT()
{
    if (chargeBT.phase != ChargeBTPhase::RecoveryPostconditions) return ChargeBTStepResult::Failed;
    if (!chargeBT.failed && chargeBT.result == ChargeAttackEndReason::JustDodge)
    {
        const auto scene = GetOwnerScene();
        const auto player = scene ? scene->GetActorManager()->GetActorOfType<Player>() : nullptr;
        if (player && player->IsRushOpportunityActive()) return ChargeBTStepResult::Running;
    }
    if (!chargeBT.failed && chargeBT.result == ChargeAttackEndReason::PlayerHit)
        RequestCombatRepositionIntent();
    CleanupChargeAttackBT();
    return ChargeBTStepResult::Complete;
}

bool GruxEnemy::ShouldAbortChargeAttackBT() const
{
    return IsChargeAttackBTActive() && (!behaviorTreeFastComboEnabled || !IsChargeBTExecutionAllowed() ||
        !activeNode || (stateMachine_ && stateMachine_->GetStateName() != chargeBT.initialStateName));
}

void GruxEnemy::CleanupChargeAttackBT()
{
    if (!IsChargeAttackBTActive()) return;
    FinishChargeAttackBT(); // Idempotent; no cooldown before StartAttack, none per leg.
    const float tripleTurnClearance = chargeBT.tripleChargeWallTurnClearance;
    const float tripleMaxDistance = chargeBT.tripleChargeLegMaxDistance;
    const float tripleMaxDuration = chargeBT.tripleChargeLegMaxDuration;
    const float tripleTransitionDuration = chargeBT.tripleChargeTransitionDuration;
    const float tripleStunMultiplier = chargeBT.tripleWallStunDurationMultiplier;
    const bool preserveAnimation = IsDead() || finalHitReactionActive || finalHitReactionHeld ||
        IsAnimationEditorPreviewActive() || (stateMachine_ && stateMachine_->GetStateName() != chargeBT.initialStateName);
    if (!preserveAnimation) EndPositioningAnimation();
    positioningAnimationMoving = false;
    positioningAnimationActualSpeed = positioningMoveStopTimer = 0.0f;
    chargeDirection = {};
    chargeDirectionLocked = false;
    chargeElapsedTime = 0.0f;
    chargeEndReasonDebug = ChargeAttackEndReason::None;
    chargeWindupAnimationTimeDebug = 0.0f;
    chargeJustDodgeSuccessDebug = chargePlayerCastHitDebug = chargeWallCastHitDebug = false;
    chargePlayerHitDistanceDebug = chargeWallHitDistanceDebug = chargeWallFacingAmountDebug = 0.0f;
    chargeWallHitNormalDebug = {};
    chargePlayerHitActorDebug = chargeSelectedHitDebug = "None";
    chargePhaseDebug = "None";
    pendingChargeRecoveryResult = ChargeAttackEndReason::None;
    nextRecoveryDuration.reset();
    nextRecoverySource = "Default";
    SetStunDebug("None", 0.0f);
    ClearPendingAttackFacing();
    selectedActionType = chargeBT.previousAction;
    chargeBT = {};
    chargeBT.tripleChargeWallTurnClearance = tripleTurnClearance;
    chargeBT.tripleChargeLegMaxDistance = tripleMaxDistance;
    chargeBT.tripleChargeLegMaxDuration = tripleMaxDuration;
    chargeBT.tripleChargeTransitionDuration = tripleTransitionDuration;
    chargeBT.tripleWallStunDurationMultiplier = tripleStunMultiplier;
}

void GruxEnemy::DrawChargeAttackBTDebug()
{
#ifdef USE_IMGUI
    ImGui::SeparatorText(U8("突進BT"));

    ImGui::DragFloat(
        U8("突進準備距離 最小"),
        &chargeSetupDistanceMin,
        0.1f, 0.1f, 30.0f, "%.2f m");

    ImGui::DragFloat(
        U8("突進準備距離 最大"),
        &chargeSetupDistanceMax,
        0.1f, 0.1f, 30.0f, "%.2f m");

    ImGui::DragFloat(
        U8("突進準備 最低移動距離"),
        &chargeSetupMinimumMoveDistance,
        0.1f, 0.0f, 30.0f, "%.2f m");

    chargeSetupDistanceMin = (std::max)(0.1f, chargeSetupDistanceMin);
    chargeSetupDistanceMax = (std::max)(chargeSetupDistanceMin, chargeSetupDistanceMax);
    chargeSetupMinimumMoveDistance = (std::max)(0.0f, chargeSetupMinimumMoveDistance);

    ImGui::DragFloat(U8("三連突進 壁前切り返し距離"), &chargeBT.tripleChargeWallTurnClearance, 0.05f, 0.0f, 5.0f, "%.2f m");
    ImGui::DragFloat(U8("三連突進 1・2段目 最大突進距離"), &chargeBT.tripleChargeLegMaxDistance, 0.1f, 0.1f, 50.0f, "%.2f m");
    ImGui::DragFloat(U8("三連突進 1・2段目 最大突進時間"), &chargeBT.tripleChargeLegMaxDuration, 0.05f, 0.1f, 10.0f, "%.2f sec");
    ImGui::DragFloat(U8("三連突進 段間切り返し時間"), &chargeBT.tripleChargeTransitionDuration, 0.01f, 0.0f, 2.0f, "%.2f sec");
    ImGui::DragFloat(U8("三連突進 最終壁スタン倍率"), &chargeBT.tripleWallStunDurationMultiplier, 0.05f, 1.0f, 5.0f, "%.2fx");
    chargeBT.tripleChargeWallTurnClearance = (std::max)(0.0f, chargeBT.tripleChargeWallTurnClearance);
    chargeBT.tripleChargeLegMaxDistance = (std::max)(0.1f, chargeBT.tripleChargeLegMaxDistance);
    chargeBT.tripleChargeLegMaxDuration = (std::max)(0.1f, chargeBT.tripleChargeLegMaxDuration);
    chargeBT.tripleChargeTransitionDuration = (std::max)(0.0f, chargeBT.tripleChargeTransitionDuration);
    chargeBT.tripleWallStunDurationMultiplier = (std::max)(1.0f, chargeBT.tripleWallStunDurationMultiplier);

    ImGui::DragFloat("Charge Player Cast Radius Scale", &chargePlayerCastRadiusScale,
        0.01f, 0.1f, 2.0f, "%.2f");
    ImGui::DragFloat("Charge Wall Cast Radius Scale", &chargeWallCastRadiusScale,
        0.01f, 0.1f, 2.0f, "%.2f");
    ImGui::Checkbox("Show Charge Cast Debug", &showChargeCastDebug);
    if (showChargeCastDebug)
    {
        ImGui::Checkbox("Show Player Cast", &showPlayerCastDebug);
        ImGui::Checkbox("Show Wall Cast", &showWallCastDebug);
    }

    // ★ 方向固定時刻を調整可能にする
    ImGui::DragFloat(
        U8("突進 方向固定時刻"),
        &chargeDirectionLockTime,
        0.01f,
        0.0f,
        GetChargeWindupEndTime(),
        "%.2f sec");

    chargeDirectionLockTime =
        std::clamp(
            chargeDirectionLockTime,
            0.0f,
            GetChargeWindupEndTime());

    // 開始時刻は現在値表示だけ
    ImGui::Text(
        U8("突進 開始時刻: %.2f sec"),
        GetChargeWindupEndTime());

    const char* phaseNames[] =
    {
        U8("待機"),
        U8("準備移動"),
        U8("旋回"),
        U8("予兆"),
        U8("突進"),
        U8("結果判定"),
        U8("スタン"),
        U8("後隙開始待ち"),
        U8("後隙"),
        U8("後隙後の条件待ち")
    };

    const char* resultNames[] =
    {
        U8("未確定"),
        U8("プレイヤー命中"),
        U8("ジャスト回避"),
        U8("壁衝突"),
        U8("段完了"),
        U8("時間切れ")
    };

    const char* stunNames[] =
    {
        U8("なし"),
        U8("開始"),
        U8("継続"),
        U8("終了")
    };

    ImGui::Text(
        U8("突進 方向固定済み: %s"),
        chargeDirectionLocked ? U8("はい") : U8("いいえ"));

    ImGui::Text(
        U8("突進 固定方向: (%.3f, %.3f, %.3f)"),
        chargeDirection.x,
        chargeDirection.y,
        chargeDirection.z);

    ImGui::Text(
        U8("突進BT状態: %s"),
        phaseNames[static_cast<int>(chargeBT.phase)]);

    ImGui::Text(
        U8("突進予兆中: %s"),
        chargeBT.phase == ChargeBTPhase::Telegraph ? U8("はい") : U8("いいえ"));

    ImGui::Text(
        U8("突進経過時間: %.2f sec"),
        chargeElapsedTime);

    ImGui::Text(
        U8("突進終了理由: %s"),
        chargeBT.failed
        ? U8("開始・実行失敗")
        : resultNames[static_cast<int>(chargeBT.result)]);

    ImGui::Text(
        U8("スタン状態: %s"),
        stunNames[static_cast<int>(chargeBT.stunPhase)]);

    ImGui::DragFloat(
        U8("今回の後隙時間: %.2f sec"),
        &chargeBT.recoveryDuration,0.05f);

    const char* triplePhaseNames[] = { U8("なし"), U8("初回予兆"), U8("突進"), U8("切り返し"), U8("完了"), U8("中断") };
    const int triplePhaseIndex = static_cast<int>(chargeBT.triplePhase);
    ImGui::Text(U8("Triple Charge Active: %s"), chargeBT.tripleChargeActive ? U8("ON") : U8("OFF"));
    ImGui::Text(U8("Triple Charge Phase: %s"), triplePhaseNames[std::clamp(triplePhaseIndex, 0, 5)]);
    ImGui::Text(U8("Triple Charge Index: %d"), chargeBT.tripleChargeIndex);
    for (int i = 0; i < 3; ++i)
        ImGui::Text(U8("Charge %d Direction: (%.3f, %.3f, %.3f)"), i + 1,
            chargeBT.tripleChargeDirections[i].x, chargeBT.tripleChargeDirections[i].y, chargeBT.tripleChargeDirections[i].z);
    const int currentIndex = std::clamp(chargeBT.tripleChargeIndex, 0, 2);
    ImGui::Text(U8("Current Charge Direction: (%.3f, %.3f, %.3f)"),
        chargeBT.tripleChargeDirections[currentIndex].x, chargeBT.tripleChargeDirections[currentIndex].y, chargeBT.tripleChargeDirections[currentIndex].z);
    ImGui::Text(U8("Current Leg Start Position: (%.3f, %.3f, %.3f)"),
        chargeBT.tripleChargeStartPositions[currentIndex].x, chargeBT.tripleChargeStartPositions[currentIndex].y, chargeBT.tripleChargeStartPositions[currentIndex].z);
    ImGui::Text(U8("Current Leg Distance: %.3f m"), chargeBT.tripleChargeLegDistance);
    ImGui::Text(U8("Current Leg Elapsed: %.3f sec"), chargeBT.tripleChargeLegElapsed);
    ImGui::Text(U8("Triple Charge Transition Elapsed: %.3f sec"), chargeBT.tripleChargeTransitionElapsed);
    ImGui::Text(U8("Current Wall Clearance: %.3f m"), chargeBT.tripleCurrentWallClearance);
    ImGui::Text(U8("Wall Turn Candidate: %s"), chargeBT.tripleWallTurnCandidate ? U8("true") : U8("false"));
    ImGui::Text(U8("Wall Turn Triggered: %s"), chargeBT.tripleWallTurnTriggered ? U8("true") : U8("false"));
    ImGui::Text(U8("Triple Early WallHit: %s"), chargeBT.tripleEarlyWallHit ? U8("true") : U8("false"));
    ImGui::Text(U8("Triple Final WallHit: %s"), chargeBT.tripleFinalWallHit ? U8("true") : U8("false"));
    ImGui::Text(U8("Active Charge Stun Duration: %.3f sec"), chargeBT.activeStunDuration);
#endif
}
