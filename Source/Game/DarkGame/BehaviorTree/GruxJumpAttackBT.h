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
private:
    bool started = false;
};

class ExecuteJumpAttack : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
private:
    bool started = false;
    bool executionStarted = false;
    bool finishAfterAnimation = false;
    int stageHitCount = 0;
};

class ExecuteJumpAttackRecovery : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
private:
    bool started = false;
    float timer = 0.0f;
};
