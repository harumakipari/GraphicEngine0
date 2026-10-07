#include "pch.h"
#include "SkeletonBodyPartActor.h"

#include "Components/CollisionShape/ShapeComponent.h"
#include "Components/Render/MeshComponent.h"

const char* SkeletonBodyPartActor::GetModelPath() const
{
    switch (spawnParams.partType)
    {
    case PartType::Skull:
        return "./Data/Models/Characters/Skeleton/BodyParts/SM_Skeleton_Skull_01.gltf";
    case PartType::Ribs:
        return "./Data/Models/Characters/Skeleton/BodyParts/SM_Skeleton_Ribs_01.gltf";
    case PartType::Spine:
        return "./Data/Models/Characters/Skeleton/BodyParts/SM_Skeleton_Spine_01.gltf";
    case PartType::Arm:
        return "./Data/Models/Characters/Skeleton/BodyParts/SM_Skeleton_Arm_01.gltf";
    case PartType::Leg:
        return "./Data/Models/Characters/Skeleton/BodyParts/SM_Skeleton_Leg_01.gltf";
    }

    return "";
}

void SkeletonBodyPartActor::Initialize(const Transform& transform)
{
    Actor::Initialize(transform);
    remainingLifetime = spawnParams.lifeTime;

    meshComponent = AddComponent<SkeletalMeshComponent>("SkeletonBodyPartMesh");
    meshComponent->SetModel(GetModelPath(), false, true);
    meshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Enemy;
    meshComponent->SetRelativeLocationDirect(spawnParams.transformCorrection.position);
    meshComponent->SetRelativeEulerRotationDirect(spawnParams.transformCorrection.rotationEuler);
    meshComponent->SetRelativeScaleDirect(spawnParams.transformCorrection.scale);

    InitializeCollision();

    // Pooled resources remain inert until this Skeleton dies.
    meshComponent->SetIsVisible(false);
    collisionComponent->SetKinematic(true);
    collisionComponent->DisableCollision();
}

void SkeletonBodyPartActor::InitializeCollision()
{
    if (!meshComponent)
        return;

    DirectX::XMFLOAT3 size = meshComponent->GetModelSize();
    size = MathHelper::Multiply(size, 100.0f);
    if (spawnParams.partType == PartType::Skull)
    {
        const float radius = (std::max)(0.01f, (std::max)(size.x, (std::max)(size.y, size.z)) * 0.5f);
        auto sphere = AddComponent<SphereComponent>("SkeletonBodyPartCollision");
        sphere->SetRadius(radius);
        collisionComponent = sphere;
    }
    else
    {
        auto box = AddComponent<BoxComponent>("SkeletonBodyPartCollision");
        box->SetBoxExtent({
            (std::max)(size.x, 0.02f),
            (std::max)(size.y, 0.02f),
            (std::max)(size.z, 0.02f) });
        collisionComponent = box;
    }

    collisionComponent->SetMass(1.0f);
    collisionComponent->SetKinematic(true);
    collisionComponent->SetGravity(true);
    collisionComponent->SetLayer(CollisionLayer::BodyPart);
    collisionComponent->SetResponseToLayer(
        CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Block);
    //collisionComponent->SetIsVisibleDebugBox(spawnParams.debugCollisionShape);
    //collisionComponent->SetIsVisibleDebugShape(spawnParams.debugCollisionShape);
    collisionComponent->SetCollisionOffsetY(size.y * 0.5f);
    if (spawnParams.partType != PartType::Skull || spawnParams.partType != PartType::Ribs)  // “ªŠWœ‚©‚ ‚Î‚çœˆÈŠO‚¾‚Á‚½‚ç
        collisionComponent->SetCollisionOffsetX(-size.x * 0.5f);
    collisionComponent->Initialize();
    SetPhysicsDamping(spawnParams.linearDamping, spawnParams.angularDamping, spawnParams.sleepThreshold);
}

void SkeletonBodyPartActor::SetPhysicsDamping(float linearDamping, float angularDamping, float sleepThreshold)
{
    if (!collisionComponent)
        return;

    collisionComponent->SetLinearDamping(linearDamping);
    collisionComponent->SetAngularDamping(angularDamping);
    collisionComponent->SetSleepThreshold(sleepThreshold);
}
void SkeletonBodyPartActor::ActivateFromDeathPose(const Transform& transform,
    const DirectX::XMFLOAT3& initialVelocity, const DirectX::XMFLOAT3& angularVelocityDegrees)
{
    if (!meshComponent || !collisionComponent || isActiveDebris)
        return;

    // Transform -> PhysX pose -> visible -> collision -> dynamic -> velocities.
    SetPosition(transform.GetLocation());
    SetQuaternionRotation(transform.GetRotation());
    SetScale(transform.GetScale());
    UpdateAllComponentTransforms();
    collisionComponent->SetPhysicsWorldTransform(collisionComponent->GetComponentWorldTransform());
    meshComponent->SetIsVisible(true);
    collisionComponent->EnableCollision();
    collisionComponent->SetKinematic(false);
    collisionComponent->SetIntialVelocity(initialVelocity);
    constexpr float degreesToRadians = DirectX::XM_PI / 180.0f;
    collisionComponent->SetInitialAngularVelocity({
        angularVelocityDegrees.x * degreesToRadians,
        angularVelocityDegrees.y * degreesToRadians,
        angularVelocityDegrees.z * degreesToRadians });
    isActiveDebris = true;
}

void SkeletonBodyPartActor::Update(float deltaTime)
{
    if (isActiveDebris && remainingLifetime > 0.0f)
    {
        remainingLifetime -= deltaTime;
        if (remainingLifetime <= 0.0f)
            MarkPendingKill();
    }
}
