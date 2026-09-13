#include "pch.h"
#include "GruxRoarBT.h"
#include "NodeBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Actors/Player/Player.h"
#include "Game/State/StateMachine.h"
#include "Engine/Scene/Scene.h"
#include "Core/ActorManager.h"
#include "Physics/CollisionFunction.h"
#include <map>

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

    struct RetreatSweepDebugHit
    {
        bool hit = false;
        bool initialOverlap = false;
        bool hasNormal = false;
        DirectX::XMFLOAT3 normal{};
        float penetrationDepth = 0.0f;
        std::string actorName = "None";
        std::string componentName = "None";
        int ignoredSupportContactCount = 0;
        bool pathBlocked = false;
        DirectX::XMFLOAT3 blockingNormal{};
        DirectX::XMFLOAT3 blockingPosition{};
        float blockingDistance = 0.0f;
        std::string blockingActorName = "None";
        std::string blockingComponentName = "None";
        DirectX::XMFLOAT3 start{};
        DirectX::XMFLOAT3 direction{};
        DirectX::XMFLOAT3 target{};
        float distance = 0.0f;
        float radius = 0.0f;
        float halfHeight = 0.0f;
        DirectX::XMFLOAT3 center{};
        uint32_t blockingLayer = 0;
        bool supportContactHit = false;
        bool supportInitialOverlap = false;
        float supportPenetrationDepth = 0.0f;
        DirectX::XMFLOAT3 supportNormal{};
    };

    std::vector<RetreatSweepDebugHit> g_repositionSweepDiagnostics;
    std::map<std::string, int> g_repositionSweepActorCounts;
    std::map<std::string, int> g_repositionSweepComponentCounts;

    void CopyPositioningPathSafetyDebugHit(const PositioningPathSafetyHit& source,
        RetreatSweepDebugHit& destination)
    {
        if (!source.hit)
            return;
        destination.hit = true;
        destination.initialOverlap = source.initialOverlap;
        destination.hasNormal = source.hasNormal;
        destination.normal = source.normal;
        destination.penetrationDepth = source.penetrationDepth;
        if (source.actor)
            destination.actorName = source.actor->GetName();
        if (source.component)
            destination.componentName = source.component->GetName();
    }

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
        float distance, HitResult& result, uint32_t obstacleMask, RetreatSweepDebugHit* debugHit = nullptr)
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

        if (debugHit)
        {
            debugHit->center = shape.center;
            debugHit->radius = shape.capsule.r;
            debugHit->halfHeight = shape.halfHeight;
            debugHit->direction = direction;
            debugHit->distance = distance;
            debugHit->ignoredSupportContactCount = pathResult.ignoredSupportContactCount;
            debugHit->pathBlocked = pathResult.pathBlocked;
            if (pathResult.ignoredSupportContact.hit)
            {
                debugHit->supportContactHit = true;
                debugHit->supportInitialOverlap = pathResult.ignoredSupportContact.initialOverlap;
                debugHit->supportPenetrationDepth = pathResult.ignoredSupportContact.penetrationDepth;
                debugHit->supportNormal = pathResult.ignoredSupportContact.normal;
            }
            CopyPositioningPathSafetyDebugHit(
                pathResult.ignoredSupportContact.hit ? pathResult.ignoredSupportContact : pathResult.blockingHit,
                *debugHit);
            if (pathResult.blockingHit.hit)
            {
                debugHit->blockingNormal = pathResult.blockingHit.normal;
                debugHit->blockingPosition = pathResult.blockingHit.position;
                debugHit->blockingDistance = pathResult.blockingHit.distance;
                if (pathResult.blockingHit.actor)
                    debugHit->blockingActorName = pathResult.blockingHit.actor->GetName();
                if (pathResult.blockingHit.component)
                {
                    debugHit->blockingComponentName = pathResult.blockingHit.component->GetName();
                    debugHit->blockingLayer = pathResult.blockingHit.component->GetCollisionLayer();
                }
            }
        }
        return pathBlocked;
    }
}
bool CanPlanRoar::Judgment() { return owner->CanPlanRoar(); }
bool CanPlanAnyDefensive::Judgment()
{
    if (owner->IsInitialRepositionFallbackIdlePending())
    {
        owner->RecordCombatDecisionDebugDefensive(false);
        return false;
    }
    if (!owner->CanPlanAttackAgainstCurrentPlayer())
    {
        owner->RecordCombatDecisionDebugDefensive(false);
        return false;
    }
    const bool result = owner->CanPlanRoar() || owner->CanPlanRetreat();
    owner->RecordCombatDecisionDebugDefensive(result);
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
    owner->RecordRetreatMoveResult(result);
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
    const bool result = EvaluateRoarPlanForDebug();
    // Preserve the actual selection-time result; drawing ImGui must not replace it
    // with the later Idle/ExecuteRoar-time evaluation.
    roarPlanDebugEvaluated = true;
    roarPlanDebugLastResult = result;
    roarPlanDebugLastRejectReason = result ? U8("なし") : GetRoarRejectReasonForDebug();
    roarPlanDebugLastNode = activeNode ? activeNode->GetName() : "None (Root inference)";
    return result;
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
    retreatDebugTargetPosition = {};
    retreatRemainingDistance = 0.0f;
    retreatLastMoveResult = PositioningMoveResult::None;
    retreatCompleteReason = U8("Prepare実行中");
    retreatRuntime.failureReason = U8("候補探索中");
    if (!CanPlanRetreat())
    {
        retreatRuntime.failureReason = U8("開始条件不成立");
        retreatRetryCooldownRemaining = retreatRetryCooldownDuration;
        return false;
    }

    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player)
    {
        retreatRuntime.failureReason = U8("Player不在");
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
    const Capsule& capsule = retreatSweepShape.capsule;
    retreatRuntime.capsulePoint1 = capsule.a;
    retreatRuntime.capsulePoint2 = capsule.b;
    retreatRuntime.capsuleRadius = capsule.r;
    const float capsuleDx = retreatRuntime.capsulePoint2.x - retreatRuntime.capsulePoint1.x;
    const float capsuleDy = retreatRuntime.capsulePoint2.y - retreatRuntime.capsulePoint1.y;
    const float capsuleDz = retreatRuntime.capsulePoint2.z - retreatRuntime.capsulePoint1.z;
    retreatRuntime.capsuleSegmentLength = std::sqrt(capsuleDx * capsuleDx +
        capsuleDy * capsuleDy + capsuleDz * capsuleDz);

    // These fields mirror the CollisionComponent -> PhysX shape construction and
    // are diagnostic only. retreatSweepShape is created from the same center,
    // radius, and halfHeight.
    const Transform& collisionWorldTransform = enemyCapsuleComponent->GetComponentWorldTransform();
    retreatRuntime.collisionComponentPosition = collisionWorldTransform.GetLocation();
    retreatRuntime.collisionComponentScale = collisionWorldTransform.GetScale();
    retreatRuntime.collisionConfiguredRadius = enemyCapsuleComponent->GetRadius();
    retreatRuntime.collisionConfiguredHeight = enemyCapsuleComponent->GetHeight();
    retreatRuntime.collisionOffsetY = enemyCapsuleComponent->GetCollisionOffsetY();
    retreatRuntime.collisionPhysXHalfHeight = retreatSweepShape.halfHeight;
    retreatRuntime.retreatSweepUsesSphere = retreatSweepShape.isSphere;
    retreatRuntime.collisionPhysXPoint1 = retreatSweepShape.capsule.a;
    retreatRuntime.collisionPhysXPoint2 = retreatSweepShape.capsule.b;
    retreatRuntime.collisionPhysXMinY = retreatRuntime.collisionPhysXPoint2.y - retreatRuntime.collisionConfiguredRadius;
    retreatRuntime.collisionPhysXMaxY = retreatRuntime.collisionPhysXPoint1.y + retreatRuntime.collisionConfiguredRadius;
    retreatRuntime.retreatCastMinY = retreatRuntime.collisionPhysXMinY;
    retreatRuntime.retreatCastMaxY = retreatRuntime.collisionPhysXMaxY;
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
            RetreatCandidateDebug debug{};
            debug.position = evaluation.clampedTarget;
            ++retreatRuntime.candidateCount;
            const float moveX = evaluation.clampedTarget.x - retreatRuntime.gruxSnapshot.x;
            const float moveZ = evaluation.clampedTarget.z - retreatRuntime.gruxSnapshot.z;
            const float moveDistance = std::sqrt(moveX * moveX + moveZ * moveZ);
            const float playerX = evaluation.clampedTarget.x - retreatRuntime.playerSnapshot.x;
            const float playerZ = evaluation.clampedTarget.z - retreatRuntime.playerSnapshot.z;
            const float plannedPlayerDistance = std::sqrt(playerX * playerX + playerZ * playerZ);
            if (evaluation.clampDistance > attackSetupClampTolerance)
            {
                debug.rejectReason = RetreatCandidateRejectReason::Clamp;
                ++retreatRuntime.clampRejectCount;
                retreatRuntime.candidates.push_back(debug);
                continue;
            }
            if (plannedPlayerDistance <= retreatRuntime.startingPlayerDistance + 0.01f)
            {
                debug.rejectReason = RetreatCandidateRejectReason::DistanceIncrease;
                ++retreatRuntime.distanceIncreaseRejectCount;
                retreatRuntime.candidates.push_back(debug);
                continue;
            }
            if (moveDistance < retreatMinimumMoveDistance)
            {
                debug.rejectReason = RetreatCandidateRejectReason::MinimumMoveDistance;
                ++retreatRuntime.minimumMoveRejectCount;
                retreatRuntime.candidates.push_back(debug);
                continue;
            }
            const DirectX::XMFLOAT3 moveDirection{ moveX / moveDistance, 0.0f, moveZ / moveDistance };
            HitResult hit{};
            RetreatSweepDebugHit sweepDebugHit{};
            debug.pathBlocked = SweepRetreatPath(retreatSweepShape, moveDirection, moveDistance,
                hit, obstacleMask, &sweepDebugHit);
            retreatRuntime.retreatSweepInitialOverlap = sweepDebugHit.initialOverlap;
            retreatRuntime.retreatSweepHasNormal = sweepDebugHit.hasNormal;
            retreatRuntime.retreatSweepHitNormal = sweepDebugHit.normal;
            retreatRuntime.retreatSweepPenetrationDepth = sweepDebugHit.penetrationDepth;
            retreatRuntime.retreatSweepHitActor = sweepDebugHit.actorName;
            retreatRuntime.retreatSweepHitComponent = sweepDebugHit.componentName;
            retreatRuntime.retreatSweepIgnoredSupportContactCount = sweepDebugHit.ignoredSupportContactCount;
            retreatRuntime.retreatSweepPathBlocked = sweepDebugHit.pathBlocked;
            retreatRuntime.retreatSweepBlockingNormal = sweepDebugHit.blockingNormal;
            retreatRuntime.retreatSweepBlockingDistance = sweepDebugHit.blockingDistance;
            retreatRuntime.retreatSweepBlockingActor = sweepDebugHit.blockingActorName;
            retreatRuntime.retreatSweepBlockingComponent = sweepDebugHit.blockingComponentName;
            if (debug.pathBlocked)
            {
                debug.rejectReason = RetreatCandidateRejectReason::CapsuleCast;
                ++retreatRuntime.capsuleCastRejectCount;
                retreatRuntime.lastCapsuleCastHit = true;
                retreatRuntime.lastCapsuleHitPosition = hit.position;
                retreatRuntime.lastCapsuleHitDistance = hit.distance;
                HitResult layerHit{};
                retreatRuntime.lastCapsuleCastWorldStatic = SweepRetreatPath(retreatSweepShape,
                    moveDirection, moveDistance, layerHit, CollisionHelper::ToBit(CollisionLayer::WorldStatic));
                retreatRuntime.lastCapsuleCastWorldProps = SweepRetreatPath(retreatSweepShape,
                    moveDirection, moveDistance, layerHit, CollisionHelper::ToBit(CollisionLayer::WorldProps));
                retreatRuntime.lastCapsuleCastConvex = SweepRetreatPath(retreatSweepShape,
                    moveDirection, moveDistance, layerHit, CollisionHelper::ToBit(CollisionLayer::Convex));
                retreatRuntime.candidates.push_back(debug);
                continue;
            }
            debug.accepted = true;
            retreatRuntime.candidates.push_back(debug);
            retreatTarget = {};
            retreatTarget.valid = true;
            retreatTarget.targetPosition = evaluation.clampedTarget;
            retreatTarget.arrivalTolerance = 0.3f;
            retreatTarget.timeout = 3.0f;
            retreatTarget.maxMoveDistance = moveDistance + 1.0f;
            retreatTarget.moveSpeed = retreatMoveSpeed;
            retreatTarget.stuckMovementThreshold = 0.01f;
            retreatTarget.stuckTimeThreshold = 0.5f;
            retreatDebugTargetPosition = retreatTarget.targetPosition;
            retreatRuntime.plannedMoveDistance = moveDistance;
            retreatRuntime.plannedPlayerDistance = plannedPlayerDistance;
            retreatRuntime.failureReason = U8("なし");
            retreatCompleteReason = U8("Move開始待ち");
            return true;
        }
    }
    retreatRuntime.failureReason = U8("安全なTargetなし");
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
    retreatRemainingDistance = remaining;
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
            retreatRuntime.failureReason = U8("経路異常");
            return PositioningMoveResult::Stuck;
        }
    }
    const auto result = UpdatePositioningTargetMovement(retreatTarget, retreatMovementRuntime,
        deltaTime, "RetreatMovement");
    if (result != PositioningMoveResult::Running && result != PositioningMoveResult::Arrived &&
        retreatRuntime.failureReason == U8("なし"))
    {
        retreatRuntime.failureReason = result == PositioningMoveResult::Timeout ? U8("Timeout") :
            result == PositioningMoveResult::Stuck ? U8("Stuck") :
            result == PositioningMoveResult::MaxDistanceReached ? U8("MaxDistance") :
            U8("Target Invalid");
    }
    return result;
}

void GruxEnemy::FinishRetreatMovement(bool arrived)
{
    StopAIMovement();
    EndPositioningAnimation();
    if (!arrived && retreatRuntime.failureReason == U8("なし"))
        retreatRuntime.failureReason = U8("移動失敗");
    retreatCompleteReason = arrived ? U8("Arrived: Remaining Distance <= Arrival Tolerance")
        : retreatRuntime.failureReason;
    ClearPositioningTarget(retreatTarget, retreatMovementRuntime);
    if (arrived)
        consecutiveAttackCount = 0;
    retreatRetryCooldownRemaining = retreatRetryCooldownDuration;
}
void GruxEnemy::RecordRetreatMoveResult(PositioningMoveResult result)
{
    retreatLastMoveResult = result;
}

void GruxEnemy::BeginCombatDecisionDebugInference()
{
    ++combatDecisionDebugInferenceSerial;
    combatDecisionDebugActiveNodeAtStart = activeNode ? activeNode->GetName() : "None";
    combatDecisionDebugDefensiveResult = "NotEvaluated";
    combatDecisionDebugRootSelectedNode = "None";
    combatDecisionDebugFailureReason = "None";
    combatDecisionDebugRepositionReason = "NotEvaluated";
    combatDecisionDebugCanPlanAny = false;
    combatDecisionDebugCanPlanReposition = false;
    combatDecisionDebugCanPlanAnyAttack = false;
    combatDecisionDebugFastCombo = false;
    combatDecisionDebugJump = false;
    combatDecisionDebugDash = false;
    combatDecisionDebugCharge = false;
}

void GruxEnemy::CompleteCombatDecisionDebugInference(const char* selectedNode)
{
    combatDecisionDebugRootSelectedNode = selectedNode ? selectedNode : "None";
    if (combatDecisionDebugRootSelectedNode == "Idle" && combatDecisionDebugFailureReason == "None")
        combatDecisionDebugFailureReason = "RootSelectedIdle";
}

void GruxEnemy::RecordCombatDecisionDebugDefensive(bool result)
{
    combatDecisionDebugDefensiveResult = result ? "True" : "False";
}

void GruxEnemy::BeginCombatDecisionInference()
{
    repositionDecisionCached = true;
    repositionDecisionResult = false;
    repositionDecisionRoll = 0.0f;
    repositionCanPlanDebug = false;
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
    repositionCanPlanDebug = repositionDecisionResult;
}

bool GruxEnemy::CanPlanAnyCombatDecision()
{
    if (initialRepositionFallbackIdlePending)
    {
        combatDecisionDebugCanPlanAny = false;
        combatDecisionDebugCanPlanAnyAttack = false;
        return false;
    }
    if (!repositionDecisionCached)
        BeginCombatDecisionInference();
    const bool playerAvailableForAttack = CanPlanAttackAgainstCurrentPlayer();
    if (playerAvailableForAttack)
    {
        combatDecisionDebugFastCombo = CanPlanFastCombo();
        combatDecisionDebugJump = CanPlanJumpAttack();
        combatDecisionDebugDash = CanPlanDashAttack();
        combatDecisionDebugCharge = CanPlanChargeAttack();
    }
    else
    {
        combatDecisionDebugFastCombo = false;
        combatDecisionDebugJump = false;
        combatDecisionDebugDash = false;
        combatDecisionDebugCharge = false;
    }
    const bool canPlanAttack = playerAvailableForAttack &&
        (combatDecisionDebugFastCombo || combatDecisionDebugJump ||
            combatDecisionDebugDash || combatDecisionDebugCharge);
    combatDecisionDebugCanPlanAnyAttack = canPlanAttack;
    const bool canPlan = CanPlanReposition() || canPlanAttack;
    combatDecisionDebugCanPlanAny = canPlan;
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
    combatDecisionDebugCanPlanReposition = result;
    if (result) combatDecisionDebugRepositionReason = "Ready";
    else if (!decisionAllows) combatDecisionDebugRepositionReason = "ProbabilityRejected";
    else if (!targetInactive) combatDecisionDebugRepositionReason = "TargetAlreadyActive";
    else if (!movementInactive) combatDecisionDebugRepositionReason = "MovementAlreadyActive";
    else if (!cooldownReady) combatDecisionDebugRepositionReason = "RepositionCooldown";
    else if (!retryReady) combatDecisionDebugRepositionReason = "RetryCooldown";
    else if (roarAvailable) combatDecisionDebugRepositionReason = "RoarAvailable";
    else if (retreatAvailable) combatDecisionDebugRepositionReason = "RetreatAvailable";
    else if (!nodeAllowed) combatDecisionDebugRepositionReason = std::string("ActiveNode=") + activeNode->GetName();
    else if (!executionAllowed) combatDecisionDebugRepositionReason = "ExecutionUnavailable";
    else combatDecisionDebugRepositionReason = "MissingComponent";
    return result;
}

bool GruxEnemy::PrepareRepositionTarget()
{
    g_repositionSweepDiagnostics.clear();
    g_repositionSweepActorCounts.clear();
    g_repositionSweepComponentCounts.clear();
    ClearPositioningTarget(repositionTarget, repositionMovementRuntime);
    repositionRuntime = {};
    repositionRemainingDistance = 0.0f;
    repositionRuntime.failureReason = "Searching";
    if (!CanPlanReposition())
    {
        repositionRuntime.failureReason = "StartConditionFailed";
        if (!IsInitialRepositionPending())
            repositionRetryCooldownRemaining = repositionRetryCooldownDuration;
        return false;
    }

    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player)
    {
        repositionRuntime.failureReason = "PlayerMissing";
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
        int candidateIndex = -1;
        float angleDegrees = 0.0f;
        float candidateDistance = 0.0f;
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
            RepositionCandidateDebug debug{};
            debug.position = evaluation.clampedTarget;
            debug.candidateIndex = repositionRuntime.candidateCount++;
            debug.angleDegrees = angleDegrees;
            debug.candidateDistance = distance;
            const float moveX = evaluation.clampedTarget.x - repositionRuntime.gruxSnapshot.x;
            const float moveZ = evaluation.clampedTarget.z - repositionRuntime.gruxSnapshot.z;
            const float moveDistance = std::sqrt(moveX * moveX + moveZ * moveZ);
            const float playerX = evaluation.clampedTarget.x - repositionRuntime.playerSnapshot.x;
            const float playerZ = evaluation.clampedTarget.z - repositionRuntime.playerSnapshot.z;
            const float plannedPlayerDistance = std::sqrt(playerX * playerX + playerZ * playerZ);

            // Reposition accepts a Playable Area clamped target when that final
            // target is otherwise valid. It intentionally does not use the
            // Attack Setup / Retreat clamp-tolerance rejection policy.
            const bool wasClamped = evaluation.wasClamped;
            if (wasClamped)
            {
                ++repositionRuntime.clampAppliedCount;
                repositionRuntime.lastClampedTarget = evaluation.clampedTarget;
                repositionRuntime.lastClampDistance = evaluation.clampDistance;
            }
            const auto recordClampPostEvaluationReject = [&]()
            {
                if (wasClamped)
                    ++repositionRuntime.clampPostClampRejectCount;
            };

            if (plannedPlayerDistance < repositionMinimumPlayerDistance)
            {
                debug.rejectReason = RepositionCandidateRejectReason::PlayerDistanceTooClose;
                ++repositionRuntime.playerDistanceTooCloseRejectCount;
                recordClampPostEvaluationReject();
                repositionRuntime.candidates.push_back(debug);
                continue;
            }
            if (moveDistance < repositionMinimumMoveDistance)
            {
                debug.rejectReason = RepositionCandidateRejectReason::MinimumMoveDistance;
                ++repositionRuntime.minimumMoveRejectCount;
                recordClampPostEvaluationReject();
                repositionRuntime.candidates.push_back(debug);
                continue;
            }

            const DirectX::XMFLOAT3 moveDirection{ moveX / moveDistance, 0.0f, moveZ / moveDistance };
            HitResult hit{};
            RetreatSweepDebugHit sweepDebug{};
            sweepDebug.start = repositionRuntime.gruxSnapshot;
            sweepDebug.target = evaluation.clampedTarget;
            debug.pathBlocked = SweepRetreatPath(sweepShape, moveDirection, moveDistance, hit, obstacleMask, &sweepDebug);
            g_repositionSweepDiagnostics.push_back(sweepDebug);
            if (debug.pathBlocked)
            {
                ++g_repositionSweepActorCounts[sweepDebug.blockingActorName];
                ++g_repositionSweepComponentCounts[sweepDebug.blockingComponentName];
            }
            if (debug.pathBlocked)
            {
                debug.rejectReason = RepositionCandidateRejectReason::Sweep;
                ++repositionRuntime.sweepRejectCount;
                recordClampPostEvaluationReject();
                repositionRuntime.candidates.push_back(debug);
                continue;
            }

            if (wasClamped)
                ++repositionRuntime.clampPostClampAcceptedCount;
            debug.accepted = true;
            repositionRuntime.candidates.push_back(debug);
            safeCandidates.push_back({ evaluation.clampedTarget, moveDistance, plannedPlayerDistance,
                debug.candidateIndex, angleDegrees, distance });
        }
    }

    repositionRuntime.safeCandidateCount = static_cast<int>(safeCandidates.size());
    if (!safeCandidates.empty())
    {
        static thread_local std::mt19937 randomEngine{ std::random_device{}() };
        std::uniform_int_distribution<size_t> distribution(0, safeCandidates.size() - 1);
        const auto& selected = safeCandidates[distribution(randomEngine)];
        repositionRuntime.selectedCandidateIndex = selected.candidateIndex;
        repositionRuntime.selectedCandidateAngleDegrees = selected.angleDegrees;
        repositionRuntime.selectedCandidateDistance = selected.candidateDistance;
        repositionRuntime.selectedInitialReposition = IsInitialRepositionPending();
        repositionRuntime.candidates[static_cast<size_t>(selected.candidateIndex)].selected = true;

        repositionTarget = {};
        repositionTarget.valid = true;
        repositionTarget.targetPosition = selected.position;
        repositionTarget.arrivalTolerance = 0.3f;
        repositionTarget.timeout = 3.0f;
        repositionTarget.maxMoveDistance = selected.moveDistance + 1.0f;
        repositionTarget.moveSpeed = repositionMoveSpeed;
        repositionTarget.stuckMovementThreshold = 0.01f;
        repositionTarget.stuckTimeThreshold = 0.5f;
        repositionRuntime.plannedMoveDistance = selected.moveDistance;
        repositionRuntime.plannedPlayerDistance = selected.plannedPlayerDistance;
        repositionRuntime.failureReason = "None";

        lastCombatDecision = "Reposition";
        return true;
    }

    repositionRuntime.failureReason = "NoSafeTarget";
    ClearPositioningTarget(repositionTarget, repositionMovementRuntime);
    if (!IsInitialRepositionPending())
        repositionRetryCooldownRemaining = repositionRetryCooldownDuration;

    return false;
}
GruxEnemy::PositioningMoveResult GruxEnemy::UpdateRepositionMovement(float dt)
{
    if (!repositionTarget.valid)
        return PositioningMoveResult::InvalidTarget;
    const auto position = GetPosition();
    const float dx = repositionTarget.targetPosition.x - position.x;
    const float dz = repositionTarget.targetPosition.z - position.z;
    repositionRemainingDistance = std::sqrt(dx * dx + dz * dz);
    const auto result = UpdatePositioningTargetMovement(repositionTarget, repositionMovementRuntime, dt, "RepositionMovement");
    if (result != PositioningMoveResult::Running && result != PositioningMoveResult::Arrived &&
        repositionRuntime.failureReason == "None")
    {
        repositionRuntime.failureReason = result == PositioningMoveResult::Timeout ? "Timeout" :
            result == PositioningMoveResult::Stuck ? "Stuck" :
            result == PositioningMoveResult::MaxDistanceReached ? "MaxDistance" : "TargetInvalid";
    }
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
    lastCombatDecision = "Reposition";
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

std::string GruxEnemy::GetRoarRejectReasonForDebug() const
{
    // Same first-failure order as IsRoarExecutionAllowed and CanPlanRoar.
    // This function is diagnostic only; it never decides BT eligibility.
    if (!behaviorTreeFastComboEnabled) return U8("BTが無効");
    if (!battleAIActive) return U8("戦闘AIが無効");
    if (IsDead()) return U8("Gruxが死亡");
    if (IsPendingKill()) return U8("Gruxが削除待ち");
    if (isDeathPerform) return U8("死亡演出中");
    if (finalHitReactionActive) return U8("FinalHit演出中");
    if (finalHitReactionHeld) return U8("FinalHitの姿勢保持中");
    if (IsAnimationEditorPreviewActive()) return U8("Animation Preview中");
    if (IsChargeAttackBTActive()) return U8("Charge BT実行中");
    if (IsDashAttackBTActive()) return U8("Dash BT実行中");
    if (chargeMovementActive) return U8("Charge移動中");
    if (dashAttackMovementActive) return U8("Dash移動中");
    if (jumpMotionWarpOverrideActive) return U8("Jump MotionWarp実行中");
    if (!characterMovementComponent) return U8("移動Componentがない");
    if (!GetBodyAnimationController()) return U8("AnimationControllerがない");
    if (!GetOwnerScene()) return U8("Sceneがない");
    if (stateMachine_)
    {
        const std::string state = stateMachine_->GetStateName();
        if (state == "EnemyDeathState") return U8("旧Death State中");
        if (state == "EnemyStunState") return U8("旧Stun State中");
    }
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player) return U8("Playerが存在しない");
    if (player->IsPendingKill()) return U8("Playerが削除待ち");
    if (player->GetHp() <= 0) return U8("Playerが死亡");
    if (IsRoarBTActive()) return U8("Roar実行中");
    if (roarCooldownRemaining > 0.0f) return U8("Cooldown中");
    if (IsOtherBTActionForRoar(activeNode))
        return std::string(U8("他BT行動中: ")) + activeNode->GetName();
    const auto context = BuildTargetContext();
    if (!context.valid) return U8("TargetContextが無効");
    if (!IsDefensiveBackForBehavior(context)) return U8("背後ではない");
    if (!(context.xzDistance <= defensiveTooCloseDistance)) return U8("距離が遠い");
    return U8("なし");
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
    roarBT = {};
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
    SpawnGroundImpactEffect();
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player || roarBT.hitPlayer || !player->CanReceiveKnockBack()) return;
    auto direction = MathHelper::Subtract(player->GetPosition(), GetPosition());
    if (std::abs(direction.y) > roarHeightTolerance) return;
    direction.y = 0.0f;
    if (direction.x * direction.x + direction.z * direction.z > roarRadius * roarRadius) return;
    if (MathHelper::Length(direction) < 0.0001f)
    {
        direction = GetForward();
        direction.y = 0.0f;
    }
    // Damage is deliberately separate from the radial query and knockback response.
    roarBT.hitPlayer = player->StartKnockBack(direction);
}

void GruxEnemy::CleanupRoarBT(const char* status)
{
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
    ImGui::Checkbox(U8("攻撃行動を無効化"), &disableAttackBehaviorsForDebug);
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
    ImGui::DragFloat(U8("咆哮 範囲"), &roarRadius, 0.1f, 0.0f, 30.0f, "%.2f m");
    ImGui::DragFloat(U8("咆哮 高低差許容"), &roarHeightTolerance, 0.1f, 0.0f, 30.0f, "%.2f m");
    const float previousCooldownDuration = roarCooldownDuration;
    ImGui::DragFloat(U8("咆哮クールタイム"), &roarCooldownDuration, 0.1f, 0.0f, 120.0f, "%.2f sec");
    defensiveTooCloseDistance = (std::max)(0.0f, defensiveTooCloseDistance);
    roarRadius = (std::max)(0.0f, roarRadius);
    roarHeightTolerance = (std::max)(0.0f, roarHeightTolerance);
    roarCooldownDuration = (std::max)(0.0f, roarCooldownDuration);
    if (roarCooldownRemaining > 0.0f && roarCooldownDuration != previousCooldownDuration)
        roarCooldownRemaining = (std::max)(0.0f,
            roarCooldownRemaining + roarCooldownDuration - previousCooldownDuration);
    const auto context = BuildTargetContext();
    const char* region = !context.valid ? "None" : context.region == PlayerRelativeRegion::Back ? "Back" :
        context.region == PlayerRelativeRegion::Side ? "Side" : "Front";
    ImGui::Text(U8("咆哮BT状態: %s"), roarBTStatus.c_str());
    ImGui::Text(U8("咆哮Stage: %d"), static_cast<int>(roarBT.stage));
    ImGui::Text(U8("咆哮衝撃波 発動済み: %s"), roarBT.shockwaveFired ? "true" : "false");
    ImGui::Text(U8("Player距離: %.2f m"), context.xzDistance);
    ImGui::Text(U8("Player位置Region: %s"), region);
    ImGui::Text(U8("咆哮クールタイム残り: %.2f sec"), roarCooldownRemaining);
    const bool currentResult = EvaluateRoarPlanForDebug();
    ImGui::Text(U8("咆哮候補: %s"), currentResult ? "true" : "false");
    const auto scene = GetOwnerScene();
    const auto player = scene ? scene->GetActorManager()->GetActorOfType<Player>() : nullptr;
    const std::string state = stateMachine_ ? stateMachine_->GetStateName() : "None";
    const bool otherBTAction = IsOtherBTActionForRoar(activeNode);
    const auto tf = [](bool value) { return value ? "true" : "false"; };
    ImGui::SeparatorText(U8("Roar候補条件の詳細（現在値）"));
    ImGui::Text(U8("Roar Player有効: %s"), tf(player && !player->IsPendingKill()));
    ImGui::Text(U8("Roar Player生存: %s"), tf(player && player->GetHp() > 0));
    ImGui::Text(U8("Roar Grux生存: %s"), tf(!IsDead()));
    ImGui::Text(U8("Roar 戦闘AI有効: %s"), tf(battleAIActive));
    ImGui::Text(U8("Battle AI Active: %s"), tf(battleAIActive));
    ImGui::Text(U8("Roar Defensive開始可能: %s"), tf(IsRoarExecutionAllowed()));
    ImGui::Text(U8("Roar TargetContext有効: %s"), tf(context.valid));
    ImGui::Text(U8("Roar Player距離: %.2f"), context.xzDistance);
    ImGui::Text(U8("Roar TooClose距離: %.2f"), defensiveTooCloseDistance);
    ImGui::Text(U8("Roar 距離条件OK: %s"), tf(context.xzDistance <= defensiveTooCloseDistance));
    ImGui::Text(U8("Roar Player Region: %s"), region);
    ImGui::Text(U8("Defensive Back判定: %s"), tf(IsDefensiveBackForBehavior(context)));
    ImGui::Text(U8("Player相対角度: %.2f deg"), context.absoluteAngleDegrees);
    ImGui::Text(U8("Roar Back条件OK: %s"), tf(IsDefensiveBackForBehavior(context)));
    ImGui::Text(U8("Roar Cooldown残り: %.2f"), roarCooldownRemaining);
    ImGui::Text(U8("Roar Cooldown条件OK: %s"), tf(!(roarCooldownRemaining > 0.0f)));
    ImGui::Text(U8("Roar 実行中: %s"), tf(IsRoarBTActive()));
    ImGui::Text(U8("Roar Death/Stun中: %s"), tf(IsDead() || state == "EnemyDeathState" || state == "EnemyStunState"));
    ImGui::Text(U8("Roar 他BT行動中: %s"), tf(otherBTAction));
    ImGui::Text(U8("Roar Preview中: %s"), tf(IsAnimationEditorPreviewActive()));
    ImGui::Text(U8("Roar 演出中: %s"), tf(isDeathPerform ||
        finalHitReactionActive || finalHitReactionHeld));
    ImGui::Text(U8("CanPlanRoar: %s"), tf(currentResult));
    const bool retreatCandidate = CanPlanRetreat();
    ImGui::Text(U8("CanPlanAnyDefensive: %s"), tf(currentResult || retreatCandidate));
    ImGui::Text(U8("Roar候補外理由: %s"), GetRoarRejectReasonForDebug().c_str());
    ImGui::Text(U8("Roar 現在BTノード: %s"), activeNode ? activeNode->GetName().c_str() : "None");
    ImGui::Text(U8("Roar 旧State: %s"), state.c_str());
    ImGui::Text(U8("Roar BT有効: %s"), tf(behaviorTreeFastComboEnabled));
    ImGui::Text(U8("Roar Grux削除待ち: %s"), tf(IsPendingKill()));
    ImGui::Text(U8("Roar 移動Component有効: %s"), tf(characterMovementComponent != nullptr));
    ImGui::Text(U8("Roar AnimationController有効: %s"), tf(GetBodyAnimationController() != nullptr));
    ImGui::Text(U8("Roar Scene有効: %s"), tf(scene != nullptr));
    ImGui::Text(U8("Roar Charge BT実行中: %s"), tf(IsChargeAttackBTActive()));
    ImGui::Text(U8("Roar Dash BT実行中: %s"), tf(IsDashAttackBTActive()));
    ImGui::Text(U8("Roar Charge移動中: %s"), tf(chargeMovementActive));
    ImGui::Text(U8("Roar Dash移動中: %s"), tf(dashAttackMovementActive));
    ImGui::Text(U8("Roar Jump MotionWarp実行中: %s"), tf(jumpMotionWarpOverrideActive));
    ImGui::SeparatorText(U8("Roar候補の直近実判定（描画では更新しない）"));
    ImGui::Text(U8("Roar 実判定済み: %s"), tf(roarPlanDebugEvaluated));
    ImGui::Text(U8("Roar 直近実判定: %s"), roarPlanDebugEvaluated ? tf(roarPlanDebugLastResult) : U8("未判定"));
    ImGui::Text(U8("Roar 直近候補外理由: %s"), roarPlanDebugEvaluated ? roarPlanDebugLastRejectReason.c_str() : U8("未判定"));
    ImGui::Text(U8("Roar 実判定時BTノード: %s"), roarPlanDebugEvaluated ? roarPlanDebugLastNode.c_str() : U8("未判定"));
    ImGui::Checkbox(U8("AI距離をWorld表示"), &showBossAIDebug);
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
    ImGui::Checkbox(U8("Retreat World Debug"), &retreatWorldDebug);
    ImGui::Text(U8("Retreat候補: %s"), tf(retreatCandidate));
    ImGui::Text(U8("Retreat Target Valid: %s"), tf(retreatTarget.valid));
    ImGui::Text(U8("Retreat Target Position: (%.2f, %.2f, %.2f)"), retreatDebugTargetPosition.x,
        retreatDebugTargetPosition.y, retreatDebugTargetPosition.z);
    ImGui::Text(U8("Retreat 開始位置: (%.2f, %.2f, %.2f)"), retreatRuntime.gruxSnapshot.x,
        retreatRuntime.gruxSnapshot.y, retreatRuntime.gruxSnapshot.z);
    ImGui::Text(U8("Playerとの現在距離: %.2f m"), context.xzDistance);
    ImGui::Text(U8("Retreat後の予定距離: %.2f m"), retreatRuntime.plannedPlayerDistance);
    ImGui::Text(U8("Retreat移動距離: %.2f m"), retreatRuntime.plannedMoveDistance);
    ImGui::Text(U8("Retreat 予定移動距離: %.2f m"), retreatRuntime.plannedMoveDistance);
    ImGui::Text(U8("Retreat 残り距離: %.2f m"), retreatRemainingDistance);
    ImGui::Text(U8("Retreat 到達許容距離: %.2f m"), retreatTarget.valid
        ? retreatTarget.arrivalTolerance : 0.3f);
    const auto moveResultName = [](PositioningMoveResult result)
        {
            switch (result)
            {
            case PositioningMoveResult::None: return "None";
            case PositioningMoveResult::Running: return "Running";
            case PositioningMoveResult::Arrived: return "Arrived";
            case PositioningMoveResult::Timeout: return "Timeout";
            case PositioningMoveResult::Stuck: return "Stuck";
            case PositioningMoveResult::InvalidTarget: return "InvalidTarget";
            case PositioningMoveResult::MaxDistanceReached: return "MaxDistanceReached";
            }
            return "Unknown";
        };
    ImGui::Text(U8("Retreat Move Result: %s"), moveResultName(retreatLastMoveResult));
    ImGui::Text(U8("Retreat Complete理由: %s"), retreatCompleteReason.c_str());
    ImGui::Text(U8("Retreat候補数: %d"), retreatRuntime.candidateCount);
    ImGui::Text(U8("Retreat 候補総数: %d"), retreatRuntime.candidateCount);
    ImGui::Text(U8("Retreat Clamp失敗数: %d"), retreatRuntime.clampRejectCount);
    ImGui::Text(U8("Retreat 距離増加失敗数: %d"), retreatRuntime.distanceIncreaseRejectCount);
    ImGui::Text(U8("Retreat 最低移動距離失敗数: %d"), retreatRuntime.minimumMoveRejectCount);
    ImGui::Text(U8("Retreat CapsuleCast失敗数: %d"), retreatRuntime.capsuleCastRejectCount);
    ImGui::Text(U8("Retreat その他失敗数: %d"), retreatRuntime.otherRejectCount);
    ImGui::SeparatorText(U8("Retreat CapsuleCast Debug"));
    ImGui::Text(U8("CapsuleCast Hit: %s"), tf(retreatRuntime.lastCapsuleCastHit));
    ImGui::Text(U8("Hit Object / Actor名: %s"), U8("Physics::CapsuleCast APIでは取得不可"));
    ImGui::Text(U8("Hit Collision Layer: WorldStatic=%s WorldProps=%s Convex=%s"),
        tf(retreatRuntime.lastCapsuleCastWorldStatic),
        tf(retreatRuntime.lastCapsuleCastWorldProps),
        tf(retreatRuntime.lastCapsuleCastConvex));
    ImGui::Text(U8("Hit Position: (%.2f, %.2f, %.2f)"), retreatRuntime.lastCapsuleHitPosition.x,
        retreatRuntime.lastCapsuleHitPosition.y, retreatRuntime.lastCapsuleHitPosition.z);
    ImGui::Text(U8("Hit Distance: %.3f m"), retreatRuntime.lastCapsuleHitDistance);
    ImGui::SeparatorText(U8("Retreat Sweep Support Debug"));
    ImGui::Text(U8("Retreat Sweep InitialOverlap: %s"), tf(retreatRuntime.retreatSweepInitialOverlap));
    ImGui::Text(U8("Retreat Sweep Hit Normal: (%.3f, %.3f, %.3f)"),
        retreatRuntime.retreatSweepHitNormal.x, retreatRuntime.retreatSweepHitNormal.y,
        retreatRuntime.retreatSweepHitNormal.z);
    ImGui::Text(U8("Retreat Sweep Normal Y: %.3f"), retreatRuntime.retreatSweepHitNormal.y);
    ImGui::Text(U8("Retreat Sweep Penetration Depth: %.3f m"),
        retreatRuntime.retreatSweepPenetrationDepth);
    ImGui::Text(U8("Retreat Sweep Hit Actor: %s"), retreatRuntime.retreatSweepHitActor.c_str());
    ImGui::Text(U8("Retreat Sweep Hit Component: %s"), retreatRuntime.retreatSweepHitComponent.c_str());
    ImGui::Text(U8("Retreat Sweep 無視したSupport数: %d"),
        retreatRuntime.retreatSweepIgnoredSupportContactCount);
    ImGui::Text(U8("Retreat Sweep Path Blocked: %s"), tf(retreatRuntime.retreatSweepPathBlocked));
    ImGui::Text(U8("Retreat Sweep 最終Block Normal: (%.3f, %.3f, %.3f)"),
        retreatRuntime.retreatSweepBlockingNormal.x, retreatRuntime.retreatSweepBlockingNormal.y,
        retreatRuntime.retreatSweepBlockingNormal.z);
    ImGui::Text(U8("Retreat Sweep 最終Block Distance: %.3f m"),
        retreatRuntime.retreatSweepBlockingDistance);
    ImGui::Text(U8("Retreat Sweep 最終Block Actor: %s"),
        retreatRuntime.retreatSweepBlockingActor.c_str());
    ImGui::Text(U8("Retreat Sweep 最終Block Component: %s"),
        retreatRuntime.retreatSweepBlockingComponent.c_str());
    ImGui::Text(U8("Capsule Point1: (%.2f, %.2f, %.2f)"), retreatRuntime.capsulePoint1.x,
        retreatRuntime.capsulePoint1.y, retreatRuntime.capsulePoint1.z);
    ImGui::Text(U8("Capsule Point2: (%.2f, %.2f, %.2f)"), retreatRuntime.capsulePoint2.x,
        retreatRuntime.capsulePoint2.y, retreatRuntime.capsulePoint2.z);
    ImGui::Text(U8("Capsule Radius: %.3f / Segment Length: %.3f"),
        retreatRuntime.capsuleRadius, retreatRuntime.capsuleSegmentLength);
    ImGui::SeparatorText(U8("Retreat Capsule形状比較"));
    ImGui::Text(U8("通常Collision 元 Radius / Height: %.3f / %.3f"),
        retreatRuntime.collisionConfiguredRadius, retreatRuntime.collisionConfiguredHeight);
    ImGui::Text(U8("通常Collision Component Position: (%.3f, %.3f, %.3f)"),
        retreatRuntime.collisionComponentPosition.x, retreatRuntime.collisionComponentPosition.y,
        retreatRuntime.collisionComponentPosition.z);
    ImGui::Text(U8("通常Collision Component Scale: (%.3f, %.3f, %.3f)"),
        retreatRuntime.collisionComponentScale.x, retreatRuntime.collisionComponentScale.y,
        retreatRuntime.collisionComponentScale.z);
    ImGui::Text(U8("通常Collision Offset Y / PhysX HalfHeight: %.3f / %.3f"),
        retreatRuntime.collisionOffsetY, retreatRuntime.collisionPhysXHalfHeight);
    ImGui::Text(U8("Retreat Sweep方式: %s"), retreatRuntime.retreatSweepUsesSphere ? "SphereCast" : "CapsuleCast");
    ImGui::Text(U8("通常Collision SphereCenter Point1: (%.3f, %.3f, %.3f)"),
        retreatRuntime.collisionPhysXPoint1.x, retreatRuntime.collisionPhysXPoint1.y,
        retreatRuntime.collisionPhysXPoint1.z);
    ImGui::Text(U8("通常Collision SphereCenter Point2: (%.3f, %.3f, %.3f)"),
        retreatRuntime.collisionPhysXPoint2.x, retreatRuntime.collisionPhysXPoint2.y,
        retreatRuntime.collisionPhysXPoint2.z);
    ImGui::Text(U8("通常Collision World Y範囲: %.3f ～ %.3f"),
        retreatRuntime.collisionPhysXMinY, retreatRuntime.collisionPhysXMaxY);
    ImGui::Text(U8("Retreat Cast World Y範囲: %.3f ～ %.3f"),
        retreatRuntime.retreatCastMinY, retreatRuntime.retreatCastMaxY);
    ImGui::Text(U8("World Debug 緑: 通常Collision / 赤: Retreat Cast開始形状"));
    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.9f, 1.0f), U8("World Debug 桃: Clamp失敗"));
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.15f, 1.0f), U8("World Debug 黄: 距離増加失敗"));
    ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), U8("World Debug 水: 最低移動距離失敗"));
    ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.15f, 1.0f), U8("World Debug 赤: CapsuleCast失敗"));
    ImGui::Text(U8("Retreat再試行残り: %.2f sec"), retreatRetryCooldownRemaining);
    ImGui::Text(U8("Retreat失敗理由: %s"), retreatRuntime.failureReason.c_str());
    ImGui::SeparatorText(U8("Reposition BT"));
    ImGui::Text(U8("連続Attack回数: %d"), consecutiveAttackCount);
    const int chanceIndex = (std::min)(consecutiveAttackCount, 3);
    ImGui::DragFloat(U8("Reposition選択確率"), &repositionChanceByAttackCount[chanceIndex], 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::Text(U8("Reposition抽選値: %.3f"), repositionDecisionRoll);
    ImGui::Text(U8("Reposition抽選結果: %s"), repositionDecisionResult ? "当選" : "落選");
    ImGui::Text(U8("現在のCombat Decision: %s"), lastCombatDecision.c_str());
    ImGui::Text(U8("Root 推論番号: %llu"), combatDecisionDebugInferenceSerial);
    ImGui::Text(U8("Root 推論開始activeNode: %s"), combatDecisionDebugActiveNodeAtStart.c_str());
    ImGui::Text(U8("Root Defensive Judgment: %s"), combatDecisionDebugDefensiveResult.c_str());
    ImGui::Text(U8("Root 最終選択Node: %s"), combatDecisionDebugRootSelectedNode.c_str());
    const bool playerAvailableForBossPlans = CanPlanAttackAgainstCurrentPlayer();
    ImGui::Text(U8("Boss Attack抑制中: %s"), playerAvailableForBossPlans ? "false" : "true");
    ImGui::Text(U8("Boss Defensive抑制中: %s"), playerAvailableForBossPlans ? "false" : "true");
    ImGui::Text(U8("CanPlanAnyCombatDecision: %s"), combatDecisionDebugCanPlanAny ? "true" : "false");
    ImGui::Text(U8("CanPlanReposition: %s (%s)"), combatDecisionDebugCanPlanReposition ? "true" : "false", combatDecisionDebugRepositionReason.c_str());
    ImGui::Text(U8("CanPlanAnyAttack: %s [Fast=%s Jump=%s Dash=%s Charge=%s]"),
        combatDecisionDebugCanPlanAnyAttack ? "true" : "false", combatDecisionDebugFastCombo ? "true" : "false",
        combatDecisionDebugJump ? "true" : "false", combatDecisionDebugDash ? "true" : "false", combatDecisionDebugCharge ? "true" : "false");
    ImGui::Text(U8("Combat Decision失敗理由: %s"), combatDecisionDebugFailureReason.c_str());
    ImGui::Text(U8("Repositionクールダウン残り: %.2f"), repositionCooldownRemaining);
    ImGui::Text(U8("Reposition再試行待ち: %.2f"), repositionRetryCooldownRemaining);
    ImGui::Text(U8("最後にCommitしたCombat Decision: %s"), lastCombatDecision.c_str());
    ImGui::DragFloat(U8("Reposition距離 最小"), &repositionDistanceMin, 0.1f, 0.0f, 30.0f, "%.2f");
    ImGui::DragFloat(U8("Reposition距離 最大"), &repositionDistanceMax, 0.1f, 0.0f, 30.0f, "%.2f");
    ImGui::DragFloat(U8("Reposition 最低移動距離"), &repositionMinimumMoveDistance, 0.1f, 0.0f, 30.0f, "%.2f");
    ImGui::DragFloat(U8("Reposition Player最小距離"), &repositionMinimumPlayerDistance, 0.1f, 0.0f, 30.0f, "%.2f");
    ImGui::DragFloat(U8("Reposition 到着後待機時間"), &repositionArrivalWaitDuration, 0.01f, 0.0f, 2.0f, "%.2f");
    ImGui::Text(U8("Reposition Target: (%.2f, %.2f, %.2f)"), repositionTarget.targetPosition.x, repositionTarget.targetPosition.y, repositionTarget.targetPosition.z);
    ImGui::Text(U8("Reposition予定移動距離: %.2f"), repositionRuntime.plannedMoveDistance);
    ImGui::Text(U8("Reposition Target時Player距離: %.2f"), repositionRuntime.plannedPlayerDistance);
    ImGui::Text(U8("Reposition残り距離: %.2f"), repositionRemainingDistance);
    ImGui::Text(U8("Reposition候補数: %d"), repositionRuntime.candidateCount);
    ImGui::Text(U8("Reposition Safe Candidate数: %d"), repositionRuntime.safeCandidateCount);
    ImGui::Text(U8("Reposition 選択Candidate Index: %d"), repositionRuntime.selectedCandidateIndex);
    ImGui::Text(U8("Reposition 選択角度: %.1f deg"), repositionRuntime.selectedCandidateAngleDegrees);
    ImGui::Text(U8("Reposition 選択距離: %.2f m"), repositionRuntime.selectedCandidateDistance);
    ImGui::Text(U8("Initial Reposition: %s"), repositionRuntime.selectedInitialReposition ? "true" : "false");
    if (ImGui::TreeNode(U8("Reposition Candidate Debug")))
    {
        for (const auto& candidate : repositionRuntime.candidates)
        {
            const char* state = candidate.selected ? "Selected" : candidate.accepted ? "Safe" : "Reject";
            ImGui::Text(U8("[%d] %s Angle %.1f Distance %.2f Position (%.2f, %.2f, %.2f)"),
                candidate.candidateIndex, state, candidate.angleDegrees, candidate.candidateDistance,
                candidate.position.x, candidate.position.y, candidate.position.z);
        }
        ImGui::TreePop();
    }
    ImGui::Text(U8("Reposition Clamp適用数: %d"), repositionRuntime.clampAppliedCount);
    ImGui::Text(U8("Reposition Clamp後採用数: %d"), repositionRuntime.clampPostClampAcceptedCount);
    ImGui::Text(U8("Reposition Clamp後不採用数: %d"), repositionRuntime.clampPostClampRejectCount);
    ImGui::Text(U8("Reposition Clamp後Target: (%.2f, %.2f, %.2f)"), repositionRuntime.lastClampedTarget.x, repositionRuntime.lastClampedTarget.y, repositionRuntime.lastClampedTarget.z);
    ImGui::Text(U8("Reposition Clamp距離: %.2f"), repositionRuntime.lastClampDistance);
    ImGui::Text(U8("Reposition Player距離不足数: %d"), repositionRuntime.playerDistanceTooCloseRejectCount);
    ImGui::Text(U8("Reposition 最低移動距離失敗数: %d"), repositionRuntime.minimumMoveRejectCount);
    ImGui::Text(U8("Reposition Sweep失敗数: %d"), repositionRuntime.sweepRejectCount);
    ImGui::Text(U8("Reposition失敗理由: %s"), repositionRuntime.failureReason.c_str());
    if (!g_repositionSweepDiagnostics.empty())
    {
        auto top = [](const std::map<std::string, int>& counts)
        {
            std::vector<std::pair<std::string, int>> values(counts.begin(), counts.end());
            std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
            return values;
        };
        const auto actors = top(g_repositionSweepActorCounts);
        const auto components = top(g_repositionSweepComponentCounts);
        ImGui::SeparatorText(U8("Reposition Sweep Hit集計"));
        for (size_t i = 0; i < (std::min)(size_t(3), actors.size()); ++i)
            ImGui::Text(U8("Actor %d: %s (%d)"), static_cast<int>(i + 1), actors[i].first.c_str(), actors[i].second);
        for (size_t i = 0; i < (std::min)(size_t(3), components.size()); ++i)
            ImGui::Text(U8("Component %d: %s (%d)"), static_cast<int>(i + 1), components[i].first.c_str(), components[i].second);
        const auto& last = g_repositionSweepDiagnostics.back();
        ImGui::Text(U8("最多Hit Actor: %s"), actors.empty() ? "None" : actors.front().first.c_str());
        ImGui::Text(U8("最多Hit Component: %s"), components.empty() ? "None" : components.front().first.c_str());
        ImGui::Text(U8("最多Hit回数: %d"), actors.empty() ? 0 : actors.front().second);
        ImGui::Text(U8("Sweep開始: (%.2f, %.2f, %.2f)"), last.start.x, last.start.y, last.start.z);
        ImGui::Text(U8("Clamp後Target: (%.2f, %.2f, %.2f)"), last.target.x, last.target.y, last.target.z);
        ImGui::Text(U8("Sweep方向: (%.3f, %.3f, %.3f) 距離 %.3f"), last.direction.x, last.direction.y, last.direction.z, last.distance);
        ImGui::Text(U8("Shape Center: (%.2f, %.2f, %.2f) Radius %.3f HalfHeight %.3f"), last.center.x, last.center.y, last.center.z, last.radius, last.halfHeight);
        ImGui::Text(U8("InitialOverlap: %s Distance: %.3f Penetration: %.3f"), tf(last.initialOverlap), last.blockingDistance, last.penetrationDepth);
        ImGui::Text(U8("Normal: (%.3f, %.3f, %.3f)"), last.normal.x, last.normal.y, last.normal.z);
        ImGui::Text(U8("Block位置: (%.2f, %.2f, %.2f) Normal: (%.3f, %.3f, %.3f)"),
            last.blockingPosition.x, last.blockingPosition.y, last.blockingPosition.z,
            last.blockingNormal.x, last.blockingNormal.y, last.blockingNormal.z);
        ImGui::Text(U8("Hit Actor: %s Component: %s Layer: 0x%X"), last.blockingActorName.c_str(), last.blockingComponentName.c_str(), last.blockingLayer);
        ImGui::Text(U8("Support無視数: %d PathBlocked: %s"), last.ignoredSupportContactCount, tf(last.pathBlocked));
        ImGui::Text(U8("Support Contact: %s InitialOverlap: %s NormalY: %.3f Penetration: %.3f"),
            tf(last.supportContactHit), tf(last.supportInitialOverlap), last.supportNormal.y, last.supportPenetrationDepth);
    }
#endif
}

void GruxEnemy::DrawRetreatDebugWorld() const
{
#ifdef USE_IMGUI
    const float y = retreatRuntime.gruxSnapshot.y + 0.15f;
    DirectX::XMFLOAT3 start = retreatRuntime.gruxSnapshot;
    start.y = y;
    if (retreatRuntime.candidateCount > 0)
    {
        DebugRender::DrawSphere(start, 0.25f, { 0.2f, 0.8f, 1.0f, 1.0f }, 0.0f, true);

        // Draw the exact medial-axis endpoints plus end spheres.  DebugRender::DrawCapsule
        // does not consume its end position, so spheres and a line are used to make the
        // compared PhysX shapes unambiguous.
        const DirectX::XMFLOAT4 collisionColor{ 0.2f, 1.0f, 0.25f, 1.0f };
        const DirectX::XMFLOAT4 retreatCastColor{ 1.0f, 0.15f, 0.15f, 1.0f };
        DebugRender::DrawLine(retreatRuntime.collisionPhysXPoint1, retreatRuntime.collisionPhysXPoint2,
            collisionColor, 0.0f, true);
        DebugRender::DrawSphere(retreatRuntime.collisionPhysXPoint1,
            retreatRuntime.collisionConfiguredRadius, collisionColor, 0.0f, true);
        DebugRender::DrawSphere(retreatRuntime.collisionPhysXPoint2,
            retreatRuntime.collisionConfiguredRadius, collisionColor, 0.0f, true);
        DebugRender::DrawLine(retreatRuntime.capsulePoint1, retreatRuntime.capsulePoint2,
            retreatCastColor, 0.0f, true);
        DebugRender::DrawSphere(retreatRuntime.capsulePoint1,
            retreatRuntime.capsuleRadius, retreatCastColor, 0.0f, true);
        DebugRender::DrawSphere(retreatRuntime.capsulePoint2,
            retreatRuntime.capsuleRadius, retreatCastColor, 0.0f, true);
    }
    for (const auto& candidate : retreatRuntime.candidates)
    {
        DirectX::XMFLOAT3 point = candidate.position;
        point.y = y;
        const DirectX::XMFLOAT4 color = candidate.accepted ? DirectX::XMFLOAT4{ 0.2f, 1.0f, 0.3f, 1.0f }
            : candidate.rejectReason == RetreatCandidateRejectReason::Clamp ? DirectX::XMFLOAT4{ 1.0f, 0.2f, 0.9f, 1.0f }
            : candidate.rejectReason == RetreatCandidateRejectReason::DistanceIncrease ? DirectX::XMFLOAT4{ 1.0f, 0.85f, 0.15f, 1.0f }
            : candidate.rejectReason == RetreatCandidateRejectReason::MinimumMoveDistance ? DirectX::XMFLOAT4{ 0.2f, 0.7f, 1.0f, 1.0f }
            : candidate.rejectReason == RetreatCandidateRejectReason::CapsuleCast ? DirectX::XMFLOAT4{ 1.0f, 0.15f, 0.15f, 1.0f }
            : DirectX::XMFLOAT4{ 0.6f, 0.6f, 0.6f, 1.0f };
        DebugRender::DrawSphere(point, candidate.accepted ? 0.28f : 0.14f, color, 0.0f, true);
        DebugRender::DrawLine(start, point, color, 0.0f, true);
    }
    if (retreatTarget.valid)
    {
        DirectX::XMFLOAT3 target = retreatTarget.targetPosition;
        target.y = y;
        DebugRender::DrawSphere(target, 0.32f, { 1.0f, 0.8f, 0.15f, 1.0f }, 0.0f, true);
        DebugRender::DrawLine(start, target, { 1.0f, 0.8f, 0.15f, 1.0f }, 0.0f, true);
    }
    for (const auto& sweep : g_repositionSweepDiagnostics)
    {
        DebugRender::DrawLine(sweep.start, sweep.target, { 1.0f, 0.1f, 0.8f, 1.0f }, 0.0f, true);
        const DirectX::XMFLOAT3 block{
            sweep.start.x + sweep.direction.x * sweep.blockingDistance,
            sweep.start.y + sweep.direction.y * sweep.blockingDistance,
            sweep.start.z + sweep.direction.z * sweep.blockingDistance };
        DebugRender::DrawSphere(block, 0.18f, { 1.0f, 0.05f, 0.05f, 1.0f }, 0.0f, true);
        DebugRender::DrawLine(block, { block.x + sweep.normal.x, block.y + sweep.normal.y, block.z + sweep.normal.z },
            { 1.0f, 0.9f, 0.1f, 1.0f }, 0.0f, true);
    }
#else
    (void)this;
#endif
}
