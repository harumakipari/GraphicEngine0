#pragma once
#include "ActionBase.h"
#include <cstdint>

// Each attack owns its duration in BossAttackData, not in the action.
class ExecuteAttackRecovery : public ActionBase
{
public:
    using ActionBase::ActionBase;
    ExecuteAttackRecovery(GruxEnemy* enemy, bool consumeOverride)
        : ActionBase(enemy), consumeRecoveryOverride(consumeOverride) {}
    State Run(float deltaTime) override;
    void Reset() { started = false; sequenceId = 0; timer = 0.0f; duration = 0.0f; }
    void ResetRuntime() override { Reset(); }
private:
    bool consumeRecoveryOverride = false;
    bool started = false;
    uint64_t sequenceId = 0;
    float timer = 0.0f;
    float duration = 0.0f;
};
