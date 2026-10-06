#pragma once

#include "Core/Actor.h"

class SkeletalMeshComponent;
// Short-lived visual debris used by the Skeleton death effect. Motion is
// intentionally simulated by this actor, rather than registered with PhysX.
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
        DirectX::XMFLOAT3 angularVelocity{}; // degrees per second, local axes
        float gravity = 9.8f;
        float groundY = 0.0f;
        float groundOffset = 0.0f;
        float lifeTime = 30.0f;
        TransformCorrection transformCorrection{};
    };

    SkeletonBodyPartActor(const std::string& actorName, const SpawnParams& params)
        : Actor(actorName), spawnParams(params) {}

    void Initialize(const Transform& transform) override;
    void Update(float deltaTime) override;

private:
    const char* GetModelPath() const;
    void ApplySimulatedTransform();

    SpawnParams spawnParams;
    std::shared_ptr<SkeletalMeshComponent> meshComponent;
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 velocity{};
    DirectX::XMFLOAT3 angularVelocity{};
    DirectX::XMFLOAT3 accumulatedAngularRotation{};
    DirectX::XMFLOAT4 initialBoneRotation{};
    float remainingLifetime = 0.0f;
    bool hasLanded = false;
};
