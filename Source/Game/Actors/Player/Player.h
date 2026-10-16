#pragma once
#include <memory>
#include <array>
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

    enum class JustDodgeSlowPhase : uint8_t
    {
        Inactive,
        Hold,
        Return,
        RushHold,
    };

    struct ActionRequest
    {
        ActionType type = ActionType::None; // 要求するアクションの種類
        float remainTime = 0.0f; // 入力要求を保持する残り時間
        DodgeDirection dodgeDirection = DodgeDirection::Forward; // 回避時に使用する方向
        DirectX::XMFLOAT3 dodgeWorldDirection{}; // ワールド座標で指定する回避方向
        bool useDodgeWorldDirection = false; // ワールド座標の回避方向を使用するか
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

    struct OperationGuideItemState
    {
        std::shared_ptr<UIImageComponent> image; // 操作ガイドに表示する画像
        DirectX::XMFLOAT2 position{}; // 画面上の表示位置
        DirectX::XMFLOAT2 baseScale{ 0.35f, 0.35f }; // 表示時の基準スケール
        bool visible = false; // 表示要求の有効状態
        bool learned = false; // 操作を学習済みか
        float animationTimer = 0.0f; // 点滅アニメーションの経過時間
    };

    enum class LocomotionMode :uint8_t
    {
        None,
        TPSWalk,
        TPSRun,
        LockOnBlendRun,
        Idle,
    };
public:
    // モデル名を指定してPlayerを生成する。
    explicit Player(const std::string& modelName) :Character(modelName)
    {
        mass = 50.0f;
        maxHp = 50;
        //maxHp = 3;
        hp = maxHp;
    }

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

    // アニメーションエディタのプレビュー用Notifyを処理する。
    void OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event) override;

    // アニメーションエディタで攻撃判定のプレビューを描画する。
    void DrawAnimationEditorPreviewState(const AnimationNotifyState& state) override;

    // NotifyEnd が省略されたアニメーション切替でも、前のNotify由来状態を解除する。
    void OnAnimationChanged() override;

    // 入力・移動
    // 現在の移動モードを返す。
    LocomotionMode GetLocomotionMode() const { return locomotionMode; }

    // 牢獄開始演出のPing-Pong再生方向を返す。
    bool IsPrisonOpeningSittingPingPongForward() const { return prisonOpeningSittingPingPongForward; }

    // 牢獄開始演出で立ち姿勢のPing-Pong再生を使用中かを返す。
    bool IsPrisonOpeningUsingStandingPingPong() const { return prisonOpeningUseStandingPingPong; }

    // ブレンドスペースのアニメーションを使用するかの更新関数
    // 移動モードに合わせてブレンドスペースアニメーションを更新する。
    void UpdateLocomotionAnimation() override;

    // 移動アニメーションを現在の移動モードで再開する。
    void RestartLocomotionAnimation();

    // TPSモードの移動時の更新処理
    // TPS操作時の移動入力と向きを更新する。
    void UpdateTPSLocomotion();

    // ロックオンモードの移動時の更新処理
    // ロックオン中の移動入力と向きを更新する。
    void UpdateLockOnLocomotion();

    // カメラ・演出・イベント
    // イベントシーン開始時に呼ぶ処理
    // イベント開始に合わせて操作とHUD表示を切り替える。
    void StartEvent();

    // イベントシーン終了時に呼ぶ処理
    // イベント終了に合わせて操作とHUD表示を復帰する。
    void EndEvent();

    // HPを変更せず、瀕死時の音と画面演出だけを停止する。
    void StopLowHpPresentation();

    // シネマティック中の瀕死演出の抑制状態を設定する。
    void SetLowHpPresentationCinematicSuppressed(bool suppressed);

    // 牢獄開始演出用の座り姿勢とTransform制御を開始する。
    bool BeginPrisonOpeningSitting(const DirectX::XMFLOAT3& position, float yawDegrees);

    // 牢獄開始演出の立ち上がり姿勢へ移行する。
    bool BeginPrisonOpeningStanding();

    // 牢獄開始演出の座り姿勢Ping-Pong再生条件を設定する。
    void ConfigurePrisonOpeningSittingMotion(bool useStandingPingPong, float startTime, float endTime, float speed, float standingBlendTime);

    // 牢獄開始演出の姿勢制御を終了し、入力状態を復帰する。
    void EndPrisonOpeningSitting();

    // 牢獄開始演出用の目閉じ姿勢オーバーライドを開始する。
    bool BeginPrisonOpeningEyeClosed();

    // 牢獄開始演出用の目閉じ姿勢の適用量を設定する。
    void SetPrisonOpeningEyeClosedWeight(float weight);

    // 牢獄開始演出用の目閉じ姿勢オーバーライドを解除する。
    void EndPrisonOpeningEyeOverride();

    // 牢獄開始演出用の目閉じ姿勢が有効かを返す。
    bool IsPrisonOpeningEyeOverrideActive() const;

    // 牢獄開始演出の座り状態が有効かを返す。
    bool IsPrisonOpeningSitting() const { return prisonOpeningSitting; }

    // 牢獄開始演出の立ち上がりアニメーションが完了したかを返す。
    bool IsPrisonOpeningStandingFinished() const;

    // HUD・操作ガイド
    // Scene が表示方針を決定し、Player は所有するHUDへ反映する。

    // Sceneが決定したHPバーの表示状態を反映する。
    void SetHpBarVisible(bool visible) const;
    // Sceneが決定したゲームプレイHUD全体の表示状態を反映する。
    void SetGameplayHudVisible(bool visible);
    // HUD表示方針を操作ガイドへ反映する。学習状態は変更しない。
    void SetOperationGuideHudVisible(bool visible);
    // 指定した操作ガイド項目の表示要求を設定する。
    void SetOperationGuideItemVisible(OperationGuideItem item, bool visible);
    // 指定した操作ガイド項目の学習状態を設定する。
    void SetOperationGuideItemLearned(OperationGuideItem item, bool learned = true);
    // 指定した操作ガイド項目の表示要求を返す。
    bool IsOperationGuideItemVisible(OperationGuideItem item) const;
    // 指定した操作ガイド項目が学習済みかを返す。
    bool IsOperationGuideItemLearned(OperationGuideItem item) const;
    // Dodge State中かを返す。
    bool IsDodging() const { return stateMachine_ && std::string(stateMachine_->GetStateName()) == "Dodge"; }
    // 戦闘中に適用するHP下限値を設定する。
    void SetMinimumHp(int value);
    // 戦闘中のHP下限値を解除する。
    void ClearMinimumHp();
    // HPを最大値へ回復し、HPバー回復演出を開始する。
    void RestoreHpToMaxWithUiAnimation();
    // ゲームプレイHUDのフェードアウトを開始する。
    void BeginGameplayHudFadeOut();
    // 登録済みゲームプレイHUDへ共通の透明度を適用する。
    void SetGameplayHudFadeAlpha(float alpha);

    // 戦闘遷移・最終ヒット

    // コンティニュー時のTransformと戦闘状態を初期状態へ戻す。
    void ResetForBattleContinue(const Transform& battleStartTransform);
    // 移動方向を補間せずに即時反映する。
    void ForceDirectionImmediate(const DirectX::XMFLOAT3& direction) const;
    // 死亡演出用の目閉じと発光消失を初期化する。
    void BeginDeathEyeClose();
    // 死亡経過時間に応じて目閉じ姿勢を更新する。
    void UpdateDeathEyeClose(float deathElapsedTime);
    // 死亡演出用の目閉じ姿勢オーバーライドを解除する。
    void EndDeathEyeClose();
    // 死亡経過時間に応じて発光消失演出を更新する。
    void UpdateDeathVisualFade(float deathElapsedTime);
    // 最終ヒット確定時に呼び出すコールバックを設定する。
    void SetFinalHitCallback(std::function<void(GruxEnemy*, const DirectX::XMFLOAT3&)> callback)
    {
        finalHitCallback = std::move(callback);
    }
    // 最終ヒット後の攻撃アニメーション終了待機を開始する。
    bool BeginFinalHitWait();

    // 死亡カメラ開始時に呼び出すコールバックを設定する。
    void SetDeathCameraStartCallback(std::function<void()> callback)
    {
        deathCameraStartCallback = std::move(callback);
    }
    // 死亡カメラ中の壁際Dither無効状態を設定する。
    void SetDeathCameraTransparencyDisabled(bool disabled)
    {
        deathCameraTransparencyDisabled = disabled;
    }

    // HPやTransformを維持したまま、入力と攻撃を停止する。
    void StopBattleActions();
    // フェーズ遷移イベント中に停止した戦闘操作を再開する。
    void ResumeBattleActionsAfterEvent();
    // フェーズ遷移で確認するRush State中かを返す。
    bool IsRushActiveForPhaseTransition() const
    {
        return stateMachine_ && std::string(stateMachine_->GetStateName()) == "Rush";
    }
    // Rush State中かを返す。
    bool IsRushActive() const { return IsRushActiveForPhaseTransition(); }
    // Rushの予備動作中かを返す。
    bool IsRushWindupActive() const { return rushWindupActive; }
    // Rush攻撃中かを返す。
    bool IsRushAttackActive() const { return rushAttackActive; }
    // Rush予備動作の時間を返す。
    float GetRushWindupDuration() const { return rushWindupDuration; }
    // Rush予備動作の有効状態を設定する。
    void SetRushWindupActive(bool active) { rushWindupActive = active; }
    // Rush攻撃の有効状態を設定する。
    void SetRushAttackActive(bool active) { rushAttackActive = active; }
    // Just Dodge成功演出中かを返す。
    bool IsJustDodgePresentationActive() const
    {
        return stateMachine_ && std::string(stateMachine_->GetStateName()) == "Dodge" && justDodgeSuccess;
    }
    // 現在のRush対象を返す。
    std::shared_ptr<Enemy> GetRushTarget() const { return rushTarget.lock(); }

    // 最終ヒット時にRushの追撃停止を要求する。
    void RequestFinalHitRushFollowUpStop()
    {
        if (IsRushActiveForPhaseTransition())
            finalHitRushFollowUpStopRequested = true;
    }
    // 最終ヒット時に現在の見た目姿勢の固定を要求する。
    void RequestFinalHitVisualPoseLatch()
    {
        finalHitVisualPoseLatchRequested = true;
    }
    // 見た目姿勢固定の要求中かを返す。
    bool IsFinalHitVisualPoseLatchRequested() const
    {
        return finalHitVisualPoseLatchRequested;
    }
    // Phase1最終攻撃が遷移可能な姿勢で完了したかを返す。
    bool IsPhase1LastHitAttackFinishedForTransition() const
    {
        const auto controller = GetBodyAnimationController();
        if (!controller || !controller->IsVisualPoseLatched() || !stateMachine_)
            return false;
        const std::string stateName = stateMachine_->GetStateName();
        return stateName != "Attack" && stateName != "Rush";
    }
    // Rush追撃停止の要求中かを返す。
    bool IsFinalHitRushFollowUpStopRequested() const
    {
        return finalHitRushFollowUpStopRequested;
    }
    // Rush追撃停止の要求を解除する。
    void ClearFinalHitRushFollowUpStop()
    {
        finalHitRushFollowUpStopRequested = false;
    }
    // 最終ヒット後の攻撃アニメーション排出が完了したかを返す。
    bool IsFinalHitAttackDrainComplete() const;
    // 最終ヒット後の攻撃アニメーション排出中かを返す。
    bool IsFinalHitAttackDrainActive() const { return finalHitAttackDrainActive; }
    // 最終ヒット姿勢の固定に成功したかを返す。
    bool IsFinalHitAttackDrainLatched() const { return finalHitAttackDrainCompleted; }
    // Phase2最終Rushの診断イベントIDを有効または無効にする。
    void SetPhase2RushFinalHitDebugEnabled(bool enabled)
    {
        if (!enabled) phase2RushFinalHitDebugEventId = 0;
    }

    // Phase2最終Rushの診断イベントIDを設定する。
    void SetPhase2RushFinalHitDebugEventId(uint64_t eventId) { phase2RushFinalHitDebugEventId = eventId; }
    // Phase2最終Rushの診断イベントIDを返す。
    uint64_t GetPhase2RushFinalHitDebugEventId() const { return phase2RushFinalHitDebugEventId; }
    // 最終ヒット後の攻撃アニメーション排出を中断として記録する。
    void MarkFinalHitAttackDrainInterrupted();

    // HP、Transform、現在Stateを維持して一時的な戦闘状態を解除する。
    void ClearTransientBattleActions();
    // フェーズ遷移前に戦闘表示とRushガイドを解除する。
    void ClearBattleVisualsForPhaseTransition();

    // シネマティックでTransformを変更する前にゲームプレイ操作を終了する。
    void CancelGameplayActionsForCinematic();

    // Phase2演出開始前にPlayerを非移動状態へ整える。
    void NeutralizeForPhase2Cinematic();

    // シネマティック移動前に剣軌跡の履歴を消去する。
    void ClearSwordTrailForPhaseTransition();
    // フェーズ遷移前に剣軌跡を消去すべき状態かを返す。
    bool IsSwordTrailActiveForPhaseTransition() const
    {
        return showTrail || !trail.trailPoints.empty();
    }
    // ロックオンガイドが表示中かを返す。
    bool IsLockOnGuideVisible() const { return lockOnGuideVisible; }

    // ボス撃破後の勝利Stateへ遷移する。
    void EnterWinState();
    // 勝利State中かを返す。
    bool IsInWinState() const;

    // 描画・残像演出
    // 軌跡を描画する処理
    // 剣の軌跡を描画する。
    void RenderTrail(ID3D11DeviceContext* immediateContext);
    // Just Dodge成功時のPlayer残像生成を開始する。
    void CapturePlayerPoseGhost();
    // Player残像の表示状態と生成状態をリセットする。
    void ResetPlayerPoseGhost();
    // カメラ連携
    // Focus開始時のForwardを設定する
    // Focus開始時に使用するPlayerの正面方向を保存する。
    void SetFocusDirection(const DirectX::XMFLOAT3& focusDir)
    {
        this->focusDirection = focusDir;
    }

    // 回避方向を取得する
    // 現在の回避方向を返す。
    DodgeDirection GetDodgeDirection()const { return dodgeDirection; }
    // ワールド座標で指定された回避方向を返す。
    const DirectX::XMFLOAT3& GetDodgeWorldDirection() const { return dodgeWorldDirection; }
    // ワールド座標の回避方向を使用するかを返す。
    bool UsesDodgeWorldDirection() const { return useDodgeWorldDirection; }
    // 回避開始時の方向をワールド座標で固定する。
    void CaptureDodgeWorldDirectionSnapshot();

    // 演出中のPlayer透明化抑制状態を設定する。
    void SetIsPlayerTransparency(const bool isTransparency) { moviePerform = !isTransparency; }

    // モード変更用関数
    // 移動モードを切り替え、対応する移動アニメーションを更新する。
    void SetLocomotionMode(LocomotionMode mode);

    // 入力要求・移動
    // 保存済みの入力要求を現在の状態で実行可能なState遷移へ変換し、成功時だけ消費する。
    // 入力コマンドによってステートが変わるかどうか
    // 保存済み入力を現在のStateで実行可能な遷移へ変換し、成功時だけ消費する。
    bool TryExecuteActionRequest();

    // 入力を消費する処理
    // 指定した種類の入力要求を消費する。
    void ConsumeActionRequest(ActionType expectedType);

    // 戦闘・攻撃判定
    // 無敵判定を含め、成立した被弾だけを適用する
    // 無敵状態を考慮して成立した被弾だけを適用する。
    bool TryTakeDamage(int damage, const DirectX::XMFLOAT3& attackerPosition,
        DamagePolicy policy = DamagePolicy::Normal);

    // ノックバック可能な状態かを確認し、ノックバックStateへ遷移する。
    bool StartKnockBack(const DirectX::XMFLOAT3& direction);

    // Bossが新しい攻撃を開始できる状態かを返す。
    // 起き上がり後の猶予時間もこの判定に含める。
    // ボスが新たな攻撃対象として選べる状態かを返す。
    bool IsBossAttackTargetAvailable() const;

    // 保持中の入力要求を破棄し、破棄理由を記録する。
    void ClearActionRequest(const char* reason);

    // 被弾リアクションの方向を返す。
    HitReactionDirection GetHitReactionDirection() const { return hitReactionDirection; }

    // 攻撃開始時の処理
    // 攻撃開始時の共通状態を初期化する。
    void StartAttack();

    // ジャスト回避・Rush
    // ジャスト回避成功時の処理　ラッシュ受付期間開始
    // Just Dodge成功後のスロー演出とRush入力受付を開始する。
    void StartJustDodgeSuccess(const std::shared_ptr<Enemy>& enemy);

    // Just Dodge後の初回Rush入力を受け付けられるかを返す。
    bool CanAcceptInitialRushInput() const;
    // 最終ヒット時の現在姿勢を固定し、成否を記録する。
    bool LatchFinalHitVisualPoseAfterAnimationUpdate();
    // Just Dodge後の初回Rush入力ガイドを表示できるかを返す。
    bool CanShowInitialRushGuide() const;
    // Rush中の追加入力ガイドを表示できるかを返す。
    bool CanShowRushComboGuide() const;
    // 初回または追加入力のRushガイドを表示できるかを返す。
    bool CanShowRushPrompt() const;
    // Just Dodge成功後またはRush中の追撃機会が有効かを返す。
    bool IsRushOpportunityActive() const;
    // 初回Rush入力の受付状態を切り替え、ガイド表示を同期する。
    void SetRushInputAcceptance(bool accepting, const char* endReason = nullptr);
    // Rush判定と入力要求の診断状態を更新する。
    void SetRushInputDebugState(bool judgeSuccess, bool rushRequested);
    // 初回Rush入力受付の時刻と状態を診断ログへ出力する。
    void LogInitialRushInputTrace(const char* event, const char* detail = nullptr);

    // Player/Bossの解除タイミングを独立して管理する。
    // PlayerのJust Dodgeスローを通常速度へ戻し始める。
    void BeginPlayerSlowReturn();
    // BossのJust DodgeまたはRushスローを通常速度へ戻し始める。
    void BeginBossSlowReturn(bool afterRush = false);
    // Rush開始までBossのスロー状態を維持する。
    void HoldBossSlowForRush();
    // Playerのスロー状態を即時に通常速度へ戻す。
    void ForceResetPlayerSlow();
    // Bossのスロー状態を即時に通常速度へ戻す。
    void ForceResetBossSlow();
    // Rush状態に合わせて剣、軌跡、残像の色を更新する。
    void SetRushWeaponVisual(bool enabled);
    // 有効範囲に収めたRush最大攻撃回数を返す。
    int GetMaxRushAttackCount() const { return std::clamp(maxRushAttackCount, 1, 7); }

    // ジャスト回避を受け付けるかどうか
    // Just Dodgeの判定受付中かを返す。
    bool GetJustDodgeWindow()const { return  justDodgeWindow; }

    // 回避開始時の方向と攻撃者方向からJust Dodge成立可否を判定する。
    bool CanJustDodgeAgainst(const DirectX::XMFLOAT3& attackerPosition);
    // 回避判定の診断情報を初期化する。
    void BeginDodgeDebug();
    // 回避判定の診断経過時間を更新する。
    void UpdateDodgeDebug(float deltaTime);
    // 通常回避の診断結果を記録する。
    void RecordNormalDodgeDebug();
    // カメラ・ボス戦連携

    // カメラの注視点の位置
    // カメラ注視点用コンポーネントを返す。
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }

    // 攻撃補助に使用する最寄りの有効な敵を取得する。
    void AcquireAttackTarget();
    // 攻撃対象へ角度制限付きで向きを補正する。
    void UpdateAttackTargetRotation(float deltaTime);
    // 攻撃対象への向き補正を停止する。
    void StopAttackTargetRotation() { attackRotationTracking = false; }
    // 攻撃補助対象と向き補正状態を解除する。
    void ClearAttackTarget();

    // ボス戦時かどうかを設定する
    // ボス戦中かを設定する。
    void SetIsBossBattle(const bool isBossBattle) { this->isBossBattle = isBossBattle; }
    // ボス戦中かを返す。
    bool IsBossBattle() const { return isBossBattle; }

private:
    // 演出用の姿勢・Notify補助
    // 現在の目閉じ設定で姿勢オーバーライドを再構築する。
    bool RebuildEyeClosePoseOverride();
    // 目閉じ姿勢オーバーライドと関連状態を初期化する。
    void ResetEyeCloseOverride();

    // SE再生Notifyを解釈して対応する効果音を再生する。
    void HandleAnimationPlaySE(const AnimationNotifyEvent& event) const;

    // 武器コンポーネントのローカルオフセットを反映した攻撃判定位置を求める。
    DirectX::XMFLOAT3 GetHitBoxPoint(const std::shared_ptr<SceneComponent>& component, const DirectX::XMFLOAT3& localOffset) const;

    // 入力要求の内部処理
    // 入力処理をまとめる
    // 入力を一定時間保持するアクション要求として記録する。
    void CaptureActionRequest(float deltaTime);

    // 優先度を比較して入力バッファへアクション要求を保存する。
    bool StoreActionRequest(ActionType type, float remainTime);

    // ノックバック・移動更新
    // ノックバック移動を開始する。
    void BeginKnockBackMovement();
    // ノックバック中の強制移動を更新する。
    void UpdateKnockBackMovement(float deltaTime);
    // ノックバックの強制移動だけを停止する。
    void StopKnockBackForcedMove();
    // ノックバック移動の状態を解除する。
    void EndKnockBackMovement();

    // 動作更新処理
    // 入力とStateに応じた通常移動を更新する。
    void UpdateMovement();
    // Notify由来の剣の軌跡、残像、発光を更新する。
    void UpdateWeaponVisualPresentation(float deltaTime);

    // HUD・操作ガイドの内部更新
    // Rush入力受付状態に合わせてガイドUIを更新する。
    void UpdateRushPromptUI();
    // ロックオン可能状態に合わせてガイドUIを更新する。
    void UpdateLockOnGuideUI();
    // ロックオンガイドUIを非表示にして表示状態を初期化する。
    void HideAndResetLockOnGuideUI();
    // 操作ガイドUIの画像と初期配置を生成する。
    void InitializeOperationGuideUI();
    // 操作ガイドUIの点滅と表示状態を更新する。
    void UpdateOperationGuideUI();
    // Hpバーの更新
    void UpdateHpBarUI();

    // HUD、イベント、HP、項目状態から操作ガイドの表示を反映する。
    void RefreshOperationGuideUIVisibility() const;

    // 戦闘計算・回避方向の補助
    // 現在のRush段階に応じたダメージ倍率を返す。
    float GetRushDamageMultiplier() const;
    // 現在のStateに応じた攻撃ダメージを返す。
    int GetCurrentAttackDamage() const;

    // 回避の方向を決定する処理
    // ロックオン中の移動入力から回避方向を決定する。
    void DecideLockOnDodgeDirection();

    // インタラクト対象検索
    IInteractable* FindInteractable() const;

    // HP状態に応じて心拍、発光、ビネットを更新する。
    void UpdateLowHpEffects();
    // 瀕死時の心拍SEと脈動演出を開始する。
    void TriggerLowHpPulse();
    // 瀕死演出のタイマー、発光、ビネット表示を初期化する。
    void ResetLowHpEffects();

public:
    // ヒット中に当たった敵を記録する
    std::unordered_set<Actor*> hitActors; // 現在の攻撃で既に命中したActor

    std::string currentAttackAnimation = "Primary_Attack_Fast_A"; // 再生中または次に再生する攻撃アニメーション名
    std::string startAttackAnimation = "Primary_Attack_Fast_A";    // コンボ開始のアニメーション

    bool comboQueued = false;   // コンボ攻撃がキューに入っているかどうか
    bool inputWindow = false;   // コンボ受付をするかどうか
    bool hitBox = false;   // 武器の当たり判定をつける
    float activeHitBoxRadius = 0.75f; // Notifyで有効化中の武器判定半径
    DirectX::XMFLOAT3 activeHitBoxOffset{ 0.0f, 0.0f, 0.0f }; // 武器判定に加えるローカルオフセット
    bool transitionWindow = false;  // ステート遷移してもいいかどうか
    bool justDodgeWindow = false;  // ジャスト回避受付時間
    bool justDodgeSuccess = false; // ジャスト回避成功フラグ
    bool showTrail = false;     // 軌跡を出現させるかどうか
    float swordEmissivePower = 0.0f;    // 剣のエミッシブの力

    // アニメーション時にどれくらい移動するか
    std::vector<AnimationMotionWarp> animationMotionWarps; // 有効中のMotionWarp Notifyごとの移動情報
    // 操作UI
    // HUD全体を非表示にしても、操作ガイドの学習状態を保持する。
    std::array<OperationGuideItemState,
        static_cast<size_t>(OperationGuideItem::Count)> operationGuideItems;
    bool operationGuideGameplayHudVisible = true; // Gameplay HUD側が許可する操作ガイド表示状態
    bool operationGuideEventVisible = true; // イベント側が許可する操作ガイド表示状態
    float operationGuidePulseAmplitude = 0.185f; // 操作ガイドの拡縮量
    float operationGuidePulseSpeed = 3.5f; // radians / second


    bool invincibleWindow = false; // アニメーションによる無敵状態かどうか
    bool invincible = false; // 無敵状態かどうか


    float justDodgeMaxTowardAttackerDot = 0.25f; // 回避方向と攻撃者方向の成立判定しきい値
    DirectX::XMFLOAT3 dodgeWorldDirectionSnapshot{}; // 回避開始時に固定したワールド方向
    bool dodgeWorldDirectionSnapshotValid = false; // 回避方向スナップショットが有効か
    DirectX::XMFLOAT3 directionToAttackerDebug{}; // 診断用に保存する攻撃者方向
    float justDodgeDirectionDotDebug = 0.0f; // 診断用の回避方向と攻撃者方向の内積
    bool justDodgeDirectionResultDebug = false; // 診断用のJust Dodge方向判定結果
    float dodgeDebugElapsed = 0.0f; // 回避判定診断の経過時間
    bool normalDodgeRecordedThisDodge = false; // 今回の回避を通常回避として記録済みか
    bool justDodgeRecordedThisDodge = false; // 今回の回避をJust Dodgeとして記録済みか
    bool lastJustDodgeValid = false; // 直近のJust Dodge判定が有効だったか
    std::string lastJustDodgeAttack = "None"; // 直近のJust Dodge対象攻撃名
    float lastJustDodgeTime = 0.0f; // 直近のJust Dodge成立時刻
    float lastJustAnimationTime = 0.0f; // Just Dodge判定時のアニメーション時刻
    float lastJustWindowStart = 0.0f; // Just Dodge判定区間の開始時刻
    float lastJustWindowEnd = 0.0f; // Just Dodge判定区間の終了時刻
    float lastJustWindowRatio = 0.0f; // Just Dodge成立位置の判定区間内比率
    float lastJustBossHitBoxElapsed = -1.0f; // 直近のBoss攻撃判定開始からの経過時間
    float moveToEnemyInterval = 0.2f;  // ラッシュ後の敵までへのダッシュにかかる時間
    float rushApproachPoseBlendTime = 0.12f; // Rush接近姿勢へ切り替えるブレンド時間
    float rushWindupDuration = 0.20f; // Rush接近前の予備動作時間
    bool rushWindupActive = false; // Rush予備動作中か
    bool rushAttackActive = false; // Rush攻撃中か

    // 通常攻撃でどれくらい攻撃を補正するか
    float normalEnemyRushSurfaceMargin = 0.15f; // 通常敵へのRush停止距離に加える余白
    float motionWarpDesiredAttackSurfaceDistance = 0.5f; // MotionWarp攻撃時に保つ敵表面までの距離
    float attackRotationMaxCorrectionDegrees = 55.0f; // 攻撃対象へ向く補正角度の上限
    float attackRotationSpeedDegrees = 240.0f; // 攻撃対象へ向く補正速度（度/秒）
    float attackAssistMaxDistance = 8.0f; // 攻撃補助対象を探す最大距離
    std::weak_ptr<Enemy> attackTarget; // 攻撃補助で向きを補正する対象
    float attackRotationStartYaw = 0.0f; // 攻撃開始時のYaw角
    bool attackRotationTracking = false; // 攻撃対象への向き補正中か

    std::weak_ptr<Enemy> rushTarget; // ターゲットを選択

    enum class RushPromptAnimationPhase : uint8_t
    {
        Hidden,
        AppearGrow,
        AppearSettle,
        PulseGrow,
        PulseReturn,
    };

    bool rushInputAccepting = false; // 初回Rush入力を受け付ける状態
    // Just Dodgeから初回Rush入力までを計測する診断用状態。
    bool initialRushInputTraceActive = false; // 初回Rush入力受付を計測する診断ログの有効状態
    uint64_t initialRushInputTraceFrame = 0; // 診断ログ用のフレーム番号
    float initialRushInputTraceGameTime = 0.0f; // 診断ログ用のゲーム時間
    float initialRushInputTraceUnscaledTime = 0.0f; // 診断ログ用の非スケール時間
    float initialRushInputTraceStartGameTime = 0.0f; // 初回Rush入力受付開始時のゲーム時間
    float initialRushInputTraceStartUnscaledTime = 0.0f; // 初回Rush入力受付開始時の非スケール時間
    bool finalHitRushFollowUpStopRequested = false; // 最終ヒット時にRush追撃を停止する要求
    bool finalHitVisualPoseLatchRequested = false; // 最終ヒット時に見た目姿勢を固定する要求
    bool finalHitAttackDrainActive = false; // 最終ヒット後の攻撃アニメーション排出中か
    bool finalHitAttackDrainCompleted = false; // 最終ヒット姿勢の固定に成功したか
    bool finalHitAttackDrainFailed = false; // 最終ヒット姿勢の固定または排出に失敗したか
    uint64_t phase2RushFinalHitDebugEventId = 0; // Phase2最終Rushの診断イベントID
    bool rushInputEndNotifyReceivedDebug = false; // rush_input_end Notify受信の診断状態
    std::string rushInputEndReasonDebug = "None"; // Rush入力受付を終了した診断上の理由
    bool rushJudgeSuccessDebug = false; // Rush判定成功の診断状態
    bool rushRequestedDebug = false; // Rush入力要求の診断状態
    float rushPromptAlpha = 0.0f; // Rush入力ガイドに適用する透明度
    float rushPromptFadeInDuration = 0.10f; // Rush入力ガイドのフェードイン時間
    RushPromptAnimationPhase rushPromptAnimationPhase = RushPromptAnimationPhase::Hidden; // Rush入力ガイドの表示アニメーション段階
    float rushPromptAnimationTimer = 0.0f; // Rush入力ガイドのアニメーション経過時間
    bool rushPromptWasVisible = false; // 前フレームでRush入力ガイドを表示していたか

    // ラッシュのUIの数値
    DirectX::XMFLOAT2 rushGuidePosition = { 1200.0f, 680.0f }; // Rushガイド画像の表示位置
    DirectX::XMFLOAT2 rushButtonPosition = { 1200.0f, 600.0f }; // Rushボタン画像の表示位置
    DirectX::XMFLOAT2 rushWordPosition = { 1197.0f, 532.0f }; // Rushテキスト画像の表示位置
    DirectX::XMFLOAT2 rushGuideSize = { 488.0f, 379.f }; // Rushガイド画像の基準サイズ
    DirectX::XMFLOAT2 rushButtonSize = { 300.0f, 300.0f }; // Rushボタン画像の基準サイズ
    DirectX::XMFLOAT2 rushWordSize = { 124.5f, 68.0f }; // Rushテキスト画像の基準サイズ
    DirectX::XMFLOAT2 rushGuideScale = { 0.32f, 0.32f }; // Rushガイド画像の表示スケール
    DirectX::XMFLOAT2 rushButtonBaseScale = { 0.32f, 0.32f }; // Rushボタン画像の基準スケール
    DirectX::XMFLOAT2 rushWordScale = { 1.0f, 1.0f }; // Rushテキスト画像の表示スケール

    // 入力受付のコマンド
    ActionRequest bufferCommand{}; // 入力コマンド

    // Rush戦闘の調整値。
    int normalAttackDamage = 2; // 通常攻撃の基本ダメージ
    float rushDamageMultiplier = 1.f; // Rush攻撃に適用するダメージ倍率
    float finalRushDamageMultiplier = 2.5f; // 最終Rush攻撃に適用するダメージ倍率
    int maxRushAttackCount = 7; // Rush連撃の最大攻撃回数

    float normalAttackHitStopDuration = 0.05f; // ヒットストップの秒数
    float rushHitStopDuration = 0.03f; // Rush攻撃のヒットストップ時間

    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent; // Player本体を描画するスケルタルメッシュ
    std::shared_ptr<InputComponent> inputComponent; // Playerの入力意図を保持するコンポーネント
    std::shared_ptr<RotationComponent> rotationComponent; // Playerの向きを制御するコンポーネント
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent; // Playerの移動を制御するコンポーネント

    std::shared_ptr<SceneComponent> swordRootComponent; // 剣の根元のコンポーネント
    std::shared_ptr<SceneComponent> swordMiddleComponent; // 剣の中間のコンポーネント
    std::shared_ptr<SceneComponent> swordTipComponent;  // 剣の先端のコンポーネント

    // 剣の描画用メッシュコンポーネント
    std::shared_ptr<SkeletalMeshComponent> swordMeshComponent; // 剣を描画するスケルタルメッシュ

    // 剣の残像用
    struct SwordGhost
    {
        std::shared_ptr<SkeletalMeshComponent> swordMeshComp; // 剣残像を描画するスケルタルメッシュ
        DirectX::XMFLOAT4X4 world = { }; // 残像を描画するワールド行列
        float alpha; // 残像の透明度
        bool isVisible = false; // 残像を描画するか
    };
    std::array<SwordGhost, 8> ghosts; // 循環利用する剣残像

    // Just Dodge成立時のPlayer固定Pose Ghost
    struct PlayerPoseGhost
    {
        std::shared_ptr<SkeletalMeshComponent> renderConstantsComponent; // Player残像の描画パラメータを持つメッシュ
        std::vector<InterleavedGltfModel::Node> nodes; // 残像に複製するスケルトンノード姿勢
        DirectX::XMFLOAT4X4 world = {}; // Player残像を描画するワールド行列
        float alpha = 0.0f; // Player残像の透明度
        float elapsedTime = 0.0f; // Player残像の表示経過時間
        bool isVisible = false; // 残像を描画するか
    };
    std::array<PlayerPoseGhost, 6> playerPoseGhosts; // 循環利用するPlayer残像

    // 歩きのスピード
    float walkSpeed = 1.0f;
    // 走りのスピード
    float runSpeed = 5.15f;
    float forwardSpeedScale = 1.0f; // 前方移動に適用する速度倍率
    float sideSpeedScale = 0.95f; // 横移動に適用する速度倍率
    float backwardSpeedScale = 1.0f; // 後方移動に適用する速度倍率

    // ラッシュ時のUI
    std::shared_ptr<UIImageComponent> rushGuideImageComponent; // Rush入力を案内する画像
    std::shared_ptr<UIImageComponent> rushButtonImageComponent; // Rush入力ボタンを示す画像
    std::shared_ptr<UIImageComponent> rushWordImageComponent; // Rush開始を示すテキスト画像
    std::shared_ptr<UIImageComponent> lockOnGuideArrowImageComponent; // ロックオン対象を示す矢印画像
    std::shared_ptr<UIImageComponent> lockOnGuideButtonImageComponent; // ロックオン操作を示すボタン画像

    // ロックオンのガイドUIの調整値
    float lockOnGuideOffscreenElapsed = 0.0f; // ロックオン対象が画面外にある経過時間
    float lockOnGuidePulseElapsed = 0.0f; // ロックオンガイドの点滅経過時間
    bool lockOnGuideVisible = false; // ロックオンガイドの表示状態
    float lockOnGuideDelay = 3.0f; // ロックオンガイドを表示するまでの待機時間
    float lockOnGuidePulsePeriod = 1.2f; // ロックオンガイドの点滅周期
    float lockOnGuidePulseMinScale = 0.92f; // 点滅時の最小スケール
    DirectX::XMFLOAT2 lockOnGuideArrowSize{ 96.0f, 64.0f }; // 矢印画像の基準サイズ
    DirectX::XMFLOAT2 lockOnGuideArrowBaseScale{ 0.5f, 0.7f }; // 矢印画像の基準スケール
    DirectX::XMFLOAT2 lockOnGuideButtonSize{ 96.0f, 96.0f }; // ボタン画像の基準サイズ
    DirectX::XMFLOAT2 lockOnGuideButtonBaseScale{ 1.0f, 1.0f }; // ボタン画像の基準スケール
    float lockOnGuideButtonOffset = 75.0f; // 矢印からボタン画像までの距離
    float lockOnGuideEdgeMargin = 130.0f; // 画面端から矢印を離す余白
    float lockOnGuideArrowRotationOffset = 180.0f; // 矢印画像の回転補正角度


private:
    DirectX::XMFLOAT3 knockBackDirection{ 0.0f, 0.0f, 1.0f }; // ノックバック移動の方向
    float knockBackInitialSpeed = 8.0f; // ノックバック開始時の速度
    float knockBackDuration = 0.45f; // ノックバック移動の継続時間
    float knockBackElapsed = 0.0f; // ノックバック移動の経過時間
    bool knockBackActive = false; // ノックバック状態が有効か
    bool knockBackForcedMoveActive = false; // ノックバックの強制移動中か


    int lowHpThreshold = 13;    // 瀕死判定HP
    bool lowHpActive = false; // 瀕死演出が有効か
    bool lowHpPresentationSuppressed = false; // 明示的に瀕死演出を停止しているか
    bool lowHpPresentationCinematicSuppressed = false; // シネマティック中に瀕死演出を停止しているか
    float heartbeatTimer = 0.0f; // 次の心拍SEまでの経過時間
    float heartbeatInterval = 3.0f; // 心拍SEを鳴らす間隔
    float lowHpPulseTimer = 0.0f; // 瀕死脈動演出の残り時間
    float lowHpPulseDuration = 0.4f; // 瀕死脈動演出の継続時間
    float lowHpPulseFlashAmount = 0.0f; // 瀕死脈動による発光強度
    float lowHpVignetteBaseAlpha = 0.12f; // 瀕死ビネットの通常透明度
    float lowHpVignettePulseAlpha = 0.32f; // 瀕死脈動時のビネット最大透明度
    std::string heartbeatSEPath = "./Data/Sound/SE/player_heartbeat.wav"; // 瀕死時に再生する心拍SEのパス
    std::shared_ptr<UIImageComponent> lowHpVignetteImageComponent; // 瀕死時に表示するビネットUI

    // 被ダメージ発光のタイマーを開始する。
    void StartDamageFlash();
    // Player本体へ被ダメージ発光量を反映する。
    void ApplyDamageFlash(float flashAmount) const;
    // 被ダメージ発光の減衰と瀕死発光との合成を更新する。
    void UpdateDamageFlash();


    float damageFlashTimer = 0.0f; // 被ダメージ発光の残り時間
    float damageFlashDuration = 0.25f; // 被ダメージ発光の継続時間
    float damageFlashBodyTintStrength = 0.5f; // 被ダメージ時の本体色強度
    float damageFlashRimStrength = 1.0f; // 被ダメージ時のリム光強度
    DirectX::XMFLOAT3 damageFlashColor = { 0.85f, 0.08f, 0.05f }; // 被ダメージ時の本体発光色

    // 目を閉じる処理の変数
    float closedEyePoseTime = 4.045f; // 目閉じ姿勢を取得するIdleアニメーション時刻
    enum class EyeCloseOverrideOwner
    {
        None,
        ManualPreview,
        PrisonOpening,
        Death,
    };

    float closeEyeWeight = 0.0f; // 目閉じ姿勢オーバーライドの適用量
    EyeCloseOverrideOwner eyeCloseOverrideOwner = EyeCloseOverrideOwner::None; // 目閉じ姿勢オーバーライドの所有者
    bool deathEyeCloseActive = false; // 死亡演出用の目閉じが有効か
    float deathEyeCloseDelay = 2.506f; // 死亡後に目閉じを開始するまでの時間
    float deathEyeCloseDuration = 1.4f; // 目閉じ姿勢を適用する時間
    float deathColorFadeDelay = 4.018f; // 死亡後に発光消失を開始するまでの時間
    float deathColorFadeDuration = 2.05f; // 死亡時の発光消失時間
    float deathVisualFade = 0.0f; // 死亡時の発光消失率
    bool deathEyeCloseStarted = false; // 死亡演出で目閉じを開始済みか
    bool deathVisualFadeStarted = false; // 死亡演出で発光消失SEを再生済みか
    DirectX::XMFLOAT3 deathDeadEmissiveColor = { 0.35f, 0.35f, 0.35f }; // 死亡時に残す発光色
    float playerAliveEmissionPower = 20.9f; // 生存時のPlayer本体発光強度


    DirectX::XMFLOAT3 prevSwordRootPos = { 0.0f,0.0f,0.0f }; // 前フレームの剣根元位置
    DirectX::XMFLOAT3 prevSwordMidPos = { 0.0f,0.0f,0.0f }; // 前フレームの剣中央位置
    DirectX::XMFLOAT3 prevSwordTipPos = { 0.0f,0.0f,0.0f }; // 前フレームの剣先端位置

    // 剣の残像の調整値
    float swordGhostElapsedTime = 0.0f; // 次の剣残像を生成するまでの経過時間
    int swordGhostIndex = 0; // 次に再利用する剣残像の添字
    float ghostInterval = 0.015f; // 残像を出す間隔
    float ghostFadeTime = 0.6f; // 残像が残る時間
    float swordGhostEmissive = 2.0f;    // 残像のemissiveColor
    bool battleActionsSuspended = false; // フェーズ遷移中にゲームプレイ処理を停止する状態
    DirectX::XMFLOAT3 swordGhostColor = { 0.0f,0.042f,0.253f };  // 残像の剣のベースカラー
    DirectX::XMFLOAT3 ghostInnerColor = { 1.0f,1.0f,1.0f }; // 剣残像の内側色
    DirectX::XMFLOAT3 rushSwordColor = { 0.5f,0.8f,1.6f }; // Rush中の剣、軌跡、残像に使用する色
    DirectX::XMFLOAT3 activeGhostBaseColor{}; // 通常またはRush状態を反映した剣残像の現在色
    DirectX::XMFLOAT3 activeGhostEdgeColor{}; // 通常またはRush状態を反映した剣残像輪郭の現在色
    float ghostEdgeWidth = 1.0f; // 残像の輪郭
    DirectX::XMFLOAT4X4 prevSwordWorld; // 前回の姿勢
    bool isPrevSwordWorldValid = false; // 前フレームの剣行列が有効か

    // playerのジャスト回避の残像の調整値
    float playerPoseGhostInitialAlpha = 0.4f;   // プレイヤーの残像の初回の透明度
    float playerPoseGhostLifetime = 0.81f;      // プレイヤーの残像のライフタイム
    float playerGhostSpawnInterval = 0.25f; // Player残像を生成する間隔
    DirectX::XMFLOAT3 playerPoseGhostColor = { 0.5f, 0.8f, 1.6f }; // Player残像の基準色
    float playerPoseGhostEmissive = 2.0f; // Player残像の発光強度
    DirectX::XMFLOAT3 playerPoseGhostEdgeColor = { 0.973f, 0.911f, 0.443f }; // Player残像の輪郭色
    DirectX::XMFLOAT3 playerPoseGhostInnerColor = { 1.0f, 1.0f, 1.0f }; // Player残像の内側色
    float playerPoseGhostEdgeWidth = 1.0f; // Player残像の輪郭幅
    float playerPoseGhostSpawnTimer = 0.0f; // 次のPlayer残像を生成するまでの経過時間
    size_t nextPlayerPoseGhostIndex = 0; // 次に再利用するPlayer残像の添字
    bool playerPoseGhostSpawnSequenceActive = false; // Player残像の連続生成中か

    // Player残像の生成、透明度、寿命を更新する。
    void UpdatePlayerPoseGhost();
    // Player本体と剣へDither透明度を反映する。
    void ApplyDitherTransparencyAlpha(float alpha) const;
    // 指定した残像スロットへ現在のPlayer姿勢を複製する。
    bool CapturePlayerPoseGhost(size_t ghostIndex);

    float weaponSphereRadius = 0.75f;    // 剣の球の当たり判定の半径

    // 剣の軌跡
    Trail trail;
    float trailRemainTime = 0.8f; // 剣の軌跡を保持する時間

    std::shared_ptr<UIGaugeFillComponent> hpDelayedFillUiComponent; // 遅延HPバーを描画するUI
    std::shared_ptr<UIGaugeFillComponent> hpCurrentFillUiComponent; // 現在HPバーを描画するUI
    std::vector<std::shared_ptr<UICoreComponent>> hpBarUiComponents; // HPバーを構成するUI一覧
    struct GameplayHudFadeEntry
    {
        std::shared_ptr<UIImageComponent> component; // フェード対象のUI画像
        CoreColor color; // フェード前に復元するUI色
    };
    std::vector<GameplayHudFadeEntry> gameplayHudFadeEntries; // 共通フェードを適用するHUD画像と元の色
    float delayedHp = 0.0f; // 遅延HPバーが表示するHP値
    float delayedHpDelayTimer = 0.0f; // 遅延HPバーの追従開始までの経過時間
    float delayedHpDelayDuration = 0.25f; // 遅延HPバーの追従開始待機時間
    float delayedHpFollowSpeed = 25.0f; // 遅延HPバーのHP追従速度
    int minimumHp = 0; // 戦闘中に適用するHP下限値
    bool hpRecoveryUiActive = false; // HP回復バー演出が有効か
    float hpRecoveryUiStart = 0.0f; // HP回復演出開始時のHP値
    float hpRecoveryUiElapsed = 0.0f; // HP回復演出の経過時間
    float hpRecoveryUiDuration = 0.5f; // HP回復バー演出の継続時間
    CoreColor playerHpCurrentColor{ 0.302f, 0.565f, 0.644f, 1.0f }; // 現在HPバーの色
    CoreColor playerHpDelayedColor{ 0.711f, 0.958f, 0.993f, 1.0f }; // 遅延HPバーの色


    // カメラの注視点の位置
    std::shared_ptr<SceneComponent> cameraTargetComponent;


    // カメラの
    DirectX::XMFLOAT3 focusDirection = {};//　カメラのFocus開始時のForward
    bool isBossBattle = false;  // ボス戦状態かどうか

    // 回避方向
    DodgeDirection dodgeDirection = DodgeDirection::Backward;
    DirectX::XMFLOAT3 dodgeWorldDirection{}; // ワールド座標で指定する回避方向
    bool useDodgeWorldDirection = false; // ワールド座標の回避方向を使用するか
    HitReactionDirection hitReactionDirection = HitReactionDirection::Front; // 被弾リアクションに使用する攻撃方向
    // 歩き走りダッシュのステート管理
    LocomotionMode locomotionMode = LocomotionMode::Idle; // 現在の移動モード


    JustDodgeSlowPhase playerSlowPhase = JustDodgeSlowPhase::Inactive; // Player側Just Dodgeスローの段階
    JustDodgeSlowPhase bossSlowPhase = JustDodgeSlowPhase::Inactive; // Boss側Just Dodgeスローの段階
    float playerSlowHoldTimer = 0.0f; // Player側スロー維持の経過時間
    float bossSlowHoldTimer = 0.0f; // Boss側スロー維持の経過時間
    float playerSlowReturnElapsed = 0.0f; // Player側スロー復帰の経過時間
    float bossSlowReturnElapsed = 0.0f; // Boss側スロー復帰の経過時間

    // ジャスト回避時のスローの調整
    float playerSlowReturnStartScale = 1.0f; // Player側スロー復帰開始時の時間倍率
    float bossSlowReturnStartScale = 1.0f; // Boss側スロー復帰開始時の時間倍率
    float activeBossSlowReturnDuration = 0.10f; // 現在のBoss側スロー復帰時間
    float justDodgeTimeScale = 0.15f;    // ジャスト回避のタイムスケール
    float justDodgeSlowHoldDuration = 1.3f;     // ジャスト回避の時間
    float justDodgeSlowReturnDuration = 0.10f; // Just Dodgeスローの復帰時間
    float rushBossSlowScale = 0.10f; // Rush中にBossへ適用する時間倍率
    float rushBossReturnDuration = 0.10f; // Rush後にBoss速度を復帰する時間

    // プレイヤーの壁に近づいた時の透明度
    float transparencyStartRatio = 0.35f; // 壁との距離に応じてDitherを開始する比率
    float transparencyMinAlpha = 0.25f;  // 最小透明度
    float transparencyMaxAlpha = 1.f;  // 最大透明度

    // 演出中かどうか
    bool moviePerform = false; // 演出中に壁際Ditherを抑制する状態
    bool prisonOpeningSitting = false; // 牢獄開始演出の座り状態が有効か
    bool prisonOpeningStanding = false; // 牢獄開始演出の立ち状態が有効か
    // 牢獄開始演出中の座り姿勢Ping-Pong再生を更新する。
    void UpdatePrisonOpeningStandingPingPong(float deltaTime);

    bool prisonOpeningUseStandingPingPong = false; // 立ち姿勢を使ったPing-Pong再生を使用するか
    float prisonOpeningSittingPingPongStartTime = 0.0f; // Ping-Pong再生区間の開始時刻
    float prisonOpeningSittingPingPongEndTime = 0.5f; // Ping-Pong再生区間の終了時刻
    float prisonOpeningSittingPingPongSpeed = 0.6f; // Ping-Pong再生速度
    float prisonOpeningSittingToStandingBlendTime = 0.15f; // 座り姿勢から立ち姿勢へのブレンド時間
    float prisonOpeningSittingPingPongPhase = 0.0f; // Ping-Pong再生の現在位相
    float prisonOpeningSittingSampleTime = 0.0f; // 座り姿勢から取得する現在サンプル時刻
    bool prisonOpeningSittingPingPongForward = true; // Ping-Pong再生が順方向か
    bool prisonOpeningInputWasEnabled = true; // 牢獄開始演出前の入力有効状態
    DirectX::XMFLOAT3 prisonOpeningSittingPosition{}; // 牢獄開始演出中に維持するPlayer位置
    float prisonOpeningSittingYawDegrees = 0.0f; // 牢獄開始演出中に維持するYaw角
    bool deathCameraTransparencyDisabled = false; // 死亡カメラ中にDitherを無効化する状態
    bool finalHitWaiting = false; // 最終ヒット後に通常更新を抑止する状態
    std::function<void(GruxEnemy*, const DirectX::XMFLOAT3&)> finalHitCallback; // 最終ヒット確定時に呼び出すコールバック
    std::function<void()> deathCameraStartCallback; // 死亡カメラ開始時に呼び出すコールバック

    friend class PlayerStateBase;
    friend class PlayerKnockBackState;
    friend class PlayerDeathPendingState;
    friend class PlayerWinState;
};
