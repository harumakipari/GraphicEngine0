#pragma once
#include "ActionBase.h"
#include "JudgementBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

// 近距離攻撃を予定するかどうか
class CanPlanFastCombo : public JudgementBase
{
public:
    CanPlanFastCombo(GruxEnemy* enemy) :JudgementBase(enemy) {};
    // 判定
    bool Judgment() override;
};

// 近距離攻撃を実行できるか
class CanExecuteFastCombo : public JudgementBase
{
public:
    CanExecuteFastCombo(GruxEnemy* enemy) :JudgementBase(enemy) {};
    // 判定
    bool Judgment() override;
};

// 
class BTCompleteAction : public ActionBase
{
public:
    BTCompleteAction(GruxEnemy* enemy) :ActionBase(enemy) {}
    ActionBase::State Run(float elapsedTime) override { return State::Complete; }
};

// プレイヤーに近づく
class ApproachIfNeeded : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
private:
    bool started=false;
};

// プレイヤーの方を向く
class FacePlayerIfNeeded : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};

// 近距離攻撃を開始する
class StartFastCombo : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};

// 近距離攻撃を実行する
class ExecuteFastCombo : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
private:
    int stage = 0;
    int stageHitCount = 0;
    bool started = false;
};

// 近距離攻撃後の後隙
class ExecuteFastComboRecovery : public ActionBase
{
    float timer=0.0f;
    bool started=false;
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};
