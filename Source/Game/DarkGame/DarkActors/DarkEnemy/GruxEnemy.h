#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Components/Effect/ParticleComponent.h"
#include "Core/Actor.h"
#include "Game/Actors/Base/Character.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "UI/Widgets/Widget.h"

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

    // 攻撃開始時に始める処理
    void StartAttack();

    // カメラの注視点の位置
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }

    // ボスの名前の演出を開始する
    void StartGruxNamePerform(float duration, float start = 0.0f, float end = 1.0f);
private:
    // プレイヤーとの距離を取得する関数
    float GetDistanceToPlayer();
    // 武器ヒット時の処理
    void OnWeaponHit(CollisionComponent* self, CollisionComponent* other);
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

    float hitWeaponRadius = 0.8f;   // 武器の半径
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
    DirectX::XMFLOAT2 justDodgeAreaSize = { 0.0f,0.0f };
    // ジャスト回避の矩形のオフセット
    DirectX::XMFLOAT3 justDodgeAreaOffset = { 0.0f,0.0f,0.0f };

    float flashDuration = 0.8f;   // 何秒でフラッシュしなくなるか

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




