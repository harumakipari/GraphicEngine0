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
        hp = maxHp;
    }

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void DrawImGuiDetails()override;

    void Finalize()override {}

    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

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

    // 軌跡を描画する処理
    void RenderTrail(ID3D11DeviceContext* immediateContext);

    // Focus開始時のForwardを設定する
    void SetFocusDirection(const DirectX::XMFLOAT3& focusDir)
    {
        this->focusDirection = focusDir;
    }

    // 回避方向を取得する
    DodgeDirection GetDodgeDirection()const { return dodgeDirection; }

    void SetIsPlayerTransparency(const bool isTransparency) { moviePerform = !isTransparency; }

    // モード変更用関数
    void SetLocomotionMode(LocomotionMode mode);


private:
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

    // ラッシュ受付期間終了
    void EndRushAttackInput();

    // ジャスト回避を受け付けるかどうか
    bool GetJustDodgeWindow()const { return  justDodgeWindow; }

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

private:
    // 動作更新処理
    void UpdateMovement();

    // 回避の方向を決定する処理
    void DecideLockOnDodgeDirection();


private:
    // プレイヤーのマックスHP
    int maxHp = 100;

    DirectX::XMFLOAT3 damageKnockbackDirection{ 0.0f, 0.0f, 1.0f };
    float damageKnockbackPower = 2.5f;

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

    float moveToEnemyInterval = 0.2f;  // ラッシュ後の敵までへのダッシュにかかる時間
    float motionWarpDesiredAttackSurfaceDistance = 0.5f;
    float attackRotationMaxCorrectionDegrees = 55.0f;
    float attackRotationSpeedDegrees = 240.0f;
    std::weak_ptr<Enemy> attackTarget;
    float attackRotationStartYaw = 0.0f;
    bool attackRotationTracking = false;

    std::weak_ptr<Enemy> rushTarget; // ターゲットを選択

    // 入力受付のコマンド
    ActionRequest bufferCommand{}; // 入力コマンド
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
    std::shared_ptr<UIImageComponent> rushButtonImageComponent;
    // ラッシュの時のコンボカウント（回避中にもラッシュをカウントするための変数）
    int rushQueuedAttackCount = 0;
private:
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
    bool isAttackActive = false;    // プレイヤーが攻撃状態に入る
    DirectX::XMFLOAT3 ghostEdgeColor = { 0.0f,0.042f,0.253f };  // 残像の剣のベースカラー
    DirectX::XMFLOAT3 ghostInnerColor = { 1.0f,1.0f,1.0f }; // 残像の剣のベースカラー
    float ghostEdgeWidth = 1.0f; // 残像の輪郭
    DirectX::XMFLOAT4X4 prevSwordWorld; // 前回の姿勢
    bool isPrevSwordWorldValid = false;

    float weaponSphereRadius = 0.75f;    // 剣の球の当たり判定の半径

    // 剣の軌跡
    Trail trail;
    float trailRemainTime = 0.8f; // 残像が残る時間


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
    // 歩き走りダッシュのステート管理
    LocomotionMode locomotionMode = LocomotionMode::Idle;

    // スロー
    float slowMotionTimer = 0.0f;
    bool slowMotionActive = false;
    float slowMotionInterval = 0.3f;   // スローモーションの時間
    float slowMotionPlayerTimeScale = 0.2f;  // どれくらいスローモーションにタイム倍率
    float slowMotionEnemyTimeScale = 0.2f;  // どれくらいスローモーションにタイム倍率


    // プレイヤーの壁に近づいた時の透明度
    float transparencyMinAlpha = 0.1f;  // 最小透明度
    float transparencyMaxAlpha = 0.2f;  // 最大透明度

    // 演出中かどうか
    bool moviePerform = false;


    AnimationController::MoveDirection currentMoveDir = AnimationController::MoveDirection::Idle;


    friend class PlayerStateBase;
};
