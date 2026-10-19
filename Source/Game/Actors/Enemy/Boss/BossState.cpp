#include "pch.h"
#include "BossState.h"

#include "Core/ActorManager.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Scene/Scene.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/Actors/Player/Player.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

EnemyStateBase::EnemyStateBase(GruxEnemy* actor) :State(actor), enemy(actor)
{
}

void EnemyIdleState::Enter()
{
    enemy->PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.3f, false,
        "EnemyIdleState::Enter");
}

void EnemyIdleState::Execute(float)
{
}
void EnemyIdleState::Exit()
{
}


// 攻撃の予兆ステートオブジェクト
void EnemyDeathState::Enter()
{
    enemy->StopBattleActions();
    if (enemy->IsCinematicDeathAnimationOwnedExternally())
        return;

    const std::string& animationName = enemy->GetDeathAnimationName();
    const bool ignoreRootMotion = animationName == "Knock_Down_Death";
    owner->PlayBodyAnimation(animationName, false, true, 0.1f, ignoreRootMotion);
}

void EnemyDeathState::Execute(float deltaTime)
{

}

void EnemyDeathState::Exit()
{
}



void EnemyStunState::Enter()
{
    phase = Phase::Start;
    stunElapsedTime = 0.0f;
    enemy->StopChargeAttackMovement();
    enemy->StopAIMovement();
    enemy->DisableAttackHitBoxes();
    enemy->SetStunDebug("Start", stunElapsedTime);
    owner->PlayBodyAnimation("Knock_Down_Start", false, true, 0.1f, true);
}

void EnemyStunState::Execute(float deltaTime)
{
    if (enemy->IsDead())
        return;

    const auto controller = enemy->GetBodyAnimationController();
    if (!controller)
    {
        owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
        return;
    }

    switch (phase)
    {
    case Phase::Start:
        if (controller->IsPlayAnimation())
            return;
        phase = Phase::Loop;
        stunElapsedTime = 0.0f;
        enemy->SetStunDebug("Loop", stunElapsedTime);
        owner->PlayBodyAnimation("Knock_Down_Loop", true, true, 0.1f, true);
        break;

    case Phase::Loop:
        stunElapsedTime += deltaTime;
        enemy->SetStunDebug("Loop", stunElapsedTime);
        if (stunElapsedTime < enemy->GetStunDuration())
            return;
        phase = Phase::End;
        enemy->SetStunDebug("End", stunElapsedTime);
        owner->PlayBodyAnimation("Knock_Down_End", false, true, 0.1f, true);
        break;

    case Phase::End:
        if (controller->IsPlayAnimation())
            return;
        enemy->SetNextRecoveryDuration(
            enemy->GetPostStunRecoveryDuration(), "PostStun");
        owner->GetStateMachine()->ChangeState("EnemyRecoveryState");
        break;
    }
}

void EnemyStunState::Exit()
{
    enemy->StopAIMovement();
    enemy->DisableAttackHitBoxes();
    enemy->SetStunDebug("None", 0.0f);
}
