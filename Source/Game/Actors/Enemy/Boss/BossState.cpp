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

        if (!enemy->IsFacingPlayerForAttack(context))
        {
            enemy->SetLastAIDecision("Turn: Player outside attack facing angle");
            owner->GetStateMachine()->ChangeState("EnemyTurnState");
            return;
        }

        if (!enemy->GetActiveIntent())
            enemy->TryStartIntent(BossIntentType::CloseCombat);
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
        owner->GetStateMachine()->ChangeState("EnemyAttackState");
        return;
    }


    if (!enemy->SelectCombatAction())
    {
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

    enemy->SetLastAIDecision("Action: Attack selected by weighted selection");
    enemy->MarkIntentAttackSelected();
    attackSelected = true;
    owner->GetStateMachine()->ChangeState("EnemyAttackState");
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
    previousPosition = enemy->GetPosition();
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
        endReasonSet = true;
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
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

    enemy->UpdatePositioningDebug(traveledDistance, elapsedTime, stuckTimer);

    const BossTargetContext context = enemy->BuildTargetContext();
    if (!context.valid)
    {
        finish("InvalidPlayer");
        return;
    }

    if (positioningData->completionType == BossPositioningCompletionType::TargetDistance)
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
        finish(positioningData->completionType == BossPositioningCompletionType::TargetDistance
            ? "MaxTravelDistance"
            : "DistanceReached");
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

    DirectX::XMFLOAT3 moveDirection = context.directionToPlayer;
    if (positioningData->direction == BossPositioningDirection::AwayFromPlayer)
    {
        moveDirection.x *= -1.0f;
        moveDirection.z *= -1.0f;
    }
    enemy->UpdatePositioningMovement(moveDirection, moveDirection, deltaTime);
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
    positioningData = std::nullopt;
}

void EnemyTurnState::Enter()
{
    timer = 0.0f;
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
        enemy->SetLastAIDecision("Turn ended: no valid Player target");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    if (context.absoluteAngleDegrees <= enemy->GetTurnCompleteAngle())
    {
        enemy->SetLastAIDecision("Turn complete");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    enemy->RotateTowardsPlayer(
        context.directionToPlayer, enemy->GetTurnSpeed(), deltaTime);

    if (timer >= enemy->GetTurnTimeout())
    {
        enemy->SetLastAIDecision("Turn ended: timeout");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
    }
}

void EnemyTurnState::Exit()
{
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
    enemy->StartAttack();
    stageStartHitCount = enemy->GetCurrentAttackHitCount();
    if (!enemy->PlayAttackStage(enemy->GetSelectedAttackType(), comboStage))
    {
        enemy->OnSelectedActionStartFailed();
        owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
        return;
    }

    enemy->OnSelectedActionStartedSuccessfully();
}

void EnemyAttackState::Execute(float deltaTime)
{
    const BossAttackType attackType = enemy->GetSelectedAttackType();

    if (attackType == BossAttackType::DashAttack && comboStage == 0)
    {
        dashWindupTimer += deltaTime;
        const BossTargetContext context = enemy->BuildTargetContext();
        if (context.valid)
        {
            enemy->RotateTowardsPlayer(
                context.directionToPlayer, enemy->GetTurnSpeed(), deltaTime);
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

    owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
}
void EnemyAttackState::Exit()
{
    enemy->StartSelectedActionCooldown();
    enemy->ClearJumpAttackMotionWarpOverride();
    enemy->StopDashAttackMovement();
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
