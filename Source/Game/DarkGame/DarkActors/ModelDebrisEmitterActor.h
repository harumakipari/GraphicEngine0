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

private:
    void InitializeDebris(DebrisType type, int index, const char* modelPath);
    void ActivateDebris(Debris& debris, const DirectX::XMFLOAT3& impactPosition,
        float baseAngleRadians);
    void DeactivateDebris(Debris& debris);

    static constexpr int BlockCount = 3;
    static constexpr int SmallDebriCount = 7;

    std::vector<Debris> debrisPool;
    std::string parentName = "RootComponent";

    float gravity = 9.8f;
};
