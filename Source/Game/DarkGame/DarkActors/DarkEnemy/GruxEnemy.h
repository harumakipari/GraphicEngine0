#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Core/Actor.h"
#include "Game/Actors/Base/Character.h"


class GruxEnemy :public Character
{
public:
    enum class BossState :uint8_t
    {
        Idle,
        Attack,
        Cooldown
    };

    explicit GruxEnemy(const std::string& actorName) :Character(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    //当たった時の処理
    void TakeDamage(int damage);
private:
    // 攻撃が当たるタイミングで呼ばれる関数
    void DoAttackHit();

    // プレイヤーとの距離を取得する関数
    float GetDistanceToPlayer();
private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<RotationComponent> rotationComponent;

    BossState state = BossState::Idle;
    float stateTimer = 0.0f;
    bool attackPlayed = false;
    float attackHitTime = 0.5f; // 何秒後に当たるか
    bool damageDone = false;
};


class KnightActor: public Character
{
public:
    explicit KnightActor(const std::string & actorName) :Character(actorName) {}

    void Initialize(const Transform & transform)override;

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




