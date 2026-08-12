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

    uint64_t GetCurrentAttackSequenceId() const { return currentAttackSequenceId; }

    BossAIMode GetBossAIMode() const { return bossAIMode; }
    BossAttackType GetSelectedAttackType() const { return selectedAttackType; }
    bool SelectAttackForCurrentMode();
    int GetAttackStageCount(BossAttackType type) const;
    bool PlayAttackStage(BossAttackType type, int stage);
    bool PlayAttackAnimationByName(const std::string& animationName);
    void BeginAdditionalAttackStage();
    void ClearJumpAttackMotionWarpOverride();
    float GetAttackInterval() const { return attackInterval; }
    float GetRecoveryDuration() const { return recoveryDuration; }
    bool IsTransitionWindowActive() const { return transitionWindow; }
    int GetCurrentAttackHitCount() const { return currentAttackHitCount; }
    bool WasCurrentAttackSequenceJustDodged() const { return !justDodgedActors.empty(); }

    BossTargetContext BuildTargetContext() const;
    float GetMaximumCombatAttackDistance() const;
    bool IsOutsideAllAttackRanges(float distance) const;
    bool IsFacingPlayerForAttack(const BossTargetContext& context) const;
    void BeginApproach();
    void UpdateApproachMovement(const DirectX::XMFLOAT3& direction, float deltaTime);
    void StopAIMovement();
    bool RotateTowardsPlayer(const DirectX::XMFLOAT3& direction,
        float degreesPerSecond, float deltaTime);
    float GetTurnCompleteAngle() const { return turnCompleteAngle; }
    float GetTurnTimeout() const { return turnTimeout; }
    float GetApproachTurnSpeed() const { return approachTurnSpeed; }
    float GetTurnSpeed() const { return turnSpeed; }
    void SetLastAIDecision(const std::string& reason) { lastAIDecisionReason = reason; }

    // カメラの注視点の位置
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }

    // ボスの名前の演出を開始する
    void StartGruxNamePerform(float duration, float start = 0.0f, float end = 1.0f);
private:
    // プレイヤーとの距離を取得する関数
    float GetDistanceToPlayer();
    // 武器ヒット時の処理
    void OnWeaponHit(CollisionComponent* self, CollisionComponent* other);
    void ResetJustDodgeRecords(const char* reason);
    bool HasJustDodgedAttack(const Actor* actor) const;
    void ResetDangerArea();
    void PrepareJumpAttackMotionWarpOverride();
    void RefreshActiveHitBoxesFromNotifyStates();
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

    bool rightHitBox = false;   // 右の剣の当たり判定
    bool leftHitBox = false;    // 左の剣の当たり判定
    bool isDangerWindow = false;

    // ヒット中に当たった敵を記録する
    std::unordered_set<Actor*> hitActors;
    // Players that successfully just-dodged the current attack sequence.
    std::unordered_set<const Actor*> justDodgedActors;
    uint64_t currentAttackSequenceId = 0;
    int currentAttackHitCount = 0;

    BossAIMode bossAIMode = BossAIMode::DebugFixedAttack;
    BossAttackType debugFixedAttackType = BossAttackType::PrimaryAttackLA;
    BossAttackType selectedAttackType = BossAttackType::PrimaryAttackLA;
    BossAttackType lastAttackType = BossAttackType::PrimaryAttackLA;
    bool hasLastAttack = false;
    std::array<BossAttackData, 4> combatAttackData = {{
        { BossAttackType::PrimaryAttackLA, "PrimaryAttack_LA", 0.0f, 5.0f, 1.0f },
        { BossAttackType::PrimaryAttackRA, "PrimaryAttack_RA", 0.0f, 5.0f, 1.0f },
        { BossAttackType::FastCombo, "FastCombo", 0.0f, 6.0f, 1.0f },
        { BossAttackType::JumpAttack, "PrimaryAttack_JumpAttack", 4.5f, 14.0f, 1.0f },
    }};
    std::array<float, 4> combatEffectiveWeights{};
    std::array<bool, 4> combatCandidateFlags{};
    float currentCombatPlayerDistance = 0.0f;
    float lastCombatSelectionDistance = 0.0f;
    float repeatWeightScale = 0.25f;
    float attackInterval = 0.1f;
    float recoveryDuration = 0.5f;
    float attackFacingAngle = 35.0f;
    float approachSpeed = 5.0f;
    float approachTurnSpeed = 180.0f;
    float turnSpeed = 180.0f;
    float turnCompleteAngle = 15.0f;
    float turnTimeout = 1.5f;
    std::string lastAIDecisionReason = "None";
    bool transitionWindow = false;

    float maxJumpDistance = 12.5f;
    float desiredAttackDistance = 0.1f;
    float currentJumpPlayerDistance = 0.0f;
    float calculatedJumpDistance = 0.0f;
    bool jumpMotionWarpOverrideActive = false;
    DirectX::XMFLOAT3 jumpAttackStartPlayerPosition{};
    DirectX::XMFLOAT3 jumpMotionWarpDirection{ 0.0f, 0.0f, 1.0f };

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




