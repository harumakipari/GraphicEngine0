#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Core/Actor.h"
#include "Game/Actors/Enemy/Enemy.h"


class SwordActor : public Actor
{
public:
    void Initialize(const Transform& transform) override
    {
    }
};

class ShieldActor : public Actor
{
public:
    void Initialize(const Transform& transform) override
    {
    }
};

class SkeletonWarriorActor :public Enemy
{
public:
    explicit SkeletonWarriorActor(const std::string& actorName) :Enemy(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    // Player-side generic Enemy damage routing is intentionally deferred to STEP 2.
    void TakeDamage(int damage);
    bool IsDead() const { return state == State::Dead; }
    int GetMaxHp() const { return maxHp; }

private:
    enum class State : uint8_t { Idle, Attacking, Recovery, Dead };

    void BeginAttack(const DirectX::XMFLOAT3& directionToPlayer);
    void UpdateAttack(float elapsedTime, class Player& player);
    void UpdateRecovery(float elapsedTime);
    void UpdateWeaponSweep(class Player& player);
    void ResetWeaponSweep();
    DirectX::XMFLOAT3 GetWeaponRootPosition() const;
    DirectX::XMFLOAT3 GetWeaponTipPosition() const;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<SkeletalMeshComponent> sword;
    std::shared_ptr<SkeletalMeshComponent> shield;
    std::shared_ptr<RotationComponent> rotationComponent;
    std::shared_ptr<SceneComponent> weaponRootPoint;
    std::shared_ptr<SceneComponent> weaponTipPoint;

    State state = State::Idle;
    float stateElapsed = 0.0f;
    bool attackHitActive = false;
    bool hasHitPlayerThisAttack = false;
    bool hasPreviousWeaponPoints = false;
    DirectX::XMFLOAT3 previousWeaponRoot{};
    DirectX::XMFLOAT3 previousWeaponTip{};

    // Tutorial tuning, isolated from Grux and Player combat settings.
    int maxHp = 6;
    float facePlayerDistance = 12.0f;
    float attackRange = 3.0f;
    float attackDuration = 1.05f;
    float attackHitStartTime = 0.42f;
    float attackHitEndTime = 0.76f;
    float recoveryDuration = 0.85f;
    int attackDamage = 4;
    float weaponHitRadius = 1.f;
    DirectX::XMFLOAT3 weaponRootOffset{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 weaponTipOffset{ 0.0f, 0.0f, 1.05f };
};




