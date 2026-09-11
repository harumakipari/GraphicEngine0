#include "pch.h"
#include "GruxJumpAttackBT.h"
#include "GruxDashAttackBT.h"
#include "NodeBase.h"

bool CanPlanJumpAttack::Judgment()
{
    const bool result = owner->CanPlanJumpAttack(); owner->SetBehaviorTreeLastJudgment(result ? "CanPlanJumpAttack: true" : "CanPlanJumpAttack: false");
    return result;
}

bool CanPlanAnyAttack::Judgment()
{
    // Match the Plan-parent gates, including Dash's battle / stun availability.
    const bool result = owner->CanPlanFastCombo() || owner->CanPlanJumpAttack() ||
        DashPlanAvailable(owner).Judgment() || owner->CanPlanChargeAttack();
    owner->SetBehaviorTreeLastJudgment(result ? "CanPlanAnyAttack: true" : "CanPlanAnyAttack: false");
    return result;
}

ActionBase::State PrepareJumpSetupTarget::Run(float)
{
    owner->SetSelectedAttackForBehaviorTree(BossAttackType::JumpAttack);
    return owner->PrepareJumpAttackSetupTarget() ? State::Complete : State::Failed;
}

ActionBase::State MoveToAttackSetupTarget::Run(float dt)
{
    if (!owner->HasAttackSetupTarget())
        return State::Failed;

    const auto result = owner->UpdateAttackSetupMovement(dt);

    if (result == GruxEnemy::AttackSetupMoveResult::Running)
        return State::Run;

    owner->StopAttackSetupMovement();

    if (result == GruxEnemy::AttackSetupMoveResult::Arrived)
        return State::Complete;

    owner->ClearAttackSetupTarget();
    return State::Failed;
}

ActionBase::State FaceJumpPlayerIfNeeded::Run(float dt)
{
    const auto context = owner->BuildTargetContext();
    if (!context.valid)
        return State::Failed;
    if (context.absoluteAngleDegrees <= owner->GetTurnCompleteAngle())
        return State::Complete;
    owner->RotateTowardsPlayer(context.directionToPlayer, owner->GetTurnSpeed(), dt, "BT_JumpFacePlayer");
    return State::Run;
}

bool CanExecuteJumpAttack::Judgment()
{
    const auto context = owner->BuildTargetContext();
    const bool result = context.valid && !owner->IsDead();
    owner->SetBehaviorTreeLastJudgment(result ? "CanExecuteJumpAttack: true" : "CanExecuteJumpAttack: false");
    return result;
}

ActionBase::State StartJumpAttack::Run(float)
{
    if (!started)
    {
        owner->SetSelectedAttackForBehaviorTree(BossAttackType::JumpAttack);
        if (!owner->StartJumpAttackTelegraph())
        {
            started = false;
            return State::Failed;
        }
        started = true;
    }
    started = false;
    return State::Complete;
}

ActionBase::State ExecuteJumpAttack::Run(float dt)
{
    if (!started)
    {
        started = true;
        executionStarted = false;
        finishAfterAnimation = false;
        stageHitCount = owner->GetCurrentAttackHitCount();
    }

    const auto controller = owner->GetBodyAnimationController();
    if (finishAfterAnimation)
    {
        if (controller && controller->IsPlayAnimation())
            return State::Run;
        owner->StartSelectedActionCooldown();
        started = false;
        return State::Complete;
    }

    if (!executionStarted)
    {
        if (owner->WasCurrentAttackSequenceJustDodged() || owner->GetCurrentAttackHitCount() > stageHitCount)
        {
            const bool dodged = owner->WasCurrentAttackSequenceJustDodged();
            owner->OnSelectedAttackCompletedSuccessfully();
            owner->SetBehaviorAttackResult(dodged ? GruxEnemy::BehaviorAttackResult::JustDodged : GruxEnemy::BehaviorAttackResult::Success);
            owner->DisableAttackHitBoxes();
            finishAfterAnimation = true;
            return State::Run;
        }
        if (!owner->UpdateJumpAttackTelegraph(dt))
            return State::Run;
        if (!owner->StartJumpAttackExecution())
            return State::Failed;
        executionStarted = true;
        stageHitCount = owner->GetCurrentAttackHitCount();
        return State::Run;
    }

    if (owner->GetCurrentAttackHitCount() > stageHitCount || owner->WasCurrentAttackSequenceJustDodged())
    {
        const bool dodged = owner->WasCurrentAttackSequenceJustDodged();
        owner->OnSelectedAttackCompletedSuccessfully();
        owner->SetBehaviorAttackResult(dodged ? GruxEnemy::BehaviorAttackResult::JustDodged : GruxEnemy::BehaviorAttackResult::Success);
        owner->DisableAttackHitBoxes();
        finishAfterAnimation = true;
        return State::Run;
    }
    if (controller && controller->IsPlayAnimation())
        return State::Run;
    owner->OnSelectedAttackCompletedSuccessfully();
    owner->SetBehaviorAttackResult(GruxEnemy::BehaviorAttackResult::Success);
    owner->StartSelectedActionCooldown();
    started = false;
    return State::Complete;
}
