#pragma once
#include "ActionBase.h"
#include "JudgementBase.h"

class DashPlanAvailable : public JudgementBase
{
public:
    using JudgementBase::JudgementBase;
    bool Judgment() override;
};

// Conditions are actions here so Sequence inference cannot skip a failed gate.
class CanPlanDashAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class PrepareDashSetupTarget : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class FaceDashPlayerIfNeeded : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class CanExecuteDashAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class StartDashAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class ExecuteDashAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
