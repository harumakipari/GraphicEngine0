#pragma once
#include "ActionBase.h"
#include "JudgementBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

class CanPlanJumpAttack : public JudgementBase
{
public:
    using JudgementBase::JudgementBase;
    bool Judgment() override;
};

class PrepareJumpSetupTarget : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};

class MoveToAttackSetupTarget : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};

class CanPlanAnyAttack : public JudgementBase
{
public:
    using JudgementBase::JudgementBase;
    bool Judgment() override;
};

class FaceJumpPlayerIfNeeded : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};

class CanExecuteJumpAttack : public JudgementBase
{
public:
    using JudgementBase::JudgementBase;
    bool Judgment() override;
};

class StartJumpAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
    void ResetRuntime() override { started = false; }
private:
    bool started = false;
};

class ExecuteJumpAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
    void ResetRuntime() override { started = false; executionStarted = false; finishAfterAnimation = false; stageHitCount = 0; }
private:
    bool started = false;
    bool executionStarted = false;
    bool finishAfterAnimation = false;
    int stageHitCount = 0;
};
