#include "pch.h"
#include "PlayerStateDerived.h"
#include "Game/Actors/Base/Character.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Actors/Player/Player.h"
#include "Components/Audio/AudioSourceComponent.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Camera/DarkGameCamera.h"

PlayerStateBase::PlayerStateBase(Player* actor) :State(actor), player(actor)
{
}

void PlayerIdleState::Enter()
{
    Logger::Log("Idle Enter");
    owner->PlayBodyAnimation("Idle", true, true, 0.2f);
}

void PlayerIdleState::Execute(float deltaTime)
{
    if (player->TryExecuteActionRequest())
    {
        return;
    }
    // 入力があれば走るステートに変更
    if (player->characterMovementComponent->GetInputMagnitude() <= 0.0f)
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
    player->RestartLocomotionAnimation();

    auto controller = player->GetBodyAnimationController();
    Logger::Log(Logger::LogCategory::Gameplay,
        "[LocomotionRecovery][RunningEnter] currentState=" +
        std::string(player->GetStateMachine()->GetStateName()) +
        " locomotionMode=" + std::to_string(static_cast<int>(player->GetLocomotionMode())) +
        " useBlendSpace=" + (controller->IsUsingBlendSpace() ? "true" : "false") +
        " currentAnimation=" + controller->GetCurrentAnimationName() +
        " animationTime=" + std::to_string(controller->GetCurrentAnimationTime()) +
        " inputMagnitude=" + std::to_string(player->characterMovementComponent->GetInputMagnitude()) +
        " actualHorizontalSpeed=" + std::to_string(player->characterMovementComponent->GetActualHorizontalSpeed()));

    // 走りSE再生
    //if (player->runAudioComp)
    //    player->runAudioComp->Play();
}

void PlayerRunningState::Execute(float deltaTime)
{
    if (player->TryExecuteActionRequest())
    {
        return;
    }

    // 入力がなければ待機ステートに変更
    if (player->characterMovementComponent->GetInputMagnitude() <= 0.0f)
    {
        player->GetStateMachine()->ChangeState("Idle");
    }
}

void PlayerRunningState::Exit()
{
    // 走りSEをストップ
    //if (player->runAudioComp)
    //    player->runAudioComp->Stop();
    // ブレンドスペースを使うのをやめる
    owner->GetBodyAnimationController()->SetUseBlendSpace(false);

}


void PlayerDashState::Enter()
{
#if 0
    owner->PlayBodyAnimation("Sprint_Fwd", true, true, 0.2f, false);
    player->characterMovementComponent->SetFixedSpeed(0.0f);
#else 
    owner->PlayBodyAnimation("Sprint_Fwd", true, true, 0.2f, true);
    // スピードを変更する
    player->characterMovementComponent->SetFixedSpeed(player->dashSpeed);
#endif

}

void PlayerDashState::Execute(float deltaTime)
{
    if (player->TryExecuteActionRequest())
    {
        return;
    }

    auto inputComp = player->inputComponent;
    DirectX::XMFLOAT3 dir = inputComp->GetMoveInput();

    if (!InputSystem::GetInputState("GamePadB", InputStateMask::Press))
    {
        if (MathHelper::Length(dir) > 0.01f)
        {
            player->GetStateMachine()->ChangeState("Running");
        }
        else
        {
            player->GetStateMachine()->ChangeState("Idle");
        }
        return;
    }
}

void PlayerDashState::Exit()
{
    // ブレンドスペースを使うのをやめる
    owner->GetBodyAnimationController()->SetUseBlendSpace(false);
    // 速度を元に戻す
    player->characterMovementComponent->ResetFixedSpeed();
}

void PlayerAttackState::Enter()
{
    const bool isComboContinuation = continuingComboTransition;
    if (!isComboContinuation)
    {
        player->currentAttackAnimation = player->startAttackAnimation;
        player->comboQueued = false;
    }

    player->AcquireAttackTarget();

    // 火花エフェクトの生成フラグと当たった相手のセットをリセット
    player->hitTargets.clear();
    player->hasSpawnedThisAttack = false;
    player->hasPrevSwordTip = false;

    // 攻撃中は移動速度を0にする
    player->characterMovementComponent->SetFixedSpeed(0.0f);

    player->PlayBodyAnimation(player->currentAttackAnimation, false, true, 0.1f);

    // 攻撃を開始する処理
    player->StartAttack();

    player->ResetAnimationStateFlag();  // アニメーションのステート系のフラグをリセットする

    Logger::Log("Attack Enter :" + player->currentAttackAnimation);

    Logger::Log("AnimationTime=" + std::to_string(player->GetBodyAnimationController()->GetCurrentAnimationTime()));
    Logger::Log("AnimationTime=" + std::to_string(player->GetBodyAnimationController()->GetCurrentAnimationTime()));

    dodgeQueued = false;
    player->comboQueued = false;
    continuingComboTransition = false;

    Logger::Log(Logger::LogCategory::Gameplay,
        "[NormalAttack][Enter] continuation=" +
        std::string(isComboContinuation ? "true" : "false") +
        " animation=" + player->currentAttackAnimation);
}

void PlayerAttackState::Execute(float deltaTime)
{
    player->UpdateAttackTargetRotation(deltaTime);

    if (player->inputWindow)
    {
        switch (player->bufferCommand.type)
        {
        case Player::ActionType::Attack:
            player->comboQueued = true;
            player->ConsumeActionRequest(Player::ActionType::Attack);
            break;
        case Player::ActionType::Dodge:
            dodgeQueued = true;
            player->ConsumeActionRequest(Player::ActionType::Dodge);
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

                    continuingComboTransition = true;
                    player->GetStateMachine()
                        ->ChangeState("Attack");
                    return;
                }
            }
        }

    }

    if (!owner->GetBodyAnimationController()->IsPlayAnimation())
    {
        auto controller = player->GetBodyAnimationController();
        const float inputMagnitude = MathHelper::Length(player->inputComponent->GetMoveInput());
        const char* targetState = inputMagnitude > 0.01f ? "Running" : "Idle";
        Logger::Log(Logger::LogCategory::Gameplay,
            "[LocomotionRecovery][AttackFinished] attackEndDetected=true previousState=Attack targetState=" +
            std::string(targetState) +
            " locomotionMode=" + std::to_string(static_cast<int>(player->GetLocomotionMode())) +
            " useBlendSpace=" + (controller->IsUsingBlendSpace() ? "true" : "false") +
            " currentAnimation=" + controller->GetCurrentAnimationName() +
            " animationTime=" + std::to_string(controller->GetCurrentAnimationTime()) +
            " inputMagnitude=" + std::to_string(inputMagnitude) +
            " actualHorizontalSpeed=" + std::to_string(player->characterMovementComponent->GetActualHorizontalSpeed()));

        player->currentAttackAnimation = player->startAttackAnimation;
        player->comboQueued = false;

        player->GetStateMachine()->ChangeState(targetState);
    }
}

void PlayerAttackState::Exit()
{
    if (!continuingComboTransition)
    {
        player->currentAttackAnimation = player->startAttackAnimation;
        player->comboQueued = false;
        dodgeQueued = false;
        Logger::Log(Logger::LogCategory::Gameplay,
            "[NormalAttack][Exit] combo state discarded");
    }

    // 攻撃を終了する処理
    player->EndAttack();
    player->characterMovementComponent->ResetFixedSpeed(); // 攻撃が終わったら移動速度をリセットする
    player->ResetAnimationStateFlag();  // アニメーションのステート系のフラグをリセットする
    if (!continuingComboTransition)
        player->ClearAttackTarget();
}

void PlayerDodgeState::Enter()
{
    player->BeginDodgeDebug();
    player->ResetAnimationStateFlag();
    player->SetRushInputAcceptance(false);
    player->SetRushInputDebugState(false, false);

    // 攻撃中は移動速度を0にする
    player->characterMovementComponent->SetFixedSpeed(0.0f);

    if (player->UsesDodgeWorldDirection())
    {
        player->rotationComponent->SetDirection(player->GetDodgeWorldDirection());
    }

    switch (player->GetDodgeDirection())
    {
    case Player::DodgeDirection::Forward:
        owner->PlayBodyAnimation("Ability_RWB_Fwd_0", false);
        break;
    case Player::DodgeDirection::Backward:
        owner->PlayBodyAnimation("Ability_RMB_Bwd_0", false);
        break;
    case Player::DodgeDirection::Left:
        owner->PlayBodyAnimation("Ability_RMB_Left_0", false);
        break;
    case Player::DodgeDirection::Right:
        owner->PlayBodyAnimation("Ability_RMB_Right_0", false);
        break;
    }

    rushRequested = false;
    judgeSuccess = false;
}

void PlayerDodgeState::Execute(float deltaTime)
{
    player->UpdateDodgeDebug(deltaTime);

    if (player->justDodgeSuccess && !judgeSuccess)
    {// ジャスト回避成功したら、
        judgeSuccess = true;
        player->SetRushInputAcceptance(!player->rushTarget.expired());
    }
    if (player->rushTarget.expired())
    {
        player->SetRushInputAcceptance(false);
    }
    player->SetRushInputDebugState(judgeSuccess, rushRequested);
    if (judgeSuccess)
    {
        if (player->CanAcceptInitialRushInput() &&
            player->bufferCommand.type == Player::ActionType::Attack)
        {
            rushRequested = true;
            player->SetRushInputDebugState(judgeSuccess, rushRequested);
            player->SetRushInputAcceptance(false);
            player->BeginPlayerSlowReturn();
            player->HoldBossSlowForRush();
            player->ConsumeActionRequest(Player::ActionType::Attack);
        }
        if (player->transitionWindow)
        {
            if (rushRequested)
            {
                Logger::Log(U8("ラッシュへ"));
                player->GetStateMachine()->ChangeState("Rush");
            }
            else
            {
                player->SetRushInputAcceptance(false);
                DirectX::XMFLOAT3 move = player->inputComponent->GetMoveInput();
                if (MathHelper::Length(move) > 0.1f)
                {
                    player->GetStateMachine()->ChangeState("Running");
                }
                else
                {
                    player->GetStateMachine()->ChangeState("Idle");
                }
            }
        }

    }
    else if (player->transitionWindow)
    {
        player->SetRushInputAcceptance(false);

        DirectX::XMFLOAT3 move = player->inputComponent->GetMoveInput();

        if (MathHelper::Length(move) > 0.1f)
        {
            player->GetStateMachine()->ChangeState("Running");
        }
        else
        {
            player->GetStateMachine()->ChangeState("Idle");
        }
    }
#if 1
    if (!player->GetBodyAnimationController()->IsPlayAnimation())
    {// 保険
        player->SetRushInputAcceptance(false);
        player->GetStateMachine()->ChangeState("Idle");
    }
#endif // 0
}

void PlayerDodgeState::Exit()
{
    const bool enteringRush = rushRequested;
    player->SetRushInputAcceptance(false);
    player->SetRushInputDebugState(false, false);
    rushRequested = false;
    judgeSuccess = false;
    player->ResetAnimationStateFlag();
    player->BeginPlayerSlowReturn();
    if (!enteringRush)
    {
        player->BeginBossSlowReturn();
    }
    player->characterMovementComponent->ResetFixedSpeed(); // 攻撃が終わったら移動速度をリセットする
}

void PlayerDamageState::Enter()
{
    player->ResetAnimationStateFlag();
    player->characterMovementComponent->SetFixedSpeed(0.0f);
    player->PlayBodyAnimation("Hit_Combat_F", false, true, 0.1f, true);

    //const DirectX::XMFLOAT3& direction = player->GetDamageKnockbackDirection();
    //const float power = player->GetDamageKnockbackPower();
    //player->characterMovementComponent->AddImpulse(
    //    { direction.x * power, 0.0f, direction.z * power });

    //Logger::Log(Logger::LogCategory::Gameplay,
    //    "[PlayerDamage][Enter] animation=Hit_Combat_F knockback=" +
    //    std::to_string(direction.x) + ",0," + std::to_string(direction.z));
}

void PlayerDamageState::Execute(float deltaTime)
{
    if (player->GetBodyAnimationController()->IsPlayAnimation())
        return;

    const DirectX::XMFLOAT3 move = player->inputComponent->GetMoveInput();
    const char* targetState = MathHelper::Length(move) > 0.01f ? "Running" : "Idle";
    Logger::Log(Logger::LogCategory::Gameplay,
        "[PlayerDamage][Finished] targetState=" + std::string(targetState));
    player->GetStateMachine()->ChangeState(targetState);
}

void PlayerDamageState::Exit()
{
    player->characterMovementComponent->ResetFixedSpeed();
    player->ResetAnimationStateFlag();
}

void PlayerKnockBackState::Enter()
{
    phase = Phase::KnockBack;
    player->BeginKnockBackMovement();
    player->PlayBodyAnimation("Hit_Large_KnockBack", false, true, 0.1f, true);
}

void PlayerKnockBackState::Execute(float deltaTime)
{
    player->UpdateKnockBackMovement(deltaTime);

    if (player->GetBodyAnimationController()->IsPlayAnimation())
        return;

    if (phase == Phase::KnockBack)
    {
        phase = Phase::GetUp;
        player->PlayBodyAnimation("Get_Up", false, true, 0.05f, true);
        return;
    }

    const DirectX::XMFLOAT3 move = player->inputComponent->GetMoveInput();
    const char* targetState = MathHelper::Length(move) > 0.01f ? "Running" : "Idle";
    player->GetStateMachine()->ChangeState(targetState);
}

void PlayerKnockBackState::Exit()
{
    player->EndKnockBackMovement();
}
void PlayerDeathPendingState::Enter()
{
    // アクターやアニメーションの更新を維持しつつ、死のカメラ演出中に継続する可能性のあるアクションを削除する。
    player->ClearTransientBattleActions();
    player->SetDeathCameraTransparencyDisabled(true);
    player->ResetAnimationStateFlag();
    player->characterMovementComponent->SetFixedSpeed(0.0f);
    player->characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    player->characterMovementComponent->SetInputMagnitude(0.0f);
    Logger::Log(Logger::LogCategory::Gameplay, "[PlayerDeathPending][Enter] hp=0");

    // ボスとプレイヤーの構図のために回転させる
    if (auto gruxEnemy = player->GetOwnerScene()->GetActorManager()->GetActorOfType<GruxEnemy>())
    {
        DirectX::XMFLOAT3 deathDirection = MathHelper::Subtract(gruxEnemy->GetPosition(), player->GetPosition());deathDirection.y = 0.0f;
        if (MathHelper::Length(deathDirection) > FLT_EPSILON)
        {
            deathDirection = MathHelper::Normalize(deathDirection);
            player->rotationComponent->SetDirectionImmediate(deathDirection);
        }
    }

}

void PlayerDeathPendingState::Execute(float deltaTime)
{
}

void PlayerDeathPendingState::Exit()
{
}

void PlayerDeathState::Enter()
{
    elapsedTime = 0.0f;
    player->ClearActionRequest("death_enter");
    player->ResetAnimationStateFlag();
    player->characterMovementComponent->SetFixedSpeed(0.0f);
    player->characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    player->characterMovementComponent->SetInputMagnitude(0.0f);
    player->PlayBodyAnimation("Hit_Combat_Death", false, true, 0.1f, true);
    player->BeginDeathEyeClose();
    Logger::Log(Logger::LogCategory::Gameplay, "[PlayerDeath][Enter] hp=0");
}

void PlayerDeathState::Execute(float deltaTime)
{
    (void)deltaTime;
    elapsedTime += Time::UnscaledDeltaTime();
    player->UpdateDeathEyeClose(elapsedTime);
    player->UpdateDeathVisualFade(elapsedTime);
}

void PlayerDeathState::Exit()
{
    player->EndDeathEyeClose();
    elapsedTime = 0.0f;
}

void PlayerWinState::Enter()
{
    player->ClearTransientBattleActions();
    player->ForceResetPlayerSlow();
    player->ForceResetBossSlow();
    player->PlayBodyAnimation("Idle", true, true, 0.2f);
}

void PlayerWinState::Execute(float deltaTime)
{
    (void)deltaTime;
}

void PlayerWinState::Exit()
{
}

// ラッシュ
void PlayerRushState::Enter()
{
    rushHpDisplayTarget.reset();
    player->SetRushWeaponVisual(true);
    player->ForceResetPlayerSlow();
    player->HoldBossSlowForRush();
    // ラッシュコンボのアニメーション名
    const std::array<const char*, 7> availableRushCombo =
    {
        "Rush_Attack_Fast_A",
        "Rush_Attack_Fast_B",
        "Rush_Attack_Fast_C",
        "Rush_Attack_Fast_D",
        "Rush_Attack_Fast_A",
        "Rush_Attack_Fast_B",
        "Rush_Attack_Fast_End",
    };
    const int rushAttackCount = std::clamp(
        player->GetMaxRushAttackCount(), 1,
        static_cast<int>(availableRushCombo.size()));
    rushCombo.assign(
        availableRushCombo.begin(),
        availableRushCombo.begin() + rushAttackCount);
    // A tuned shorter Rush still finishes with the existing finisher animation.
    rushCombo.back() = "Rush_Attack_Fast_End";

    comboIndex = 0;

    player->characterMovementComponent->SetFixedSpeed(0.0f);

    phase = RushPhase::DashToTarget;

    if (auto target = player->rushTarget.lock())
    {// 移動する
        if (auto gruxTarget = std::dynamic_pointer_cast<GruxEnemy>(target))
        {
            rushHpDisplayTarget = gruxTarget;
            gruxTarget->BeginRushHpDisplay();
        }

        player->characterMovementComponent->MoveToActor(target, player->moveToEnemyInterval, 2.5f);
        // ルートモーションを無視する
        player->PlayBodyAnimation("0_Jog_Fwd", false, true, 0.2f, true);
    }

    rushComboAdvanced = false;

    elapsedTime = 0.0f;
    // The Attack request that started Rush was already consumed in Dodge.
    queuedAttackCount = 0;
    ResetRushInputGrace();
    player->invincible = true;  // ラッシュ攻撃中は無敵状態にする

    currentAttackAnimation = "Rush_Attack_Fast_A";
}

void PlayerRushState::Execute(float deltaTime)
{
    (void)deltaTime;
    if (player->rushTarget.expired())
    {
        player->ForceResetBossSlow();
        player->GetStateMachine()->ChangeState("Idle");
        return;
    }
    if (auto target = player->rushTarget.lock())
    {
        const auto targetStateMachine = target->GetStateMachine();
        if (!target->IsAlive() || target->IsPendingKill() ||
            (targetStateMachine && std::string(targetStateMachine->GetStateName()) == "EnemyDeathState"))
        {
            player->BeginBossSlowReturn(true);
            player->GetStateMachine()->ChangeState("Idle");
            return;
        }
    }
    if (auto target = player->rushTarget.lock())
    {
        DirectX::XMFLOAT3 targetDirection = MathHelper::Subtract(
            target->GetPosition(), player->GetPosition());
        targetDirection.y = 0.0f;
        player->rotationComponent->SetDirection(targetDirection);
    }




    switch (phase)
    {
    case RushPhase::DashToTarget:
        if (player->characterMovementComponent->IsMoveToActorFinished())
        {// targetまで付いたら、
            Logger::Log(U8("Rush中のtargetまで移動完了"));
            player->PlayBodyAnimation(currentAttackAnimation, false);
            phase = RushPhase::Attack;
        }
        break;
    case RushPhase::Attack:
    {
        const bool animationPlaying =
            player->GetBodyAnimationController()->IsPlayAnimation();
        if (player->transitionWindow)
        {
            rushTransitionWindowObserved = true;
        }

        const bool lateInputGraceActive =
            rushTransitionWindowObserved && animationPlaying &&
            queuedAttackCount == 0 && !IsFinalRushAttack();
        const bool acceptsRushInput =
            !IsFinalRushAttack() &&
            (player->inputWindow || lateInputGraceActive);
        if (player->bufferCommand.type == Player::ActionType::Attack)
        {
            if (acceptsRushInput && queuedAttackCount == 0)
            {
                queuedAttackCount = 1;
                player->ConsumeActionRequest(Player::ActionType::Attack);
                Logger::Log(Logger::LogCategory::Gameplay,
                    "[Rush][AttackQueued] comboIndex=" + std::to_string(comboIndex) +
                    " lateInput=" + std::string(lateInputGraceActive ? "true" : "false"));
            }
            else if (IsFinalRushAttack())
            {
                // The final Rush cannot reserve an eighth attack.
                player->ConsumeActionRequest(Player::ActionType::Attack);
            }
        }

        if (player->transitionWindow && !rushComboAdvanced)
        {
            if (IsFinalRushAttack())
            {
                rushComboAdvanced = true;
                phase = RushPhase::Finished;
            }
            else if (queuedAttackCount > 0)
            {
                AdvanceRushCombo();
                break;
            }
        }

        if (!animationPlaying && phase == RushPhase::Attack)
        {
            if (queuedAttackCount > 0 && rushTransitionWindowObserved &&
                !IsFinalRushAttack())
            {
                AdvanceRushCombo();
            }
            else
            {
                phase = RushPhase::Finished;
            }
        }
        break;
    }
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
    if (auto gruxTarget = rushHpDisplayTarget.lock())
    {
        gruxTarget->EndRushHpDisplay();
    }
    rushHpDisplayTarget.reset();
    player->SetRushWeaponVisual(false);
    comboIndex = 0;
    queuedAttackCount = 0;
    rushComboAdvanced = false;
    ResetRushInputGrace();
    if (player->bufferCommand.type == Player::ActionType::Attack)
    {
        player->ConsumeActionRequest(Player::ActionType::Attack);
    }
    phase = RushPhase::DashToTarget;
    currentAttackAnimation = "Rush_Attack_Fast_A";
    elapsedTime = 0.0f;
    Logger::Log(Logger::LogCategory::Gameplay, "[Rush][Exit] combo state reset");

    player->characterMovementComponent->ResetFixedSpeed(); // 攻撃が終わったら移動速度をリセットする
    player->GetBodyAnimationController()->ResetAnimationRate();
    player->ForceResetPlayerSlow();
    player->BeginBossSlowReturn(true);
    player->invincible = false;  // ラッシュ攻撃中は無敵状態解除
}

bool PlayerRushState::AdvanceRushCombo()
{
    if (queuedAttackCount <= 0 || IsFinalRushAttack())
        return false;

    queuedAttackCount--;
    comboIndex++;
    if (comboIndex >= static_cast<int>(rushCombo.size()))
    {
        phase = RushPhase::Finished;
        return false;
    }

    currentAttackAnimation = rushCombo[comboIndex];
    rushComboAdvanced = true;
    ResetRushInputGrace();
    player->PlayBodyAnimation(currentAttackAnimation, false);
    // PlayBodyAnimation switches to a fresh animation and resets its Notify flags.
    rushComboAdvanced = false;
    return true;
}

bool PlayerRushState::IsFinalRushAttack() const
{
    return !rushCombo.empty() &&
        comboIndex + 1 >= static_cast<int>(rushCombo.size());
}

void PlayerRushState::ResetRushInputGrace()
{
    rushTransitionWindowObserved = false;
}

#if 0
void PlayerInteractState::Enter()
{
    switch (player->swordState)
    {
    case Player::SwordState::Equipped:

        break;
    case Player::SwordState::Sheathed:
        break;
    }

    owner->PlayBodyAnimation("anim_OpenDoor_L_0", false);
}

void PlayerInteractState::Execute(float deltaTime)
{
    if (!owner->GetBodyAnimationController()->IsPlayAnimation())
    {
        player->GetStateMachine()->ChangeState("Idle");
    }
}

void PlayerInteractState::Exit()
{

}
#endif // 0

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
