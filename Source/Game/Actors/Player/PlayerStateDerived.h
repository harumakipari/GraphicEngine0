#pragma once
#include "Game/State/StateBase.h"

class Player;

class PlayerStateBase : public State
{
public:
    PlayerStateBase(Player* player);
    virtual ~PlayerStateBase() = default;
    // ステートに入った時のメソッド
    virtual void Enter() = 0;

    // ステートで実行するメソッド
    virtual void Execute(float deltaTime) = 0;

    // ステージから出ていくときのメソッド
    virtual void Exit() = 0;

    void Reset();

    virtual const char* GetName() const = 0;

protected:
    Player* player = nullptr;


};

// 待機ステートオブジェクト
class PlayerIdleState : public PlayerStateBase
{
public:
    // コンストラクタ
    PlayerIdleState(Player* player) :PlayerStateBase(player) {}
    // デストラクタ
    virtual ~PlayerIdleState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Idle"; }
};

// 移動ステートオブジェクト
class PlayerRunningState : public PlayerStateBase
{
public:
    // コンストラクタ
    PlayerRunningState(Player* player) :PlayerStateBase(player) {}
    // デストラクタ
    ~PlayerRunningState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Running"; }
};

// 攻撃ステートオブジェクト
class PlayerAttackState : public PlayerStateBase
{
public:
    // コンストラクタ
    PlayerAttackState(Player* player) :PlayerStateBase(player) {}
    // デストラクタ
    ~PlayerAttackState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Attack"; }

private:
    float attackTimer = 0.0f;
    bool hitDone = false;
};

// 回避ステートオブジェクト
class PlayerDodgeState : public PlayerStateBase
{
public:
    // コンストラクタ
    PlayerDodgeState(Player* player) :PlayerStateBase(player) {}
    // デストラクタ
    ~PlayerDodgeState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Dodge"; }

private:
    float dodgeTimer = 0.0f;

};

// ジャンプステートオブジェクト
class PlayerJumpState : public PlayerStateBase
{
public:
    enum class JumpState :uint8_t
    {
        JumpStart,
        JumpMid,
        JumpLand,
        JumpAttack,
    };

    // コンストラクタ
    PlayerJumpState(Player* player) :PlayerStateBase(player) {}
    // デストラクタ
    ~PlayerJumpState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Jump"; }
private:
    //上方向への力
    float jumpPower = 5.0f;
    //重力
    float gravity = -9.8f;
    // ジャンプステート
    JumpState jumpState = JumpState::JumpStart;

    bool isAnimatedRecovering = false;

};

// ジャンプステートオブジェクト
class PlayerJumpAttackState : public PlayerStateBase
{
public:
    enum class JumpState :uint8_t
    {
        JumpAttack,
        JumpLand,
    };

    // コンストラクタ
    PlayerJumpAttackState(Player* player) :PlayerStateBase(player) {}
    // デストラクタ
    ~PlayerJumpAttackState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "JumpAttack"; }
private:
    //上方向への力
    float jumpPower = 5.0f;
    //重力
    float gravity = -9.8f;
    // ジャンプステート
    JumpState jumpState = JumpState::JumpAttack;
};

