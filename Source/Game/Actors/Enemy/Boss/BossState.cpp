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
            return;
        }

        if (enemy->IsOutsideAllAttackRanges(context.xzDistance))
        {
            enemy->SetLastAIDecision("Approach: outside all attack ranges");
            owner->GetStateMachine()->ChangeState("EnemyApproachState");
            return;
        }

        if (!enemy->IsFacingPlayerForAttack(context))
        {
            enemy->SetLastAIDecision("Turn: Player outside attack facing angle");
            owner->GetStateMachine()->ChangeState("EnemyTurnState");
            return;
        }
    }

#if 1
    timer += deltaTime;
    if (timer < enemy->GetAttackInterval())
        return;

#endif // 1

    if (!enemy->SelectAttackForCurrentMode())
    {
        enemy->SetLastAIDecision("Wait: no weighted attack candidate");
        timer = (std::max)(0.0f, enemy->GetAttackInterval() - 0.25f);
        return;
    }

    enemy->SetLastAIDecision(
        enemy->GetBossAIMode() == BossAIMode::DebugFixedAttack
        ? "Attack: DebugFixedAttack" : "Attack: weighted selection");
    attackSelected = true;
    owner->GetStateMachine()->ChangeState("EnemyAttackState");
}
void EnemyThinkState::Exit()
{
}


void EnemyApproachState::Enter()
{
    enemy->BeginApproach();
}

void EnemyApproachState::Execute(float deltaTime)
{
    if (enemy->GetBossAIMode() != BossAIMode::CombatAI)
    {
        enemy->SetLastAIDecision("Approach ended: AI mode changed");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    const BossTargetContext context = enemy->BuildTargetContext();
    if (!context.valid)
    {
        enemy->SetLastAIDecision("Approach ended: no valid Player target");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    if (!enemy->IsOutsideAllAttackRanges(context.xzDistance))
    {
        enemy->SetLastAIDecision("Approach complete: entered attack range");
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
        return;
    }

    enemy->UpdateApproachMovement(context.directionToPlayer, deltaTime);
}

void EnemyApproachState::Exit()
{
    enemy->StopAIMovement();
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
    enemy->StartAttack();
    stageStartHitCount = enemy->GetCurrentAttackHitCount();
    if (!enemy->PlayAttackStage(enemy->GetSelectedAttackType(), comboStage))
        owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
}

void EnemyAttackState::Execute(float deltaTime)
{
    const BossAttackType attackType = enemy->GetSelectedAttackType();

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
    enemy->ClearJumpAttackMotionWarpOverride();
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
    if (timer >= enemy->GetRecoveryDuration())
        owner->GetStateMachine()->ChangeState("EnemyThinkState");
}

void EnemyRecoveryState::Exit()
{
}
