#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Core/Actor.h"
#include "Game/Actors/Base/Character.h"
#include "Game/Actors/Enemy/Enemy.h"

class GruxEnemy :public Enemy
{
public:
    explicit GruxEnemy(const std::string& actorName) :Enemy(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void DrawImGuiDetails() override;

    //当たった時の処理
    void TakeDamage(int damage);

    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

    // 攻撃開始時に始める処理
    void StartAttack();

    // カメラの注視点の位置
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() { return cameraTargetComponent; }

private:
    // 攻撃が当たるタイミングで呼ばれる関数
    void DoAttackHit();

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




