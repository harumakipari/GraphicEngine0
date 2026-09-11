#pragma once
#include "ActionBase.h"
#include "JudgementBase.h"

class CanPlanRoar : public JudgementBase
{
public:
    using JudgementBase::JudgementBase;
    bool Judgment() override;
};
class CanPlanAnyDefensive : public CanPlanRoar
{
public:
    using CanPlanRoar::CanPlanRoar;
};
class StartRoar : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class ExecuteRoar : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float dt) override;
};
class FinishRoar : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
