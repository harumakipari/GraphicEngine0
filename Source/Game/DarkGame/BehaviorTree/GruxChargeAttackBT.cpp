#include "pch.h"
#include "GruxChargeAttackBT.h"
#include "NodeBase.h"
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

void GruxEnemy::RecordExecuteChargeAttackRunDebug()
{
    ++debugExecuteChargeAttackRunCount;
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
    owner->RecordExecuteChargeAttackRunDebug();
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
    debugChargeExecutionNotAllowedReason = "None";
    if (IsDead()) debugChargeExecutionNotAllowedReason = "Dead";
    else if (!battleAIActive) debugChargeExecutionNotAllowedReason = "BattleAIInactive";
    else if (finalHitReactionActive) debugChargeExecutionNotAllowedReason = "FinalHitReactionActive";
    else if (finalHitReactionHeld) debugChargeExecutionNotAllowedReason = "FinalHitReactionHeld";
    else if (IsAnimationEditorPreviewActive()) debugChargeExecutionNotAllowedReason = "AnimationEditorPreview";
    else if (stateMachine_ && (!std::strcmp(stateMachine_->GetStateName(), "EnemyDeathState") ||
        !std::strcmp(stateMachine_->GetStateName(), "EnemyStunState")))
        debugChargeExecutionNotAllowedReason = stateMachine_->GetStateName();
    return debugChargeExecutionNotAllowedReason == "None";
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
    debugShouldAbortChargeBT = false;
    debugAbortBehaviorTreeDisabled = false;
    debugAbortExecutionNotAllowed = false;
    debugAbortActiveNodeNull = false;
    debugAbortStateMismatch = false;
    debugChargeExecutionNotAllowedReason = "None";
    debugChargeBTAbortDetected = false;
    debugCleanupChargeAttackBTCalled = false;
    debugFailChargeAttackBTCalled = false;
    debugFinishChargeRecoveryBTCalled = false;
    debugLastChargeAbortReason = "None";
    debugUpdateBehaviorTreeCallCount = 0;
    debugExecuteChargeAttackRunCount = 0;
    debugUpdateTripleChargeBTCallCount = 0;
    debugTelegraphHoldUpdateCount = 0;
    debugTelegraphHoldLastDeltaTime = 0.0f;
    debugTelegraphEntryActiveNode = "None";
    debugTelegraphEntryInitialState = "None";
    debugTelegraphEntryCurrentState = "None";
    debugTelegraphEntryBTLastResult = "None";
    debugTelegraphEntryUpdateBTCount = 0;
    debugTelegraphEntryTripleUpdateCount = 0;
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
    debugFailChargeAttackBTCalled = true;
    debugLastChargeAbortReason = "FailChargeAttackBT";
    HideTripleChargeTelegraph();
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
    ++chargeBT.tripleChargeRuntimeInitializeCount;
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
    chargeBT.tripleBeginTripleChargeLegCalled = true;
    chargeBT.tripleBeginTripleChargeLegResult = false;
    chargeBT.tripleBeginChargeMovementResult = false;
    HideTripleChargeTelegraph();
    const bool movementStarted = BeginChargeAttackMovement();
    chargeBT.tripleBeginChargeMovementResult = movementStarted;
    chargeBT.tripleHoldEndChargeStartValidation = chargeStartValidationValidDebug;
    chargeBT.tripleHoldEndChargeStartClearance = chargeStartClearanceDebug;
    chargeBT.tripleHoldEndChargeStartFailureReason = chargeStartFailureReasonDebug;
    if (!movementStarted &&
        (chargeStartFailureReasonDebug == "InvalidTargetContext" ||
            chargeStartFailureReasonDebug == "DirectionLockFailed"))
    {
        chargeBT.tripleHoldEndChargeStartValidation = false;
        chargeBT.tripleHoldEndChargeStartClearance = 0.0f;
    }
    if (!movementStarted && chargeBT.tripleHoldEndChargeStartFailureReason == "None")
        chargeBT.tripleHoldEndChargeStartFailureReason = "Other";
    if (!movementStarted)
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
    chargeBT.tripleBeginTripleChargeLegResult = true;
    return true;
}

bool GruxEnemy::BeginTripleChargeTransition()
{
    if (chargeBT.tripleChargeIndex > 2)
        return false;
    StopChargeAttackMovement();
    PlayBodyAnimation(
        "TravelMode_Idle_0",
        true,
        true,
        0.15f,
        true,
        "GruxEnemy::BeginTripleChargeTransition");
    chargeBT.triplePhase = TripleChargePhase::InterChargeTransition;
    chargeBT.tripleChargeTransitionElapsed = 0.0f;
    chargeBT.tripleChargeRepositionActive = false;
    chargeBT.tripleChargeRepositionRequired = false;
    chargeBT.tripleChargeRepositionStartPosition = GetPosition();
    chargeBT.tripleChargeRepositionTargetPosition = chargeBT.tripleChargeRepositionStartPosition;
    chargeBT.tripleChargeRepositionCurrentPosition = chargeBT.tripleChargeRepositionStartPosition;
    chargeBT.tripleChargeRepositionDirection = {};
    chargeBT.tripleChargeRepositionRequiredClearance = 0.0f;
    chargeBT.tripleChargeRepositionCurrentClearance = 0.0f;
    chargeBT.tripleChargeRepositionShortage = 0.0f;
    chargeBT.tripleChargeRepositionPlannedDistance = 0.0f;
    chargeBT.tripleChargeRepositionMovedDistance = 0.0f;
    chargeBT.tripleChargeRepositionValidationBefore = false;
    chargeBT.tripleChargeRepositionValidationAfter = false;
    chargeBT.tripleChargeRepositionPlayerPositionBefore = {};
    chargeBT.tripleChargeRepositionPlayerPositionAfter = {};
    chargeBT.tripleChargeRepositionDirectionBefore = {};
    chargeBT.tripleChargeRepositionDirectionAfter = {};
    chargeBT.tripleChargeFinalLockedDirection = {};
    chargeBT.tripleChargeFinalValidationOrigin = {};
    chargeBT.tripleChargeFinalValidationDirection = {};
    chargeBT.tripleChargeFinalValidationClearance = 0.0f;
    chargeBT.tripleChargeFinalValidationResult = false;
    chargeBT.tripleChargeSideWallHitPosition = {};
    chargeBT.tripleChargeSideWallHitNormal = {};
    chargeBT.tripleChargeSideClearance = 0.0f;
    chargeBT.tripleChargeSideWallHit = false;
    chargeBT.tripleChargeSideWallActor = "None";
    chargeBT.tripleChargeSideWallComponent = "None";
    chargeBT.tripleChargeForwardCorrectionDistance = 0.0f;
    chargeBT.tripleChargeSideCorrectionDistance = 0.0f;
    chargeBT.tripleChargeFinalPathSafe = false;
    const auto transitionScene = GetOwnerScene();
    const auto transitionPlayer = transitionScene
        ? transitionScene->GetActorManager()->GetActorOfType<Player>() : nullptr;
    if (transitionPlayer)
        chargeBT.tripleChargeRepositionPlayerPositionBefore = transitionPlayer->GetPosition();
    const auto transitionContext = BuildTargetContext();
    if (transitionContext.valid)
        chargeBT.tripleChargeRepositionDirectionBefore = transitionContext.directionToPlayer;
    chargeBT.tripleWallTurnTriggered = false;
    SetChargePhaseDebug("TripleTransition");
    return true;
}

void GruxEnemy::HideTripleChargeTelegraph()
{
    if (tripleChargeTelegraphMeshComponent)
        tripleChargeTelegraphMeshComponent->SetIsVisible(false);
    chargeBT.tripleChargeTelegraphVisible = false;
}

bool GruxEnemy::BeginTripleChargeTelegraph()
{
    if (!chargeBT.tripleChargeActive)
        return false;
    ++chargeBT.tripleChargeTelegraphCallCount;

    debugTelegraphEntryActiveNode = activeNode ? activeNode->GetName() : "None";
    debugTelegraphEntryInitialState = chargeBT.initialStateName;
    debugTelegraphEntryCurrentState = stateMachine_ ? stateMachine_->GetStateName() : "None";
    debugTelegraphEntryBTLastResult = behaviorTreeLastResult;
    debugTelegraphEntryUpdateBTCount = debugUpdateBehaviorTreeCallCount;
    debugTelegraphEntryTripleUpdateCount = debugUpdateTripleChargeBTCallCount;

    const int index = std::clamp(chargeBT.tripleChargeIndex, 0, 2);
    const DirectX::XMFLOAT3 direction = chargeDirection;
    const float directionLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (directionLength <= FLT_EPSILON)
        return false;

    const DirectX::XMFLOAT3 normalizedDirection{
        direction.x / directionLength, 0.0f, direction.z / directionLength };
    const float wallRadius = (std::max)(0.05f, radius * chargeWallCastRadiusScale);
    const float playerRadius = (std::max)(0.05f, radius * chargePlayerCastRadiusScale);
    const float castDistance = (std::max)(0.1f, tripleChargeTelegraphMaxDistance);
    const DirectX::XMFLOAT3 startPosition{
        GetPosition().x + normalizedDirection.x * tripleChargeTelegraphForwardOffset,
        tripleChargeTelegraphFixedWorldY,
        GetPosition().z + normalizedDirection.z * tripleChargeTelegraphForwardOffset };
    DirectX::XMFLOAT3 castOrigin = GetPosition();
    castOrigin.x = startPosition.x;
    castOrigin.z = startPosition.z;
    castOrigin.y += (std::max)((std::max)(playerRadius, wallRadius) + 0.05f, height * 0.5f);

    // Telegraph-only query. Gameplay Wall SphereCast remains in
    // UpdateChargeAttackMovement() with its original mask and conditions.
    const uint32_t telegraphWallMask = CollisionHelper::ToBit(CollisionLayer::WorldStatic);
    HitResultWithActor wallHit{};
    const bool rawWallHit = Physics::Instance().SphereCast(
        castOrigin, normalizedDirection, castDistance, wallRadius, wallHit, telegraphWallMask);

    const float worldStaticDistance = rawWallHit ? (std::max)(0.0f, wallHit.distance) : 0.0f;
    const float boundaryDistance = [&]()
    {
        constexpr float epsilon = 1.0e-5f;
        float result = FLT_MAX;
        const auto consider = [&result](const float distance)
        {
            if (distance >= 0.0f && distance < result)
                result = distance;
        };
        if (normalizedDirection.x > epsilon)
            consider((bossRoomMaxX - startPosition.x) / normalizedDirection.x);
        else if (normalizedDirection.x < -epsilon)
            consider((bossRoomMinX - startPosition.x) / normalizedDirection.x);
        if (normalizedDirection.z > epsilon)
            consider((bossRoomMaxZ - startPosition.z) / normalizedDirection.z);
        else if (normalizedDirection.z < -epsilon)
            consider((bossRoomMinZ - startPosition.z) / normalizedDirection.z);
        return result == FLT_MAX ? 0.0f : result;
    }();
    chargeBT.tripleChargeTelegraphWorldStaticDistance = worldStaticDistance;
    chargeBT.tripleChargeTelegraphBoundaryDistance = boundaryDistance;

    float telegraphLength = castDistance;
    if (rawWallHit)
    {
        chargeBT.tripleChargeTelegraphWallHit = true;
        chargeBT.tripleChargeTelegraphWallDistance = worldStaticDistance;
        telegraphLength = worldStaticDistance;
        chargeBT.tripleChargeTelegraphLengthSource = "WorldStatic";
    }
    else if (boundaryDistance > 0.0f)
    {
        chargeBT.tripleChargeTelegraphWallHit = false;
        chargeBT.tripleChargeTelegraphWallDistance = 0.0f;
        telegraphLength = boundaryDistance;
        chargeBT.tripleChargeTelegraphLengthSource = "BossRoomBoundary";
    }
    else
    {
        chargeBT.tripleChargeTelegraphWallHit = false;
        chargeBT.tripleChargeTelegraphWallDistance = 0.0f;
        chargeBT.tripleChargeTelegraphLengthSource = "MaxDistanceFallback";
    }
    telegraphLength = (std::max)(0.1f, telegraphLength);

    const float telegraphWidth = playerRadius * 2.0f *
        (std::max)(0.0f, tripleChargeTelegraphWidthMultiplier);
    chargeBT.tripleChargeTelegraphDirection = normalizedDirection;
    chargeBT.tripleChargeTelegraphStartPosition = startPosition;
    chargeBT.tripleChargeTelegraphLength = telegraphLength;
    chargeBT.tripleChargeTelegraphWidth = telegraphWidth;
    chargeBT.tripleChargeTelegraphVisible = showTripleChargeTelegraph &&
        tripleChargeTelegraphMeshComponent != nullptr;

    // Diagnostic snapshot only. Gameplay cast conditions and length calculation
    // remain unchanged; retain the last three generation results for comparison.
    const size_t snapshotIndex = static_cast<size_t>(
        chargeBT.tripleChargeTelegraphSnapshotWriteIndex % chargeBT.tripleChargeTelegraphSnapshots.size());
    auto& snapshot = chargeBT.tripleChargeTelegraphSnapshots[snapshotIndex];
    snapshot = {};
    snapshot.generation = ++chargeBT.tripleChargeTelegraphSnapshotCount;
    snapshot.chargeIndex = index;
    snapshot.direction = normalizedDirection;
    snapshot.directionLocked = chargeDirectionLocked;
    snapshot.castOrigin = castOrigin;
    snapshot.castEnd = {
        castOrigin.x + normalizedDirection.x * castDistance,
        castOrigin.y + normalizedDirection.y * castDistance,
        castOrigin.z + normalizedDirection.z * castDistance };
    snapshot.startPosition = startPosition;
    snapshot.gruxPosition = GetPosition();
    snapshot.gruxScale = GetScale();
    snapshot.rawWallHit = rawWallHit;
    snapshot.wallHit = rawWallHit;
    // Preserve the raw query distance even when the existing Candidate filter
    // rejects the surface; this is diagnostic data only.
    snapshot.wallDistance = rawWallHit ? (std::max)(0.0f, wallHit.distance) : 0.0f;
    snapshot.worldStaticDistance = worldStaticDistance;
    snapshot.boundaryDistance = boundaryDistance;
    snapshot.lengthSource = chargeBT.tripleChargeTelegraphLengthSource;
    snapshot.length = telegraphLength;
    snapshot.wallCastRadius = wallRadius;
    snapshot.playerCastRadius = playerRadius;
    snapshot.wallTurnClearance = chargeBT.tripleChargeWallTurnClearance;
    snapshot.maxDistance = tripleChargeTelegraphMaxDistance;
    snapshot.capsuleRadius = radius;
    snapshot.phase2Scale = enemyScale;
    snapshot.phase2TransformProgress = phaseTransformProgress;
    snapshot.bossInFinalPhase = IsPhase2ChargeActive();
    if (rawWallHit)
    {
        snapshot.hitPosition = wallHit.hitPoint;
        snapshot.hitNormal = wallHit.normal;
        if (wallHit.actor)
            snapshot.wallHitActor = wallHit.actor->GetName();
        if (wallHit.component)
        {
            snapshot.wallHitComponent = wallHit.component->GetName();
            snapshot.wallHitLayer = wallHit.component->GetCollisionLayer();
        }
    }
    ++chargeBT.tripleChargeTelegraphSnapshotWriteIndex;

    if (showTripleChargeTelegraph)
    {
        const DirectX::XMFLOAT3 castEnd{
            castOrigin.x + normalizedDirection.x * castDistance,
            castOrigin.y + normalizedDirection.y * castDistance,
            castOrigin.z + normalizedDirection.z * castDistance };
        const DirectX::XMFLOAT4 castColor{ 1.0f, 0.75f, 0.10f, 1.0f };
        DebugRender::DrawLine(castOrigin, castEnd, castColor, 0.0f, true);
        DebugRender::DrawSphere(castOrigin, wallRadius, castColor, 0.0f, true);
        DebugRender::DrawSphere(castEnd, wallRadius, castColor, 0.0f, true);
        if (rawWallHit)
        {
            DebugRender::DrawSphere(wallHit.hitPoint, 0.12f,
                { 1.0f, 0.15f, 0.05f, 1.0f }, 0.0f, true);
            const DirectX::XMFLOAT3 normalEnd{
                wallHit.hitPoint.x + wallHit.normal.x,
                wallHit.hitPoint.y + wallHit.normal.y,
                wallHit.hitPoint.z + wallHit.normal.z };
            DebugRender::DrawLine(wallHit.hitPoint, normalEnd,
                { 1.0f, 0.15f, 0.05f, 1.0f }, 0.0f, true);
        }
    }

    if (tripleChargeTelegraphMeshComponent)
    {
        const DirectX::XMFLOAT3 endPosition{
            startPosition.x + normalizedDirection.x * telegraphLength,
            startPosition.y,
            startPosition.z + normalizedDirection.z * telegraphLength };
        const DirectX::XMFLOAT3 centerPosition{
            (startPosition.x + endPosition.x) * 0.5f,
            startPosition.y,
            (startPosition.z + endPosition.z) * 0.5f };
        tripleChargeTelegraphMeshComponent->SetWorldLocationDirect(startPosition);
        const float yaw = std::atan2(-normalizedDirection.x, -normalizedDirection.z);
        const auto rotation = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f);
        DirectX::XMFLOAT4 rotationFloat{};
        DirectX::XMStoreFloat4(&rotationFloat, rotation);
        const DirectX::XMFLOAT3 rootScale = GetScale();
        const DirectX::XMFLOAT3 requestedScale{
            telegraphWidth / (std::abs(rootScale.x) > 0.001f ? rootScale.x : 1.0f),
            1.0f,
            telegraphLength / (std::abs(rootScale.z) > 0.001f ? rootScale.z : 1.0f) };
        tripleChargeTelegraphMeshComponent->SetWorldRotationDirect(rotationFloat);
        tripleChargeTelegraphMeshComponent->SetRelativeScaleDirect(requestedScale);

        snapshot.requestedPosition = startPosition;
        snapshot.requestedRotation = rotationFloat;
        snapshot.requestedYaw = yaw;
        snapshot.requestedScale = requestedScale;
        snapshot.relativePosition = tripleChargeTelegraphMeshComponent->GetRelativeLocation();
        snapshot.relativeScale = tripleChargeTelegraphMeshComponent->GetRelativeScale();
        snapshot.componentWorldPosition = tripleChargeTelegraphMeshComponent->GetComponentLocation();
        snapshot.componentWorldScale = tripleChargeTelegraphMeshComponent->GetComponentScale();
        snapshot.componentWorldMatrix =
            tripleChargeTelegraphMeshComponent->GetComponentWorldTransform().ToWorldTransform();
        const auto worldMatrix = tripleChargeTelegraphMeshComponent->GetComponentWorldTransform().ToWorldTransform();
        const auto transformPoint = [&worldMatrix](const DirectX::XMFLOAT3& point)
        {
            DirectX::XMFLOAT3 result{};
            DirectX::XMStoreFloat3(&result,
                DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point),
                    DirectX::XMLoadFloat4x4(&worldMatrix)));
            return result;
        };
        snapshot.actualMeshStart = transformPoint({ 0.0f, 0.0f, 0.0f });
        snapshot.actualMeshEnd = transformPoint({ 0.0f, 0.0f, -1.0f });
        if (showTripleChargeTelegraph)
        {
            const DirectX::XMFLOAT4 meshColor{ 0.25f, 1.0f, 0.35f, 1.0f };
            DebugRender::DrawLine(snapshot.actualMeshStart, snapshot.actualMeshEnd,
                meshColor, 0.0f, true);
            DebugRender::DrawSphere(snapshot.actualMeshStart, 0.10f, meshColor, 0.0f, true);
            DebugRender::DrawSphere(snapshot.actualMeshEnd, 0.10f, meshColor, 0.0f, true);
        }
        tripleChargeTelegraphMeshComponent->SetIsVisible(chargeBT.tripleChargeTelegraphVisible);
        (void)centerPosition; // The asset pivot is the start edge, not the center.
    }
    // Keep the boss-side edge fixed and reveal the plane towards its end.
    UpdateTripleChargeTelegraphTransform(0.0f);
    return true;
}

void GruxEnemy::UpdateTripleChargeTelegraphTransform(float lengthProgress)
{
    if (!tripleChargeTelegraphMeshComponent)
        return;
    const float progress = std::clamp(lengthProgress, 0.0f, 1.0f);
    const DirectX::XMFLOAT3 direction = chargeBT.tripleChargeTelegraphDirection;
    const float directionLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (directionLength <= FLT_EPSILON)
        return;
    const float yaw = std::atan2(-direction.x, -direction.z);
    const auto rotation = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f);
    DirectX::XMFLOAT4 rotationFloat{};
    DirectX::XMStoreFloat4(&rotationFloat, rotation);
    const DirectX::XMFLOAT3 rootScale = GetScale();
    const float width = chargeBT.tripleChargeTelegraphWidth;
    const float length = chargeBT.tripleChargeTelegraphLength * progress;
    const DirectX::XMFLOAT3 requestedScale{
        width / (std::abs(rootScale.x) > 0.001f ? rootScale.x : 1.0f),
        1.0f,
        length / (std::abs(rootScale.z) > 0.001f ? rootScale.z : 1.0f) };
    // ChargeTelegraphPlane2's local -Z axis starts at the local origin;
    // changing only Z scale therefore preserves the world-space start edge.
    tripleChargeTelegraphMeshComponent->SetWorldLocationDirect(
        chargeBT.tripleChargeTelegraphStartPosition);
    tripleChargeTelegraphMeshComponent->SetWorldRotationDirect(rotationFloat);
    tripleChargeTelegraphMeshComponent->SetRelativeScaleDirect(requestedScale);
}

void GruxEnemy::DrawTripleChargeTelegraphDebug() const
{
    if (!chargeBT.tripleChargeTelegraphVisible)
        return;
    const auto& start = chargeBT.tripleChargeTelegraphStartPosition;
    const auto& direction = chargeBT.tripleChargeTelegraphDirection;
    const float length = chargeBT.tripleChargeTelegraphLength;
    const float halfWidth = chargeBT.tripleChargeTelegraphWidth * 0.5f;
    const DirectX::XMFLOAT3 end{
        start.x + direction.x * length, start.y + 0.01f,
        start.z + direction.z * length };
    const DirectX::XMFLOAT3 debugStart{ start.x, start.y + 0.01f, start.z };
    const DirectX::XMFLOAT3 lateral{ -direction.z * halfWidth, 0.0f, direction.x * halfWidth };
    const DirectX::XMFLOAT3 leftStart{ debugStart.x + lateral.x, debugStart.y, debugStart.z + lateral.z };
    const DirectX::XMFLOAT3 rightStart{ debugStart.x - lateral.x, debugStart.y, debugStart.z - lateral.z };
    const DirectX::XMFLOAT3 leftEnd{ end.x + lateral.x, end.y, end.z + lateral.z };
    const DirectX::XMFLOAT3 rightEnd{ end.x - lateral.x, end.y, end.z - lateral.z };
    const DirectX::XMFLOAT4 color{ 1.0f, 0.25f, 0.05f, 1.0f };
    DebugRender::DrawLine(debugStart, end, color, 0.0f, true);
    DebugRender::DrawLine(leftStart, leftEnd, color, 0.0f, true);
    DebugRender::DrawLine(rightStart, rightEnd, color, 0.0f, true);
    DebugRender::DrawSphere(debugStart, 0.08f, color, 0.0f, true);
    DebugRender::DrawSphere(end, 0.08f, color, 0.0f, true);
}

GruxEnemy::ChargeBTStepResult GruxEnemy::UpdateChargeAttackBT(float dt)
{
    return chargeBT.tripleChargeActive ? UpdateTripleChargeBT(dt) : UpdateSingleChargeBT(dt);
}

GruxEnemy::ChargeBTStepResult GruxEnemy::UpdateTripleChargeBT(float dt)
{
    ++debugUpdateTripleChargeBTCallCount;
    chargeBT.triplePhaseAtUpdateEntry = chargeBT.triplePhase;
    chargeBT.triplePhaseAtUpdateExit = chargeBT.triplePhase;
    if (!IsChargeAttackBTActive()) return ChargeBTStepResult::Failed;
    if (chargeBT.phase == ChargeBTPhase::Result) return ChargeBTStepResult::Complete;
    const auto controller = GetBodyAnimationController();
    if (!controller || !characterMovementComponent || !rotationComponent)
    {
        FailChargeAttackBT();
        chargeBT.triplePhase = TripleChargePhase::Aborted;
        return ChargeBTStepResult::Complete;
    }

    if (chargeBT.phase == ChargeBTPhase::Telegraph &&
        chargeBT.triplePhase == TripleChargePhase::InitialWindup)
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
            if (!BeginTripleChargeTelegraph())
            {
                FailChargeAttackBT();
                chargeBT.triplePhase = TripleChargePhase::Aborted;
                return ChargeBTStepResult::Complete;
            }
            chargeBT.triplePhase = TripleChargePhase::TelegraphHold;
            chargeBT.triplePhaseAtUpdateExit = chargeBT.triplePhase;
            ++chargeBT.triplePhaseChangeCount;
            chargeBT.tripleChargeTelegraphElapsed = 0.0f;
            SetChargePhaseDebug("TripleTelegraphHold");
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

        // Before locking the next leg, ensure the current position has enough
        // forward clearance.  This is deliberately limited to legs 2/3;
        // the existing BeginChargeAttackMovement validation remains authoritative.
        const float directionLength = std::sqrt(
            context.directionToPlayer.x * context.directionToPlayer.x +
            context.directionToPlayer.z * context.directionToPlayer.z);
        if (directionLength > 0.0001f)
        {
            const DirectX::XMFLOAT3 nextDirection{
                context.directionToPlayer.x / directionLength, 0.0f,
                context.directionToPlayer.z / directionLength };
            float clearance = 0.0f;
            bool wallHit = false;
            const DirectX::XMFLOAT3 currentPosition = GetPosition();
            const bool clearanceMeasured = EvaluateChargeStartClearance(
                currentPosition, nextDirection, clearance, wallHit);
            const float requiredClearance = chargeStartValidationClearance +
                tripleChargeRepositionSafetyMargin;
            float sideClearance = 0.0f;
            DirectX::XMFLOAT3 sideNormal{};
            DirectX::XMFLOAT3 sideHitPosition{};
            std::string sideActor;
            std::string sideComponent;
            bool sideWallHit = false;
            EvaluateChargeSideClearance(currentPosition, nextDirection,
                sideClearance, sideNormal, sideHitPosition, sideActor,
                sideComponent, sideWallHit);
            chargeBT.tripleChargeRepositionCurrentPosition = currentPosition;
            chargeBT.tripleChargeRepositionCurrentClearance = clearance;
            chargeBT.tripleChargeRepositionRequiredClearance = requiredClearance;
            chargeBT.tripleChargeRepositionValidationBefore = clearanceMeasured &&
                clearance > chargeStartValidationClearance;
            chargeBT.tripleChargeRepositionShortage = wallHit
                ? (std::max)(0.0f, requiredClearance - clearance) : 0.0f;
            const float requiredSideClearance = (std::max)(0.05f,
                radius * chargeWallCastRadiusScale) + chargeWallCastSafetyMargin +
                tripleChargeRepositionSideSafetyMargin;
            const float sideShortage = sideWallHit
                ? (std::max)(0.0f, requiredSideClearance - sideClearance) : 0.0f;
            chargeBT.tripleChargeSideClearance = sideClearance;
            chargeBT.tripleChargeSideWallHit = sideWallHit;
            chargeBT.tripleChargeSideWallHitPosition = sideHitPosition;
            chargeBT.tripleChargeSideWallHitNormal = sideNormal;
            chargeBT.tripleChargeSideWallActor = sideActor;
            chargeBT.tripleChargeSideWallComponent = sideComponent;
            chargeBT.tripleChargeForwardCorrectionDistance = chargeBT.tripleChargeRepositionShortage;
            chargeBT.tripleChargeSideCorrectionDistance = sideShortage;
            if (showChargeCastDebug)
            {
                const DirectX::XMFLOAT4 repositionColor{ 0.75f, 0.20f, 1.0f, 1.0f };
                DebugRender::DrawLine(chargeBT.tripleChargeRepositionStartPosition,
                    chargeBT.tripleChargeRepositionTargetPosition, repositionColor, 0.0f, true);
                DebugRender::DrawSphere(chargeBT.tripleChargeRepositionTargetPosition,
                    0.10f, repositionColor, 0.0f, true);
                if (sideWallHit)
                {
                    DebugRender::DrawSphere(sideHitPosition, 0.12f,
                        { 1.0f, 0.05f, 0.80f, 1.0f }, 0.0f, true);
                    DebugRender::DrawLine(sideHitPosition,
                        { sideHitPosition.x + sideNormal.x,
                            sideHitPosition.y + sideNormal.y,
                            sideHitPosition.z + sideNormal.z },
                        { 1.0f, 0.05f, 0.80f, 1.0f }, 0.0f, true);
                }
            }

            if (clearanceMeasured && !chargeBT.tripleChargeRepositionActive &&
                (chargeBT.tripleChargeRepositionShortage > 0.0f || sideShortage > 0.0f))
            {
                DirectX::XMFLOAT3 inwardNormal = sideNormal;
                const DirectX::XMFLOAT3 roomCenter{
                    (bossRoomMinX + bossRoomMaxX) * 0.5f, currentPosition.y,
                    (bossRoomMinZ + bossRoomMaxZ) * 0.5f };
                const float toCenterX = roomCenter.x - currentPosition.x;
                const float toCenterZ = roomCenter.z - currentPosition.z;
                if (inwardNormal.x * toCenterX + inwardNormal.z * toCenterZ < 0.0f)
                {
                    inwardNormal.x = -inwardNormal.x;
                    inwardNormal.z = -inwardNormal.z;
                }
                DirectX::XMFLOAT3 correction{
                    -nextDirection.x * chargeBT.tripleChargeRepositionShortage +
                        inwardNormal.x * sideShortage,
                    0.0f,
                    -nextDirection.z * chargeBT.tripleChargeRepositionShortage +
                        inwardNormal.z * sideShortage };
                const float correctionLength = std::sqrt(
                    correction.x * correction.x + correction.z * correction.z);
                const float plannedDistance = (std::min)(correctionLength,
                    tripleChargeRepositionMaxDistance);
                const float wallRadius = (std::max)(0.05f,
                    radius * chargeWallCastRadiusScale);
                const float boundaryMargin = (std::max)(
                    bossRoomSafetyMargin,
                    wallRadius + chargeWallCastSafetyMargin);
                chargeBT.tripleChargeRepositionRequired = true;
                chargeBT.tripleChargeRepositionActive = plannedDistance > 0.0001f;
                chargeBT.tripleChargeRepositionStartPosition = currentPosition;
                chargeBT.tripleChargeRepositionDirection = correctionLength > 0.0001f
                    ? DirectX::XMFLOAT3{ correction.x / correctionLength, 0.0f, correction.z / correctionLength }
                    : DirectX::XMFLOAT3{};
                chargeBT.tripleChargeRepositionPlannedDistance = plannedDistance;
                chargeBT.tripleChargeRepositionTargetPosition = currentPosition;
                chargeBT.tripleChargeRepositionTargetPosition.x +=
                    chargeBT.tripleChargeRepositionDirection.x * plannedDistance;
                chargeBT.tripleChargeRepositionTargetPosition.z +=
                    chargeBT.tripleChargeRepositionDirection.z * plannedDistance;
                chargeBT.tripleChargeRepositionTargetPosition.x = std::clamp(
                    chargeBT.tripleChargeRepositionTargetPosition.x,
                    bossRoomMinX + boundaryMargin, bossRoomMaxX - boundaryMargin);
                chargeBT.tripleChargeRepositionTargetPosition.z = std::clamp(
                    chargeBT.tripleChargeRepositionTargetPosition.z,
                    bossRoomMinZ + boundaryMargin, bossRoomMaxZ - boundaryMargin);
            }

            if (chargeBT.tripleChargeRepositionActive)
            {
                const DirectX::XMFLOAT3 beforeMove = GetPosition();
                const DirectX::XMFLOAT3 target = chargeBT.tripleChargeRepositionTargetPosition;
                const float dx = target.x - beforeMove.x;
                const float dz = target.z - beforeMove.z;
                const float remaining = std::sqrt(dx * dx + dz * dz);
                const float step = tripleChargeRepositionSpeed * (std::max)(0.0f, dt);
                DirectX::XMFLOAT3 moved = beforeMove;
                if (remaining <= step || remaining <= 0.0001f)
                {
                    moved = target;
                    chargeBT.tripleChargeRepositionActive = false;
                }
                else
                {
                    moved.x += dx / remaining * step;
                    moved.z += dz / remaining * step;
                }
                SetPosition(moved);
                chargeBT.tripleChargeRepositionCurrentPosition = moved;
                const float mdx = moved.x - chargeBT.tripleChargeRepositionStartPosition.x;
                const float mdz = moved.z - chargeBT.tripleChargeRepositionStartPosition.z;
                chargeBT.tripleChargeRepositionMovedDistance = std::sqrt(mdx * mdx + mdz * mdz);
                bool ignoredWallHit = false;
                EvaluateChargeStartClearance(moved, nextDirection,
                    chargeBT.tripleChargeRepositionCurrentClearance, ignoredWallHit);
                chargeBT.tripleChargeRepositionValidationAfter =
                    chargeBT.tripleChargeRepositionCurrentClearance > chargeStartValidationClearance;
            }
        }
        if (chargeBT.tripleChargeTransitionElapsed < chargeBT.tripleChargeTransitionDuration)
            return ChargeBTStepResult::Running;
        // Rebuild all target-dependent state after the final repositioned
        // position has been applied. The old pre-reposition direction is
        // never reused for the next leg.
        const auto finalContext = BuildTargetContext();
        if (!finalContext.valid)
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        chargeBT.tripleChargeRepositionCurrentPosition = GetPosition();
        chargeBT.tripleChargeRepositionDirectionAfter = finalContext.directionToPlayer;
        const auto finalScene = GetOwnerScene();
        const auto finalPlayer = finalScene
            ? finalScene->GetActorManager()->GetActorOfType<Player>() : nullptr;
        if (finalPlayer)
            chargeBT.tripleChargeRepositionPlayerPositionAfter = finalPlayer->GetPosition();
        chargeDirectionLocked = false;
        chargeDirection = {};
        if (!LockChargeDirectionToPlayer())
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        chargeBT.tripleChargeFinalLockedDirection = chargeDirection;
        chargeBT.tripleChargeFinalValidationOrigin = GetPosition();
        chargeBT.tripleChargeFinalValidationDirection = chargeDirection;
        bool finalValidationHit = false;
        const bool finalValidationMeasured = EvaluateChargeStartClearance(
            chargeBT.tripleChargeFinalValidationOrigin,
            chargeBT.tripleChargeFinalValidationDirection,
            chargeBT.tripleChargeFinalValidationClearance,
            finalValidationHit);
        chargeBT.tripleChargeFinalValidationResult = finalValidationMeasured &&
            (!finalValidationHit || chargeBT.tripleChargeFinalValidationClearance > chargeStartValidationClearance);
        float finalSideClearance = 0.0f;
        DirectX::XMFLOAT3 finalSideNormal{};
        DirectX::XMFLOAT3 finalSideHitPosition{};
        std::string finalSideActor;
        std::string finalSideComponent;
        bool finalSideHit = false;
        EvaluateChargeSideClearance(chargeBT.tripleChargeFinalValidationOrigin,
            chargeBT.tripleChargeFinalValidationDirection, finalSideClearance,
            finalSideNormal, finalSideHitPosition, finalSideActor,
            finalSideComponent, finalSideHit);
        chargeBT.tripleChargeSideClearance = finalSideClearance;
        chargeBT.tripleChargeSideWallHit = finalSideHit;
        chargeBT.tripleChargeSideWallHitPosition = finalSideHitPosition;
        chargeBT.tripleChargeSideWallHitNormal = finalSideNormal;
        chargeBT.tripleChargeSideWallActor = finalSideActor;
        chargeBT.tripleChargeSideWallComponent = finalSideComponent;
        const float finalRequiredSideClearance = (std::max)(0.05f,
            radius * chargeWallCastRadiusScale) + chargeWallCastSafetyMargin +
            tripleChargeRepositionSideSafetyMargin;
        chargeBT.tripleChargeFinalPathSafe = chargeBT.tripleChargeFinalValidationResult &&
            (!finalSideHit || finalSideClearance > finalRequiredSideClearance);
        if (!BeginTripleChargeTelegraph())
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        chargeBT.triplePhase = TripleChargePhase::TelegraphHold;
        chargeBT.triplePhaseAtUpdateExit = chargeBT.triplePhase;
        ++chargeBT.triplePhaseChangeCount;
        chargeBT.tripleChargeTelegraphElapsed = 0.0f;
        SetChargePhaseDebug("TripleTelegraphHold");
        return ChargeBTStepResult::Running;
    }

    if (chargeBT.triplePhase == TripleChargePhase::TelegraphHold)
    {
        ++debugTelegraphHoldUpdateCount;
        debugTelegraphHoldLastDeltaTime = dt;
        chargeBT.tripleChargeTelegraphElapsed += (std::max)(0.0f, dt);
        const float expandDuration = (std::max)(0.001f, tripleChargeTelegraphExpandDuration);
        const float expandProgress = std::clamp(
            chargeBT.tripleChargeTelegraphElapsed / expandDuration, 0.0f, 1.0f);
        // Ease-out cubic: quick initial extension, gentle settle at full length.
        const float easedProgress = 1.0f - std::pow(1.0f - expandProgress, 3.0f);
        UpdateTripleChargeTelegraphTransform(easedProgress);
        DrawTripleChargeTelegraphDebug();
        if (chargeBT.tripleChargeTelegraphElapsed < expandDuration +
            tripleChargeTelegraphHoldDuration)
            return ChargeBTStepResult::Running;
        if (!BeginTripleChargeLeg())
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
    HideTripleChargeTelegraph();
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
    HideTripleChargeTelegraph();
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
    debugFinishChargeRecoveryBTCalled = true;
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

bool GruxEnemy::ShouldAbortChargeAttackBT()
{
    debugAbortBehaviorTreeDisabled = !behaviorTreeFastComboEnabled;
    debugAbortExecutionNotAllowed = !IsChargeBTExecutionAllowed();
    debugAbortActiveNodeNull = !activeNode;
    debugAbortStateMismatch = stateMachine_ && stateMachine_->GetStateName() != chargeBT.initialStateName;
    debugShouldAbortChargeBT = IsChargeAttackBTActive() &&
        (debugAbortBehaviorTreeDisabled || debugAbortExecutionNotAllowed ||
            debugAbortActiveNodeNull || debugAbortStateMismatch);
    if (debugShouldAbortChargeBT)
    {
        debugChargeBTAbortDetected = true;
        debugLastChargeAbortReason = debugAbortBehaviorTreeDisabled ? "BehaviorTreeDisabled" :
            debugAbortExecutionNotAllowed ? "ExecutionNotAllowed" :
            debugAbortActiveNodeNull ? "ActiveNodeNull" : "StateMismatch";
    }
    return debugShouldAbortChargeBT;
}

void GruxEnemy::CleanupChargeAttackBT()
{
    debugCleanupChargeAttackBTCalled = true;
    if (debugLastChargeAbortReason == "None")
        debugLastChargeAbortReason = "CleanupChargeAttackBT";
    HideTripleChargeTelegraph();
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
    DrawTripleJumpDebug();
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
    ImGui::DragFloat("Triple Charge Reposition Max Distance", &tripleChargeRepositionMaxDistance, 0.05f, 0.0f, 2.0f, "%.2f m");
    ImGui::DragFloat("Triple Charge Reposition Safety Margin", &tripleChargeRepositionSafetyMargin, 0.01f, 0.0f, 1.0f, "%.2f m");
    ImGui::DragFloat("Triple Charge Reposition Speed", &tripleChargeRepositionSpeed, 0.05f, 0.1f, 10.0f, "%.2f m/s");
    ImGui::DragFloat("Triple Charge Reposition Side Safety Margin", &tripleChargeRepositionSideSafetyMargin, 0.01f, 0.0f, 1.0f, "%.2f m");
    ImGui::Checkbox("Show Triple Charge Telegraph", &showTripleChargeTelegraph);
    ImGui::DragFloat("Triple Charge Telegraph Expand Duration", &tripleChargeTelegraphExpandDuration, 0.01f, 0.0f, 1.0f, "%.2f sec");
    ImGui::DragFloat("Triple Charge Telegraph Hold Duration", &tripleChargeTelegraphHoldDuration, 0.01f, 0.0f, 1.0f, "%.2f sec");
    ImGui::DragFloat("Triple Charge Telegraph Ground Offset", &tripleChargeTelegraphGroundOffset, 0.005f, -0.1f, 0.5f, "%.3f m");
    ImGui::DragFloat("Triple Charge Telegraph Fixed World Y", &tripleChargeTelegraphFixedWorldY, 0.01f, -5.0f, 5.0f, "%.3f");
    ImGui::DragFloat("Triple Charge Telegraph Forward Offset", &tripleChargeTelegraphForwardOffset, 0.01f, -1.0f, 2.0f, "%.2f m");
    ImGui::DragFloat("Triple Charge Telegraph Max Distance", &tripleChargeTelegraphMaxDistance, 0.1f, 0.1f, 100.0f, "%.2f m");
    ImGui::DragFloat("Triple Charge Telegraph Width Multiplier", &tripleChargeTelegraphWidthMultiplier, 0.01f, 0.0f, 3.0f, "%.2fx");
    chargeBT.tripleChargeWallTurnClearance = (std::max)(0.0f, chargeBT.tripleChargeWallTurnClearance);
    chargeBT.tripleChargeLegMaxDistance = (std::max)(0.1f, chargeBT.tripleChargeLegMaxDistance);
    chargeBT.tripleChargeLegMaxDuration = (std::max)(0.1f, chargeBT.tripleChargeLegMaxDuration);
    chargeBT.tripleChargeTransitionDuration = (std::max)(0.0f, chargeBT.tripleChargeTransitionDuration);
    chargeBT.tripleWallStunDurationMultiplier = (std::max)(1.0f, chargeBT.tripleWallStunDurationMultiplier);
    tripleChargeRepositionMaxDistance = (std::max)(0.0f, tripleChargeRepositionMaxDistance);
    tripleChargeRepositionSafetyMargin = (std::max)(0.0f, tripleChargeRepositionSafetyMargin);
    tripleChargeRepositionSpeed = (std::max)(0.1f, tripleChargeRepositionSpeed);
    tripleChargeRepositionSideSafetyMargin = (std::max)(0.0f, tripleChargeRepositionSideSafetyMargin);
    tripleChargeTelegraphExpandDuration = std::clamp(tripleChargeTelegraphExpandDuration, 0.0f, 1.0f);
    tripleChargeTelegraphHoldDuration = std::clamp(tripleChargeTelegraphHoldDuration, 0.0f, 1.0f);
    tripleChargeTelegraphMaxDistance = (std::max)(0.1f, tripleChargeTelegraphMaxDistance);
    tripleChargeTelegraphWidthMultiplier = (std::max)(0.0f, tripleChargeTelegraphWidthMultiplier);

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

    const char* triplePhaseNames[] = { U8("なし"), U8("初回予兆"), U8("突進"), U8("切り返し"), U8("Telegraph Hold"), U8("完了"), U8("中断") };
    const int triplePhaseIndex = static_cast<int>(chargeBT.triplePhase);
    ImGui::Text(U8("Triple Charge Active: %s"), chargeBT.tripleChargeActive ? U8("ON") : U8("OFF"));
    ImGui::Text(U8("Triple Charge Phase: %s"), triplePhaseNames[std::clamp(triplePhaseIndex, 0, 6)]);
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
    ImGui::Text("Triple Charge Reposition Active: %s", chargeBT.tripleChargeRepositionActive ? "true" : "false");
    ImGui::Text("Triple Charge Reposition Required: %s", chargeBT.tripleChargeRepositionRequired ? "true" : "false");
    ImGui::Text("Reposition Start: (%.2f, %.2f, %.2f)", chargeBT.tripleChargeRepositionStartPosition.x, chargeBT.tripleChargeRepositionStartPosition.y, chargeBT.tripleChargeRepositionStartPosition.z);
    ImGui::Text("Reposition Target: (%.2f, %.2f, %.2f)", chargeBT.tripleChargeRepositionTargetPosition.x, chargeBT.tripleChargeRepositionTargetPosition.y, chargeBT.tripleChargeRepositionTargetPosition.z);
    ImGui::Text("Reposition Current: (%.2f, %.2f, %.2f)", chargeBT.tripleChargeRepositionCurrentPosition.x, chargeBT.tripleChargeRepositionCurrentPosition.y, chargeBT.tripleChargeRepositionCurrentPosition.z);
    ImGui::Text("Reposition Final Position: (%.2f, %.2f, %.2f)", chargeBT.tripleChargeRepositionCurrentPosition.x, chargeBT.tripleChargeRepositionCurrentPosition.y, chargeBT.tripleChargeRepositionCurrentPosition.z);
    ImGui::Text("Reposition Direction: (%.2f, %.2f, %.2f)", chargeBT.tripleChargeRepositionDirection.x, chargeBT.tripleChargeRepositionDirection.y, chargeBT.tripleChargeRepositionDirection.z);
    ImGui::Text("Reposition Clearance Required/Current: %.3f / %.3f", chargeBT.tripleChargeRepositionRequiredClearance, chargeBT.tripleChargeRepositionCurrentClearance);
    ImGui::Text("Reposition Shortage/Planned/Moved: %.3f / %.3f / %.3f", chargeBT.tripleChargeRepositionShortage, chargeBT.tripleChargeRepositionPlannedDistance, chargeBT.tripleChargeRepositionMovedDistance);
    ImGui::Text("Reposition Validation Before/After: %s / %s", chargeBT.tripleChargeRepositionValidationBefore ? "Valid" : "Invalid", chargeBT.tripleChargeRepositionValidationAfter ? "Valid" : "Invalid");
    ImGui::Text("Player Before Reposition: (%.2f, %.2f, %.2f)", chargeBT.tripleChargeRepositionPlayerPositionBefore.x, chargeBT.tripleChargeRepositionPlayerPositionBefore.y, chargeBT.tripleChargeRepositionPlayerPositionBefore.z);
    ImGui::Text("Player After Reposition: (%.2f, %.2f, %.2f)", chargeBT.tripleChargeRepositionPlayerPositionAfter.x, chargeBT.tripleChargeRepositionPlayerPositionAfter.y, chargeBT.tripleChargeRepositionPlayerPositionAfter.z);
    ImGui::Text("Direction Before/After: (%.2f, %.2f, %.2f) / (%.2f, %.2f, %.2f)",
        chargeBT.tripleChargeRepositionDirectionBefore.x, chargeBT.tripleChargeRepositionDirectionBefore.y, chargeBT.tripleChargeRepositionDirectionBefore.z,
        chargeBT.tripleChargeRepositionDirectionAfter.x, chargeBT.tripleChargeRepositionDirectionAfter.y, chargeBT.tripleChargeRepositionDirectionAfter.z);
    ImGui::Text("Final Locked Charge Direction: (%.3f, %.3f, %.3f)", chargeBT.tripleChargeFinalLockedDirection.x, chargeBT.tripleChargeFinalLockedDirection.y, chargeBT.tripleChargeFinalLockedDirection.z);
    ImGui::Text("Final Validation Origin: (%.3f, %.3f, %.3f)", chargeBT.tripleChargeFinalValidationOrigin.x, chargeBT.tripleChargeFinalValidationOrigin.y, chargeBT.tripleChargeFinalValidationOrigin.z);
    ImGui::Text("Final Validation Direction: (%.3f, %.3f, %.3f)", chargeBT.tripleChargeFinalValidationDirection.x, chargeBT.tripleChargeFinalValidationDirection.y, chargeBT.tripleChargeFinalValidationDirection.z);
    ImGui::Text("Final Validation Clearance: %.3f", chargeBT.tripleChargeFinalValidationClearance);
    ImGui::Text("Final Validation Result: %s", chargeBT.tripleChargeFinalValidationResult ? "Valid" : "Invalid");
    ImGui::Text("Final Charge Direction: (%.3f, %.3f, %.3f)", chargeBT.tripleChargeFinalLockedDirection.x, chargeBT.tripleChargeFinalLockedDirection.y, chargeBT.tripleChargeFinalLockedDirection.z);
    ImGui::Text("Forward Clearance: %.3f / Side Clearance: %.3f", chargeBT.tripleChargeFinalValidationClearance, chargeBT.tripleChargeSideClearance);
    ImGui::Text("Side Wall Hit: %s  Distance: %.3f", chargeBT.tripleChargeSideWallHit ? "true" : "false", chargeBT.tripleChargeSideClearance);
    ImGui::Text("Side Wall Normal: (%.3f, %.3f, %.3f)", chargeBT.tripleChargeSideWallHitNormal.x, chargeBT.tripleChargeSideWallHitNormal.y, chargeBT.tripleChargeSideWallHitNormal.z);
    ImGui::Text("Side Wall Actor/Component: %s / %s", chargeBT.tripleChargeSideWallActor.c_str(), chargeBT.tripleChargeSideWallComponent.c_str());
    ImGui::Text("Forward/Side Correction: %.3f / %.3f", chargeBT.tripleChargeForwardCorrectionDistance, chargeBT.tripleChargeSideCorrectionDistance);
    ImGui::Text("Final Path Safe: %s", chargeBT.tripleChargeFinalPathSafe ? "true" : "false");
    ImGui::Text("Telegraph Visible: %s", chargeBT.tripleChargeTelegraphVisible ? "true" : "false");
    ImGui::Text("Telegraph Direction: (%.3f, %.3f, %.3f)", chargeBT.tripleChargeTelegraphDirection.x,
        chargeBT.tripleChargeTelegraphDirection.y, chargeBT.tripleChargeTelegraphDirection.z);
    ImGui::Text("Telegraph Start Position: (%.3f, %.3f, %.3f)", chargeBT.tripleChargeTelegraphStartPosition.x,
        chargeBT.tripleChargeTelegraphStartPosition.y, chargeBT.tripleChargeTelegraphStartPosition.z);
    ImGui::Text("Grux World Y: %.3f", GetPosition().y);
    ImGui::Text("Telegraph Fixed World Y: %.3f", tripleChargeTelegraphFixedWorldY);
    ImGui::Text("Final Telegraph World Y: %.3f", chargeBT.tripleChargeTelegraphStartPosition.y);
    ImGui::Text("Telegraph Length: %.3f m", chargeBT.tripleChargeTelegraphLength);
    ImGui::Text("Telegraph Width: %.3f m", chargeBT.tripleChargeTelegraphWidth);
    ImGui::Text("Telegraph Wall Hit: %s", chargeBT.tripleChargeTelegraphWallHit ? "true" : "false");
    ImGui::Text("Telegraph Wall Distance: %.3f m", chargeBT.tripleChargeTelegraphWallDistance);
    ImGui::Text("Telegraph Length Source: %s", chargeBT.tripleChargeTelegraphLengthSource.c_str());
    ImGui::Text("WorldStatic Distance: %.3f m", chargeBT.tripleChargeTelegraphWorldStaticDistance);
    ImGui::Text("Boundary Distance: %.3f m", chargeBT.tripleChargeTelegraphBoundaryDistance);
    ImGui::SeparatorText("Telegraph Cast Snapshots");
    for (int displayIndex = 0; displayIndex < 3; ++displayIndex)
    {
        if (chargeBT.tripleChargeTelegraphSnapshotCount <= static_cast<uint64_t>(displayIndex))
            continue;
        const uint64_t newest = chargeBT.tripleChargeTelegraphSnapshotWriteIndex == 0
            ? 0 : chargeBT.tripleChargeTelegraphSnapshotWriteIndex - 1;
        const size_t snapshotIndex = static_cast<size_t>(
            (newest + chargeBT.tripleChargeTelegraphSnapshots.size() - displayIndex) %
            chargeBT.tripleChargeTelegraphSnapshots.size());
        const auto& snapshot = chargeBT.tripleChargeTelegraphSnapshots[snapshotIndex];
        ImGui::Text("Telegraph Snapshot [%d] %s (Generation %llu, Leg %d)", displayIndex,
            displayIndex == 0 ? "Latest" : displayIndex == 1 ? "Previous" : "Previous 2",
            static_cast<unsigned long long>(snapshot.generation), snapshot.chargeIndex + 1);
        ImGui::Text("  Phase: %s, Final Phase: %s, Progress: %.3f, Scale: (%.3f, %.3f, %.3f)",
            snapshot.bossInFinalPhase ? "Phase2" : "Phase1",
            snapshot.bossInFinalPhase ? "true" : "false", snapshot.phase2TransformProgress,
            snapshot.gruxScale.x, snapshot.gruxScale.y, snapshot.gruxScale.z);
        ImGui::Text("  Grux: (%.3f, %.3f, %.3f), Dir: (%.3f, %.3f, %.3f), Locked: %s",
            snapshot.gruxPosition.x, snapshot.gruxPosition.y, snapshot.gruxPosition.z,
            snapshot.direction.x, snapshot.direction.y, snapshot.direction.z,
            snapshot.directionLocked ? "true" : "false");
        ImGui::Text("  Requested Pos: (%.3f, %.3f, %.3f), Yaw: %.3f",
            snapshot.requestedPosition.x, snapshot.requestedPosition.y, snapshot.requestedPosition.z,
            snapshot.requestedYaw);
        ImGui::Text("  Requested Rot: (%.3f, %.3f, %.3f, %.3f), Scale: (%.3f, %.3f, %.3f)",
            snapshot.requestedRotation.x, snapshot.requestedRotation.y,
            snapshot.requestedRotation.z, snapshot.requestedRotation.w,
            snapshot.requestedScale.x, snapshot.requestedScale.y, snapshot.requestedScale.z);
        ImGui::Text("  Relative Pos: (%.3f, %.3f, %.3f), Relative Scale: (%.3f, %.3f, %.3f)",
            snapshot.relativePosition.x, snapshot.relativePosition.y, snapshot.relativePosition.z,
            snapshot.relativeScale.x, snapshot.relativeScale.y, snapshot.relativeScale.z);
        ImGui::Text("  Component World Pos: (%.3f, %.3f, %.3f), World Scale: (%.3f, %.3f, %.3f)",
            snapshot.componentWorldPosition.x, snapshot.componentWorldPosition.y,
            snapshot.componentWorldPosition.z, snapshot.componentWorldScale.x,
            snapshot.componentWorldScale.y, snapshot.componentWorldScale.z);
        ImGui::Text("  Mesh World Start: (%.3f, %.3f, %.3f), End: (%.3f, %.3f, %.3f)",
            snapshot.actualMeshStart.x, snapshot.actualMeshStart.y, snapshot.actualMeshStart.z,
            snapshot.actualMeshEnd.x, snapshot.actualMeshEnd.y, snapshot.actualMeshEnd.z);
        ImGui::Text("  World Matrix: [%.3f %.3f %.3f %.3f]",
            snapshot.componentWorldMatrix._11, snapshot.componentWorldMatrix._12,
            snapshot.componentWorldMatrix._13, snapshot.componentWorldMatrix._14);
        ImGui::Text("               [%.3f %.3f %.3f %.3f]",
            snapshot.componentWorldMatrix._21, snapshot.componentWorldMatrix._22,
            snapshot.componentWorldMatrix._23, snapshot.componentWorldMatrix._24);
        ImGui::Text("               [%.3f %.3f %.3f %.3f]",
            snapshot.componentWorldMatrix._31, snapshot.componentWorldMatrix._32,
            snapshot.componentWorldMatrix._33, snapshot.componentWorldMatrix._34);
        ImGui::Text("               [%.3f %.3f %.3f %.3f]",
            snapshot.componentWorldMatrix._41, snapshot.componentWorldMatrix._42,
            snapshot.componentWorldMatrix._43, snapshot.componentWorldMatrix._44);
        ImGui::Text("  CastOrigin: (%.3f, %.3f, %.3f), CastEnd: (%.3f, %.3f, %.3f)",
            snapshot.castOrigin.x, snapshot.castOrigin.y, snapshot.castOrigin.z,
            snapshot.castEnd.x, snapshot.castEnd.y, snapshot.castEnd.z);
        ImGui::Text("  Start: (%.3f, %.3f, %.3f), RawHit: %s, WorldStaticHit: %s, Dist: %.3f, Length: %.3f",
            snapshot.startPosition.x, snapshot.startPosition.y, snapshot.startPosition.z,
            snapshot.rawWallHit ? "true" : "false", snapshot.wallHit ? "true" : "false",
            snapshot.wallDistance, snapshot.length);
        ImGui::Text("  Length Source: %s, WorldStatic: %.3f, Boundary: %.3f",
            snapshot.lengthSource.c_str(), snapshot.worldStaticDistance, snapshot.boundaryDistance);
        ImGui::Text("  HitPos: (%.3f, %.3f, %.3f), Normal: (%.3f, %.3f, %.3f)",
            snapshot.hitPosition.x, snapshot.hitPosition.y, snapshot.hitPosition.z,
            snapshot.hitNormal.x, snapshot.hitNormal.y, snapshot.hitNormal.z);
        ImGui::Text("  Actor: %s, Component: %s, Layer: 0x%08X",
            snapshot.wallHitActor.c_str(), snapshot.wallHitComponent.c_str(), snapshot.wallHitLayer);
        ImGui::Text("  WallRadius: %.3f, PlayerRadius: %.3f, TurnClearance: %.3f, Max: %.3f, Capsule: %.3f",
            snapshot.wallCastRadius, snapshot.playerCastRadius, snapshot.wallTurnClearance,
            snapshot.maxDistance, snapshot.capsuleRadius);
    }
    ImGui::Text("Telegraph Expand Duration: %.3f sec", tripleChargeTelegraphExpandDuration);
    ImGui::Text("Telegraph Expand Progress: %.3f", std::clamp(
        chargeBT.tripleChargeTelegraphElapsed /
            (std::max)(0.001f, tripleChargeTelegraphExpandDuration), 0.0f, 1.0f));
    ImGui::Text("Telegraph Hold Elapsed: %.3f sec", chargeBT.tripleChargeTelegraphElapsed);
    ImGui::Text("Telegraph Hold Duration: %.3f sec", tripleChargeTelegraphHoldDuration);
    ImGui::Text("BeginTripleChargeLeg Called: %s", chargeBT.tripleBeginTripleChargeLegCalled ? "true" : "false");
    ImGui::Text("BeginTripleChargeLeg Result: %s", chargeBT.tripleBeginTripleChargeLegResult ? "true" : "false");
    ImGui::Text("BeginChargeAttackMovement Result: %s", chargeBT.tripleBeginChargeMovementResult ? "true" : "false");
    ImGui::Text("Hold-End Charge Start Validation: %s", chargeBT.tripleHoldEndChargeStartValidation ? "Valid" : "Invalid");
    ImGui::Text("Hold-End Charge Start Clearance: %.3f m", chargeBT.tripleHoldEndChargeStartClearance);
    ImGui::Text("Hold-End Charge Start Failure Reason: %s", chargeBT.tripleHoldEndChargeStartFailureReason.c_str());
    ImGui::SeparatorText("Telegraph BT Continuation Debug");
    ImGui::Text("Should Abort Charge BT: %s", debugShouldAbortChargeBT ? "true" : "false");
    ImGui::Text("Abort: BehaviorTree Disabled: %s", debugAbortBehaviorTreeDisabled ? "true" : "false");
    ImGui::Text("Abort: Execution Not Allowed: %s", debugAbortExecutionNotAllowed ? "true" : "false");
    ImGui::Text("Abort: ActiveNode Null: %s", debugAbortActiveNodeNull ? "true" : "false");
    ImGui::Text("Abort: State Mismatch: %s", debugAbortStateMismatch ? "true" : "false");
    ImGui::Text("Charge Initial State Name: %s", chargeBT.initialStateName.c_str());
    ImGui::Text("Current State Machine State Name: %s", stateMachine_ ? stateMachine_->GetStateName() : "None");
    ImGui::Text("IsChargeBTExecutionAllowed Reason: %s", debugChargeExecutionNotAllowedReason.c_str());
    ImGui::Text("Current Active BT Node: %s", debugCurrentActiveBTNode.c_str());
    ImGui::Text("Previous Active BT Node: %s", behaviorTreePreviousNode.c_str());
    ImGui::Text("BehaviorTree Last Run Result: %s", behaviorTreeLastResult.c_str());
    ImGui::Text("UpdateBehaviorTree Call Count: %llu", static_cast<unsigned long long>(debugUpdateBehaviorTreeCallCount));
    ImGui::Text("ExecuteChargeAttack Run Count: %llu", static_cast<unsigned long long>(debugExecuteChargeAttackRunCount));
    ImGui::Text("UpdateTripleChargeBT Call Count: %llu", static_cast<unsigned long long>(debugUpdateTripleChargeBTCallCount));
    ImGui::Text("Last BehaviorTree dt: %.4f sec", debugLastBehaviorTreeDeltaTime);
    ImGui::Text("Telegraph Entry Active Node: %s", debugTelegraphEntryActiveNode.c_str());
    ImGui::Text("Telegraph Entry Initial State: %s", debugTelegraphEntryInitialState.c_str());
    ImGui::Text("Telegraph Entry Current State: %s", debugTelegraphEntryCurrentState.c_str());
    ImGui::Text("Telegraph Entry BT Last Result: %s", debugTelegraphEntryBTLastResult.c_str());
    ImGui::Text("Telegraph Entry UpdateBT Count: %llu", static_cast<unsigned long long>(debugTelegraphEntryUpdateBTCount));
    ImGui::Text("Telegraph Entry TripleUpdate Count: %llu", static_cast<unsigned long long>(debugTelegraphEntryTripleUpdateCount));
    ImGui::Text("Telegraph Hold Update Count: %llu", static_cast<unsigned long long>(debugTelegraphHoldUpdateCount));
    ImGui::Text("Telegraph Hold Last dt: %.4f sec", debugTelegraphHoldLastDeltaTime);
    ImGui::Text("BeginTripleChargeTelegraph Call Count: %llu", static_cast<unsigned long long>(chargeBT.tripleChargeTelegraphCallCount));
    ImGui::Text("Triple Runtime Initialize Count: %llu", static_cast<unsigned long long>(chargeBT.tripleChargeRuntimeInitializeCount));
    ImGui::Text("Triple Phase At Update Entry: %s", triplePhaseNames[std::clamp(static_cast<int>(chargeBT.triplePhaseAtUpdateEntry), 0, 6)]);
    ImGui::Text("Triple Phase At Update Exit: %s", triplePhaseNames[std::clamp(static_cast<int>(chargeBT.triplePhaseAtUpdateExit), 0, 6)]);
    ImGui::Text("Triple Phase Assignment Count: %llu", static_cast<unsigned long long>(chargeBT.triplePhaseChangeCount));
    ImGui::Text("CleanupChargeAttackBT Called: %s", debugCleanupChargeAttackBTCalled ? "true" : "false");
    ImGui::Text("FailChargeAttackBT Called: %s", debugFailChargeAttackBTCalled ? "true" : "false");
    ImGui::Text("FinishChargeRecoveryBT Called: %s", debugFinishChargeRecoveryBTCalled ? "true" : "false");
    ImGui::Text("Charge BT Abort Detected: %s", debugChargeBTAbortDetected ? "true" : "false");
    ImGui::Text("Last Charge Abort/Cleanup Reason: %s", debugLastChargeAbortReason.c_str());
    ImGui::Text(U8("Current Wall Clearance: %.3f m"), chargeBT.tripleCurrentWallClearance);
    ImGui::Text(U8("Wall Turn Candidate: %s"), chargeBT.tripleWallTurnCandidate ? U8("true") : U8("false"));
    ImGui::Text(U8("Wall Turn Triggered: %s"), chargeBT.tripleWallTurnTriggered ? U8("true") : U8("false"));
    ImGui::Text(U8("Triple Early WallHit: %s"), chargeBT.tripleEarlyWallHit ? U8("true") : U8("false"));
    ImGui::Text(U8("Triple Final WallHit: %s"), chargeBT.tripleFinalWallHit ? U8("true") : U8("false"));
    ImGui::Text(U8("Active Charge Stun Duration: %.3f sec"), chargeBT.activeStunDuration);
#endif
}
