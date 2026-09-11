#include "pch.h"
#include "AttackRecoveryBT.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

ActionBase::State ExecuteAttackRecovery::Run(float deltaTime)
{
    // A fresh sequence also resets a recovery that was interrupted.
    if (!started || sequenceId != owner->GetCurrentAttackSequenceId())
    {
        sequenceId = owner->GetCurrentAttackSequenceId();
        duration = consumeRecoveryOverride ? owner->ConsumeNextRecoveryDuration()
            : owner->GetSelectedAttackRecoveryDuration();
        owner->BeginRecovery();
        timer = 0.0f;
        started = true;
    }
    timer += (std::max)(0.0f, deltaTime);
    owner->UpdateRecoveryDebug(timer, duration);
    if (timer < duration)
        return State::Run;
    started = false;
    if (owner->IsDashAttackBTActive())
        owner->CleanupDashAttackBT();
    return State::Complete;
}
