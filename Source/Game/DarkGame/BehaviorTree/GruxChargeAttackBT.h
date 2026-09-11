#pragma once
#include "ActionBase.h"
#include "JudgementBase.h"
#include "AttackRecoveryBT.h"

class CanPlanChargeAttack : public JudgementBase
{
public:
    using JudgementBase::JudgementBase;
    bool Judgment() override;
};
class PrepareChargeSetupTarget : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class FaceChargePlayerIfNeeded : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class CanExecuteChargeAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class StartChargeAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class ExecuteChargeAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class ResolveChargeResult : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
// Charge-specific postconditions wrap the unchanged default attack recovery behavior.
class ExecuteChargeRecovery : public ActionBase
{
public:
    explicit ExecuteChargeRecovery(GruxEnemy* enemy) : ActionBase(enemy), timerAction(enemy, true) {}
    State Run(float) override;
private:
    ExecuteAttackRecovery timerAction;
};
