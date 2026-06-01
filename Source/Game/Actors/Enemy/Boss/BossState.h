#pragma once
#include "Game/State/StateBase.h"

class Enemy;

class EnemyStateBase : public State
{
public:
    EnemyStateBase(Enemy* player);
    virtual ~EnemyStateBase() = default;
    // ステートに入った時のメソッド
    virtual void Enter() = 0;

    // ステートで実行するメソッド
    virtual void Execute(float deltaTime) = 0;

    // ステージから出ていくときのメソッド
    virtual void Exit() = 0;

    virtual const char* GetName() const = 0;

protected:
    Enemy* enemy = nullptr;

};

// 待機ステートオブジェクト
class EnemyIdleState :public EnemyStateBase
{
public:
    // コンストラクタ
    EnemyIdleState(Enemy* enemy) :EnemyStateBase(enemy) {}
    // デストラクタ
    virtual ~EnemyIdleState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Idle"; }
};

class EnemyWalkState : public EnemyStateBase
{
public:
    // コンストラクタ
    EnemyWalkState(Enemy* enemy) :EnemyStateBase(enemy) {}
    // デストラクタ
    ~EnemyWalkState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Walk"; }
};

// 攻撃ステートオブジェクト
class EnemyAttackState : public EnemyStateBase
{
public:
    // コンストラクタ
    EnemyAttackState(Enemy* enemy) :EnemyStateBase(enemy) {}
    // デストラクタ
    ~EnemyAttackState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Attack"; }
};

// 攻撃前の予兆ステートオブジェクト
class EnemyCastState :public EnemyStateBase
{
    public:
    // コンストラクタ
    EnemyCastState(Enemy* enemy) :EnemyStateBase(enemy) {}
    // デストラクタ
    ~EnemyCastState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Cast"; }
};

// クールダウンステートオブジェクト
class EnemyCoolDownState :public EnemyStateBase
{
    public:
    // コンストラクタ
    EnemyCoolDownState(Enemy* enemy) :EnemyStateBase(enemy) {}
    // デストラクタ
    ~EnemyCoolDownState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "CoolDown"; }
};