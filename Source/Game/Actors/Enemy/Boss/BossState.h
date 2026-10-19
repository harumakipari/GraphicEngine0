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


class EnemyIdleState :public EnemyStateBase
{
public:
    EnemyIdleState(GruxEnemy* enemy) :EnemyStateBase(enemy) {}
    virtual ~EnemyIdleState() = default;
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "EnemyIdleState"; }

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



class EnemyStunState : public EnemyStateBase
{
public:
    EnemyStunState(GruxEnemy* enemy) : EnemyStateBase(enemy) {}
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "EnemyStunState"; }

private:
    enum class Phase
    {
        Start,
        Loop,
        End,
    };

    Phase phase = Phase::Start;
    float stunElapsedTime = 0.0f;
};


