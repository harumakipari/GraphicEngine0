#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Components/Effect/ParticleComponent.h"
#include "Core/Actor.h"
#include "Game/Actors/Base/Character.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "UI/Widgets/Widget.h"
#include "Animation/DangerArea.h"
#include "Game/Actors/Enemy/Boss/BossAITypes.h"
#include <array>

class GruxEnemy :public Enemy
{
public:
    explicit GruxEnemy(const std::string& actorName) :Enemy(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void DrawImGuiDetails() override;

    //当たった時の処理
    void TakeDamage(int damage);

    // ヒットエフェクトを生成する
    void SpawnHitEffect(DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 playerPos) const;

    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

    void OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event) override;

    void DrawAnimationEditorPreviewState(const AnimationNotifyState& state) override;

    void OnAnimationChanged() override;

    // 攻撃開始時に始める処理
    void StartAttack();

    void DisableAttackHitBoxes();

    // Actionの選択結果を取得する
    BossActionType GetSelectedActionType() const
    {
        return selectedActionType;
    }

    uint64_t GetCurrentAttackSequenceId() const { return currentAttackSequenceId; }

    BossAIMode GetBossAIMode() const { return bossAIMode; }
    BossAttackType GetSelectedAttackType() const { return selectedAttackType; }

    bool TryStartIntent(BossIntentType intentType);
    bool SelectIntentByWeight();
    void ClearActiveIntent();
    void MarkIntentPositioningAttempted();
    void BeginIntentReevaluation();
    void MarkIntentAttackSelected();
    void OnSelectedActionStartedSuccessfully();
    void OnSelectedActionStartFailed();
    void FailActiveIntent(const char* reason);
    const std::optional<BossIntentType>& GetActiveIntent() const { return activeIntent; }
    bool WasIntentPositioningAttempted() const { return intentPositioningAttempted; }

    const std::optional<BossPositioningData>& GetSelectedPositioningData() const
    {
        return selectedPositioningData;
    }

    std::optional<BossAttackType> GetAttackTypeForAction(BossActionType actionType) const;
    bool PrepareAttackForSelectedAction();
    const BossPositioningData* GetPositioningDataForAction(BossActionType actionType) const;
    void BeginPositioning(const BossPositioningData& data);
    void UpdatePositioningMovement(const DirectX::XMFLOAT3& moveDirection,
        const DirectX::XMFLOAT3& facingDirection, float deltaTime);
    void UpdatePositioningDebug(float traveledDistance, float elapsedTime, float stuckTimer);
    void FinishPositioningDebug(const std::string& reason);
    void StartSelectedActionCooldown();

    bool SelectAttackForCurrentMode();
    int GetAttackStageCount(BossAttackType type) const;
    bool PlayAttackStage(BossAttackType type, int stage);
    bool PlayAttackAnimationByName(const std::string& animationName);
    void BeginAdditionalAttackStage();
    void ClearJumpAttackMotionWarpOverride();
    bool BeginDashAttackMovement();
    bool UpdateDashAttackMovement(float deltaTime);
    void StopDashAttackMovement();
    float GetAttackInterval() const { return attackInterval; }
    float GetDashWindupDuration() const { return dashWindupDuration; }
    float GetRecoveryDurationForCurrentAttack() const;
    int GetDamageForCurrentAttack() const;
    bool IsTransitionWindowActive() const { return transitionWindow; }
    int GetCurrentAttackHitCount() const { return currentAttackHitCount; }
    bool WasCurrentAttackSequenceJustDodged() const { return !justDodgedActors.empty(); }

    BossTargetContext BuildTargetContext() const;
    bool ShouldWaitForActiveIntentCooldown(const BossTargetContext& context) const;
    bool IsFacingPlayerForAttack(const BossTargetContext& context) const;
    void StopAIMovement();
    bool RotateTowardsPlayer(const DirectX::XMFLOAT3& direction,
        float degreesPerSecond, float deltaTime);
    float GetTurnCompleteAngle() const { return turnCompleteAngle; }
    float GetTurnTimeout() const { return turnTimeout; }
    float GetTurnSpeed() const { return turnSpeed; }
    void SetLastAIDecision(const std::string& reason) { lastAIDecisionReason = reason; }

    // カメラの注視点の位置
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }

    // ボスの名前の演出を開始する
    void StartGruxNamePerform(float duration, float start = 0.0f, float end = 1.0f);


    // 抽選結果をmemberへ保存する関数
    bool SelectCombatAction();


private:
    // プレイヤーとの距離を取得する関数
    float GetDistanceToPlayer();

    // プレイヤーとの距離に応じた距離領域を取得する関数
    BossDistanceRegion GetDistanceRegion(float distance) const;

    // 現在の距離領域に基づいて、候補となる行動を選択する関数
    void UpdateActionCandidateFlags(const BossTargetContext& context);
    bool IsActionForCurrentIntent(BossActionType actionType, const BossTargetContext& context) const;
    const BossIntentData* GetActiveIntentData() const;
    BossIntentRangeStatus GetIntentRangeStatus(
        const BossIntentData& intentData, float distance) const;

    // アクションが現在の距離(Region)で候補になるかを判定する関数
    bool IsActionCandidateForCurrentDistance(const BossActionData& actionData, BossDistanceRegion currentRegion) const;

    // Action候補とBase WeightからEffective Weightを更新する
    void UpdateActionEffectiveWeights();
    void UpdateActionCooldowns(float deltaTime);
    void UpdateIntentEffectiveWeights(const BossTargetContext& context);
    float GetIntentWeightForDistance(const BossIntentData& data, BossDistanceRegion region) const;
    float GetTotalIntentWeight() const;

    // ActionのEffective Weight合計を求める関数
    float GetTotalActionWeight() const;

    // Weightに基づいて行動を選択する関数。候補がない場合はstd::nulloptを返す
    std::optional<BossActionType> SelectActionByWeight();

    void ResetJustDodgeRecords(const char* reason);
    bool HasJustDodgedAttack(const Actor* actor) const;
    void ResetDangerArea();
    void PrepareJumpAttackMotionWarpOverride();


    void RefreshActiveHitBoxesFromNotifyStates();
    // ボスの距離範囲のデバック描画
    void DrawBossAIDebugWorld(const BossTargetContext& context) const;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    // 回転コンポーネントを追加
    std::shared_ptr<RotationComponent> rotationComponent;
    // キャラクタームーブコンポーネントを追加
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;

    // 左の武器の当たり判定のコンポーネント
    std::shared_ptr<CapsuleComponent> leftWeaponCollisionComp;
    // 右の武器の当たり判定のコンポーネント
    std::shared_ptr<CapsuleComponent> rightWeaponCollisionComp;
    std::string leftWeapon = "leftWeapon";
    std::string rightWeapon = "rightWeapon";
    std::string bothWeapon = "bothWeapon";

    std::shared_ptr<SceneComponent> weaponLeftRootComponent; // 左の武器の根元のコンポーネント
    std::shared_ptr<SceneComponent> weaponLeftMiddleComponent; // 左の武器の中間のコンポーネント
    std::shared_ptr<SceneComponent> weaponLeftTipComponent;  // 左の武器の先端のコンポーネント

    std::shared_ptr<SceneComponent> weaponRightRootComponent; // 右の武器の根元のコンポーネント
    std::shared_ptr<SceneComponent> weaponRightMiddleComponent; // 右の武器の中間のコンポーネント
    std::shared_ptr<SceneComponent> weaponRightTipComponent;  // 右の武器の先端のコンポーネント

    std::shared_ptr<ParticleComponent> hitSwordEffectComponent; // ヒット時の剣のエフェクト

    std::shared_ptr<UIGaugeComponent> hpFrameUiComponent;   // HPバー

    bool rightHitBox = false;   // 右の剣の当たり判定
    bool leftHitBox = false;    // 左の剣の当たり判定
    bool isDangerWindow = false;

    // ヒット中に当たった敵を記録する
    std::unordered_set<Actor*> hitActors;
    // Players that successfully just-dodged the current attack sequence.
    std::unordered_set<const Actor*> justDodgedActors;
    uint64_t currentAttackSequenceId = 0;
    int currentAttackHitCount = 0;

    BossAIMode bossAIMode = BossAIMode::CombatAI;
    //BossAIMode bossAIMode = BossAIMode::DebugFixedAttack;
    BossAttackType debugFixedAttackType = BossAttackType::PrimaryAttackLA;

    std::optional<BossIntentType> activeIntent = std::nullopt;
    BossIntentStep activeIntentStep = BossIntentStep::Selecting;
    bool intentPositioningAttempted = false;
    std::string intentLifecycleState = "None";
    std::string intentLifecycleTrace = "None";
    std::string intentLifecycleReason = "None";

    static constexpr int intentCount = 2;
    std::array<BossIntentData, intentCount> combatIntentData =
    { {
        { BossIntentType::CloseCombat, 70.0f, 40.0f, 30.0f, 4.0f, 5.5f },
        { BossIntentType::DashAttackPlan, 30.0f, 60.0f, 70.0f, 8.0f, 10.0f },
    } };
    std::array<float, intentCount> combatIntentEffectiveWeights{};
    bool hasLastIntentRandomRoll = false;
    float lastIntentRandomRoll = 0.0f;
    float lastIntentRandomTotalWeight = 0.0f;
    std::array<float, intentCount> lastIntentRandomRangeBegin{};
    std::array<float, intentCount> lastIntentRandomRangeEnd{};
    std::array<float, intentCount> lastIntentRandomWeights{};
    std::optional<BossIntentType> lastSelectedIntent = std::nullopt;

    BossActionType selectedActionType = BossActionType::AttackLA;
    BossActionType lastActionType = BossActionType::AttackLA;
    BossAttackType selectedAttackType = BossAttackType::PrimaryAttackLA;
    std::optional<BossPositioningData> selectedPositioningData = std::nullopt;

    BossAttackType lastAttackType = BossAttackType::PrimaryAttackLA;
    bool hasLastAttack = false;

    // 行動のデータを定義する配列。攻撃の種類、距離条件などを設定する。
    static constexpr int actionCount = 7;
    std::array<BossActionData, actionCount> combatActionData =
    {
        {
        { BossActionType::AttackLA, BossAttackType::PrimaryAttackLA ,BossDistanceRegion::Near,BossDistanceRegion::Near,30.0f,1.0f},
        { BossActionType::AttackRA, BossAttackType::PrimaryAttackRA,BossDistanceRegion::Near,BossDistanceRegion::Near,30.0f,1.0f},
        { BossActionType::FastCombo, BossAttackType::FastCombo ,BossDistanceRegion::Near,BossDistanceRegion::Near,40.0f,2.0f},
        { BossActionType::JumpAttack,BossAttackType::JumpAttack ,BossDistanceRegion::Middle,BossDistanceRegion::Middle,30.0f,3.0f},
        { BossActionType::DashAttack,BossAttackType::DashAttack,BossDistanceRegion::Middle,BossDistanceRegion::Far,40.0f,4.0f},
        { BossActionType::Approach, std::nullopt,BossDistanceRegion::Near,BossDistanceRegion::Far,40.0f,0.5f},
        { BossActionType::Retreat, std::nullopt,BossDistanceRegion::Near,BossDistanceRegion::Far,40.0f,2.5f},
    }
    };

    // 各行動が距離条件を満たしているかのフラグ　
    std::array<bool, actionCount> combatActionCandidateFlags{};
    std::array<BossActionCandidateReason, actionCount> combatActionCandidateReasons{};
    // 各行動の有効な重み。距離条件とRepeat条件を考慮した有効な重み
    std::array<float, actionCount> combatActionEffectiveWeights{};
    std::array<float, actionCount> combatActionCooldownRemaining{};

    bool showBossAIDebug = false;
    BossTargetContext aiDebugTargetContext{};
    bool hasSelectedActionDebug = false;
    bool hasLastActionRandomRoll = false;
    float lastActionRandomRoll = 0.0f;
    float lastActionRandomTotalWeight = 0.0f;
    std::array<float, actionCount> lastActionRandomRangeBegin{};
    std::array<float, actionCount> lastActionRandomRangeEnd{};
    std::array<float, actionCount> lastActionRandomWeights{};

    // 攻撃ごとのデータを定義する配列。アニメーション名、距離条件、重みなどを設定する。
    std::array<BossAttackData, 5> combatAttackData =
    { {
        { BossAttackType::PrimaryAttackLA, "PrimaryAttack_LA", 0.0f, 5.0f, 1.0f, 1.25f, 1 },
        { BossAttackType::PrimaryAttackRA, "PrimaryAttack_RA", 0.0f, 5.0f, 1.0f, 1.30f, 1 },
        { BossAttackType::FastCombo, "FastCombo", 0.0f, 6.0f, 1.0f, 2.0f, 1 },
        { BossAttackType::JumpAttack, "PrimaryAttack_JumpAttack", 4.5f, 12.0f, 1.0f, 2.80f, 1 },
        { BossAttackType::DashAttack, "Stampede_0 > Stampede_Knockup_0", 6.0f, 100.0f, 1.0f, 1.20f, 1 },
    } };

    // 既存のAttack選択用
    std::array<float, 5> combatEffectiveWeights{};  // 距離条件とRepeat条件を考慮した有効な重み
    std::array<bool, 5> combatCandidateFlags{}; // 各攻撃が距離条件を満たしているかのフラグ　主にImGuiで使用。
    float currentCombatPlayerDistance = 0.0f;   // Attack選択時点のプレイヤーとの距離。距離条件の判定に直接使用。
    float lastCombatSelectionDistance = 0.0f;   //  最後にAttack抽選を行ったときの距離。現在はImGui表示用として保持。
    float repeatWeightScale = 0.25f;    //  直前と同じAttackのWeightへ掛ける倍率。現在は0.25なので、同じ攻撃のWeightを25%まで下げる。

    std::array<BossPositioningData, 2> combatPositioningData =
    { {
        { BossActionType::Approach, BossPositioningDirection::TowardPlayer, 20.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TargetDistance, 0.0f },
        { BossActionType::Retreat, BossPositioningDirection::AwayFromPlayer, 7.0f, 6.0f, 3.0f, 0.5f, 0.1f, BossPositioningCompletionType::TravelDistance, 0.0f },
    } };

    //  StateMachineのタイミングと方向制御
    float attackInterval = 0.1f;   // EnemyThinkStateへ入ってからAttack選択を開始するまでの待ち時間。
    float recoveryDuration = 0.5f;  //  攻撃終了後、EnemyRecoveryStateに滞在する時間。現在は全Attack共通の0.5秒。
    float attackFacingAngle = 35.0f;    // この角度以内なら攻撃可能とみなす

    float nearDistanceThreshold = 6.0f; // この距離以下は近距離とみなす
    float intentPositioningArrivalInset = 0.25f;
    float middleDistanceThreshold = 12.0f; // この距離以下は中距離とみなす

    const std::array<BossActionData, actionCount> initialCombatActionData = combatActionData;
    const std::array<BossIntentData, intentCount> initialCombatIntentData = combatIntentData;
    const std::array<BossAttackData, 5> initialCombatAttackData = combatAttackData;
    const float initialNearDistanceThreshold = nearDistanceThreshold;
    const float initialMiddleDistanceThreshold = middleDistanceThreshold;

    float turnSpeed = 720.0f;  // EnemyTurnStateでその場回転するときの速度
    float turnCompleteAngle = 15.0f;    // この角度以内なら回転完了とみなす
    float turnTimeout = 1.5f;   //   Turnがいつまでも完了しない場合の制限時間。現在は1.5秒でThinkへ戻る。
    std::string lastAIDecisionReason = "None";  // 最後にAIが行った判断理由を文字列で保存。ImGuiに表示。AIの動作確認用。
    bool positioningDebugActive = false;
    BossPositioningData activePositioningDebugData{};
    float positioningDebugTraveledDistance = 0.0f;
    float positioningDebugElapsedTime = 0.0f;
    float positioningDebugStuckTimer = 0.0f;
    std::string positioningEndReason = "None";

    // FastComboの連続攻撃用
    bool transitionWindow = false;  //   Animation NotifyのTransitionWindowが現在有効かを表す。FastComboで次のコンボ段階へ進めるタイミングの判定に使用。

    // JumpAttackのMotionWarp用
    float maxJumpDistance = 12.5f;//  JumpAttackで実際に移動してよい最大距離。プレイヤーが遠くても12.5より長くは移動しない。
    float desiredAttackDistance = 0.1f; //  JumpAttack後にプレイヤーとの間へ残したい距離
    float currentJumpPlayerDistance = 0.0f; //  JumpAttack開始時点のプレイヤーまでの距離
    float calculatedJumpDistance = 0.0f;    //  最終的にMotionWarpで移動する距離
    bool jumpMotionWarpOverrideActive = false;  // 通常のAnimation Notifyに設定された移動距離ではなく、JumpAttack用に計算した距離と方向を使用するかどうか
    DirectX::XMFLOAT3 jumpAttackStartPlayerPosition{};  //  JumpAttack開始時のプレイヤー位置
    DirectX::XMFLOAT3 jumpMotionWarpDirection{ 0.0f, 0.0f, 1.0f };  //  ボスからJumpAttack開始時のプレイヤー位置へ向かう正規化済み方向

    // DashAttack
    float dashWindupDuration = 1.40f;   // 予備動作の時間
    float dashAttackSpeed = 12.0f;
    float minDashAttackDistance = 4.0f;
    float maxDashAttackDistance = 16.0f;
    float desiredDashAttackDistance = 2.0f;
    float dashArrivalDistance = 0.35f;
    float dashAttackTimeout = 1.10f;
    float currentDashAttackPlayerDistance = 0.0f;
    float calculatedDashAttackDistance = 0.0f;
    float dashAttackElapsedTime = 0.0f;
    bool dashAttackMovementActive = false;
    DirectX::XMFLOAT3 dashAttackDirection{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 dashAttackStartPosition{};
    DirectX::XMFLOAT3 dashTargetPosition{};

    bool isDeathPerform = false;
    float pitchBaseValue = 0.45f;

    // 左目の位置用コンポーネントを追加　暗闇で光る目の表現用
    std::shared_ptr<SceneComponent> leftEyeSceneComponent;
    // 右目の位置用コンポーネントを追加　暗闇で光る目の表現用
    std::shared_ptr<SceneComponent> rightEyeSceneComponent;

    // カメラの注視点の位置 
    std::shared_ptr<SceneComponent> cameraTargetComponent;
    // ボス戦時のオフセット
    float bossBattleCameraDistance = 0.0f;
    //float bossBattleCameraRightDistance = 2.5f;
    float bossBattleCameraRightDistance = 0.0f;
    DirectX::XMFLOAT3 bossBattleCameraOffset = { 0.0f,0.0f,0.0f };

    // 前フレームの左の武器
    DirectX::XMFLOAT3 prevWeaponLeftRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponLeftMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponLeftTipPos = { 0.0f,0.0f,0.0f };

    // 前フレームの右の武器
    DirectX::XMFLOAT3 prevWeaponRightRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponRightMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevWeaponRightTipPos = { 0.0f,0.0f,0.0f };

    float hitWeaponRadius = 0.8f;
    float activeLeftHitBoxRadius = 0.8f;
    float activeRightHitBoxRadius = 0.8f;
    std::vector<const AnimationNotifyState*> activeHitBoxNotifyStates;   // 武器の半径
    float enemyScale = 1.7f;    // 敵のスケール
    float hitEnemyEffectOffsetY = 2.2f;  // ヒットエフェクトのオフセットY
    float hitPlayerEffectOffsetY = 2.4f;  // ヒットエフェクトのオフセットY

    // 登場シーンのボス名前のUI
    std::shared_ptr<UIImageComponent> gruxNameImageComponent;
    std::unique_ptr<EasingRunner> easingRunner;
    float easingFactorAlpha = 0.0f;

    // ロックオンのイメージモデル
    std::shared_ptr<SkeletalMeshComponent> lockOnTargetMeshComponent;
    float lockOnOffset = 0.0f;  // プレイヤー側に押し出すオフセット
    float lockOnOffsetY = 1.65f;
    // ロックオンのイメージUI
    std::shared_ptr<UIImageComponent> lockOnTargetImageComponent;

    // ジャスト回避の矩形の範囲
    DirectX::XMFLOAT3 justDodgeAreaSize = { 0.0f,2.0f,0.0f };
    // ジャスト回避の矩形のオフセット
    DirectX::XMFLOAT3 justDodgeAreaOffset = { 0.0f,0.0f,0.0f };
    DangerArea dangerArea{};

    float flashDuration = 0.8f;   // 何秒でフラッシュしなくなるか

    // アニメーション時にどれくらい移動するか
    std::vector<AnimationMotionWarp> animationMotionWarps;

    friend class GruxEnemyEyeActor;
};


class KnightActor : public Character
{
public:
    explicit KnightActor(const std::string& actorName) :Character(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<RotationComponent> rotationComponent;

};

class SavarogEnemy :public Character
{
public:
    explicit SavarogEnemy(const std::string& actorName) :Character(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<RotationComponent> rotationComponent;

};

class GracialEnemy :public Character
{
public:
    explicit GracialEnemy(const std::string& actorName) :Character(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<RotationComponent> rotationComponent;

};

