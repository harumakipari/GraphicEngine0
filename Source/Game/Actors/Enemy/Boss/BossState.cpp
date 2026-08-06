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
    timer = 0.0f;
    owner->PlayBodyAnimation("TravelMode_Idle_0");
}

// ステートで実行するメソッド
void EnemyIdleState::Execute(float deltaTime)
{
    timer += deltaTime;

    if (timer >= waitTime)
    {
        owner->GetStateMachine()->ChangeState("EnemyAttackState");
    }
}

void EnemyIdleState::Exit()
{
}


// 次の攻撃を考えるステートオブジェクト
void EnemyThinkState::Enter()
{

}

// ステートで実行するメソッド
void EnemyThinkState::Execute(float deltaTime)
{

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
    enemy->StartAttack();
    owner->PlayBodyAnimation("PrimaryAttack_LA", false, true, 0.1f);
}

void EnemyAttackState::Execute(float deltaTime)
{
    if (!enemy->GetBodyAnimationController()->IsPlayAnimation())
    {
        owner->GetStateMachine()->ChangeState("EnemyIdleState");
    }
}

void EnemyAttackState::Exit()
{
    enemy->DisableAttackHitBoxes();
}
