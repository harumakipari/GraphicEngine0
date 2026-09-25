#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Core/Actor.h"
#include "Animation/DangerArea.h"
#include "Game/Actors/Enemy/Enemy.h"

class ParticleComponent;

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
private:
    enum class State : uint8_t { Idle, Attacking, Recovery, Dead };

public:
    explicit SkeletonWarriorActor(const std::string& actorName) :Enemy(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;
    void DrawImGuiDetails() override;
    void OnAnimationNotifyBegin(const AnimationNotifyState& state) override;
    void OnAnimationNotifyEnd(const AnimationNotifyState& state) override;

    bool TakeDamageFromPlayer(int damage) override;
    bool IsDefeated() const override { return state == State::Dead; }
    void SpawnPlayerHitEffect(const DirectX::XMFLOAT3& hitPosition,
        const DirectX::XMFLOAT3& hitNormal, const DirectX::XMFLOAT3& playerPosition) override;
    void SpawnPlayerRushHitEffect(const DirectX::XMFLOAT3& hitPosition,
        const DirectX::XMFLOAT3& hitNormal, const DirectX::XMFLOAT3& playerPosition,
        bool hasHitPosition, bool hasHitNormal) override;
    bool IsDead() const { return state == State::Dead; }
    int GetMaxHp() const { return maxHp; }
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() const { return cameraTargetComponent; }

private:
    void BeginAttack(const DirectX::XMFLOAT3& directionToPlayer);
    void UpdateAttack(float elapsedTime, class Player& player);
    void UpdateRecovery(float elapsedTime);
    void UpdateWeaponSweep(class Player& player);
    void UpdateDangerWindow(class Player& player);
    bool TryStartJustDodgeSuccess(class Player& player);
    void RefreshDangerAreaFromNotify();
    bool IsPlayerInsideDangerArea(class Player& player) const;
    AnimationNotifyState* GetAttackDangerNotifyState();
    void DrawDangerAreaDebug() const;
    void ResetWeaponSweep();
    DirectX::XMFLOAT3 GetWeaponRootPosition() const;
    DirectX::XMFLOAT3 GetWeaponTipPosition() const;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<SkeletalMeshComponent> sword;
    std::shared_ptr<SkeletalMeshComponent> shield;
    std::shared_ptr<RotationComponent> rotationComponent;
    std::shared_ptr<SceneComponent> cameraTargetComponent;
    std::shared_ptr<SceneComponent> weaponRootPoint;
    std::shared_ptr<SceneComponent> weaponTipPoint;
    std::shared_ptr<ParticleComponent> hitSwordEffectComponent;
    std::shared_ptr<ParticleComponent> rushHitRingEffectComponent;
    std::shared_ptr<ParticleComponent> rushHitSparkEffectComponent;

    State state = State::Idle;
    float stateElapsed = 0.0f;
    bool attackHitActive = false;
    bool isDangerWindow = false;
    bool hasHitPlayerThisAttack = false;
    bool hasJustDodgedPlayerThisAttack = false;
    bool hasPreviousWeaponPoints = false;
    DirectX::XMFLOAT3 previousWeaponRoot{};
    DirectX::XMFLOAT3 previousWeaponTip{};
    const AnimationNotifyState* activeDangerNotifyState = nullptr;
    DangerArea dangerArea{};
    DirectX::XMFLOAT3 lockedAttackRight{ 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 lockedAttackUp{ 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 lockedAttackForward{ 0.0f, 0.0f, 1.0f };
    bool dangerAreaDebug = false;
    std::string dangerAreaSaveStatus;

    // Tutorial tuning, isolated from Grux and Player combat settings.
    int maxHp = 8;
    float facePlayerDistance = 12.0f;
    float attackRange = 3.0f;
    float attackDuration = 1.05f;
    float attackHitStartTime = 0.42f;
    float attackHitEndTime = 0.76f;
    float recoveryDuration = 5.85f;
    int attackDamage = 4;
    float weaponHitRadius = 0.5f;
    DirectX::XMFLOAT3 weaponRootOffset{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 weaponTipOffset{ 0.0f, 0.0f, 1.05f };
};




