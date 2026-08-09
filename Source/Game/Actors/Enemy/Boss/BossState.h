#pragma once
#include "Game/State/StateBase.h"
#include "BossAITypes.h"

class GruxEnemy;

class EnemyStateBase : public State
{
public:
    EnemyStateBase(GruxEnemy* player);
    virtual ~EnemyStateBase() = default;
    // ステートに入った時のメソッド
    virtual void Enter() = 0;

    // ステートで実行するメソッド
    virtual void Execute(float deltaTime) = 0;

    // ステージから出ていくときのメソッド
    virtual void Exit() = 0;

    virtual const char* GetName() const = 0;

protected:
    GruxEnemy* enemy = nullptr;

};

// 待機ステートオブジェクト
class EnemyIdleState :public EnemyStateBase
{
public:
    // コンストラクタ
    EnemyIdleState(GruxEnemy* enemy) :EnemyStateBase(enemy) {}
    // デストラクタ
    virtual ~EnemyIdleState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "EnemyIdleState"; }

};

// 次の攻撃を考えるステートオブジェクト
class EnemyThinkState :public EnemyStateBase
{
public:
    // コンストラクタ
    EnemyThinkState(GruxEnemy* enemy) :EnemyStateBase(enemy) {}
    // デストラクタ
    virtual ~EnemyThinkState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "EnemyThinkState"; }

private:
    bool attackSelected = false;
    float timer = 0.0f;
};


// 死亡ステートオブジェクト
class EnemyDeathState : public EnemyStateBase
{
public:
    // コンストラクタ
    EnemyDeathState(GruxEnemy* enemy) :EnemyStateBase(enemy) {}
    // デストラクタ
    ~EnemyDeathState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "EnemyDeathState"; }
};

// 攻撃ステートオブジェクト
class EnemyAttackState : public EnemyStateBase
{
public:
    // コンストラクタ
    EnemyAttackState(GruxEnemy* enemy) :EnemyStateBase(enemy) {}
    // デストラクタ
    ~EnemyAttackState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "EnemyAttackState"; }

private:
    int comboStage = 0;
};

class EnemyRecoveryState : public EnemyStateBase
{
public:
    EnemyRecoveryState(GruxEnemy* enemy) : EnemyStateBase(enemy) {}
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "EnemyRecoveryState"; }

private:
    float timer = 0.0f;
};



