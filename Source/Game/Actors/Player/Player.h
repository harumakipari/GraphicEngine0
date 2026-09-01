#pragma once
#include <stack>
#include <memory>
#include "Game/Actors/Base/Character.h"
#include "Engine/Input/GamePad.h"

#include "Components/Controller/ControllerComponent.h"
#include "Components/Render/MeshComponent.h"

#include "Core/ActorManager.h"
#include "Components/Effect/ParticleComponent.h"
#include "Graphics/Renderer/TrailRenderer.h"
#include "UI/Widgets/Widget.h"

class AudioSourceComponent;
class IInteractable;
class Enemy;

class Player :public Character
{
public:
    struct ComboAttack
    {
        std::string animationName;
        int nextComboIndex = -1;
    };

    enum class DodgeDirection :uint8_t
    {
        Forward,
        Backward,
        Left,
        Right,
    };

    enum class ActionType :uint8_t
    {
        None,
        Attack,
        Dodge,
        Dash,
        Jump,
        Interact,
    };

    struct ActionRequest
    {
        ActionType type = ActionType::None;
        float remainTime = 0.0f;
        DodgeDirection dodgeDirection = DodgeDirection::Forward;
        DirectX::XMFLOAT3 dodgeWorldDirection{};
        bool useDodgeWorldDirection = false;
    };

    enum class SwordState :uint8_t
    {
        Equipped,   // 剣を装備している状態
        Sheathed    // 剣を鞘に収めている状態
    };

    // 回避方向
    enum class LocomotionMode :uint8_t
    {
        None,
        TPSWalk,
        TPSRun,
        LockOnBlendWalk,
        LockOnBlendRun,

        // 一時的に残すが使用禁止
        Idle,
        Dash,
    };
public:
    explicit Player(const std::string& modelName) :Character(modelName)
    {
        mass = 50.0f;
        maxHp = 50;
        maxHp = 10;
        hp = maxHp;
    }

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void DrawImGuiDetails()override;

    void Finalize()override { ForceResetPlayerSlow(); ForceResetBossSlow(); }

    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

    void OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event) override;

    void OnAnimationChanged() override;

    // ブレンドスペースのアニメーションを使用するかの更新関数
    void UpdateLocomotionAnimation() override;

    void RestartLocomotionAnimation();

    LocomotionMode GetLocomotionMode() const { return locomotionMode; }

    // TPSモードの移動時の更新処理
    void UpdateTPSLocomotion();

    // ロックオンモードの移動時の更新処理
    void UpdateLockOnLocomotion();

    // アニメーションステート関連のフラグをリセットする
    void ResetAnimationStateFlag();

    // イベントシーン開始時に呼ぶ処理
    void StartEvent();

    // イベントシーン終了時に呼ぶ処理
    void EndEvent();

    // Battle HUD visibility is decided by GameScene; Player only owns its components.
    void SetHpBarVisible(bool visible);

    // Clears Player-owned transient combat state and restores full HP at the saved battle start.
    void ResetForBattleContinue(const Transform& battleStartTransform);

    // Stops residual attacks when the battle has ended without restoring HP.
    void StopBattleActions();

    // Enters the terminal, animation-playing state used after the boss is defeated.
    void EnterWinState();
    bool IsInWinState() const;

    // 軌跡を描画する処理
    void RenderTrail(ID3D11DeviceContext* immediateContext);

    // Focus開始時のForwardを設定する
    void SetFocusDirection(const DirectX::XMFLOAT3& focusDir)
    {
        this->focusDirection = focusDir;
    }

    // 回避方向を取得する
    DodgeDirection GetDodgeDirection()const { return dodgeDirection; }
    const DirectX::XMFLOAT3& GetDodgeWorldDirection() const { return dodgeWorldDirection; }
    bool UsesDodgeWorldDirection() const { return useDodgeWorldDirection; }

    void SetIsPlayerTransparency(const bool isTransparency) { moviePerform = !isTransparency; }

    // モード変更用関数
    void SetLocomotionMode(LocomotionMode mode);

private:
    void ClearTransientBattleActions();

    void HandleAnimationPlaySE(const AnimationNotifyEvent& event);

    // 火花エフェクトの生成
    void SpawnSpark(DirectX::XMFLOAT3 hitPosition);

    // 剣の攻撃判定
    void CheckSwordLineHit(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end);

    // 入力処理をまとめる
    void CaptureActionRequest(float deltaTime);

    bool StoreActionRequest(ActionType type, float remainTime);

public:
    // 入力コマンドによってステートが変わるかどうか
    bool TryExecuteActionRequest();

    // 入力を消費する処理
    void ConsumeActionRequest(ActionType expectedType);

    // 無敵判定を含め、成立した被弾だけを適用する
    bool TryTakeDamage(int damage, const DirectX::XMFLOAT3& attackerPosition);

    // Damageとは独立した、外部攻撃からの強制移動開始口。
    bool StartKnockBack(const DirectX::XMFLOAT3& direction);

    void ClearActionRequest(const char* reason);

    const DirectX::XMFLOAT3& GetDamageKnockbackDirection() const
    {
        return damageKnockbackDirection;
    }
    float GetDamageKnockbackPower() const { return damageKnockbackPower; }

    // 攻撃開始時の処理
    void StartAttack();

    // 攻撃終了時の処理
    void EndAttack();

    // ジャスト回避成功時の処理　ラッシュ受付期間開始
    void StartJustDodgeSuccess(const std::shared_ptr<Enemy>& enemy);

    bool CanAcceptInitialRushInput() const;
    bool CanShowInitialRushGuide() const;
    bool CanShowRushComboGuide() const;
    bool CanShowRushPrompt() const;
    bool IsRushOpportunityActive() const;
    void SetRushInputAcceptance(bool accepting);
    void SetRushInputDebugState(bool judgeSuccess, bool rushRequested);

    // Player/Bossの解除タイミングを独立して管理する。
    void BeginPlayerSlowReturn();
    void BeginBossSlowReturn(bool afterRush = false);
    void HoldBossSlowForRush();
    void ForceResetPlayerSlow();
    void ForceResetBossSlow();

    // ラッシュ受付期間終了
    void EndRushAttackInput();

    // Rush State owns the lifetime of this visual override.
    void SetRushWeaponVisual(bool enabled);

    int GetMaxRushAttackCount() const { return std::clamp(maxRushAttackCount, 1, 7); }

    // ジャスト回避を受け付けるかどうか
    bool GetJustDodgeWindow()const { return  justDodgeWindow; }
    void BeginDodgeDebug();
    void UpdateDodgeDebug(float deltaTime);
    void RecordNormalDodgeDebug();
    bool IsJustDodgeDebugEnabled() const { return justDodgeDebugEnabled; }
    // カメラの目の位置を取得する
    const std::shared_ptr<SceneComponent>& GetCameraEyeComponent() { return cameraEyeComponent; }

    // カメラの注視点の位置
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }

    void AcquireAttackTarget();
    void UpdateAttackTargetRotation(float deltaTime);
    void StopAttackTargetRotation() { attackRotationTracking = false; }
    void ClearAttackTarget();

    // ボス戦時かどうかを設定する
    void SetIsBossBattle(const bool isBossBattle) { this->isBossBattle = isBossBattle; }
    bool IsBossBattle() const { return isBossBattle; }

private:
    void BeginKnockBackMovement();
    void UpdateKnockBackMovement(float deltaTime);
    void StopKnockBackForcedMove();
    void EndKnockBackMovement();

    // 動作更新処理
    void UpdateMovement();

    void UpdateRushPromptUI();

    float GetRushDamageMultiplier() const;
    int GetCurrentAttackDamage() const;

    // 回避の方向を決定する処理
    void DecideLockOnDodgeDirection();


private:

    DirectX::XMFLOAT3 damageKnockbackDirection{ 0.0f, 0.0f, 1.0f };
    float damageKnockbackPower = 2.5f;

    DirectX::XMFLOAT3 knockBackDirection{ 0.0f, 0.0f, 1.0f };
    float knockBackInitialSpeed = 8.0f;
    float knockBackDuration = 0.45f;
    float knockBackElapsed = 0.0f;
    bool knockBackActive = false;
    bool knockBackForcedMoveActive = false;

    // インタラクト対象検索
    IInteractable* FindInteractable();

public:
    // ヒット中に当たった敵を記録する
    std::unordered_set<Actor*> hitActors;

    //当たった相手を記録するためのセット 火花エフェクトの生成やダメージの適用を一度だけ行うために使用
    std::unordered_set<Actor*> hitTargets;
    bool hasPrevSwordTip = false; // 前フレームの剣先の位置が有効かどうか
    bool hasSpawnedThisAttack = false; // 今攻撃でエフェクトを生成したかどうか

    std::string currentAttackAnimation = "Primary_Attack_Fast_A";
    std::string startAttackAnimation = "Primary_Attack_Fast_A";    // コンボ開始のアニメーション

    bool comboQueued = false;   // コンボ攻撃がキューに入っているかどうか
    bool inputWindow = false;   // コンボ受付をするかどうか
    bool hitBox = false;   // 武器の当たり判定をつける
    bool transitionWindow = false;  // ステート遷移してもいいかどうか
    bool justDodgeWindow = false;  // ジャスト回避受付時間
    bool justDodgeSuccess = false; // ジャスト回避成功フラグ
    bool showTrail = false;     // 軌跡を出現させるかどうか
    float swordEmissivePower = 0.0f;    // 剣のエミッシブの力

    // アニメーション時にどれくらい移動するか
    std::vector<AnimationMotionWarp> animationMotionWarps;
    // 操作UI
    std::shared_ptr<UIImageComponent> operateUiComponent;


    bool invincibleWindow = false; // アニメーションによる無敵状態かどうか
    bool invincible = false; // 無敵状態かどうか

    float dodgeSpeed = 3.0f; // 回避する時のスピード
    float dodgeDuration = 0.5f; // 回避するときの時間

    bool justDodgeDebugEnabled = false;
    float dodgeDebugElapsed = 0.0f;
    bool normalDodgeRecordedThisDodge = false;
    bool justDodgeRecordedThisDodge = false;
    int dodgeDebugAttempts = 0;
    int dodgeDebugJustCount = 0;
    int dodgeDebugNormalCount = 0;
    int dodgeDebugDamageCount = 0;
    bool lastJustDodgeValid = false;
    std::string lastJustDodgeAttack = "None";
    float lastJustDodgeTime = 0.0f;
    float lastJustAnimationTime = 0.0f;
    float lastJustWindowStart = 0.0f;
    float lastJustWindowEnd = 0.0f;
    float lastJustWindowRatio = 0.0f;
    float lastJustBossHitBoxElapsed = -1.0f;
    float moveToEnemyInterval = 0.2f;  // ラッシュ後の敵までへのダッシュにかかる時間
    float motionWarpDesiredAttackSurfaceDistance = 0.5f;
    float attackRotationMaxCorrectionDegrees = 55.0f;
    float attackRotationSpeedDegrees = 240.0f;
    std::weak_ptr<Enemy> attackTarget;
    float attackRotationStartYaw = 0.0f;
    bool attackRotationTracking = false;

    std::weak_ptr<Enemy> rushTarget; // ターゲットを選択

    enum class RushPromptAnimationPhase : uint8_t
    {
        Hidden,
        AppearGrow,
        AppearSettle,
        PulseGrow,
        PulseReturn,
    };

    bool rushInputAccepting = false;
    bool rushJudgeSuccessDebug = false;
    bool rushRequestedDebug = false;
    bool swordHitDebug = false;
    float rushPromptAlpha = 0.0f;
    float rushPromptFadeInDuration = 0.10f;
    RushPromptAnimationPhase rushPromptAnimationPhase = RushPromptAnimationPhase::Hidden;
    float rushPromptAnimationTimer = 0.0f;
    bool rushPromptWasVisible = false;

    DirectX::XMFLOAT2 rushGuidePosition = { 520.0f, 450.0f };
    DirectX::XMFLOAT2 rushButtonPosition = { 680.0f, 450.0f };
    DirectX::XMFLOAT2 rushWordPosition = { 800.0f, 450.0f };
    DirectX::XMFLOAT2 rushGuideSize = { 244.0f, 197.5f };
    DirectX::XMFLOAT2 rushButtonSize = { 104.0f, 116.0f };
    DirectX::XMFLOAT2 rushWordSize = { 124.5f, 68.0f };
    DirectX::XMFLOAT2 rushGuideScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 rushButtonBaseScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 rushWordScale = { 1.0f, 1.0f };

    // 入力受付のコマンド
    ActionRequest bufferCommand{}; // 入力コマンド

    // Rush combat tuning values. Kept together for future runtime tuning.
    int normalAttackDamage = 1;
    float rushDamageMultiplier = 2.0f;
    float finalRushDamageMultiplier = 3.0f;
    int maxRushAttackCount = 7;

    float normalAttackHitStopDuration = 0.05f; // ヒットストップの秒数
    float rushHitStopDuration = 0.03f;

public:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    // 描画用コンポーネント（透明）を追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshBlendComponent;

    std::shared_ptr<ParticleComponent> sparkComponent; // 火花エフェクト用コンポーネント
    std::shared_ptr<InputComponent> inputComponent;
    std::shared_ptr<RotationComponent> rotationComponent;
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;
    std::shared_ptr<CapsuleComponent> swordCollisionComp;

    std::shared_ptr<SceneComponent> swordPointComp;

    std::shared_ptr<SceneComponent> swordRootComponent; // 剣の根元のコンポーネント
    std::shared_ptr<SceneComponent> swordMiddleComponent; // 剣の中間のコンポーネント
    std::shared_ptr<SceneComponent> swordTipComponent;  // 剣の先端のコンポーネント

    std::shared_ptr<SceneComponent> swordSheathComponent; // 剣をしまうときのコンポーネント


    float elapsedTime_ = 0.0f;
    bool isGrounded_ = false;

    struct TrailPoint
    {
        XMFLOAT3 position;
        float life;
    };
    std::vector<TrailPoint> trailPoints;

    SwordState swordState = SwordState::Equipped; // 剣の状態

    // 剣の描画用メッシュコンポーネント
    std::shared_ptr<SkeletalMeshComponent> swordMeshComponent;

    // 剣の残像用
    struct SwordGhost
    {
        std::shared_ptr<SkeletalMeshComponent> swordMeshComp;
        DirectX::XMFLOAT4X4 world = { };
        float alpha;
        bool isVisible = false;
    };
    std::array<SwordGhost, 8> ghosts;

    // ダッシュのスピード
    float dashSpeed = 6.2f;
    // 歩きのスピード
    float walkSpeed = 1.0f;
    // 走りのスピード
    float runSpeed = 5.15f;
    float forwardSpeedScale = 1.0f;
    float sideSpeedScale = 0.95f;
    float backwardSpeedScale = 1.0f;

    // ラッシュ時のUI
    std::shared_ptr<UIImageComponent> rushGuideImageComponent;
    std::shared_ptr<UIImageComponent> rushButtonImageComponent;
    std::shared_ptr<UIImageComponent> rushWordImageComponent;
    // ラッシュの時のコンボカウント（回避中にもラッシュをカウントするための変数）
    int rushQueuedAttackCount = 0;
private:
    void StartDamageFlash();
    void UpdateDamageFlash();
    void ApplyDamageFlash(float flashAmount);

    float damageFlashTimer = 0.0f;
    float damageFlashDuration = 0.25f;
    float damageFlashBodyTintStrength = 0.5f;
    float damageFlashRimStrength = 1.0f;
    DirectX::XMFLOAT3 damageFlashColor = { 0.85f, 0.08f, 0.05f };

    DirectX::XMFLOAT3 prevSwordTip; // 前フレームの剣先の位置
    float hitStopTimer = 0.0f; // ヒットストップのタイマー

    DirectX::XMFLOAT3 prevSwordRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevSwordMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevSwordTipPos = { 0.0f,0.0f,0.0f };

    // 剣をしまうときの描画用メッシュコンポーネント
    std::shared_ptr<SkeletalMeshComponent> swordSheathMeshComponent;

    // 剣の残像の調整値
    float swordGhostElapsedTime = 0.0f;
    int swordGhostIndex = 0;
    float ghostInterval = 0.015f; // 残像を出す間隔
    float ghostFadeTime = 0.6f; // 残像が残る時間
    DirectX::XMFLOAT3 swordGhostColor = { 0.5f,0.8f,1.6f }; // 残像の剣のベースカラー
    float swordGhostEmissive = 2.0f;    // 残像のemissiveColor
    bool battleActionsSuspended = false;
    bool isAttackActive = false;    // プレイヤーが攻撃状態に入る
    DirectX::XMFLOAT3 ghostEdgeColor = { 0.0f,0.042f,0.253f };  // 残像の剣のベースカラー
    DirectX::XMFLOAT3 ghostInnerColor = { 1.0f,1.0f,1.0f }; // 残像の剣のベースカラー
    DirectX::XMFLOAT3 rushSwordColor = { 0.5f,0.8f,1.6f };
    DirectX::XMFLOAT3 activeGhostBaseColor{};
    DirectX::XMFLOAT3 activeGhostEdgeColor{};
    bool rushWeaponVisualEnabled = false;
    float ghostEdgeWidth = 1.0f; // 残像の輪郭
    DirectX::XMFLOAT4X4 prevSwordWorld; // 前回の姿勢
    bool isPrevSwordWorldValid = false;

    float weaponSphereRadius = 0.75f;    // 剣の球の当たり判定の半径

    // 剣の軌跡
    Trail trail;
    float trailRemainTime = 0.8f; // 残像が残る時間

    std::shared_ptr<UIGaugeFillComponent> hpDelayedFillUiComponent;
    std::shared_ptr<UIGaugeFillComponent> hpCurrentFillUiComponent;
    std::vector<std::shared_ptr<UICoreComponent>> hpBarUiComponents;
    float delayedHp = 0.0f;
    float delayedHpDelayTimer = 0.0f;
    float delayedHpDelayDuration = 0.25f;
    float delayedHpFollowSpeed = 25.0f;
    CoreColor playerHpCurrentColor{ 0.70f, 0.10f, 0.08f, 1.0f };
    CoreColor playerHpDelayedColor{ 0.95f, 0.78f, 0.45f, 1.0f };

    // カメラの目の位置
    std::shared_ptr<SceneComponent> cameraEyeComponent;
    // カメラの注視点の位置
    std::shared_ptr<SceneComponent> cameraTargetComponent;
    // ボス戦時のオフセット
    float bossBattleCameraDistance = -3.0f;
    DirectX::XMFLOAT3 bossBattleCameraOffset = { -0.5f,2.0f,0.0f };

    // カメラの
    DirectX::XMFLOAT3 focusDirection = {};//　カメラのFocus開始時のForwawrd
    bool isBossBattle = false;  // ボス戦状態かどうか

    // 回避方向
    DodgeDirection dodgeDirection = DodgeDirection::Backward;
    DirectX::XMFLOAT3 dodgeWorldDirection{};
    bool useDodgeWorldDirection = false;
    // 歩き走りダッシュのステート管理
    LocomotionMode locomotionMode = LocomotionMode::Idle;

    // スロー
    enum class JustDodgeSlowPhase : uint8_t
    {
        Inactive,
        Hold,
        Return,
        RushHold,
    };

    JustDodgeSlowPhase playerSlowPhase = JustDodgeSlowPhase::Inactive;
    JustDodgeSlowPhase bossSlowPhase = JustDodgeSlowPhase::Inactive;
    float playerSlowHoldTimer = 0.0f;
    float bossSlowHoldTimer = 0.0f;
    float playerSlowReturnElapsed = 0.0f;
    float bossSlowReturnElapsed = 0.0f;

    
    float playerSlowReturnStartScale = 1.0f;
    float bossSlowReturnStartScale = 1.0f;
    float activeBossSlowReturnDuration = 0.10f;
    float justDodgeTimeScale = 0.2f;    // ジャスト回避のタイムスケール
    float justDodgeSlowHoldDuration = 1.0f;     // ジャスト回避の時間
    float justDodgeSlowReturnDuration = 0.10f;
    float rushBossSlowScale = 0.2f;
    float rushBossReturnDuration = 0.10f;

    // プレイヤーの壁に近づいた時の透明度
    float transparencyMinAlpha = 0.1f;  // 最小透明度
    float transparencyMaxAlpha = 0.2f;  // 最大透明度

    // 演出中かどうか
    bool moviePerform = false;

    AnimationController::MoveDirection currentMoveDir = AnimationController::MoveDirection::Idle;

    friend class PlayerStateBase;
    friend class PlayerKnockBackState;
    friend class PlayerDeathPendingState;
    friend class PlayerWinState;
};
