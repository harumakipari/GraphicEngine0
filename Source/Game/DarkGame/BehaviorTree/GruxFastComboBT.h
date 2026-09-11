#pragma once
#include "ActionBase.h"
#include "JudgementBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

// 死亡処理を開始する
class BTStartDeath : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
private:
    bool started = false;
};

// 死亡処理を更新する
class BTExecuteDeath : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};

// ボスが死亡したかどうか
class DeadJudgment : public JudgementBase
{
public:
    using JudgementBase::JudgementBase;
    bool Judgment() override;
};

// 常にtrueを返す
class AlwaysJudgment : public JudgementBase
{
public:
    using JudgementBase::JudgementBase;
    bool Judgment() override { return true; }
};

// 待機処理
class BTIdle : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float deltaTime) override;
private:
    float timer = 0.0f;
    bool started = false;
};

// FastComboを攻撃候補として選択できるか
class CanPlanFastCombo : public JudgementBase
{
public:
    CanPlanFastCombo(GruxEnemy* enemy) :JudgementBase(enemy) {};
    // FastComboの計画可否を判定する
    bool Judgment() override;
};

// FastComboを実行できるか判定する
class CanExecuteFastCombo : public JudgementBase
{
public:
    CanExecuteFastCombo(GruxEnemy* enemy) :JudgementBase(enemy) {};
    // FastComboの即時実行可否を判定する
    bool Judgment() override;
};

// 常に完了する条件通過用Action
class BTCompleteAction : public ActionBase
{
public:
    BTCompleteAction(GruxEnemy* enemy) :ActionBase(enemy) {}
    ActionBase::State Run(float elapsedTime) override { return State::Complete; }
};

// FastComboの射程まで接近する
class ApproachIfNeeded : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
private:
    bool started = false;
    float timer = 0.0f;
};

// Player方向へ向き直る
class FacePlayerIfNeeded : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};


class StartFastCombo : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
};

// FastComboを実行する
class ExecuteFastCombo : public ActionBase
{
public:
    using ActionBase::ActionBase;
    State Run(float) override;
private:
    int stage = 0;
    int stageHitCount = 0;
    bool started = false;
    bool finishAfterAnimation = false;
    float timer = 0.0f;
    GruxEnemy::FastComboRuntimeState runtimeState = GruxEnemy::FastComboRuntimeState::Attack;
};


// FastCombo開始前の短い準備時間
class PrepareFastCombo : public ActionBase
{
    float timer = 0.0f;
    bool started = false;
public:
    using ActionBase::ActionBase;
    State Run(float deltaTime) override;
};
