#pragma once

#include "Core/Actor.h"

class SkeletalMeshComponent;
class ShapeComponent;

// Death-effect debris simulated as an isolated PhysX dynamic actor.
class SkeletonBodyPartActor : public Actor
{
public:
    enum class PartType : uint8_t
    {
        Skull,
        Ribs,
        Spine,
        Arm,
        Leg,
    };

    struct TransformCorrection
    {
        // Applied in the body-part mesh's local space after its bone transform.
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT3 rotationEuler{};
        DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
    };

    struct SpawnParams
    {
        PartType partType = PartType::Skull;
        DirectX::XMFLOAT3 initialVelocity{};
        DirectX::XMFLOAT3 angularVelocity{}; // degrees per second
        // A value <= 0 keeps the visual debris until its owning scene is destroyed.
        float lifeTime = 0.0f;
        bool debugCollisionShape = false;
        float linearDamping = 1.0f;
        float angularDamping = 3.0f;
        float sleepThreshold = 0.005f;
        TransformCorrection transformCorrection{};
    };

    SkeletonBodyPartActor(const std::string& actorName, const SpawnParams& params)
        : Actor(actorName), spawnParams(params) {}

    void Initialize(const Transform& transform) override;
    void Update(float deltaTime) override;
    void SetPhysicsDamping(float linearDamping, float angularDamping, float sleepThreshold);
    void ActivateFromDeathPose(const Transform& transform,
        const DirectX::XMFLOAT3& initialVelocity,
        const DirectX::XMFLOAT3& angularVelocityDegrees);

private:
    const char* GetModelPath() const;
    void InitializeCollision();

    SpawnParams spawnParams;
    std::shared_ptr<SkeletalMeshComponent> meshComponent;
    std::shared_ptr<ShapeComponent> collisionComponent;
    float remainingLifetime = 0.0f;
    bool isActiveDebris = false;
};