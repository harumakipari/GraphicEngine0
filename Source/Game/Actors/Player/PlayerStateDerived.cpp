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

    // 攻撃アニメーションを再生
    auto& attack = player->comboAttacks[player->currentComboIndex];

    player->PlayBodyAnimation(attack.animationName, false, true, 0.1f);

    //player->PlayBodyAnimation("Primary_Attack_Fast_D", false, true, 0.1f);

    // 攻撃タイマーをリセット
    attackTimer = 0.0f;
    hitDone = false;
}

void PlayerAttackState::Execute(float deltaTime)
{
    attackTimer += deltaTime;

    // 0.3秒後に当たる
    if (!hitDone && attackTimer > 0.3f)
    {
        player->DoAttackHit();
        hitDone = true;
    }

    auto& attack = player->comboAttacks[player->currentComboIndex];

    if (InputSystem::GetInputState("Attack", InputStateMask::Trigger))
    {
        if (attackTimer >= attack.comboWindowStart &&
            attackTimer <= attack.comboWindowEnd)
        {
            player->comboQueued = true;
        }
    }

    // 今のアニメーションの時間を取得する
    float animationLength = owner->GetBodyAnimationController()->GetCurrentAnimationLength();
    // 今のアニメーションの再生時間を取得する
    float animTime = owner->GetBodyAnimationController()->GetCurrentAnimationTime();
    // アニメーションの終了間際でコンボ入力があれば次の攻撃に移る
    if (animTime >= attack.comboWindowEnd)
    {
        if (player->comboQueued &&
            attack.nextComboIndex != -1)
        {
            player->comboQueued = false;

            player->currentComboIndex =
                attack.nextComboIndex;

            player->GetStateMachine()
                ->ChangeState("Attack");

            return;
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
}

void PlayerDodgeState::Enter()
{
    owner->PlayBodyAnimation("HitReact_Front");
    dodgeTimer = 0.0f;
    player->invincible = true; // ←無敵ON

}

void PlayerDodgeState::Execute(float deltaTime)
{
    dodgeTimer += deltaTime;

    // 無敵時間（ここがキモ）
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

}
