#include "pch.h"
#include "GruxRoarBT.h"
#include "NodeBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Scenes/GameScene.h"
#include "Game/State/StateMachine.h"
#include "Engine/Scene/Scene.h"
#include "Core/ActorManager.h"
#include "Physics/CollisionFunction.h"

namespace
{
    bool IsOtherBTActionForRoar(NodeBase* node)
    {
        // Idle can advertise eligibility, but the scheduler still waits for its
        // Complete before inferring Root. Active attacks/recovery remain blocked.
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
        shape.center.y += component.GetCollisionOffsetY();
        shape.capsule.r = component.GetRadius();
        shape.halfHeight = (std::max)(component.GetHeight() * 0.5f - shape.capsule.r, 0.0f);
        shape.isSphere = shape.halfHeight <= FLT_EPSILON;
        shape.capsule.a = shape.center;
        shape.capsule.b = shape.center;
        shape.capsule.a.y += shape.halfHeight;
        shape.capsule.b.y -= shape.halfHeight;
        return shape;
    }

    bool SweepRetreatPath(const RetreatSweepShape& shape, const DirectX::XMFLOAT3& direction,
        float distance, HitResult& result, uint32_t obstacleMask)
    {
        PositioningSweepShape queryShape{};
        queryShape.type = shape.isSphere ? PositioningSweepShape::Type::Sphere
            : PositioningSweepShape::Type::Capsule;
        queryShape.center = shape.center;
        queryShape.point1 = shape.capsule.a;
        queryShape.point2 = shape.capsule.b;
        queryShape.radius = shape.capsule.r;

        PositioningPathSafetyPolicy policy{};
        policy.floorNormalThreshold = 0.7f;
        policy.supportPenetrationTolerance = 0.30f;
        PositioningPathSafetyResult pathResult{};
        const bool pathBlocked = Physics::Instance().SweepPositioningPath(queryShape, direction,
            distance, obstacleMask, policy, pathResult);

        if (pathResult.blockingHit.hit)
        {
            result.position = pathResult.blockingHit.position;
            result.normal = pathResult.blockingHit.normal;
            result.distance = pathResult.blockingHit.distance;
        }

        return pathBlocked;
    }
}
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
    owner->CleanupRoarBT(complete ? "Complete" : "Failed");
    return complete ? State::Complete : State::Failed;
}

bool GruxEnemy::IsRoarExecutionAllowed() const
{
    if (!behaviorTreeFastComboEnabled || !battleAIActive || IsDead() || IsPendingKill() ||
        isDeathPerform ||
        finalHitReactionActive || finalHitReactionHeld || IsAnimationEditorPreviewActive() ||
        IsChargeAttackBTActive() || IsDashAttackBTActive() || chargeMovementActive ||
        dashAttackMovementActive || jumpMotionWarpOverrideActive || !characterMovementComponent ||
        !GetBodyAnimationController() || !GetOwnerScene())
        return false;
    if (stateMachine_)
    {
        const std::string state = stateMachine_->GetStateName();
        // BT mode suspends the legacy FSM; its last Attack state is not activity.
        // Death/Stun still serve as external interruption guards.
        if (state == "EnemyDeathState" || state == "EnemyStunState")
            return false;
    }
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
    return EvaluateRoarPlanForDebug();
}

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
    retreatRuntime = {};
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
    retreatRuntime.playerSnapshot = player->GetPosition();
    retreatRuntime.gruxSnapshot = GetPosition();
    float awayX = retreatRuntime.gruxSnapshot.x - retreatRuntime.playerSnapshot.x;
    float awayZ = retreatRuntime.gruxSnapshot.z - retreatRuntime.playerSnapshot.z;
    retreatRuntime.startingPlayerDistance = std::sqrt(awayX * awayX + awayZ * awayZ);
    if (retreatRuntime.startingPlayerDistance > FLT_EPSILON)
    {
        awayX /= retreatRuntime.startingPlayerDistance;
        awayZ /= retreatRuntime.startingPlayerDistance;
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
            const DirectX::XMFLOAT3 desired{
                retreatRuntime.gruxSnapshot.x + directionX * distance,
                retreatRuntime.gruxSnapshot.y,
                retreatRuntime.gruxSnapshot.z + directionZ * distance };
            RepositionTargetEvaluation evaluation{};
            EvaluateClampedPositioningTarget(retreatRuntime.gruxSnapshot, desired, evaluation);
            const float moveX = evaluation.clampedTarget.x - retreatRuntime.gruxSnapshot.x;
            const float moveZ = evaluation.clampedTarget.z - retreatRuntime.gruxSnapshot.z;
            const float moveDistance = std::sqrt(moveX * moveX + moveZ * moveZ);
            const float playerX = evaluation.clampedTarget.x - retreatRuntime.playerSnapshot.x;
            const float playerZ = evaluation.clampedTarget.z - retreatRuntime.playerSnapshot.z;
            const float plannedPlayerDistance = std::sqrt(playerX * playerX + playerZ * playerZ);
            if (evaluation.clampDistance > attackSetupClampTolerance)
                continue;
            if (plannedPlayerDistance <= retreatRuntime.startingPlayerDistance + 0.01f)
                continue;
            if (moveDistance < retreatMinimumMoveDistance)
                continue;
            if (evaluation.bossRoomPathBlocked)
                continue;
            const DirectX::XMFLOAT3 moveDirection{ moveX / moveDistance, 0.0f, moveZ / moveDistance };
            HitResult hit{};
            if (SweepRetreatPath(retreatSweepShape, moveDirection, moveDistance, hit, obstacleMask))
                continue;
            retreatTarget = {};
            retreatTarget.valid = true;
            retreatTarget.targetPosition = evaluation.clampedTarget;
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
    retreatRetryCooldownRemaining = retreatRetryCooldownDuration;
    return false;
}

GruxEnemy::PositioningMoveResult GruxEnemy::UpdateRetreatMovement(float deltaTime)
{
    if (!IsRoarExecutionAllowed() || !retreatTarget.valid || !enemyCapsuleComponent)
        return PositioningMoveResult::InvalidTarget;
    const auto position = GetPosition();
    const float dx = retreatTarget.targetPosition.x - position.x;
    const float dz = retreatTarget.targetPosition.z - position.z;
    const float remaining = std::sqrt(dx * dx + dz * dz);
    if (remaining > retreatTarget.arrivalTolerance)
    {
        const RetreatSweepShape retreatSweepShape = BuildRetreatSweepShape(*enemyCapsuleComponent);
        const DirectX::XMFLOAT3 direction{ dx / remaining, 0.0f, dz / remaining };
        const uint32_t obstacleMask = CollisionHelper::ToBit(CollisionLayer::WorldStatic) |
            CollisionHelper::ToBit(CollisionLayer::WorldProps) |
            CollisionHelper::ToBit(CollisionLayer::Convex);
        HitResult hit{};
        if (SweepRetreatPath(retreatSweepShape, direction, remaining, hit, obstacleMask))
        {
            return PositioningMoveResult::Stuck;
        }
    }
    const auto result = UpdatePositioningTargetMovement(retreatTarget, retreatMovementRuntime,deltaTime);
    return result;
}

void GruxEnemy::FinishRetreatMovement(bool arrived)
{
    StopAIMovement();
    EndPositioningAnimation();
    ClearPositioningTarget(retreatTarget, retreatMovementRuntime);
    if (arrived)
        consecutiveAttackCount = 0;
    retreatRetryCooldownRemaining = retreatRetryCooldownDuration;
}

void GruxEnemy::BeginCombatDecisionInference()
{
    repositionDecisionCached = true;
    repositionDecisionResult = false;
    repositionDecisionRoll = 0.0f;
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
        BeginCombatDecisionInference();
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
    const bool canPlanAttack = playerAvailableForAttack &&
        (canPlanFastCombo || canPlanJump || canPlanDash || canPlanCharge);
    const bool canPlan = CanPlanReposition() || canPlanAttack;
    return canPlan;
}

bool GruxEnemy::CanPlanReposition()
{
    const bool initialReposition = CanPlanInitialReposition();
    const bool decisionAllows = initialReposition || (repositionDecisionCached && repositionDecisionResult);
    const bool targetInactive = !repositionTarget.valid;
    const bool movementInactive = !repositionMovementRuntime.movementActive;
    const bool cooldownReady = repositionCooldownRemaining <= 0.0f;
    const bool retryReady = repositionRetryCooldownRemaining <= 0.0f;
    // When the Player cannot act, Defensive is suppressed but Reposition remains allowed.
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
    repositionRuntime = {};
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

    // Reposition intentionally shares Retreat's fixed-target candidate policy.
    // Only its activation conditions and distance/cooldown values differ.
    repositionRuntime.playerSnapshot = player->GetPosition();
    repositionRuntime.gruxSnapshot = GetPosition();
    float awayX = repositionRuntime.gruxSnapshot.x - repositionRuntime.playerSnapshot.x;
    float awayZ = repositionRuntime.gruxSnapshot.z - repositionRuntime.playerSnapshot.z;
    repositionRuntime.startingPlayerDistance = std::sqrt(awayX * awayX + awayZ * awayZ);
    if (repositionRuntime.startingPlayerDistance > FLT_EPSILON)
    {
        awayX /= repositionRuntime.startingPlayerDistance;
        awayZ /= repositionRuntime.startingPlayerDistance;
    }
    else
    {
        const auto forward = GetForward();
        awayX = forward.x;
        awayZ = forward.z;
    }

    const float minDistance = (std::max)(0.0f, repositionDistanceMin);
    const float maxDistance = (std::max)(minDistance, repositionDistanceMax);
    const std::array<float, 3> distances{ maxDistance, (minDistance + maxDistance) * 0.5f, minDistance };
    const uint32_t obstacleMask = CollisionHelper::ToBit(CollisionLayer::WorldStatic) |
        CollisionHelper::ToBit(CollisionLayer::WorldProps) |
        CollisionHelper::ToBit(CollisionLayer::Convex);
    const RetreatSweepShape sweepShape = BuildRetreatSweepShape(*enemyCapsuleComponent);

    struct SafeRepositionCandidate
    {
        DirectX::XMFLOAT3 position{};
        float moveDistance = 0.0f;
        float plannedPlayerDistance = 0.0f;
    };
    std::vector<SafeRepositionCandidate> safeCandidates;
    safeCandidates.reserve(distances.size() * 13);

    for (const float distance : distances)
    {
        for (int index = 0; index <= 12; ++index)
        {
            const float magnitude = static_cast<float>((index + 1) / 2);
            const float signedStep = index == 0 ? 0.0f : (index % 2 == 1 ? magnitude : -magnitude);
            const float angleDegrees = signedStep * 30.0f;
            const float angle = DirectX::XMConvertToRadians(angleDegrees);
            const float directionX = awayX * std::cos(angle) - awayZ * std::sin(angle);
            const float directionZ = awayX * std::sin(angle) + awayZ * std::cos(angle);
            const DirectX::XMFLOAT3 desired{
                repositionRuntime.gruxSnapshot.x + directionX * distance,
                repositionRuntime.gruxSnapshot.y,
                repositionRuntime.gruxSnapshot.z + directionZ * distance };

            RepositionTargetEvaluation evaluation{};
            EvaluateClampedPositioningTarget(repositionRuntime.gruxSnapshot, desired, evaluation);
            const float moveX = evaluation.clampedTarget.x - repositionRuntime.gruxSnapshot.x;
            const float moveZ = evaluation.clampedTarget.z - repositionRuntime.gruxSnapshot.z;
            const float moveDistance = std::sqrt(moveX * moveX + moveZ * moveZ);
            const float playerX = evaluation.clampedTarget.x - repositionRuntime.playerSnapshot.x;
            const float playerZ = evaluation.clampedTarget.z - repositionRuntime.playerSnapshot.z;
            const float plannedPlayerDistance = std::sqrt(playerX * playerX + playerZ * playerZ);

            if (plannedPlayerDistance < repositionMinimumPlayerDistance)
                continue;
            if (moveDistance < repositionMinimumMoveDistance)
                continue;
            if (evaluation.bossRoomPathBlocked)
                continue;
            const DirectX::XMFLOAT3 moveDirection{ moveX / moveDistance, 0.0f, moveZ / moveDistance };
            HitResult hit{};
            if (SweepRetreatPath(sweepShape, moveDirection, moveDistance, hit, obstacleMask))
                continue;
            safeCandidates.push_back({ evaluation.clampedTarget, moveDistance, plannedPlayerDistance });
        }
    }

    if (!safeCandidates.empty())
    {
        static thread_local std::mt19937 randomEngine{ std::random_device{}() };
        std::uniform_int_distribution<size_t> distribution(0, safeCandidates.size() - 1);
        const auto& selected = safeCandidates[distribution(randomEngine)];
        repositionTarget = {};
        repositionTarget.valid = true;
        repositionTarget.targetPosition = selected.position;
        repositionTarget.arrivalTolerance = 0.3f;
        repositionTarget.timeout = 3.0f;
        repositionTarget.maxMoveDistance = selected.moveDistance + 1.0f;
        repositionTarget.moveSpeed = repositionMoveSpeed;
        repositionTarget.stuckMovementThreshold = 0.01f;
        repositionTarget.stuckTimeThreshold = 0.5f;

        return true;
    }

    ClearPositioningTarget(repositionTarget, repositionMovementRuntime);
    if (!IsInitialRepositionPending())
        repositionRetryCooldownRemaining = repositionRetryCooldownDuration;

    return false;
}
GruxEnemy::PositioningMoveResult GruxEnemy::UpdateRepositionMovement(float dt)
{
    if (!repositionTarget.valid)
        return PositioningMoveResult::InvalidTarget;
    const auto result = UpdatePositioningTargetMovement(repositionTarget, repositionMovementRuntime, dt);
    return result;
}
void GruxEnemy::FinishRepositionMovement(bool arrived)
{
    // Arrival must stop positioning immediately.  The success cooldown is
    // deliberately deferred until WaitAfterReposition completes.
    StopAIMovement();
    EndPositioningAnimation();
    ClearPositioningTarget(repositionTarget, repositionMovementRuntime);
    if (!arrived && !IsInitialRepositionPending())
        repositionRetryCooldownRemaining = repositionRetryCooldownDuration;
}
void GruxEnemy::BeginInitialRepositionFallback()
{
    initialRepositionPending = false;
    initialRepositionFallbackIdlePending = true;
    repositionDecisionCached = false;
    repositionDecisionResult = false;
    repositionDecisionRoll = 0.0f;
}
void GruxEnemy::CompleteInitialRepositionFallbackIdle()
{
    initialRepositionFallbackIdlePending = false;
    repositionDecisionCached = false;
    repositionDecisionResult = false;
    repositionDecisionRoll = 0.0f;
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
        repositionDecisionRoll = 0.0f;
    }
}

bool GruxEnemy::EvaluateRoarPlanForDebug() const
{
    if (!IsRoarExecutionAllowed() || IsRoarBTActive() || roarCooldownRemaining > 0.0f ||
        IsOtherBTActionForRoar(activeNode))
        return false;
    const auto context = BuildTargetContext();
    return IsDefensiveBackForBehavior(context) &&
        context.xzDistance <= defensiveTooCloseDistance;
}

bool GruxEnemy::BeginRoarBT()
{
    if (!CanPlanRoar()) { roarBTStatus = "Start rejected"; return false; }
    const auto controller = GetBodyAnimationController();
    if (!std::isfinite(roarPreStampedeStartTime) || !std::isfinite(roarPreStampedeEndTime) ||
        roarPreStampedeStartTime < 0.0f || roarPreStampedeEndTime <= roarPreStampedeStartTime ||
        controller->GetAnimationLength("Pre_Stampede_0") < roarPreStampedeEndTime)
    { roarBTStatus = "Invalid playback range"; return false; }
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
        CleanupRoarBT("Invalid playback range");
        return false;
    }
    roarBT.stage = RoarStage::Telegraph;
    roarBT.previousTime = roarPreStampedeStartTime;
    roarBT.endTime = roarPreStampedeEndTime;
    ShowRoarTelegraph();
    InitializeRoarFloatingDebris();
    UpdateRoarFloatingDebris();
    roarBTStatus = "Running";
    roarCooldownRemaining = roarCooldownDuration;
    return true;
}

// 0 = Running, 1 = Complete, -1 = Failed. Only this action advances stages.
int GruxEnemy::UpdateRoarBT(float dt)
{
    auto fail = [this](const char* reason) { CleanupRoarBT(reason); return -1; };
    if (!IsRoarBTActive() || !IsRoarExecutionAllowed()) return fail("Interrupted");
    const auto controller = GetBodyAnimationController();
    const char* expected = roarBT.stage == RoarStage::Telegraph ? "Pre_Stampede_0" : "LevelStart_0";
    if (controller->GetCurrentAnimationName() != expected) return fail("Animation replaced");
    const float time = controller->GetCurrentAnimationTime();
    const float safeDt = (std::max)(0.0f, dt);
    roarBT.elapsed += safeDt;
    roarBT.stalled = time > roarBT.previousTime + 0.00001f ? 0.0f : roarBT.stalled + safeDt;
    roarBT.previousTime = time;
    if (roarBT.stalled > 2.0f || roarBT.elapsed > 60.0f) return fail("Playback timeout");
    StopAIMovement();
    UpdateRoarTelegraphProgress();
    UpdateRoarTelegraphTransform();
    UpdateRoarFloatingDebris();
    if (controller->IsPlayAnimation()) return 0;
    // A stopped clip is only a successful completion at its expected end.
    if (time + 0.02f < roarBT.endTime) return fail("Animation stopped early");
    if (roarBT.stage == RoarStage::Telegraph)
    {
        PlayBodyAnimation("LevelStart_0", false, true, 0.15f, true);
        roarBT.stage = RoarStage::Shockwave;
        roarBT.previousTime = 0.0f;
        roarBT.stalled = 0.0f;
        roarBT.endTime = controller->GetAnimationLength("LevelStart_0");
        if (roarBT.endTime < 0.30f) return fail("Invalid shockwave clip");
        return 0;
    }
    if (!roarBT.shockwaveFired) return fail("Missing RoarShockwave notify");
    return 1;
}

void GruxEnemy::ApplyRoarShockwave()
{
    const auto controller = GetBodyAnimationController();
    if (roarBT.stage != RoarStage::Shockwave || roarBT.shockwaveFired ||
        !IsRoarExecutionAllowed() || !controller || controller->GetCurrentAnimationName() != "LevelStart_0")
        return;
    // Consume the event even on a miss: moving into the radius later must not hit.
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
    if (!player || roarBT.hitPlayer) return;
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
    if (!player->TryTakeDamage(
        roarDamage, GetPosition(), Player::DamagePolicy::IgnoreDodgeIFrames))
    {
        return;
    }

    roarBT.hitPlayer = player->StartKnockBack(direction);
}

float GruxEnemy::GetRoarAttackRadius() const
{
    const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
    return scene && scene->IsBossInFinalPhase()
        ? roarRadiusPhase2
        : roarRadiusPhase1;
}

void GruxEnemy::EnsureRoarTelegraphMeshes()
{
    if (!roarTelegraphOuterMeshComponent)
        roarTelegraphOuterMeshComponent = AddComponent<StaticMeshComponent>("roarTelegraphOuter", "GruxEnemy");
    if (!roarTelegraphFillMeshComponent)
        roarTelegraphFillMeshComponent = AddComponent<StaticMeshComponent>("roarTelegraphFill", "GruxEnemy");

    const auto configureMesh = [](const std::shared_ptr<StaticMeshComponent>& mesh,
        const char* modelPath, const DirectX::XMFLOAT4& color)
    {
        if (!mesh->model)
            mesh->SetModel(modelPath);
        if (mesh->model)
            for (auto& material : mesh->model->materials)
                material.data.alphaMode = 2; // BLEND
        mesh->overrideDeferredPipelineName = "chargeTelegraphUnlitForward";
        mesh->overrideForwardPipelineName = "chargeTelegraphUnlitForward";
        mesh->SetIsCastShadow(false);
        mesh->SetIsVisible(false);
        mesh->SetUsingAbsoluteLocation(true);
        mesh->SetUsingAbsoluteRotation(true);
        mesh->SetUsingAbsoluteScale(true);
        mesh->plusAlphaCBuffer->data.cpuColor = color;
        mesh->plusAlphaCBuffer->data.emissionPower = 0.0f;
        mesh->plusAlphaCBuffer->data.objectType = ObjectType::NoLighting;
    };

    configureMesh(roarTelegraphOuterMeshComponent,
        "./Data/Models/EffectModel/RoarTelegraphOuter1.glb", { 1.0f, 0.16f, 0.03f, 1.0f });
    configureMesh(roarTelegraphFillMeshComponent,
        "./Data/Models/EffectModel/RoarTelegraphFill.glb", { 1.0f, 0.08f, 0.03f, 1.0f });

    // Roar owns both visual layers through its dedicated tint / glow shader.
    // Charge / Jump / Dash remain on the shared Telegraph shader.
    roarTelegraphOuterMeshComponent->overrideDeferredPipelineName = "roarTelegraphUnlitForward";
    roarTelegraphOuterMeshComponent->overrideForwardPipelineName = "roarTelegraphUnlitForward";
    roarTelegraphFillMeshComponent->overrideDeferredPipelineName = "roarTelegraphUnlitForward";
    roarTelegraphFillMeshComponent->overrideForwardPipelineName = "roarTelegraphUnlitForward";
}

void GruxEnemy::ShowRoarTelegraph()
{
    EnsureRoarTelegraphMeshes();
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
    roarTelegraphOuterWorldPosition = { position.x, roarTelegraphOuterYOffset, position.z };
    roarTelegraphFillWorldPosition = { position.x, roarTelegraphFillYOffset, position.z };
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
    roarTelegraphVisualProgress = t * t * (3.0f - 2.0f * t);
    const auto lerp = [this](float minimum, float maximum)
    {
        return minimum + (maximum - minimum) * roarTelegraphVisualProgress;
    };
    roarTelegraphFillTintCurrent = {
        lerp(fillTintMin.x, fillTintMax.x),
        lerp(fillTintMin.y, fillTintMax.y),
        lerp(fillTintMin.z, fillTintMax.z) };
    roarTelegraphFillGlowCurrent = lerp(roarTelegraphFillGlowMin, roarTelegraphFillGlowMax);
    roarTelegraphFillAlphaCurrent = lerp(roarTelegraphFillAlphaMin, roarTelegraphFillAlphaMax);

    roarTelegraphOuterMeshComponent->SetRelativeLocationDirect(roarTelegraphOuterWorldPosition);
    roarTelegraphOuterMeshComponent->SetRelativeRotationDirect(identityRotation);
    roarTelegraphOuterMeshComponent->SetRelativeScaleDirect(scale);
    roarTelegraphOuterMeshComponent->plusAlphaCBuffer->data.brightness =
        std::clamp(roarTelegraphOuterAlpha, 0.0f, 1.0f) - 1.0f;
    roarTelegraphOuterMeshComponent->plusAlphaCBuffer->data.cpuColor = {
        outerTint.x, outerTint.y, outerTint.z, 1.0f };
    roarTelegraphOuterMeshComponent->plusAlphaCBuffer->data.emissionPower = 1.0f;

    roarTelegraphFillMeshComponent->SetRelativeLocationDirect(roarTelegraphFillWorldPosition);
    roarTelegraphFillMeshComponent->SetRelativeRotationDirect(identityRotation);
    roarTelegraphFillMeshComponent->SetRelativeScaleDirect(scale);
    roarTelegraphFillMeshComponent->plusAlphaCBuffer->data.brightness =
        std::clamp(roarTelegraphFillAlphaCurrent, 0.0f, 1.0f) - 1.0f;
    roarTelegraphFillMeshComponent->plusAlphaCBuffer->data.cpuColor = {
        roarTelegraphFillTintCurrent.x, roarTelegraphFillTintCurrent.y, roarTelegraphFillTintCurrent.z, 1.0f };
    roarTelegraphFillMeshComponent->plusAlphaCBuffer->data.emissionPower =
        (std::max)(0.0f, roarTelegraphFillGlowCurrent);
}

void GruxEnemy::UpdateRoarTelegraphProgress()
{
    const auto controller = GetBodyAnimationController();
    if (!controller || roarBT.stage == RoarStage::None)
        return;

    // The two sections are weighted by their Animation Timeline spans, not
    // world time.  Speed Curve therefore changes how quickly the playhead
    // reaches each value, while preserving pose-aligned progress.
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

void GruxEnemy::SpawnRoarGroundBurst()
{
    roarGroundBurstLastSpawnCount = 0;
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
            ++roarGroundBurstLastSpawnCount;
        }
    };

    emitRing(innerCount, innerRatio, 0.0f);
    emitRing(outerCount, outerRatio, DirectX::XM_PI / static_cast<float>(outerCount));
}

void GruxEnemy::EnsureRoarImpactDebris()
{
    constexpr const char* smallModelPaths[] = {
        "./Data/Models/EffectDebri/SmallDebri/debri.gltf",
        "./Data/Models/EffectDebri/SmallDebri/debri1.gltf",
        "./Data/Models/EffectDebri/SmallDebri/debri2.gltf",
    };
    constexpr const char* blockModelPaths[] = {
        "./Data/Models/EffectDebri/Block/block1.gltf",
        "./Data/Models/EffectDebri/Block/block2.gltf",
        "./Data/Models/EffectDebri/Block/block3.gltf",
        "./Data/Models/EffectDebri/Block/block4.gltf",
    };
    for (size_t index = 0; index < roarImpactDebris.size(); ++index)
    {
        auto& debris = roarImpactDebris[index];
        debris.isBlock = index >= 12;
        if (!debris.meshComponent)
        {
            const char* modelPath = debris.isBlock
                ? blockModelPaths[(index - 12) % std::size(blockModelPaths)]
                : smallModelPaths[index % std::size(smallModelPaths)];
            debris.meshComponent = AddComponent<SkeletalMeshComponent>(
                "RoarImpactDebris_" + std::to_string(index), "GruxEnemy");
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
        debris.active = false;
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
        debris.active = false;
        const float angle = MathHelper::RandomRange(0.0f, DirectX::XM_2PI);
        debris.normalizedDirection = { std::sin(angle), 0.0f, std::cos(angle) };
        debris.normalizedRingDistance = MathHelper::RandomRange(innerRatio, outerRatio);
        debris.spawnThreshold = std::clamp(
            thresholdStart + thresholdStep * static_cast<float>(index) +
            MathHelper::RandomRange(-0.025f, 0.025f),
            0.20f, 0.90f);
        debris.baseScale = MathHelper::RandomRange(minScale, maxScale);
        debris.maxFloatHeight = MathHelper::RandomRange(minHeight, maxHeight);
        debris.baseRotation = {
            MathHelper::RandomRange(0.0f, 360.0f),
            MathHelper::RandomRange(0.0f, 360.0f),
            MathHelper::RandomRange(0.0f, 360.0f) };
        debris.rotationAmount = {
            MathHelper::RandomRange(-25.0f, 25.0f),
            MathHelper::RandomRange(-40.0f, 40.0f),
            MathHelper::RandomRange(-25.0f, 25.0f) };
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
            debris.active = false;
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
        debris.active = true;
    }
}

void GruxEnemy::HideRoarFloatingDebris()
{
    for (auto& debris : roarFloatingDebris)
    {
        debris.active = false;
        if (debris.meshComponent)
            debris.meshComponent->SetIsVisible(false);
    }
}

void GruxEnemy::CleanupRoarBT(const char* status)
{
    HideRoarTelegraph();
    HideRoarFloatingDebris();
    HideRoarImpactDebris();
    roarTelegraphProgress = 0.0f;
    roarTelegraphVisualProgress = 0.0f;
    const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
    roarTelegraphFillTintCurrent = scene && scene->IsBossInFinalPhase()
        ? roarTelegraphPhase2FillTintMin : roarTelegraphPhase1FillTintMin;
    roarTelegraphFillGlowCurrent = roarTelegraphFillGlowMin;
    roarTelegraphFillAlphaCurrent = roarTelegraphFillAlphaMin;
    if (IsRoarBTActive())
    {
        StopAIMovement();
        if (characterMovementComponent) characterMovementComponent->ResetFixedSpeed();
        DisableAttackHitBoxes();
        const auto controller = GetBodyAnimationController();
        // Do not overwrite death, stun, preview or externally owned animation.
        if (IsRoarExecutionAllowed() && controller &&
            (controller->GetCurrentAnimationName() == "Pre_Stampede_0" ||
             controller->GetCurrentAnimationName() == "LevelStart_0"))
            PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
    }
    roarBT = {};
    roarBTStatus = status;
    // Cooldown belongs to Defensive, not to the lifetime of this execution.
}

void GruxEnemy::TickRoarLifecycle(float dt)
{
    if (!IsAnimationEditorPreviewActive())
    {
        roarCooldownRemaining = (std::max)(0.0f, roarCooldownRemaining - (std::max)(0.0f, dt));
        retreatRetryCooldownRemaining = (std::max)(0.0f,
            retreatRetryCooldownRemaining - (std::max)(0.0f, dt));
    }
    if (IsRoarBTActive() && (!IsRoarExecutionAllowed() || !activeNode ||
        (activeNode->GetName() != "ExecuteRoar" && activeNode->GetName() != "FinishRoar")))
    {
        CleanupRoarBT("Interrupted");
        activeNode = nullptr;
        if (behaviorData) behaviorData->Init();
    }
}

float GruxEnemy::GetRoarRenderFootOffset() const
{
    // Query at draw time so interruption, preview and animation changes cannot
    // leave a stale offset between the animation update and a render pass.
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

void GruxEnemy::DrawRoarBTDebug()
{
#ifdef USE_IMGUI
    ImGui::SeparatorText(U8("咆哮BT"));
    ImGui::DragFloat(U8("咆哮 LevelStart 足元補正"), &roarLevelStartFootOffset, 0.01f, -2.0f, 2.0f, "%.3f m");
    ImGui::DragFloat(U8("咆哮 足元補正終了時間"), &roarLevelStartFootOffsetEndTime, 0.01f, 0.0f, 5.0f, "%.2f sec");
    if (!std::isfinite(roarLevelStartFootOffset)) roarLevelStartFootOffset = 0.0f;
    roarLevelStartFootOffsetEndTime = std::isfinite(roarLevelStartFootOffsetEndTime)
        ? (std::max)(0.0f, roarLevelStartFootOffsetEndTime) : 0.0f;
    ImGui::Text(U8("咆哮 足元補正 現在値: %.3f"), GetRoarRenderFootOffset());
    const auto footController = GetBodyAnimationController();
    ImGui::Text(U8("LevelStart 再生時間: %.3f"),
        footController && footController->GetCurrentAnimationName() == "LevelStart_0"
        ? footController->GetCurrentAnimationTime() : 0.0f);
    const auto roarController = GetBodyAnimationController();
    const float clipDuration = roarController ? roarController->GetAnimationLength("Pre_Stampede_0") : 60.0f;
    const float rangeLimit = std::isfinite(clipDuration) ? (std::max)(0.05f, clipDuration) : 60.0f;
    ImGui::DragFloat(U8("咆哮 開始時間"), &roarPreStampedeStartTime, 0.05f, 0.0f, rangeLimit - 0.05f, "%.2f sec");
    roarPreStampedeStartTime = std::isfinite(roarPreStampedeStartTime)
        ? std::clamp(roarPreStampedeStartTime, 0.0f, rangeLimit - 0.05f) : 0.0f;
    ImGui::DragFloat(U8("咆哮 終了時間"), &roarPreStampedeEndTime, 0.05f,
        roarPreStampedeStartTime + 0.05f, rangeLimit, "%.2f sec");
    roarPreStampedeEndTime = std::isfinite(roarPreStampedeEndTime)
        ? std::clamp(roarPreStampedeEndTime, roarPreStampedeStartTime + 0.05f, rangeLimit) : rangeLimit;
    ImGui::DragFloat(U8("防御行動 至近距離"), &defensiveTooCloseDistance, 0.1f, 0.0f, 30.0f, "%.2f m");
    ImGui::DragFloat(U8("防御行動 Back最小角度"), &defensiveBackMinAngle, 1.0f, 0.0f, 180.0f, "%.1f deg");
    defensiveBackMinAngle = std::clamp(defensiveBackMinAngle, 0.0f, 180.0f);
    ImGui::DragFloat("Roar Radius Phase1", &roarRadiusPhase1, 0.1f, 0.0f, 30.0f, "%.2f m");
    ImGui::DragFloat("Roar Radius Phase2", &roarRadiusPhase2, 0.1f, 0.0f, 30.0f, "%.2f m");
    ImGui::DragInt("Roar Damage", &roarDamage, 1.0f, 0, 1000);
    ImGui::DragFloat(U8("咆哮 高低差許容"), &roarHeightTolerance, 0.1f, 0.0f, 30.0f, "%.2f m");
    const float previousCooldownDuration = roarCooldownDuration;
    ImGui::DragFloat(U8("咆哮クールタイム"), &roarCooldownDuration, 0.1f, 0.0f, 120.0f, "%.2f sec");
    defensiveTooCloseDistance = (std::max)(0.0f, defensiveTooCloseDistance);
    roarRadiusPhase1 = (std::max)(0.0f, roarRadiusPhase1);
    roarRadiusPhase2 = (std::max)(0.0f, roarRadiusPhase2);
    roarDamage = (std::max)(0, roarDamage);
    roarHeightTolerance = (std::max)(0.0f, roarHeightTolerance);
    roarCooldownDuration = (std::max)(0.0f, roarCooldownDuration);
    if (roarCooldownRemaining > 0.0f && roarCooldownDuration != previousCooldownDuration)
        roarCooldownRemaining = (std::max)(0.0f,
            roarCooldownRemaining + roarCooldownDuration - previousCooldownDuration);
    const auto context = BuildTargetContext();
    const auto phaseScene = dynamic_cast<GameScene*>(GetOwnerScene());
    const bool isPhase2 = phaseScene && phaseScene->IsBossInFinalPhase();
    const char* region = !context.valid ? "None" : context.region == PlayerRelativeRegion::Back ? "Back" :
        context.region == PlayerRelativeRegion::Side ? "Side" : "Front";
    ImGui::ProgressBar(std::clamp(roarTelegraphProgress, 0.0f, 1.0f), ImVec2(-FLT_MIN, 0.0f));
    ImGui::DragFloat("Roar Telegraph Outer Alpha", &roarTelegraphOuterAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::ColorEdit3("Roar Phase 1 Outer Tint", &roarTelegraphPhase1OuterTint.x);
    ImGui::ColorEdit3("Roar Phase 1 Fill Tint Min", &roarTelegraphPhase1FillTintMin.x);
    ImGui::ColorEdit3("Roar Phase 1 Fill Tint Max", &roarTelegraphPhase1FillTintMax.x);
    ImGui::ColorEdit3("Roar Phase 2 Outer Tint", &roarTelegraphPhase2OuterTint.x);
    ImGui::ColorEdit3("Roar Phase 2 Fill Tint Min", &roarTelegraphPhase2FillTintMin.x);
    ImGui::ColorEdit3("Roar Phase 2 Fill Tint Max", &roarTelegraphPhase2FillTintMax.x);
    ImGui::DragFloat("Roar Fill Glow Min", &roarTelegraphFillGlowMin, 0.05f, 0.0f, 20.0f, "%.2f");
    ImGui::DragFloat("Roar Fill Glow Max", &roarTelegraphFillGlowMax, 0.05f, 0.0f, 20.0f, "%.2f");
    ImGui::DragFloat("Roar Fill Alpha Min", &roarTelegraphFillAlphaMin, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Roar Fill Alpha Max", &roarTelegraphFillAlphaMax, 0.01f, 0.0f, 1.0f, "%.2f");
    roarTelegraphOuterAlpha = std::clamp(roarTelegraphOuterAlpha, 0.0f, 1.0f);
    roarTelegraphFillGlowMin = (std::max)(0.0f, roarTelegraphFillGlowMin);
    roarTelegraphFillGlowMax = (std::max)(roarTelegraphFillGlowMin, roarTelegraphFillGlowMax);
    roarTelegraphFillAlphaMin = std::clamp(roarTelegraphFillAlphaMin, 0.0f, 1.0f);
    roarTelegraphFillAlphaMax = std::clamp(roarTelegraphFillAlphaMax, roarTelegraphFillAlphaMin, 1.0f);
    ImGui::Checkbox("Floating Debris Enabled", &roarFloatingDebrisEnabled);
    ImGui::DragFloat("Roar Floating Debris Inner Ratio", &roarFloatingDebrisInnerRatio, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Roar Floating Debris Outer Ratio", &roarFloatingDebrisOuterRatio, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Roar Floating Debris Min Height", &roarFloatingDebrisMinHeight, 0.01f, 0.0f, 5.0f, "%.2f m");
    ImGui::DragFloat("Roar Floating Debris Max Height", &roarFloatingDebrisMaxHeight, 0.01f, 0.0f, 5.0f, "%.2f m");
    ImGui::DragFloat("Roar Floating Debris Scale Min", &roarFloatingDebrisScaleMin, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Roar Floating Debris Scale Max", &roarFloatingDebrisScaleMax, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::Checkbox("Impact Debris Enabled", &roarImpactDebrisEnabled);
    ImGui::DragFloat("Impact Debris Spawn Height", &roarImpactDebrisSpawnHeight, 0.01f, 0.0f, 1.0f, "%.2f m");
    ImGui::DragFloat("Impact Debris Gravity", &roarImpactDebrisGravity, 0.1f, 0.0f, 30.0f, "%.2f m/s2");
    ImGui::DragFloatRange2("Impact Small Horizontal Speed", &roarImpactDebrisSmallHorizontalSpeedMin, &roarImpactDebrisSmallHorizontalSpeedMax, 0.05f, 0.0f, 20.0f, "Min: %.2f", "Max: %.2f");
    ImGui::DragFloatRange2("Impact Small Vertical Speed", &roarImpactDebrisSmallVerticalSpeedMin, &roarImpactDebrisSmallVerticalSpeedMax, 0.05f, 0.0f, 20.0f, "Min: %.2f", "Max: %.2f");
    ImGui::DragFloatRange2("Impact Small Lifetime", &roarImpactDebrisSmallLifetimeMin, &roarImpactDebrisSmallLifetimeMax, 0.01f, 0.0f, 5.0f, "Min: %.2f", "Max: %.2f");
    ImGui::DragFloatRange2("Impact Block Horizontal Speed", &roarImpactDebrisBlockHorizontalSpeedMin, &roarImpactDebrisBlockHorizontalSpeedMax, 0.05f, 0.0f, 20.0f, "Min: %.2f", "Max: %.2f");
    ImGui::DragFloatRange2("Impact Block Vertical Speed", &roarImpactDebrisBlockVerticalSpeedMin, &roarImpactDebrisBlockVerticalSpeedMax, 0.05f, 0.0f, 20.0f, "Min: %.2f", "Max: %.2f");
    ImGui::DragFloatRange2("Impact Block Lifetime", &roarImpactDebrisBlockLifetimeMin, &roarImpactDebrisBlockLifetimeMax, 0.01f, 0.0f, 5.0f, "Min: %.2f", "Max: %.2f");
    ImGui::DragFloatRange2("Small Debris Scale", &roarImpactDebrisSmallScaleMin, &roarImpactDebrisSmallScaleMax, 0.01f, 0.0f, 10.0f, "Min: %.2f", "Max: %.2f");
    ImGui::DragFloatRange2("Block Debris Scale", &roarImpactDebrisBlockScaleMin, &roarImpactDebrisBlockScaleMax, 0.01f, 0.0f, 10.0f, "Min: %.2f", "Max: %.2f");
    ImGui::Checkbox("Roar Ground Burst Enabled", &roarGroundBurstEnabled);
    ImGui::DragFloat("Roar Ground Burst Inner Ring Ratio", &roarGroundBurstInnerRingRatio, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Roar Ground Burst Outer Ring Ratio", &roarGroundBurstOuterRingRatio, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Roar Ground Burst Angle Jitter", &roarGroundBurstAngleJitterDegrees, 0.25f, 0.0f, 45.0f, "%.1f deg");
    ImGui::DragFloat("Roar Ground Burst Radius Jitter", &roarGroundBurstRadiusJitter, 0.005f, 0.0f, 0.25f, "%.3f");
    ImGui::DragFloat("Roar Ground Burst Y Offset", &roarGroundBurstYOffset, 0.01f, -1.0f, 1.0f, "%.2f m");
    roarFloatingDebrisInnerRatio = std::clamp(roarFloatingDebrisInnerRatio, 0.0f, 1.0f);
    roarFloatingDebrisOuterRatio = std::clamp(roarFloatingDebrisOuterRatio, roarFloatingDebrisInnerRatio, 1.0f);
    roarFloatingDebrisMinHeight = (std::max)(0.0f, roarFloatingDebrisMinHeight);
    roarFloatingDebrisMaxHeight = (std::max)(roarFloatingDebrisMinHeight, roarFloatingDebrisMaxHeight);
    roarFloatingDebrisScaleMin = (std::max)(0.0f, roarFloatingDebrisScaleMin);
    roarFloatingDebrisScaleMax = (std::max)(roarFloatingDebrisScaleMin, roarFloatingDebrisScaleMax);
    roarImpactDebrisSpawnHeight = std::clamp(roarImpactDebrisSpawnHeight, 0.0f, 1.0f);
    roarImpactDebrisGravity = (std::max)(0.0f, roarImpactDebrisGravity);
    roarImpactDebrisSmallHorizontalSpeedMin = (std::max)(0.0f, roarImpactDebrisSmallHorizontalSpeedMin);
    roarImpactDebrisSmallHorizontalSpeedMax = (std::max)(roarImpactDebrisSmallHorizontalSpeedMin, roarImpactDebrisSmallHorizontalSpeedMax);
    roarImpactDebrisSmallVerticalSpeedMin = (std::max)(0.0f, roarImpactDebrisSmallVerticalSpeedMin);
    roarImpactDebrisSmallVerticalSpeedMax = (std::max)(roarImpactDebrisSmallVerticalSpeedMin, roarImpactDebrisSmallVerticalSpeedMax);
    roarImpactDebrisSmallLifetimeMin = (std::max)(0.0f, roarImpactDebrisSmallLifetimeMin);
    roarImpactDebrisSmallLifetimeMax = (std::max)(roarImpactDebrisSmallLifetimeMin, roarImpactDebrisSmallLifetimeMax);
    roarImpactDebrisBlockHorizontalSpeedMin = (std::max)(0.0f, roarImpactDebrisBlockHorizontalSpeedMin);
    roarImpactDebrisBlockHorizontalSpeedMax = (std::max)(roarImpactDebrisBlockHorizontalSpeedMin, roarImpactDebrisBlockHorizontalSpeedMax);
    roarImpactDebrisBlockVerticalSpeedMin = (std::max)(0.0f, roarImpactDebrisBlockVerticalSpeedMin);
    roarImpactDebrisBlockVerticalSpeedMax = (std::max)(roarImpactDebrisBlockVerticalSpeedMin, roarImpactDebrisBlockVerticalSpeedMax);
    roarImpactDebrisBlockLifetimeMin = (std::max)(0.0f, roarImpactDebrisBlockLifetimeMin);
    roarImpactDebrisBlockLifetimeMax = (std::max)(roarImpactDebrisBlockLifetimeMin, roarImpactDebrisBlockLifetimeMax);
    roarImpactDebrisSmallScaleMin = (std::max)(0.0f, roarImpactDebrisSmallScaleMin);
    roarImpactDebrisSmallScaleMax = (std::max)(roarImpactDebrisSmallScaleMin, roarImpactDebrisSmallScaleMax);
    roarImpactDebrisBlockScaleMin = (std::max)(0.0f, roarImpactDebrisBlockScaleMin);
    roarImpactDebrisBlockScaleMax = (std::max)(roarImpactDebrisBlockScaleMin, roarImpactDebrisBlockScaleMax);
    roarGroundBurstInnerRingRatio = std::clamp(roarGroundBurstInnerRingRatio, 0.0f, 1.0f);
    roarGroundBurstOuterRingRatio = std::clamp(roarGroundBurstOuterRingRatio,
        roarGroundBurstInnerRingRatio, 1.0f);
    roarGroundBurstAngleJitterDegrees = (std::max)(0.0f, roarGroundBurstAngleJitterDegrees);
    roarGroundBurstRadiusJitter = (std::max)(0.0f, roarGroundBurstRadiusJitter);
    if (!roarFloatingDebrisEnabled)
        HideRoarFloatingDebris();
    if (!roarImpactDebrisEnabled)
        HideRoarImpactDebris();
    int activeFloatingDebrisCount = 0;
    for (const auto& debris : roarFloatingDebris)
        if (debris.meshComponent && debris.meshComponent->IsVisible())
            ++activeFloatingDebrisCount;
    ImGui::SeparatorText(U8("Roar候補の直近実判定（描画では更新しない）"));
    ImGui::TextColored(ImVec4(0.1f, 0.8f, 1.0f, 1.0f), U8("水色: 防御行動 至近距離"));
    ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.6f, 1.0f), U8("桃色: 咆哮 範囲"));
    ImGui::SeparatorText(U8("Retreat"));
    ImGui::DragFloat(U8("退避距離 最小"), &retreatDistanceMin, 0.1f, 0.0f, 30.0f, "%.2f m");
    ImGui::DragFloat(U8("退避距離 最大"), &retreatDistanceMax, 0.1f, 0.0f, 30.0f, "%.2f m");
    ImGui::DragFloat(U8("退避 最低移動距離"), &retreatMinimumMoveDistance, 0.1f, 0.0f, 30.0f, "%.2f m");
    ImGui::DragFloat(U8("退避 移動速度"), &retreatMoveSpeed, 0.1f, 0.0f, 30.0f, "%.2f m/s");
    ImGui::DragFloat(U8("退避 再試行待ち"), &retreatRetryCooldownDuration, 0.1f, 0.0f, 10.0f, "%.2f sec");
    retreatDistanceMin = (std::max)(0.0f, retreatDistanceMin);
    retreatDistanceMax = (std::max)(retreatDistanceMin, retreatDistanceMax);
    retreatMinimumMoveDistance = (std::max)(0.0f, retreatMinimumMoveDistance);
    retreatMoveSpeed = (std::max)(0.0f, retreatMoveSpeed);
    retreatRetryCooldownDuration = (std::max)(0.0f, retreatRetryCooldownDuration);

#endif
}
