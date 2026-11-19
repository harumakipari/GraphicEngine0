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
    State Run(float dt) override;
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
    State Run(float dt) override;
};

class StartChargeAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float dt) override;
};

class ExecuteChargeAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float dt) override;
};

class ResolveChargeResult : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float dt) override;
};

class ExecuteChargeRecovery : public ActionBase
{
public:
    explicit ExecuteChargeRecovery(GruxEnemy* enemy) : ActionBase(enemy), timerAction(enemy, true) {}
    State Run(float dt) override;
    void ResetRuntime() override { timerAction.ResetRuntime(); }
private:
    ExecuteAttackRecovery timerAction;
};