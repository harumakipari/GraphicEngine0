#include "pch.h"
#include "PlayerStateDerived.h"
#include "Game/Actors/Base/Character.h"
#include "Game/Actors/Player/Player.h"

PlayerStateBase::PlayerStateBase(Player* actor) :State(actor), player(actor)
{
}

void PlayerIdleState::Enter()
{
    owner->PlayBodyAnimation("Idle");
}

void PlayerIdleState::Execute(float deltaTime)
{
    // 攻撃入力チェック
    if (InputSystem::GetInputState("Attack", InputStateMask::Trigger))
    {
        player->GetStateMachine()->ChangeState("Attack");
        return;
    }

    if (InputSystem::GetInputState("Dodge", InputStateMask::Trigger))
    {
        player->GetStateMachine()->ChangeState("Dodge");
        return;
    }

    // 入力があれば走るステートに変更
    auto inputComp = player->inputComponent;
    DirectX::XMFLOAT3 dir = inputComp->GetMoveInput();

    if (std::abs(dir.x - 0.0f) <= FLT_EPSILON && std::abs(dir.y - 0.0f) <= FLT_EPSILON && std::abs(dir.z - 0.0f) <= FLT_EPSILON)
    {
        return;
    }
    player->GetStateMachine()->ChangeState("Running");
}

void PlayerIdleState::Exit()
{

}

void PlayerRunningState::Enter()
{
    owner->PlayBodyAnimation("Jog_Fwd", true, true, 0.2f);
}

void PlayerRunningState::Execute(float deltaTime)
{
    // 攻撃入力チェック
    if (InputSystem::GetInputState("Attack", InputStateMask::Trigger))
    {
        player->GetStateMachine()->ChangeState("Attack");
        return;
    }

    if (InputSystem::GetInputState("Dodge", InputStateMask::Trigger))
    {
        player->GetStateMachine()->ChangeState("Dodge");
        return;
    }


    // 入力がなければ待機ステートに変更
    auto inputComp = player->inputComponent;
    DirectX::XMFLOAT3 dir = inputComp->GetMoveInput();
    if (std::abs(dir.x - 0.0f) <= FLT_EPSILON && std::abs(dir.y - 0.0f) <= FLT_EPSILON && std::abs(dir.z - 0.0f) <= FLT_EPSILON)
    {
        player->GetStateMachine()->ChangeState("Idle");
    }
}

void PlayerRunningState::Exit()
{
}

void PlayerAttackState::Enter()
{
    // 火花エフェクトの生成フラグと当たった相手のセットをリセット
    player->hitTargets.clear();
    player->hasSpawnedThisAttack = false;
    player->hasPrevSwordTip = false;

    // 攻撃中は移動速度を0にする
    player->characterMovementComponent->SetSpeed(0.0f);

    auto& attack = player->comboAttacks[player->currentComboIndex];
    player->PlayBodyAnimation(attack.animationName, false, true, 0.1f);

    //player->PlayBodyAnimation("Primary_Attack_Fast_D", false, true, 0.1f);

    // 攻撃タイマーをリセット
    attackTimer = 0.0f;
    hitDone = false;

    player->ResetAnimationStateFlag();  // アニメーションのステート系のフラグをリセットする

    Logger::Log("Attack Enter :"+attack.animationName);

    Logger::Log("AnimationTime="+std::to_string( player->GetBodyAnimationController()->GetCurrentAnimationTime()));
    Logger::Log("AnimationTime="+std::to_string( player->GetBodyAnimationController()->GetCurrentAnimationTime()));


}

void PlayerAttackState::Execute(float deltaTime)
{
    attackTimer += deltaTime;

#if 0
    if (player->hitBoxEnabled)
    {
        player->DoAttackHit();
    }

#endif // 0

    if (InputSystem::GetInputState("Attack", InputStateMask::Trigger))
    {
        if (player->comboWindow)
        {
            player->comboQueued = true;
            Logger::Log(U8("comboQueued が true になりました"));
        }
    }

    if (player->transitionWindow)
    {
        if (player->comboQueued)
        {
            auto& attack =player->comboAttacks[player->currentComboIndex];

            if (attack.nextComboIndex != -1)
            {
                player->currentComboIndex =
                    attack.nextComboIndex;
                player->comboQueued = false;
                player->GetStateMachine()->ChangeState("Attack");
            }
        }
    }


    if (!owner->GetBodyAnimationController()->IsPlayAnimation())
    {
        player->currentComboIndex = 0;
        player->comboQueued = false;

        auto dir = player->inputComponent->GetMoveInput();
        if (MathHelper::Length(dir) > 0.01f)
        {
            player->GetStateMachine()->ChangeState("Running");
        }
        else
        {
            player->GetStateMachine()->ChangeState("Idle");
        }
    }

    if (InputSystem::GetInputState("Dodge", InputStateMask::Trigger))
    {
        player->GetStateMachine()->ChangeState("Dodge");
        return;
    }

}

void PlayerAttackState::Exit()
{
    player->characterMovementComponent->ResetSpeed(); // 攻撃が終わったら移動速度をリセットする
    player->ResetAnimationStateFlag();  // アニメーションのステート系のフラグをリセットする
}

void PlayerDodgeState::Enter()
{
    owner->PlayBodyAnimation("Roll_front_0");
    dodgeTimer = 0.0f;
    player->invincible = true; // ←無敵ON

    // 攻撃中は移動速度を0にする
    player->characterMovementComponent->SetSpeed(0.0f);


}

void PlayerDodgeState::Execute(float deltaTime)
{
    dodgeTimer += deltaTime;

    // 無敵時間
    if (dodgeTimer > 0.3f)
    {
        player->invincible = false;
    }

    // 終了
    if (dodgeTimer > 0.6f)
    {
        auto dir = player->inputComponent->GetMoveInput();
        if (MathHelper::Length(dir) > 0.01f)
        {
            player->GetStateMachine()->ChangeState("Running");
        }
        else
        {
            player->GetStateMachine()->ChangeState("Idle");
        }
    }
}

void PlayerDodgeState::Exit()
{
    player->characterMovementComponent->ResetSpeed(); // 攻撃が終わったら移動速度をリセットする

}
