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
        timerAction.Reset(); 
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
    StopAttackSetupMovement();
    ClearAttackSetupTarget();
    nextRecoveryDuration.reset();
    DirectX::XMFLOAT3 target{};
    const float chargeCastRadius = (std::max)(0.05f, radius * chargeWallCastRadiusScale);
    constexpr float chargeSetupExtraClearance = 0.05f;
    const float chargeSetupBoundaryMargin = (std::max)(
        bossRoomSafetyMargin, chargeCastRadius + chargeWallCastSafetyMargin + chargeSetupExtraClearance);
    if (!FindAttackSetupTarget(chargeSetupDistanceMin, chargeSetupDistanceMax,
        attackSetupCandidateAngleStep, attackSetupClampTolerance, chargeSetupMinimumMoveDistance,
        target, chargeSetupBoundaryMargin))
    {
        CleanupChargeAttackBT();
        return false;
    }
    attackSetupTarget = {};
    attackSetupTarget.valid = true;
    attackSetupTarget.targetPosition = target;
    attackSetupTarget.stuckMovementThreshold = 0.01f; 
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
        attackSetupTarget.valid && !attackSetupRuntime.movementActive && !chargeMovementActive &&
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
    chargeBT.result = ChargeAttackEndReason::None; 
    chargeBT.phase = ChargeBTPhase::Result;
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
    StartAttack(); 
    chargeBT.attackStarted = true;
    chargeBT.phase2SettingsLatched = IsPhase2ChargeActive();
    chargeBT.triplePhase = chargeBT.phase2SettingsLatched
        ? TripleChargePhase::InitialWindup : TripleChargePhase::None;
    chargeBT.tripleChargeIndex = 0;
    chargeBT.tripleChargeTransitionElapsed = 0.0f;
    chargeBT.tripleChargeLegElapsed = 0.0f;
    chargeBT.tripleChargeLegDistance = 0.0f;
    chargeBT.tripleFinalWallHit = false;
    chargeBT.activeStunDuration = GetStunDuration();
    chargeBT.phase = ChargeBTPhase::Telegraph;
    chargeBT.animationElapsed = 0.0f;
    chargeBT.animationStalled = 0.0f;
    chargeBT.previousAnimationTime = 0.0f;
    chargeDirectionLocked = false;
    chargeDirection = {};
    PlayBodyAnimation("Pre_FootSlide_0", false, true, 0.1f, true);
    OnSelectedActionStartedSuccessfully();
}

bool GruxEnemy::BeginSingleChargeBT()
{
    HideChargeTelegraphVisual();
    if (!BeginChargeAttackMovement()) return false;
    chargeBT.result = ChargeAttackEndReason::None;
    chargeBT.phase = ChargeBTPhase::Charging;
    return true;
}

bool GruxEnemy::IsPhase2ChargeActive()
{
    const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
    return scene && scene->IsBossInFinalPhase();
}

bool GruxEnemy::BeginTripleChargeLeg()
{
    justDodgedActors.clear();
    hitActors.clear();
    HideChargeTelegraphVisual();
    const bool movementStarted = BeginChargeAttackMovement();
    if (!movementStarted)
        return false;
    chargeBT.result = ChargeAttackEndReason::None;
    const int index = std::clamp(chargeBT.tripleChargeIndex, 0, 2);
    chargeBT.tripleChargeStartPositions[index] = GetPosition();
    chargeBT.tripleChargeLegElapsed = 0.0f;
    chargeBT.tripleChargeLegDistance = 0.0f;
    chargeBT.triplePhase = TripleChargePhase::Charging;
    chargeBT.phase = ChargeBTPhase::Charging;
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
    chargeBT.tripleChargeRepositionStartPosition = GetPosition();
    chargeBT.tripleChargeRepositionTargetPosition = chargeBT.tripleChargeRepositionStartPosition;
    chargeBT.tripleChargeRepositionDirection = {};
    chargeBT.tripleChargeRepositionShortage = 0.0f;
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

    float telegraphLength = castDistance;
    if (rawWallHit)
    {
        telegraphLength = worldStaticDistance;
    }
    else if (boundaryDistance > 0.0f)
    {
        telegraphLength = boundaryDistance;
    }
    else
    {
    }
    telegraphLength = (std::max)(0.1f, telegraphLength);

    const float telegraphWidth = playerRadius * 2.0f;
    chargeBT.tripleChargeTelegraphDirection = normalizedDirection;
    chargeBT.tripleChargeTelegraphStartPosition = startPosition;
    chargeBT.tripleChargeTelegraphLength = telegraphLength;
    chargeBT.tripleChargeTelegraphWidth = telegraphWidth;
    chargeBT.tripleChargeTelegraphVisible =chargeTelegraphMeshComponent != nullptr;

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
        chargeTelegraphMeshComponent->UpdateComponentToWorld();

        chargeTelegraphMeshComponent->SetIsVisible(chargeBT.tripleChargeTelegraphVisible);
        if (chargeBT.tripleChargeTelegraphVisible)
            PlayBossTelegraphSE("boss_charge_telegraph.wav");
    }
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
    chargeTelegraphMeshComponent->SetWorldLocationDirect(
        chargeBT.tripleChargeTelegraphStartPosition);
    chargeTelegraphMeshComponent->SetWorldRotationDirect(rotationFloat);
    chargeTelegraphMeshComponent->SetRelativeScaleDirect(requestedScale);
    chargeTelegraphMeshComponent->UpdateComponentToWorld();
}


GruxEnemy::ChargeBTStepResult GruxEnemy::UpdateChargeAttackBT(float dt)
{
    return chargeBT.phase2SettingsLatched ? UpdateTripleChargeBT(dt) : UpdateSingleChargeBT(dt);
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
            chargeBT.tripleChargeTelegraphElapsed = 0.0f;
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
            bool sideWallHit = false;
            EvaluateChargeSideClearance(currentPosition, nextDirection,
                sideClearance, sideNormal, sideWallHit);
            chargeBT.tripleChargeRepositionShortage = wallHit
                ? (std::max)(0.0f, requiredClearance - clearance) : 0.0f;
            const float requiredSideClearance = (std::max)(0.05f,
                radius * chargeWallCastRadiusScale) + chargeWallCastSafetyMargin +
                tripleChargeRepositionSideSafetyMargin;
            const float sideShortage = sideWallHit
                ? (std::max)(0.0f, requiredSideClearance - sideClearance) : 0.0f;
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
                chargeBT.tripleChargeRepositionActive = plannedDistance > 0.0001f;
                chargeBT.tripleChargeRepositionStartPosition = currentPosition;
                chargeBT.tripleChargeRepositionDirection = correctionLength > 0.0001f
                    ? DirectX::XMFLOAT3{ correction.x / correctionLength, 0.0f, correction.z / correctionLength }
                    : DirectX::XMFLOAT3{};
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
        chargeDirectionLocked = false;
        chargeDirection = {};
        if (!LockChargeDirectionToPlayer())
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        if (!BeginChargeTelegraphVisual())
        {
            FailChargeAttackBT();
            chargeBT.triplePhase = TripleChargePhase::Aborted;
            return ChargeBTStepResult::Complete;
        }
        chargeBT.triplePhase = TripleChargePhase::TelegraphHold;
        chargeBT.tripleChargeTelegraphElapsed = 0.0f;
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
        const float stunDuration = chargeBT.activeStunDuration > 0.0f
            ? chargeBT.activeStunDuration : GetStunDuration();
        if (chargeBT.stunElapsed < stunDuration) return ChargeBTStepResult::Running;
        chargeBT.stunPhase = ChargeBTStunPhase::End;
        chargeBT.animationElapsed = chargeBT.animationStalled = chargeBT.previousAnimationTime = 0.0f;
        PlayBodyAnimation("Knock_Down_End", false, true, 0.1f, true);
        return ChargeBTStepResult::Running;
    }
    if (controller->IsPlayAnimation())
        return UpdateChargeAnimationWatchdogBT(controller->GetCurrentAnimationTime(), dt)
            ? ChargeBTStepResult::Running : ChargeBTStepResult::Failed;
    if (chargeBT.stunPhase == ChargeBTStunPhase::End) return ChargeBTStepResult::Complete;
    chargeBT.stunPhase = ChargeBTStunPhase::Loop;
    chargeBT.stunElapsed = 0.0f;
    PlayBodyAnimation("Knock_Down_Loop", true, true, 0.1f, true);
    return ChargeBTStepResult::Running;
}

void GruxEnemy::FinishChargeAttackBT()
{
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
        chargeBT.recoveryDuration = status == ChargeBTStepResult::Complete
            ? GetPostStunRecoveryDuration() : GetSelectedAttackRecoveryDuration();
        SetNextRecoveryDuration(chargeBT.recoveryDuration);
    }
    else if (chargeBT.phase == ChargeBTPhase::Result)
    {
        if (!chargeBT.failed && chargeBT.result == ChargeAttackEndReason::WallHit)
        {
            if (BeginChargeStunBT()) return ChargeBTStepResult::Running;
            chargeBT.failed = true;
        }
        chargeBT.recoveryDuration = GetSelectedAttackRecoveryDuration();
        if (!chargeBT.failed && chargeBT.result == ChargeAttackEndReason::PlayerHit)
        {
            chargeBT.recoveryDuration = GetChargePlayerHitRecoveryDuration();
        }
        else if (!chargeBT.failed && chargeBT.result == ChargeAttackEndReason::JustDodge)
        {
            chargeBT.recoveryDuration = GetChargeJustDodgeRecoveryDuration();
        }
        SetNextRecoveryDuration(chargeBT.recoveryDuration);
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
    const bool executionNotAllowed = !IsChargeBTExecutionAllowed();
    const bool activeNodeNull = !activeNode;
    // Charge Stunは通常の自己Abort対象ではなく、ResolveChargeResultBTで完了させる。
    return IsChargeAttackBTActive() &&
        (executionNotAllowed || activeNodeNull);
}
void GruxEnemy::CleanupChargeAttackBT()
{
    // Charge固有のRuntime、演出、当たり判定を後始末する。
    HideChargeTelegraphVisual();
    if (!IsChargeAttackBTActive()) return;
    FinishChargeAttackBT(); 
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
    chargeEndReason = ChargeAttackEndReason::None;
    nextRecoveryDuration.reset();
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
    ImGui::Checkbox("Triple Charge Telegraph Emissive Enabled", &tripleChargeTelegraphEmissiveEnabled);
    ImGui::ColorEdit3("Triple Charge Telegraph Phase 1 Emissive Color", &tripleChargeTelegraphPhase1EmissiveColor.x);
    ImGui::ColorEdit3("Triple Charge Telegraph Phase 2 Emissive Color", &tripleChargeTelegraphPhase2EmissiveColor.x);
    ImGui::DragFloat("Triple Charge Telegraph Emissive Intensity", &tripleChargeTelegraphEmissiveIntensity,
        0.05f, 0.0f, 20.0f, "%.2f");
    tripleChargeTelegraphEmissiveIntensity = (std::max)(0.0f, tripleChargeTelegraphEmissiveIntensity);
    ImGui::DragFloat("Triple Charge Telegraph Expand Duration", &tripleChargeTelegraphExpandDuration, 0.01f, 0.0f, 1.0f, "%.2f sec");
    ImGui::DragFloat("Triple Charge Telegraph Hold Duration", &tripleChargeTelegraphHoldDuration, 0.01f, 0.0f, 1.0f, "%.2f sec");
    ImGui::DragFloat("Triple Charge Telegraph Fixed World Y", &tripleChargeTelegraphFixedWorldY, 0.01f, -5.0f, 5.0f, "%.3f");
    ImGui::DragFloat("Triple Charge Telegraph Forward Offset", &tripleChargeTelegraphForwardOffset, 0.01f, -1.0f, 2.0f, "%.2f m");
    ImGui::DragFloat("Triple Charge Telegraph Max Distance", &tripleChargeTelegraphMaxDistance, 0.1f, 0.1f, 100.0f, "%.2f m");
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
