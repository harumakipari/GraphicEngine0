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

    timer += deltaTime;
    if (timer < enemy->GetAttackInterval())
        return;

    attackSelected = true;
    enemy->SelectAttackForCurrentMode();
    owner->GetStateMachine()->ChangeState("EnemyAttackState");
}
void EnemyThinkState::Exit()
{
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
