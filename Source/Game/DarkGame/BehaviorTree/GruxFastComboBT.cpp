#include "pch.h"
#include "GruxFastComboBT.h"
#include "NodeBase.h"

bool CanPlanFastCombo::Judgment()
{
    const bool result = owner->CanPlanFastCombo();
    owner->SetBehaviorTreeLastJudgment(result ? "CanPlanFastCombo: true" : "CanPlanFastCombo: false");
    return result;
}

bool CanExecuteFastCombo::Judgment()
{
    const bool result = owner->CanExecuteFastCombo();
    owner->SetBehaviorTreeLastJudgment(result ? "CanExecuteFastCombo: true" : "CanExecuteFastCombo: false");
    return result;
}

// ボスが死亡したかどうか
bool DeadJudgment::Judgment()
{
    return owner->IsDead();
}

ActionBase::State BTStartDeath::Run(float)
{
    //GruxEnemyの既存の更新終了時の死亡処理がDeathStateを開始する。
    started = true;
    return State::Complete;
}

ActionBase::State BTExecuteDeath::Run(float)
{
    return owner->IsDead() ? State::Run : State::Complete;
}

ActionBase::State BTIdle::Run(float deltaTime)
{
    if (!started)
    {
        timer = 0.0f;
        started = true;
        owner->StopAIMovement();
        const auto controller = owner->GetBodyAnimationController();
        if (!controller || controller->GetCurrentAnimationName() != "TravelMode_Idle_0")
            owner->PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
    }
    timer += deltaTime;
    if (timer < owner->GetBehaviorIdleDuration())
    {
        return State::Run;
    }
    started = false;
    return State::Complete;
}

ActionBase::State ApproachIfNeeded::Run(float dt)
{
    if (owner->IsFastComboInRange())
    {
        owner->StopAIMovement();
        started = false;
        timer = 0.0f;
        return State::Complete;
    }
    if (!started)
    {
        owner->BeginFastComboApproach();
        started = true;
        timer = 0.0f;
    }
    timer += dt;
    if (timer >= owner->GetFastComboApproachMaxDuration())
    {
        owner->StopAIMovement();
        started = false;
        timer = 0.0f;
        return State::Failed;
    }
    if (!owner->UpdateFastComboApproach(dt))
        return State::Run;
    owner->StopAIMovement();
    started = false;
    timer = 0.0f;
    return State::Complete;
}

ActionBase::State FacePlayerIfNeeded::Run(float dt)
{
    auto c = owner->BuildTargetContext();
    if (!c.valid)
        return State::Failed;
    if (owner->IsPlayerInFastComboFacingRange(c))
        return State::Complete;
    owner->RotateTowardsPlayer(c.directionToPlayer, owner->GetTurnSpeed(), dt, "BT_FacePlayer");
    return owner->IsPlayerInFastComboFacingRange(owner->BuildTargetContext()) ? State::Complete : State::Run;
}

ActionBase::State StartFastCombo::Run(float)
{
    owner->SetSelectedAttackForBehaviorTree(BossAttackType::FastCombo);
    owner->StartAttack();
    owner->OnSelectedActionStartedSuccessfully();
    return State::Complete;
}

ActionBase::State ExecuteFastCombo::Run(float)
{
    if (!started)
    {
        stage = 0;
        stageHitCount = owner->GetCurrentAttackHitCount();
        started = true;
        finishAfterAnimation = false;
        if (!owner->PlayAttackStage(BossAttackType::FastCombo, stage))
        {
            started = false;
            return State::Failed;
        }
    }
    if (finishAfterAnimation)
    {
        auto controller = owner->GetBodyAnimationController();
        if (controller && controller->IsPlayAnimation())
            return State::Run;
        owner->StartSelectedActionCooldown();
        started = false;
        finishAfterAnimation = false;
        return State::Complete;
    }

    if (owner->GetCurrentAttackHitCount() > stageHitCount || owner->WasCurrentAttackSequenceJustDodged())
    {
        owner->OnSelectedAttackCompletedSuccessfully();
        const bool justDodged = owner->WasCurrentAttackSequenceJustDodged();
        owner->SetBehaviorAttackResult(justDodged ? GruxEnemy::BehaviorAttackResult::JustDodged : GruxEnemy::BehaviorAttackResult::Success);
        owner->DisableAttackHitBoxes();
        finishAfterAnimation = true;
        return State::Run;
    }

    if (owner->IsTransitionWindowActive())
    {
        auto c = owner->GetBodyAnimationController();
        auto a = c ? c->GetAnimationAsset(c->GetCurrentAnimationName()) : nullptr;

        if (a && !a->nextCombo.empty())
        {
            ++stage;
            stageHitCount = owner->GetCurrentAttackHitCount();
            if (!owner->PlayAttackAnimationByName(a->nextCombo))
            {
                started = false;
                return State::Failed;
            }
            return State::Run;
        }
    }
    if (owner->GetBodyAnimationController()->IsPlayAnimation())
        return State::Run;
    owner->OnSelectedAttackCompletedSuccessfully();
    owner->SetBehaviorAttackResult(GruxEnemy::BehaviorAttackResult::Success);
    owner->StartSelectedActionCooldown();
    started = false;
    if (!owner->GetBodyAnimationController()->IsPlayAnimation())
        return State::Complete;
}

ActionBase::State ExecuteFastComboRecovery::Run(float dt)
{
    if (!started)
    {
        owner->BeginRecovery();
        timer = 0.0f;
        started = true;
    }
    timer += dt;
    if (timer < owner->GetBehaviorRecoveryDuration())
        return State::Run;
    started = false;
    return State::Complete;
}

ActionBase::State PrepareFastCombo::Run(float deltaTime)
{
    if (!started)
    {
        timer = 0.0f;
        started = true;
        owner->StopAIMovement();
        const auto controller = owner->GetBodyAnimationController();
        if (!controller || controller->GetCurrentAnimationName() != "TravelMode_Idle_0")
            owner->PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
        owner->PlayAttackReadySE();
    }
    timer += deltaTime;
    if (timer < owner->GetBehaviorPrepareDuration())
        return State::Run;
    started = false;
    return State::Complete;
}

// 近距離攻撃を予定していいかどうか
bool GruxEnemy::CanPlanFastCombo() const
{
    const auto c = BuildTargetContext();
    if (!c.valid || c.region == PlayerRelativeRegion::Back)
    {// playerが後ろにいる時
        return false;
    }
    float maxRange = closeCombatSettings.planMaxRange;
    for (const auto& a : combatAttackData)
    {
        if (a.type == BossAttackType::FastCombo)
        {// 近距離攻撃の最大範囲を取得する
            break;
        }
    }
    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        if (combatActionData[i].type == BossActionType::FastCombo && (combatActionData[i].weight <= 0.0f ||
            (c.xzDistance <= maxRange && combatActionCooldownRemaining[i] > 0.0f)))
        {
            return false;
        }
    }
    return c.xzDistance <= maxRange;
}

// 近距離攻撃が実行可能かどうか
bool GruxEnemy::CanExecuteFastCombo() const
{
    const auto c = BuildTargetContext();
    if (!c.valid || !IsPlayerInFastComboFacingRange(c))
        return false;
    float minRange = closeCombatSettings.minRange, maxRange = closeCombatSettings.executeMaxRange;
    for (const auto& a : combatAttackData)
    {
        if (a.type == BossAttackType::FastCombo)
        {
            break;
        }
    }

    if (c.xzDistance < minRange || c.xzDistance > maxRange)
        return false;
    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        if (combatActionData[i].type == BossActionType::FastCombo)
            return combatActionCooldownRemaining[i] <= 0.0f;
    }
    return false;
}

bool GruxEnemy::IsFastComboInRange() const
{
    const auto c = BuildTargetContext();
    if (!c.valid) return false;
    return c.xzDistance >= closeCombatSettings.minRange && c.xzDistance <= closeCombatSettings.executeMaxRange;
}

bool GruxEnemy::IsPlayerInFastComboFacingRange(const BossTargetContext& c) const
{
    return c.valid && c.absoluteAngleDegrees <= closeCombatSettings.facingLimitDegrees;
}

void GruxEnemy::BeginFastComboApproach()
{
    const auto controller = GetBodyAnimationController();
    if (!controller || controller->GetCurrentAnimationName() != "TravelMode_Fwd_0")
        PlayBodyAnimation("TravelMode_Fwd_0", true, true, 0.15f, true);
    behaviorApproachActive = true;
}

bool GruxEnemy::UpdateFastComboApproach(float dt)
{
    const auto c = BuildTargetContext();
    if (!c.valid)
        return false;
    if (IsFastComboInRange())
        return true;
    if (characterMovementComponent)
    {
        for (const auto& p : combatPositioningData)
        {
            if (p.actionType == BossActionType::Approach)
            {
                characterMovementComponent->SetFixedSpeed(p.moveSpeed);
                break;
            }
        }
        characterMovementComponent->SetInputMagnitude(1.0f);
        characterMovementComponent->SetMoveDirection(c.directionToPlayer);
    }
    RotateTowardsPlayer(c.directionToPlayer, GetTurnSpeed(), dt, "BT_Approach");
    return false;
}


// Recoveryの処理を開始
void GruxEnemy::BeginRecovery() const
{
    PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.5f, true);
}


float GruxEnemy::GetBehaviorRecoveryDuration() const
{
    for (const auto& a : combatAttackData)
    {
        if (a.type == BossAttackType::FastCombo)
            return a.recoveryDuration;
    }
    return recoveryDuration;
}

void GruxEnemy::UpdateBehaviorTree(float dt)
{
    if (!aiTree || !behaviorData)
        return;
    if (!activeNode)
    {// 現在実行されているノードが無ければ
        // 次に実行するノードを推論する。
        activeNode = aiTree->ActiveNodeInference(behaviorData.get());
        behaviorTreeCurrentNode = activeNode ? activeNode->GetName() : "None";
    }
    if (!activeNode)
        return;
    behaviorTreePreviousNode = activeNode->GetName();
    behaviorTreeCurrentNode = activeNode->GetName();
    activeNode = aiTree->Run(activeNode, behaviorData.get(), dt);
    const auto result = aiTree->GetLastRunResult();
    behaviorTreeLastResult = result == ActionBase::State::Run ? "Run" : result == ActionBase::State::Complete ? "Complete" : "Failed";

    if (!activeNode)
    {
        behaviorTreeCurrentNode = "None";
        return;
    }
    if (result == ActionBase::State::Complete)
    {
        behaviorTreeCurrentNode = "None";
    }
    else if (activeNode->GetName() != behaviorTreePreviousNode)
    {
        behaviorTreeCurrentNode = activeNode->GetName();
        behaviorTreeLastResult = "Complete";
    }
}
