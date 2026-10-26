#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Components/Effect/ParticleComponent.h"
#include "Core/Actor.h"
#include "Game/Actors/Base/Character.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "UI/Widgets/Widget.h"
#include "Animation/DangerArea.h"
#include "Game/Actors/Enemy/Boss/BossAITypes.h"
#include "Graphics/Renderer/TrailRenderer.h"
#include <array>

#include "Engine/Audio/Audio.h"
#include "Game/DarkGame/BehaviorTree/BehaviorData.h"

class Player;
class BehaviorTree;
class NodeBase;

class MainChargeWindMeshComponent;
class GruxEnemy :public Enemy
{
public:
    enum class PlayerAttackHitType : uint8_t
    {
        Normal,  // 通常攻撃。
        ComboFinal,  // コンボ最終攻撃。
        Rush,  // Rush攻撃。
        RushFinal,  // Rush最終攻撃。
    };

    enum class DashBTPhase : uint8_t
    {
        None,  // 未設定。
        Setup,  // 準備中。
        Facing,  // 向き合わせ中。
        Telegraph,  // 予兆中。
        Movement,  // 移動中。
        Knockup,  // Knockup中。
        InterDashTransition,  // Dash間の遷移中。
        Recovery  // 硬直中。
    };

    enum class DashBTResult : uint8_t
    {
        Running,  // 実行中。
        Complete,  // 完了。
        Failed  // 失敗。
    };

    enum class ChargeBTPhase : uint8_t
    {
        None,  // 未設定。
        Setup,  // 準備中。
        Facing,  // 向き合わせ中。
        Telegraph,  // 予兆中。
        Charging,  // 突進中。
        Result,  // 結果処理中。
        Stun,  // スタン中。
        RecoveryPending,  // 硬直開始待ち。
        Recovery,  // 硬直中。
        RecoveryPostconditions  // 硬直後の条件確認中。
    };

    enum class TripleChargePhase : uint8_t
    {
        None,  // 未設定。
        InitialWindup,  // 初回の予備動作中。
        Charging,  // 突進中。
        InterChargeTransition,  // Charge間の遷移中。
        TelegraphHold,  // 予兆の保持中。
        Completed,  // 完了。
        Aborted  // 中断。
    };

    enum class ChargeBTStunPhase : uint8_t
    {
        None,  // 未設定。
        Start,  // 開始段階。
        Loop,  // 継続段階。
        End  // 終了段階。
    };

    enum class ChargeBTStepResult : uint8_t
    {
        Running,  // 実行中。
        Complete,  // 完了。
        Failed  // 失敗。
    };

    enum class RoarStage : uint8_t
    {
        None,  // 未設定。
        Telegraph,  // 予兆中。
        Shockwave  // 衝撃波の処理中。
    };

    enum class PositioningMoveResult : uint8_t
    {
        None,  // 未設定。
        Running,  // 実行中。
        Arrived,  // 目標に到着。
        Timeout,  // 時間切れ。
        Stuck,  // 移動が停滞。
        InvalidTarget,  // 目標が無効。
        MaxDistanceReached  // 最大移動距離に到達。
    };

    enum class TripleJumpPhase : uint8_t
    {
        None,  // 未設定。
        InitialTelegraph,  // 初回の予兆中。
        Jumping,  // Jump中。
        InterJumpTransition,  // Jump間の遷移中。
        Completed,  // 完了。
        Aborted  // 中断。
    };

    struct ChargeWindTrailRuntime
    {
        Trail trail;  // 風の軌跡の描画データ。
        bool active = false;  // 軌跡が有効か。
        float age = 0.0f;  // 軌跡の経過時間。
        float lifetime = 0.0f;  // 軌跡の存続時間。
        DirectX::XMFLOAT3 center{};  // 軌跡の中心位置。
        DirectX::XMFLOAT3 backwardVelocity{};  // 軌跡が後方へ流れる速度。
        float width = 0.05f;  // 軌跡の幅。
        float lateralWobbleAmplitude = 0.0f;  // 軌跡の横方向の揺れ幅。
        float verticalWobbleAmplitude = 0.0f;  // 軌跡の上下方向の揺れ幅。
        float wobbleFrequency = 0.0f;  // 軌跡の揺れの周波数。
        float randomPhase = 0.0f;  // 軌跡の揺れに使う位相。
    };

    struct PositioningTargetContext
    {
        bool valid = false;  // 目標が有効か。
        DirectX::XMFLOAT3 targetPosition{};  // 移動目標位置。
        float arrivalTolerance = 0.3f;  // 到着と判定する距離。
        float timeout = 3.0f;  // 移動のタイムアウト時間。
        float maxMoveDistance = 20.0f;  // 最大移動距離。
        float moveSpeed = 6.0f;  // 移動速度。
        float stuckMovementThreshold = 0.1f;  // 停滞判定に使う移動量の閾値。
        float stuckTimeThreshold = 0.5f;  // 停滞と判定する時間。
    };

    struct PositioningTargetRuntime
    {
        DirectX::XMFLOAT3 previousPosition{};  // 前回の位置。
        float elapsed = 0.0f;  // 移動の経過時間。
        float traveledDistance = 0.0f;  // 移動済み距離。
        float remainingDistance = 0.0f;  // 目標までの残り距離。
        float stuckTime = 0.0f;  // 停滞した時間。
        bool movementActive = false;  // 移動が有効か。
    };

public:
    explicit GruxEnemy(const std::string& actorName) :Enemy(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void RenderTrail(ID3D11DeviceContext* immediateContext);

    void DrawImGuiDetails() override;

    void PlayBodyAnimation(const std::string& name, bool loop = true, bool blend = true, float blendTime = 0.3f, bool ignoreRootMotion = false) const;

    // Plays a body animation owned by the phase-two cinematic.
    void PlayPhase2CinematicBodyAnimation(const std::string& name, bool loop,
        bool blend, float blendTime, bool ignoreRootMotion) const;

    // Playerの攻撃種別と命中位置を受け取りダメージを処理する。
    void TakeDamageFromPlayerAttack(int damage, bool isNormalFourthHit, PlayerAttackHitType hitType, const DirectX::XMFLOAT3& hitSourcePosition);

    // ボスのHPバーを表示または非表示にする。
    void SetHpBarVisible(bool visible);

    // HPバーのフェードアウトを開始する。
    void BeginHpBarFadeOut();

    // HPバーのフェード係数を設定する。
    void SetHpBarFadeAlpha(float alpha);
    // 第2段階用HPバーの演出を開始する。
    void BeginPhase2HpBarPresentation(int presentationMaxHp);

    // 第2段階用HPバーの演出を完了する。
    void CompletePhase2HpBarPresentation();

    // 戦闘フェーズに対応するHPバーの表示を設定する。
    void SetHpBarVisualProfileForBattlePhase(bool phase2);

    // 戦闘行動を中断し、攻撃判定や移動などを停止する。
    void StopBattleActions();

    // 死亡時に行動ツリーの実行状態を破棄する。
    void AbortBehaviorTreeForDeath();

    // 死亡演出中のワンショット音声を停止する。
    void StopBossDeathCinematicOneShots();

    // 第1段階の最後の被弾リアクションを開始する。
    void BeginPhase1LastHitReaction(const DirectX::XMFLOAT3& hitSourcePosition);

    // 最後の被弾リアクションを開始する。
    void BeginFinalHitReaction(const std::string& animationName);

    // 最後の被弾リアクションを終了する。
    void EndFinalHitReaction();

    // 戦闘判断と進行中の行動を停止する。
    void PauseBattleAI();

    // フェーズ遷移の演出にアニメーションを渡して戦闘判断を停止する。
    void PauseBattleAIForPhaseTransition();

    // 指定方向に向きを即時変更する。
    void SetDirectionImmediate(const DirectX::XMFLOAT3& direction) const;

    // 戦闘判断を再開する。
    void ResumeBattleAI();

    // 戦闘AIが有効かを返す。
    bool IsBattleAIActive() const { return battleAIActive; }

    // フェーズ遷移のアニメーションを外部が管理するか設定する。
    void SetPhase2CinematicAnimationOwnedExternally(bool owned)
    {
        phase2CinematicAnimationOwnedExternally = owned;
    }

    // フェーズ遷移演出用のIdleアニメーションを即時再生する。
    void ForcePhase2CinematicIdleImmediate() const;

    // HPを維持して戦闘位置と実行状態をリセットする。
    void ResetForBattleContinue(const Transform& battleStartTransform);

    // 行動ツリーのノードと実行状態を初期化する。
    void ResetBehaviorTreeForBattleRestart();

    // 攻撃や位置調整の戦闘時状態を初期化する。
    void ResetCombatRuntimeForBattleRestart();

    // 戦闘状態とHPを再戦用にリセットする。
    void ResetForBattleRestart(const Transform& battleStartTransform);

    // 戦闘用の現在HPと最大HPを設定する。
    void SetBattleHp(int currentHp, int maximumHp);

    // フェーズ遷移演出の進行度を設定する。
    void SetPhaseTransformProgress(float progress);

    // フェーズごとのボスのスケールを設定する。
    void SetPhaseScaleRange(float phase1Scale, float phase2Scale);

    // 第2段階への変化で使うマスクの上下オフセットを設定する。
    void SetPhase2MaskOffsets(float minYOffset, float maxYOffset);

    // 第2段階への変化で使うマスクの境界の柔らかさを設定する。
    void SetPhase2MaskSoftness(float softness);

    // 第1段階のボスのスケールを返す。
    float GetPhase1BossScale() const { return phase1BossScale; }

    // 第2段階のボスのスケールを返す。
    float GetPhase2BossScale() const { return phase2BossScale; }

    // フェーズ変化マスクの下端オフセットを返す。
    float GetPhase2MaskMinYOffset() const { return phase2MaskMinYOffset; }

    // フェーズ変化マスクの上端オフセットを返す。
    float GetPhase2MaskMaxYOffset() const { return phase2MaskMaxYOffset; }

    // フェーズ変化マスクの境界の柔らかさを返す。
    float GetPhase2MaskSoftness() const { return phase2MaskSoftness; }

    // 最大HPを返す。
    int GetMaxHp() const { return maxHp; }

    // フェーズ遷移時の遅延HP表示を進める。
    void AdvanceDelayedHpBarForPhaseTransition();

    // 遅延HP表示が目標値に到達したかを返す。
    bool IsDelayedHpSettled() const;

    // フェーズ遷移時に被弾表示を解除する。
    void ClearDamageFlash() const;
    void ClearDamageVisualsForPhaseTransition() const;

    // Rush中のHP表示を開始する。
    void BeginRushHpDisplay();

    // Rush中のHP表示を終了する。
    void EndRushHpDisplay();

    // 命中位置と向きに応じた被弾エフェクトを生成する。
    void SpawnHitEffect(DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 playerPos) const;

    // Rushの命中位置にリングエフェクトを生成する。
    void SpawnRushHitRing(const DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal,
        DirectX::XMFLOAT3 playerPos, bool hasHitPosition, bool hasHitNormal) const;

    // Notify Stateの開始に応じて攻撃判定や移動などを開始する。
    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    // 終了したNotify Stateに対応する処理を解除する。
    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    // 単発のAnimation Notifyイベントを処理する。
    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

    // アニメーションエディタのプレビューイベントを処理する。
    void OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event) override;

    // アニメーションエディタの状態プレビューを描画する。
    void DrawAnimationEditorPreviewState(const AnimationNotifyState& state) override;

    // アニメーション切り替え時に攻撃判定や予兆を整理する。
    void OnAnimationChanged() override;

    // 攻撃シーケンスの開始に必要な共通状態を初期化する。
    void StartAttack();

    // 攻撃用のヒットボックスを無効にする。
    void DisableAttackHitBoxes();

    // 現在の攻撃シーケンスIDを返す。
    uint64_t GetCurrentAttackSequenceId() const { return currentAttackSequenceId; }


    // 攻撃開始履歴を更新し、必要なログとカメラ補正を行う。
    void OnSelectedActionStartedSuccessfully();

    // Jump攻撃時のカメラ補正を要求する。
    void RequestJumpAttackCameraAssist() const;

    // 攻撃後の旧Intent用位置調整フラグを設定する。
    void OnSelectedAttackCompletedSuccessfully();

    // 旧Intentの位置調整が選択中かを返す。
    bool IsCombatRepositionActive() const;


    // 位置調整時の移動速度に応じてアニメーションを更新する。
    void UpdatePositioningAnimation(float actualSpeed, float deltaTime);

    // 位置調整用の移動アニメーションを終了する。
    void EndPositioningAnimation();

    // 選択中の行動にクールダウンを設定する。Charge強制時は設定しない。
    void StartSelectedActionCooldown();

    // 攻撃種別と段階に対応するアニメーションを再生する。
    bool PlayAttackStage(BossAttackType type, int stage);

    // 指定名の攻撃アニメーションを再生する。
    bool PlayAttackAnimationByName(const std::string& animationName);

    // 連続攻撃の次段階に向けて攻撃状態を更新する。
    void BeginAdditionalAttackStage();

    // Jump専用のMotionWarp上書きを解除する。
    void ClearJumpAttackMotionWarpOverride();

    // Dashを攻撃候補に含められるか判定する。
    bool CanPlanDashAttack() const;

    // Dash開始に向けた目標位置と実行状態を準備する。
    bool PrepareDashAttackSetupTarget();

    // 準備後にDashを開始できるか判定する。
    bool CanExecuteDashAttack() const;

    // Dashの予兆と攻撃シーケンスを開始する。
    bool StartDashAttackTelegraph();

    // Dashの予兆、移動、段階間遷移などを進める。
    DashBTResult UpdateDashAttackBT(float deltaTime);

    // Dashの移動、攻撃判定、予兆などを終了時または中断時に整理する。
    void CleanupDashAttackBT();

    // Dashの実行を終了してRecoveryへ移る。
    void FinishDashAttackBT();

    // DashのBT実行状態が有効かを返す。
    bool IsDashAttackBTActive() const { return dashBTPhase != DashBTPhase::None; }

    // Dashの実行段階を向き合わせに設定する。
    void SetDashBTFacing() { dashBTPhase = DashBTPhase::Facing; }

    // Dash移動に使う位置や方向を準備する。
    bool PrepareDashAttackMovementSnapshot();

    // 指定されたボス攻撃予兆のSEを再生する。
    void PlayBossTelegraphSE(const char* fileName) const;

    // Dash予兆の描画用コンポーネントを用意する。
    void EnsureDashTelegraphVisual();

    // Dash予兆の表示を開始する。
    void BeginDashTelegraphVisual();

    // 保持したDash予兆の表示情報を適用する。
    void ApplyDashTelegraphVisualSnapshot();

    // Dash予兆の表示状態を更新する。
    void UpdateDashTelegraphVisual(float deltaTime);

    // Dash予兆の扇形の点滅を開始する。
    void BeginDashTelegraphFanFlash();

    // Dash予兆の線の短縮を開始する。
    void BeginDashTelegraphLineShrink();

    // Dash予兆の線の短縮を更新する。
    void UpdateDashTelegraphLineShrink();

    // Dash予兆のフェードアウトを開始する。
    void BeginDashTelegraphFadeOut();

    // Dash予兆を非表示にする。
    void HideDashTelegraphVisual();

    // Dash予兆のデバッグ表示を更新する。
    void UpdateDashTelegraphVisualDebug();

    // Dash予兆のデバッグ情報をワールド上に描画する。
    void DrawDashTelegraphDebugWorld() const;

    // Dashの移動を開始する。
    bool BeginDashAttackMovement();

    // Dashの移動を更新し、移動結果を返す。
    bool UpdateDashAttackMovement(float deltaTime, bool keepLockedDirection = false);

    // Dashの移動を停止する。
    void StopDashAttackMovement();


    // Chargeを攻撃候補に含められるか判定する。
    bool CanPlanChargeAttack() const;

    // Chargeの目標位置と実行状態を準備する。
    bool PrepareChargeAttackSetupTarget();

    // Charge開始前の向き合わせを進め、結果を返す。
    ChargeBTStepResult UpdateChargeAttackFacingBT(float deltaTime);

    // 準備後にChargeを実行できるか判定する。
    bool CanExecuteChargeAttack() const;

    // Chargeの攻撃状態と予兆を開始する。
    void StartChargeAttackBT();

    // 単発Chargeの移動を開始する。
    bool BeginSingleChargeBT();

    // 単発Chargeの進行を更新する。
    ChargeBTStepResult UpdateSingleChargeBT(float deltaTime);

    // 単発または複数回のChargeを更新する。
    ChargeBTStepResult UpdateChargeAttackBT(float deltaTime);

    // 複数回Chargeの進行と段階間遷移を更新する。
    ChargeBTStepResult UpdateTripleChargeBT(float deltaTime);

    // 第2段階のCharge設定を使うか判定する。
    bool IsPhase2ChargeActive();

    // 現在のフェーズに対応するCharge速度を返す。
    float GetActiveChargeSpeed() const;

    // 現在のChargeのPlayer判定半径倍率を返す。
    float GetActiveChargePlayerCastRadiusScale() const;

    // 現在のChargeのダメージを返す。
    int GetActiveChargeDamage() const;

    // 複数回Chargeの現在の突進を開始する。
    bool BeginTripleChargeLeg();

    // 複数回Chargeの段間遷移を開始する。
    bool BeginTripleChargeTransition();

    // Chargeの予兆表示を開始する。
    bool BeginChargeTelegraphVisual();

    // 進行度に応じてChargeの予兆表示を更新する。
    void UpdateChargeTelegraphVisual(float lengthProgress);

    // Charge開始方向の障害物までの距離を調べる。
    bool EvaluateChargeStartClearance(const DirectX::XMFLOAT3& startPosition,

        const DirectX::XMFLOAT3& direction, float& outClearance, bool& outHit) const;

    // Chargeの側方の障害物までの距離と接触情報を調べる。
    bool EvaluateChargeSideClearance(const DirectX::XMFLOAT3& startPosition,
        const DirectX::XMFLOAT3& direction, float& outClearance,
        DirectX::XMFLOAT3& outNormal, DirectX::XMFLOAT3& outHitPosition,
        std::string& outActorName, std::string& outComponentName, bool& outHit) const;

    // Chargeの予兆を非表示にする。
    void HideChargeTelegraphVisual();

    // 複数回Chargeの予兆情報をデバッグ表示する。
    void DrawTripleChargeTelegraphDebug() const;

    // Chargeの終了結果に応じてスタンやRecoveryへの移行を進める。
    ChargeBTStepResult ResolveChargeResultBT(float deltaTime);

    // Charge後のスタン処理を開始する。
    bool BeginChargeStunBT();

    // Charge後のスタンアニメーションと経過時間を更新する。
    ChargeBTStepResult UpdateChargeStunBT(float deltaTime);

    // Chargeの攻撃区間を終了し、移動や攻撃判定を停止する。
    void FinishChargeAttackBT();

    // Chargeの失敗を記録して終了処理へ進める。
    void FailChargeAttackBT();

    // ChargeのBT実行状態が有効かを返す。
    bool IsChargeAttackBTActive() const { return chargeBT.phase != ChargeBTPhase::None; }

    // Chargeの現在の実行段階を返す。
    ChargeBTPhase GetChargeBTPhase() const { return chargeBT.phase; }

    // 旧StateMachineのスタン状態かを返す。
    bool IsStunned() const;

    // Chargeの実行段階をRecoveryに進める。
    void BeginChargeRecoveryBT() { chargeBT.phase = ChargeBTPhase::Recovery; }

    // ChargeのRecoveryタイマー完了後の段階へ進める。
    void MarkChargeRecoveryTimerFinishedBT() { chargeBT.phase = ChargeBTPhase::RecoveryPostconditions; }

    // Charge後のRecovery完了条件を処理する。
    ChargeBTStepResult FinishChargeRecoveryBT();

    // 対象PlayerのRush開始を受け、必要なCharge中断を処理する。
    void NotifyPlayerRushStarted(Player* player);

    // 対象PlayerのRush終了を受けて戦闘再開処理を行う。
    void NotifyPlayerRushEnded(Player* player);

    // Chargeを中断すべき状態か判定する。
    bool ShouldAbortChargeAttackBT();

    // Chargeの中断または終了に伴う状態と演出を整理する。
    void CleanupChargeAttackBT();

    // Chargeの実行状態と調整項目をデバッグ表示する。
    void DrawChargeAttackBTDebug();

    // Chargeの移動を開始する。
    bool BeginChargeAttackMovement();

    // Playerに向けてChargeの移動方向を固定する。
    bool LockChargeDirectionToPlayer();

    // Chargeの移動と衝突を更新し、終了理由を返す。
    ChargeAttackEndReason UpdateChargeAttackMovement(float deltaTime, bool allowTripleWallTurn = false);

    // Charge時の風の軌跡を開始する。
    void StartChargeWindTrails();

    // Chargeの風演出に設定値を適用する。
    void ApplyMainChargeWindSettings();

    // Charge時の風の軌跡を更新する。
    void UpdateChargeWindTrails(float deltaTime);

    // Charge時の風の軌跡を停止する。
    void StopChargeWindTrails();

    // Charge時の風の軌跡を生成する。
    void SpawnChargeWindTrail(struct ChargeWindTrailRuntime& streak) const;

    // Chargeの移動と関連する状態を停止する。
    void StopChargeAttackMovement();

    // Charge予備動作の終了時刻を返す。
    float GetChargeWindupEndTime() const { return chargeWindupEndTime; }

    // スタンの継続時間を返す。
    float GetStunDuration() const { return stunDuration; }

    // HPがゼロ以下かを返す。
    bool IsDead() const { return hp <= 0; }

    // 死亡演出のアニメーション管理を外部に委ねるか設定する。
    void SetCinematicDeathAnimationOwnedExternally(const bool owned)
    {
        cinematicDeathAnimationOwnedExternally = owned;
    }

    // 死亡演出のアニメーションを外部が管理しているか返す。
    bool IsCinematicDeathAnimationOwnedExternally() const
    {
        return cinematicDeathAnimationOwnedExternally;
    }

    // 死亡時のパーティクル開始要求を取得して解除する。
    bool ConsumeBeginHuskParticleRequest()
    {
        const bool requested = beginHuskParticleRequest;
        beginHuskParticleRequest = false;
        return requested;
    }

    // 死亡時のパーティクル開始を要求しSEを再生する。
    void RequestBeginHuskParticle()
    {
        beginHuskParticleRequest = true;
        CoreAudio::PlayOneShot("./Data/Sound/SE/boss_dead_dissolve.wav");
    }

    // ボスのスケルタルメッシュを返す。
    const std::shared_ptr<SkeletalMeshComponent>& GetSkeletalMeshComponent() const
    {
        return skeletalMeshComponent;  // ボス本体のスケルタルメッシュ。
    }

    // 死亡時のアニメーション名を返す。
    const std::string& GetDeathAnimationName() const { return deathAnimationName; }

    // スタン段階と経過時間のデバッグ表示値を設定する。
    void SetStunDebug(const char* phase, float elapsed)
    {
        stunPhaseDebug = phase ? phase : "None";
        stunElapsedDebug = elapsed;
    }

    // Chargeの段階のデバッグ表示値を設定する。
    void SetChargePhaseDebug(const char* phase) { chargePhaseDebug = phase ? phase : "None"; }

    // Charge予備動作のアニメーション時刻を記録する。
    void SetChargeWindupAnimationTimeDebug(float time) { chargeWindupAnimationTimeDebug = time; }

    // 選択中の攻撃に対応するRecovery時間を返す。
    float GetRecoveryDurationForCurrentAttack() const;

    // 次のRecoveryに使う時間と設定元を記録する。
    void SetNextRecoveryDuration(float duration, const char* source);

    // 次のRecovery時間を取得し、一時設定を解除する。
    float ConsumeNextRecoveryDuration();

    // Charge後のRecoveryに渡す結果を保持する。
    void SetPendingChargeRecoveryResult(ChargeAttackEndReason reason);

    // 保持したCharge結果を取得して解除する。
    ChargeAttackEndReason ConsumePendingChargeRecoveryResult();

    // 旧Intent用の位置調整要求を記録する。
    void RequestCombatRepositionIntent();

    // Recoveryの経過時間と設定時間を記録する。
    void UpdateRecoveryDebug(float elapsed, float duration)
    {
        recoveryElapsedDebug = elapsed;
        currentRecoveryDurationDebug = duration;
    }

    // ChargeのPlayer命中後に使うRecovery時間を返す。
    float GetChargePlayerHitRecoveryDuration() const
    {
        return chargePlayerHitRecoveryDuration;
    }

    // Chargeのジャスト回避後に使うRecovery時間を返す。
    float GetChargeJustDodgeRecoveryDuration() const
    {
        return chargeJustDodgeRecoveryDuration;
    }

    // スタン後に使うRecovery時間を返す。
    float GetPostStunRecoveryDuration() const
    {
        return postStunRecoveryDuration;
    }

    // 選択中の攻撃に対応するダメージを返す。
    int GetDamageForCurrentAttack() const;

    // 連続攻撃の遷移受付期間中かを返す。
    bool IsTransitionWindowActive() const { return transitionWindow; }

    // 現在の攻撃シーケンス中の命中数を返す。
    int GetCurrentAttackHitCount() const { return currentAttackHitCount; }

    // 現在の攻撃がジャスト回避されたかを返す。
    bool WasCurrentAttackSequenceJustDodged() const { return !justDodgedActors.empty(); }

    // 有効な攻撃判定の経過時間をデバッグ用に返す。
    float GetActiveHitBoxElapsedForDebug() const;

    // 現在の攻撃名をデバッグ用に返す。
    std::string GetCurrentAttackNameForDebug() const;



    // 現在のPlayerを新しい攻撃・防御行動の対象にできるか判定する。
    bool CanPlanAttackAgainstCurrentPlayer() const;

    // Roarを防御行動として選択できるか判定する。
    bool CanPlanRoar() const;

    // 背後のPlayerから退避する条件を満たすか判定する。
    bool CanPlanRetreat() const;

    // Roarの計画条件を評価する。
    bool EvaluateRoarPlanForDebug() const;

    // Roarを選択できない理由をデバッグ表示用に返す。

    // Roarの計画・実行を許可する共通条件を判定する。
    bool IsRoarExecutionAllowed() const;

    // RoarのBT実行状態が有効かを返す。
    bool IsRoarBTActive() const { return roarBT.stage != RoarStage::None; }

    // Roarの予兆と実行状態を開始する。
    bool BeginRoarBT();

    // Roarの予兆から衝撃波までの進行を更新する。
    int UpdateRoarBT(float dt);

    // Roarの衝撃波による判定を適用する。
    void ApplyRoarShockwave();

    // 現在のフェーズに対応するRoarの攻撃半径を返す。
    float GetRoarAttackRadius() const;

    // Roar予兆用のメッシュを用意する。
    void EnsureRoarTelegraphMeshes();

    // Roar予兆を表示する。
    void ShowRoarTelegraph();

    // Roar予兆の位置と大きさを更新する。
    void UpdateRoarTelegraphTransform();

    // Roar予兆の進行度を更新する。
    void UpdateRoarTelegraphProgress();

    // Roar予兆を非表示にする。
    void HideRoarTelegraph();

    // Roar予兆中の浮遊物を用意する。
    void EnsureRoarFloatingDebris();

    // Roar予兆中の浮遊物を初期化する。
    void InitializeRoarFloatingDebris();

    // Roar予兆中の浮遊物を更新する。
    void UpdateRoarFloatingDebris();

    // Roar予兆中の浮遊物を非表示にする。
    void HideRoarFloatingDebris();

    // Roar衝撃時の破片を用意する。
    void EnsureRoarImpactDebris();

    // Roar衝撃時の破片を生成する。
    void SpawnRoarImpactDebris();

    // Roar衝撃時の破片の移動や寿命を更新する。
    void UpdateRoarImpactDebris(float scaledDeltaTime);

    // Roar衝撃時の破片を非表示にする。
    void HideRoarImpactDebris();

    // Roar衝撃時の地面エフェクトを生成する。
    void SpawnRoarGroundBurst();

    // Roarの実行状態と演出を終了時または中断時に整理する。
    void CleanupRoarBT(const char* status);

    // Roarなどのクールダウンを更新し、Roarの中断を処理する。
    void TickRoarLifecycle(float dt);

    // Roarと防御行動の状態・調整項目を表示する。
    void DrawRoarBTDebug();

    // Roar演出時の描画用足元オフセットを返す。
    float GetRoarRenderFootOffset() const;

    // PlayerとのXZ距離、方向、角度と相対領域を計算する。
    BossTargetContext BuildTargetContext() const;

    // 攻撃抽選用の近距離・正面条件を判定する。
    bool IsNearFrontForAttackSelection() const;

    // 防御行動用の背後角度条件を判定する。
    bool IsDefensiveBackForBehavior(const BossTargetContext& context) const;

    // 近距離正面でのFastCombo基本選択確率を返す。
    float GetNearFrontFastComboProbability() const { return nearFrontFastComboProbability; }

    // 攻撃Plan名に対応する連続攻撃ペナルティを返す。
    float GetAttackRepeatPenaltyForPlan(const std::string& planName) const;

    // FastComboの指定段階における対象情報を更新する。
    void RefreshFastComboTargetContext(int stage);

    // Notifyの期間に対応するFastComboの踏み込みを開始する。
    void BeginFastComboStepIn(int stage, float notifyStartTime, float notifyEndTime);

    // FastComboの踏み込みを更新する。
    bool UpdateFastComboStepIn(float deltaTime);

    // FastComboの踏み込み状態を解除する。
    void ClearFastComboStepIn();

    // 指定されたNotifyがFastComboのMotionWarp対象かを判定する。
    bool IsFastComboMotionWarpNotify(const AnimationNotifyState& state) const;

    using AttackSetupTargetContext = PositioningTargetContext;
    using AttackSetupMoveResult = PositioningMoveResult;

    // Jumpを攻撃候補に含められるか判定する。
    bool CanPlanJumpAttack() const;

    // Jump開始前の目標位置を探索して準備する。
    bool PrepareJumpAttackSetupTarget();

    // 指定距離や障害物条件を満たす攻撃準備位置を探索する。
    bool FindAttackSetupTarget(float minDistance, float maxDistance, float angleStep, float clampTolerance, float minimumMoveDistance, DirectX::XMFLOAT3& outTarget, float& outDistance, int& outCandidateCount, float boundaryMarginOverride = -1.0f) const;

    // 攻撃準備位置の目標が有効かを返す。
    bool HasAttackSetupTarget() const { return attackSetupTarget.valid; }

    // 攻撃準備位置の目標と移動状態を解除する。
    void ClearAttackSetupTarget();

    // 攻撃準備位置への移動を進め、到着や失敗の結果を返す。
    AttackSetupMoveResult UpdateAttackSetupMovement(float deltaTime);

    // 攻撃準備位置への移動を停止する。
    void StopAttackSetupMovement();

    // 退避先を探索し、移動目標として設定する。
    bool PrepareRetreatTarget();

    // 退避先への移動を進め、到着や失敗の結果を返す。
    PositioningMoveResult UpdateRetreatMovement(float deltaTime);

    // 退避移動を終了し、結果に応じた後処理を行う。
    void FinishRetreatMovement(bool arrived);

    // 退避移動の結果を記録する。
    void RecordRetreatMoveResult(PositioningMoveResult result);

    // 戦闘行動を計画できるか判定する。
    bool CanPlanAnyCombatDecision();

    // 位置調整を計画できるか判定する。
    bool CanPlanReposition();

    // 戦闘判断の推論を開始する。
    void BeginCombatDecisionInference();

    // 攻撃の完了を記録する。
    void RecordBehaviorAttackCompleted();

    // 戦闘判断の診断情報を初期化する。
    void BeginCombatDecisionDebugInference();

    // 戦闘判断で選択されたノードを記録する。
    void CompleteCombatDecisionDebugInference(const char* selectedNode);

    // 防御行動の判定結果を記録する。
    void RecordCombatDecisionDebugDefensive(bool result);

    // 戦闘開始時の位置調整を計画できるかを返す。
    bool CanPlanInitialReposition() const { return initialRepositionPending && !initialRepositionFallbackIdlePending; }

    // 戦闘開始時の位置調整が未完了かを返す。
    bool IsInitialRepositionPending() const { return initialRepositionPending; }

    // 初期位置調整の失敗後のIdleが必要かを返す。
    bool IsInitialRepositionFallbackIdlePending() const { return initialRepositionFallbackIdlePending; }

    // 初期位置調整に失敗した場合の待機へ移る。
    void BeginInitialRepositionFallback();

    // 初期位置調整失敗後の待機を完了する。
    void CompleteInitialRepositionFallbackIdle();

    // ボス部屋の壁を考慮して移動先と経路を評価する。
    bool ResolveBossRoomPositioningMove(const DirectX::XMFLOAT3& startPosition,
        const DirectX::XMFLOAT3& desiredTarget, DirectX::XMFLOAT3& outResolvedTarget,
        bool& outPathBlocked) const;

    // 位置調整先を探索し、移動目標として設定する。
    bool PrepareRepositionTarget();

    // 位置調整先への移動を進め、結果を返す。
    PositioningMoveResult UpdateRepositionMovement(float deltaTime);

    // 位置調整移動を終了し、結果に応じた後処理を行う。
    void FinishRepositionMovement(bool arrived);

    // 位置調整先への到着後の待機を完了する。
    void CompleteRepositionArrivalWait();

    // 位置調整後の待機時間を返す。
    float GetRepositionArrivalWaitDuration() const { return repositionArrivalWaitDuration; }

    // Jumpの予兆アニメーションを開始する。
    bool StartJumpAttackTelegraph();

    // Jumpの予兆を更新し、アニメーション終了を判定する。
    bool UpdateJumpAttackTelegraph(float deltaTime) const;

    // Jump攻撃のアニメーションを開始し、成功時に攻撃開始を記録する。
    bool StartJumpAttackExecution();

    // Jump予兆用のメッシュを用意する。
    void EnsureJumpTelegraphMesh();

    // 現在のJumpの着地点に予兆を表示する。
    void ShowJumpTelegraphForCurrentJump();

    // Jump着地情報をワールド上にデバッグ描画する。
    void DrawJumpLandingDebugWorld() const;

    // Jump予兆を非表示にする。
    void HideJumpTelegraph();

    // Jump着地時の予兆の発光を開始する。
    void BeginJumpTelegraphImpactFlash();

    // Jump着地時の予兆の発光を更新する。
    void UpdateJumpTelegraphImpactFlash(float deltaTime);

    // Jump予兆の進行度を更新する。
    void UpdateJumpTelegraphProgress();


    // 第2段階の複数回Jumpを使うか判定する。
    bool IsTripleJumpPhase2() const;

    // 複数回Jumpの実行状態を初期化する。
    void BeginTripleJumpRuntime();

    // 現在のJump回の目標を固定して攻撃を開始する。
    bool StartTripleJumpJump();

    // 次のJumpまでの遷移を開始する。
    void BeginTripleJumpTransition();

    // 次のJumpまでの待機と向き合わせを進める。
    bool UpdateTripleJumpTransition(float deltaTime);

    // 複数回Jumpを完了状態にする。
    void CompleteTripleJumpRuntime();

    // 複数回Jumpの実行を中断する。
    void AbortTripleJumpRuntime();

    // 複数回Jumpの実行中かを返す。
    bool IsTripleJumpActive() const { return tripleJumpActive; }

    // 現在のJump回のインデックスを返す。
    int GetTripleJumpIndex() const { return tripleJumpIndex; }

    // 複数回Jumpの実行段階を返す。
    TripleJumpPhase GetTripleJumpPhase() const { return tripleJumpPhase; }

    // 複数回Jumpの実行状態をデバッグ表示する。
    void DrawTripleJumpDebug();

    // FastCombo用の対象情報を返す。
    const BossTargetContext& GetFastComboTargetContext() const { return fastComboTargetContext; }

    // 攻撃準備の理由に対応する予兆時間を返す。
    float GetAttackReadyDuration() const
    {
        return attackReadyReason == AttackReadyReason::AfterTurn
            ? sideAttackReadyDuration
            : frontAttackReadyDuration;
    }

    // 攻撃準備のSEを再生する。
    bool PlayAttackReadySE();

    // 指定アニメーション用の武器予兆を開始する。
    void StartWeaponTelegraph(const std::string& animationName);

    // 攻撃前の向き合わせ関連の値をリセットする。
    void ClearPendingAttackFacing();

    // AIによる移動入力と固定速度を解除する。
    void StopAIMovement() const;

    // 指定方向へ回転する。
    bool RotateTowardsPlayer(const DirectX::XMFLOAT3& direction,
        float degreesPerSecond, float deltaTime) const;

    // 攻撃前の向き合わせ完了角度を返す。
    float GetTurnCompleteAngle() const { return turnCompleteAngle; }

    // Chargeの向き合わせの制限時間を返す。
    float GetTurnTimeout() const { return turnTimeout; }

    // 向き合わせの回転速度を返す。
    float GetTurnSpeed() const { return turnSpeed; }

    // カメラが注視するコンポーネントを返す。
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }

    // ボス名表示の演出を開始する。
    void StartGruxNamePerform(float duration, float start = 0.0f, float end = 1.0f);

    // FastComboを攻撃候補に含められるか判定する。
    bool CanPlanFastCombo() const;

    // 接近失敗後のFastCombo再試行待ちを開始する。
    void StartFastComboApproachRetryCooldown();

    // FastCombo接近の再試行待ち中かを返す。
    bool IsFastComboApproachRetryCooldownActive() const;

    // 現在の距離、向き、クールダウンでFastComboを実行できるか判定する。
    bool CanExecuteFastCombo() const;

    // PlayerがFastComboの攻撃距離内かを判定する。
    bool IsFastComboInRange() const;

    // PlayerがFastComboの実行可能角度内か判定する。
    bool IsPlayerInFastComboFacingRange(const BossTargetContext& context) const;

    // PlayerがFastComboの向き合わせ完了角度内か判定する。
    bool IsPlayerInFastComboFaceCompleteRange(const BossTargetContext& context) const;

    // FastCombo前の接近移動を開始する。
    void BeginFastComboApproach();

    // Playerへの接近と向き合わせを進め、攻撃距離への到達を返す。
    bool UpdateFastComboApproach(float deltaTime);

    // BTが選んだ攻撃種別と対応する行動種別を設定する。
    void SetSelectedAttackForBehaviorTree(BossAttackType type)
    {
        selectedAttackType = type;
        switch (type)
        {
        case BossAttackType::FastCombo:
            selectedActionType = BossActionType::FastCombo;
            break;
        case BossAttackType::JumpAttack:
            selectedActionType = BossActionType::JumpAttack;
            break;
        case BossAttackType::DashAttack:
            selectedActionType = BossActionType::DashAttack;
            break;
        case BossAttackType::ChargeAttack:
            selectedActionType = BossActionType::ChargeAttack;
            break;
        default:
            break;
        }
    }

    enum class BehaviorAttackResult
    {
        None,        // 結果なし。
        Success,     // 攻撃が成功。
        JustDodged   // Playerがジャスト回避
    };

    // 攻撃結果を保持する。
    void SetBehaviorAttackResult(BehaviorAttackResult result)
    {
        behaviorAttackResult = result;
    }

    // 攻撃後の待機アニメーションを再生する。
    void BeginRecovery() const;

    // 選択中の攻撃に対応するRecovery時間を返す。
    float GetSelectedAttackRecoveryDuration() const { return GetRecoveryDurationForCurrentAttack(); }

    // FastComboの準備待機時間を返す。
    float GetBehaviorPrepareDuration() const { return fastComboPrepareDuration; }

    // FastCombo接近の制限時間を返す。
    float GetFastComboApproachMaxDuration() const { return fastComboApproachMaxDuration; }

    // 現在のBT Idleに使う待機時間を返す。
    float GetBehaviorIdleDuration() const
    {
        return initialRepositionFallbackIdlePending ? initialRepositionFallbackIdleDuration : behaviorIdleDuration;
    }

    // 実行ノードがなければ推論し、現在のアクションを更新する。
    void UpdateBehaviorTree(float deltaTime);


    // Chargeの強制選択設定が有効かを返す。
    bool IsForceBehaviorTreeChargeEnabled() const
    {
        return forceBehaviorTreeCharge;
    }

    struct CloseCombatSettings
    {
        float minRange = 0.0f;  // 近接攻撃の最小距離。
        float executeMaxRange = 6.0f;  // 近接攻撃を実行できる最大距離。
        float planMaxRange = 10.0f;  // 近接攻撃を計画できる最大距離。
        float facingLimitDegrees = 35.0f;  // 実行時の向きの許容角度。
        float faceCompleteAngleDegrees = 7.0f;  // 向き合わせ完了と判定する角度。
    };

    // FastCombo段間の向き合わせ完了角度を返す。
    float GetInterStageFaceCompleteAngle() const { return interStageFaceCompleteAngle; }

    // FastComboの次段へ進むための角度上限を返す。
    float GetInterStageMaxFacingAngle() const { return interStageMaxFacingAngle; }

    // FastCombo段間の向き合わせ開始までの待機時間を返す。
    float GetInterStageFaceDelay() const { return interStageFaceDelay; }
    enum class FastComboRuntimeState
    {
        Attack,  // 攻撃中。
        InterStageDelay,  // 段間の待機中。
        InterStageFacing  // 段間の向き合わせ中。
    };

    // FastComboの実行状態を設定する。
    void SetFastComboRuntimeState(FastComboRuntimeState state) { fastComboRuntimeState = state; }

    // FastComboの実行段階を設定する。
    void SetFastComboRuntimeStage(int stage) { fastComboRuntimeStage = stage; }

    // Charge実行ノードの更新回数を記録する。
    void RecordExecuteChargeAttackRunDebug();
private:
    // ビヘイビアツリーを作成する
    void CreateBossBehaviorTree();

    // HPバーを作成する
    void CreateHpUI();

    void PlayBodyAnimationInternal(const std::string& name, bool loop, bool blend, float blendTime, bool ignoreRootMotion) const;

    // 武器の攻撃予兆表示を更新する。
    void UpdateWeaponTelegraphRimLight(float deltaTime);

    // 武器の攻撃予兆表示を解除する。
    void ResetWeaponTelegraphRimLight();

    bool weaponEmissiveEnabled = true;  // 武器の発光を有効にするか。
    DirectX::XMFLOAT3 weaponEmissiveColor = { 0.0f, 0.0f, 0.0f };  // 武器の発光色。
    float weaponEmissiveIntensity = 7.0f;  // 武器の発光強度。
    DirectX::XMFLOAT3 weaponTelegraphRimColor = { 1.0f, 0.35f, 0.0f };  // 武器予兆の縁の色。

    bool weaponTelegraphActive = false;  // 武器予兆が有効か。
    bool weaponTelegraphFadingOut = false;  // 武器予兆がフェードアウト中か。
    std::string weaponTelegraphAnimationName;  // 武器予兆に対応するアニメーション名。
    float weaponTelegraphLeadDuration = 0.0f;  // 武器予兆の先行表示時間。
    float weaponTelegraphShowEmissiveEndTime = 0.0f;  // 武器予兆の発光表示終了時刻。
    float weaponTelegraphMaximumPower = 0.0f;  // 武器予兆の最大発光強度。
    float weaponTelegraphElapsed = 0.0f;  // 武器予兆の経過時間。
    float weaponTelegraphCurrentPower = 0.0f;  // 武器予兆の現在の発光強度。
    float weaponTelegraphFadeOutElapsed = 0.0f;  // 武器予兆のフェードアウト経過時間。
    float weaponTelegraphFadeOutStartPower = 0.0f;  // 武器予兆のフェードアウト開始時の発光強度。
    float weaponTelegraphFadeOutDuration = 0.15f;  // 武器予兆のフェードアウト時間。

    // 行動ツリーの実行ノードとアクション状態をリセットする。
    void ResetBehaviorTreeRuntime();

    // 通常攻撃の4段目による被弾リアクションを開始する。
    void BeginFourthHitReaction(const DirectX::XMFLOAT3& hitSourcePosition);

    // 通常攻撃の4段目による被弾リアクションを終了する。
    void EndFourthHitReaction();

    // 4段目の被弾リアクションの診断情報を初期化する。
    void ResetFourthHitReactionDebug();
    bool finalHitReactionActive = false;  // 最後の被弾リアクション中か。
    bool finalHitReactionHeld = false;  // 最後の被弾リアクションを保持中か。
    bool fourthHitReactionActive = false;  // 4段目の被弾リアクション中か。
    float fourthHitReactionRemaining = 0.0f;  // 4段目の被弾リアクションの残り時間。
    float fourthHitReactionChance = 0.3f;  // 4段目の被弾リアクションを発生させる確率。
    float fourthHitReactionDuration = 1.25f;  // 4段目の被弾リアクション時間。
    float fourthHitReactionLastRoll = -1.0f;  // 4段目の被弾リアクションに使った直近の乱数値。
    bool fourthHitReactionHasLastRoll = false;  // 4段目の被弾リアクションの抽選結果があるか。
    bool fourthHitReactionLastWon = false;  // 直近の4段目の被弾リアクション抽選が成功したか。
    float fourthHitReactionLastDirectionDot = 0.0f;  // 4段目の被弾方向の判定に使った内積。
    std::string fourthHitReactionDirection = "None";  // 4段目の被弾方向の表示文字列。
    std::string fourthHitReactionAnimation = "None";  // 4段目の被弾リアクションのアニメーション名。

    // PlayerとのXZ平面上の距離を返す。
    float GetDistanceToPlayer() const;

    // 距離を近距離・中距離・遠距離に分類する。
    BossDistanceRegion GetDistanceRegion(float distance) const;

    // 選択中の旧Intentに対応するデータを返す。
    const BossIntentData* GetActiveIntentData() const;

    // 旧Intentの推奨距離に対する現在距離を判定する。
    BossIntentRangeStatus GetIntentRangeStatus(
        const BossIntentData& intentData, float distance) const;

    // 旧Intentに対応する攻撃の距離範囲を取得する。
    bool GetIntentAttackValidRange(float& outMinDistance, float& outMaxDistance) const;
    struct RepositionTargetEvaluation
    {
        DirectX::XMFLOAT3 desiredTarget{};  // 境界補正前の目標位置。
        DirectX::XMFLOAT3 clampedTarget{};  // 境界補正後の目標位置。
        bool wasClamped = false;  // 目標位置に境界補正を適用したか。
        float clampDistance = 0.0f;  // 目標位置の境界補正距離。
        bool bossRoomPathBlocked = false;  // ボス部屋の経路が遮られているか。
    };

    // 位置調整候補をボス部屋の範囲に収めて評価する。
    void EvaluateClampedPositioningTarget(const DirectX::XMFLOAT3& startPosition,
        const DirectX::XMFLOAT3& desiredTarget, RepositionTargetEvaluation& outEvaluation, float boundaryMarginOverride = -1.0f) const;

    // 指定行動が左右への位置調整かを判定する。
    bool IsRepositionAction(BossActionType actionType) const;

    // 連続攻撃ペナルティの対象となる行動か判定する。
    bool IsRepeatPenaltyCombatAttack(BossActionType actionType) const;

    // 攻撃行動が指定された旧Intentに対応するか判定する。
    bool IsAttackActionForIntent(BossActionType actionType, BossIntentType intentType) const;

    // 直近2回の開始履歴に基づく候補の重み倍率を返す。
    float GetRecentAttackPenalty(BossActionType actionType) const;

    // 旧Intentに対応する攻撃のペナルティ倍率を求める。
    float GetIntentRecentAttackPenalty(BossIntentType intentType) const;

    // 各行動の残りクールダウンを経過時間分減らす。
    void UpdateActionCooldowns(float deltaTime);

    // 旧Intent用の距離・向き・履歴に応じた重みを計算する。
    void UpdateIntentEffectiveWeights(const BossTargetContext& context);

    // 距離領域に対応する旧Intentの重みを返す。
    float GetIntentWeightForDistance(const BossIntentData& data, BossDistanceRegion region) const;


    // 攻撃シーケンスのジャスト回避記録を消去する。
    void ResetJustDodgeRecords(const char* reason);

    // 指定アクターが現在の攻撃をジャスト回避したか判定する。
    bool HasJustDodgedAttack(const Actor* actor) const;

    // Playerのジャスト回避成功処理を試みる。
    bool TryStartJustDodgeSuccess(Player* player);

    // 攻撃の危険範囲を初期化する。
    void ResetDangerArea();

    // 危険範囲の初期設定を記録する。
    void CaptureInitialDangerObbSettings();

    // 選択中の危険範囲Notify Stateを返す。
    AnimationNotifyState* GetSelectedDangerNotifyState();

    // 選択中の危険範囲Notify Stateを読み取り専用で返す。
    const AnimationNotifyState* GetSelectedDangerNotifyState() const;

    // デバッグ用にPlayerと危険範囲の重なりを判定する。

    // 有効なNotifyから危険範囲を更新する。
    void RefreshActiveDangerAreaFromNotify();

    // 危険範囲をワールド上にデバッグ描画する。

    // Jump用のMotionWarp上書き情報を準備する。
    void PrepareJumpAttackMotionWarpOverride();

    // JumpのMotionWarp終了時の着地診断情報を記録する。
    void CaptureJumpLandingMotionWarpEndSnapshot();

    // Jumpの地面衝撃時の着地診断情報を記録する。
    void CaptureJumpLandingGroundImpactSnapshot();

    // Jump着地時の壁衝突診断情報を更新する。
    void UpdateJumpLandingDebugWallSnapshot();

    // 有効なNotify Stateから攻撃判定を更新する。
    void RefreshActiveHitBoxesFromNotifyStates();

    // 距離や向きなどのAI情報をワールド上に描画する。

    // 位置調整のデバッグ情報をワールド上に描画する。

    // 退避候補などのデバッグ情報をワールド上に描画する。

    // 指定された位置調整目標への移動を更新する。
    PositioningMoveResult UpdatePositioningTargetMovement(PositioningTargetContext& target,
        PositioningTargetRuntime& runtime, float deltaTime);

    // 位置調整の目標と実行状態を解除する。
    void ClearPositioningTarget(PositioningTargetContext& target, PositioningTargetRuntime& runtime);

    // 回転に関する診断情報をワールド上に描画する。

    // 地面への衝撃エフェクトを生成する。
    void SpawnGroundImpactEffect() const;

    // 壁への衝撃エフェクトを生成する。
    void SpawnWallImpactEffect(const DirectX::XMFLOAT3& impactPosition, const DirectX::XMFLOAT3& wallNormal) const;

    // 武器の衝突エフェクトを生成する。
    void SpawnWeaponClashEffect() const;

    // 左足の地面を擦るエフェクトを生成する。
    void SpawnLeftFootScrapeEffect()const;

    // 右足の地面を擦るエフェクトを生成する。
    void SpawnRightFootScrapeEffect()const;

    // 地面へのダウンエフェクトを生成する。
    void SpawnGroundDownEffect()const;

    // 左武器を落とすエフェクトを生成する。
    void SpawnDropLeftWeaponEffect()const;

    // 右武器を落とすエフェクトを生成する。
    void SpawnDropRightWeaponEffect()const;

private:
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;  // ボス本体のスケルタルメッシュ。
    std::shared_ptr<RotationComponent> rotationComponent;  // ボスの回転を制御するコンポーネント。
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;  // ボスの移動を制御するコンポーネント。

    std::shared_ptr<CapsuleComponent> enemyCapsuleComponent;  // ボス本体の衝突カプセル。
    std::shared_ptr<CapsuleComponent> leftWeaponCollisionComp;  // 左武器の衝突カプセル。
    std::shared_ptr<CapsuleComponent> rightWeaponCollisionComp;  // 右武器の衝突カプセル。
    std::string leftWeapon = "leftWeapon";  // 左武器の識別名。
    std::string rightWeapon = "rightWeapon";  // 右武器の識別名。
    std::string bothWeapon = "bothWeapon";  // 両武器の識別名。

    std::shared_ptr<SceneComponent> weaponLeftRootComponent;  // 左武器の根元位置のコンポーネント。
    std::shared_ptr<SceneComponent> weaponLeftMiddleComponent;  // 左武器の中央位置のコンポーネント。
    std::shared_ptr<SceneComponent> weaponLeftTipComponent;  // 左武器の先端位置のコンポーネント。

    std::shared_ptr<SceneComponent> weaponRightRootComponent;  // 右武器の根元位置のコンポーネント。
    std::shared_ptr<SceneComponent> weaponRightMiddleComponent;  // 右武器の中央位置のコンポーネント。
    std::shared_ptr<SceneComponent> weaponRightTipComponent;  // 右武器の先端位置のコンポーネント。

    std::shared_ptr<SceneComponent> neckComponent;  // ベルト位置のコンポーネント。

    std::shared_ptr<SceneComponent> leftFootComponent;  // 左足位置のコンポーネント。
    std::shared_ptr<SceneComponent> rightFootComponent;  // 右足位置のコンポーネント。

    Trail leftWeaponTrail;  // 左武器の軌跡の描画データ。
    Trail rightWeaponTrail;  // 右武器の軌跡の描画データ。
    bool showLeftWeaponTrail = false;  // 左武器の軌跡を表示するか。
    bool showRightWeaponTrail = false;  // 右武器の軌跡を表示するか。


    static constexpr size_t chargeWindTrailCount = 10;  // Chargeの風の軌跡の数。
    std::array<ChargeWindTrailRuntime, chargeWindTrailCount> chargeWindTrails{};  // Chargeの風の軌跡の実行状態。
    bool chargeWindTrailsActive = false;  // Chargeの風の軌跡が有効か。
    bool chargeWindTrailEnabled = true;  // Chargeの風の軌跡の表示を有効にするか。
    int chargeWindTrailActiveCount = static_cast<int>(chargeWindTrailCount);
    DirectX::XMFLOAT3 chargeWindTrailColor{ 0.70f, 0.84f, 0.92f };  // Chargeの風の軌跡の色。
    float chargeWindTrailEmissiveStrength = 1.5f;  // Chargeの風の軌跡の発光強度。
    float chargeWindTrailLifetimeMin = 0.35f;  // Chargeの風の軌跡の存続時間の下限。
    float chargeWindTrailLifetimeMax = 0.5f;  // Chargeの風の軌跡の存続時間の上限。
    float chargeWindTrailSpeedMin = 18.0f;  // Chargeの風の軌跡の速度の下限。
    float chargeWindTrailSpeedMax = 24.0f;  // Chargeの風の軌跡の速度の上限。
    float chargeWindTrailWidthMin = 0.15f;  // Chargeの風の軌跡の幅の下限。
    float chargeWindTrailWidthMax = 0.3f;  // Chargeの風の軌跡の幅の上限。
    float chargeWindTrailHeadWidthScale = 1.0f;  // Chargeの風の軌跡の先頭の幅倍率。
    float chargeWindTrailTailWidthScale = 0.1f;  // Chargeの風の軌跡の末尾の幅倍率。
    float chargeWindTrailLateralSpawnRange = 1.4f;  // Chargeの風の軌跡の横方向の生成範囲。
    float chargeWindTrailHeightMin = 0.35f;  // Chargeの風の軌跡の生成高さの下限。
    float chargeWindTrailHeightMax = 2.2f;  // Chargeの風の軌跡の生成高さの上限。
    float chargeWindTrailForwardMin = -0.2f;  // Chargeの風の軌跡の前方生成位置の下限。
    float chargeWindTrailForwardMax = 0.5f;  // Chargeの風の軌跡の前方生成位置の上限。
    float chargeWindTrailLateralWobbleMin = 0.025f;  // Chargeの風の軌跡の横方向の揺れ幅の下限。
    float chargeWindTrailLateralWobbleMax = 0.07f;  // Chargeの風の軌跡の横方向の揺れ幅の上限。
    float chargeWindTrailVerticalWobbleMin = 0.015f;  // Chargeの風の軌跡の上下方向の揺れ幅の下限。
    float chargeWindTrailVerticalWobbleMax = 0.05f;  // Chargeの風の軌跡の上下方向の揺れ幅の上限。
    float chargeWindTrailWobbleFrequencyMin = 3.0f;  // Chargeの風の軌跡の揺れの周波数の下限。
    float chargeWindTrailWobbleFrequencyMax = 6.0f;  // Chargeの風の軌跡の揺れの周波数の上限。

    bool mainChargeWindEnabled = true;  // Chargeの風メッシュを有効にするか。
    DirectX::XMFLOAT3 mainChargeWindLocalPosition{ 0.0f, 1.05f, 0.8f };  // Chargeの風メッシュのローカル位置。
    DirectX::XMFLOAT3 mainChargeWindLocalRotation{ 0.0f,90.0f,90.0f };  // Chargeの風メッシュのローカル回転。
    DirectX::XMFLOAT3 mainChargeWindLocalScale{ 1.0f, 1.0f, 1.0f };  // Chargeの風メッシュのローカルスケール。
    DirectX::XMFLOAT2 mainChargeWindScrollSpeed{ 0.0f, 0.0f };  // Chargeの風メッシュのUVスクロール速度。
    int mainChargeWindMaskFlowMode = 1; // 0: Linear, 1: Radial
    float mainChargeWindRadialSpeed = 1.45f;  // Chargeの風メッシュの放射方向の移動速度。
    float mainChargeWindRadialTiling = 3.0f;  // Chargeの風メッシュの放射方向の繰り返し数。
    DirectX::XMFLOAT3 mainChargeWindColor{ 0.70f, 0.85f, 1.00f };  // Chargeの風メッシュの色。
    float mainChargeWindEmissionPower = 1.5f;  // Chargeの風メッシュの発光強度。
    float mainChargeWindOpacity = 0.5f;  // Chargeの風メッシュの不透明度。
    DirectX::XMFLOAT3 bossTrailColor{ 0.0f, 0.13f, 0.002f };  // ボスの武器軌跡の色。
    float bossTrailEmissiveStrength = 7.0f;  // ボスの武器軌跡の発光強度。
    float bossTrailLifetime = 0.8f;  // ボスの武器軌跡の存続時間。

    std::vector<std::shared_ptr<CoreStandaloneAudioSource>> bossDeathCinematicOneShots;  // 死亡演出で再生したワンショット音声。
    std::shared_ptr<ParticleComponent> hitSwordEffectComponent; // Existing normal hit effect
    std::shared_ptr<ParticleComponent> rushHitRingEffectComponent; // Rush World Ring effect
    std::shared_ptr<ParticleComponent> rushHitSparkEffectComponent; // Rush Spark effect
    float rushHitEffectSurfaceOffset = 0.08f;  // Rush命中エフェクトを表面から離す距離。
    std::shared_ptr<ParticleComponent> groundDustEffectComponent;  // 地面の砂煙エフェクト。
    std::shared_ptr<ParticleComponent> roarGroundBurstEffectComponent;  // Roar時の地面噴出エフェクト。
    std::shared_ptr<ParticleComponent> wallImpactDustEffectComponent;  // 壁衝突時の砂煙エフェクト。
    std::shared_ptr<ParticleComponent> wallImpactFlashEffectComponent;  // 壁衝突時の閃光エフェクト。
    std::shared_ptr<ParticleComponent> metalSparkEffectComponent;  // 金属の火花エフェクト。
    std::shared_ptr<ParticleComponent> footScrapeEffectComponent;  // 足の擦過エフェクト。
    std::shared_ptr<MainChargeWindMeshComponent> mainChargeWindMeshComponent;  // Chargeの風を描画するメッシュ。
    std::shared_ptr<StaticMeshComponent> tripleChargeTelegraphMeshComponent;  // 複数回Chargeの予兆メッシュ。
    std::shared_ptr<StaticMeshComponent> jumpTelegraphMeshComponent;  // Jump予兆の描画メッシュ。
    std::shared_ptr<StaticMeshComponent> jumpTelegraphInnerMeshComponent;  // Jump予兆の内側のメッシュ。
    std::shared_ptr<StaticMeshComponent> roarTelegraphOuterMeshComponent;  // Roar予兆の外側メッシュ。
    std::shared_ptr<StaticMeshComponent> roarTelegraphFillMeshComponent;  // Roar予兆の塗りつぶしメッシュ。
    std::shared_ptr<StaticMeshComponent> dashTelegraphLineMeshComponent;  // Dash予兆の線メッシュ。
    std::shared_ptr<StaticMeshComponent> dashTelegraphFanMeshComponent;  // Dash予兆の扇形メッシュ。

    std::shared_ptr<UIGaugeFillComponent> hpDelayedFillUiComponent;  // 遅延HPゲージのUI。
    std::shared_ptr<UIGaugeFillComponent> hpCurrentFillUiComponent;  // 現在HPゲージのUI。
    std::shared_ptr<UIImageComponent> hpNameUiComponent;  // HPバーの名前表示UI。
    std::shared_ptr<UIImageComponent> hpBackgroundUiComponent;  // HPバーの背景UI。
    std::shared_ptr<UIImageComponent> hpFrameUiComponent;  // HPバーの枠UI。
    std::shared_ptr<Sprite> hpPhase1BackgroundTexture;  // 第1段階のHPバー背景テクスチャ。
    std::shared_ptr<Sprite> hpPhase1FillTexture;  // 第1段階のHPゲージテクスチャ。
    std::shared_ptr<Sprite> hpPhase1FrameTexture;  // 第1段階のHPバー枠テクスチャ。
    std::shared_ptr<Sprite> hpPhase2BackgroundTexture;  // 第2段階のHPバー背景テクスチャ。
    std::shared_ptr<Sprite> hpPhase2FillTexture;  // 第2段階のHPゲージテクスチャ。
    std::shared_ptr<Sprite> hpPhase2FrameTexture;  // 第2段階のHPバー枠テクスチャ。
    enum class HpBarVisualProfile : uint8_t
    {
        Phase1,  // 第1段階用。
        Phase2  // 第2段階用。
    };
    enum class Phase2HpBarPresentationState : uint8_t
    {
        Hidden,  // 非表示。
        RevealingFrame,  // 枠を表示中。
        WaitingForFill,  // ゲージ表示待ち。
        Filling,  // ゲージを表示中。
        Complete,  // 完了。
    };
    HpBarVisualProfile hpBarVisualProfile = HpBarVisualProfile::Phase1;  // 現在のHPバーの表示プロファイル。
    Phase2HpBarPresentationState phase2HpBarPresentationState = Phase2HpBarPresentationState::Hidden;  // 第2段階のHPバープレビューの状態。
    DirectX::XMFLOAT2 phase2HpBarPresentationPosition{ 650.0f, 115.0f };  // 第2段階のHPバープレビューの位置。
    DirectX::XMFLOAT2 phase2HpBarPresentationFrameSize{ 895.65f, 17.15f };  // 第2段階のHPバープレビューの枠の大きさ。
    float phase2HpBarPresentationFrameRevealDuration = 2.0f;  // 第2段階のHPバーの枠を表示する時間。
    float phase2HpBarPresentationFillDelay = 0.3f;  // 第2段階のHPゲージ表示開始までの待機時間。
    float phase2HpBarPresentationFillDuration = 2.3f;  // 第2段階のHPゲージを表示する時間。
    float phase2HpBarPresentationElapsed = 0.0f;  // 第2段階のHPバー演出の経過時間。
    float phase2HpBarPresentationFrameProgress = 0.0f;  // 第2段階のHPバーの枠の表示進行度。
    float phase2HpBarPresentationFillProgress = 0.0f;  // 第2段階のHPゲージの表示進行度。
    float phase2HpBarPresentationHp = 0.0f;  // 第2段階のHPバー演出で表示するHP。
    int phase2HpBarPresentationMaxHp = 1;  // 第2段階のHPバー演出で使用する最大HP。
    bool phase2HpBarPresentationRestoreVisible = false;  // HPバープレビュー解除時に表示を復元するか。

    // HPバーに指定されたフェーズの表示設定を適用する。
    void ApplyHpBarVisualProfile(HpBarVisualProfile profile);

    void ResetPhase2HpBarPresentation();

    // 第2段階HPバーのプレビューを更新する。
    void UpdatePhase2HpBarPresentation();

    // 第2段階HPバーのプレビュー配置を更新する。
    void UpdatePhase2HpBarPresentationLayout();

    // 第2段階HPバーのプレビューが有効かを返す。
    bool IsPhase2HpBarPresentationActive() const;
    std::vector<std::shared_ptr<UICoreComponent>> hpBarUiComponents;  // HPバーを構成するUIコンポーネント。
    struct HpBarFadeEntry
    {
        std::shared_ptr<UIImageComponent> component;  // フェード対象の画像コンポーネント。
        CoreColor color;  // フェード対象の元の色。
    };
    std::vector<HpBarFadeEntry> hpBarFadeEntries;  // HPバーのフェード対象と元の表示情報。
    bool rushHpDisplayActive = false;  // Rush中のHP表示が有効か。
    bool rushCombatHoldActive = false;  // Rush中の戦闘処理の保持が有効か。
    bool interruptedChargeRecoveryPending = false;  // 中断されたChargeのRecoveryが保留中か。
    bool interruptedChargeRecoveryActive = false;  // 中断されたChargeのRecovery中か。
    float interruptedChargeRecoveryTimer = 0.0f;  // 中断されたChargeのRecovery経過時間。
    float interruptedChargeRecoveryDuration = 0.0f;  // 中断されたChargeのRecovery時間。
    bool useRushDelayedHpFollowSpeed = false;  // Rush用の遅延HP追従速度を使用するか。
    float delayedHp = 0.0f;  // 遅延表示中のHP値。
    float delayedHpDelayTimer = 0.0f;  // HPの遅延表示が追従するまでの経過時間。
    float delayedHpDelayDuration = 0.25f;  // HPの遅延表示が追従するまでの待機時間。
    float delayedHpFollowSpeed = 8.95f;  // 遅延HPゲージの追従速度。
    float delayedHpRushFollowSpeed = 12.0f;  // Rush中の遅延HPゲージの追従速度。
    CoreColor bossHpCurrentColor{ 0.55f, 0.08f, 0.06f, 1.0f };  // 現在HPゲージの色。
    CoreColor bossHpDelayedColor{ 0.95f, 0.72f, 0.38f, 1.0f };  // 遅延HPゲージの色。

    bool rightHitBox = false;  // 右武器のヒット判定が有効か。
    bool leftHitBox = false;  // 左武器のヒット判定が有効か。
    bool isDangerWindow = false;  // 危険判定期間が有効か。

    std::unordered_set<Actor*> hitActors;  // 現在の攻撃ですでにヒットしたアクター。
    std::unordered_set<const Actor*> justDodgedActors;  // 現在の攻撃をジャスト回避したアクター。
    uint64_t currentAttackSequenceId = 0;  // 現在の攻撃シーケンスの識別番号。
    int currentAttackHitCount = 0;  // 現在の攻撃シーケンスのヒット数。

    bool battleAIActive = true;  // 戦闘AIの更新を許可する状態。

    std::optional<BossIntentType> activeIntent = std::nullopt;  // 旧Intent処理で保持する現在の目的。
    BossIntentStep activeIntentStep = BossIntentStep::Selecting;  // 現在のIntentの処理段階。
    bool intentPositioningAttempted = false;  // Intentの位置調整を試行済みか。
    bool intentPositioningCompleted = false;  // Intentの位置調整が完了したか。
    float dashAttackValidMinDistance = 6.0f;  // Dash攻撃の有効な最小距離。
    float dashAttackValidMaxDistance = 14.0f;  // Dash攻撃の有効な最大距離。
    std::string intentRepositionReason = "None";  // Intentの位置調整理由の表示文字列。
    std::string intentLifecycleState = "None";  // Intentの処理状態の表示文字列。
    std::string intentLifecycleTrace = "None";  // Intentの処理履歴の表示文字列。
    std::string intentLifecycleReason = "None";  // Intentの処理理由の表示文字列。

    static constexpr int intentCount = 4;  // Intentデータの要素数。
    std::array<BossIntentData, intentCount> combatIntentData =  // Intentの設定データ。
    { {
        { BossIntentType::CloseCombat, 60.0f, 30.0f, 25.0f, 4.0f, 5.5f, 0.25f },
        { BossIntentType::DashAttackPlan, 25.0f, 35.0f, 45.0f, 8.0f, 10.0f, 0.25f },
        { BossIntentType::JumpAttackPlan, 15.0f, 35.0f, 30.0f, 7.0f, 9.0f, 0.25f },
        { BossIntentType::CombatReposition, 3.0f, 12.0f, 10.0f, 0.0f, 100.0f, 0.0f },
    } };
    std::array<float, intentCount> combatIntentEffectiveWeights{};  // 旧Intent用に計算した候補ごとの重み。
    std::optional<BossIntentType> lastSelectedIntent = std::nullopt;  // 直近に選択されたIntent。

    BossActionType selectedActionType = BossActionType::AttackLA;  // 現在選択されている行動種別。
    std::optional<BossActionType> lastStartedCombatAttack = std::nullopt;  // 最後に開始した連続ペナルティ対象の攻撃。
    std::optional<BossActionType> secondLastStartedCombatAttack = std::nullopt;  // その一つ前に開始した対象攻撃。
    static constexpr float initialRepeatAttackPenalty = 0.25f;  // 連続攻撃ペナルティの初期倍率。
    float repeatAttackPenalty = initialRepeatAttackPenalty;  // 直近2回と同じ攻撃候補に適用する倍率。
    BossAttackType selectedAttackType = BossAttackType::PrimaryAttackLA;  // 現在選択されている攻撃種別。
    std::optional<BossPositioningData> selectedPositioningData = std::nullopt;  // 選択中の位置調整データ。

    bool hasLastAttack = false;  // 直近の攻撃履歴があるか。

    static constexpr int actionCount = 10;  // 行動データの要素数。
    std::array<BossActionData, actionCount> combatActionData =  // 行動の設定データ。
    {
        {
        { BossActionType::AttackLA, BossAttackType::PrimaryAttackLA ,20.0f,1.0f},
        { BossActionType::AttackRA, BossAttackType::PrimaryAttackRA,20.0f,1.0f},
        { BossActionType::FastCombo, BossAttackType::FastCombo ,100.0f,2.0f},
        { BossActionType::JumpAttack,BossAttackType::JumpAttack ,30.0f,3.0f},
        { BossActionType::DashAttack,BossAttackType::DashAttack,40.0f,4.0f},
        { BossActionType::ChargeAttack,BossAttackType::ChargeAttack,40.0f,4.0f},
        { BossActionType::Approach, std::nullopt,40.0f,0.5f},
        { BossActionType::Retreat, std::nullopt,40.0f,2.5f},
        { BossActionType::RepositionLeft, std::nullopt,50.0f,1.0f},
        { BossActionType::RepositionRight, std::nullopt,50.0f,1.0f},
    }
    };

    std::array<float, actionCount> combatActionCooldownRemaining{};  // 行動データと同じ順序で保持する残りクールダウン。

    float defensiveTooCloseDistance = 4.5f;  // 背後のPlayerに対する防御行動の距離上限。
    float roarRadiusPhase1 = 7.0f;  // 第1段階のRoarの判定半径。
    float roarRadiusPhase2 = 10.0f;  // 第2段階のRoarの判定半径。
    int roarDamage = 10;  // Roarが与えるダメージ量。
    float roarTelegraphOuterAlpha = 0.85f;  // Roar予兆の外側の不透明度。
    DirectX::XMFLOAT3 roarTelegraphPhase1OuterTint{ 0.05f, 0.72f, 1.0f };  // 第1段階のRoar予兆の外側の色。
    DirectX::XMFLOAT3 roarTelegraphPhase1FillTintMin{ 0.03f, 0.50f, 0.95f };  // 第1段階のRoar予兆の塗りつぶしの開始色。
    DirectX::XMFLOAT3 roarTelegraphPhase1FillTintMax{ 0.12f, 0.90f, 1.0f };  // 第1段階のRoar予兆の塗りつぶしの終了色。
    DirectX::XMFLOAT3 roarTelegraphPhase2OuterTint{ 1.0f, 0.08f, 0.03f };  // 第2段階のRoar予兆の外側の色。
    DirectX::XMFLOAT3 roarTelegraphPhase2FillTintMin{ 1.0f, 0.08f, 0.03f };  // 第2段階のRoar予兆の塗りつぶしの開始色。
    DirectX::XMFLOAT3 roarTelegraphPhase2FillTintMax{ 1.0f, 0.15f, 0.03f };  // 第2段階のRoar予兆の塗りつぶしの終了色。
    float roarTelegraphFillGlowMin = 1.0f;  // Roar予兆の塗りつぶしの発光強度の下限。
    float roarTelegraphFillGlowMax = 10.0f;  // Roar予兆の塗りつぶしの発光強度の上限。
    float roarTelegraphFillAlphaMin = 0.20f;  // Roar予兆の塗りつぶしの不透明度の下限。
    float roarTelegraphFillAlphaMax = 0.8f;  // Roar予兆の塗りつぶしの不透明度の上限。
    DirectX::XMFLOAT3 roarTelegraphFillTintCurrent{ 0.03f, 0.50f, 0.95f };  // Roar予兆の塗りつぶしの現在の色。
    float roarTelegraphFillGlowCurrent = 1.0f;  // Roar予兆の塗りつぶしの現在の発光強度。
    float roarTelegraphFillAlphaCurrent = 0.20f;  // Roar予兆の塗りつぶしの現在の不透明度。


    float roarTelegraphProgress = 0.0f;  // Roar予兆の進行度。
    float roarTelegraphVisualProgress = 0.0f;  // Roar予兆の表示上の進行度。
    float roarTelegraphOuterYOffset = 0.07f;  // Roar予兆の外側の高さオフセット。
    float roarTelegraphFillYOffset = 0.07f;  // Roar予兆の塗りつぶしの高さオフセット。
    struct RoarFloatingDebris
    {
        std::shared_ptr<SkeletalMeshComponent> meshComponent;  // 浮遊する破片のメッシュ。
        bool active = false;  // 破片が有効か。
        DirectX::XMFLOAT3 normalizedDirection{};  // 浮遊する破片の正規化された方向。
        float normalizedRingDistance = 0.0f;  // 浮遊する破片のリング内の距離比率。
        float spawnThreshold = 1.0f;  // 浮遊する破片の出現閾値。
        float baseScale = 1.0f;  // 浮遊する破片の基準スケール。
        float maxFloatHeight = 0.0f;  // 浮遊する破片の最大高さ。
        DirectX::XMFLOAT3 baseRotation{};  // 浮遊する破片の基準回転。
        DirectX::XMFLOAT3 rotationAmount{};  // 浮遊する破片の回転量。
    };
    static constexpr size_t RoarFloatingDebrisCount = 15;  // Roarの浮遊する破片の数。
    std::array<RoarFloatingDebris, RoarFloatingDebrisCount> roarFloatingDebris{};  // Roarの浮遊する破片の実行状態。
    bool roarFloatingDebrisEnabled = true;  // Roarの浮遊する破片を有効にするか。
    float roarFloatingDebrisInnerRatio = 0.30f;  // Roarの浮遊する破片の内側の配置比率。
    float roarFloatingDebrisOuterRatio = 0.90f;  // Roarの浮遊する破片の外側の配置比率。
    float roarFloatingDebrisMinHeight = 0.20f;  // Roarの浮遊する破片の最小高さ。
    float roarFloatingDebrisMaxHeight = 2.50f;  // Roarの浮遊する破片の最大高さ。
    float roarFloatingDebrisScaleMin = 0.75f;  // Roarの浮遊する破片のスケールの下限。
    float roarFloatingDebrisScaleMax = 2.5f;  // Roarの浮遊する破片のスケールの上限。
    struct RoarImpactDebris
    {
        std::shared_ptr<SkeletalMeshComponent> meshComponent;  // 衝撃時の破片のメッシュ。
        bool active = false;  // 破片が有効か。
        bool isBlock = false;  // 破片がブロック状か。
        DirectX::XMFLOAT3 position{};  // 破片の位置。
        DirectX::XMFLOAT3 velocity{};  // 破片の速度。
        DirectX::XMFLOAT3 rotation{};  // 破片の回転。
        DirectX::XMFLOAT3 angularVelocity{};  // 破片の角速度。
        float lifetime = 0.0f;  // 破片の存続時間。
        float elapsedTime = 0.0f;  // 破片の経過時間。
        float scale = 1.0f;  // 破片のスケール。
    };
    static constexpr size_t RoarImpactDebrisPoolCount = 16;  // Roarの衝撃時の破片プールの数。
    std::array<RoarImpactDebris, RoarImpactDebrisPoolCount> roarImpactDebris{};  // Roarの衝撃時の破片の実行状態。
    bool roarImpactDebrisEnabled = true;  // Roarの衝撃時の破片を有効にするか。
    float roarImpactDebrisSpawnHeight = 0.10f;  // Roarの衝撃時の破片の生成高さ。
    float roarImpactDebrisGravity = 9.8f;  // Roarの衝撃時の破片に適用する重力。
    float roarImpactDebrisAngleJitterDegrees = 8.0f;  // Roarの衝撃時の破片の角度の揺らぎ。
    float roarImpactDebrisRadiusJitter = 0.04f;  // Roarの衝撃時の破片の半径方向の揺らぎ。
    float roarImpactDebrisSmallHorizontalSpeedMin = 1.5f;  // Roarの小さい破片の水平速度の下限。
    float roarImpactDebrisSmallHorizontalSpeedMax = 3.2f;  // Roarの小さい破片の水平速度の上限。
    float roarImpactDebrisSmallVerticalSpeedMin = 4.2f;  // Roarの小さい破片の垂直速度の下限。
    float roarImpactDebrisSmallVerticalSpeedMax = 7.6f;  // Roarの小さい破片の垂直速度の上限。
    float roarImpactDebrisSmallLifetimeMin = 1.0f;  // Roarの小さい破片の存続時間の下限。
    float roarImpactDebrisSmallLifetimeMax = 1.5f;  // Roarの小さい破片の存続時間の上限。
    float roarImpactDebrisBlockHorizontalSpeedMin = 0.8f;  // Roarのブロック状の破片の水平速度の下限。
    float roarImpactDebrisBlockHorizontalSpeedMax = 3.5f;  // Roarのブロック状の破片の水平速度の上限。
    float roarImpactDebrisBlockVerticalSpeedMin = 4.5f;  // Roarのブロック状の破片の垂直速度の下限。
    float roarImpactDebrisBlockVerticalSpeedMax = 7.8f;  // Roarのブロック状の破片の垂直速度の上限。
    float roarImpactDebrisBlockLifetimeMin = 1.1f;  // Roarのブロック状の破片の存続時間の下限。
    float roarImpactDebrisBlockLifetimeMax = 1.6f;  // Roarのブロック状の破片の存続時間の上限。
    float roarImpactDebrisSmallScaleMin = 2.5f;  // Roarの小さい破片のスケールの下限。
    float roarImpactDebrisSmallScaleMax = 3.5f;  // Roarの小さい破片のスケールの上限。
    float roarImpactDebrisBlockScaleMin = 1.8f;  // Roarのブロック状の破片のスケールの下限。
    float roarImpactDebrisBlockScaleMax = 2.2f;  // Roarのブロック状の破片のスケールの上限。
    bool roarGroundBurstEnabled = true;  // Roarの地面噴出エフェクトを有効にするか。
    float roarGroundBurstInnerRingRatio = 0.38f;  // Roarの地面噴出の内側の配置比率。
    float roarGroundBurstOuterRingRatio = 0.75f;  // Roarの地面噴出の外側の配置比率。
    float roarGroundBurstAngleJitterDegrees = 8.0f;  // Roarの地面噴出の角度の揺らぎ。
    float roarGroundBurstRadiusJitter = 0.04f;  // Roarの地面噴出の半径方向の揺らぎ。
    float roarGroundBurstYOffset = 0.0f;  // Roarの地面噴出の高さオフセット。
    int roarGroundBurstLastSpawnCount = 0;  // 直近のRoarで生成した地面噴出エフェクト数。
    DirectX::XMFLOAT3 roarTelegraphOuterWorldPosition{};  // Roar予兆の外側のワールド位置。
    DirectX::XMFLOAT3 roarTelegraphFillWorldPosition{};  // Roar予兆の塗りつぶしのワールド位置。
    float roarHeightTolerance = 2.0f;  // Roarの判定に使う高さの許容差。
    float roarLevelStartFootOffset = -0.30f;  // Roar開始時の足元の高さオフセット。
    float roarLevelStartFootOffsetEndTime = 0.30f;  // Roar開始時の足元オフセットの終了時刻。
    float roarPreStampedeStartTime = 3.4f;  // Roarに使用するアニメーションの開始時刻。
    float roarPreStampedeEndTime = 4.8f;  // Roarに使用するアニメーションの終了時刻。
    float roarCooldownDuration = 30.0f;  // Roarを再選択するまでの待ち時間。
    float roarCooldownRemaining = 0.0f;  // Roarの残りクールダウン。
    float retreatDistanceMin = 5.0f;  // 退避先とプレイヤーとの距離の下限。
    float retreatDistanceMax = 12.0f;  // 退避先とプレイヤーとの距離の上限。
    float retreatMinimumMoveDistance = 5.0f;  // 退避候補に必要な最小移動距離。
    float retreatMoveSpeed = 6.0f;  // 退避時の移動速度。
    float retreatRetryCooldownDuration = 0.5f;  // 退避の再試行までの待ち時間。
    float retreatRetryCooldownRemaining = 0.0f;  // 退避の再試行までの残り時間。
    enum class RetreatCandidateRejectReason
    {
        None,  // 未設定。
        Clamp,  // 境界補正による除外。
        DistanceIncrease,  // 対象との距離増加条件による除外。
        MinimumMoveDistance,  // 最小移動距離による除外。
        CapsuleCast,  // カプセル判定による除外。
        BossRoomProbe,  // ボス部屋の経路判定による除外。
        Other  // その他の理由による除外。
    };
    struct RetreatCandidateDebug
    {
        DirectX::XMFLOAT3 position{};  // 退避候補の位置。
        RetreatCandidateRejectReason rejectReason = RetreatCandidateRejectReason::None;  // 候補を除外した理由。
        bool pathBlocked = false;  // 候補までの経路が遮られたか。
        bool accepted = false;  // 候補が採用可能と判定されたか。
    };
    struct RetreatRuntime
    {
        DirectX::XMFLOAT3 playerSnapshot{};  // 評価時に記録したプレイヤー位置。
        DirectX::XMFLOAT3 gruxSnapshot{};  // 評価時に記録したボス位置。
        float startingPlayerDistance = 0.0f;  // 評価開始時のプレイヤーとの距離。
        float plannedPlayerDistance = 0.0f;  // 移動予定先とプレイヤーとの距離。
        float plannedMoveDistance = 0.0f;  // 予定移動距離。
        int candidateCount = 0;  // 評価した候補数。
        int clampRejectCount = 0;  // 境界への補正により除外した候補数。
        int distanceIncreaseRejectCount = 0;  // プレイヤーとの距離が増加せず除外した候補数。
        int minimumMoveRejectCount = 0;  // 最小移動距離を満たさず除外した候補数。
        int capsuleCastRejectCount = 0;  // カプセル判定により除外した候補数。
        int bossRoomProbeRejectCount = 0;  // ボス部屋の経路判定により除外した候補数。
        bool lastCapsuleCastHit = false;  // 直近のカプセル判定がヒットしたか。
        bool lastCapsuleCastWorldStatic = false;  // 直近のカプセル判定が静的な地形にヒットしたか。
        bool lastCapsuleCastWorldProps = false;  // 直近のカプセル判定が配置物にヒットしたか。
        bool lastCapsuleCastConvex = false;  // 直近のカプセル判定がConvexにヒットしたか。
        DirectX::XMFLOAT3 lastCapsuleHitPosition{};  // 直近のカプセル判定のヒット位置。
        float lastCapsuleHitDistance = 0.0f;  // 直近のカプセル判定のヒット距離。
        DirectX::XMFLOAT3 capsulePoint1{};  // カプセル判定の一方の端点。
        DirectX::XMFLOAT3 capsulePoint2{};  // カプセル判定のもう一方の端点。
        float capsuleRadius = 0.0f;  // カプセルの半径。
        float capsuleSegmentLength = 0.0f;  // カプセルの両端点間の長さ。
        DirectX::XMFLOAT3 collisionComponentPosition{};  // 衝突コンポーネントの位置。
        DirectX::XMFLOAT3 collisionComponentScale{};  // 衝突コンポーネントのスケール。
        float collisionConfiguredRadius = 0.0f;  // 衝突判定に設定された半径。
        float collisionConfiguredHeight = 0.0f;  // 衝突判定に設定された高さ。
        float collisionOffsetY = 0.0f;  // 衝突判定の高さオフセット。
        float collisionPhysXHalfHeight = 0.0f;  // PhysX衝突形状の半高さ。
        DirectX::XMFLOAT3 collisionPhysXPoint1{};  // PhysX衝突形状の一方の端点。
        DirectX::XMFLOAT3 collisionPhysXPoint2{};  // PhysX衝突形状のもう一方の端点。
        float collisionPhysXMinY = 0.0f;  // PhysX衝突形状のY座標の下限。
        float collisionPhysXMaxY = 0.0f;  // PhysX衝突形状のY座標の上限。
        float retreatCastMinY = 0.0f;  // 退避判定形状のY座標の下限。
        float retreatCastMaxY = 0.0f;  // 退避判定形状のY座標の上限。
        bool retreatSweepUsesSphere = false;  // 退避経路の判定に球を使用したか。
        bool retreatSweepInitialOverlap = false;  // 退避経路の判定開始時に重なりがあったか。
        bool retreatSweepHasNormal = false;  // 退避経路のヒット法線を取得できたか。
        DirectX::XMFLOAT3 retreatSweepHitNormal{};  // 退避経路の判定でヒットした面の法線。
        float retreatSweepPenetrationDepth = 0.0f;  // 退避経路の判定開始時のめり込み深さ。
        std::string retreatSweepHitActor = "None";  // 退避経路の判定でヒットしたアクター名。
        std::string retreatSweepHitComponent = "None";  // 退避経路の判定でヒットしたコンポーネント名。
        int retreatSweepIgnoredSupportContactCount = 0;  // 退避経路判定で無視した支持面との接触数。
        bool retreatSweepPathBlocked = false;  // 退避経路が障害物に遮られたか。
        DirectX::XMFLOAT3 retreatSweepBlockingNormal{};  // 退避経路を遮った面の法線。
        float retreatSweepBlockingDistance = 0.0f;  // 退避経路を遮った障害物までの距離。
        std::string retreatSweepBlockingActor = "None";  // 退避経路を遮ったアクター名。
        std::string retreatSweepBlockingComponent = "None";  // 退避経路を遮ったコンポーネント名。
        std::string failureReason = "None";  // 探索失敗の理由。
        std::vector<RetreatCandidateDebug> candidates;  // 評価した退避候補の診断情報。
    } retreatRuntime;  // 退避候補の探索結果と診断情報。
    PositioningTargetContext retreatTarget{};  // 退避先と移動条件。
    PositioningTargetRuntime retreatMovementRuntime{};  // 退避移動の経過時間や移動距離。
    PositioningTargetRuntime attackSetupRuntime{};  // 攻撃準備位置への移動状態。
    DirectX::XMFLOAT3 retreatDebugTargetPosition{};  // 退避先のデバッグ描画位置。
    float retreatRemainingDistance = 0.0f;  // 退避先までの残り距離。
    PositioningMoveResult retreatLastMoveResult = PositioningMoveResult::None;  // 直近の退避移動結果。
    std::string retreatCompleteReason = "None";  // 退避終了理由の表示文字列。
    enum class RepositionCandidateRejectReason
    {
        None,  // 未設定。
        Clamp,  // 境界補正による除外。
        PlayerDistanceTooClose,  // プレイヤーとの距離不足による除外。
        MinimumMoveDistance,  // 最小移動距離による除外。
        Sweep  // 経路の衝突判定による除外。
    };
    struct RepositionCandidateDebug
    {
        DirectX::XMFLOAT3 position{};  // 位置調整候補の位置。
        int candidateIndex = -1;  // 候補の番号。
        float angleDegrees = 0.0f;  // 候補の角度。
        float candidateDistance = 0.0f;  // 候補の距離。
        RepositionCandidateRejectReason rejectReason = RepositionCandidateRejectReason::None;  // 位置調整候補を除外した理由。
        bool accepted = false;  // 候補が採用可能と判定されたか。
        bool selected = false;  // 候補が選択されたか。
        bool pathBlocked = false;  // 候補までの経路が遮られたか。
    };
    struct RepositionRuntime
    {
        DirectX::XMFLOAT3 playerSnapshot{};  // 評価時に記録したプレイヤー位置。
        DirectX::XMFLOAT3 gruxSnapshot{};  // 評価時に記録したボス位置。
        float startingPlayerDistance = 0.0f;  // 評価開始時のプレイヤーとの距離。
        float plannedPlayerDistance = 0.0f;  // 移動予定先とプレイヤーとの距離。
        float plannedMoveDistance = 0.0f;  // 予定移動距離。
        int candidateCount = 0;  // 評価した候補数。
        int safeCandidateCount = 0;  // 安全と判定した位置調整候補数。
        int selectedCandidateIndex = -1;  // 選択された位置調整候補の番号。
        float selectedCandidateAngleDegrees = 0.0f;  // 選択された位置調整候補の角度。
        float selectedCandidateDistance = 0.0f;  // 選択された位置調整候補の距離。
        bool selectedInitialReposition = false;  // 初期位置調整の候補が選択されたか。
        int clampAppliedCount = 0;  // 境界補正を適用した候補数。
        int clampPostClampRejectCount = 0;  // 境界補正後に除外した候補数。
        int clampPostClampAcceptedCount = 0;  // 境界補正後に採用可能と判定した候補数。
        DirectX::XMFLOAT3 lastClampedTarget{};  // 直近に境界補正した目標位置。
        float lastClampDistance = 0.0f;  // 直近の目標位置の境界補正距離。
        int playerDistanceTooCloseRejectCount = 0;  // プレイヤーに近すぎるため除外した候補数。
        int minimumMoveRejectCount = 0;  // 最小移動距離を満たさず除外した候補数。
        int sweepRejectCount = 0;  // 経路の衝突判定により除外した候補数。
        std::string failureReason = "None";  // 探索失敗の理由。
        std::vector<RepositionCandidateDebug> candidates;  // 評価した位置調整候補の診断情報。
    } repositionRuntime;  // 位置調整候補の探索結果と診断情報。
    PositioningTargetContext repositionTarget{}; PositioningTargetRuntime repositionMovementRuntime{};  // 位置調整の移動目標と実行状態。
    float repositionRemainingDistance = 0.0f;  // 位置調整先までの残り距離。
    float repositionDistanceMin = 6.0f, repositionDistanceMax = 13.0f;  // 位置調整候補と対象との距離の下限と上限。
    float repositionMinimumMoveDistance = 5.0f, repositionMinimumPlayerDistance = 4.0f, repositionMoveSpeed = 6.0f;  // 位置調整の最小移動距離、対象との最小距離、移動速度。
    float repositionArrivalWaitDuration = 0.5f;  // 位置調整の到着後に待機する時間。
    bool initialRepositionPending = true;  // 戦闘開始時の位置調整が未完了かを示す。
    bool initialRepositionFallbackIdlePending = false;  // 初期位置調整の失敗後にIdleを挟む状態。
    float initialRepositionFallbackIdleDuration = 0.2f;  // 初期位置調整の失敗後に待機する時間。
    int consecutiveAttackCount = 0;  // 位置調整の選択確率に使う攻撃完了数。
    std::array<float, 4> repositionChanceByAttackCount{ 0.10f, 0.25f, 0.60f, 1.0f };  // 攻撃完了数に応じた位置調整の選択確率。
    mutable bool repositionDecisionCached = false;  // 今回の推論で位置調整の抽選結果を保持しているか。
    mutable bool repositionDecisionResult = false;  // 今回の推論で保持した位置調整の抽選結果。
    mutable float repositionDecisionRoll = 0.0f;  // 位置調整の抽選に使った乱数値。
    bool repositionCanPlanDebug = false;  // 位置調整の計画可否の診断結果。
    unsigned long long combatDecisionDebugInferenceSerial = 0;  // 戦闘判断の推論回数。
    std::string combatDecisionDebugActiveNodeAtStart = "None";  // 推論開始時の実行中ノード名。
    std::string combatDecisionDebugDefensiveResult = "NotEvaluated";  // 防御行動の判定結果の表示文字列。
    std::string combatDecisionDebugRootSelectedNode = "None";  // 戦闘判断で選択されたノード名。
    std::string combatDecisionDebugFailureReason = "None";  // 戦闘判断の失敗理由。
    std::string combatDecisionDebugRepositionReason = "NotEvaluated";  // 位置調整の判定理由。
    bool combatDecisionDebugCanPlanAny = false;  // 戦闘行動を計画できるかの診断結果。
    bool combatDecisionDebugCanPlanReposition = false;  // 位置調整を計画できるかの診断結果。
    bool combatDecisionDebugCanPlanAnyAttack = false;  // 攻撃を計画できるかの診断結果。
    bool combatDecisionDebugFastCombo = false, combatDecisionDebugJump = false, combatDecisionDebugDash = false, combatDecisionDebugCharge = false;  // 各攻撃の計画可否の診断結果。
    float repositionCooldownDuration = 3.0f, repositionCooldownRemaining = 0.0f, repositionRetryCooldownDuration = 0.75f, repositionRetryCooldownRemaining = 0.0f;  // 位置調整と再試行のクールダウン時間および残り時間。

    std::string lastCombatDecision = "None";  // 直近の戦闘判断の表示文字列。

    struct RoarRuntime
    {
        RoarStage stage = RoarStage::None;  // Roarの実行段階。
        bool shockwaveFired = false;  // Roarの衝撃波を発生済みか。
        bool hitPlayer = false;  // Roarがプレイヤーにヒットしたか。
        float elapsed = 0.0f;  // Roarの経過時間。
        float stalled = 0.0f;  // Roarアニメーションの進行が停止した時間。
        float previousTime = 0.0f;  // 前回のRoarアニメーション時刻。
        float endTime = 0.0f;  // Roarアニメーションの終了時刻。
    } roarBT;  // Roarの実行段階と衝撃波の状態。
    std::string roarBTStatus = "Idle";  // Roarの実行・終了状態を示す表示文字列。
    BossTargetContext fastComboTargetContext{};  // FastComboの対象に関する距離と角度の情報。
    std::array<BossTargetContext, 3> fastComboStageTargetContexts{};  // FastComboの各段階で取得した対象情報。
    int fastComboTargetStage = -1;  // 対象情報を取得したFastComboの段階。
    float interStageFaceCompleteAngle = 10.0f;  // FastComboの段間の向き合わせを完了とする角度。
    float fastComboDesiredAttackDistance = 3.0f;  // FastComboの踏み込みで目指す攻撃距離。
    float fastComboStepInRatio = 1.0f;  // FastComboの踏み込み距離に適用する倍率。
    std::array<float, 3> fastComboMaxStepInDistance{ 1.5f, 2.f, 2.0f };  // FastComboの各段階で許容する最大踏み込み距離。
    float fastComboStepInMaxSpeed = 30.0f;  // FastComboの最大踏み込み速度。
    float interStageMaxFacingAngle = 70.0f;  // FastComboの段間で向き合わせを行う角度の上限。
    float interStageFaceDelay = 0.5f;  // FastComboの段間で向き合わせを始めるまでの時間。
    FastComboRuntimeState fastComboRuntimeState = FastComboRuntimeState::Attack;  // FastComboの攻撃中・段間待機・向き合わせの状態。
    bool fastComboStepInActive = false;  // FastComboの踏み込み移動中かを示す。
    int fastComboStepInStage = -1;  // 踏み込み中のFastCombo段階。
    float fastComboStepInDistance = 0.0f;  // FastComboの踏み込み予定距離。
    float fastComboStepInRemainingDistance = 0.0f;  // 踏み込みの残り移動距離。
    float fastComboStepInElapsed = 0.0f;  // FastComboの踏み込み経過時間。
    DirectX::XMFLOAT3 fastComboStepInDirection{};  // 踏み込みの移動方向。
    float fastComboStepInCalculatedSpeed = 0.0f;  // FastComboの踏み込み計算速度。
    float fastComboStepInAppliedSpeed = 0.0f;  // FastComboの踏み込みに適用した速度。
    int fastComboRuntimeStage = -1;  // 実行中のFastCombo段階。
    AttackSetupTargetContext attackSetupTarget{};  // 攻撃準備位置への移動目標と条件。
    DirectX::XMFLOAT3 attackSetupPreviousPosition{};  // 攻撃準備移動の前回位置。
    float attackSetupElapsedTime = 0.0f;  // 攻撃準備移動の経過時間。
    float attackSetupTraveledDistance = 0.0f;  // 攻撃準備移動の移動済み距離。
    float attackSetupRemainingDistance = 0.0f;  // 攻撃準備位置までの残り距離。
    float attackSetupStuckTime = 0.0f;  // 攻撃準備移動で停滞した時間。
    bool attackSetupMovementActive = false;  // 攻撃準備位置へ移動中かを示す。
    float attackSetupChosenDistance = 0.0f;  // 選択した攻撃準備位置と対象との距離。
    float attackSetupPlannedMoveDistance = 0.0f;  // 攻撃準備位置までの予定移動距離。
    int attackSetupCandidateCount = 0;  // 攻撃準備位置の候補数。
    bool attackReadySEFired = false;  // 攻撃準備SEを再生できたかの記録。
    AttackReadyReason attackReadyReason = AttackReadyReason::Front;  // 攻撃準備時間の選択に使う理由。

    bool hasSelectedActionDebug = false;  // 選択行動の診断情報があるか。

    std::array<BossAttackData, 6> combatAttackData =  // 攻撃の設定データ。
    { {
        { BossAttackType::PrimaryAttackLA, "PrimaryAttack_LA", 0.0f, 5.0f, 1.0f, 1.25f, 5 },
        { BossAttackType::PrimaryAttackRA, "PrimaryAttack_RA", 0.0f, 5.0f, 1.0f, 1.30f, 5 },
        { BossAttackType::FastCombo, "FastCombo", 0.0f, 6.0f, 1.0f, 2.0f, 4 },
        { BossAttackType::JumpAttack, "PrimaryAttack_JumpAttack", 4.5f, 12.0f, 1.0f, 2.80f, 4 },
        { BossAttackType::DashAttack, "Stampede_0 > Stampede_Knockup_0", 6.0f, 100.0f, 1.0f, 1.20f, 6 },
        { BossAttackType::ChargeAttack, "Pre_FootSlide_0 > Stampede_0", 6.0f, 100.0f, 1.0f, 0.1f, 10 },
    } };

    std::array<BossPositioningData, 4> combatPositioningData =  // 位置調整の設定データ。
    { {
        { BossActionType::Approach, BossPositioningDirection::TowardPlayer, 20.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TargetDistance, 0.0f },
        { BossActionType::Retreat, BossPositioningDirection::AwayFromPlayer, 7.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TravelDistance, 0.0f },
        { BossActionType::RepositionLeft, BossPositioningDirection::TowardPlayer, 3.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TravelDistance, 0.0f },
        { BossActionType::RepositionRight, BossPositioningDirection::TowardPlayer, 3.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TravelDistance, 0.0f },
    } };

    float recoveryDuration = 0.5f;  // 攻撃別Recovery時間を取得できない場合の代替時間。

    float nearFrontFastComboProbability = 0.7f;  // 近距離正面でFastComboを抽選する基本確率。
    float fastComboFrontMaxAngle = 90.0f;  // 近距離正面のFastCombo抽選に使う角度上限。
    float defensiveBackMinAngle = 120.0f;  // 防御行動で背後とみなす最小角度。
    float nearDistanceThreshold = 8.0f;  // 距離領域を近距離と判定する上限。
    float middleDistanceThreshold = 12.0f;  // 距離領域を中距離と判定する上限。

    float relativeFrontMaxAngle = 50.0f;  // 相対領域を正面と判定する角度上限。
    float relativeBackMinAngle = 110.0f;  // 相対領域を背後と判定する最小角度。
    const std::array<BossActionData, actionCount> initialCombatActionData = combatActionData;  // 行動設定を初期値に戻すためのデータ。
    const std::array<BossIntentData, intentCount> initialCombatIntentData = combatIntentData;  // Intent設定を初期値に戻すためのデータ。
    const std::array<BossAttackData, 6> initialCombatAttackData = combatAttackData;  // 攻撃設定を初期値に戻すためのデータ。
    const float initialNearDistanceThreshold = nearDistanceThreshold;  // 近距離判定の初期距離。
    const float initialMiddleDistanceThreshold = middleDistanceThreshold;  // 中距離判定の初期距離。
    const float initialRelativeFrontMaxAngle = relativeFrontMaxAngle;  // 正面判定の初期角度。
    const float initialRelativeBackMinAngle = relativeBackMinAngle;  // 背後判定の初期角度。
    static constexpr float initialCombatRepositionMoveDistance = 10.0f;  // 戦闘位置調整の初期移動距離。
    static constexpr float initialCombatRepositionMoveSpeed = 6.0f;  // 戦闘位置調整の初期移動速度。
    static constexpr float initialCombatRepositionSettleDuration = 1.5f;  // 戦闘位置調整後の初期待機時間。

    static constexpr float initialFrontAttackReadyDuration = 1.0f;  // 正面での攻撃準備時間の初期値。
    static constexpr float initialSideAttackReadyDuration = 1.5f;  // 側面での攻撃準備時間の初期値。
    static constexpr float initialCloseCombatReadyFacingAngle = 7.5f;  // 近接攻撃準備時の向き判定角度の初期値。

    float turnSpeed = 480.0f;  // 攻撃前などの向き合わせに使う回転速度。
    float turnCompleteAngle = 15.0f;  // 攻撃前の向き合わせを完了とみなす角度。
    float turnTimeout = 1.5f;  // Chargeの向き合わせに使うタイムアウト時間。
    std::string lastAIDecisionReason = "None";  // 直近のAI判断理由の表示文字列。
    float frontAttackReadyDuration = initialFrontAttackReadyDuration;  // 正面での攻撃準備時間。
    float sideAttackReadyDuration = initialSideAttackReadyDuration;  // 側面からの攻撃準備時間。
    float closeCombatReadyFacingAngle = initialCloseCombatReadyFacingAngle;  // 近接攻撃準備時の向き判定角度。
    std::string attackReadySEName = "enemy_attack_ready1";  // 攻撃準備時に再生するSE名。
    float attackReadySEVolume = 1.0f;  // 攻撃準備SEの音量。
    struct PositioningDebugSnapshot
    {
        bool valid = false;  // 診断情報が有効か。
        BossPositioningData data{};  // 診断対象の位置調整データ。
        std::optional<BossIntentType> intent = std::nullopt;  // 位置調整に対応するIntent。
        float preferredMin = 0.0f;  // 望ましい距離の下限。
        float preferredMax = 0.0f;  // 望ましい距離の上限。
        DirectX::XMFLOAT3 startBossPosition{};  // 位置調整開始時のボス位置。
        DirectX::XMFLOAT3 startPlayerPosition{};  // 位置調整開始時のプレイヤー位置。
        DirectX::XMFLOAT3 desiredTargetPosition{};  // 境界補正前の目標位置。
        DirectX::XMFLOAT3 clampedTargetPosition{};  // 境界補正後の目標位置。
        bool targetWasClamped = false;  // 目標位置に境界補正を適用したか。
        float targetClampDistance = 0.0f;  // 目標位置の境界補正距離。
        float availableMoveDistance = 0.0f;  // 移動可能な距離。
        float safetyMargin = 0.0f;  // 境界から確保する余白距離。
        float safeMinX = 0.0f;  // 安全範囲のX座標の下限。
        float safeMaxX = 0.0f;  // 安全範囲のX座標の上限。
        float safeMinZ = 0.0f;  // 安全範囲のZ座標の下限。
        float safeMaxZ = 0.0f;  // 安全範囲のZ座標の上限。
        DirectX::XMFLOAT3 currentPosition{};  // 位置調整中の現在位置。
        DirectX::XMFLOAT3 requestedMoveDirection{};  // 要求された移動方向。
        float startPlayerDistance = 0.0f;  // 開始時のプレイヤーとの距離。
        float currentPlayerDistance = 0.0f;  // 現在のプレイヤーとの距離。
        float plannedMoveDistance = 0.0f;  // 予定移動距離。
        float actualMoveDistance = 0.0f;  // 実際に移動した距離。
        float traveledPathDistance = 0.0f;  // 移動経路の累積距離。
        float remainingDistance = 0.0f;  // 目標位置までの残り距離。
        float frameMovement = 0.0f;  // 現在のフレームの移動量。
        float actualSpeed = 0.0f;  // 実際の移動速度。
        float elapsedTime = 0.0f;  // 位置調整の経過時間。
        float stuckTimer = 0.0f;  // 移動中の停滞時間。
        float inputMagnitude = 0.0f;  // 移動入力の大きさ。
        std::string endReason = "None";  // 位置調整の終了理由。
    };
    static constexpr float bossRoomMinX = -2.5f;  // ボス部屋のX座標の下限。
    static constexpr float bossRoomMaxX = 19.5f;  // ボス部屋のX座標の上限。
    static constexpr float bossRoomMinZ = 0.0f;  // ボス部屋のZ座標の下限。
    static constexpr float bossRoomMaxZ = 21.5f;  // ボス部屋のZ座標の上限。
    float bossRoomSafetyMargin = 0.5f;  // ボス部屋の境界から確保する余白距離。
    float positioningArrivalDistance = 0.3f;  // 位置調整先への到着と判定する距離。
    float positioningMoveStartSpeedThreshold = 0.30f;  // 移動アニメーションを開始する速度の閾値。
    float positioningMoveStopSpeedThreshold = 0.10f;  // 移動アニメーションを停止する速度の閾値。
    float positioningMoveStopDelay = 0.15f;  // 移動アニメーションを停止するまでの待ち時間。
    float positioningMoveStopTimer = 0.0f;  // 移動停止判定の経過時間。
    float positioningAnimationActualSpeed = 0.0f;  // 位置調整アニメーションに使用する実速度。
    bool positioningAnimationMoving = false;  // 位置調整の移動アニメーション中か。
    float combatRepositionBackWeight = 80.0f;  // 背後からの戦闘位置調整に使う重み。
    float dashAttackPlanBackWeight = 35.0f;  // 背後におけるDash計画の重み。
    float jumpAttackPlanBackWeight = 35.0f;  // 背後におけるJump計画の重み。
    float combatRepositionMoveDistance = initialCombatRepositionMoveDistance;  // 戦闘位置調整の移動距離。
    float combatRepositionMoveSpeed = initialCombatRepositionMoveSpeed;  // 戦闘位置調整の移動速度。
    float combatRepositionSettleDuration = initialCombatRepositionSettleDuration;  // 戦闘位置調整後の初期待機時間。
    bool combatRepositionSettling = false;  // 戦闘位置調整後の待機中か。
    float combatRepositionSettleRemaining = 0.0f;  // 戦闘位置調整後の残り待機時間。
    const float initialCombatRepositionBackWeight = combatRepositionBackWeight;  // 背後からの戦闘位置調整の重みの初期値。
    const float initialDashAttackPlanBackWeight = dashAttackPlanBackWeight;  // 背後からのDash計画の重みの初期値。
    const float initialJumpAttackPlanBackWeight = jumpAttackPlanBackWeight;  // 背後からのJump計画の重みの初期値。
    bool combatRepositionIntentPending = false;  // 戦闘位置調整のIntentが保留中か。
    float postAttackCombatRepositionWeight = 45.0f;  // 攻撃後の戦闘位置調整に使う重み。
    const float initialPostAttackCombatRepositionWeight =  // 攻撃後の戦闘位置調整の重みの初期値。
        postAttackCombatRepositionWeight;
    bool postAttackCombatRepositionBoostPending = false;  // 攻撃後の位置調整重みの補正が保留中か。
    bool postAttackCombatRepositionBoostApplied = false;  // 攻撃後の位置調整重みの補正を適用済みか。
    BossRepositionReason repositionReason = BossRepositionReason::Normal;  // 位置調整を選択した理由。
    BossRepositionDirection selectedRepositionDirection = BossRepositionDirection::None;  // 選択された位置調整の方向。

    bool positioningDebugActive = true;  // 位置調整の診断情報を有効にするか。
    PositioningDebugSnapshot currentPositioningDebug{};  // 実行中の位置調整の診断情報。
    PositioningDebugSnapshot lastPositioningDebug{};  // 直近の位置調整の診断情報。

    // FastCombo??A???U???p
    bool transitionWindow = false;  // FastComboの次段階へ移行できるNotify期間か。

    // JumpAttack??MotionWarp?p
    float maxJumpDistance = 21.5f;  // Jump攻撃の最大移動距離。
    float desiredAttackDistance = 0.1f;  // Jump攻撃で目指す対象との距離。
    float jumpSetupDistanceMin = 6.5f;  // Jumpの準備位置に必要な対象との最小距離。
    float jumpSetupDistanceMax = 20.f;  // Jumpの準備位置に必要な対象との最大距離。
    float jumpSetupMinimumMoveDistance = 5.0f;  // Jump準備位置への最小移動距離。
    float attackSetupCandidateAngleStep = 30.0f;  // 攻撃準備位置の候補を探索する角度間隔。
    float attackSetupClampTolerance = 0.75f;  // 攻撃準備位置の境界補正の許容距離。
    float jumpAttackTelegraphStartTime = 1.4f;  // Jump予兆の開始アニメーション時刻。
    float jumpAttackTelegraphEndTime = 2.6f;  // Jump予兆の終了アニメーション時刻。
    float currentJumpPlayerDistance = 0.0f;  // Jump開始時のプレイヤーとの距離。
    float calculatedJumpDistance = 0.0f;  // Jump攻撃で計算された移動距離。
    bool tripleJumpActive = false;  // 複数回Jumpの実行中かを示す。
    int tripleJumpIndex = 0;  // 現在のJump回のインデックス。
    TripleJumpPhase tripleJumpPhase = TripleJumpPhase::None;  // 複数回Jumpの実行段階。
    float tripleJumpTransitionElapsed = 0.0f;  // Jump間の遷移経過時間。
    float tripleJumpTransitionDuration = 0.3f;  // Jump間の遷移時間。
    float jumpTelegraphScale = 4.0f;// ジャンプ予兆モデルの大きさ
    bool jumpTelegraphEmissiveEnabled = true;  // Jump予兆の発光を有効にするか。
    DirectX::XMFLOAT3 jumpTelegraphPhase1EmissiveColor{ 0.05f, 0.75f, 1.0f };  // 第1段階のJump予兆の発光色。
    DirectX::XMFLOAT3 jumpTelegraphPhase2EmissiveColor{ 1.0f, 0.16f, 0.03f };  // 第2段階のJump予兆の発光色。
    float jumpTelegraphEmissiveIntensity = 15.0f;  // Jump予兆の発光強度。
    float jumpTelegraphInnerStartScale = 0.01f;  // Jump予兆の内側モデルの開始スケール。
    float jumpTelegraphLandingTime = 0.869391f;  // Jump予兆の着地時刻。
    float jumpTelegraphProgress = 0.0f;  // Jump予兆の進行度。
    float jumpTelegraphCurrentAnimationTime = 0.0f;  // Jump予兆に使用する現在のアニメーション時刻。
    float jumpTelegraphInnerCurrentScale = 0.75f;  // Jump予兆の内側モデルの現在のスケール。
    int jumpTelegraphInnerScaleCurve = 0;  // Jump予兆の内側モデルの拡縮曲線の選択。
    bool jumpTelegraphEnableRotation = true;  // Jump予兆の回転を有効にするか。
    float jumpTelegraphOuterRotationSpeed = 360.0f;  // Jump予兆の外側モデルの回転速度。
    float jumpTelegraphInnerRotationSpeed = -360.0f;  // Jump予兆の内側モデルの回転速度。
    float jumpTelegraphInnerScaleProgress = 0.0f;  // Jump予兆の内側モデルの拡縮進行度。
    float jumpTelegraphOuterRotation = 0.0f;  // Jump予兆の外側モデルの回転角度。
    float jumpTelegraphInnerRotation = 0.0f;  // Jump予兆の内側モデルの回転角度。
    bool jumpTelegraphEnableImpactFlash = true;  // Jump着地時の発光演出を有効にするか。
    float jumpTelegraphImpactFlashDuration = 0.10f;  // Jump着地時の発光演出の時間。
    float jumpTelegraphImpactFlashIntensity = 5.f;  // Jump着地時の発光演出の強度。
    bool jumpTelegraphImpactFlashActive = false;  // Jump着地時の発光演出中か。
    float jumpTelegraphImpactFlashElapsed = 0.0f;  // Jump着地時の発光演出の経過時間。
    float jumpTelegraphImpactFlashProgress = 0.0f;  // Jump着地時の発光演出の進行度。
    float jumpTelegraphOffset = 0.05f;// ジャンプ予兆モデルのオフセット
    DirectX::XMFLOAT3 jumpTelegraphWorldPosition{}; // ジャンプ攻撃の間
    DirectX::XMFLOAT3 tripleJumpPlayerPosition{};  // 現在のJumpで固定したPlayer位置。
    DirectX::XMFLOAT3 tripleJumpStartPosition{};  // 現在のJump開始位置。
    DirectX::XMFLOAT3 tripleJumpLockedDirection{};  // 現在のJumpで固定した方向。
    float tripleJumpPlannedDistance = 0.0f;  // 現在のJumpの予定移動距離。
    DirectX::XMFLOAT3 tripleJumpPlannedLandingPosition{};  // 現在のJumpの予定着地点。
    bool tripleJumpTargetLockActive = false;  // Jump開始時に固定した目標を適用中かを示す。
    // Temporary Jump landing diagnostics. Values are reset once per jump telegraph.
    bool showJumpLandingDebug = true;  // Jump着地の診断描画を表示するか。
    bool jumpLandingDebugSnapshotValid = false;  // Jump着地の診断情報が有効か。
    bool jumpLandingDebugMotionWarpEndCaptured = false;  // MotionWarp終了時の位置を記録済みか。
    bool jumpLandingDebugGroundImpactCaptured = false;  // 着地衝撃時の位置を記録済みか。
    bool jumpLandingDebugWallHit = false;  // Jump中に壁へ衝突したかの診断値。
    float jumpLandingDebugTargetErrorXZ = 0.0f;  // Jump着地点と目標位置の水平距離の誤差。
    DirectX::XMFLOAT3 jumpLandingDebugPlayerSnapshot{};  // Jump着地診断に記録したプレイヤー位置。
    DirectX::XMFLOAT3 jumpLandingDebugPlannedLanding{};  // Jump着地診断に記録した予定着地点。
    DirectX::XMFLOAT3 jumpLandingDebugTelegraphCenter{};  // Jump予兆の中心位置の診断値。
    DirectX::XMFLOAT3 jumpLandingDebugMotionWarpEndBossRoot{};  // MotionWarp終了時のボス位置。
    DirectX::XMFLOAT3 jumpLandingDebugGroundImpactBossRoot{};  // 着地衝撃時のボス位置。
    DirectX::XMFLOAT3 jumpLandingDebugGroundImpactWeaponRoot{};  // 着地衝撃時の武器の根元位置。
    DirectX::XMFLOAT3 jumpLandingDebugGroundImpactWeaponMiddle{};  // 着地衝撃時の武器の中央位置。
    DirectX::XMFLOAT3 jumpLandingDebugGroundImpactWeaponTip{};  // 着地衝撃時の武器の先端位置。
    DirectX::XMFLOAT3 jumpLandingDebugWallCollisionPosition{};  // Jump中に壁へ衝突した位置。
    bool jumpMotionWarpOverrideActive = false;  // Jump用のMotionWarp上書きが有効か。
    DirectX::XMFLOAT3 jumpAttackStartPlayerPosition{};  // Jump開始時のプレイヤー位置。
    DirectX::XMFLOAT3 jumpMotionWarpDirection{ 0.0f, 0.0f, 1.0f };  // Jump用MotionWarpの方向。

    // DashAttack
    float dashSetupDistanceMin = 8.0f;  // Dashの準備位置に必要な対象との最小距離。
    float dashSetupDistanceMax = 10.0f;  // Dashの準備位置に必要な対象との最大距離。
    float dashSetupMinimumMoveDistance = 5.0f;  // Dash準備位置への最小移動距離。
    DashBTPhase dashBTPhase = DashBTPhase::None;  // Dashの準備からRecoveryまでの実行段階。
    bool dashBTAttackStarted = false;  // Dashの攻撃シーケンスが開始済みかを示す。
    BossActionType dashBTPreviousAction = BossActionType::AttackLA;  // Dash開始前に選択されていた行動。
    float dashBTTelegraphElapsed = 0.0f;  // Dash予兆の経過時間。
    float dashBTTraveledDistance = 0.0f;  // Dashで移動した距離。
    // Dash count is deliberately independent of DashAttack's three animation stages.
    bool dashBTTripleDashActive = false;  // 複数回Dashを実行する状態。
    int dashBTDashIndex = 0;  // 現在のDash回のインデックス。
    int dashBTDashMaxCount = 1;  // 今回の攻撃で実行するDashの回数。
    int dashBTCurrentDashHitStartCount = 0;  // 現在のDashで記録したヒット開始回数。
    bool dashBTAbortRemainingDashes = false;  // 残りのDashを中断する状態。
    // Dash 1 consumes these on the BT update following the authored Pre_Stampede events.
    // They deliberately do not participate in Dash 2/3's fixed inter-dash transition.
    bool dashBTDirectionLockRequested = false;  // Dashの方向固定が要求されているか。
    bool dashBTDashStartRequested = false;  // Dashの開始が要求されているか。
    bool dashBTDirectionLockEventReceived = false;  // Dashの方向固定イベントを受信したか。
    bool dashBTDashStartEventReceived = false;  // Dash開始イベントを受信したか。
    float dashBTInterDashTrackingDuration = 0.10f;  // Dash間に対象を追跡する時間。
    float dashBTTransitionDuration = 0.40f;  // Dash間の遷移時間。
    float dashBTTransitionElapsed = 0.0f;  // Dash間の遷移経過時間。
    float dashBTTelegraphHoldDuration = 0.0f;  // Dash予兆を保持する時間。
    bool dashBTDirectionLocked = false;  // Dashの移動方向が固定済みかを示す。
    bool dashBTMovementSnapshotPrepared = false;  // Dash移動に使う情報が準備済みかを示す。
    DirectX::XMFLOAT3 dashBTPredictedKnockupPosition{};  // Dash後のKnockup開始予測位置。
    DirectX::XMFLOAT3 dashBTActualKnockupStartPosition{};  // Dash後のKnockup実際の開始位置。
    float dashBTPredictionError = 0.0f;  // Knockup開始位置の予測誤差。
    bool dashBTKnockupPositionCaptured = false;  // Knockup開始位置を記録済みか。
    // Visual-only Direction Lock snapshot; it never re-targets gameplay.
    bool showDashTelegraph = true;  // Dash予兆の表示を有効にする。
    bool dashTelegraphActive = false;  // Dash予兆が表示中か。
    bool forceShowDashTelegraph = false;  // Dash予兆をデバッグ用に表示する。
    bool dashTelegraphSnapshotValid = false;  // Dash予兆の位置と方向の記録が有効か。
    enum class DashTelegraphVisualState
    {
        Hidden,  // 非表示。
        LineExpanding,  // 線を伸ばしている状態。
        FanFadingIn,  // 扇形をフェードインしている状態。
        Holding,  // 予兆を保持している状態。
        DashActive,  // Dash中。
        FanFlashing,  // 扇形が発光中。
        FanFadingOut  // 扇形をフェードアウトしている状態。
    };
    DashTelegraphVisualState dashTelegraphVisualState = DashTelegraphVisualState::Hidden;  // Dash予兆の表示アニメーション段階。
    float dashTelegraphVisualElapsed = 0.0f;  // Dash予兆の表示段階の経過時間。
    float dashTelegraphLineExpandDuration = 0.15f;  // Dash予兆の線を伸ばす時間。
    float dashTelegraphLineExpandProgress = 0.0f;  // Dash予兆の線を伸ばす進行度。
    bool dashTelegraphLineDynamicShrinkActive = false;  // Dash予兆の線が移動に合わせて縮小中か。
    DirectX::XMFLOAT3 dashTelegraphDynamicLineStart{};  // Dash予兆の縮小する線の開始位置。
    float dashTelegraphCurrentLineLength = 0.0f;  // Dash予兆の線の現在の長さ。
    float dashTelegraphLineShrinkProgress = 0.0f;  // Dash予兆の線の縮小進行度。
    float dashTelegraphLineMinimumVisibleLength = 0.05f;  // Dash予兆の線が表示される最小長さ。
    float dashTelegraphLineFadeOutDuration = 0.12f;  // Dash予兆の線のフェードアウト時間。
    float dashTelegraphLineFadeOutElapsed = 0.0f;  // Dash予兆の線のフェードアウト経過時間。
    float dashTelegraphLineAlpha = 0.0f;  // Dash予兆の線の不透明度。
    float dashTelegraphFanFadeInDuration = 0.12f;  // Dash予兆の扇形のフェードイン時間。
    float dashTelegraphFanFadeInProgress = 0.0f;  // Dash予兆の扇形のフェードイン進行度。
    float dashTelegraphFanAlpha = 0.0f;  // Dash予兆の扇形の不透明度。
    bool dashTelegraphFanFlashActive = false;  // Dash予兆の扇形が発光中か。
    float dashTelegraphFanFlashDuration = 0.05f;  // Dash予兆の扇形の発光時間。
    float dashTelegraphFanFlashIntensity = 2.0f;  // Dash予兆の扇形の発光強度。
    float dashTelegraphFanFadeOutDuration = 0.10f;  // Dash予兆の扇形のフェードアウト時間。
    float dashTelegraphLineFadeOutStartAlpha = 0.0f;  // Dash予兆の線のフェードアウト開始時の不透明度。
    float dashTelegraphFanFadeOutStartAlpha = 0.0f;  // Dash予兆の扇形のフェードアウト開始時の不透明度。
    bool dashTelegraphFadeOutEventReceived = false;  // Dash予兆のフェードアウトイベントを受信したか。
    bool dashTelegraphFadeOutActive = false;  // Dash予兆のフェードアウト中か。
    float dashTelegraphLineWidth = 4.f;  // Dash予兆の線の幅。
    bool dashTelegraphEmissiveEnabled = true;  // Dash予兆の発光を有効にするか。
    DirectX::XMFLOAT3 dashTelegraphPhase1EmissiveColor{ 0.05f, 0.75f, 1.0f };  // 第1段階のDash予兆の発光色。
    DirectX::XMFLOAT3 dashTelegraphPhase2EmissiveColor{ 1.0f, 0.16f, 0.03f };  // 第2段階のDash予兆の発光色。
    float dashTelegraphEmissiveIntensity = 15.0f;  // Dash予兆の発光強度。
    float dashTelegraphFanRadius = 4.0f;    // 突進の扇形モデルのスケール
    float dashTelegraphLineYOffset = 0.07f;  // Dash予兆の線の高さオフセット。
    float dashTelegraphFanYOffset = 0.07f;  // Dash予兆の扇形の高さオフセット。
    DirectX::XMFLOAT3 dashTelegraphLineStart{};  // Dash予兆の線の開始位置。
    DirectX::XMFLOAT3 dashTelegraphLineEnd{};  // Dash予兆の線の終了位置。
    float dashTelegraphLineLength = 0.0f;  // Dash予兆の線の長さ。
    DirectX::XMFLOAT3 dashTelegraphFanPosition{};  // Dash予兆の扇形の位置。
    DirectX::XMFLOAT3 dashTelegraphFanForward{};  // Dash予兆の扇形の向き。
    // Used only by the legacy EnemyAttackState path, not Dash BT's Timeline gate.
    float dashWindupDuration = 1.40f;  // 旧Dash攻撃の予備動作時間。
    float dashAttackSpeed = 12.0f;  // Dash攻撃の移動速度。
    float minDashAttackDistance = 4.0f;  // Dash攻撃の最小移動距離。
    float maxDashAttackDistance = 16.0f;  // Dash攻撃の最大移動距離。
    float desiredDashAttackDistance = 2.0f;  // Dash攻撃で目指す対象との距離。
    float dashArrivalDistance = 0.35f;  // Dash攻撃の到着判定距離。
    float dashAttackTimeout = 1.10f;  // Dash攻撃の移動タイムアウト時間。
    float currentDashAttackPlayerDistance = 0.0f;  // Dash攻撃時のプレイヤーとの距離。
    float calculatedDashAttackDistance = 0.0f;  // Dash攻撃で計算された移動距離。
    float dashAttackElapsedTime = 0.0f;  // Dash攻撃の経過時間。
    bool dashAttackMovementActive = false;  // Dashの移動処理が有効かを示す。
    DirectX::XMFLOAT3 dashAttackDirection{ 0.0f, 0.0f, 1.0f };  // Dashの移動方向。
    DirectX::XMFLOAT3 dashAttackStartPosition{};  // Dash攻撃の開始位置。
    DirectX::XMFLOAT3 dashTargetPosition{};  // Dash攻撃の目標位置。

    // ChargeAttack
    // Charge BTの実行を許可する状態か判定する。
    bool IsChargeBTExecutionAllowed() const;

    // Chargeアニメーションの進行停止を監視する。
    bool UpdateChargeAnimationWatchdogBT(float animationTime, float deltaTime);
    struct ChargeBTRuntime
    {
        struct TripleChargeTelegraphSnapshot
        {
            uint64_t generation = 0;  // 予兆の診断記録の世代番号。
            int chargeIndex = 0;  // 診断記録に対応するChargeの番号。
            DirectX::XMFLOAT3 direction{};  // 診断記録に対応するChargeの方向。
            bool directionLocked = false;  // 予兆の方向が固定済みか。
            DirectX::XMFLOAT3 castOrigin{};  // 予兆の判定開始位置。
            DirectX::XMFLOAT3 castEnd{};  // 予兆の判定終了位置。
            DirectX::XMFLOAT3 requestedPosition{};  // 予兆に要求された位置。
            DirectX::XMFLOAT4 requestedRotation{};  // 予兆に要求された回転。
            float requestedYaw = 0.0f;  // 予兆に要求されたYaw角。
            DirectX::XMFLOAT3 requestedScale{};  // 予兆に要求されたスケール。
            DirectX::XMFLOAT3 relativePosition{};  // 予兆コンポーネントの相対位置。
            DirectX::XMFLOAT3 relativeScale{};  // 予兆コンポーネントの相対スケール。
            DirectX::XMFLOAT3 componentWorldPosition{};  // 予兆コンポーネントのワールド位置。
            DirectX::XMFLOAT3 componentWorldScale{};  // 予兆コンポーネントのワールドスケール。
            DirectX::XMFLOAT4X4 componentWorldMatrix{};  // 予兆コンポーネントのワールド行列。
            DirectX::XMFLOAT3 actualMeshStart{};  // 予兆メッシュの実際の開始位置。
            DirectX::XMFLOAT3 actualMeshEnd{};  // 予兆メッシュの実際の終了位置。
            DirectX::XMFLOAT3 startPosition{};  // 予兆の開始位置。
            DirectX::XMFLOAT3 gruxPosition{};  // 診断記録時のボス位置。
            DirectX::XMFLOAT3 gruxScale{ 1.0f, 1.0f, 1.0f };  // 診断記録時のボスのスケール。
            bool rawWallHit = false;  // 壁判定の生のヒット結果。
            bool wallHit = false;  // 壁にヒットしたか。
            float wallDistance = 0.0f;  // 壁までの距離。
            float worldStaticDistance = 0.0f;  // 静的地形までの距離。
            float boundaryDistance = 0.0f;  // 境界までの距離。
            std::string lengthSource = "MaxDistanceFallback";  // 予兆の長さを決めた要因。
            DirectX::XMFLOAT3 hitPosition{};  // ヒット位置。
            DirectX::XMFLOAT3 hitNormal{};  // ヒットした面の法線。
            float length = 0.0f;  // 予兆の長さ。
            float wallCastRadius = 0.0f;  // 壁判定の半径。
            float playerCastRadius = 0.0f;  // プレイヤー判定の半径。
            float wallTurnClearance = 0.0f;  // 壁際の方向転換に必要な余白距離。
            float maxDistance = 0.0f;  // 予兆の最大距離。
            float capsuleRadius = 0.0f;  // カプセルの半径。
            float phase2Scale = 0.0f;  // 第2段階のスケール。
            float phase2TransformProgress = 0.0f;  // 第2段階への変化の進行度。
            bool bossInFinalPhase = false;  // ボスが最終段階にあるか。
            std::string wallHitActor = "None";  // ヒットした壁のアクター名。
            std::string wallHitComponent = "None";  // ヒットした壁のコンポーネント名。
            uint32_t wallHitLayer = 0;  // ヒットした壁のレイヤー。
        };

        ChargeBTPhase phase = ChargeBTPhase::None;  // Chargeの準備からRecoveryまでの実行段階。
        ChargeBTStunPhase stunPhase = ChargeBTStunPhase::None;  // Charge後のスタン処理の段階。
        ChargeAttackEndReason result = ChargeAttackEndReason::None;  // Chargeの終了理由。
        bool failed = false;  // Charge BTが失敗したか。
        bool attackStarted = false;  // Chargeの攻撃シーケンスを開始済みか。
        bool cooldownStarted = false;  // Chargeのクールダウン開始処理を実施済みか。
        BossActionType previousAction = BossActionType::AttackLA;  // Charge開始前に選択されていた行動。
        std::string initialStateName;  // Charge BT開始時の状態名。
        float facingElapsed = 0.0f;  // Chargeの向き合わせの経過時間。
        float animationElapsed = 0.0f;  // Chargeアニメーションの経過時間。
        float animationStalled = 0.0f;  // Chargeアニメーションの進行が停止した時間。
        float previousAnimationTime = 0.0f;  // 前回のChargeアニメーション時刻。
        float stunElapsed = 0.0f;  // Charge後のスタンの経過時間。
        float recoveryDuration = 0.0f;  // Charge後のRecovery時間。
        float activeStunDuration = 0.0f;  // 現在適用するスタン時間。
        TripleChargePhase triplePhase = TripleChargePhase::None;  // 複数回Chargeの実行段階。
        bool tripleChargeActive = false;  // 複数回Chargeを実行する状態。
        bool phase2SettingsLatched = false;  // 第2段階の設定を確定済みか。
        int tripleChargeIndex = 0;  // 現在のCharge回のインデックス。
        float tripleChargeTransitionElapsed = 0.0f;  // Charge間の遷移経過時間。
        float tripleChargeTransitionDuration = 0.3f;  // Charge間の遷移時間。
        float tripleChargeLegElapsed = 0.0f;  // 現在のChargeの経過時間。
        float tripleChargeLegDistance = 0.0f;  // 現在のChargeの移動距離。
        float tripleChargeTelegraphElapsed = 0.0f;  // 複数回Chargeの予兆の経過時間。
        uint64_t tripleChargeTelegraphCallCount = 0;  // 複数回Chargeの予兆処理の呼び出し回数。
        uint64_t tripleChargeRuntimeInitializeCount = 0;  // 複数回Chargeの実行状態の初期化回数。
        TripleChargePhase triplePhaseAtUpdateEntry = TripleChargePhase::None;  // 更新開始時の複数回Chargeの段階。
        TripleChargePhase triplePhaseAtUpdateExit = TripleChargePhase::None;  // 更新終了時の複数回Chargeの段階。
        uint64_t triplePhaseChangeCount = 0;  // 複数回Chargeの段階の変更回数。
        bool tripleChargeTelegraphVisible = false;  // 複数回Chargeの予兆が表示中か。
        DirectX::XMFLOAT3 tripleChargeTelegraphDirection{};  // 複数回Chargeの予兆の方向。
        DirectX::XMFLOAT3 tripleChargeTelegraphStartPosition{};  // 複数回Chargeの予兆の開始位置。
        float tripleChargeTelegraphLength = 0.0f;  // 複数回Chargeの予兆の長さ。
        float tripleChargeTelegraphWidth = 0.0f;  // 複数回Chargeの予兆の幅。
        bool tripleChargeTelegraphWallHit = false;  // 複数回Chargeの予兆の壁判定がヒットしたか。
        float tripleChargeTelegraphWallDistance = 0.0f;  // 複数回Chargeの予兆の壁までの距離。
        float tripleChargeTelegraphWorldStaticDistance = 0.0f;  // 複数回Chargeの予兆の静的地形までの距離。
        float tripleChargeTelegraphBoundaryDistance = 0.0f;  // 複数回Chargeの予兆の境界までの距離。
        std::string tripleChargeTelegraphLengthSource = "MaxDistanceFallback";  // 複数回Chargeの予兆の長さを決めた要因。
        std::array<TripleChargeTelegraphSnapshot, 3> tripleChargeTelegraphSnapshots{};  // 複数回Chargeの予兆表示の診断記録。
        uint64_t tripleChargeTelegraphSnapshotCount = 0;  // 複数回Chargeの予兆の診断記録数。
        uint64_t tripleChargeTelegraphSnapshotWriteIndex = 0;  // 複数回Chargeの予兆の診断記録の書き込み位置。
        bool tripleBeginTripleChargeLegCalled = false;  // 各Chargeの開始処理が呼ばれたか。
        bool tripleBeginTripleChargeLegResult = false;  // 各Chargeの開始処理の結果。
        bool tripleBeginChargeMovementResult = false;  // Charge移動開始処理の結果。
        bool tripleHoldEndChargeStartValidation = false;  // 予兆保持終了時のCharge開始可否の検証結果。
        float tripleHoldEndChargeStartClearance = 0.0f;  // 予兆保持終了時のCharge開始経路の余白距離。
        std::string tripleHoldEndChargeStartFailureReason = "None";  // 予兆保持終了時にCharge開始が失敗した理由。
        DirectX::XMFLOAT3 tripleChargeDirections[3]{};  // 各Chargeの方向。
        DirectX::XMFLOAT3 tripleChargeStartPositions[3]{};  // 各Chargeの開始位置。
        bool tripleFinalWallHit = false;  // 複数回Chargeの最後に壁へ衝突したか。
        bool tripleEarlyWallHit = false;  // 複数回Chargeの途中で壁へ衝突したか。
        float tripleWallStunDurationMultiplier = 1.3f;  // 複数回Chargeで壁へ衝突した際のスタン時間倍率。
        float tripleChargeWallTurnClearance = 15.0f;  // 複数回Chargeで壁際の方向転換に必要な余白距離。
        float tripleChargeLegMaxDistance = 50.0f;  // 複数回Chargeの各移動の最大距離。
        float tripleChargeLegMaxDuration = 50.0f;  // 複数回Chargeの各移動の最大時間。
        float tripleCurrentWallClearance = 0.0f;  // 現在の壁までの余白距離。
        bool tripleWallTurnCandidate = false;  // 壁際での方向転換の候補か。
        bool tripleWallTurnTriggered = false;  // 壁際での方向転換が発生したか。

        bool tripleChargeRepositionActive = false;  // 複数回Chargeの位置補正中か。

        bool tripleChargeRepositionRequired = false;  // 複数回Chargeの位置補正が必要か。

        DirectX::XMFLOAT3 tripleChargeRepositionStartPosition{};  // 複数回Chargeの位置補正の開始位置。

        DirectX::XMFLOAT3 tripleChargeRepositionTargetPosition{};  // 複数回Chargeの位置補正の目標位置。

        DirectX::XMFLOAT3 tripleChargeRepositionCurrentPosition{};  // 複数回Chargeの位置補正中の現在位置。

        DirectX::XMFLOAT3 tripleChargeRepositionDirection{};  // 複数回Chargeの位置補正の方向。

        float tripleChargeRepositionRequiredClearance = 0.0f;  // 複数回Chargeの位置補正に必要な余白距離。

        float tripleChargeRepositionCurrentClearance = 0.0f;  // 複数回Chargeの位置補正時の現在の余白距離。

        float tripleChargeRepositionShortage = 0.0f;  // 複数回Chargeの位置補正時の不足距離。

        float tripleChargeRepositionPlannedDistance = 0.0f;  // 複数回Chargeの位置補正の予定距離。

        float tripleChargeRepositionMovedDistance = 0.0f;  // 複数回Chargeの位置補正の移動済み距離。

        bool tripleChargeRepositionValidationBefore = false;  // 複数回Chargeの位置補正前の検証結果。

        bool tripleChargeRepositionValidationAfter = false;  // 複数回Chargeの位置補正後の検証結果。
        DirectX::XMFLOAT3 tripleChargeRepositionPlayerPositionBefore{};  // 複数回Chargeの位置補正前のプレイヤー位置。
        DirectX::XMFLOAT3 tripleChargeRepositionPlayerPositionAfter{};  // 複数回Chargeの位置補正後のプレイヤー位置。
        DirectX::XMFLOAT3 tripleChargeRepositionDirectionBefore{};  // 複数回Chargeの位置補正前の方向。
        DirectX::XMFLOAT3 tripleChargeRepositionDirectionAfter{};  // 複数回Chargeの位置補正後の方向。
        DirectX::XMFLOAT3 tripleChargeFinalLockedDirection{};  // 複数回Chargeの最終的に固定した方向。
        DirectX::XMFLOAT3 tripleChargeFinalValidationOrigin{};  // 複数回Chargeの最終経路検証の開始位置。
        DirectX::XMFLOAT3 tripleChargeFinalValidationDirection{};  // 複数回Chargeの最終経路検証の方向。
        float tripleChargeFinalValidationClearance = 0.0f;  // 複数回Chargeの最終経路検証の余白距離。
        bool tripleChargeFinalValidationResult = false;  // 複数回Chargeの最終経路検証の結果。
        DirectX::XMFLOAT3 tripleChargeSideWallHitPosition{};  // 複数回Chargeの側方の壁のヒット位置。
        DirectX::XMFLOAT3 tripleChargeSideWallHitNormal{};  // 複数回Chargeの側方の壁のヒット法線。
        float tripleChargeSideClearance = 0.0f;  // 複数回Chargeの側方の余白距離。
        bool tripleChargeSideWallHit = false;  // 複数回Chargeの側方の壁判定がヒットしたか。
        std::string tripleChargeSideWallActor;  // 複数回Chargeの側方でヒットしたアクター名。
        std::string tripleChargeSideWallComponent;  // 複数回Chargeの側方でヒットしたコンポーネント名。
        float tripleChargeForwardCorrectionDistance = 0.0f;  // 複数回Chargeの前方への補正距離。
        float tripleChargeSideCorrectionDistance = 0.0f;  // 複数回Chargeの側方への補正距離。
        bool tripleChargeFinalPathSafe = false;  // 複数回Chargeの最終経路が安全か。
    } chargeBT;  // Chargeの実行状態と診断情報。
    float chargeSetupDistanceMin = 8.0f;  // Chargeの準備位置に必要な対象との最小距離。
    float chargeSetupDistanceMax = 10.0f;  // Chargeの準備位置に必要な対象との最大距離。
    float chargeSetupMinimumMoveDistance = 3.0f;  // Chargeの準備位置への最小移動距離。
    float chargeWindupEndTime = 3.0f;  // Chargeの予備動作終了時刻。
    float chargeDirectionLockTime = 2.35f;  // Chargeの方向を固定するアニメーション時刻。
    float chargeSpeed = 15.0f;  // Chargeの移動速度。
    float chargeSpeedPhase2 = 18.0f;  // 第2段階のCharge移動速度。
    float chargeSafetyTimeout = 8.0f;  // Chargeの安全用タイムアウト時間。
    float chargeWallCastSafetyMargin = 0.10f;  // Chargeの壁判定に使う余白距離。
    float chargeWallFacingThreshold = 0.70f;  // Chargeの壁の向き判定に使う閾値。
    float chargeWallNormalYThreshold = 0.60f;  // Chargeの壁判定に使う法線Y成分の閾値。
    float chargePlayerCastRadiusScale = 0.88f;  // Chargeのプレイヤー判定半径の倍率。
    float chargePlayerCastRadiusScalePhase2 = 1.5f;  // 第2段階のChargeプレイヤー判定半径の倍率。
    float chargePlayerDamageForwardOffset = 0.5f;  // Chargeのプレイヤーダメージ判定の前方オフセット。
    // Charge攻撃時のジャスト回避の判定
    float chargeJustDodgeCastRadiusScale = 1.8f;  // Chargeのジャスト回避判定半径の倍率。
    float chargeJustDodgeCastRadiusScalePhase2 = 2.8f;  // 第2段階のChargeジャスト回避判定半径の倍率。
    int chargeDamagePhase2 = 10;  // 第2段階のChargeによるダメージ量。
    float chargeWallCastRadiusScale = 0.3f;  // Chargeの壁判定半径の倍率。
    float chargeStartValidationClearance = 0.05f;  // Charge開始時の経路確認に使う余白距離。

    float tripleChargeRepositionMaxDistance = 0.8f;  // 複数回Charge前の位置補正の最大距離。

    float tripleChargeRepositionSafetyMargin = 0.15f;  // 複数回Charge前の位置補正で確保する余白距離。

    float tripleChargeRepositionSpeed = 3.0f;  // 複数回Charge前の位置補正速度。
    float tripleChargeRepositionSideSafetyMargin = 0.20f;  // 複数回Charge前の位置補正で横方向に確保する余白距離。
    float tripleChargeTelegraphHoldDuration = 0.65f;  // 複数回Chargeの予兆を保持する時間。
    float tripleChargeTelegraphExpandDuration = 0.18f;  // 複数回Chargeの予兆を伸ばす時間。
    float tripleChargeTelegraphGroundOffset = 0.1f;  // 複数回Chargeの予兆の地面からのオフセット。
    float tripleChargeTelegraphFixedWorldY = 0.35f;  // 複数回Chargeの予兆に使うワールドY座標。
    float tripleChargeTelegraphForwardOffset = 0.30f;  // 複数回Chargeの予兆の前方オフセット。
    float tripleChargeTelegraphMaxDistance = 30.0f;  // 複数回Chargeの予兆の最大長さ。
    float tripleChargeTelegraphWidthMultiplier = 1.0f;  // 複数回Chargeの予兆の幅の倍率。
    bool tripleChargeTelegraphEmissiveEnabled = true;  // 複数回Chargeの予兆の発光を有効にするか。
    DirectX::XMFLOAT3 tripleChargeTelegraphPhase1EmissiveColor{ 0.05f, 0.75f, 1.0f };  // 第1段階の複数回Charge予兆の発光色。
    DirectX::XMFLOAT3 tripleChargeTelegraphPhase2EmissiveColor{ 1.0f, 0.16f, 0.03f };  // 第2段階の複数回Charge予兆の発光色。
    float tripleChargeTelegraphEmissiveIntensity = 15.0f;  // 複数回Charge予兆の発光強度。
    bool showTripleChargeTelegraph = true;  // 複数回Chargeの予兆を表示するか。
    float chargeElapsedTime = 0.0f;  // Chargeの経過時間。
    bool chargeMovementActive = false;  // Chargeの移動処理が有効か。
    bool chargeDangerWindowActive = false;  // Chargeの危険判定期間が有効か。
    bool chargeJustDodgeSuccessDebug = false;  // Chargeのジャスト回避成功を記録した診断値。
    DirectX::XMFLOAT3 chargeDirection{ 0.0f, 0.0f, 1.0f };  // Chargeの移動方向。
    bool chargeDirectionLocked = false;  // Chargeの移動方向を固定済みか。
    std::string chargePhaseDebug = "None";  // Chargeの実行段階の診断表示。
    float chargeWindupAnimationTimeDebug = 0.0f;  // Charge予備動作のアニメーション時刻の診断値。
    bool chargePlayerCastHitDebug = false;  // Chargeのプレイヤー判定がヒットしたかの診断値。
    float chargePlayerHitDistanceDebug = 0.0f;  // Chargeのプレイヤー判定でのヒット距離。
    std::string chargePlayerHitActorDebug = "None";  // Chargeのプレイヤー判定でヒットしたアクター名。
    std::string chargeSelectedHitDebug = "None";  // Chargeで選択したヒット結果の診断表示。
    bool chargeWallCastHitDebug = false;  // Chargeの壁判定がヒットしたかの診断値。
    float chargeWallFacingAmountDebug = 0.0f;  // Chargeの壁の向き判定に使った値。
    DirectX::XMFLOAT3 chargeWallHitNormalDebug{};  // Chargeでヒットした壁の法線。
    float chargeWallHitDistanceDebug = 0.0f;  // Chargeの壁判定でのヒット距離。
    DirectX::XMFLOAT3 chargeStartPositionDebug{};  // Charge開始位置の診断値。
    DirectX::XMFLOAT3 chargeCurrentPositionDebug{};  // Charge現在位置の診断値。
    float chargeTraveledDistanceDebug = 0.0f;  // Chargeの移動済み距離の診断値。
    bool chargeStartValidationValidDebug = false;  // Charge開始時の経路確認が有効かの診断値。
    float chargeStartClearanceDebug = 0.0f;  // Charge開始時の経路の余白距離の診断値。
    float chargePlayerCastRadiusDebug = 0.0f;  // Chargeのプレイヤー判定半径の診断値。
    float chargeJustDodgeCastRadiusDebug = 0.0f;  // Chargeのジャスト回避判定半径の診断値。
    float chargeWallCastRadiusDebug = 0.0f;  // Chargeの壁判定半径の診断値。
    std::string chargeStartFailureReasonDebug = "None";  // Charge開始失敗の理由。
    ChargeAttackEndReason chargeEndReasonDebug = ChargeAttackEndReason::None;  // Chargeの終了理由の診断値。
    ChargeAttackEndReason pendingChargeRecoveryResult = ChargeAttackEndReason::None;  // Recovery処理に引き継ぐChargeの終了理由。

    float stunDuration = 2.5f;  // スタン状態の継続時間。
    std::string stunPhaseDebug = "None";  // スタンの実行段階の診断表示。
    float stunElapsedDebug = 0.0f;  // スタンの経過時間の診断値。
    std::string deathAnimationName = "Death_A_0";  // 死亡時に再生するアニメーション名。
    bool cinematicDeathAnimationOwnedExternally = false;  // 死亡演出のアニメーションを外部が管理している状態。
    bool phase2CinematicAnimationOwnedExternally = false;  // 第2段階の演出アニメーションを外部が管理しているか。

    float chargePlayerHitRecoveryDuration = 0.8f;  // Chargeがプレイヤーにヒットした後のRecovery時間。
    float chargeJustDodgeRecoveryDuration = 0.7f;  // Chargeをジャスト回避された後のRecovery時間。
    float postStunRecoveryDuration = 0.1f;  // スタン後のRecovery時間。
    std::optional<float> nextRecoveryDuration;  // 次のRecoveryに使用する時間の指定。
    std::string nextRecoverySource = "Default";  // 次のRecovery時間を指定した要因。
    float currentRecoveryDurationDebug = 0.0f;  // 現在のRecovery時間の診断値。
    float recoveryElapsedDebug = 0.0f;  // Recoveryの経過時間の診断値。
    std::string recoverySourceDebug = "Default";  // 現在のRecovery時間の設定元の診断表示。

    bool isDeathPerform = false;  // 死亡演出中か。
    bool beginHuskParticleRequest = false;  // 死亡時のHuskパーティクル開始要求があるか。
    uint64_t animationDebugFrameCounter = 0;  // アニメーション要求の履歴に使うフレーム番号。
    float pitchBaseValue = 0.45f;  // 音声再生時のピッチ基準値。


    std::shared_ptr<SceneComponent> cameraTargetComponent;  // カメラが注視する位置のコンポーネント。
    float bossBattleCameraDistance = 0.0f;  // ボス戦カメラの距離調整値。
    //float bossBattleCameraRightDistance = 2.5f;
    float bossBattleCameraRightDistance = 0.0f;  // ボス戦カメラの右方向の距離調整値。
    DirectX::XMFLOAT3 bossBattleCameraOffset = { 0.0f,0.0f,0.0f };  // ボス戦カメラのオフセット。

    struct WeaponHitBoxPoints
    {
        DirectX::XMFLOAT3 root{};  // 武器の根元位置。
        DirectX::XMFLOAT3 middle{};  // 武器の中央位置。
        DirectX::XMFLOAT3 tip{};  // 武器の先端位置。
    };

    // 武器の攻撃判定に使う位置を計算する。
    WeaponHitBoxPoints BuildWeaponHitBoxPoints(
        const std::shared_ptr<SceneComponent>& root,
        const std::shared_ptr<SceneComponent>& middle,
        const std::shared_ptr<SceneComponent>& tip,
        const DirectX::XMFLOAT3& localOffset) const;

    DirectX::XMFLOAT3 prevWeaponLeftRootPos = { 0.0f,0.0f,0.0f };  // 前フレームの左武器の根元位置。
    DirectX::XMFLOAT3 prevWeaponLeftMidPos = { 0.0f,0.0f,0.0f };  // 前フレームの左武器の中央位置。
    DirectX::XMFLOAT3 prevWeaponLeftTipPos = { 0.0f,0.0f,0.0f };  // 前フレームの左武器の先端位置。

    DirectX::XMFLOAT3 prevWeaponRightRootPos = { 0.0f,0.0f,0.0f };  // 前フレームの右武器の根元位置。
    DirectX::XMFLOAT3 prevWeaponRightMidPos = { 0.0f,0.0f,0.0f };  // 前フレームの右武器の中央位置。
    DirectX::XMFLOAT3 prevWeaponRightTipPos = { 0.0f,0.0f,0.0f };  // 前フレームの右武器の先端位置。

    float hitWeaponRadius = 0.8f;  // 武器のヒット判定に使う半径。
    float activeLeftHitBoxRadius = 0.8f;  // 有効な左武器ヒットボックスの半径。
    float activeRightHitBoxRadius = 0.8f;  // 有効な右武器ヒットボックスの半径。
    DirectX::XMFLOAT3 activeLeftHitBoxOffset{};  // 有効な左武器ヒットボックスのオフセット。
    DirectX::XMFLOAT3 activeRightHitBoxOffset{};  // 有効な右武器ヒットボックスのオフセット。
    std::vector<const AnimationNotifyState*> activeHitBoxNotifyStates;  // 現在有効なヒットボックスのNotify State。
    WeaponHitBoxPoints editorPreviewLeftHitBoxPoints{};  // エディタプレビューの左武器ヒットボックス位置。
    WeaponHitBoxPoints editorPreviewRightHitBoxPoints{};  // エディタプレビューの右武器ヒットボックス位置。
    const AnimationNotifyState* editorPreviewLeftHitBoxState = nullptr;  // エディタプレビューの左武器ヒットボックスに対応するNotify State。
    const AnimationNotifyState* editorPreviewRightHitBoxState = nullptr;  // エディタプレビューの右武器ヒットボックスに対応するNotify State。
    float editorPreviewLeftHitBoxTime = -1.0f;  // 左武器ヒットボックスのプレビュー時刻。
    float editorPreviewRightHitBoxTime = -1.0f;  // 右武器ヒットボックスのプレビュー時刻。

    // 第2段階のテクスチャ切り替え範囲をワールド座標で更新する。
    void UpdatePhase2TextureBlendWorldYRange();
    float phase1BossScale = 1.7f;  // 第1段階のボスのスケール。
    float phase2BossScale = 2.1f;  // 第2段階のボスのスケール。
    float phaseTransformProgress = 0.0f;  // フェーズ変化演出の進行度。
    float phase2MaskMinYOffset = 0.0f;  // 第2段階のテクスチャ切り替え範囲の下端オフセット。
    float phase2MaskMaxYOffset = 2.6f;  // 第2段階のテクスチャ切り替え範囲の上端オフセット。
    float phase2MaskSoftness = 0.05f;  // テクスチャ切り替え境界の柔らかさ。
    float phase2WorldMinY = 0.0f;  // テクスチャ切り替え範囲のワールド座標での下端。
    float phase2WorldMaxY = 1.0f;  // テクスチャ切り替え範囲のワールド座標での上端。
    float enemyScale = 1.7f;  // ボスのスケール。
    float hitEnemyEffectOffsetY = 2.2f;
    float hitPlayerEffectOffsetY = 2.4f;

    std::shared_ptr<UIImageComponent> gruxNameImageComponent;  // ボス名表示用の画像コンポーネント。
    std::unique_ptr<EasingRunner> easingRunner;  // 表示演出の補間処理。
    float easingFactorAlpha = 0.0f;  // 表示演出の不透明度の補間係数。

    DirectX::XMFLOAT3 justDodgeAreaSize = { 0.0f,2.0f,0.0f };  // ジャスト回避判定領域の大きさ。
    DirectX::XMFLOAT3 justDodgeAreaOffset = { 0.0f,0.0f,0.0f };  // ジャスト回避判定領域のオフセット。
    DangerArea dangerArea{};  // 危険範囲の判定情報。
    struct DangerObbInitialValue
    {
        DirectX::XMFLOAT3 centerOffset{};  // 危険範囲OBBの中心オフセット。
        DirectX::XMFLOAT3 fullSize{};  // 危険範囲OBBの全体サイズ。
    };
    std::unordered_map<uint64_t, DangerObbInitialValue> initialDangerObbSettings;  // 危険範囲OBB設定の初期値。
    std::unordered_map<uint64_t, DangerObbInitialValue> savedDangerObbSettings;  // 保存済みの危険範囲OBB設定。
    std::string dangerObbSaveStatus = "Not saved this session";  // 危険範囲OBB設定の保存状態の表示文字列。
    std::string dangerObbSavePath;  // 危険範囲OBB設定の保存先。
    bool dangerObbLastSaveSucceeded = false;  // 直近の危険範囲OBB設定の保存が成功したか。
    size_t dangerObbSelectedClip = static_cast<size_t>(-1);  // 危険範囲編集で選択中のアニメーション。
    size_t dangerObbSelectedStateIndex = 0;  // 危険範囲編集で選択中のNotify Stateのインデックス。
    const AnimationNotifyState* activeDangerNotifyState = nullptr;  // 現在有効な危険範囲のNotify State。

    float damageFlashDuration = 0.20f;  // 被弾時の発光演出の時間。
    float damageFlashStartValue = 0.60f;  // 被弾時の発光演出の開始値。
    float hitVoiceCooldown = 0.50f;  // 被弾ボイスの再生間隔。
    float hitVoiceCooldownTimer = 0.0f;  // 被弾ボイスのクールダウン時間。
    int lastHitVoiceIndex = -1;  // 直近に再生した被弾ボイスのインデックス。
    float normalHitRumbleStrength = 1.2f;  // 通常攻撃被弾時の振動強度。
    float normalHitRumbleDuration = 0.135f;  // 通常攻撃被弾時の振動時間。
    float comboFinalHitRumbleStrength = 1.65f;  // コンボ最終段の被弾時の振動強度。
    float comboFinalHitRumbleDuration = 0.165f;  // コンボ最終段の被弾時の振動時間。
    float rushHitRumbleStrength = 1.4f;  // Rush被弾時の振動強度。
    float rushHitRumbleDuration = 0.125f;  // Rush被弾時の振動時間。
    float rushFinalHitRumbleStrength = 1.5f;  // Rush最終攻撃の被弾時の振動強度。
    float rushFinalHitRumbleDuration = 0.185f;  // Rush最終攻撃の被弾時の振動時間。


    std::vector<AnimationMotionWarp> animationMotionWarps;  // アニメーションに設定されたMotionWarp情報。


    std::unique_ptr<BehaviorTree>	aiTree = nullptr;  // Gruxの行動判断とアクションを構成するツリー。
    std::unique_ptr<BehaviorData>	behaviorData = nullptr;  // Sequenceの進行状態を保持するデータ。
    NodeBase* activeNode = nullptr;  // 現在実行中のBTアクションノード。
    bool behaviorTreeFastComboEnabled = true;  // 行動ツリー全体の更新を有効にする。

    bool forceBehaviorTreeCharge = false;  // Chargeを優先選択するデバッグ設定。

    CloseCombatSettings closeCombatSettings;  // FastComboの計画・実行距離と向きの条件。
    bool showCloseCombatDebugRange = false;  // 近接攻撃範囲のデバッグ表示を有効にする。
    std::string behaviorTreeCurrentNode = "None";  // 現在のBTノードの表示名。
    std::string behaviorTreePreviousNode = "None";  // 直前に実行したBTノードの表示名。
    std::string behaviorTreeLastResult = "None";  // 直近のBTアクション結果の表示名。
    std::string behaviorTreeLastJudgment = "None";  // 直近の判定結果の表示文字列。
    mutable bool debugShouldAbortChargeBT = false;  // Charge BTを中断すべきかの診断値。
    mutable bool debugAbortBehaviorTreeDisabled = false;  // BT無効による中断かの診断値。
    mutable bool debugAbortExecutionNotAllowed = false;  // 実行条件を満たさないことによる中断かの診断値。
    mutable bool debugAbortActiveNodeNull = false;  // 実行中ノードがないことによる中断かの診断値。
    mutable bool debugAbortStateMismatch = false;  // 状態の不一致による中断かの診断値。
    mutable std::string debugChargeExecutionNotAllowedReason = "None";  // Chargeを実行できない理由。
    bool debugChargeBTAbortDetected = false;  // Charge BTの中断を検出したか。
    bool debugCleanupChargeAttackBTCalled = false;  // Charge BTの後処理が呼ばれたか。
    bool debugFailChargeAttackBTCalled = false;  // Charge BTの失敗処理が呼ばれたか。
    bool debugFinishChargeRecoveryBTCalled = false;  // Charge Recoveryの終了処理が呼ばれたか。
    std::string debugLastChargeAbortReason = "None";  // 直近のCharge中断理由。
    std::string debugCurrentActiveBTNode = "None";  // 現在実行中のBTノード名。
    uint64_t debugUpdateBehaviorTreeCallCount = 0;  // BT更新処理の呼び出し回数。
    uint64_t debugExecuteChargeAttackRunCount = 0;  // Charge攻撃実行ノードの実行回数。
    uint64_t debugUpdateTripleChargeBTCallCount = 0;  // 複数回Charge BTの更新回数。
    float debugLastBehaviorTreeDeltaTime = 0.0f;  // 直近のBT更新に渡された時間差。
    uint64_t debugTelegraphHoldUpdateCount = 0;  // 予兆保持処理の更新回数。
    float debugTelegraphHoldLastDeltaTime = 0.0f;  // 予兆保持処理の直近の時間差。
    std::string debugTelegraphEntryActiveNode = "None";  // 予兆開始時の実行中ノード名。
    std::string debugTelegraphEntryInitialState = "None";  // 予兆開始時に記録した初期状態名。
    std::string debugTelegraphEntryCurrentState = "None";  // 予兆開始時の現在状態名。
    std::string debugTelegraphEntryBTLastResult = "None";  // 予兆開始時の直近のBT結果。
    uint64_t debugTelegraphEntryUpdateBTCount = 0;  // 予兆開始時のBT更新回数。
    uint64_t debugTelegraphEntryTripleUpdateCount = 0;  // 予兆開始時の複数回Charge更新回数。
    BehaviorAttackResult behaviorAttackResult = BehaviorAttackResult::None;  // 攻撃結果の保持値。
    bool behaviorApproachActive = false;  // FastComboの接近処理が有効かを示す。
    float fastComboPrepareDuration = 0.5f;  // FastComboの準備待機時間。
    float fastComboApproachMaxDuration = 5.0f;  // FastCombo接近の制限時間。
    float fastComboApproachRetryCooldown = 2.0f;  // 接近失敗後の再試行待ち時間。
    float fastComboApproachRetryRemaining = 0.0f;  // 接近失敗後の残り再試行待ち時間。
    float behaviorIdleDuration = 1.0f;  // BTの通常Idle時間。

};

