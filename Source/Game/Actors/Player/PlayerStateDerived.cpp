#include "pch.h"
#include "PlayerStateDerived.h"
#include "Game/Actors/Base/Character.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Actors/Player/Player.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Utility/Time.h"

PlayerStateBase::PlayerStateBase(Player* actor) :State(actor), player(actor)
{
}

void PlayerIdleState::Enter()
{
    owner->PlayBodyAnimation("Idle", true, true, 0.3f);
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
    // ブレンドスペースを使うのをやめる
    owner->GetBodyAnimationController()->SetUseBlendSpace(false);
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


    // 攻撃中は移動速度を0にする
    player->characterMovementComponent->SetFixedSpeed(0.0f);

    player->PlayBodyAnimation(player->currentAttackAnimation, false, true, 0.1f);

    // 攻撃を開始する処理
    player->StartAttack();


    dodgeQueued = false;
    player->comboQueued = false;
    continuingComboTransition = false;
}

void PlayerAttackState::Execute(float deltaTime)
{
    player->UpdateAttackTargetRotation(deltaTime);

    if (player->IsFinalHitVisualPoseLatchRequested())
    {
        player->comboQueued = false;
        dodgeQueued = false;
        if (player->bufferCommand.type != Player::ActionType::None)
            player->ClearActionRequest("phase1_final_hit");
    }

    if (player->inputWindow && !player->IsFinalHitVisualPoseLatchRequested())
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

        player->currentAttackAnimation = player->startAttackAnimation;
        player->comboQueued = false;

        player->GetStateMachine()->ChangeState(targetState);
    }
}

void PlayerAttackState::Exit()
{
    if (!continuingComboTransition)
    {
        const auto controller = player->GetBodyAnimationController();
        const bool naturalAnimationEnd = controller && !controller->IsPlayAnimation();
        if (naturalAnimationEnd)
        {
            // Latch only the completed attack pose before Idle/locomotion replaces the clip.
            player->LatchFinalHitVisualPoseAfterAnimationUpdate();
        }
        else if (player->IsFinalHitAttackDrainActive())
        {
            player->MarkFinalHitAttackDrainInterrupted();
        }
        player->currentAttackAnimation = player->startAttackAnimation;
        player->comboQueued = false;
        dodgeQueued = false;
    }

    // 攻撃を終了する処理
    player->characterMovementComponent->ResetFixedSpeed(); // 攻撃が終わったら移動速度をリセットする
    if (!continuingComboTransition)
        player->ClearAttackTarget();
}

void PlayerDodgeState::Enter()
{
    player->SetRushInputAcceptance(false, "None");

    // 攻撃中は移動速度を0にする
    player->characterMovementComponent->SetFixedSpeed(0.0f);

    if (player->UsesDodgeWorldDirection())
    {
        player->rotationComponent->SetDirection(player->GetDodgeWorldDirection());
    }
    player->CaptureDodgeWorldDirectionSnapshot();

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
    if (player->justDodgeSuccess && !judgeSuccess)
    {// ジャスト回避成功したら、
        judgeSuccess = true;
        player->SetRushInputAcceptance(!player->rushTarget.expired());
    }
    if (player->rushTarget.expired())
    {
        player->SetRushInputAcceptance(false, "RushTargetExpired");
        rushRequested = false;
    }
    if (judgeSuccess)
    {
        if (player->CanAcceptInitialRushInput() &&
            player->bufferCommand.type == Player::ActionType::Attack)
        {
            rushRequested = true;
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
                player->SetRushInputAcceptance(false, "RushStarted");
                player->GetStateMachine()->ChangeState("Rush");
            }
            else
            {
                player->SetRushInputAcceptance(false, "TransitionWithoutRush");
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
        player->SetRushInputAcceptance(false, "TransitionWithoutRush");

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
        player->SetRushInputAcceptance(false, "DodgeFinished");
        player->GetStateMachine()->ChangeState("Idle");
    }
#endif // 0
}

void PlayerDodgeState::Exit()
{
    if (auto* scene = dynamic_cast<SceneBase*>(player->GetOwnerScene()))
        scene->StopJustDodgeRadialBlur();

    const bool enteringRush = rushRequested;
    player->SetRushInputAcceptance(false);
    rushRequested = false;
    judgeSuccess = false;
    player->BeginPlayerSlowReturn();
    if (!enteringRush)
    {
        player->BeginBossSlowReturn();
    }
    player->characterMovementComponent->ResetFixedSpeed(); // 攻撃が終わったら移動速度をリセットする
}

void PlayerDamageState::Enter()
{
    player->characterMovementComponent->SetFixedSpeed(0.0f);
    const char* animationName = "Hit_Combat_F";
    switch (player->GetHitReactionDirection())
    {
    case Player::HitReactionDirection::Back:
        animationName = "Hit_Combat_B";
        break;
    case Player::HitReactionDirection::Left:
        animationName = "Hit_Combat_L";
        break;
    case Player::HitReactionDirection::Right:
        animationName = "Hit_Combat_R";
        break;
    case Player::HitReactionDirection::Front:
    default:
        break;
    }
    player->PlayBodyAnimation(animationName, false, true, 0.1f, true);

}

void PlayerDamageState::Execute(float deltaTime)
{
    if (player->GetBodyAnimationController()->IsPlayAnimation())
        return;

    const DirectX::XMFLOAT3 move = player->inputComponent->GetMoveInput();
    const char* targetState = MathHelper::Length(move) > 0.01f ? "Running" : "Idle";
    player->GetStateMachine()->ChangeState(targetState);
}

void PlayerDamageState::Exit()
{
    player->characterMovementComponent->ResetFixedSpeed();
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
    player->SetRushInputAcceptance(false, "Death");
    player->SetDeathCameraTransparencyDisabled(true);
    player->characterMovementComponent->SetFixedSpeed(0.0f);
    player->characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    player->characterMovementComponent->SetInputMagnitude(0.0f);

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
    player->characterMovementComponent->SetFixedSpeed(0.0f);
    player->characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    player->characterMovementComponent->SetInputMagnitude(0.0f);
    player->PlayBodyAnimation("Hit_Combat_Death", false, true, 0.1f, true);
    player->BeginDeathEyeClose();
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
    if (auto* scene = dynamic_cast<SceneBase*>(player->GetOwnerScene()))
        scene->StopJustDodgeRadialBlur();

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
    windupElapsedTime = 0.0f;
    player->SetRushWindupActive(false);
    player->SetRushAttackActive(false);

    if (auto target = player->rushTarget.lock())
    {// 移動する
        const auto gruxTarget = std::dynamic_pointer_cast<GruxEnemy>(target);
        if (gruxTarget)
        {
            rushHpDisplayTarget = gruxTarget;
            gruxTarget->BeginRushHpDisplay();
            gruxTarget->NotifyPlayerRushStarted(player);
        }

        constexpr float gruxRushStopDistance = 2.5f;
        constexpr float minimumNormalEnemyRushStopDistance = 0.25f;
        float rushStopDistance = gruxRushStopDistance;
        float playerRadius = 0.0f;
        float targetRadius = 0.0f;
        bool usedCapsuleDistance = false;

        // MoveToActor compares Actor positions in XZ, while both character
        // capsules are only offset vertically. The horizontal center distance
        // can therefore use their radii directly for normal enemies.
        if (!gruxTarget)
        {
            const auto playerCapsule = std::dynamic_pointer_cast<CapsuleComponent>(
                player->FindComponentByName("capsuleComponent"));
            const auto targetCapsule = std::dynamic_pointer_cast<CapsuleComponent>(
                target->FindComponentByName("capsuleComponent"));
            if (playerCapsule && targetCapsule)
            {
                playerRadius = playerCapsule->GetRadius();
                targetRadius = targetCapsule->GetRadius();
                if (std::isfinite(playerRadius) && std::isfinite(targetRadius) &&
                    playerRadius > 0.0f && targetRadius > 0.0f)
                {
                    float surfaceMargin = player->normalEnemyRushSurfaceMargin;
                    if (!std::isfinite(surfaceMargin))
                        surfaceMargin = 0.0f;
                    surfaceMargin = std::clamp(surfaceMargin, 0.0f, 1.0f);
                    rushStopDistance = (std::max)(minimumNormalEnemyRushStopDistance,
                        playerRadius + targetRadius + surfaceMargin);
                    usedCapsuleDistance = true;
                }
            }
        }


        this->rushStopDistance = rushStopDistance;
        // ルートモーションを無視する
        if (const auto controller = player->GetBodyAnimationController())
        {
            constexpr float rushApproachPoseTime = 0.766f;
            controller->BlendToHeldAnimationPose(
                "Jog_Fwd", rushApproachPoseTime, player->rushApproachPoseBlendTime);
        }

        // A zero-duration Windup preserves the previous immediate dash behavior.
        if (player->GetRushWindupDuration() <= FLT_EPSILON)
        {
            player->characterMovementComponent->MoveToActor(target, player->moveToEnemyInterval, this->rushStopDistance);
        }
        else
        {
            phase = RushPhase::Windup;
            player->SetRushWindupActive(true);
        }
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
    if (player->rushTarget.expired())
    {
        player->ForceResetBossSlow();
        player->GetStateMachine()->ChangeState("Idle");
        return;
    }
    if (auto target = player->rushTarget.lock())
    {
        const auto targetStateMachine = target->GetStateMachine();
        if (target->IsDefeated() ||
            (targetStateMachine && std::string(targetStateMachine->GetStateName()) == "EnemyDeathState")&&(!player->IsFinalHitAttackDrainActive()))
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
    case RushPhase::Windup:
    {
        // Do not query completion until this one MoveToActor call starts the dash.
        windupElapsedTime += (std::max)(deltaTime, 0.0f);
        if (windupElapsedTime >= player->GetRushWindupDuration())
        {
            if (const auto target = player->rushTarget.lock())
            {
                player->characterMovementComponent->MoveToActor(
                    target, player->moveToEnemyInterval, rushStopDistance);
                phase = RushPhase::DashToTarget;
                player->SetRushWindupActive(false);
            }
        }
        break;
    }
    case RushPhase::DashToTarget:
        if (player->characterMovementComponent->IsMoveToActorFinished())
        {// targetまで付いたら、
            Logger::Log(U8("Rush中のtargetまで移動完了"));
            // The held Jog_Fwd approach pose remains the source of the first Rush attack blend.
            // Later Rush combo transitions still use PlayBodyAnimation's 0.3 s default.
            if (const auto controller = player->GetBodyAnimationController())
                controller->ReleaseRuntimeHeldAnimationPose(true);
            player->PlayBodyAnimation(currentAttackAnimation, false, true, 0.10f);
            phase = RushPhase::Attack;
            player->SetRushAttackActive(true);
        }
        break;
    case RushPhase::Attack:
    {
        const bool animationPlaying =
            player->GetBodyAnimationController()->IsPlayAnimation();

        if (player->IsFinalHitRushFollowUpStopRequested())
        {
            queuedAttackCount = 0;
            if (player->bufferCommand.type == Player::ActionType::Attack)
                player->ConsumeActionRequest(Player::ActionType::Attack);
            if (!animationPlaying)
            {
                phase = RushPhase::Finished;
            }
            break;
        }
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
            // Rush reaches Idle only after Finished observes the clip end.
            // Latch here so the direct Idle path cannot capture an in-flight pose.
            player->LatchFinalHitVisualPoseAfterAnimationUpdate();
            player->GetStateMachine()->ChangeState("Idle");
        }
        break;
    }
}

void PlayerRushState::Exit()
{
    player->SetRushWindupActive(false);
    player->SetRushAttackActive(false);
    if (const auto controller = player->GetBodyAnimationController())
        controller->ReleaseRuntimeHeldAnimationPose();
    if (player->IsFinalHitAttackDrainActive() &&
        !player->IsFinalHitAttackDrainLatched())
    {
        player->MarkFinalHitAttackDrainInterrupted();
    }
    if (auto gruxTarget = rushHpDisplayTarget.lock())
    {
        gruxTarget->NotifyPlayerRushEnded(player);
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
    windupElapsedTime = 0.0f;
    rushStopDistance = 0.0f;
    currentAttackAnimation = "Rush_Attack_Fast_A";
    elapsedTime = 0.0f;

    player->ClearFinalHitRushFollowUpStop();
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

