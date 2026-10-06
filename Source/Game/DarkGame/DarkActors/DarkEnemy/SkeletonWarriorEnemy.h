#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Core/Actor.h"
#include "Animation/DangerArea.h"
#include "Game/Actors/Enemy/Enemy.h"

class ParticleComponent;


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
    void DrawAnimationEditorPreviewState(const AnimationNotifyState& state) override;

    bool TakeDamageFromPlayer(int damage) override;
    bool IsDefeated() const override { return state == State::Dead; }
    void SpawnPlayerHitEffect(const DirectX::XMFLOAT3& hitPosition,
        const DirectX::XMFLOAT3& hitNormal, const DirectX::XMFLOAT3& playerPosition) override;
    void SpawnPlayerRushHitEffect(const DirectX::XMFLOAT3& hitPosition,
        const DirectX::XMFLOAT3& hitNormal, const DirectX::XMFLOAT3& playerPosition,
        bool hasHitPosition, bool hasHitNormal) override;
    bool IsDead() const { return state == State::Dead; }
    int GetMaxHp() const { return maxHp; }
    void SetMaxHp(int value);
    const std::shared_ptr<SceneComponent>& GetCameraTargetComponent() const { return cameraTargetComponent; }
    void SetTutorialPassive(bool enabled);
    bool IsTutorialPassive() const { return tutorialPassive; }
    bool IsPlayerWithinAttackRange(const class Player& player) const;
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
    void DrawWeaponHitDebug() const;
    void ResetWeaponSweep();
    void ApplyRimLight();
    bool SpawnDeathBodyParts();
    void DrawBodyPartBoneDebug() const;
    void ResetAnimationEditorPreviewWeaponSweep();
    DirectX::XMFLOAT3 GetWeaponHitPoint(const std::shared_ptr<SceneComponent>& point,
        const DirectX::XMFLOAT3& localOffset) const;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    DirectX::XMFLOAT3 rimLightColor{ 0.45f, 0.70f, 1.00f };
    float rimLightPower = 1.25f;
    bool bodyPartsDebug = false;
    float bodyPartsInitialSpeed = 1.0f;
    float bodyPartsUpwardSpeed = 0.75f;
    float bodyPartsLifetime = 3.0f;
    float skullGroundOffset = 0.0f;
    float ribsGroundOffset = 0.0f;
    float spineGroundOffset = 0.0f;
    float armGroundOffset = 0.0f;
    float legGroundOffset = 0.0f;
    bool hasDeathBodyPartBonePositions = false;
    DirectX::XMFLOAT3 deathHeadBonePosition{};
    DirectX::XMFLOAT3 deathSpineHighBonePosition{};
    DirectX::XMFLOAT3 deathThighLeftBonePosition{};
    std::shared_ptr<SkeletalMeshComponent> sword;
    std::shared_ptr<SkeletalMeshComponent> shield;
    std::shared_ptr<RotationComponent> rotationComponent;
    std::shared_ptr<SceneComponent> cameraTargetComponent;
    std::shared_ptr<SceneComponent> weaponRootPoint;
    std::shared_ptr<SceneComponent> weaponMiddlePoint;
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
    bool hasPreviousWeaponRoot = false;
    DirectX::XMFLOAT3 previousWeaponRoot{};
    DirectX::XMFLOAT3 previousWeaponMiddle{};
    DirectX::XMFLOAT3 previousWeaponTip{};
    float activeWeaponHitRadius = 0.01f;
    DirectX::XMFLOAT3 activeWeaponHitOffset{};
    bool editorPreviewHasPreviousWeaponRoot = false;
    DirectX::XMFLOAT3 editorPreviewPreviousWeaponRoot{};
    DirectX::XMFLOAT3 editorPreviewPreviousWeaponMiddle{};
    DirectX::XMFLOAT3 editorPreviewPreviousWeaponTip{};
    const AnimationNotifyState* editorPreviewWeaponHitBoxState = nullptr;
    float editorPreviewWeaponHitBoxTime = -1.0f;
    const AnimationNotifyState* activeDangerNotifyState = nullptr;
    DangerArea dangerArea{};
    DirectX::XMFLOAT3 lockedAttackRight{ 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 lockedAttackUp{ 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 lockedAttackForward{ 0.0f, 0.0f, 1.0f };
    bool dangerAreaDebug = false;
    bool weaponHitDebug = false;
    std::string dangerAreaSaveStatus;

    // Per-actor tutorial switch. It suppresses only combat AI; animation,
    // damage, death, collision, and LockOn participation remain active.
    bool tutorialPassive = false;
    // Tutorial tuning, isolated from Grux and Player combat settings.
    int maxHp = 8;
    float facePlayerDistance = 12.0f;
    float attackRange = 7.0f;
    float recoveryDuration = 2.0f;
    int attackDamage = 4;
    DirectX::XMFLOAT3 weaponRootOffset{ 0.0f, -0.5f, 0.0f };
};




