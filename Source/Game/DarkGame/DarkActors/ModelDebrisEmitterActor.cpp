#include "pch.h"
#include "ModelDebrisEmitterActor.h"

namespace
{
    constexpr const char* BlockModelPath =
        "./Data/Models/EffectDebri/Block/block1.gltf";
    constexpr const char* SmallDebriModelPath =
        "./Data/Models/EffectDebri/SmallDebri/debri.gltf";
}

void ModelDebrisEmitterActor::Initialize(const Transform& transform)
{
    debrisPool.reserve(BlockCount + SmallDebriCount);
    for (int i = 0; i < BlockCount; ++i)
        InitializeDebris(DebrisType::Block, i, BlockModelPath);
    for (int i = 0; i < SmallDebriCount; ++i)
        InitializeDebris(DebrisType::SmallDebri, i, SmallDebriModelPath);
}

void ModelDebrisEmitterActor::InitializeDebris(
    const DebrisType type, const int index, const char* modelPath)
{
    Debris debris;
    debris.type = type;
    const char* typeName = type == DebrisType::Block ? "Block" : "SmallDebri";
    debris.meshComponent = AddComponent<SkeletalMeshComponent>(std::string("ModelDebris_") + typeName + "_" + std::to_string(index), parentName);
    debris.meshComponent->SetModel(modelPath);
    debris.meshComponent->SetIsVisible(false);
    debrisPool.push_back(std::move(debris));
}

void ModelDebrisEmitterActor::Update(const float deltaTime)
{
    for (Debris& debris : debrisPool)
    {
        if (!debris.active)
            continue;

        debris.velocity.y -= gravity * deltaTime;
        debris.position.x += debris.velocity.x * deltaTime;
        debris.position.y += debris.velocity.y * deltaTime;
        debris.position.z += debris.velocity.z * deltaTime;
        debris.rotation.x += debris.angularVelocity.x * deltaTime;
        debris.rotation.y += debris.angularVelocity.y * deltaTime;
        debris.rotation.z += debris.angularVelocity.z * deltaTime;
        debris.remainingLifetime -= deltaTime;

        debris.meshComponent->SetWorldLocationDirect(debris.position);
        debris.meshComponent->SetRelativeEulerRotationDirect(debris.rotation);

        if (debris.remainingLifetime <= 0.0f)
            DeactivateDebris(debris);
    }
}

void ModelDebrisEmitterActor::Emit(const DirectX::XMFLOAT3& impactPosition)
{
    const float angleStep = DirectX::XM_2PI / static_cast<float>(debrisPool.size());
    for (size_t i = 0; i < debrisPool.size(); ++i)
        ActivateDebris(debrisPool[i], impactPosition, angleStep * static_cast<float>(i));
}

void ModelDebrisEmitterActor::ActivateDebris(
    Debris& debris, const DirectX::XMFLOAT3& impactPosition, const float baseAngleRadians)
{
    const float angle = baseAngleRadians + DirectX::XMConvertToRadians(
        MathHelper::RandomRange(-22.0f, 22.0f));
    const DirectX::XMFLOAT3 outward{ sinf(angle), 0.0f, cosf(angle) };

    const bool isBlock = debris.type == DebrisType::Block;
    const float horizontalSpeed = isBlock
        ? MathHelper::RandomRange(2.0f, 3.2f)
        : MathHelper::RandomRange(2.5f, 4.8f);
    const float verticalSpeed = isBlock
        ? MathHelper::RandomRange(1.8f, 2.6f)
        : MathHelper::RandomRange(2.2f, 3.8f);

    debris.active = true;
    debris.position = impactPosition;
    debris.position.y += 0.05f;
    debris.velocity = MathHelper::Multiply(outward, horizontalSpeed);
    debris.velocity.y = verticalSpeed;
    debris.rotation = {
        MathHelper::RandomRange(0.0f, 360.0f),
        MathHelper::RandomRange(0.0f, 360.0f),
        MathHelper::RandomRange(0.0f, 360.0f) };

    const float angularSpeedMin = isBlock ? 180.0f : 300.0f;
    const float angularSpeedMax = isBlock ? 420.0f : 720.0f;
    debris.angularVelocity = {
        MathHelper::RandomRange(-angularSpeedMax, angularSpeedMax),
        MathHelper::RandomRange(-angularSpeedMax, angularSpeedMax),
        MathHelper::RandomRange(-angularSpeedMax, angularSpeedMax) };
    // Avoid a nearly motionless roll while keeping each axis random.
    if (MathHelper::Length(debris.angularVelocity) < angularSpeedMin)
        debris.angularVelocity.y = angularSpeedMin;

    debris.remainingLifetime = isBlock
        ? MathHelper::RandomRange(0.8f, 1.0f)
        : MathHelper::RandomRange(0.6f, 0.9f);
    debris.scale = isBlock
        ? MathHelper::RandomRange(0.9f, 1.2f)
        : MathHelper::RandomRange(0.55f, 0.85f);

    debris.meshComponent->SetWorldLocationDirect(debris.position);
    debris.meshComponent->SetRelativeEulerRotationDirect(debris.rotation);
    debris.meshComponent->SetRelativeScaleDirect({ debris.scale, debris.scale, debris.scale });
    debris.meshComponent->SetIsVisible(true);
}

void ModelDebrisEmitterActor::DeactivateDebris(Debris& debris)
{
    debris.active = false;
    debris.remainingLifetime = 0.0f;
    debris.meshComponent->SetIsVisible(false);
}
