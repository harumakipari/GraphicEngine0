#pragma once
#include "Game/State/StateBase.h"

class Player;
class GruxEnemy;

class PlayerStateBase : public State
{
public:
    PlayerStateBase(Player* player);
    virtual ~PlayerStateBase() = default;

    // ステートに入った時のメソッド
    virtual void Enter() = 0;

    // ステートで実行するメソッド
    virtual void Execute(float deltaTime) = 0;

    // Stateから退出するときのメソッド
    virtual void Exit() = 0;

    virtual const char* GetName() const = 0;

protected:
    Player* player = nullptr;
};

// 待機ステートオブジェクト
class PlayerIdleState : public PlayerStateBase
{
public:
    PlayerIdleState(Player* player) :PlayerStateBase(player) {}
    virtual ~PlayerIdleState() = default;
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "Idle"; }
};

// 移動ステートオブジェクト
class PlayerRunningState : public PlayerStateBase
{
public:
    PlayerRunningState(Player* player) :PlayerStateBase(player) {}
    ~PlayerRunningState() = default;
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "Running"; }
};


// 攻撃ステートオブジェクト
class PlayerAttackState : public PlayerStateBase
{
public:
    PlayerAttackState(Player* player) :PlayerStateBase(player) {}
    ~PlayerAttackState() = default;
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "Attack"; }

private:
    bool dodgeQueued = false;
    bool continuingComboTransition = false;
    std::string startAttackAnimation = "Primary_Attack_Fast_A";
};

// 回避ステートオブジェクト
class PlayerDodgeState : public PlayerStateBase
{
public:
    PlayerDodgeState(Player* player) :PlayerStateBase(player) {}
    ~PlayerDodgeState() = default;
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "Dodge"; }

private:
    bool rushRequested = false;
    bool justDodgeSuccessHandled = false;
};

class PlayerDamageState : public PlayerStateBase
{
public:
    PlayerDamageState(Player* player) : PlayerStateBase(player) {}
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "Damage"; }
};

class PlayerKnockBackState : public PlayerStateBase
{
public:
    enum class Phase : uint8_t
    {
        KnockBack,
        GetUp,
    };

    PlayerKnockBackState(Player* player) : PlayerStateBase(player) {}
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "KnockBack"; }
private:
    Phase phase = Phase::KnockBack;
};

class PlayerDeathPendingState : public PlayerStateBase
{
public:
    PlayerDeathPendingState(Player* player) : PlayerStateBase(player) {}
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "DeathPending"; }
};

class PlayerDeathState : public PlayerStateBase
{
public:
    PlayerDeathState(Player* player) : PlayerStateBase(player) {}
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "Death"; }

private:
    float elapsedTime = 0.0f;
};

class PlayerWinState : public PlayerStateBase
{
public:
    PlayerWinState(Player* player) : PlayerStateBase(player) {}
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "Win"; }
};

// ラッシュステート
class PlayerRushState : public PlayerStateBase
{
    enum class RushPhase :uint8_t
    {
        Windup,         
        DashToTarget,   // 敵へ接近
        Attack,         // ラッシュ攻撃
        Finished,
    };
public:
    PlayerRushState(Player* player) :PlayerStateBase(player) {}
    ~PlayerRushState() = default;
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "Rush"; }
    int GetComboIndex() const { return comboIndex; }
    const std::string& GetCurrentAttackAnimation() const { return currentAttackAnimation; }
private:
    void AdvanceRushCombo();
    bool IsFinalRushAttack() const;
    void ResetRushInputGrace();

    // Rush接近・予備動作の調整値
    float windupElapsedTime = 0.0f;
    float rushStopDistance = 0.0f;
    float moveToEnemyInterval = 0.2f;  // Rush接近時に敵へ移動する所要時間（秒）。
    const float rushApproachPoseTime = 0.766f;
    float rushApproachPoseBlendTime = 0.12f; // Rush接近姿勢へ切り替えるブレンド時間（秒）。
    float normalEnemyRushSurfaceMargin = 0.15f; // 通常敵へのRush停止位置で、敵表面から追加する余白（ワールド単位）。
    float rushWindupDuration = 0.20f;   // Rush予備動作時間

    int queuedAttackCount = 0;// ラッシュの時のコンボカウント
    int comboIndex = 0; 
    bool rushTransitionWindowObserved = false;

    std::string currentAttackAnimation = "Rush_Attack_Fast_A";

    std::vector<std::string> rushCombo; // ラッシュコンボのアニメーションを持つ
    RushPhase phase = RushPhase::DashToTarget;
    std::weak_ptr<GruxEnemy> rushHpDisplayTarget;
};



