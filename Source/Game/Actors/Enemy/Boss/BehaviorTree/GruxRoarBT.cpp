#include "pch.h"
#include "GruxRoarBT.h"
#include "NodeBase.h"
#include "Game/Actors/Enemy/Boss/GruxEnemy.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Scenes/GameScene.h"

#include "Engine/Scene/Scene.h"
#include "Core/ActorManager.h"
#include "Physics/CollisionFunction.h"

// Retreat・Roarの内部判定
namespace
{
    bool IsOtherBTActionForRoar(NodeBase* node)
    {
        return node && node->GetName() != "Idle" && node->GetName() != "StartRoar";
    }

    bool IsOtherBTActionForRetreat(NodeBase* node)
    {
        return node && node->GetName() != "Idle" &&
            node->GetName() != "PrepareRetreatTarget";
    }

    struct RetreatSweepShape
    {
        Capsule capsule{};
        DirectX::XMFLOAT3 center{};
        float halfHeight = 0.0f;
        bool isSphere = false;
    };

    RetreatSweepShape BuildRetreatSweepShape(const CapsuleComponent& component)
    {
        RetreatSweepShape shape{};
        const auto componentPosition = component.GetComponentWorldTransform().GetLocation();
        shape.center = componentPosition;
        float collisionOffset = 0.8f;	// 少し上げる
        shape.center.y += component.GetCollisionOffsetY() + collisionOffset;
        shape.capsule.r = component.GetRadius();
        shape.halfHeight = (std::max)(component.GetHeight() * 0.5f - shape.capsule.r, 0.0f);
        shape.isSphere = shape.halfHeight <= FLT_EPSILON;
        shape.capsule.a = shape.center;
        shape.capsule.b = shape.center;
        shape.capsule.a.y += shape.halfHeight;
        shape.capsule.b.y -= shape.halfHeight;
        return shape;
    }

    bool SweepRetreatPath(const RetreatSweepShape& shape, const DirectX::XMFLOAT3& direction, float distance, HitResult& result, uint32_t obstacleMask)
    {
        PositioningSweepShape queryShape{};
        queryShape.type = shape.isSphere ? PositioningSweepShape::Type::Sphere : PositioningSweepShape::Type::Capsule;
        queryShape.center = shape.center;
        queryShape.point1 = shape.capsule.a;
        queryShape.point2 = shape.capsule.b;
        queryShape.radius = shape.capsule.r;
        PositioningPathSafetyPolicy policy{};
        policy.floorNormalThreshold = 0.7f;
        policy.supportPenetrationTolerance = 0.30f;
        PositioningPathSafetyResult pathResult{};
        const bool pathBlocked = Physics::Instance().SweepPositioningPath(queryShape, direction, distance, obstacleMask, policy, pathResult);
        if (pathResult.blockingHit.hit)
        {
            result.position = pathResult.blockingHit.position;
            result.normal = pathResult.blockingHit.normal;
            result.distance = pathResult.blockingHit.distance;
        }

        return pathBlocked;
    }
}

// 防御・Positioning・RoarのBTノード
bool CanPlanRoar::Judgment() { return owner->CanPlanRoar(); }
bool CanPlanAnyDefensive::Judgment()
{
    if (owner->IsInitialRepositionFallbackIdlePending())
    {
        return false;
    }
    if (!owner->CanPlanAttackAgainstCurrentPlayer())
    {
        return false;
    }
    const bool result = owner->CanPlanRoar() || owner->CanPlanRetreat();
    return result;
}
bool CanPlanRetreat::Judgment() { return owner->CanPlanRetreat(); }
bool CanPlanAnyCombatDecision::Judgment() { return owner->CanPlanAnyCombatDecision(); }
bool CanPlanInitialReposition::Judgment() { return owner->CanPlanInitialReposition(); }
bool CanPlanReposition::Judgment() { return owner->CanPlanReposition(); }
ActionBase::State PrepareInitialRepositionTarget::Run(float)
{
    if (owner->PrepareRepositionTarget())
        return State::Complete;
    owner->BeginInitialRepositionFallback();
    return State::Failed;
}
ActionBase::State PrepareRepositionTarget::Run(float)
{
    return owner->PrepareRepositionTarget() ? State::Complete : State::Failed;
}
ActionBase::State MoveToRepositionTarget::Run(float dt)
{
    const auto result = owner->UpdateRepositionMovement(dt);
    if (result == GruxEnemy::PositioningMoveResult::Running)
        return State::Run;
    const bool arrived = result == GruxEnemy::PositioningMoveResult::Arrived;
    owner->FinishRepositionMovement(arrived);
    if (!arrived && owner->IsInitialRepositionPending())
        owner->BeginInitialRepositionFallback();
    return arrived ? State::Complete : State::Failed;
}
ActionBase::State WaitBeforeInitialReposition::Run(float dt)
{
    elapsed += (std::max)(0.0f, dt);
    if (elapsed < 1.5f) // 1.5秒待機してからボス戦を始める
        return State::Run;
    elapsed = 0.0f;
    return State::Complete;
}
ActionBase::State WaitAfterReposition::Run(float dt)
{
    elapsed += (std::max)(0.0f, dt);
    if (elapsed < owner->GetRepositionArrivalWaitDuration())
        return State::Run;
    elapsed = 0.0f;
    owner->CompleteRepositionArrivalWait();
    return State::Complete;
}
ActionBase::State PrepareRetreatTarget::Run(float)
{
    return owner->PrepareRetreatTarget() ? State::Complete : State::Failed;
}
ActionBase::State MoveToPositioningTarget::Run(float dt)
{
    const auto result = owner->UpdateRetreatMovement(dt);
    if (result == GruxEnemy::PositioningMoveResult::Running)
        return State::Run;
    const bool arrived = result == GruxEnemy::PositioningMoveResult::Arrived;
    owner->FinishRetreatMovement(arrived);
    return arrived ? State::Complete : State::Failed;
}
ActionBase::State StartRoar::Run(float)
{
    return owner->BeginRoarBT() ? State::Complete : State::Failed;
}
ActionBase::State ExecuteRoar::Run(float dt)
{
    const int result = owner->UpdateRoarBT(dt);
    return result < 0 ? State::Failed : result > 0 ? State::Complete : State::Run;
}
ActionBase::State FinishRoar::Run(float)
{
    const bool complete = owner->IsRoarBTActive();
    owner->CleanupRoarBT();
    return complete ? State::Complete : State::Failed;
}

// 攻撃・Roar実行条件
bool GruxEnemy::IsRoarExecutionAllowed() const
{
    if (!battleAIActive || IsDead() || IsPendingKill() ||
        isDeathPerform ||
        finalHitReactionActive || finalHitReactionHeld || IsAnimationEditorPreviewActive() ||
        IsChargeAttackBTActive() || IsDashAttackBTActive() || chargeMovementActive ||
        dashAttackMovementActive || jumpMotionWarpOverrideActive || !characterMovementComponent ||
        !GetBodyAnimationController() || !GetOwnerScene())
        return false;
    if (IsStunned())
        return false;
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    return player && !player->IsPendingKill() && player->GetHp() > 0;
}

bool GruxEnemy::CanPlanAttackAgainstCurrentPlayer() const
{
    const auto scene = GetOwnerScene();
    if (!scene)
        return false;

    const auto player = scene->GetActorManager()->GetActorOfType<Player>();
    return player && !player->IsPendingKill() && player->IsBossAttackTargetAvailable();
}

bool GruxEnemy::CanPlanRoar() const
{
    if (!IsRoarExecutionAllowed() || IsRoarBTActive() || roarCooldownRemaining > 0.0f ||
        IsOtherBTActionForRoar(activeNode))
        return false;
    const auto context = BuildTargetContext();
    return IsDefensiveBackForBehavior(context) &&
        context.xzDistance <= defensiveTooCloseDistance;
}

// Retreat
bool GruxEnemy::CanPlanRetreat() const
{
    if (!IsRoarExecutionAllowed() || IsRoarBTActive() || roarCooldownRemaining <= 0.0f ||
        retreatRetryCooldownRemaining > 0.0f || retreatTarget.valid ||
        retreatMovementRuntime.movementActive || IsOtherBTActionForRetreat(activeNode) ||
        !rotationComponent || !enemyCapsuleComponent)
        return false;
    const auto context = BuildTargetContext();
    return IsDefensiveBackForBehavior(context) &&
        context.xzDistance <= defensiveTooCloseDistance;
}

bool GruxEnemy::PrepareRetreatTarget()
{
    ClearPositioningTarget(retreatTarget, retreatMovementRuntime);
    retreatMoveDirection = RepositionStrafeDirection::None;
    retreatMoveDirectionVector = {};
    if (!CanPlanRetreat())
    {
        retreatRetryCooldownRemaining = retreatRetryCooldownDuration;
        return false;
    }

    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player)
    {
        retreatRetryCooldownRemaining = retreatRetryCooldownDuration;
        return false;
    }
    const DirectX::XMFLOAT3 playerSnapshot = player->GetPosition();
    const DirectX::XMFLOAT3 gruxSnapshot = GetPosition();
    float awayX = gruxSnapshot.x - playerSnapshot.x;
    float awayZ = gruxSnapshot.z - playerSnapshot.z;
    const float startingPlayerDistance = std::sqrt(awayX * awayX + awayZ * awayZ);
    if (startingPlayerDistance > FLT_EPSILON)
    {
        awayX /= startingPlayerDistance;
        awayZ /= startingPlayerDistance;
    }
    else
    {
        const auto forward = GetForward();
        awayX = forward.x;
        awayZ = forward.z;
    }

    const float minDistance = (std::max)(0.0f, retreatDistanceMin);
    const float maxDistance = (std::max)(minDistance, retreatDistanceMax);
    const std::array<float, 3> distances{ maxDistance, (minDistance + maxDistance) * 0.5f, minDistance };
    const uint32_t obstacleMask = CollisionHelper::ToBit(CollisionLayer::WorldStatic) |
        CollisionHelper::ToBit(CollisionLayer::WorldProps) |
        CollisionHelper::ToBit(CollisionLayer::Convex);
    const RetreatSweepShape retreatSweepShape = BuildRetreatSweepShape(*enemyCapsuleComponent);
    for (const float distance : distances)
    {
        for (int index = 0; index <= 12; ++index)
        {
            const float magnitude = static_cast<float>((index + 1) / 2);
            const float signedStep = index == 0 ? 0.0f : (index % 2 == 1 ? magnitude : -magnitude);
            const float angle = DirectX::XMConvertToRadians(signedStep * 30.0f);
            const float directionX = awayX * std::cos(angle) - awayZ * std::sin(angle);
            const float directionZ = awayX * std::sin(angle) + awayZ * std::cos(angle);
            const DirectX::XMFLOAT3 desired{ gruxSnapshot.x + directionX * distance, gruxSnapshot.y, gruxSnapshot.z + directionZ * distance };
            RepositionTargetEvaluation evaluation{};
            EvaluateClampedPositioningTarget(gruxSnapshot, desired, evaluation);
            const float moveX = evaluation.clampedTarget.x - gruxSnapshot.x;
            const float moveZ = evaluation.clampedTarget.z - gruxSnapshot.z;
            const float moveDistance = std::sqrt(moveX * moveX + moveZ * moveZ);
            const float playerX = evaluation.clampedTarget.x - playerSnapshot.x;
            const float playerZ = evaluation.clampedTarget.z - playerSnapshot.z;
            const float plannedPlayerDistance = std::sqrt(playerX * playerX + playerZ * playerZ);
            if (evaluation.clampDistance > attackSetupClampTolerance || plannedPlayerDistance <= startingPlayerDistance + 0.01f || moveDistance < retreatMinimumMoveDistance || evaluation.bossRoomPathBlocked)
                continue;

            const DirectX::XMFLOAT3 targetDirection{ moveX / moveDistance, 0.0f, moveZ / moveDistance };
            const DirectX::XMFLOAT3 playerDirection{ -awayX, 0.0f, -awayZ };
            const DirectX::XMFLOAT3 leftDirection{ -playerDirection.z, 0.0f, playerDirection.x };
            const DirectX::XMFLOAT3 rightDirection{ playerDirection.z, 0.0f, -playerDirection.x };
            const DirectX::XMFLOAT3 backstepDirection{ -playerDirection.x, 0.0f, -playerDirection.z };
            const auto isSafeMove = [&](const DirectX::XMFLOAT3& moveDirection, DirectX::XMFLOAT3& outTarget)
                {
                    const DirectX::XMFLOAT3 moveTarget{ gruxSnapshot.x + moveDirection.x * moveDistance, gruxSnapshot.y, gruxSnapshot.z + moveDirection.z * moveDistance };
                    RepositionTargetEvaluation moveEvaluation{};
                    EvaluateClampedPositioningTarget(gruxSnapshot, moveTarget, moveEvaluation);
                    if (moveEvaluation.bossRoomPathBlocked || moveEvaluation.clampDistance > 0.001f)
                        return false;
                    HitResult hit{};
                    if (SweepRetreatPath(retreatSweepShape, moveDirection, moveDistance, hit, obstacleMask))
                        return false;
                    outTarget = moveEvaluation.clampedTarget;
                    return true;
                };

            DirectX::XMFLOAT3 leftTarget{};
            DirectX::XMFLOAT3 rightTarget{};
            const bool leftSafe = isSafeMove(leftDirection, leftTarget);
            const bool rightSafe = isSafeMove(rightDirection, rightTarget);
            DirectX::XMFLOAT3 selectedTarget{};
            if (leftSafe || rightSafe)
            {
                const float leftAlignment = targetDirection.x * leftDirection.x + targetDirection.z * leftDirection.z;
                const float rightAlignment = targetDirection.x * rightDirection.x + targetDirection.z * rightDirection.z;
                if (leftSafe && (!rightSafe || leftAlignment >= rightAlignment))
                {
                    retreatMoveDirection = RepositionStrafeDirection::Left;
                    selectedTarget = leftTarget;
                }
                else
                {
                    retreatMoveDirection = RepositionStrafeDirection::Right;
                    selectedTarget = rightTarget;
                }
            }
            else if (isSafeMove(backstepDirection, selectedTarget))
            {
                retreatMoveDirection = RepositionStrafeDirection::Backstep;
                retreatMoveDirectionVector = backstepDirection;
            }
            else
            {
                continue;
            }

            retreatTarget = {};
            retreatTarget.valid = true;
            retreatTarget.targetPosition = selectedTarget;
            retreatTarget.arrivalTolerance = 0.3f;
            retreatTarget.timeout = 3.0f;
            retreatTarget.maxMoveDistance = moveDistance + 1.0f;
            retreatTarget.moveSpeed = retreatMoveSpeed;
            retreatTarget.stuckMovementThreshold = 0.01f;
            retreatTarget.stuckTimeThreshold = 0.5f;
            return true;
        }
    }
    ClearPositioningTarget(retreatTarget, retreatMovementRuntime);
    retreatMoveDirection = RepositionStrafeDirection::None;
    retreatMoveDirectionVector = {};
    retreatRetryCooldownRemaining = retreatRetryCooldownDuration;
    return false;
}

GruxEnemy::PositioningMoveResult GruxEnemy::UpdateRetreatMovement(float deltaTime)
{
    if (!IsRoarExecutionAllowed() || !retreatTarget.valid || !characterMovementComponent || !rotationComponent || !enemyCapsuleComponent || retreatMoveDirection == RepositionStrafeDirection::None)
        return PositioningMoveResult::InvalidTarget;
    const auto controller = GetBodyAnimationController();
    if (!controller)
        return PositioningMoveResult::InvalidTarget;
    const bool isBackstep = retreatMoveDirection == RepositionStrafeDirection::Backstep;
    const char* animationName = isBackstep ? "Dodge_Bwd" : (retreatMoveDirection == RepositionStrafeDirection::Left ? "TravelMode_Lft_0" : "TravelMode_Rgt_0");
    if (!controller->GetAnimationAsset(animationName))
        return PositioningMoveResult::InvalidTarget;

    const float safeDt = (std::max)(0.0f, deltaTime);
    if (!retreatMovementRuntime.movementActive)
    {
        retreatMovementRuntime.movementActive = true;
        retreatMovementRuntime.previousPosition = GetPosition();
    }
    const float previousElapsed = retreatMovementRuntime.elapsed;
    retreatMovementRuntime.elapsed += safeDt;
    if (!isBackstep && retreatMovementRuntime.elapsed >= retreatTarget.timeout)
        return PositioningMoveResult::Timeout;

    const auto position = GetPosition();
    const float movedX = position.x - retreatMovementRuntime.previousPosition.x;
    const float movedZ = position.z - retreatMovementRuntime.previousPosition.z;
    const float frameMovement = std::sqrt(movedX * movedX + movedZ * movedZ);
    retreatMovementRuntime.traveledDistance += frameMovement;
    retreatMovementRuntime.previousPosition = position;
    const float requiredDistance = (std::max)(0.0f, retreatTarget.maxMoveDistance - 1.0f);
    if (isBackstep && controller->GetCurrentAnimationName() == animationName && !controller->IsPlayAnimation())
        return retreatMovementRuntime.traveledDistance >= requiredDistance - retreatTarget.arrivalTolerance ? PositioningMoveResult::Arrived : PositioningMoveResult::Stuck;

    const float targetX = retreatTarget.targetPosition.x - position.x;
    const float targetZ = retreatTarget.targetPosition.z - position.z;
    const float remainingDistance = std::sqrt(targetX * targetX + targetZ * targetZ);
    if (!isBackstep && remainingDistance <= retreatTarget.arrivalTolerance)
        return PositioningMoveResult::Arrived;
    if (isBackstep && retreatMovementRuntime.traveledDistance < requiredDistance)
        retreatMovementRuntime.stuckTime = frameMovement < retreatTarget.stuckMovementThreshold ? retreatMovementRuntime.stuckTime + safeDt : 0.0f;
    else if (!isBackstep)
        retreatMovementRuntime.stuckTime = frameMovement < retreatTarget.stuckMovementThreshold ? retreatMovementRuntime.stuckTime + safeDt : 0.0f;
    if (retreatMovementRuntime.traveledDistance >= retreatTarget.maxMoveDistance)
        return PositioningMoveResult::MaxDistanceReached;
    if (retreatMovementRuntime.stuckTime >= retreatTarget.stuckTimeThreshold)
        return PositioningMoveResult::Stuck;

    const auto context = BuildTargetContext();
    if (!context.valid)
        return PositioningMoveResult::InvalidTarget;
    RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), safeDt);
    DirectX::XMFLOAT3 moveDirection = retreatMoveDirectionVector;
    if (!isBackstep)
    {
        moveDirection = retreatMoveDirection == RepositionStrafeDirection::Left
            ? DirectX::XMFLOAT3{ -context.directionToPlayer.z, 0.0f, context.directionToPlayer.x }
            : DirectX::XMFLOAT3{ context.directionToPlayer.z, 0.0f, -context.directionToPlayer.x };
    }
    float frameDistance = retreatTarget.moveSpeed * safeDt;
    if (isBackstep)
    {
        constexpr std::array<std::pair<float, float>, 23> distanceCurve
        { { { 0.166666700f, 0.000000000f }, { 0.183333300f, 0.006010519f }, { 0.200000000f, 0.023290758f }, { 0.216666700f, 0.050713751f }, { 0.233333300f, 0.087152512f }, { 0.250000000f, 0.131480097f }, { 0.266666700f, 0.182569507f }, { 0.283333400f, 0.239293748f }, { 0.300000000f, 0.300525895f }, { 0.316666700f, 0.365139014f }, { 0.333333300f, 0.432006004f }, { 0.350000000f, 0.500000000f }, { 0.366666700f, 0.567993996f }, { 0.383333400f, 0.634860986f }, { 0.400000000f, 0.699474105f }, { 0.416666700f, 0.760706252f }, { 0.433333400f, 0.817430469f }, { 0.450000000f, 0.868519844f }, { 0.466666700f, 0.912847465f }, { 0.483333300f, 0.949286231f }, { 0.500000000f, 0.976709231f }, { 0.516666700f, 0.993989457f }, { 0.533333400f, 1.000000000f } } };
        const auto getDistanceAtTime = [&](float time)
            {
                if (time <= distanceCurve.front().first) return distanceCurve.front().second;
                if (time >= distanceCurve.back().first) return distanceCurve.back().second;
                for (size_t index = 1; index < distanceCurve.size(); ++index)
                {
                    const auto& previousKey = distanceCurve[index - 1];
                    const auto& nextKey = distanceCurve[index];
                    if (time <= nextKey.first)
                    {
                        const float duration = nextKey.first - previousKey.first;
                        return std::lerp(previousKey.second, nextKey.second, duration > FLT_EPSILON ? (time - previousKey.first) / duration : 0.0f);
                    }
                }
                return distanceCurve.back().second;
            };
        frameDistance = (std::max)(0.0f, getDistanceAtTime(retreatMovementRuntime.elapsed) - getDistanceAtTime(previousElapsed)) * requiredDistance;
    }

    const DirectX::XMFLOAT3 desired{ position.x + moveDirection.x * frameDistance, position.y, position.z + moveDirection.z * frameDistance };
    RepositionTargetEvaluation evaluation{};
    EvaluateClampedPositioningTarget(position, desired, evaluation);
    const RetreatSweepShape retreatSweepShape = BuildRetreatSweepShape(*enemyCapsuleComponent);
    const uint32_t obstacleMask = CollisionHelper::ToBit(CollisionLayer::WorldStatic) | CollisionHelper::ToBit(CollisionLayer::WorldProps) | CollisionHelper::ToBit(CollisionLayer::Convex);
    HitResult hit{};
    if (evaluation.bossRoomPathBlocked || evaluation.clampDistance > 0.001f || (frameDistance > 0.0f && SweepRetreatPath(retreatSweepShape, moveDirection, frameDistance, hit, obstacleMask)))
    {
        StopAIMovement();
        return PositioningMoveResult::Stuck;
    }

    const float frameSpeed = safeDt > FLT_EPSILON ? frameDistance / safeDt : 0.0f;
    characterMovementComponent->SetFixedSpeed(isBackstep ? frameSpeed : retreatTarget.moveSpeed);
    characterMovementComponent->SetInputMagnitude(frameSpeed > 0.0f || !isBackstep ? 1.0f : 0.0f);
    characterMovementComponent->SetMoveDirection(moveDirection);
    if (!controller->IsPlayAnimation() || controller->GetCurrentAnimationName() != animationName)
        PlayBodyAnimation(animationName, !isBackstep, true, 0.15f, true);
    return PositioningMoveResult::Running;
}

void GruxEnemy::FinishRetreatMovement(bool arrived)
{
    StopAIMovement();
    EndPositioningAnimation();
    ClearPositioningTarget(retreatTarget, retreatMovementRuntime);
    retreatMoveDirection = RepositionStrafeDirection::None;
    retreatMoveDirectionVector = {};
    if (arrived)
        consecutiveAttackCount = 0;
    retreatRetryCooldownRemaining = retreatRetryCooldownDuration;
}

// 防御行動選択
void GruxEnemy::BeginCombatDecisionInference()
{
    repositionDecisionCached = true;
    repositionDecisionResult = false;
    float repositionDecisionRoll = 0.0f;
    const bool defensiveAvailable = CanPlanAttackAgainstCurrentPlayer() &&
        (CanPlanRoar() || CanPlanRetreat());
    if (!IsRoarExecutionAllowed() || defensiveAvailable ||
        repositionCooldownRemaining > 0.0f || repositionRetryCooldownRemaining > 0.0f ||
        repositionTarget.valid || repositionMovementRuntime.movementActive || !rotationComponent ||
        !characterMovementComponent || !enemyCapsuleComponent)
        return;
    const auto context = BuildTargetContext();
    if (!context.valid || context.distanceRegion == BossDistanceRegion::Far)
        return;
    const int index = (std::min)(consecutiveAttackCount, 3);
    const float chance = std::clamp(repositionChanceByAttackCount[index], 0.0f, 1.0f);
    static thread_local std::mt19937 engine{ std::random_device{}() };
    repositionDecisionRoll = std::uniform_real_distribution<float>(0.0f, 1.0f)(engine);
    repositionDecisionResult = repositionDecisionRoll < chance;
}

bool GruxEnemy::CanPlanAnyCombatDecision()
{
    if (initialRepositionFallbackIdlePending)
    {
        return false;
    }
    if (!repositionDecisionCached)
    {
        BeginCombatDecisionInference();
    }
    const bool playerAvailableForAttack = CanPlanAttackAgainstCurrentPlayer();
    bool canPlanFastCombo = false;
    bool canPlanJump = false;
    bool canPlanDash = false;
    bool canPlanCharge = false;
    if (playerAvailableForAttack)
    {
        canPlanFastCombo = CanPlanFastCombo();
        canPlanJump = CanPlanJumpAttack();
        canPlanDash = CanPlanDashAttack();
        canPlanCharge = CanPlanChargeAttack();
    }
    else
    {
        canPlanFastCombo = false;
        canPlanJump = false;
        canPlanDash = false;
        canPlanCharge = false;
    }
    const bool canPlanAttack = playerAvailableForAttack && (canPlanFastCombo || canPlanJump || canPlanDash || canPlanCharge);
    const bool canPlan = CanPlanReposition() || canPlanAttack;
    return canPlan;
}

// Reposition
bool GruxEnemy::CanPlanReposition()
{
    const bool initialReposition = CanPlanInitialReposition();
    const bool decisionAllows = initialReposition || (repositionDecisionCached && repositionDecisionResult);
    const bool targetInactive = !repositionTarget.valid;
    const bool movementInactive = !repositionMovementRuntime.movementActive;
    const bool cooldownReady = repositionCooldownRemaining <= 0.0f;
    const bool retryReady = repositionRetryCooldownRemaining <= 0.0f;
    const bool playerAvailableForDefensive = CanPlanAttackAgainstCurrentPlayer();
    const bool roarAvailable = playerAvailableForDefensive && CanPlanRoar();
    const bool retreatAvailable = playerAvailableForDefensive && !roarAvailable && CanPlanRetreat();
    const bool defensiveInactive = !roarAvailable && !retreatAvailable;
    const bool nodeAllowed = !activeNode || activeNode->GetName() == "PrepareRepositionTarget" ||
        activeNode->GetName() == "PrepareInitialRepositionTarget";
    const bool executionAllowed = IsRoarExecutionAllowed();
    const bool componentsValid = rotationComponent && characterMovementComponent && enemyCapsuleComponent;
    const bool normalRepositionReady = cooldownReady && retryReady && defensiveInactive && executionAllowed;
    const bool result = !initialRepositionFallbackIdlePending && decisionAllows && targetInactive && movementInactive &&
        nodeAllowed && componentsValid && (initialReposition || normalRepositionReady);

    return result;
}

bool GruxEnemy::PrepareRepositionTarget()
{
    ClearPositioningTarget(repositionTarget, repositionMovementRuntime);
    repositionStrafeDirection = RepositionStrafeDirection::None;
    repositionBackstepDirection = {};
    if (!CanPlanReposition())
    {
        if (!IsInitialRepositionPending())
            repositionRetryCooldownRemaining = repositionRetryCooldownDuration;
        return false;
    }

    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player)
    {
        if (!IsInitialRepositionPending())
            repositionRetryCooldownRemaining = repositionRetryCooldownDuration;
        return false;
    }

    const BossTargetContext context = BuildTargetContext();
    if (!context.valid)
    {
        if (!IsInitialRepositionPending())
            repositionRetryCooldownRemaining = repositionRetryCooldownDuration;
        return false;
    }
    const DirectX::XMFLOAT3 gruxPosition = GetPosition();
    const float strafeDistance = (std::max)(0.0f, repositionMoveSpeed) *
        (std::max)(0.0f, repositionStrafeDuration);
    const float strafeProbeDistance = strafeDistance;
    const float backstepDistance = (std::max)(0.0f, repositionBackstepDistance);
    const uint32_t obstacleMask = CollisionHelper::ToBit(CollisionLayer::WorldStatic) |
        CollisionHelper::ToBit(CollisionLayer::WorldProps) |
        CollisionHelper::ToBit(CollisionLayer::Convex);
    const RetreatSweepShape sweepShape = BuildRetreatSweepShape(*enemyCapsuleComponent);
    const DirectX::XMFLOAT3 leftDirection{
        -context.directionToPlayer.z, 0.0f, context.directionToPlayer.x };
    const DirectX::XMFLOAT3 rightDirection{
        context.directionToPlayer.z, 0.0f, -context.directionToPlayer.x };
    const DirectX::XMFLOAT3 backstepDirection{
        -context.directionToPlayer.x, 0.0f, -context.directionToPlayer.z };
    const auto isSafeMoveDirection = [&](const DirectX::XMFLOAT3& direction, float distance,
        float minimumDistance)
        {
            if (distance < (std::max)(0.0f, minimumDistance))
                return false;
            const DirectX::XMFLOAT3 desired{
                gruxPosition.x + direction.x * distance,
                gruxPosition.y,
                gruxPosition.z + direction.z * distance };
            RepositionTargetEvaluation evaluation{};
            EvaluateClampedPositioningTarget(gruxPosition, desired, evaluation);
            if (evaluation.bossRoomPathBlocked || evaluation.clampDistance > 0.01f)
                return false;
            HitResult hit{};
            return !SweepRetreatPath(sweepShape, direction, distance, hit, obstacleMask);
        };

    const bool canBackstep = context.xzDistance <= repositionBackstepTriggerDistance &&
        isSafeMoveDirection(backstepDirection, backstepDistance, backstepDistance);
    if (canBackstep)
    {
        repositionStrafeDirection = RepositionStrafeDirection::Backstep;
        repositionBackstepDirection = backstepDirection;
        rotationComponent->SetDirectionImmediate(context.directionToPlayer);
        backstepDebugRuntime = {};
        backstepDebugRuntime.active = true;
        backstepDebugRuntime.startPosition = gruxPosition;
        Logger::Log(Logger::LogCategory::Gameplay, std::format(
            "[GruxBackstep] Begin position=({:.3f}, {:.3f}, {:.3f}) configuredDistance={:.3f} playerDistance={:.3f}",
            gruxPosition.x, gruxPosition.y, gruxPosition.z, backstepDistance, context.xzDistance));
    }
    else
    {
        const bool leftSafe = isSafeMoveDirection(leftDirection, strafeProbeDistance, 0.0f);
        const bool rightSafe = isSafeMoveDirection(rightDirection, strafeProbeDistance, 0.0f);
        if (leftSafe && rightSafe)
        {
            static thread_local std::mt19937 randomEngine{ std::random_device{}() };
            repositionStrafeDirection = std::uniform_int_distribution<int>(0, 1)(randomEngine) == 0
                ? RepositionStrafeDirection::Left : RepositionStrafeDirection::Right;
        }
        else if (leftSafe)
        {
            repositionStrafeDirection = RepositionStrafeDirection::Left;
        }
        else if (rightSafe)
        {
            repositionStrafeDirection = RepositionStrafeDirection::Right;
        }
        const char* reason = context.xzDistance > repositionBackstepTriggerDistance
            ? "player_distance" : "backstep_path";
        const char* selected = repositionStrafeDirection == RepositionStrafeDirection::Left
            ? "left_strafe" : (repositionStrafeDirection == RepositionStrafeDirection::Right
                ? "right_strafe" : "none");
        Logger::Log(Logger::LogCategory::Gameplay, std::format(
            "[GruxBackstep] Fallback reason={} selected={} configuredDistance={:.3f} playerDistance={:.3f}",
            reason, selected, backstepDistance, context.xzDistance));
    }

    if (repositionStrafeDirection == RepositionStrafeDirection::None)
    {
        if (!IsInitialRepositionPending())
            repositionRetryCooldownRemaining = repositionRetryCooldownDuration;
        return false;
    }

    repositionTarget = {};
    repositionTarget.valid = true;
    if (repositionStrafeDirection == RepositionStrafeDirection::Backstep)
    {
        repositionTarget.timeout = (std::max)(0.0f, repositionBackstepDuration);
        repositionTarget.moveSpeed = repositionBackstepDuration > FLT_EPSILON
            ? backstepDistance / repositionBackstepDuration : 0.0f;
    }
    else
    {
        repositionTarget.timeout = (std::max)(0.0f, repositionStrafeDuration);
        repositionTarget.moveSpeed = (std::max)(0.0f, repositionMoveSpeed);
    }
    repositionTarget.stuckMovementThreshold = 0.01f;
    repositionTarget.stuckTimeThreshold = 0.5f;
    if (repositionStrafeDirection == RepositionStrafeDirection::Left ||
        repositionStrafeDirection == RepositionStrafeDirection::Right)
    {
        strafeDebugRuntime = {};
        strafeDebugRuntime.active = true;
        strafeDebugRuntime.initialReposition = IsInitialRepositionPending();
        strafeDebugRuntime.leftStrafe = repositionStrafeDirection == RepositionStrafeDirection::Left;
        strafeDebugRuntime.startPosition = gruxPosition;
        strafeDebugRuntime.configuredDuration = repositionTarget.timeout;
        strafeDebugRuntime.configuredSpeed = repositionTarget.moveSpeed;
        strafeDebugRuntime.startingPlayerDistance = context.xzDistance;
        Logger::Log(Logger::LogCategory::Gameplay, std::format(
            "[GruxStrafe] Begin initial={} direction={} position=({:.3f}, {:.3f}, {:.3f}) configuredDuration={:.3f} configuredSpeed={:.3f} playerDistance={:.3f}",
            strafeDebugRuntime.initialReposition, strafeDebugRuntime.leftStrafe ? "left" : "right",
            gruxPosition.x, gruxPosition.y, gruxPosition.z, strafeDebugRuntime.configuredDuration,
            strafeDebugRuntime.configuredSpeed, strafeDebugRuntime.startingPlayerDistance));
    }
    return true;
}
GruxEnemy::PositioningMoveResult GruxEnemy::UpdateRepositionMovement(float dt)
{
    if (!repositionTarget.valid)
    {
        if (strafeDebugRuntime.active)
            strafeDebugRuntime.endReason = "invalid";
        return PositioningMoveResult::InvalidTarget;
    }
    if (repositionStrafeDirection == RepositionStrafeDirection::None)
    {
        if (strafeDebugRuntime.active)
            strafeDebugRuntime.endReason = "invalid";
        return PositioningMoveResult::Arrived;
    }
    if (!IsRoarExecutionAllowed() || !characterMovementComponent || !rotationComponent ||
        !enemyCapsuleComponent)
    {
        if (strafeDebugRuntime.active)
            strafeDebugRuntime.endReason = "invalid";
        return PositioningMoveResult::InvalidTarget;
    }
    const BossTargetContext context = BuildTargetContext();
    if (!context.valid)
    {
        if (strafeDebugRuntime.active)
            strafeDebugRuntime.endReason = "invalid";
        return PositioningMoveResult::InvalidTarget;
    }
    const auto controller = GetBodyAnimationController();
    if (!controller)
    {
        if (strafeDebugRuntime.active)
            strafeDebugRuntime.endReason = "invalid";
        return PositioningMoveResult::InvalidTarget;
    }
    const bool isBackstep = repositionStrafeDirection == RepositionStrafeDirection::Backstep;
    const char* animationName = isBackstep ? "Dodge_Bwd" :
        (repositionStrafeDirection == RepositionStrafeDirection::Left
            ? "TravelMode_Lft_0" : "TravelMode_Rgt_0");
    if (!controller->GetAnimationAsset(animationName))
    {
        if (strafeDebugRuntime.active)
            strafeDebugRuntime.endReason = "invalid";
        return PositioningMoveResult::InvalidTarget;
    }
    if (isBackstep && controller->GetCurrentAnimationName() == animationName &&
        !controller->IsPlayAnimation())
        return PositioningMoveResult::Arrived;

    const float safeDt = (std::max)(0.0f, dt);
    if (!repositionMovementRuntime.movementActive)
    {
        repositionMovementRuntime.movementActive = true;
        repositionMovementRuntime.previousPosition = GetPosition();
    }
    const float previousElapsed = repositionMovementRuntime.elapsed;
    repositionMovementRuntime.elapsed += safeDt;
    if (!isBackstep && repositionMovementRuntime.elapsed >= repositionTarget.timeout)
    {
        strafeDebugRuntime.endReason = "timeout";
        return PositioningMoveResult::Arrived;
    }

    const DirectX::XMFLOAT3 position = GetPosition();
    const float movedX = position.x - repositionMovementRuntime.previousPosition.x;
    const float movedZ = position.z - repositionMovementRuntime.previousPosition.z;
    const float frameMovement = std::sqrt(movedX * movedX + movedZ * movedZ);
    repositionMovementRuntime.previousPosition = position;
    if (!isBackstep)
    {
        repositionMovementRuntime.stuckTime = frameMovement < repositionTarget.stuckMovementThreshold
            ? repositionMovementRuntime.stuckTime + safeDt : 0.0f;
        if (repositionMovementRuntime.stuckTime >= repositionTarget.stuckTimeThreshold)
        {
            strafeDebugRuntime.endReason = "stuck";
            return PositioningMoveResult::Arrived;
        }
    }
    if (isBackstep && repositionMovementRuntime.movementBlocked)
    {
        characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
        characterMovementComponent->SetInputMagnitude(0.0f);
        characterMovementComponent->ResetFixedSpeed();
        return PositioningMoveResult::Stuck;
    }

    DirectX::XMFLOAT3 direction{};
    if (isBackstep)
    {
        direction = repositionBackstepDirection;
    }
    else
    {
        RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), safeDt);
        direction = repositionStrafeDirection == RepositionStrafeDirection::Left
            ? DirectX::XMFLOAT3{ -context.directionToPlayer.z, 0.0f, context.directionToPlayer.x }
        : DirectX::XMFLOAT3{ context.directionToPlayer.z, 0.0f, -context.directionToPlayer.x };
    }
    const float directionLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (directionLength <= FLT_EPSILON)
    {
        if (strafeDebugRuntime.active)
            strafeDebugRuntime.endReason = "invalid";
        return PositioningMoveResult::InvalidTarget;
    }
    direction.x /= directionLength;
    direction.z /= directionLength;
    float frameDistance = repositionTarget.moveSpeed * safeDt;
    if (isBackstep)
    {
        constexpr std::array<std::pair<float, float>, 23> distanceCurve
        { {
            { 0.166666700f, 0.000000000f }, { 0.183333300f, 0.006010519f },
            { 0.200000000f, 0.023290758f }, { 0.216666700f, 0.050713751f },
            { 0.233333300f, 0.087152512f }, { 0.250000000f, 0.131480097f },
            { 0.266666700f, 0.182569507f }, { 0.283333400f, 0.239293748f },
            { 0.300000000f, 0.300525895f }, { 0.316666700f, 0.365139014f },
            { 0.333333300f, 0.432006004f }, { 0.350000000f, 0.500000000f },
            { 0.366666700f, 0.567993996f }, { 0.383333400f, 0.634860986f },
            { 0.400000000f, 0.699474105f }, { 0.416666700f, 0.760706252f },
            { 0.433333400f, 0.817430469f }, { 0.450000000f, 0.868519844f },
            { 0.466666700f, 0.912847465f }, { 0.483333300f, 0.949286231f },
            { 0.500000000f, 0.976709231f }, { 0.516666700f, 0.993989457f },
            { 0.533333400f, 1.000000000f }
        } };
        const auto getDistanceAtTime = [&](float time)
            {
                if (time <= distanceCurve.front().first)
                    return distanceCurve.front().second;
                if (time >= distanceCurve.back().first)
                    return distanceCurve.back().second;
                for (size_t index = 1; index < distanceCurve.size(); ++index)
                {
                    const auto& previousKey = distanceCurve[index - 1];
                    const auto& nextKey = distanceCurve[index];
                    if (time <= nextKey.first)
                    {
                        const float keyDuration = nextKey.first - previousKey.first;
                        const float blend = keyDuration > FLT_EPSILON
                            ? (time - previousKey.first) / keyDuration : 0.0f;
                        return std::lerp(previousKey.second, nextKey.second, blend);
                    }
                }
                return distanceCurve.back().second;
            };
        frameDistance = (std::max)(0.0f, getDistanceAtTime(repositionMovementRuntime.elapsed) -
            getDistanceAtTime(previousElapsed)) * (std::max)(0.0f, repositionBackstepDistance);
        if (backstepDebugRuntime.active)
            backstepDebugRuntime.requestedDistance += frameDistance;
    }
    const DirectX::XMFLOAT3 desired{
        position.x + direction.x * frameDistance,
        position.y,
        position.z + direction.z * frameDistance };
    RepositionTargetEvaluation evaluation{};
    EvaluateClampedPositioningTarget(position, desired, evaluation);
    if (evaluation.bossRoomPathBlocked || evaluation.clampDistance > 0.001f)
    {
        if (isBackstep)
        {
            repositionMovementRuntime.movementBlocked = true;
            backstepDebugRuntime.roomBoundaryBlocked = true;
            characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
            characterMovementComponent->SetInputMagnitude(0.0f);
            characterMovementComponent->ResetFixedSpeed();
            return PositioningMoveResult::Stuck;
        }
        strafeDebugRuntime.endReason = "room_boundary";
        return PositioningMoveResult::Arrived;
    }
    const RetreatSweepShape sweepShape = BuildRetreatSweepShape(*enemyCapsuleComponent);
    const uint32_t obstacleMask = CollisionHelper::ToBit(CollisionLayer::WorldStatic) |
        CollisionHelper::ToBit(CollisionLayer::WorldProps) |
        CollisionHelper::ToBit(CollisionLayer::Convex);
    HitResult hit{};
    if (frameDistance > 0.0f && SweepRetreatPath(sweepShape, direction, frameDistance, hit, obstacleMask))
    {
        if (isBackstep)
        {
            repositionMovementRuntime.movementBlocked = true;
            backstepDebugRuntime.sweepBlocked = true;
            characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
            characterMovementComponent->SetInputMagnitude(0.0f);
            characterMovementComponent->ResetFixedSpeed();
            return PositioningMoveResult::Stuck;
        }
        strafeDebugRuntime.endReason = "sweep";
        return PositioningMoveResult::Arrived;
    }

    const float frameSpeed = safeDt > FLT_EPSILON ? frameDistance / safeDt : 0.0f;
    if (isBackstep && backstepDebugRuntime.active)
    {
        backstepDebugRuntime.appliedDistance += frameDistance;
        backstepDebugRuntime.maximumFrameSpeed = (std::max)(backstepDebugRuntime.maximumFrameSpeed, frameSpeed);
    }
    characterMovementComponent->SetFixedSpeed(isBackstep ? frameSpeed : repositionTarget.moveSpeed);
    characterMovementComponent->SetInputMagnitude(frameSpeed > 0.0f || !isBackstep ? 1.0f : 0.0f);
    characterMovementComponent->SetMoveDirection(direction);
    if (!controller->IsPlayAnimation() || controller->GetCurrentAnimationName() != animationName)
        PlayBodyAnimation(animationName, !isBackstep, true, 0.15f, true);
    return PositioningMoveResult::Running;
}
void GruxEnemy::FinishRepositionMovement(bool arrived)
{
    if (strafeDebugRuntime.active)
    {
        const DirectX::XMFLOAT3 endPosition = GetPosition();
        const float deltaX = endPosition.x - strafeDebugRuntime.startPosition.x;
        const float deltaZ = endPosition.z - strafeDebugRuntime.startPosition.z;
        const float horizontalDistance = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        Logger::Log(Logger::LogCategory::Gameplay, std::format(
            "[GruxStrafe] End initial={} direction={} start=({:.3f}, {:.3f}, {:.3f}) end=({:.3f}, {:.3f}, {:.3f}) configuredDuration={:.3f} elapsed={:.3f} configuredSpeed={:.3f} horizontalDistance={:.3f} playerDistance={:.3f} reason={}",
            strafeDebugRuntime.initialReposition, strafeDebugRuntime.leftStrafe ? "left" : "right",
            strafeDebugRuntime.startPosition.x, strafeDebugRuntime.startPosition.y, strafeDebugRuntime.startPosition.z,
            endPosition.x, endPosition.y, endPosition.z, strafeDebugRuntime.configuredDuration,
            repositionMovementRuntime.elapsed, strafeDebugRuntime.configuredSpeed, horizontalDistance,
            strafeDebugRuntime.startingPlayerDistance,
            strafeDebugRuntime.endReason ? strafeDebugRuntime.endReason : (arrived ? "timeout" : "invalid")));
        strafeDebugRuntime = {};
    }
    if (backstepDebugRuntime.active)
    {
        const DirectX::XMFLOAT3 endPosition = GetPosition();
        const float deltaX = endPosition.x - backstepDebugRuntime.startPosition.x;
        const float deltaZ = endPosition.z - backstepDebugRuntime.startPosition.z;
        const float horizontalDistance = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        Logger::Log(Logger::LogCategory::Gameplay, std::format(
            "[GruxBackstep] End arrived={} start=({:.3f}, {:.3f}, {:.3f}) end=({:.3f}, {:.3f}, {:.3f}) horizontalDistance={:.3f} configuredDistance={:.3f} requestedDistance={:.3f} appliedDistance={:.3f} maximumFrameSpeed={:.3f} roomBoundaryBlocked={} sweepBlocked={}",
            arrived, backstepDebugRuntime.startPosition.x, backstepDebugRuntime.startPosition.y,
            backstepDebugRuntime.startPosition.z, endPosition.x, endPosition.y, endPosition.z,
            horizontalDistance, repositionBackstepDistance, backstepDebugRuntime.requestedDistance,
            backstepDebugRuntime.appliedDistance, backstepDebugRuntime.maximumFrameSpeed,
            backstepDebugRuntime.roomBoundaryBlocked, backstepDebugRuntime.sweepBlocked));
        backstepDebugRuntime = {};
    }
    StopAIMovement();
    EndPositioningAnimation();
    ClearPositioningTarget(repositionTarget, repositionMovementRuntime);
    repositionStrafeDirection = RepositionStrafeDirection::None;
    repositionBackstepDirection = {};
    if (!arrived && !IsInitialRepositionPending())
        repositionRetryCooldownRemaining = repositionRetryCooldownDuration;
}
void GruxEnemy::BeginInitialRepositionFallback()
{
    initialRepositionPending = false;
    initialRepositionFallbackIdlePending = true;
    repositionDecisionCached = false;
    repositionDecisionResult = false;
}
void GruxEnemy::CompleteInitialRepositionFallbackIdle()
{
    initialRepositionFallbackIdlePending = false;
    repositionDecisionCached = false;
    repositionDecisionResult = false;
}
void GruxEnemy::CompleteRepositionArrivalWait()
{
    const bool completedInitialReposition = initialRepositionPending;
    repositionCooldownRemaining = repositionCooldownDuration;
    consecutiveAttackCount = 0;
    if (completedInitialReposition)
    {
        initialRepositionPending = false;
        repositionDecisionCached = false;
        repositionDecisionResult = false;
    }
}

// Roar攻撃
bool GruxEnemy::BeginRoarBT()
{
    if (!CanPlanRoar())
    {
        return false;
    }
    const auto controller = GetBodyAnimationController();
    if (!std::isfinite(roarPreStampedeStartTime) || !std::isfinite(roarPreStampedeEndTime) || roarPreStampedeStartTime < 0.0f || roarPreStampedeEndTime <= roarPreStampedeStartTime || controller->GetAnimationLength("Pre_Stampede_0") < roarPreStampedeEndTime)
    {
        return false;
    }
    StopAIMovement();
    DisableAttackHitBoxes();
    HideRoarFloatingDebris();
    HideRoarImpactDebris();
    roarBT = {};
    roarTelegraphProgress = 0.0f;
    PlayBodyAnimation("Pre_Stampede_0", false, true, 0.05f, true);
    if (!controller->SetPlaybackRange(roarPreStampedeStartTime, roarPreStampedeEndTime))
    {
        roarBT.stage = RoarStage::Telegraph;
        CleanupRoarBT();
        return false;
    }
    roarBT.stage = RoarStage::Telegraph;
    roarBT.previousTime = roarPreStampedeStartTime;
    roarBT.endTime = roarPreStampedeEndTime;
    ShowRoarTelegraph();
    InitializeRoarFloatingDebris();
    UpdateRoarFloatingDebris();
    roarCooldownRemaining = roarCooldownDuration;
    return true;
}

int GruxEnemy::UpdateRoarBT(float dt)
{
    auto fail = [this]() { CleanupRoarBT(); return -1; };
    if (!IsRoarBTActive() || !IsRoarExecutionAllowed()) return fail();
    const auto controller = GetBodyAnimationController();
    const char* expected = roarBT.stage == RoarStage::Telegraph ? "Pre_Stampede_0" : "LevelStart_0";
    if (controller->GetCurrentAnimationName() != expected) return fail();
    const float time = controller->GetCurrentAnimationTime();
    const float safeDt = (std::max)(0.0f, dt);
    roarBT.elapsed += safeDt;
    roarBT.stalled = time > roarBT.previousTime + 0.00001f ? 0.0f : roarBT.stalled + safeDt;
    roarBT.previousTime = time;
    if (roarBT.stalled > 2.0f || roarBT.elapsed > 60.0f) return fail();
    StopAIMovement();
    UpdateRoarTelegraphProgress();
    UpdateRoarTelegraphTransform();
    UpdateRoarFloatingDebris();
    if (controller->IsPlayAnimation()) return 0;
    if (time + 0.02f < roarBT.endTime) return fail();
    if (roarBT.stage == RoarStage::Telegraph)
    {
        PlayBodyAnimation("LevelStart_0", false, true, 0.15f, true);
        roarBT.stage = RoarStage::Shockwave;
        roarBT.previousTime = 0.0f;
        roarBT.stalled = 0.0f;
        roarBT.endTime = controller->GetAnimationLength("LevelStart_0");
        if (roarBT.endTime < 0.30f) return fail();
        return 0;
    }
    if (!roarBT.shockwaveFired) return fail();
    return 1;
}

void GruxEnemy::ApplyRoarShockwave()
{
    const auto controller = GetBodyAnimationController();
    if (roarBT.stage != RoarStage::Shockwave || roarBT.shockwaveFired ||
        !IsRoarExecutionAllowed() || !controller || controller->GetCurrentAnimationName() != "LevelStart_0")
        return;
    roarBT.shockwaveFired = true;
    roarTelegraphProgress = 1.0f;
    UpdateRoarTelegraphTransform();
    UpdateRoarFloatingDebris();
    HideRoarTelegraph();
    HideRoarFloatingDebris();
    SpawnRoarImpactDebris();
    SpawnRoarGroundBurst();
    SpawnGroundImpactEffect();
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player) return;
    auto direction = MathHelper::Subtract(player->GetPosition(), GetPosition());
    if (std::abs(direction.y) > roarHeightTolerance) return;
    direction.y = 0.0f;
    const float roarRadius = GetRoarAttackRadius();
    if (direction.x * direction.x + direction.z * direction.z > roarRadius * roarRadius) return;
    if (MathHelper::Length(direction) < 0.0001f)
    {
        direction = GetForward();
        direction.y = 0.0f;
    }
    if (!player->TryTakeDamage(roarDamage, GetPosition(), Player::DamagePolicy::IgnoreDodgeIFrames))
    {
        return;
    }

    player->StartKnockBack(direction);
}

float GruxEnemy::GetRoarAttackRadius() const
{
    const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
    return scene && scene->IsBossInFinalPhase() ? roarRadiusPhase2 : roarRadiusPhase1;
}

// Roar予兆描画
void GruxEnemy::ShowRoarTelegraph()
{
    if (!roarTelegraphOuterMeshComponent || !roarTelegraphFillMeshComponent)
        return;
    UpdateRoarTelegraphTransform();
    roarTelegraphOuterMeshComponent->SetIsVisible(true);
    roarTelegraphFillMeshComponent->SetIsVisible(true);
}

void GruxEnemy::UpdateRoarTelegraphTransform()
{
    if (!roarTelegraphOuterMeshComponent || !roarTelegraphFillMeshComponent)
        return;

    const DirectX::XMFLOAT3 position = GetPosition();
    const float radius = GetRoarAttackRadius();
    const DirectX::XMFLOAT3 outerWorldPosition = { position.x, roarTelegraphOuterYOffset, position.z };
    const DirectX::XMFLOAT3 fillWorldPosition = { position.x, roarTelegraphFillYOffset, position.z };
    const DirectX::XMFLOAT4 identityRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    const DirectX::XMFLOAT3 scale{ radius, 1.0f, radius };
    const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
    const bool isPhase2 = scene && scene->IsBossInFinalPhase();
    const DirectX::XMFLOAT3& outerTint = isPhase2
        ? roarTelegraphPhase2OuterTint : roarTelegraphPhase1OuterTint;
    const DirectX::XMFLOAT3& fillTintMin = isPhase2
        ? roarTelegraphPhase2FillTintMin : roarTelegraphPhase1FillTintMin;
    const DirectX::XMFLOAT3& fillTintMax = isPhase2
        ? roarTelegraphPhase2FillTintMax : roarTelegraphPhase1FillTintMax;
    const float t = std::clamp(roarTelegraphProgress, 0.0f, 1.0f);
    const float visualProgress = t * t * (3.0f - 2.0f * t);
    const auto lerp = [visualProgress](float minimum, float maximum)
        {
            return minimum + (maximum - minimum) * visualProgress;
        };
    const DirectX::XMFLOAT3 fillTint = {
        lerp(fillTintMin.x, fillTintMax.x),
        lerp(fillTintMin.y, fillTintMax.y),
        lerp(fillTintMin.z, fillTintMax.z) };
    const float fillGlow = lerp(roarTelegraphFillGlowMin, roarTelegraphFillGlowMax);
    const float fillAlpha = lerp(roarTelegraphFillAlphaMin, roarTelegraphFillAlphaMax);

    roarTelegraphOuterMeshComponent->SetRelativeLocationDirect(outerWorldPosition);
    roarTelegraphOuterMeshComponent->SetRelativeRotationDirect(identityRotation);
    roarTelegraphOuterMeshComponent->SetRelativeScaleDirect(scale);
    roarTelegraphOuterMeshComponent->plusAlphaCBuffer->data.brightness =
        std::clamp(roarTelegraphOuterAlpha, 0.0f, 1.0f) - 1.0f;
    roarTelegraphOuterMeshComponent->plusAlphaCBuffer->data.cpuColor = {
        outerTint.x, outerTint.y, outerTint.z, 1.0f };
    roarTelegraphOuterMeshComponent->plusAlphaCBuffer->data.emissionPower = 1.0f;

    roarTelegraphFillMeshComponent->SetRelativeLocationDirect(fillWorldPosition);
    roarTelegraphFillMeshComponent->SetRelativeRotationDirect(identityRotation);
    roarTelegraphFillMeshComponent->SetRelativeScaleDirect(scale);
    roarTelegraphFillMeshComponent->plusAlphaCBuffer->data.brightness =
        std::clamp(fillAlpha, 0.0f, 1.0f) - 1.0f;
    roarTelegraphFillMeshComponent->plusAlphaCBuffer->data.cpuColor = {
        fillTint.x, fillTint.y, fillTint.z, 1.0f };
    roarTelegraphFillMeshComponent->plusAlphaCBuffer->data.emissionPower =
        (std::max)(0.0f, fillGlow);
}

void GruxEnemy::UpdateRoarTelegraphProgress()
{
    const auto controller = GetBodyAnimationController();
    if (!controller || roarBT.stage == RoarStage::None)
        return;

    constexpr float shockwaveEventTime = 0.30f;
    const float preDuration = (std::max)(0.0f, roarPreStampedeEndTime - roarPreStampedeStartTime);
    const float totalDuration = preDuration + shockwaveEventTime;
    if (totalDuration <= 0.0f)
    {
        roarTelegraphProgress = 0.0f;
        return;
    }

    const float preWeight = preDuration / totalDuration;
    const float time = controller->GetCurrentAnimationTime();
    if (roarBT.stage == RoarStage::Telegraph)
    {
        const float preProgress = preDuration > 0.0f
            ? std::clamp((time - roarPreStampedeStartTime) / preDuration, 0.0f, 1.0f)
            : 1.0f;
        roarTelegraphProgress = preProgress * preWeight;
        return;
    }

    const float shockwaveProgress = std::clamp(time / shockwaveEventTime, 0.0f, 1.0f);
    roarTelegraphProgress = preWeight + (1.0f - preWeight) * shockwaveProgress;
}

void GruxEnemy::HideRoarTelegraph()
{
    if (roarTelegraphOuterMeshComponent)
        roarTelegraphOuterMeshComponent->SetIsVisible(false);
    if (roarTelegraphFillMeshComponent)
        roarTelegraphFillMeshComponent->SetIsVisible(false);
}

// Roar地面演出・衝撃破片
void GruxEnemy::SpawnRoarGroundBurst()
{
    if (!roarGroundBurstEnabled || !roarGroundBurstEffectComponent)
        return;

    const DirectX::XMFLOAT3 center = GetPosition();
    const float attackRadius = GetRoarAttackRadius();
    if (attackRadius <= FLT_EPSILON)
        return;

    const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
    const bool isPhase2 = scene && scene->IsBossInFinalPhase();
    const int innerCount = isPhase2 ? 4 : 3;
    const int outerCount = isPhase2 ? 8 : 6;
    const float innerRatio = std::clamp(roarGroundBurstInnerRingRatio, 0.0f, 1.0f);
    const float outerRatio = std::clamp(roarGroundBurstOuterRingRatio, innerRatio, 1.0f);
    const float angleJitterRadians = DirectX::XMConvertToRadians(
        (std::max)(0.0f, roarGroundBurstAngleJitterDegrees));
    const float radiusJitter = (std::max)(0.0f, roarGroundBurstRadiusJitter);
    const EffectHandle effectHandle = roarGroundBurstEffectComponent->GetEffectHandle();

    const auto emitRing = [&](const int count, const float ringRatio, const float phaseOffset)
        {
            for (int index = 0; index < count; ++index)
            {
                const float baseAngle = DirectX::XM_2PI *
                    (static_cast<float>(index) / static_cast<float>(count)) + phaseOffset;
                const float angle = baseAngle + MathHelper::RandomRange(
                    -angleJitterRadians, angleJitterRadians);
                const float ratio = std::clamp(ringRatio + MathHelper::RandomRange(
                    -radiusJitter, radiusJitter), 0.0f, 1.0f);
                const float distance = attackRadius * ratio;
                const DirectX::XMFLOAT3 spawnPosition{
                    center.x + sinf(angle) * distance,
                    center.y + roarGroundBurstYOffset,
                    center.z + cosf(angle) * distance };
                EffectManager::EmitParticle(effectHandle, spawnPosition, { 0.0f, 0.0f, 0.0f });
            }
        };

    emitRing(innerCount, innerRatio, 0.0f);
    emitRing(outerCount, outerRatio, DirectX::XM_PI / static_cast<float>(outerCount));
}

void GruxEnemy::EnsureRoarImpactDebris()
{
    constexpr const char* smallModelPaths[] =
    {
        "./Data/Models/EffectDebri/SmallDebri/debri.gltf",
        "./Data/Models/EffectDebri/SmallDebri/debri1.gltf",
        "./Data/Models/EffectDebri/SmallDebri/debri2.gltf",
    };
    constexpr const char* blockModelPaths[] =
    {
        "./Data/Models/EffectDebri/Block/block1.gltf",
        "./Data/Models/EffectDebri/Block/block2.gltf",
        "./Data/Models/EffectDebri/Block/block3.gltf",
        "./Data/Models/EffectDebri/Block/block4.gltf",
    };
    for (size_t index = 0; index < roarImpactDebris.size(); ++index)
    {
        auto& debris = roarImpactDebris[index];
        bool isBlock = index >= 12;
        if (!debris.meshComponent)
        {
            const char* modelPath = isBlock ? blockModelPaths[(index - 12) % std::size(blockModelPaths)] : smallModelPaths[index % std::size(smallModelPaths)];
            debris.meshComponent = AddComponent<SkeletalMeshComponent>("RoarImpactDebris_" + std::to_string(index), "GruxEnemy");
            debris.meshComponent->SetModel(modelPath);
            debris.meshComponent->SetIsCastShadow(false);
            debris.meshComponent->SetUsingAbsoluteLocation(true);
            debris.meshComponent->SetUsingAbsoluteRotation(true);
            debris.meshComponent->SetUsingAbsoluteScale(true);
        }
        debris.active = false;
        debris.meshComponent->SetIsVisible(false);
    }
}

void GruxEnemy::SpawnRoarImpactDebris()
{
    EnsureRoarImpactDebris();
    HideRoarImpactDebris();
    if (!roarImpactDebrisEnabled)
        return;

    const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
    const bool isPhase2 = scene && scene->IsBossInFinalPhase();
    const int innerCount = isPhase2 ? 5 : 4;
    const int outerCount = isPhase2 ? 11 : 8;
    const int smallCount = isPhase2 ? 12 : 10;
    const DirectX::XMFLOAT3 center = GetPosition();
    const float attackRadius = GetRoarAttackRadius();
    const float innerRatio = std::clamp(roarGroundBurstInnerRingRatio, 0.0f, 1.0f);
    const float outerRatio = std::clamp(roarGroundBurstOuterRingRatio, innerRatio, 1.0f);
    const float angleJitterRadians = DirectX::XMConvertToRadians((std::max)(0.0f, roarImpactDebrisAngleJitterDegrees));
    const float radiusJitter = (std::max)(0.0f, roarImpactDebrisRadiusJitter);
    const float spawnHeight = std::clamp(roarImpactDebrisSpawnHeight, 0.0f, 1.0f);
    int emittedIndex = 0;
    const auto emitRing = [&](const int count, const float ringRatio, const float phaseOffset)
        {
            for (int index = 0; index < count; ++index)
            {
                const bool isBlock = emittedIndex >= smallCount;
                const size_t poolIndex = isBlock
                    ? 12 + static_cast<size_t>(emittedIndex - smallCount)
                    : static_cast<size_t>(emittedIndex);
                auto& debris = roarImpactDebris[poolIndex];
                const float baseAngle = DirectX::XM_2PI * (static_cast<float>(index) / static_cast<float>(count)) + phaseOffset;
                const float angle = baseAngle + MathHelper::RandomRange(-angleJitterRadians, angleJitterRadians);
                const float distance = attackRadius * std::clamp(ringRatio + MathHelper::RandomRange(-radiusJitter, radiusJitter), 0.0f, 1.0f);
                const DirectX::XMFLOAT3 outward{ sinf(angle), 0.0f, cosf(angle) };
                const float horizontalSpeed = isBlock ? MathHelper::RandomRange(roarImpactDebrisBlockHorizontalSpeedMin, roarImpactDebrisBlockHorizontalSpeedMax) : MathHelper::RandomRange(roarImpactDebrisSmallHorizontalSpeedMin, roarImpactDebrisSmallHorizontalSpeedMax);
                const float verticalSpeed = isBlock ? MathHelper::RandomRange(roarImpactDebrisBlockVerticalSpeedMin, roarImpactDebrisBlockVerticalSpeedMax) : MathHelper::RandomRange(roarImpactDebrisSmallVerticalSpeedMin, roarImpactDebrisSmallVerticalSpeedMax);
                const float angularMin = isBlock ? 150.0f : 240.0f;
                const float angularMax = isBlock ? 330.0f : 540.0f;
                const auto randomAngularVelocity = [angularMin, angularMax]()
                    {
                        return (MathHelper::RandomRange(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f) * MathHelper::RandomRange(angularMin, angularMax);
                    };
                debris.active = true;
                debris.position = { center.x + outward.x * distance, center.y + spawnHeight, center.z + outward.z * distance };
                debris.velocity = MathHelper::Multiply(outward, horizontalSpeed);
                debris.velocity.y = verticalSpeed;
                debris.rotation = { MathHelper::RandomRange(0.0f, 360.0f), MathHelper::RandomRange(0.0f, 360.0f), MathHelper::RandomRange(0.0f, 360.0f) };
                debris.angularVelocity = { randomAngularVelocity(), randomAngularVelocity(), randomAngularVelocity() };
                debris.lifetime = isBlock ? MathHelper::RandomRange(roarImpactDebrisBlockLifetimeMin, roarImpactDebrisBlockLifetimeMax) : MathHelper::RandomRange(roarImpactDebrisSmallLifetimeMin, roarImpactDebrisSmallLifetimeMax);
                debris.elapsedTime = 0.0f;
                debris.scale = isBlock ? MathHelper::RandomRange(roarImpactDebrisBlockScaleMin, roarImpactDebrisBlockScaleMax) : MathHelper::RandomRange(roarImpactDebrisSmallScaleMin, roarImpactDebrisSmallScaleMax);
                debris.meshComponent->SetRelativeLocationDirect(debris.position);
                debris.meshComponent->SetRelativeEulerRotationDirect(debris.rotation);
                debris.meshComponent->SetRelativeScaleDirect({ debris.scale, debris.scale, debris.scale });
                debris.meshComponent->SetIsVisible(true);
                ++emittedIndex;
            }
        };
    emitRing(innerCount, innerRatio, 0.0f);
    emitRing(outerCount, outerRatio, DirectX::XM_PI / static_cast<float>(outerCount));
}

void GruxEnemy::UpdateRoarImpactDebris(float scaledDeltaTime)
{
    if (!roarImpactDebrisEnabled)
    {
        HideRoarImpactDebris();
        return;
    }
    const float dt = (std::max)(0.0f, scaledDeltaTime);
    const float gravity = (std::max)(0.0f, roarImpactDebrisGravity);
    for (auto& debris : roarImpactDebris)
    {
        if (!debris.active || !debris.meshComponent)
            continue;
        debris.velocity.y -= gravity * dt;
        debris.position.x += debris.velocity.x * dt;
        debris.position.y += debris.velocity.y * dt;
        debris.position.z += debris.velocity.z * dt;
        debris.rotation.x += debris.angularVelocity.x * dt;
        debris.rotation.y += debris.angularVelocity.y * dt;
        debris.rotation.z += debris.angularVelocity.z * dt;
        debris.elapsedTime += dt;
        if (debris.elapsedTime >= debris.lifetime)
        {
            debris.active = false;
            debris.meshComponent->SetIsVisible(false);
            continue;
        }
        debris.meshComponent->SetRelativeLocationDirect(debris.position);
        debris.meshComponent->SetRelativeEulerRotationDirect(debris.rotation);
    }
}

void GruxEnemy::HideRoarImpactDebris()
{
    for (auto& debris : roarImpactDebris)
    {
        debris.active = false;
        debris.elapsedTime = 0.0f;
        debris.lifetime = 0.0f;
        if (debris.meshComponent)
            debris.meshComponent->SetIsVisible(false);
    }
}

// Roar浮遊破片
void GruxEnemy::EnsureRoarFloatingDebris()
{
    constexpr const char* debrisModels[] = {
        "./Data/Models/EffectDebri/SmallDebri/debri.gltf",
        "./Data/Models/EffectDebri/SmallDebri/debri1.gltf",
        "./Data/Models/EffectDebri/SmallDebri/debri2.gltf",
    };

    for (size_t index = 0; index < roarFloatingDebris.size(); ++index)
    {
        auto& debris = roarFloatingDebris[index];
        if (!debris.meshComponent)
        {
            debris.meshComponent = AddComponent<SkeletalMeshComponent>(
                "RoarFloatingDebris_" + std::to_string(index), "GruxEnemy");
            debris.meshComponent->SetModel(debrisModels[index % std::size(debrisModels)]);
            debris.meshComponent->SetIsCastShadow(false);
            debris.meshComponent->SetUsingAbsoluteLocation(true);
            debris.meshComponent->SetUsingAbsoluteRotation(true);
            debris.meshComponent->SetUsingAbsoluteScale(true);
        }
        debris.meshComponent->SetIsVisible(false);
    }
}

void GruxEnemy::InitializeRoarFloatingDebris()
{
    EnsureRoarFloatingDebris();
    if (!roarFloatingDebrisEnabled)
    {
        HideRoarFloatingDebris();
        return;
    }

    const float innerRatio = std::clamp(roarFloatingDebrisInnerRatio, 0.0f, 1.0f);
    const float outerRatio = std::clamp(roarFloatingDebrisOuterRatio, innerRatio, 1.0f);
    const float minHeight = (std::max)(0.0f, roarFloatingDebrisMinHeight);
    const float maxHeight = (std::max)(minHeight, roarFloatingDebrisMaxHeight);
    const float minScale = (std::max)(0.0f, roarFloatingDebrisScaleMin);
    const float maxScale = (std::max)(minScale, roarFloatingDebrisScaleMax);
    const float thresholdStart = 0.22f;
    const float thresholdEnd = 0.84f;
    const float thresholdStep = roarFloatingDebris.size() > 1
        ? (thresholdEnd - thresholdStart) / static_cast<float>(roarFloatingDebris.size() - 1)
        : 0.0f;

    for (size_t index = 0; index < roarFloatingDebris.size(); ++index)
    {
        auto& debris = roarFloatingDebris[index];
        const float angle = MathHelper::RandomRange(0.0f, DirectX::XM_2PI);
        debris.normalizedDirection = { std::sin(angle), 0.0f, std::cos(angle) };
        debris.normalizedRingDistance = MathHelper::RandomRange(innerRatio, outerRatio);
        debris.spawnThreshold = std::clamp(thresholdStart + thresholdStep * static_cast<float>(index) + MathHelper::RandomRange(-0.025f, 0.025f), 0.20f, 0.90f);
        debris.baseScale = MathHelper::RandomRange(minScale, maxScale);
        debris.maxFloatHeight = MathHelper::RandomRange(minHeight, maxHeight);
        debris.baseRotation =
        {
            MathHelper::RandomRange(0.0f, 360.0f),
            MathHelper::RandomRange(0.0f, 360.0f),
            MathHelper::RandomRange(0.0f, 360.0f)
        };
        debris.rotationAmount =
        {
            MathHelper::RandomRange(-25.0f, 25.0f),
            MathHelper::RandomRange(-40.0f, 40.0f),
            MathHelper::RandomRange(-25.0f, 25.0f)
        };
        debris.meshComponent->SetIsVisible(false);
    }
}

void GruxEnemy::UpdateRoarFloatingDebris()
{
    if (!roarFloatingDebrisEnabled || !IsRoarBTActive() || roarBT.shockwaveFired)
    {
        HideRoarFloatingDebris();
        return;
    }

    const DirectX::XMFLOAT3 center = GetPosition();
    const float radius = GetRoarAttackRadius();
    for (auto& debris : roarFloatingDebris)
    {
        if (!debris.meshComponent || roarTelegraphProgress < debris.spawnThreshold)
        {
            if (debris.meshComponent) debris.meshComponent->SetIsVisible(false);
            continue;
        }

        const float localProgress = std::clamp(
            (roarTelegraphProgress - debris.spawnThreshold) /
            (1.0f - debris.spawnThreshold), 0.0f, 1.0f);
        const float easedProgress = localProgress * localProgress * (3.0f - 2.0f * localProgress);
        const float ringDistance = radius * debris.normalizedRingDistance;
        const DirectX::XMFLOAT3 position{
            center.x + debris.normalizedDirection.x * ringDistance,
            center.y + 0.05f + debris.maxFloatHeight * easedProgress,
            center.z + debris.normalizedDirection.z * ringDistance };
        const DirectX::XMFLOAT3 rotation{
            debris.baseRotation.x + debris.rotationAmount.x * easedProgress,
            debris.baseRotation.y + debris.rotationAmount.y * easedProgress,
            debris.baseRotation.z + debris.rotationAmount.z * easedProgress };

        debris.meshComponent->SetRelativeLocationDirect(position);
        debris.meshComponent->SetRelativeEulerRotationDirect(rotation);
        debris.meshComponent->SetRelativeScaleDirect(
            { debris.baseScale, debris.baseScale, debris.baseScale });
        debris.meshComponent->SetIsVisible(true);
    }
}

void GruxEnemy::HideRoarFloatingDebris()
{
    for (auto& debris : roarFloatingDebris)
    {
        if (debris.meshComponent)
            debris.meshComponent->SetIsVisible(false);
    }
}

// Roar Cleanup・ライフサイクル
void GruxEnemy::CleanupRoarBT()
{
    // Roar固有のRuntime、演出、当たり判定を後始末する。
    HideRoarTelegraph();
    HideRoarFloatingDebris();
    HideRoarImpactDebris();
    roarTelegraphProgress = 0.0f;
    if (IsRoarBTActive())
    {
        StopAIMovement();
        if (characterMovementComponent) characterMovementComponent->ResetFixedSpeed();
        DisableAttackHitBoxes();
        const auto controller = GetBodyAnimationController();
        if (IsRoarExecutionAllowed() && controller &&
            (controller->GetCurrentAnimationName() == "Pre_Stampede_0" ||
                controller->GetCurrentAnimationName() == "LevelStart_0"))
            PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
    }
    roarBT = {};
}

void GruxEnemy::TickRoarLifecycle(float dt)
{
    if (!IsAnimationEditorPreviewActive())
    {
        roarCooldownRemaining = (std::max)(0.0f, roarCooldownRemaining - (std::max)(0.0f, dt));
        retreatRetryCooldownRemaining = (std::max)(0.0f, retreatRetryCooldownRemaining - (std::max)(0.0f, dt));
    }
    if (IsRoarBTActive() && (!IsRoarExecutionAllowed() || !activeNode ||
        (activeNode->GetName() != "ExecuteRoar" && activeNode->GetName() != "FinishRoar")))
    {
        CleanupRoarBT();
        activeNode = nullptr;
        if (behaviorData) behaviorData->Init();
    }
}

float GruxEnemy::GetRoarRenderFootOffset() const
{
    const auto controller = GetBodyAnimationController();
    if (roarBT.stage != RoarStage::Shockwave || !IsRoarExecutionAllowed() ||
        !activeNode || (activeNode->GetName() != "ExecuteRoar" && activeNode->GetName() != "FinishRoar") ||
        !controller || !controller->IsPlayAnimation() || controller->GetCurrentAnimationName() != "LevelStart_0" ||
        !std::isfinite(roarLevelStartFootOffset) || !std::isfinite(roarLevelStartFootOffsetEndTime) ||
        roarLevelStartFootOffsetEndTime <= 0.0f)
        return 0.0f;
    const float time = controller->GetCurrentAnimationTime();
    if (!std::isfinite(time)) return 0.0f;
    return roarLevelStartFootOffset * (1.0f - std::clamp(time / roarLevelStartFootOffsetEndTime, 0.0f, 1.0f));
}
