#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Core/Actor.h"
#include "Components/Render/PointLightComponent.h"

class IceFragmentEmitterActor :public Actor
{
private:
    struct Fragment
    {
        std::shared_ptr<InstanceMeshComponent> meshComponent;

        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 velocity;

        DirectX::XMFLOAT3 rotation;
        DirectX::XMFLOAT3 rotationSpeed;

        float scale;
        float life;
    };

public:
    explicit IceFragmentEmitterActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    void DrawImGuiDetails() override;

    // ”ò‚Ô•ûŒü‚ğŒˆ’è‚·‚é
    void SetDirection(DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal);

private:
    std::string parentName = "RootComponent";

    std::shared_ptr<RotationComponent> rotationComponent;

    std::vector<Fragment> fragments;


    // ’²®’l
    float spreadAngle = 20.0f;      // L‚ª‚èŠp“x
    float speedMin = 5.0f;
    float speedMax = 12.0f;
    float gravity = 18.0f;
    float lifeTime = 0.7f;
    int fragmentCount = 15;
};