#include "pch.h"
#include "AttackRecoveryBT.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

ActionBase::State ExecuteAttackRecovery::Run(float deltaTime)
{
	if (!started || sequenceId != owner->GetCurrentAttackSequenceId())
	{
		sequenceId = owner->GetCurrentAttackSequenceId();
		duration = consumeRecoveryOverride ? owner->ConsumeNextRecoveryDuration() : owner->GetRecoveryDurationForCurrentAttack();
		owner->BeginRecovery();
		timer = 0.0f;
		started = true;
	}
	timer += (std::max)(0.0f, deltaTime);
	if (timer < duration)
		return State::Run;
	started = false;
	if (owner->IsDashAttackBTActive())
		owner->CleanupDashAttackBT();
	return State::Complete;
}
