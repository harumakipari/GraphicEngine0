#include "pch.h"
#include "GruxFastComboBT.h"

bool CanPlanFastCombo::Judgment()
{
    return owner->CanPlanFastCombo();
}

bool CanExecuteFastCombo::Judgment()
{
    return owner->CanExecuteFastCombo();
}

ActionBase::State ApproachIfNeeded::Run(float dt)
{
    if (owner->IsFastComboInRange())
    {
        owner->StopAIMovement();
        started = false;
        return State::Complete;
    }
    if (!started)
    {
        owner->BeginFastComboApproach();
        started = true;
    }
    if (!owner->UpdateFastComboApproach(dt))
        return State::Run;
    owner->StopAIMovement();
    started = false;
    return owner->CanExecuteFastCombo() ? State::Complete : State::Failed;
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
    if (!owner->CanExecuteFastCombo()) return State::Failed;
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
        if (!owner->PlayAttackStage(BossAttackType::FastCombo, stage))
        {
            started = false;
            return State::Failed;
        }
    }
    if (owner->IsTransitionWindowActive())
    {
        if (owner->GetCurrentAttackHitCount() > stageHitCount || owner->WasCurrentAttackSequenceJustDodged())
        {
            owner->OnSelectedAttackCompletedSuccessfully();
            owner->SetBehaviorAttackResult(GruxEnemy::BehaviorAttackResult::JustDodged);
            owner->StartSelectedActionCooldown();
            started = false;
            return State::Complete;
        }
        auto c = owner->GetBodyAnimationController();
        auto a = c ? c->GetAnimationAsset(c->GetCurrentAnimationName()) : nullptr;
        if (a && !a->nextCombo.empty())
        {
            ++stage;
            stageHitCount = owner->GetCurrentAttackHitCount();
            if (!owner->PlayAttackAnimationByName(a->nextCombo))
            {
                started = false; return State::Failed;
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

// 近距離攻撃を予定していいかどうか
bool GruxEnemy::CanPlanFastCombo() const
{
    const auto c = BuildTargetContext();
    if (!c.valid || c.region == PlayerRelativeRegion::Back)
    {// playerが後ろにいる時
        return false;
    }
    float maxRange = 0.0f;
    for (const auto& a : combatAttackData)
    {
        if (a.type == BossAttackType::FastCombo)
        {// 近距離攻撃の最大範囲を取得する
            maxRange = a.maxDistance;
            break;
        }
    }
    float approachRange = 0.0f;
    for (const auto& p : combatPositioningData)
    {
        if (p.actionType == BossActionType::Approach)
        {// 近づく時の最大距離
            approachRange = p.maxMoveDistance;
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
    return c.xzDistance <= maxRange + approachRange;
}

// 近距離攻撃が実行可能かどうか
bool GruxEnemy::CanExecuteFastCombo() const
{
    const auto c = BuildTargetContext();
    if (!c.valid || !IsPlayerInFastComboFacingRange(c))
        return false;
    float minRange = 0.0f, maxRange = 0.0f;
    for (const auto& a : combatAttackData)
    {
        if (a.type == BossAttackType::FastCombo)
        {
            minRange = a.minDistance; maxRange = a.maxDistance; break;
        }
    }

    if (c.xzDistance < minRange || c.xzDistance > maxRange)
        return false;
    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        if (combatActionData[i].type == BossActionType::FastCombo)
            return combatActionData[i].weight > 0.0f && combatActionCooldownRemaining[i] <= 0.0f;
    }
    return false;
}

bool GruxEnemy::IsFastComboInRange() const
{
    const auto c = BuildTargetContext();
    if (!c.valid) return false;
    for (const auto& a : combatAttackData)
        if (a.type == BossAttackType::FastCombo)
            return c.xzDistance >= a.minDistance && c.xzDistance <= a.maxDistance;
    return false;
}

bool GruxEnemy::IsPlayerInFastComboFacingRange(const BossTargetContext& c) const
{
    return IsFacingPlayerForAttack(c);
}

void GruxEnemy::BeginFastComboApproach()
{
    PlayBodyAnimation("TravelMode_Fwd_0", true, true, 0.15f, true);
    behaviorApproachActive = true;
}

bool GruxEnemy::UpdateFastComboApproach(float dt)
{
    const auto c = BuildTargetContext();
    if (!c.valid) 
        return false;
    if (CanExecuteFastCombo()) 
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
    PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
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
    }
    if (!activeNode)
        return;
    // ビヘイビアツリーからノードを実行。
    activeNode = aiTree->Run(activeNode, behaviorData.get(), dt);
}
