#pragma once
#include "Game/State/StateBase.h"

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

class EnemyWalkState : public EnemyStateBase
{
public:
    // コンストラクタ
    EnemyWalkState(GruxEnemy* enemy) :EnemyStateBase(enemy) {}
    // デストラクタ
    ~EnemyWalkState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "EnemyWalkState"; }
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

// 攻撃前の予兆ステートオブジェクト
class EnemyCastState :public EnemyStateBase
{
public:
    // コンストラクタ
    EnemyCastState(GruxEnemy* enemy) :EnemyStateBase(enemy) {}
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
    EnemyCoolDownState(GruxEnemy* enemy) :EnemyStateBase(enemy) {}
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