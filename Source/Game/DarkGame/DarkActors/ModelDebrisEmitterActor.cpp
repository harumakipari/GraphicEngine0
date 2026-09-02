#include "pch.h"
#include "ModelDebrisEmitterActor.h"

namespace
{
    constexpr const char* BlockModelPaths[] = {
        "./Data/Models/EffectDebri/Block/block1.gltf",
        "./Data/Models/EffectDebri/Block/block2.gltf",
        "./Data/Models/EffectDebri/Block/block3.gltf",
        "./Data/Models/EffectDebri/Block/block4.gltf",
    };
    constexpr const char* SmallDebriModelPaths[] = {
        "./Data/Models/EffectDebri/SmallDebri/debri.gltf",
        "./Data/Models/EffectDebri/SmallDebri/debri1.gltf",
        "./Data/Models/EffectDebri/SmallDebri/debri2.gltf",
    };

#ifdef USE_IMGUI
    void DrawNonNegativeRange(const char* label, float& minimum, float& maximum,
        const float speed)
    {
        ImGui::DragFloatRange2(label, &minimum, &maximum, speed,
            0.0f, FLT_MAX, "Min: %.2f", "Max: %.2f");
        minimum = (std::max)(minimum, 0.0f);
        maximum = (std::max)(maximum, minimum);
    }

#endif
}

void ModelDebrisEmitterActor::Initialize(const Transform& transform)
{
    debrisPool.reserve(MaxBlockPoolCount + MaxSmallDebriPoolCount);
    for (int i = 0; i < MaxBlockPoolCount; ++i)
        InitializeDebris(DebrisType::Block, i, BlockModelPaths[i]);
    for (int i = 0; i < MaxSmallDebriPoolCount; ++i)
        InitializeDebris(DebrisType::SmallDebri, i,
            SmallDebriModelPaths[i % std::size(SmallDebriModelPaths)]);
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
    const int emitCount = activeBlockCount + activeSmallDebriCount;
    const float angleStep = emitCount > 0
        ? DirectX::XM_2PI / static_cast<float>(emitCount)
        : 0.0f;
    int blockIndex = 0;
    int smallDebriIndex = 0;
    int emittedIndex = 0;
    for (Debris& debris : debrisPool)
    {
        const bool shouldEmit = debris.type == DebrisType::Block
            ? blockIndex++ < activeBlockCount
            : smallDebriIndex++ < activeSmallDebriCount;
        if (shouldEmit)
        {
            ActivateDebris(debris, impactPosition,
                angleStep * static_cast<float>(emittedIndex++));
        }
        else
        {
            DeactivateDebris(debris);
        }
    }
}

void ModelDebrisEmitterActor::ActivateDebris(
    Debris& debris, const DirectX::XMFLOAT3& impactPosition, const float baseAngleRadians)
{
    const float angle = baseAngleRadians + DirectX::XMConvertToRadians(
        MathHelper::RandomRange(-spawnAngleRandomRange, spawnAngleRandomRange));
    const DirectX::XMFLOAT3 outward{ sinf(angle), 0.0f, cosf(angle) };

    const bool isBlock = debris.type == DebrisType::Block;
    const MotionSettings& settings = isBlock ? blockSettings : smallDebriSettings;
    const float horizontalSpeed = MathHelper::RandomRange(
        settings.horizontalSpeedMin, settings.horizontalSpeedMax);
    const float verticalSpeed = MathHelper::RandomRange(
        settings.upSpeedMin, settings.upSpeedMax);

    debris.active = true;
    debris.position = impactPosition;
    debris.position.y += 0.05f;
    debris.velocity = MathHelper::Multiply(outward, horizontalSpeed);
    debris.velocity.y = verticalSpeed;
    debris.rotation = {
        MathHelper::RandomRange(0.0f, 360.0f),
        MathHelper::RandomRange(0.0f, 360.0f),
        MathHelper::RandomRange(0.0f, 360.0f) };

    const float angularSpeedMin = settings.angularVelocityMinimumMagnitude;
    const float angularSpeedMax = settings.angularVelocityMax;
    debris.angularVelocity = {
        MathHelper::RandomRange(-angularSpeedMax, angularSpeedMax),
        MathHelper::RandomRange(-angularSpeedMax, angularSpeedMax),
        MathHelper::RandomRange(-angularSpeedMax, angularSpeedMax) };
    // Avoid a nearly motionless roll while keeping each axis random.
    if (MathHelper::Length(debris.angularVelocity) < angularSpeedMin)
        debris.angularVelocity.y = angularSpeedMin;

    debris.remainingLifetime = MathHelper::RandomRange(
        settings.lifetimeMin, settings.lifetimeMax);
    debris.scale = MathHelper::RandomRange(settings.scaleMin, settings.scaleMax);

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

void ModelDebrisEmitterActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    const auto drawMotionSettings = [](MotionSettings& settings)
        {
            DrawNonNegativeRange("Horizontal Speed", settings.horizontalSpeedMin,
                settings.horizontalSpeedMax, 0.05f);
            DrawNonNegativeRange("Up Speed", settings.upSpeedMin,
                settings.upSpeedMax, 0.05f);
            DrawNonNegativeRange("Lifetime", settings.lifetimeMin,
                settings.lifetimeMax, 0.01f);
            DrawNonNegativeRange("Scale", settings.scaleMin,
                settings.scaleMax, 0.01f);
            ImGui::DragFloat("Angular Velocity Max", &settings.angularVelocityMax,
                5.0f, 0.0f, FLT_MAX, "%.0f deg/sec");
            ImGui::DragFloat("Minimum Rotation", &settings.angularVelocityMinimumMagnitude,
                5.0f, 0.0f, FLT_MAX, "%.0f deg/sec");
            settings.angularVelocityMax = (std::max)(settings.angularVelocityMax, 0.0f);
            settings.angularVelocityMinimumMagnitude = std::clamp(
                settings.angularVelocityMinimumMagnitude, 0.0f,
                settings.angularVelocityMax);
        };

    if (!ImGui::CollapsingHeader("Model Debris", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (ImGui::TreeNodeEx("Common", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Gravity", &gravity, 0.1f, 0.0f, FLT_MAX, "%.2f");
        ImGui::DragFloat("Angle Random", &spawnAngleRandomRange,
            0.5f, 0.0f, 180.0f, "+/- %.1f deg");
        gravity = (std::max)(gravity, 0.0f);
        spawnAngleRandomRange = std::clamp(spawnAngleRandomRange, 0.0f, 180.0f);
        ImGui::SliderInt("Active Block Count", &activeBlockCount,
            0, MaxBlockPoolCount);
        ImGui::SliderInt("Active Small Count", &activeSmallDebriCount,
            0, MaxSmallDebriPoolCount);
        ImGui::TextDisabled("Fixed Pool: Block %d / Small %d",
            MaxBlockPoolCount, MaxSmallDebriPoolCount);
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Block", ImGuiTreeNodeFlags_DefaultOpen))
    {
        drawMotionSettings(blockSettings);
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Small Debri", ImGuiTreeNodeFlags_DefaultOpen))
    {
        drawMotionSettings(smallDebriSettings);
        ImGui::TreePop();
    }

    if (ImGui::Button("Emit Test"))
        Emit(GetPosition());
#endif
}
