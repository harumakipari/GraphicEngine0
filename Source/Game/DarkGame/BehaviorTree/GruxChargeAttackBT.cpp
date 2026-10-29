#include "pch.h"
#include "GruxChargeAttackBT.h"
#include "NodeBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Actors/Player/Player.h"
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
    if (!ready)
        owner->FailChargeAttackBT();
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
    if (IsDead() || !battleAIActive || IsDeathPresentationActive() ||
        IsAnimationEditorPreviewActive())
        return false;
    return true;
}
bool GruxEnemy::CanPlanChargeAttack() const
{
    if (!IsChargeBTExecutionAllowed() || !BuildTargetContext().valid ||
        IsChargeAttackBTActive() || IsDashAttackBTActive() || chargeMovementActive || dashAttackMovementActive ||
        !characterMovementComponent || !rotationComponent || !GetBodyAnimationController())
        return false;
    for (size_t i = 0; i < combatActionData.size(); ++i)
        if (combatActionData[i].type == BossActionType::ChargeAttack)
            return combatActionCooldownRemaining[i] <= 0.0f;
    return false;
}

bool GruxEnemy::PrepareChargeAttackSetupTarget()
{
    if (!CanPlanChargeAttack())
        return false;
    chargeBT = {};
    chargeBT.phase = ChargeBTPhase::Setup;
    chargeBT.previousAction = selectedActionType;
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
    RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt);
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
    HideChargeTelegraphVisual();
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
    chargeBT.phase2SettingsLatched = IsPhase2ChargeActive();
    chargeBT.tripleChargeActive = chargeBT.phase2SettingsLatched;
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
    HideChargeTelegraphVisual();
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
    // A triple Charge is one attack sequence, but each leg owns its own dodge and
    // damage eligibility. Keep the sequence id while resetting per-leg records.
    ResetJustDodgeRecords("triple_charge_leg_start");
    hitActors.clear();
    HideChargeTelegraphVisual();
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
    PlayBodyAnimation("TravelMode_Idle_0",true,true,0.15f,true);
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

void GruxEnemy::HideChargeTelegraphVisual()
{
    if (chargeTelegraphMeshComponent)
        chargeTelegraphMeshComponent->SetIsVisible(false);
    chargeBT.tripleChargeTelegraphVisible = false;
}

bool GruxEnemy::BeginChargeTelegraphVisual()
{
    ++chargeBT.tripleChargeTelegraphCallCount;

    const int index = std::clamp(chargeBT.tripleChargeIndex, 0, 2);
    const DirectX::XMFLOAT3 direction = chargeDirection;
    const float directionLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (directionLength <= FLT_EPSILON)
        return false;

    const DirectX::XMFLOAT3 normalizedDirection{
        direction.x / directionLength, 0.0f, direction.z / directionLength };
    const float wallRadius = (std::max)(0.05f, radius * chargeWallCastRadiusScale);
    const float playerRadius = (std::max)(0.05f, radius * GetActiveChargePlayerCastRadiusScale());
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

    const float telegraphWidth = playerRadius * 2.0f;
    chargeBT.tripleChargeTelegraphDirection = normalizedDirection;
    chargeBT.tripleChargeTelegraphStartPosition = startPosition;
    chargeBT.tripleChargeTelegraphLength = telegraphLength;
    chargeBT.tripleChargeTelegraphWidth = telegraphWidth;
    chargeBT.tripleChargeTelegraphVisible = showTripleChargeTelegraph &&
        chargeTelegraphMeshComponent != nullptr;

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

    if (chargeTelegraphMeshComponent)
    {
        const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
        const DirectX::XMFLOAT3& phaseColor = scene && scene->IsBossInFinalPhase()
            ? tripleChargeTelegraphPhase2EmissiveColor : tripleChargeTelegraphPhase1EmissiveColor;
        chargeTelegraphMeshComponent->plusAlphaCBuffer->data.cpuColor = {
            phaseColor.x, phaseColor.y, phaseColor.z, 1.0f };
        chargeTelegraphMeshComponent->plusAlphaCBuffer->data.emissionPower =
            tripleChargeTelegraphEmissiveEnabled
            ? (std::max)(0.0f, tripleChargeTelegraphEmissiveIntensity) : 0.0f;
        const DirectX::XMFLOAT3 endPosition{
            startPosition.x + normalizedDirection.x * telegraphLength,
            startPosition.y,
            startPosition.z + normalizedDirection.z * telegraphLength };
        const DirectX::XMFLOAT3 centerPosition{
            (startPosition.x + endPosition.x) * 0.5f,
            startPosition.y,
            (startPosition.z + endPosition.z) * 0.5f };
        chargeTelegraphMeshComponent->SetWorldLocationDirect(startPosition);
        const float yaw = std::atan2(-normalizedDirection.x, -normalizedDirection.z);
        const auto rotation = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f);
        DirectX::XMFLOAT4 rotationFloat{};
        DirectX::XMStoreFloat4(&rotationFloat, rotation);
        const DirectX::XMFLOAT3 rootScale = GetScale();
        const DirectX::XMFLOAT3 requestedScale{
            telegraphWidth / (std::abs(rootScale.x) > 0.001f ? rootScale.x : 1.0f),
            1.0f,
            telegraphLength / (std::abs(rootScale.z) > 0.001f ? rootScale.z : 1.0f) };
        chargeTelegraphMeshComponent->SetWorldRotationDirect(rotationFloat);
        chargeTelegraphMeshComponent->SetRelativeScaleDirect(requestedScale);

        snapshot.requestedPosition = startPosition;
        snapshot.requestedRotation = rotationFloat;
        snapshot.requestedYaw = yaw;
        snapshot.requestedScale = requestedScale;
        snapshot.relativePosition = chargeTelegraphMeshComponent->GetRelativeLocation();
        snapshot.relativeScale = chargeTelegraphMeshComponent->GetRelativeScale();
        snapshot.componentWorldPosition = chargeTelegraphMeshComponent->GetComponentLocation();
        snapshot.componentWorldScale = chargeTelegraphMeshComponent->GetComponentScale();
        snapshot.componentWorldMatrix =
            chargeTelegraphMeshComponent->GetComponentWorldTransform().ToWorldTransform();
        const auto worldMatrix = chargeTelegraphMeshComponent->GetComponentWorldTransform().ToWorldTransform();
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
        chargeTelegraphMeshComponent->SetIsVisible(chargeBT.tripleChargeTelegraphVisible);
        if (chargeBT.tripleChargeTelegraphVisible)
            PlayBossTelegraphSE("boss_charge_telegraph.wav");
        (void)centerPosition; // The asset pivot is the start edge, not the center.
    }
    // Keep the boss-side edge fixed and reveal the plane towards its end.
    UpdateChargeTelegraphVisual(0.0f);
    return true;
}

void GruxEnemy::UpdateChargeTelegraphVisual(float lengthProgress)
{
    if (!chargeTelegraphMeshComponent)
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
    chargeTelegraphMeshComponent->SetWorldLocationDirect(
        chargeBT.tripleChargeTelegraphStartPosition);
    chargeTelegraphMeshComponent->SetWorldRotationDirect(rotationFloat);
    chargeTelegraphMeshComponent->SetRelativeScaleDirect(requestedScale);
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
            RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt);
        else if (!chargeDirectionLocked && !LockChargeDirectionToPlayer())
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        if (animationTime >= GetChargeWindupEndTime())
        {
            if (!BeginChargeTelegraphVisual())
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
        RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt);

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
                DirectX::XMFLOAT3 bossRoomResolvedMove{};
                bool bossRoomPathBlocked = false;
                if (ResolveBossRoomPositioningMove(beforeMove, moved, bossRoomResolvedMove,
                    bossRoomPathBlocked))
                {
                    moved = bossRoomResolvedMove;
                    if (bossRoomPathBlocked)
                        chargeBT.tripleChargeRepositionActive = false;
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
        if (!BeginChargeTelegraphVisual())
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
        chargeBT.tripleChargeTelegraphElapsed += (std::max)(0.0f, dt);
        const float expandDuration = (std::max)(0.001f, tripleChargeTelegraphExpandDuration);
        const float expandProgress = std::clamp(
            chargeBT.tripleChargeTelegraphElapsed / expandDuration, 0.0f, 1.0f);
        // Ease-out cubic: quick initial extension, gentle settle at full length.
        const float easedProgress = 1.0f - std::pow(1.0f - expandProgress, 3.0f);
        UpdateChargeTelegraphVisual(easedProgress);
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
            RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt);
        else if (!chargeDirectionLocked)
        {
            if (!LockChargeDirectionToPlayer() || !BeginChargeTelegraphVisual())
            {
                FailChargeAttackBT();
                return ChargeBTStepResult::Complete;
            }
            chargeBT.tripleChargeTelegraphElapsed = 0.0f;
        }
        if (chargeDirectionLocked && chargeBT.tripleChargeTelegraphVisible)
        {
            chargeBT.tripleChargeTelegraphElapsed = (std::max)(
                0.0f, animationTime - chargeDirectionLockTime);
            const float expandDuration = (std::max)(0.001f, tripleChargeTelegraphExpandDuration);
            const float expandProgress = std::clamp(
                chargeBT.tripleChargeTelegraphElapsed / expandDuration, 0.0f, 1.0f);
            UpdateChargeTelegraphVisual(expandProgress);
            DrawTripleChargeTelegraphDebug();
        }
        if (animationTime >= GetChargeWindupEndTime())
        {
            HideChargeTelegraphVisual();
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
    HideChargeTelegraphVisual();
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
    HideChargeTelegraphVisual();
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
    CleanupChargeAttackBT();
    return ChargeBTStepResult::Complete;
}

void GruxEnemy::NotifyPlayerRushStarted(Player* player)
{
    // PlayerRushState::Enter calls this only after it locks Player's rushTarget
    // to this Grux. Charge execution is the remaining cancellation guard.
    if (!player || !IsChargeAttackBTActive())
        return;

    rushCombatHoldActive = true;
    interruptedChargeRecoveryPending = true;
    interruptedChargeRecoveryActive = false;
    interruptedChargeRecoveryTimer = 0.0f;
    interruptedChargeRecoveryDuration = GetChargeJustDodgeRecoveryDuration();

    CleanupChargeAttackBT();
    PlayBodyAnimation("Stun_Idle", true, true, 0.15f, true);
    ResetBehaviorTreeRuntime();
    behaviorTreePreviousNode = "None";
}

void GruxEnemy::NotifyPlayerRushEnded(Player* player)
{
    if (!player)
        return;

    rushCombatHoldActive = false;
    if (interruptedChargeRecoveryPending)
    {
        interruptedChargeRecoveryPending = false;
        PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
        interruptedChargeRecoveryActive = true;
        interruptedChargeRecoveryTimer = 0.0f;
    }
}

bool GruxEnemy::ShouldAbortChargeAttackBT()
{
    const bool behaviorTreeDisabled = !behaviorTreeFastComboEnabled;
    const bool executionNotAllowed = !IsChargeBTExecutionAllowed();
    const bool activeNodeNull = !activeNode;
    return IsChargeAttackBTActive() &&
        (behaviorTreeDisabled || executionNotAllowed || activeNodeNull);
}
void GruxEnemy::CleanupChargeAttackBT()
{
    HideChargeTelegraphVisual();
    if (!IsChargeAttackBTActive()) return;
    FinishChargeAttackBT(); // Idempotent; no cooldown before StartAttack, none per leg.
    const float tripleTurnClearance = chargeBT.tripleChargeWallTurnClearance;
    const float tripleMaxDistance = chargeBT.tripleChargeLegMaxDistance;
    const float tripleMaxDuration = chargeBT.tripleChargeLegMaxDuration;
    const float tripleTransitionDuration = chargeBT.tripleChargeTransitionDuration;
    const float tripleStunMultiplier = chargeBT.tripleWallStunDurationMultiplier;
    const bool preserveAnimation = rushCombatHoldActive || IsDead() || IsDeathPresentationActive() || !battleAIActive ||
        IsAnimationEditorPreviewActive();
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

    ImGui::DragFloat(U8("三連突進 壁前切り返し距離"), &chargeBT.tripleChargeWallTurnClearance, 0.05f, 0.0f, 10.0f, "%.2f m");
    ImGui::DragFloat(U8("三連突進 1・2段目 最大突進距離"), &chargeBT.tripleChargeLegMaxDistance, 0.1f, 0.1f, 50.0f, "%.2f m");
    ImGui::DragFloat(U8("三連突進 1・2段目 最大突進時間"), &chargeBT.tripleChargeLegMaxDuration, 0.05f, 0.1f, 10.0f, "%.2f sec");
    ImGui::DragFloat(U8("三連突進 段間切り返し時間"), &chargeBT.tripleChargeTransitionDuration, 0.01f, 0.0f, 2.0f, "%.2f sec");
    ImGui::DragFloat(U8("三連突進 最終壁スタン倍率"), &chargeBT.tripleWallStunDurationMultiplier, 0.05f, 1.0f, 5.0f, "%.2fx");
    ImGui::DragFloat("Triple Charge Reposition Max Distance", &tripleChargeRepositionMaxDistance, 0.05f, 0.0f, 2.0f, "%.2f m");
    ImGui::DragFloat("Triple Charge Reposition Safety Margin", &tripleChargeRepositionSafetyMargin, 0.01f, 0.0f, 1.0f, "%.2f m");
    ImGui::DragFloat("Triple Charge Reposition Speed", &tripleChargeRepositionSpeed, 0.05f, 0.1f, 10.0f, "%.2f m/s");
    ImGui::DragFloat("Triple Charge Reposition Side Safety Margin", &tripleChargeRepositionSideSafetyMargin, 0.01f, 0.0f, 1.0f, "%.2f m");
    ImGui::Checkbox("Show Triple Charge Telegraph", &showTripleChargeTelegraph);
    ImGui::Checkbox("Triple Charge Telegraph Emissive Enabled", &tripleChargeTelegraphEmissiveEnabled);
    ImGui::ColorEdit3("Triple Charge Telegraph Phase 1 Emissive Color", &tripleChargeTelegraphPhase1EmissiveColor.x);
    ImGui::ColorEdit3("Triple Charge Telegraph Phase 2 Emissive Color", &tripleChargeTelegraphPhase2EmissiveColor.x);
    ImGui::DragFloat("Triple Charge Telegraph Emissive Intensity", &tripleChargeTelegraphEmissiveIntensity,
        0.05f, 0.0f, 20.0f, "%.2f");
    tripleChargeTelegraphEmissiveIntensity = (std::max)(0.0f, tripleChargeTelegraphEmissiveIntensity);
    ImGui::DragFloat("Triple Charge Telegraph Expand Duration", &tripleChargeTelegraphExpandDuration, 0.01f, 0.0f, 1.0f, "%.2f sec");
    ImGui::DragFloat("Triple Charge Telegraph Hold Duration", &tripleChargeTelegraphHoldDuration, 0.01f, 0.0f, 1.0f, "%.2f sec");
    ImGui::DragFloat("Triple Charge Telegraph Ground Offset", &tripleChargeTelegraphGroundOffset, 0.005f, -0.1f, 0.5f, "%.3f m");
    ImGui::DragFloat("Triple Charge Telegraph Fixed World Y", &tripleChargeTelegraphFixedWorldY, 0.01f, -5.0f, 5.0f, "%.3f");
    ImGui::DragFloat("Triple Charge Telegraph Forward Offset", &tripleChargeTelegraphForwardOffset, 0.01f, -1.0f, 2.0f, "%.2f m");
    ImGui::DragFloat("Triple Charge Telegraph Max Distance", &tripleChargeTelegraphMaxDistance, 0.1f, 0.1f, 100.0f, "%.2f m");
    tripleChargeTelegraphWidthMultiplier = 1.0f;
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
    tripleChargeTelegraphWidthMultiplier = 1.0f;

    ImGui::DragFloat(U8("第一形態時のChargeの速さ"), &chargeSpeed,
        0.1f, 0.1f, 50.0f, "%.2f m/s");
    ImGui::DragFloat(U8("第二形態時のChargeの速さ"), &chargeSpeedPhase2,
        0.1f, 0.1f, 50.0f, "%.2f m/s");

    for (BossAttackData& attackData : combatAttackData)
    {
        if (attackData.type != BossAttackType::ChargeAttack)
            continue;
        ImGui::DragInt("Charge Damage (Phase 1)", &attackData.damagePerHit, 1.0f, 0, 1000);
        attackData.damagePerHit = (std::max)(0, attackData.damagePerHit);
        break;
    }
    ImGui::DragInt("Charge Damage (Phase 2)", &chargeDamagePhase2, 1.0f, 0, 1000);
    chargeDamagePhase2 = (std::max)(0, chargeDamagePhase2);

    ImGui::DragFloat(U8("第一形態時突進当たり判定"), &chargePlayerCastRadiusScale,
        0.01f, 0.1f, 2.0f, "%.2f");
    ImGui::DragFloat(U8("第二形態時突進当たり判定"), &chargePlayerCastRadiusScalePhase2,
        0.01f, 0.1f, 2.0f, "%.2f");
    ImGui::DragFloat("Charge Player Damage Forward Offset", &chargePlayerDamageForwardOffset,
        0.01f, 0.0f, 5.0f, "%.2f m");
    ImGui::DragFloat(U8("第一形態時Charge Just Dodge判定"), &chargeJustDodgeCastRadiusScale,
        0.01f, 0.1f, 2.0f, "%.2f");
    ImGui::DragFloat(U8("第二形態時Charge Just Dodge判定"), &chargeJustDodgeCastRadiusScalePhase2,
        0.01f, 0.1f, 5.0f, "%.2f");
    ImGui::DragFloat("Charge Wall Cast Radius Scale", &chargeWallCastRadiusScale,
        0.01f, 0.1f, 2.0f, "%.2f");
    chargePlayerCastRadiusScalePhase2 = std::clamp(chargePlayerCastRadiusScalePhase2, 0.1f, 2.0f);
    chargePlayerDamageForwardOffset = std::clamp(chargePlayerDamageForwardOffset, 0.0f, 5.0f);
    chargeJustDodgeCastRadiusScale = std::clamp(chargeJustDodgeCastRadiusScale, 0.1f, 2.0f);
    chargeJustDodgeCastRadiusScalePhase2 = std::clamp(chargeJustDodgeCastRadiusScalePhase2, 0.1f, 5.0f);

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




    ImGui::DragFloat(
        U8("今回の後隙時間: %.2f sec"),
        &chargeBT.recoveryDuration,0.05f);

#endif
}
