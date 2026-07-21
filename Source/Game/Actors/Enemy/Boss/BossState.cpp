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


