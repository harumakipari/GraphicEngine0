#include "pch.h"
#include "PlayerStateDerived.h"
#include "Game/Actors/Base/Character.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/Actors/Player/Player.h"
#include "Components/Audio/AudioSourceComponent.h"

PlayerStateBase::PlayerStateBase(Player* actor) :State(actor), player(actor)
{
}

void PlayerIdleState::Enter()
{
    owner->PlayBodyAnimation("Idle");
}

void PlayerIdleState::Execute(float deltaTime)
{
    if (player->TryHandleGlobalTransition())
    {
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

    // 走りSE再生
    if (player->runAudioComp)
        player->runAudioComp->Play();
}

void PlayerRunningState::Execute(float deltaTime)
{
    if (player->TryHandleGlobalTransition())
    {
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
    // 走りSEをストップ
    if (player->runAudioComp)
        player->runAudioComp->Stop();
}

void PlayerAttackState::Enter()
{
    // 火花エフェクトの生成フラグと当たった相手のセットをリセット
    player->hitTargets.clear();
    player->hasSpawnedThisAttack = false;
    player->hasPrevSwordTip = false;

    // 攻撃中は移動速度を0にする
    player->characterMovementComponent->SetSpeed(0.0f);

    player->PlayBodyAnimation(player->currentAttackAnimation, false, true, 0.1f);
    //player->PlayBodyAnimation("Primary_Attack_Fast_D", false, true, 0.1f);

    // 攻撃を開始する処理
    player->StartAttack();

    player->ResetAnimationStateFlag();  // アニメーションのステート系のフラグをリセットする

    Logger::Log("Attack Enter :" + player->currentAttackAnimation);

    Logger::Log("AnimationTime=" + std::to_string(player->GetBodyAnimationController()->GetCurrentAnimationTime()));
    Logger::Log("AnimationTime=" + std::to_string(player->GetBodyAnimationController()->GetCurrentAnimationTime()));

    dodgeQueued = false;
}

void PlayerAttackState::Execute(float deltaTime)
{
    if (player->inputWindow)
    {
        switch (player->bufferCommand.command)
        {
        case Player::InputCommand::Attack:
            player->comboQueued = true;
            player->ConsumeBufferCommand();
            break;
        case Player::InputCommand::Dodge:
            dodgeQueued = true;
            player->ConsumeBufferCommand();
            break;
        }
    }

    if (player->transitionWindow)
    {
        if (dodgeQueued)
        {
            player->GetStateMachine()
                ->ChangeState("Dodge");
            return;
        }

        if (player->comboQueued)
        {
            auto controller =
                player->GetBodyAnimationController();

            const auto* asset =
                controller->GetAnimationAsset(
                    player->currentAttackAnimation);

            if (asset)
            {
                if (!asset->nextCombo.empty())
                {
                    player->currentAttackAnimation =
                        asset->nextCombo;

                    player->comboQueued = false;

                    player->GetStateMachine()
                        ->ChangeState("Attack");
                }
            }
        }

    }

    if (!owner->GetBodyAnimationController()->IsPlayAnimation())
    {
        player->currentAttackAnimation = player->startAttackAnimation;
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


}

void PlayerAttackState::Exit()
{
    player->characterMovementComponent->ResetSpeed(); // 攻撃が終わったら移動速度をリセットする
    player->ResetAnimationStateFlag();  // アニメーションのステート系のフラグをリセットする
}

void PlayerDodgeState::Enter()
{
    player->ResetAnimationStateFlag();

    // 攻撃中は移動速度を0にする
    player->characterMovementComponent->SetSpeed(0.0f);

    // 入力方向を見る
    DirectX::XMFLOAT3 inputDir = player->dodgeDirection;
    DirectX::XMFLOAT3 forward = player->GetForward();
    DirectX::XMFLOAT3 right = player->GetRight();

    // 入力無しなら後ろ回避
    if (MathHelper::Length(inputDir) < 0.1f)
    {
        inputDir = {
            -player->GetForward().x,
            0.0f,
            -player->GetForward().z
        };
    }


#if 0
    inputDir = MathHelper::Normalize(inputDir);

    float forwardDot =
        inputDir.x * forward.x +
        inputDir.z * forward.z;

    float rightDot =
        inputDir.x * right.x +
        inputDir.z * right.z;

    DirectX::XMFLOAT3 dodgeMoveDir;
    std::string animName;

    if (std::abs(forwardDot) > std::abs(rightDot))
    {
        // 前後
        if (forwardDot > 0.0f)
        {
            animName = "Ability_RWB_Fwd_0";
            dodgeMoveDir = forward;
        }
        else
        {
            animName = "Ability_RMB_Bwd_0";
            dodgeMoveDir = {
                -forward.x,
                0.0f,
                -forward.z
            };
        }
    }
    else
    {
        // 左右
        if (rightDot > 0.0f)
        {
            animName = "Ability_RMB_Right_0";
            dodgeMoveDir = right;
        }
        else
        {
            animName = "Ability_RMB_Left_0";
            dodgeMoveDir = {
                -right.x,
                0.0f,
                -right.z
            };
        }
    }

    owner->PlayBodyAnimation(animName, false);

    player->characterMovementComponent->AddForcedMove(
        dodgeMoveDir,
        player->dodgeSpeed,
        player->dodgeDuration);

#else
    owner->PlayBodyAnimation("Ability_RMB_Bwd_0", false);

    // 一定時間だけ強制移動する速度を設定する
    player->characterMovementComponent->AddForcedMove({ -forward.x,0.0f,-forward.z }, player->dodgeSpeed, player->dodgeDuration);

#endif // 0

    rushRequested = false;
    judgeSuccess = false;
}

void PlayerDodgeState::Execute(float deltaTime)
{
    if (player->justDodgeSuccess)
    {
        judgeSuccess = true;
    }
    if (judgeSuccess)
    {
        player->rushInputTimer -= deltaTime;

        if (player->bufferCommand.command == Player::InputCommand::Attack)
        {
            rushRequested = true;
            player->ConsumeBufferCommand();
        }
        //if (player->rushInputTimer <= 0.0f)
        if (player->transitionWindow)
        {
            if (rushRequested)
            {
                Logger::Log(U8("ラッシュへ"));
                player->GetStateMachine()->ChangeState("Rush");
            }
            else
            {
                if (auto target = player->rushTarget.lock())
                {// タイムスケールをリセットする
                    target->ResetTimeScale();
                }
                player->GetStateMachine()->ChangeState("Idle");
            }
        }

    }
    else if (player->transitionWindow)
    {
        if (auto target = player->rushTarget.lock())
        {// タイムスケールをリセットする
            target->ResetTimeScale();
        }
        player->GetStateMachine()->ChangeState("Idle");
    }
#if 0
    if (!player->GetBodyAnimationController()->IsPlayAnimation())
    {
        player->GetStateMachine()->ChangeState("Idle");
    }
#endif // 0
}

void PlayerDodgeState::Exit()
{
    player->ResetAnimationStateFlag();
    player->ResetTimeScale();
    player->characterMovementComponent->ResetSpeed(); // 攻撃が終わったら移動速度をリセットする
}

// ラッシュ
void PlayerRushState::Enter()
{
    player->characterMovementComponent->SetSpeed(0.0f);

    phase = RushPhase::DashToTarget;

    if (auto target = player->rushTarget.lock())
    {// 移動する
        player->characterMovementComponent->MoveToActor(target,
            0.05f, 1.7f);
    }

    rushComboAdvanced = false;

    elapsedTime = 0.0f;
    queuedAttackCount = 1;
    player->invincible = true;  // ラッシュ攻撃中は無敵状態にする

}

void PlayerRushState::Execute(float deltaTime)
{
    if (InputSystem::GetInputState("Attack", InputStateMask::Trigger))
    {
        queuedAttackCount++;
        queuedAttackCount = std::min<int>(queuedAttackCount, 8);
    }

    switch (phase)
    {
    case RushPhase::DashToTarget:
        if (player->characterMovementComponent->IsMoveToActorFinished())
        {// targetまで付いたら、
            Logger::Log(U8("Rush中のtargetまで移動完了"));
            player->currentAttackAnimation = "Primary_Attack_Fast_A";
            player->PlayBodyAnimation(player->currentAttackAnimation, false);
            phase = RushPhase::Attack;
        }
        break;
    case RushPhase::Attack:
        if (!player->transitionWindow)
        {
            rushComboAdvanced = false;
        }
        if (player->transitionWindow && !rushComboAdvanced)
        {
            rushComboAdvanced = true;
            auto controller = player->GetBodyAnimationController();

            const auto* asset = controller->GetAnimationAsset(player->currentAttackAnimation);

            if (asset && !asset->nextCombo.empty() && queuedAttackCount > 0)
            {
                queuedAttackCount--;

                player->currentAttackAnimation = asset->nextCombo;
                //player->ResetAnimationStateFlag();
                player->PlayBodyAnimation(player->currentAttackAnimation, false);
            }

            if (asset == nullptr || asset->nextCombo.empty())
            {
                phase = RushPhase::Finished;
            }
        }
        if (!player->GetBodyAnimationController()->IsPlayAnimation())
        {
            phase = RushPhase::Finished;
        }
        break;
    case RushPhase::Finished:
        if (!player->GetBodyAnimationController()->IsPlayAnimation())
        {
            player->GetStateMachine()->ChangeState("Idle");
        }
        break;
    }
}

void PlayerRushState::Exit()
{
    player->characterMovementComponent->ResetSpeed(); // 攻撃が終わったら移動速度をリセットする
    player->GetBodyAnimationController()->ResetAnimationRate();
    if (auto target = player->rushTarget.lock())
    {
        target->ResetTimeScale();
    }
    player->ResetTimeScale();
    player->invincible = false;  // ラッシュ攻撃中は無敵状態解除
}


void PlayerJumpState::Enter()
{
    //ジャンプの初速度
    player->characterMovementComponent->Jump(jumpPower);
    //上昇時間　　v = v0 + gt を変形して ジャンプの上昇時 v は 0 になるから t = v0 / g ;
    float t_up = jumpPower / std::fabs(gravity);
    //Jump_Startのアニメーション時間
    float t_anim = player->GetBodyAnimationController()->GetAnimationLength("Jump_Start_0");
    //アニメーションの再生速度をt_upに合わせる
    float animationRate = t_anim / t_up;
    player->GetBodyAnimationController()->SetAnimationRate(animationRate);
    // アニメーションを再生
    player->PlayBodyAnimation("Jump_Start_0", false);
    // ステート
    jumpState = JumpState::JumpStart;
}

void PlayerJumpState::Execute(float deltaTime)
{
    switch (jumpState)
    {
    case JumpState::JumpStart:
        //上昇が完了した時
        if (!player->GetBodyAnimationController()->IsPlayAnimation())
        {//アニメーションの再生が終わったら 
            jumpState = JumpState::JumpMid;
            // アニメーションを再生
            player->PlayBodyAnimation("Jump_Land_0", false);
            // アニメーションの再生倍率をリセットする
            player->GetBodyAnimationController()->ResetAnimationRate();
        }
        break;
    case JumpState::JumpMid:
    {
        if (!player->GetBodyAnimationController()->IsPlayAnimation() && player->characterMovementComponent->GetVelocity().y < 0.0f)
        {//上昇が完了した時
            jumpState = JumpState::JumpLand;
            // アニメーションを再生
            player->PlayBodyAnimation("Jump_Recovery_0", false);

            isAnimatedRecovering = false;

            float t_middle_anim = player->GetBodyAnimationController()->GetAnimationLength("Jump_Land_0");
            //自由落下の公式より
            //v0 = √ (2gh)  から　 h = v0 * v0 / 2g 
            float h = player->GetPosition().y - 0.0f/*地面の高さ*/ + (3.0f)/*調整値*/;
            // t = √ (2h / g) から　上の式代入して　
            float t_fall = std::sqrtf(2 * h / std::abs(gravity)) - t_middle_anim/*"Jump_Middle"*/;
            //"Jump_Finish"のアニメーションの再生時間取得
            float t_anim = player->GetBodyAnimationController()->GetAnimationLength("Jump_Recovery_0");
            //アニメーションの再生速度を"t_fall"に合わせる
            float animationRate = t_anim / t_fall;
            player->GetBodyAnimationController()->SetAnimationRate(animationRate);
        }
    }
    break;
    case JumpState::JumpLand:
        if (!player->GetBodyAnimationController()->IsPlayAnimation() && player->characterMovementComponent->IsGround())
        {//アニメーションの再生が終わったら
            player->GetStateMachine()->ChangeState("Idle");
        }
        break;
    }
    if (InputSystem::GetInputState("Attack", InputStateMask::Trigger))
    {
        player->GetStateMachine()->ChangeState("JumpAttack");
    }
}

void PlayerJumpState::Exit()
{
    // アニメーションの再生倍率をリセットする
    player->GetBodyAnimationController()->ResetAnimationRate();
}

void PlayerJumpAttackState::Enter()
{
    player->GetBodyAnimationController()->ResetAnimationRate();
    // アニメーションを再生
    player->PlayBodyAnimation("Jump_Pad_0", false, true, 0.3f);
    // ステート
    jumpState = JumpState::JumpAttack;
}

void PlayerJumpAttackState::Execute(float deltaTime)
{
    switch (jumpState)
    {
    case JumpState::JumpAttack:
        if (!player->GetBodyAnimationController()->IsPlayAnimation())
        {//アニメーションの再生が終わったら 
            jumpState = JumpState::JumpLand;
        }
        break;
    case JumpState::JumpLand:
        player->GetStateMachine()->ChangeState("Idle");
        break;
    }
}

void PlayerJumpAttackState::Exit()
{

}

