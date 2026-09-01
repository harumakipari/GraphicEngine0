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

    // ステージから出ていくときのメソッド
    virtual void Exit() = 0;

    virtual const char* GetName() const = 0;

protected:
    Player* player = nullptr;
public:
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

// ダッシュステートオブジェクト
class PlayerDashState : public PlayerStateBase
{
public:
    // コンストラクタ
    PlayerDashState(Player* player) :PlayerStateBase(player) {}
    // デストラクタ
    ~PlayerDashState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Dash"; }
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
    bool dodgeQueued = false;
    // True only while the synchronous Attack -> Attack combo transition runs.
    bool continuingComboTransition = false;
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
    bool rushRequested = false;
    bool judgeSuccess = false;
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
    const char* GetPhaseName() const
    {
        return phase == Phase::KnockBack ? "KnockBack" : "GetUp";
    }

private:
    Phase phase = Phase::KnockBack;
};class PlayerDeathState : public PlayerStateBase
{
public:
    PlayerDeathState(Player* player) : PlayerStateBase(player) {}
    void Enter() override;
    void Execute(float deltaTime) override;
    void Exit() override;
    const char* GetName() const override { return "Death"; }
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
        DashToTarget,   // 敵へ接近
        Attack,         // ラッシュ攻撃
        Finished,
    };
public:
    // コンストラクタ
    PlayerRushState(Player* player) :PlayerStateBase(player) {}
    // デストラクタ
    ~PlayerRushState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Rush"; }
    // 0-based Rush step. comboIndex remains the single source of truth.
    int GetComboIndex() const { return comboIndex; }
    const std::string& GetCurrentAttackAnimationForDebug() const { return currentAttackAnimation; }
    int GetQueuedAttackCountForDebug() const { return queuedAttackCount; }
    bool WasTransitionWindowObservedForDebug() const { return rushTransitionWindowObserved; }
private:
    bool AdvanceRushCombo();
    bool IsFinalRushAttack() const;
    void ResetRushInputGrace();

    float elapsedTime = 0.0f;
    bool rushComboAdvanced = false;
    int queuedAttackCount = 0;
    int comboIndex = 0; 
    bool rushTransitionWindowObserved = false;

    std::string currentAttackAnimation = "Rush_Attack_Fast_A";

    std::vector<std::string> rushCombo; // ラッシュコンボのアニメーションを持つ
    RushPhase phase = RushPhase::DashToTarget;
    std::weak_ptr<GruxEnemy> rushHpDisplayTarget;
};

#if 0
// インタラクトステートオブジェクト
class PlayerInteractState : public PlayerStateBase
{
public:
    // コンストラクタ
    PlayerInteractState(Player* player) :PlayerStateBase(player) {}
    // デストラクタ
    ~PlayerInteractState() = default;
    // ステートに入った時のメソッド
    void Enter() override;
    // ステートで実行するメソッド
    void Execute(float deltaTime) override;
    // ステートから出ていくときのメソッド
    void Exit() override;
    // ステート名を取得
    const char* GetName() const override { return "Interact"; }
};
#endif // 0


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

