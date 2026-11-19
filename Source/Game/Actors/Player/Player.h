#pragma once
#include <algorithm>
#include <memory>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "Game/Actors/Base/Character.h"

#include "Components/Controller/ControllerComponent.h"
#include "Components/Render/MeshComponent.h"

#include "Core/ActorManager.h"
#include "Graphics/Renderer/TrailRenderer.h"
#include "UI/Widgets/Widget.h"

class AudioSourceComponent;
class IInteractable;
class Enemy;
class GruxEnemy;

class Player :public Character
{
public:
    enum class DodgeDirection :uint8_t
    {
        Forward,
        Backward,
        Left,
        Right,
    };

    enum class HitReactionDirection :uint8_t
    {
        Front,
        Back,
        Left,
        Right,
    };

    enum class ActionType :uint8_t
    {
        None,
        Attack,
        Dodge,
    };

    enum class DamagePolicy :uint8_t
    {
        Normal,
        IgnoreDodgeIFrames,
    };

    // ジャスト回避によるスロー演出の状態
    enum class JustDodgeSlowPhase : uint8_t
    {
        Inactive,
        Hold,
        Return,
        RushHold,
    };

    struct ActionRequest
    {
        ActionType type = ActionType::None;
        float remainTime = 0.0f;
        DodgeDirection dodgeDirection = DodgeDirection::Forward;
        DirectX::XMFLOAT3 dodgeWorldDirection{}; // ワールド座標で指定する回避方向
        bool useDodgeWorldDirection = false;
    };

    // チュートリアルの操作UI
    enum class OperationGuideItem : uint8_t
    {
        L,
        LT,
        Y,
        X,
        R,
        Count,
    };

    enum class RushPromptAnimationPhase : uint8_t
    {
        Hidden,
        AppearGrow,
        AppearSettle,
        PulseGrow,
        PulseReturn,
    };

    enum class LocomotionMode :uint8_t
    {
        None,
        TPSWalk,
        TPSRun,
        LockOnBlendRun,
        Idle,
    };

    enum class EyeCloseOverrideOwner :uint8_t
    {
        None,
        PrisonOpening,
        Death,
    };

    struct GameplayHudFadeEntry
    {
        std::shared_ptr<UIImageComponent> component;
        CoreColor color;
    };

    struct OperationGuideItemState
    {
        std::shared_ptr<UIImageComponent> image;
        DirectX::XMFLOAT2 position{};
        DirectX::XMFLOAT2 baseScale{ 0.35f, 0.35f };
        bool visible = false;
        bool learned = false;
        float animationTimer = 0.0f;
    };

public:
    explicit Player(const std::string& modelName) :Character(modelName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void Finalize()override;

    void DrawImGuiDetails()override;

    // 開始Notifyに応じて当たり判定、入力受付、無敵、MotionWarpを有効化する。
    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    // 終了Notifyに応じて、開始Notifyで有効化した戦闘状態を解除する。
    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    // 単発NotifyによるSE再生とゲームプレイイベントを処理する。
    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

    void OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event) override;
    void DrawAnimationEditorPreviewState(const AnimationNotifyState& state) override;

    void OnAnimationChanged() override;

    void UpdateLocomotionAnimation() override;

    void RestartLocomotionAnimation();

    // カメラ・演出・イベント
    // イベント開始に合わせて操作とHUD表示を切り替える。
    void StartEvent();

    // イベント終了に合わせて操作とHUD表示を復帰する。
    void EndEvent();

    // HPを変更せず、瀕死時の音と画面演出だけを停止する。
    void StopLowHpPresentation();

    void SetLowHpPresentationCinematicSuppressed(bool suppressed);

    // 牢獄開始演出用の座り姿勢とTransform制御を開始する。
    bool BeginPrisonOpeningSitting(const DirectX::XMFLOAT3& position, float yawDegrees);

    // 牢獄開始演出の立ち上がり姿勢へ移行する。
    bool BeginPrisonOpeningStanding();

    // 牢獄開始演出の座り姿勢Ping-Pong再生条件を設定する。
    void ConfigurePrisonOpeningSittingMotion(bool useStandingPingPong, float startTime, float endTime, float speed, float standingBlendTime);

    // 牢獄開始演出の姿勢制御を終了し、入力状態を復帰する。
    void EndPrisonOpeningSitting();

    bool BeginPrisonOpeningEyeClosed();
    void SetPrisonOpeningEyeClosedWeight(float weight);
    void EndPrisonOpeningEyeOverride();
    bool IsPrisonOpeningEyeOverrideActive() const;
    bool IsPrisonOpeningStandingFinished() const;

    // HUD・操作ガイド
    // Sceneが決定したHPバーの表示状態を反映する。
    void SetHpBarVisible(bool visible) const;
    // Sceneが決定したゲームプレイHUD全体の表示状態を反映する。
    void SetGameplayHudVisible(bool visible);
    // HUD表示方針を操作ガイドへ反映する。学習状態は変更しない。
    void SetOperationGuideHudVisible(bool visible);
    void SetOperationGuideItemVisible(OperationGuideItem item, bool visible);
    void SetOperationGuideItemLearned(OperationGuideItem item);
    bool IsOperationGuideItemVisible(OperationGuideItem item) const;
    bool IsOperationGuideItemLearned(OperationGuideItem item) const;
    bool IsDodging() const { return stateMachine && std::string(stateMachine->GetStateName()) == "Dodge"; }
    void SetMinimumHp(int value);
    void RestoreHpToMaxWithUiAnimation();
    void BeginGameplayHudFadeOut();
    void SetGameplayHudFadeAlpha(float alpha);

    // 戦闘遷移・最終ヒット
    // コンティニュー時のTransformと戦闘状態を初期状態へ戻す。
    void ResetForBattleContinue(const Transform& battleStartTransform);
    void ForceDirectionImmediate(const DirectX::XMFLOAT3& direction) const;
    void BeginDeathEyeClose();
    void UpdateDeathEyeClose(float deathElapsedTime);
    void EndDeathEyeClose();
    void UpdateDeathVisualFade(float deathElapsedTime);
    void SetFinalHitCallback(std::function<void(GruxEnemy*, const DirectX::XMFLOAT3&)> callback)
    {
        finalHitCallback = std::move(callback);
    }
    // 最終ヒット用に現在の攻撃を完走・姿勢固定する待機状態へ移行する。
    bool BeginFinalHitWait();

    void SetDeathCameraStartCallback(std::function<void()> callback)
    {
        deathCameraStartCallback = std::move(callback);
    }
    void SetDeathCameraTransparencyDisabled(bool disabled)
    {
        deathCameraTransparencyDisabled = disabled;
    }

    // HPやTransformを維持したまま、入力と攻撃を停止する。
    void StopBattleActions();
    // フェーズ遷移イベント中に停止した戦闘操作を再開する。
    void ResumeBattleActionsAfterEvent();
    bool IsRushActiveForPhaseTransition() const { return stateMachine && std::string(stateMachine->GetStateName()) == "Rush"; }
    bool IsRushActive() const { return IsRushActiveForPhaseTransition(); }
    bool IsRushWindupActive() const { return rushWindupActive; }
    bool IsRushAttackActive() const { return rushAttackActive; }
    void SetRushWindupActive(bool active) { rushWindupActive = active; }
    void SetRushAttackActive(bool active) { rushAttackActive = active; }
    bool IsJustDodgePresentationActive() const { return stateMachine && std::string(stateMachine->GetStateName()) == "Dodge" && justDodgeSuccess; }
    std::shared_ptr<Enemy> GetRushTarget() const { return rushTarget.lock(); }

    void RequestFinalHitRushFollowUpStop()
    {
        if (IsRushActiveForPhaseTransition())
            finalHitRushFollowUpStopRequested = true;
    }
    void RequestFinalHitVisualPoseLatch()
    {
        finalHitVisualPoseLatchRequested = true;
    }
    bool IsFinalHitVisualPoseLatchRequested() const
    {
        return finalHitVisualPoseLatchRequested;
    }

    // 見た目姿勢が固定済みで、Attack/Rush State を抜けたかを返す。
    bool IsPhase1LastHitAttackFinishedForTransition() const
    {
        const auto controller = GetBodyAnimationController();
        if (!controller || !controller->IsVisualPoseLatched() || !stateMachine)
            return false;
        const std::string stateName = stateMachine->GetStateName();
        return stateName != "Attack" && stateName != "Rush";
    }
    bool IsFinalHitRushFollowUpStopRequested() const { return finalHitRushFollowUpStopRequested; }
    void ClearFinalHitRushFollowUpStop()
    {
        finalHitRushFollowUpStopRequested = false;
    }
    bool IsFinalHitAttackDrainComplete() const;
    bool IsFinalHitAttackDrainActive() const { return finalHitAttackDrainActive; }
    bool IsFinalHitAttackDrainLatched() const { return finalHitAttackDrainCompleted; }
    // 最終ヒット用の攻撃完走待機を中断し、姿勢固定要求を解除する。
    void MarkFinalHitAttackDrainInterrupted();

    // HP、Transform、現在Stateを維持して一時的な戦闘状態を解除する。
    void ClearTransientBattleActions();
    // シネマティックでTransformを変更する前にゲームプレイ操作を終了する。
    void CancelGameplayActionsForCinematic();
    // シネマティック移動前に剣軌跡の履歴を消去する。
    void ClearSwordTrailForPhaseTransition();
    void EnterWinState();
    bool IsInWinState() const;
    // 描画・残像演出
    void RenderTrail(ID3D11DeviceContext* immediateContext);
    // カメラ連携
    void SetFocusDirection(const DirectX::XMFLOAT3& focusDir) { this->focusDirection = focusDir; }
    DodgeDirection GetDodgeDirection()const { return dodgeDirection; }
    const DirectX::XMFLOAT3& GetDodgeWorldDirection() const { return dodgeWorldDirection; }
    bool UsesDodgeWorldDirection() const { return useDodgeWorldDirection; }
    void CaptureDodgeWorldDirectionSnapshot();
    // カメラ遮蔽時のDither透明化を有効または無効にする。
    void SetCameraOcclusionTransparencyEnabled(const bool enabled) { cameraOcclusionTransparencySuppressed = !enabled; }

    // 入力要求・移動
    // 保存済みの入力要求を現在の状態で実行可能なState遷移へ変換し、成功時だけ消費する。
    bool TryExecuteActionRequest();
    void ConsumeActionRequest(ActionType expectedType);

    // 戦闘・攻撃判定
    // 無敵状態を考慮して成立した被弾だけを適用する。
    bool TryTakeDamage(int damage, const DirectX::XMFLOAT3& attackerPosition, DamagePolicy policy = DamagePolicy::Normal);
    bool StartKnockBack(const DirectX::XMFLOAT3& direction);
    // 生存中かつ破棄予定でなく、有効なStateを持ち、ノックバック・起き上がり・死亡中でないかを返す。
    bool IsBossAttackTargetAvailable() const;
    void ClearActionRequest();
    HitReactionDirection GetHitReactionDirection() const { return hitReactionDirection; }
    void StartAttack();

    // ジャスト回避・Rush
    // Just Dodge成功フラグとスロー演出を開始する。Rush入力受付はDodge Stateで管理する。
    void StartJustDodgeSuccess(const std::shared_ptr<Enemy>& enemy);
    bool CanAcceptInitialRushInput() const;
    void LatchFinalHitVisualPoseAfterAnimationUpdate();
    // Just Dodge成功後またはRush中の追撃機会が有効かを返す。
    bool IsRushOpportunityActive() const;
    // 初回Rush入力の受付状態を切り替え、ガイド表示を同期する。
    void SetRushInputAcceptance(bool accepting);
    // Player/Bossの解除タイミングを独立して管理する。
    void BeginPlayerSlowReturn();
    void BeginBossSlowReturn(bool afterRush = false);
    void HoldBossSlowForRush();
    void ForceResetPlayerSlow();
    void ForceResetBossSlow();
    // Rush状態に合わせて剣、軌跡、残像の色を更新する。
    void SetRushWeaponVisual(bool enabled);
    // Rushの最大連撃数を1～7回の範囲に丸めて返す。
    int GetMaxRushAttackCount() const { return std::clamp(maxRushAttackCount, 1, 7); }
    bool IsJustDodgeWindowActive() const { return justDodgeWindow; }
    // 回避開始時の方向と攻撃者方向からJust Dodge成立可否を判定する。
    bool CanJustDodgeAgainst(const DirectX::XMFLOAT3& attackerPosition);
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }
    void AcquireAttackTarget();
    // 攻撃対象へ角度制限付きで向きを補正する。
    void UpdateAttackTargetRotation(float deltaTime);
    void ClearAttackTarget();
    void SetIsBossBattle(const bool isBossBattle) { this->isBossBattle = isBossBattle; }
    bool IsBossBattle() const { return isBossBattle; }
private:
    void SetLocomotionMode(LocomotionMode mode);
    void UpdateJustDodgeTimeScales();
    void CapturePlayerPoseGhost();
    void UpdateTPSLocomotion();
    void UpdateLockOnLocomotion();
    bool CanShowInitialRushGuide() const;

    // 演出用の姿勢・Notify補助
    bool RebuildEyeClosePoseOverride();
    void ResetEyeCloseOverride();
    void ResetPlayerPoseGhost();
    void HandleAnimationPlaySE(const AnimationNotifyEvent& event) const;
    // 武器コンポーネントのローカルオフセットを反映した攻撃判定位置を求める。
    DirectX::XMFLOAT3 GetHitBoxPoint(const std::shared_ptr<SceneComponent>& component, const DirectX::XMFLOAT3& localOffset) const;
    void UpdateSwordHitDetection(const DirectX::XMFLOAT3& playerPos, const DirectX::XMFLOAT3& swordRootPos, const DirectX::XMFLOAT3& swordMidPos, const DirectX::XMFLOAT3& swordTipPos);
    // 入力要求の内部処理
    void CaptureActionRequest(float deltaTime);
    void StoreActionRequest(ActionType type, float remainTime);

    // ノックバック・移動更新
    void BeginKnockBackMovement();
    void UpdateKnockBackMovement(float deltaTime);
    void StopKnockBackForcedMove();
    void EndKnockBackMovement();
    void UpdateMovement();
    // MotionWarpによる追加速度を反映し、移動と衝突判定を更新する。
    void UpdateMovementWithMotionWarp(float deltaTime);
    void UpdateSwordGhosts(float deltaTime);
    void UpdateSwordVisuals(float deltaTime, const DirectX::XMFLOAT3* swordRootPos, const DirectX::XMFLOAT3* swordTipPos, bool updateTrail);
    void UpdateWeaponVisualPresentation(float deltaTime, bool updateTrail = true);

    void InitializeSkeletalMeshAndAnimations(const std::string& parentName);
    void CreateUI();

    // HUD・操作ガイドの内部更新
    void UpdateRushPromptUI();
    void UpdateLockOnGuideUI();
    void HideAndResetLockOnGuideUI();
    void UpdateOperationGuideUI();
    // 現在HPと遅延HPのゲージ表示を更新する。
    void UpdateHpBarUI();
    // HUD、イベント、HP、項目状態から操作ガイドの表示を反映する。
    void RefreshOperationGuideUIVisibility() const;

    // 戦闘計算・回避方向の補助
    float GetRushDamageMultiplier() const;
    int GetCurrentAttackDamage() const;
    void DecideLockOnDodgeDirection();

    // インタラクト対象検索
    IInteractable* FindInteractable() const;

    void UpdateLowHpEffects();
    void TriggerLowHpPulse();
    void ResetLowHpEffects();

    void StartDamageFlash();
    void ApplyDamageFlash(float flashAmount) const;
    void UpdateDamageFlash();

    void UpdatePlayerPoseGhost();
    void UpdateCameraOcclusionTransparency() const;
    void ApplyDitherTransparencyAlpha(float alpha) const;
    bool CapturePlayerPoseGhost(size_t ghostIndex);
    // 牢獄開始演出中の座り姿勢Ping-Pong再生を更新する。
    void UpdatePrisonOpeningStandingPingPong(float deltaTime);

public:
    // 攻撃アニメーションとコンボ・Notify関連の状態
    std::string currentAttackAnimation = "Primary_Attack_Fast_A";
    std::string startAttackAnimation = "Primary_Attack_Fast_A";

    bool comboQueued = false;
    bool inputWindow = false;
    bool transitionWindow = false;
    bool justDodgeSuccess = false;

    bool invincible = false;



    // Rush対象と初回入力の受付状態
    std::weak_ptr<Enemy> rushTarget; // Just Dodge時に保持するRush対象。

    // 入力受付のコマンド
    ActionRequest bufferCommand{}; // 入力コマンド

    // Player本体の描画・操作コンポーネント
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<InputComponent> inputComponent;
    std::shared_ptr<RotationComponent> rotationComponent;
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;

    // 剣の残像用
    struct SwordGhost
    {
        std::shared_ptr<SkeletalMeshComponent> swordMeshComp;
        DirectX::XMFLOAT4X4 world = { };
        float alpha = 0.0f;
        bool isVisible = false;
    };
    std::array<SwordGhost, 8> ghosts;

    // Just Dodge成立時のPlayer固定Pose Ghost
    struct PlayerPoseGhost
    {
        std::shared_ptr<SkeletalMeshComponent> renderConstantsComponent;
        std::vector<InterleavedGltfModel::Node> nodes;
        DirectX::XMFLOAT4X4 world = {};
        float alpha = 0.0f;
        float elapsedTime = 0.0f;
        bool isVisible = false;
    };
    std::array<PlayerPoseGhost, 6> playerPoseGhosts;

private:
    // 操作UI
    float operationGuidePulseAmplitude = 0.185f;
    float operationGuidePulseSpeed = 3.5f; // 操作ガイドの拡縮アニメーション角速度（rad/s）。

    // Rushガイドの表示状態とフェード設定
    float rushPromptAlpha = 0.0f;
    float rushPromptFadeInDuration = 0.10f;
    RushPromptAnimationPhase rushPromptAnimationPhase = RushPromptAnimationPhase::Hidden;
    float rushPromptAnimationTimer = 0.0f;
    bool rushPromptWasVisible = false;

    // RushガイドのUI画像
    std::shared_ptr<UIImageComponent> rushGuideImageComponent;
    std::shared_ptr<UIImageComponent> rushButtonImageComponent;
    std::shared_ptr<UIImageComponent> rushWordImageComponent;
    // ロックオンガイドのUI画像
    std::shared_ptr<UIImageComponent> lockOnGuideArrowImageComponent;
    std::shared_ptr<UIImageComponent> lockOnGuideButtonImageComponent;

    // ロックオンガイドの表示状態
    float lockOnGuideOffscreenElapsed = 0.0f;
    float lockOnGuidePulseElapsed = 0.0f;
    bool lockOnGuideVisible = false;
    // ロックオンガイドの表示設定
    float lockOnGuideDelay = 3.0f;
    float lockOnGuidePulsePeriod = 1.2f;
    float lockOnGuidePulseMinScale = 0.92f;
    DirectX::XMFLOAT2 lockOnGuideArrowSize{ 96.0f, 64.0f };
    DirectX::XMFLOAT2 lockOnGuideArrowBaseScale{ 0.5f, 0.7f };
    DirectX::XMFLOAT2 lockOnGuideButtonSize{ 96.0f, 96.0f };
    DirectX::XMFLOAT2 lockOnGuideButtonBaseScale{ 1.0f, 1.0f };
    float lockOnGuideButtonOffset = 75.0f;
    float lockOnGuideEdgeMargin = 130.0f;
    float lockOnGuideArrowRotationOffset = 180.0f;

    // Rushガイドの配置・サイズ設定
    DirectX::XMFLOAT2 rushGuidePosition = { 1200.0f, 680.0f };
    DirectX::XMFLOAT2 rushButtonPosition = { 1200.0f, 600.0f };
    DirectX::XMFLOAT2 rushWordPosition = { 1197.0f, 532.0f };
    DirectX::XMFLOAT2 rushGuideSize = { 488.0f, 379.f };
    DirectX::XMFLOAT2 rushButtonSize = { 300.0f, 300.0f };
    DirectX::XMFLOAT2 rushWordSize = { 124.5f, 68.0f };
    DirectX::XMFLOAT2 rushGuideScale = { 0.32f, 0.32f };
    DirectX::XMFLOAT2 rushButtonBaseScale = { 0.32f, 0.32f };
    DirectX::XMFLOAT2 rushWordScale = { 1.0f, 1.0f };


    // HUD全体を非表示にしても、操作ガイドの学習状態を保持する。
    std::array<OperationGuideItemState, static_cast<size_t>(OperationGuideItem::Count)> operationGuideItems;
    bool operationGuideGameplayHudVisible = true;
    bool operationGuideEventVisible = true;

    float motionWarpDesiredAttackSurfaceDistance = 0.5f; // MotionWarp攻撃時に維持する敵表面からの距離（ワールド単位）。
    float attackRotationMaxCorrectionDegrees = 55.0f;
    float attackRotationSpeedDegrees = 240.0f; // 攻撃対象へ向く補正速度（度/秒）
    float attackAssistMaxDistance = 8.0f;

    // 通常攻撃・Rushのダメージとヒットストップの調整値
    int normalAttackDamage = 2;
    float rushDamageMultiplier = 1.f;
    float finalRushDamageMultiplier = 2.5f;
    int maxRushAttackCount = 7;
    float normalAttackHitStopDuration = 0.05f; // 通常攻撃のヒットストップ時間（秒）。
    float rushHitStopDuration = 0.03f; // Rush攻撃のヒットストップ時間（秒）。

    std::weak_ptr<Enemy> attackTarget;
    float attackRotationStartYaw = 0.0f;
    bool attackRotationTracking = false;

    // ノックバックの移動状態と調整値
    DirectX::XMFLOAT3 knockBackDirection{ 0.0f, 0.0f, 1.0f };
    float knockBackInitialSpeed = 8.0f;
    float knockBackDuration = 0.45f;
    float knockBackElapsed = 0.0f;
    bool knockBackForcedMoveActive = false;


    // 瀕死演出の判定・表示状態と調整値
    int lowHpThreshold = 13;    // 瀕死演出を開始するHP上限値。HPが1以上かつこの値以下で有効。
    bool lowHpActive = false;
    bool lowHpPresentationSuppressed = false;
    bool lowHpPresentationCinematicSuppressed = false;
    float heartbeatTimer = 0.0f;
    float heartbeatInterval = 3.0f;
    float lowHpPulseTimer = 0.0f;
    float lowHpPulseDuration = 0.4f;
    float lowHpPulseFlashAmount = 0.0f;
    float lowHpVignetteBaseAlpha = 0.12f;
    float lowHpVignettePulseAlpha = 0.32f;
    std::shared_ptr<UIImageComponent> lowHpVignetteImageComponent;

    // 被弾時の発光状態と調整値
    float damageFlashTimer = 0.0f;
    float damageFlashDuration = 0.25f;
    float damageFlashBodyTintStrength = 0.5f;
    float damageFlashRimStrength = 1.0f;
    DirectX::XMFLOAT3 damageFlashColor = { 0.85f, 0.08f, 0.05f };

    // 牢獄・死亡演出で共用する目閉じ姿勢
    float closedEyePoseTime = 4.045f;
    float closeEyeWeight = 0.0f;
    EyeCloseOverrideOwner eyeCloseOverrideOwner = EyeCloseOverrideOwner::None;
    // 死亡時の目閉じ・発光消失の状態と調整値
    bool deathEyeCloseActive = false;
    float deathEyeCloseDelay = 2.506f;
    float deathEyeCloseDuration = 1.4f;
    float deathColorFadeDelay = 4.018f;
    float deathColorFadeDuration = 2.05f;
    bool deathVisualFadeStarted = false;
    DirectX::XMFLOAT3 deathDeadEmissiveColor = { 0.35f, 0.35f, 0.35f };
    float playerAliveEmissionPower = 20.9f;

    // ヒット中に当たった敵を記録する
    std::unordered_set<Actor*> hitActors;
    // 武器判定・軌跡に使う剣の参照点
    std::shared_ptr<SceneComponent> swordRootComponent;
    std::shared_ptr<SceneComponent> swordMiddleComponent;
    std::shared_ptr<SceneComponent> swordTipComponent;

    // 剣の描画用メッシュコンポーネント
    std::shared_ptr<SkeletalMeshComponent> swordMeshComponent;

    bool hitBox = false;
    float activeHitBoxRadius = 0.75f;
    DirectX::XMFLOAT3 activeHitBoxOffset{ 0.0f, 0.0f, 0.0f }; // 武器判定に加えるローカルオフセット
    bool justDodgeWindow = false;
    bool showTrail = false;
    float swordEmissivePower = 0.0f;

    // MotionWarp Notifyの移動状態
    std::vector<AnimationMotionWarp> animationMotionWarps;

    bool invincibleWindow = false;

    float justDodgeMaxTowardAttackerDot = 0.25f; // Just Dodge成立判定に使う、回避方向と攻撃者方向の水平内積の上限値。
    DirectX::XMFLOAT3 dodgeWorldDirectionSnapshot{}; // 回避開始時に固定したワールド方向
    bool dodgeWorldDirectionSnapshotValid = false;

    // 武器の連続判定に使う前フレームの参照点
    DirectX::XMFLOAT3 prevSwordRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevSwordMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevSwordTipPos = { 0.0f,0.0f,0.0f };

    // 剣残像の生成・描画状態と調整値
    float swordGhostElapsedTime = 0.0f;
    int swordGhostIndex = 0;
    float ghostInterval = 0.015f;
    float ghostFadeTime = 0.6f;
    float swordGhostEmissive = 2.0f;    // 剣残像のエミッシブ強度。
    bool battleActionsSuspended = false;
    DirectX::XMFLOAT3 swordGhostColor = { 0.0f,0.042f,0.253f };
    DirectX::XMFLOAT3 ghostInnerColor = { 1.0f,1.0f,1.0f };
    DirectX::XMFLOAT3 rushSwordColor = { 0.5f,0.8f,1.6f };
    DirectX::XMFLOAT3 activeGhostBaseColor{}; // 通常またはRush状態を反映した剣残像の現在色
    float ghostEdgeWidth = 1.0f; // 剣残像の輪郭幅。
    DirectX::XMFLOAT4X4 prevSwordWorld;
    bool isPrevSwordWorldValid = false;

    // ジャスト回避時のPlayer残像の表示設定
    float playerPoseGhostInitialAlpha = 0.4f;
    float playerPoseGhostLifetime = 0.81f;
    float playerGhostSpawnInterval = 0.25f;
    DirectX::XMFLOAT3 playerPoseGhostColor = { 0.5f, 0.8f, 1.6f };
    float playerPoseGhostEmissive = 2.0f;
    DirectX::XMFLOAT3 playerPoseGhostEdgeColor = { 0.973f, 0.911f, 0.443f };
    DirectX::XMFLOAT3 playerPoseGhostInnerColor = { 1.0f, 1.0f, 1.0f };
    float playerPoseGhostEdgeWidth = 1.0f;
    // Player残像の生成状態
    float playerPoseGhostSpawnTimer = 0.0f;
    size_t nextPlayerPoseGhostIndex = 0;
    bool playerPoseGhostSpawnSequenceActive = false;

    // 剣の軌跡
    Trail trail;
    float trailRemainTime = 0.8f;

    std::shared_ptr<UIGaugeFillComponent> hpDelayedFillUiComponent;
    std::shared_ptr<UIGaugeFillComponent> hpCurrentFillUiComponent;
    std::vector<std::shared_ptr<UICoreComponent>> hpBarUiComponents;
    std::vector<GameplayHudFadeEntry> gameplayHudFadeEntries;
    float delayedHp = 0.0f;
    float delayedHpDelayTimer = 0.0f;
    float delayedHpDelayDuration = 0.25f;
    float delayedHpFollowSpeed = 25.0f;
    int minimumHp = 0;
    bool hpRecoveryUiActive = false;
    float hpRecoveryUiStart = 0.0f;
    float hpRecoveryUiElapsed = 0.0f;
    float hpRecoveryUiDuration = 0.5f;
    CoreColor playerHpCurrentColor{ 0.302f, 0.565f, 0.644f, 1.0f };
    CoreColor playerHpDelayedColor{ 0.711f, 0.958f, 0.993f, 1.0f };

    // カメラの注視点の位置
    std::shared_ptr<SceneComponent> cameraTargetComponent;
    DirectX::XMFLOAT3 focusDirection = {}; // Focus開始時に保持するPlayerの前方ベクトル。
    bool isBossBattle = false;

    // 回避方向
    DodgeDirection dodgeDirection = DodgeDirection::Backward;
    DirectX::XMFLOAT3 dodgeWorldDirection{}; // ワールド座標で指定する回避方向
    bool useDodgeWorldDirection = false;
    HitReactionDirection hitReactionDirection = HitReactionDirection::Front;
    float walkSpeed = 1.0f;
    float runSpeed = 5.15f;
    float forwardSpeedScale = 1.0f;
    float sideSpeedScale = 0.95f;
    float backwardSpeedScale = 1.0f;

    // 歩き走りダッシュのステート管理
    LocomotionMode locomotionMode = LocomotionMode::Idle;
    // Rush戦闘の進行状態
    bool rushWindupActive = false;
    bool rushAttackActive = false;
    bool rushInputAccepting = false;

    // ジャスト回避・Rushスローの進行状態
    JustDodgeSlowPhase playerSlowPhase = JustDodgeSlowPhase::Inactive;
    JustDodgeSlowPhase bossSlowPhase = JustDodgeSlowPhase::Inactive;
    float playerSlowHoldTimer = 0.0f;
    float bossSlowHoldTimer = 0.0f;
    float playerSlowReturnElapsed = 0.0f;
    float bossSlowReturnElapsed = 0.0f;

    // スロー復帰開始時の状態
    float playerSlowReturnStartScale = 1.0f;
    float bossSlowReturnStartScale = 1.0f;
    float activeBossSlowReturnDuration = 0.10f;
    // ジャスト回避・Rushスローの調整値
    float justDodgeTimeScale = 0.15f;
    float justDodgeSlowHoldDuration = 1.3f;     // Just Dodgeによるスローを維持する時間（秒）。
    float justDodgeSlowReturnDuration = 0.10f;
    float rushBossSlowScale = 0.10f;
    float rushBossReturnDuration = 0.10f;

    // プレイヤーの壁に近づいた時の透明度
    float transparencyStartRatio = 0.35f;
    float transparencyMinAlpha = 0.25f;
    float transparencyMaxAlpha = 1.f;
    // Movie演出中の透明化抑制
    bool cameraOcclusionTransparencySuppressed = false;
    // 牢獄開始演出の姿勢状態
    bool prisonOpeningSitting = false;
    bool prisonOpeningStanding = false;

    // 牢獄開始演出の姿勢再生設定
    bool prisonOpeningUseStandingPingPong = false;
    float prisonOpeningSittingPingPongStartTime = 0.0f;
    float prisonOpeningSittingPingPongEndTime = 0.5f;
    float prisonOpeningSittingPingPongSpeed = 0.6f;
    float prisonOpeningSittingToStandingBlendTime = 0.15f;
    // 牢獄開始演出の再生状態・入力復帰情報と固定Transform
    float prisonOpeningSittingPingPongPhase = 0.0f;
    bool prisonOpeningInputWasEnabled = true;
    DirectX::XMFLOAT3 prisonOpeningSittingPosition{};
    float prisonOpeningSittingYawDegrees = 0.0f;
    bool deathCameraTransparencyDisabled = false;
    // FinalHitの追撃停止・姿勢固定要求と攻撃終了の進行状態
    bool finalHitRushFollowUpStopRequested = false;
    bool finalHitVisualPoseLatchRequested = false;
    bool finalHitAttackDrainActive = false;
    bool finalHitAttackDrainCompleted = false;

    // FinalHitの待機状態と通知先
    bool finalHitWaiting = false;
    std::function<void(GruxEnemy*, const DirectX::XMFLOAT3&)> finalHitCallback;
    std::function<void()> deathCameraStartCallback;

    friend class PlayerStateBase;
    friend class PlayerKnockBackState;
    friend class PlayerDeathPendingState;
    friend class PlayerWinState;
};
