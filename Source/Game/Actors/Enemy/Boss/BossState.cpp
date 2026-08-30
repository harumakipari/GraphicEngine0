#include "pch.h"
#include "BossState.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

EnemyStateBase::EnemyStateBase(GruxEnemy* actor) :State(actor), enemy(actor)
{
}

// 待機ステートオブジェクト
void EnemyIdleState::Enter()
{
    owner->PlayBodyAnimation("TravelMode_Idle_0");
}

// ステートで実行するメソッド
void EnemyIdleState::Execute(float deltaTime)
{
    owner->GetStateMachine()->ChangeState("EnemyThinkState");
}
void EnemyIdleState::Exit()
{
}


// 次の攻撃を考えるステートオブジェクト
void EnemyThinkState::Enter()
{
    attackSelected = false;
    timer = 0.0f;
    enemy->BeginIntentReevaluation();
}

void EnemyThinkState::Execute(float deltaTime)
{
    if (attackSelected)
        return;

    if (enemy->GetBossAIMode() == BossAIMode::CombatAI)
    {
        const BossTargetContext context = enemy->BuildTargetContext();
        if (!context.valid)
        {
            enemy->SetLastAIDecision("No valid Player target");
            enemy->FailActiveIntent("InvalidTarget");
            return;
        }

        enemy->InvalidateCloseCombatIntentForBack(context);

        if (!enemy->GetActiveIntent() && !enemy->SelectIntentByWeight())
        {
            enemy->SetLastAIDecision("Wait: no weighted Intent candidate");
            return;
        }
    }

    // デバック固定用の分岐
    if (enemy->GetBossAIMode() == BossAIMode::DebugFixedAttack)
    {
        if (enemy->GetActiveIntent())
            enemy->ClearActiveIntent();

        timer += deltaTime;
        if (timer < enemy->GetAttackInterval())
            return;

        if (!enemy->SelectAttackForCurrentMode())
        {
            enemy->SetLastAIDecision("Wait: DebugFixedAttack selection failed");
            return;
        }
        enemy->SetLastAIDecision("Attack: DebugFixedAttack");
        attackSelected = true;
        owner->GetStateMachine()->ChangeState(
            enemy->GetSelectedAttackType() == BossAttackType::ChargeAttack
                ? "EnemyChargeAttackState"
                : "EnemyAttackState");
        return;
    }


    const BossTargetContext actionContext = enemy->BuildTargetContext();
    if (enemy->ShouldFailIntentForPositioningRetryLimit(actionContext))
    {
        enemy->SetLastAIDecision("Intent Failed: Positioning retry limit");
        enemy->FailActiveIntent("PositioningRetryLimit");
        timer = (std::max)(0.0f, enemy->GetAttackInterval() - 0.25f);
        return;
    }

    if (!enemy->SelectCombatAction())
    {
        const BossTargetContext context = enemy->BuildTargetContext();
        if (enemy->ShouldWaitForActiveIntentCooldown(context))
        {
            enemy->SetLastAIDecision("Wait: active Intent Action cooldown");
            return;
        }
        enemy->SetLastAIDecision("Wait: no weighted attack candidate");
        enemy->FailActiveIntent("NoActionCandidate");
        timer = (std::max)(0.0f, enemy->GetAttackInterval() - 0.25f);
        return;
    }

    if (enemy->GetSelectedPositioningData())
    {
        enemy->SetLastAIDecision("Action: Positioning selected by weighted selection");
        enemy->MarkIntentPositioningAttempted();
        attackSelected = true;
        owner->GetStateMachine()->ChangeState("EnemyPositioningState");
        return;
    }

    if (!enemy->PrepareAttackForSelectedAction())
    {
        enemy->SetLastAIDecision("Wait: selected Action has no executable mapping");
        enemy->FailActiveIntent("ActionMappingFailed");
        timer = (std::max)(0.0f, enemy->GetAttackInterval() - 0.25f);
        return;
    }
    const BossTargetContext facingContext = enemy->BuildTargetContext();
    if (enemy->PreparePendingAttackFacing(facingContext))
    {
        enemy->SetLastAIDecision("Turn: selected Attack requires facing");
        enemy->MarkIntentAttackSelected();
        attackSelected = true;
        owner->GetStateMachine()->ChangeState("EnemyTurnState");
        return;
    }


    enemy->SetLastAIDecision("Action: Attack selected by weighted selection");
    enemy->MarkIntentAttackSelected();
    attackSelected = true;
    if (enemy->IsCloseCombatAttackType(enemy->GetSelectedAttackType()))
    {
        enemy->SetAttackReadyReason(AttackReadyReason::Front);
        owner->GetStateMachine()->ChangeState("EnemyAttackReadyState");
    }
    else
    {
        owner->GetStateMachine()->ChangeState(
            enemy->GetSelectedAttackType() == BossAttackType::ChargeAttack
                ? "EnemyChargeAttackState"
                : "EnemyAttackState");
    }
}
void EnemyThinkState::Exit()
{
}


void EnemyPositioningState::Enter()
{
    positioningData = enemy->GetSelectedPositioningData();
    traveledDistance = 0.0f;
    elapsedTime = 0.0f;
    stuckTimer = 0.0f;
    settleTimer = 0.0f;
    settling = false;
    settleCompletionReason = "WorldTargetReached";
    previousPosition = enemy->GetPosition();
    enemy->SetCombatRepositionSettleDebug(false, 0.0f);
    endReasonSet = false;

    if (!positioningData)
    {
        enemy->SetLastAIDecision("Positioning End: Interrupted (missing data)");
        enemy->FinishPositioningDebug("Interrupted");
        endReasonSet = true;
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    enemy->BeginPositioning(*positioningData);
}

void EnemyPositioningState::Execute(float deltaTime)
{
    auto finish = [this](const char* reason)
    {
        enemy->SetLastAIDecision(std::string("Positioning End: ") + reason);
        enemy->FinishPositioningDebug(reason);
        const bool completed =
            std::strcmp(reason, "WorldTargetReached") == 0 ||
            std::strcmp(reason, "TargetDistanceReached") == 0 ||
            std::strcmp(reason, "DistanceReached") == 0;
        if (completed && enemy->IsCombatRepositionActive())
            enemy->CompleteCombatReposition();
        if (!completed)
            enemy->FailActiveIntent(reason);
        endReasonSet = true;
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
    };

    auto beginCombatRepositionSettle = [this, &finish](const char* completionReason)
    {
        enemy->StopAIMovement();
        enemy->EndPositioningAnimation();
        settling = true;
        settleTimer = 0.0f;
        settleCompletionReason = completionReason;
        const float duration = enemy->GetCombatRepositionSettleDuration();
        enemy->SetCombatRepositionSettleDebug(true, duration);
        if (duration <= 0.0f)
            finish(settleCompletionReason);
    };

    if (enemy->GetBossAIMode() != BossAIMode::CombatAI)
    {
        finish("AIModeChanged");
        return;
    }
    if (!positioningData)
    {
        finish("Interrupted");
        return;
    }

    if (settling)
    {
        if (!enemy->BuildTargetContext().valid)
        {
            finish("InvalidPlayer");
            return;
        }
        enemy->StopAIMovement();
        settleTimer += deltaTime;
        const float duration = enemy->GetCombatRepositionSettleDuration();
        enemy->SetCombatRepositionSettleDebug(
            true, (std::max)(0.0f, duration - settleTimer));
        if (settleTimer >= duration)
            finish(settleCompletionReason);
        return;
    }

    const DirectX::XMFLOAT3 currentPosition = enemy->GetPosition();
    const float deltaX = currentPosition.x - previousPosition.x;
    const float deltaZ = currentPosition.z - previousPosition.z;
    const float frameMovement = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
    traveledDistance += frameMovement;
    previousPosition = currentPosition;
    elapsedTime += deltaTime;

    const float actualMovementSpeed = deltaTime > 0.0f ? frameMovement / deltaTime : 0.0f;
    if (actualMovementSpeed < positioningData->stuckMovementThreshold)
        stuckTimer += deltaTime;
    else
        stuckTimer = 0.0f;

    const BossTargetContext context = enemy->BuildTargetContext();
    DirectX::XMFLOAT3 fixedTarget{};
    const bool hasFixedTarget = enemy->GetFixedPositioningTarget(fixedTarget);
    DirectX::XMFLOAT3 requestedMoveDirection = context.directionToPlayer;
    float targetRemainingDistance = FLT_MAX;
    if (hasFixedTarget)
    {
        const float targetX = fixedTarget.x - currentPosition.x;
        const float targetZ = fixedTarget.z - currentPosition.z;
        targetRemainingDistance = std::sqrt(targetX * targetX + targetZ * targetZ);
        if (targetRemainingDistance > 0.0001f)
            requestedMoveDirection = { targetX / targetRemainingDistance, 0.0f, targetZ / targetRemainingDistance };
        else
            requestedMoveDirection = { 0.0f, 0.0f, 0.0f };
    }
    else if (positioningData->direction == BossPositioningDirection::AwayFromPlayer)
    {
        requestedMoveDirection.x *= -1.0f;
        requestedMoveDirection.z *= -1.0f;
    }
    enemy->UpdatePositioningDebug(traveledDistance, elapsedTime, stuckTimer,
        frameMovement, actualMovementSpeed, requestedMoveDirection);
    enemy->UpdatePositioningAnimation(actualMovementSpeed, deltaTime);

    if (!context.valid)
    {
        finish("InvalidPlayer");
        return;
    }
    if (hasFixedTarget)
    {
        if (targetRemainingDistance <= enemy->GetPositioningArrivalDistance())
        {
            if (enemy->IsCombatRepositionActive())
            {
                beginCombatRepositionSettle("WorldTargetReached");
                return;
            }
            if (!enemy->IsCombatRepositionActive())
                enemy->MarkIntentPositioningCompleted();
            finish("WorldTargetReached");
            return;
        }
    }
    else if (positioningData->completionType == BossPositioningCompletionType::TargetDistance)
    {
        const bool targetDistanceReached =
            positioningData->direction == BossPositioningDirection::TowardPlayer
            ? context.xzDistance <= positioningData->targetDistance
            : context.xzDistance >= positioningData->targetDistance;
        if (targetDistanceReached)
        {
            finish("TargetDistanceReached");
            return;
        }
    }
    if (traveledDistance >= positioningData->maxMoveDistance)
    {
        const char* reason = positioningData->completionType == BossPositioningCompletionType::TargetDistance
            ? "MaxTravelDistance" : "DistanceReached";
        if (enemy->IsCombatRepositionActive() &&
            std::strcmp(reason, "DistanceReached") == 0)
        {
            beginCombatRepositionSettle(reason);
            return;
        }
        finish(reason);
        return;
    }
    if (elapsedTime >= positioningData->timeout)
    {
        finish("Timeout");
        return;
    }
    if (stuckTimer >= positioningData->stuckTimeThreshold)
    {
        finish("Stuck");
        return;
    }

    enemy->UpdatePositioningMovement(requestedMoveDirection, requestedMoveDirection, deltaTime);
}

void EnemyPositioningState::Exit()
{
    if (!endReasonSet)
    {
        enemy->SetLastAIDecision("Positioning End: Interrupted");
        enemy->FinishPositioningDebug("Interrupted");
    }
    enemy->StartSelectedActionCooldown();
    enemy->StopAIMovement();
    enemy->SetCombatRepositionSettleDebug(false, 0.0f);
    enemy->EndPositioningAnimation();
    positioningData = std::nullopt;
}

void EnemyTurnState::Enter()
{
    timer = 0.0f;
    enemy->BeginTurnRotationDebug("SelectedAttack");
    enemy->StopAIMovement();
    owner->PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
}

void EnemyTurnState::Execute(float deltaTime)
{
    if (enemy->GetBossAIMode() != BossAIMode::CombatAI)
    {
        enemy->SetLastAIDecision("Turn ended: AI mode changed");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    timer += deltaTime;
    const BossTargetContext context = enemy->BuildTargetContext();
    if (!context.valid)
    {
        enemy->ClearPendingAttackFacing();
        enemy->SetLastAIDecision("Turn ended: no valid Player target");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    if (!enemy->HasPendingAttackFacing())
    {
        enemy->SetLastAIDecision("Turn ended: no pending Attack facing");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    if (enemy->GetPendingAttackFacingAngle() <=
        enemy->GetPendingAttackFacingCompleteAngle())
    {
        enemy->SetLastAIDecision("Turn complete: resume selected Attack");
        if (!enemy->ResumeSelectedAttackAfterTurn())
            owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    enemy->RotateTowardsPlayer(
        enemy->GetPendingAttackFacingDirection(),
        enemy->GetTurnSpeed(), deltaTime, "TurnState");

    if (timer >= enemy->GetTurnTimeout())
    {
        enemy->SetLastAIDecision("Turn ended: timeout");
        enemy->ClearPendingAttackFacing();
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
    }
}

void EnemyTurnState::Exit()
{
    enemy->ClearPendingAttackFacing();
    enemy->EndTurnRotationDebug(timer);
    enemy->StopAIMovement();
}
void EnemyAttackReadyState::Enter()
{
    timer = 0.0f;
    enemy->StopAIMovement();
    enemy->BeginAttackReadyDebug();
    enemy->PlayAttackReadySE();
}

void EnemyAttackReadyState::Execute(float deltaTime)
{
    if (enemy->GetBossAIMode() != BossAIMode::CombatAI)
    {
        enemy->SetLastAIDecision("Attack Ready ended: AI mode changed");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    if (!enemy->IsCloseCombatAttackType(enemy->GetSelectedAttackType()))
    {
        enemy->SetLastAIDecision("Attack Ready ended: invalid Attack type");
        enemy->FailActiveIntent("AttackReadyInvalidAttack");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    const BossTargetContext context = enemy->BuildTargetContext();
    if (!context.valid)
    {
        enemy->SetLastAIDecision("Attack Ready ended: invalid Player target");
        enemy->FailActiveIntent("InvalidTarget");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    timer += deltaTime;
    enemy->UpdateAttackReadyDebug(timer);
    if (timer >= enemy->GetAttackReadyDuration())
    {
        enemy->SetLastAIDecision("Attack Ready complete: start selected Attack");
        owner->GetStateMachine()->ChangeState("EnemyAttackState");
    }
}

void EnemyAttackReadyState::Exit()
{
    enemy->EndAttackReadyDebug();
    enemy->StopAIMovement();
}

// 攻撃の予兆ステートオブジェクト
void EnemyDeathState::Enter()
{
    owner->PlayBodyAnimation("Death_A_0", false, true, 0.1f);
}

void EnemyDeathState::Execute(float deltaTime)
{

}

void EnemyDeathState::Exit()
{
}


// 攻撃の予兆ステートオブジェクト
void EnemyAttackState::Enter()
{
    comboStage = 0;
    stageStartHitCount = 0;
    dashWindupTimer = 0.0f;
    const bool isJumpAttack = enemy->GetSelectedAttackType() == BossAttackType::JumpAttack;
    enemy->StopAIMovement();
    if (!isJumpAttack)
        enemy->StartAttack();
    stageStartHitCount = enemy->GetCurrentAttackHitCount();
    if (!enemy->PlayAttackStage(enemy->GetSelectedAttackType(), comboStage))
    {
        enemy->OnSelectedActionStartFailed();
        owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
        return;
    }

    if (!isJumpAttack)
        enemy->OnSelectedActionStartedSuccessfully();
}

void EnemyAttackState::Execute(float deltaTime)
{
    const BossAttackType attackType = enemy->GetSelectedAttackType();

    if (attackType == BossAttackType::JumpAttack && comboStage == 0)
    {
        enemy->StopAIMovement();
        const BossTargetContext context = enemy->BuildTargetContext();
        if (context.valid)
        {
            enemy->RotateTowardsPlayer(
                context.directionToPlayer, enemy->GetTurnSpeed(), deltaTime, "JumpTelegraph");
        }

        if (enemy->GetBodyAnimationController()->IsPlayAnimation())
            return;

        enemy->StartAttack();
        stageStartHitCount = enemy->GetCurrentAttackHitCount();
        ++comboStage;
        if (!enemy->PlayAttackStage(attackType, comboStage))
        {
            enemy->OnSelectedActionStartFailed();
            owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
            return;
        }

        enemy->OnSelectedActionStartedSuccessfully();
        return;
    }

    if (attackType == BossAttackType::DashAttack && comboStage == 0)
    {
        dashWindupTimer += deltaTime;
        const BossTargetContext context = enemy->BuildTargetContext();
        if (context.valid)
        {
            enemy->RotateTowardsPlayer(
                context.directionToPlayer, enemy->GetTurnSpeed(), deltaTime, "DashTelegraph");
        }

        if (dashWindupTimer < enemy->GetDashWindupDuration())
            return;

        enemy->BeginAdditionalAttackStage();
        stageStartHitCount = enemy->GetCurrentAttackHitCount();
        ++comboStage;
        if (!enemy->PlayAttackStage(attackType, comboStage))
            owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
        return;
    }

    if (attackType == BossAttackType::DashAttack && comboStage == 1)
    {
        const bool dashFinished = enemy->UpdateDashAttackMovement(deltaTime);
        const bool animationFinished = !enemy->GetBodyAnimationController()->IsPlayAnimation();
        if (!dashFinished && !animationFinished)
            return;

        enemy->StopDashAttackMovement();
        enemy->BeginAdditionalAttackStage();
        stageStartHitCount = enemy->GetCurrentAttackHitCount();
        ++comboStage;
        if (!enemy->PlayAttackStage(attackType, comboStage))
            owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
        return;
    }

    if (attackType == BossAttackType::FastCombo &&
        enemy->IsTransitionWindowActive())
    {
        const bool playerHitThisStage =
            enemy->GetCurrentAttackHitCount() > stageStartHitCount;
        const bool justDodgedThisSequence =
            enemy->WasCurrentAttackSequenceJustDodged();
        if (playerHitThisStage || justDodgedThisSequence)
        {
            enemy->OnSelectedAttackCompletedSuccessfully();
            owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
            return;
        }

        const auto controller = enemy->GetBodyAnimationController();
        const auto* asset = controller
            ? controller->GetAnimationAsset(controller->GetCurrentAnimationName())
            : nullptr;
        if (asset && !asset->nextCombo.empty())
        {
            const std::string nextAnimation = asset->nextCombo;
            enemy->BeginAdditionalAttackStage();
            stageStartHitCount = enemy->GetCurrentAttackHitCount();
            ++comboStage;
            if (!enemy->PlayAttackAnimationByName(nextAnimation))
                owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
            return;
        }
    }

    if (enemy->GetBodyAnimationController()->IsPlayAnimation())
        return;

    enemy->OnSelectedAttackCompletedSuccessfully();
    owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
}
void EnemyAttackState::Exit()
{
    enemy->ClearPendingAttackFacing();
    enemy->StartSelectedActionCooldown();
    enemy->ClearJumpAttackMotionWarpOverride();
    enemy->StopDashAttackMovement();
    enemy->DisableAttackHitBoxes();
}

void EnemyChargeAttackState::Enter()
{
    phase = Phase::Windup;
    enemy->StopAIMovement();
    enemy->StartAttack();
    enemy->SetChargePhaseDebug("Windup");
    enemy->PlayBodyAnimation("Pre_FootSlide_0", false, true, 0.1f, true);
    enemy->OnSelectedActionStartedSuccessfully();
}

void EnemyChargeAttackState::Execute(float deltaTime)
{
    if (phase == Phase::Windup)
    {
        const auto controller = enemy->GetBodyAnimationController();
        const float animationTime = controller
            ? controller->GetCurrentAnimationTime()
            : 0.0f;
        enemy->SetChargeWindupAnimationTimeDebug(animationTime);
        const BossTargetContext context = enemy->BuildTargetContext();
        if (context.valid)
        {
            enemy->RotateTowardsPlayer(
                context.directionToPlayer, enemy->GetTurnSpeed(), deltaTime, "ChargeWindup");
        }

        if (animationTime < enemy->GetChargeWindupEndTime())
            return;

        if (!enemy->BeginChargeAttackMovement())
        {
            owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
            return;
        }

        phase = Phase::Charging;
        enemy->SetChargePhaseDebug("Charging");
        return;
    }

    const ChargeAttackEndReason endReason =
        enemy->UpdateChargeAttackMovement(deltaTime);
    if (endReason != ChargeAttackEndReason::None)
    {
        if (endReason == ChargeAttackEndReason::PlayerHit ||
            endReason == ChargeAttackEndReason::WallHit)
            enemy->OnSelectedAttackCompletedSuccessfully();
        owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
    }
}

void EnemyChargeAttackState::Exit()
{
    enemy->ClearPendingAttackFacing();
    enemy->StartSelectedActionCooldown();
    enemy->StopChargeAttackMovement();
    enemy->DisableAttackHitBoxes();
}

void EnemyRecoveryState::Enter()
{
    timer = 0.0f;
    owner->PlayBodyAnimation("TravelMode_Idle_0");
}

void EnemyRecoveryState::Execute(float deltaTime)
{
    timer += deltaTime;
    if (timer >= enemy->GetRecoveryDurationForCurrentAttack())
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
}

void EnemyRecoveryState::Exit()
{
}
