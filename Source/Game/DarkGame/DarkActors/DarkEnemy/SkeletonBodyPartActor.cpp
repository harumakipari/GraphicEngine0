#include "pch.h"
#include "SkeletonBodyPartActor.h"

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

    position = transform.GetLocation();
    initialBoneRotation = transform.GetRotation();
    velocity = spawnParams.initialVelocity;
    angularVelocity = spawnParams.angularVelocity;
    remainingLifetime = spawnParams.lifeTime;

    meshComponent = AddComponent<SkeletalMeshComponent>("SkeletonBodyPartMesh");
    meshComponent->SetModel(GetModelPath(), false, true);
    meshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Enemy;
    meshComponent->SetRelativeLocationDirect(spawnParams.transformCorrection.position);
    meshComponent->SetRelativeEulerRotationDirect(spawnParams.transformCorrection.rotationEuler);
    meshComponent->SetRelativeScaleDirect(spawnParams.transformCorrection.scale);

}

void SkeletonBodyPartActor::Update(float deltaTime)
{
    if (remainingLifetime <= 0.0f)
    {
        MarkPendingKill();
        return;
    }

    if (!hasLanded)
    {
        velocity.y -= spawnParams.gravity * deltaTime;
        position.x += velocity.x * deltaTime;
        position.y += velocity.y * deltaTime;
        position.z += velocity.z * deltaTime;
        accumulatedAngularRotation.x += angularVelocity.x * deltaTime;
        accumulatedAngularRotation.y += angularVelocity.y * deltaTime;
        accumulatedAngularRotation.z += angularVelocity.z * deltaTime;

        const float groundContactY = spawnParams.groundY + spawnParams.groundOffset;
        if (position.y <= groundContactY)
        {
            position.y = groundContactY;
            velocity = {};
            angularVelocity = {};
            hasLanded = true;
        }
    }

    ApplySimulatedTransform();
    remainingLifetime -= deltaTime;
    if (remainingLifetime <= 0.0f)
        MarkPendingKill();
}

void SkeletonBodyPartActor::ApplySimulatedTransform()
{
    using namespace DirectX;

    const XMVECTOR initial = XMLoadFloat4(&initialBoneRotation);
    const XMVECTOR spin = XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(accumulatedAngularRotation.x),
        XMConvertToRadians(accumulatedAngularRotation.y),
        XMConvertToRadians(accumulatedAngularRotation.z));
    XMFLOAT4 rotation{};
    XMStoreFloat4(&rotation, XMQuaternionNormalize(XMQuaternionMultiply(initial, spin)));

    SetPosition(position);
    SetQuaternionRotation(rotation);
    UpdateAllComponentTransforms();
}
