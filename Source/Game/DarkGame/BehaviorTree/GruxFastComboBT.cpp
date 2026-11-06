#include "pch.h"
#include "GruxFastComboBT.h"
#include "NodeBase.h"


// FastCombo conditions
bool CanPlanFastCombo::Judgment()
{
    const bool result = owner->CanPlanFastCombo();
    return result;
}

bool CanExecuteFastCombo::Judgment()
{
    const bool result = owner->CanExecuteFastCombo();
    return result;
}

// Common BT: death and idle
bool DeadJudgment::Judgment()
{
    return owner->IsDead();
}

ActionBase::State BTStartDeath::Run(float)
{
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
    if (owner->IsInitialRepositionFallbackIdlePending())
        owner->CompleteInitialRepositionFallbackIdle();
    return State::Complete;
}

// FastCombo action nodes
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
        owner->StartFastComboApproachRetryCooldown();
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
    if (owner->IsPlayerInFastComboFaceCompleteRange(c))
        return State::Complete;
    owner->RotateTowardsPlayer(c.directionToPlayer, owner->GetTurnSpeed(), dt);
    return owner->IsPlayerInFastComboFaceCompleteRange(owner->BuildTargetContext()) ? State::Complete : State::Run;
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
        {
            owner->PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
        }
        CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_attack_ready1.wav", 1.0f);
        owner->StartWeaponTelegraph("Attack_A_Fast_0");
    }
    timer += deltaTime;
    if (timer < owner->GetBehaviorPrepareDuration())
        return State::Run;
    started = false;
    return State::Complete;
}


ActionBase::State StartFastCombo::Run(float)
{
    owner->SetSelectedAttackForBehaviorTree(BossAttackType::FastCombo);
    owner->StartAttack();
    owner->OnSelectedActionStartedSuccessfully();
    return State::Complete;
}

ActionBase::State ExecuteFastCombo::Run(float dt)
{
    if (!started)
    {
        stage = 0; stageHitCount = owner->GetCurrentAttackHitCount(); started = true;
        finishAfterAnimation = false; timer = 0.0f;
        runtimeState = GruxEnemy::FastComboRuntimeState::Attack;
        owner->SetFastComboRuntimeStage(stage); owner->SetFastComboRuntimeState(runtimeState);
        owner->RefreshFastComboTargetContext(stage);
        if (!owner->PlayAttackStage(BossAttackType::FastCombo, stage)) { owner->ClearFastComboStepIn(); started = false; return State::Failed; }
    }
    if (owner->IsFastComboStepInComplete()) return State::Run;
    if (finishAfterAnimation)
    {
        auto controller = owner->GetBodyAnimationController();
        if (controller && controller->IsPlayAnimation()) return State::Run;
        owner->StartSelectedActionCooldown(); started = false; finishAfterAnimation = false; return State::Complete;
    }
    if (owner->GetCurrentAttackHitCount() > stageHitCount || owner->WasCurrentAttackSequenceJustDodged())
    {
        owner->DisableAttackHitBoxes(); finishAfterAnimation = true; return State::Run;
    }
    if (runtimeState == GruxEnemy::FastComboRuntimeState::InterStageDelay)
    {
        timer += dt;
        if (timer < owner->GetInterStageFaceDelay()) return State::Run;
        runtimeState = GruxEnemy::FastComboRuntimeState::InterStageFacing; owner->SetFastComboRuntimeState(runtimeState);
    }
    if (runtimeState == GruxEnemy::FastComboRuntimeState::InterStageFacing)
    {
        const BossTargetContext context = owner->BuildTargetContext();
        if (!context.valid || context.absoluteAngleDegrees > owner->GetInterStageMaxFacingAngle())
        {
            owner->DisableAttackHitBoxes(); finishAfterAnimation = true; runtimeState = GruxEnemy::FastComboRuntimeState::Attack; owner->SetFastComboRuntimeState(runtimeState); return State::Run;
        }
        if (context.absoluteAngleDegrees > owner->GetInterStageFaceCompleteAngle())
        {
            owner->RotateTowardsPlayer(context.directionToPlayer, owner->GetTurnSpeed(), dt);
            return State::Run;
        }
        runtimeState = GruxEnemy::FastComboRuntimeState::Attack; owner->SetFastComboRuntimeState(runtimeState);
        stageHitCount = owner->GetCurrentAttackHitCount();
        auto controller = owner->GetBodyAnimationController();
        auto animation = controller ? controller->GetAnimationAsset(controller->GetCurrentAnimationName()) : nullptr;
        if (!animation || animation->nextCombo.empty())
        { owner->DisableAttackHitBoxes(); finishAfterAnimation = true; return State::Run; }
        owner->ClearFastComboStepIn();
        owner->RefreshFastComboTargetContext(stage);
        owner->StartWeaponTelegraph(animation->nextCombo);
        if (!owner->PlayAttackAnimationByName(animation->nextCombo))
        { owner->DisableAttackHitBoxes(); finishAfterAnimation = true; return State::Run; }
        return State::Run;
    }
    if (owner->IsTransitionWindowActive())
    {
        auto controller = owner->GetBodyAnimationController();
        auto animation = controller ? controller->GetAnimationAsset(controller->GetCurrentAnimationName()) : nullptr;
        if (animation && !animation->nextCombo.empty())
        {
            ++stage; owner->ClearFastComboStepIn(); owner->SetFastComboRuntimeStage(stage);
            if (!owner->GetFastComboTargetContext().valid || owner->GetFastComboTargetContext().absoluteAngleDegrees > owner->GetInterStageMaxFacingAngle())
            {
                owner->DisableAttackHitBoxes();
                finishAfterAnimation = true;
                return State::Run; 
            }
            timer = 0.0f;
            runtimeState = GruxEnemy::FastComboRuntimeState::InterStageDelay; 
            owner->SetFastComboRuntimeState(runtimeState); 
            owner->StopAIMovement(); 
            return State::Run;
        }
    }
    if (owner->GetBodyAnimationController()->IsPlayAnimation())
    {
        return State::Run;
    }
    owner->ClearFastComboStepIn(); 
    owner->StartSelectedActionCooldown(); 
    started = false; 
    return State::Complete;
}

// FastCombo planning and approach
bool GruxEnemy::CanPlanFastCombo() const
{
    if (IsFastComboApproachRetryCooldownActive())
    {
        return false;
    }
    const auto c = BuildTargetContext();
    if (!c.valid || c.region == PlayerRelativeRegion::Back)
    {
        return false;
    }
    float maxRange = closeCombatSettings.planMaxRange;
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

void GruxEnemy::StartFastComboApproachRetryCooldown()
{
    fastComboApproachRetryRemaining = fastComboApproachRetryCooldown;
}

bool GruxEnemy::IsFastComboApproachRetryCooldownActive() const
{
    return fastComboApproachRetryRemaining > 0.0f;
}

bool GruxEnemy::CanExecuteFastCombo() const
{
    const auto c = BuildTargetContext();
    if (!c.valid || !IsPlayerInFastComboFacingRange(c))
        return false;
    float minRange = closeCombatSettings.minRange, maxRange = closeCombatSettings.executeMaxRange;

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

bool GruxEnemy::IsPlayerInFastComboFaceCompleteRange(const BossTargetContext& c) const
{
    return c.valid && c.absoluteAngleDegrees <= closeCombatSettings.faceCompleteAngleDegrees;
}

void GruxEnemy::BeginFastComboApproach()
{
    const auto controller = GetBodyAnimationController();
    if (!controller || controller->GetCurrentAnimationName() != "TravelMode_Fwd_0")
        PlayBodyAnimation("TravelMode_Fwd_0", true, true, 0.15f, true);
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
    RotateTowardsPlayer(c.directionToPlayer, GetTurnSpeed(), dt);
    return false;
}


// Common recovery
void GruxEnemy::BeginRecovery() const
{
    PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.5f, true);
}



