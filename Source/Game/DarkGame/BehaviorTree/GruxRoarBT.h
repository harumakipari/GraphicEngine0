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
    bool Judgment() override;
};
class CanPlanRetreat : public JudgementBase
{
public:
    using JudgementBase::JudgementBase;
    bool Judgment() override;
};
class CanPlanAnyCombatDecision : public JudgementBase
{ public: using JudgementBase::JudgementBase; bool Judgment() override; };
class CanPlanCombatBagAttack : public JudgementBase
{ public: using JudgementBase::JudgementBase; bool Judgment() override; };
class CanPlanReposition : public JudgementBase
{ public: using JudgementBase::JudgementBase; bool Judgment() override; };
class PrepareRepositionTarget : public ActionBase
{ public: using ActionBase::ActionBase; State Run(float) override; };
class MoveToRepositionTarget : public ActionBase
{ public: using ActionBase::ActionBase; State Run(float) override; };
class WaitAfterReposition : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float dt) override;
    void ResetRuntime() override { elapsed = 0.0f; }
private:
    float elapsed = 0.0f;
};

class PrepareRetreatTarget : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
class MoveToPositioningTarget : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float dt) override;
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
