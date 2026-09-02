#pragma once

#include "Core/Actor.h"
#include "Components/Render/MeshComponent.h"

class ModelDebrisEmitterActor : public Actor
{
private:
    enum class DebrisType : uint8_t
    {
        Block,
        SmallDebri,
    };

    struct Debris
    {
        bool active = false;
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT3 velocity{};
        DirectX::XMFLOAT3 rotation{};
        DirectX::XMFLOAT3 angularVelocity{};
        float remainingLifetime = 0.0f;
        float scale = 1.0f;
        DebrisType type = DebrisType::Block;
        std::shared_ptr<SkeletalMeshComponent> meshComponent;
    };

public:
    explicit ModelDebrisEmitterActor(const std::string& actorName) : Actor(actorName) {}

    void Initialize(const Transform& transform) override;
    void Update(float deltaTime) override;
    void Emit(const DirectX::XMFLOAT3& impactPosition);
    void Emit(const DirectX::XMFLOAT3& impactPosition,
        const DirectX::XMFLOAT3& outwardDirection);
    void DrawImGuiDetails() override;

private:
    struct MotionSettings
    {
        float horizontalSpeedMin;
        float horizontalSpeedMax;
        float upSpeedMin;
        float upSpeedMax;
        float lifetimeMin;
        float lifetimeMax;
        float scaleMin;
        float scaleMax;
        float angularVelocityMax;
        float angularVelocityMinimumMagnitude;
    };

    void InitializeDebris(DebrisType type, int index, const char* modelPath);
    void ActivateDebris(Debris& debris, const DirectX::XMFLOAT3& impactPosition,
        float baseAngleRadians);
    void ActivateDirectionalDebris(Debris& debris,
        const DirectX::XMFLOAT3& impactPosition,
        const DirectX::XMFLOAT3& outwardDirection);
    void DeactivateDebris(Debris& debris);

    static constexpr int MaxBlockPoolCount = 4;
    static constexpr int MaxSmallDebriPoolCount = 8;

    std::vector<Debris> debrisPool;
    std::string parentName = "RootComponent";

    float gravity = 9.8f;
    float spawnAngleRandomRange = 22.0f;
    int activeBlockCount = 4;
    int activeSmallDebriCount = 8;
    MotionSettings blockSettings
    {
        2.5f, 3.5f,
        3.f, 4.f,
        1.2f, 1.5f,
        1.3f, 1.7f,
        420.0f, 180.0f };
    MotionSettings smallDebriSettings{
        3.3f, 5.2f,
        2.5f, 4.2f,
        1.4f, 1.7f,
        2.f, 3.f,
        720.0f, 300.0f };
};
