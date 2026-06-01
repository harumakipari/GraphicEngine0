#include "pch.h"
#include "BossState.h"
#include "Game/Actors/Enemy/Enemy.h"

EnemyStateBase::EnemyStateBase(Enemy* actor) :State(actor), enemy(actor)
{
}

// 待機ステートオブジェクト
void EnemyIdleState::Enter()
{
    owner->PlayBodyAnimation("Idle");
}

// ステートで実行するメソッド
void EnemyIdleState::Execute(float deltaTime)
{

}

void EnemyIdleState::Exit()
{
}

// 移動ステートオブジェクト
void EnemyWalkState::Enter()
{
    owner->PlayBodyAnimation("Jog_Fwd", true, true, 0.2f);
}

void EnemyWalkState::Execute(float deltaTime)
{

}

void EnemyWalkState::Exit()
{
}

// 攻撃ステートオブジェクト
void EnemyAttackState::Enter()
{
    owner->PlayBodyAnimation("Swing1_Medium", false, true, 0.1f);
}

void EnemyAttackState::Execute(float deltaTime)
{

}

void EnemyAttackState::Exit()
{

}

// 攻撃の予兆ステートオブジェクト
void EnemyCastState::Enter()
{
    
}

void EnemyCastState::Execute(float deltaTime)
{

}

void EnemyCastState::Exit()
{
}

