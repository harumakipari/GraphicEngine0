#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Core/Actor.h"
#include "Components/Render/PointLightComponent.h"

class IceFragmentEffectActor :public Actor
{
public:
    explicit IceFragmentEffectActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    void DrawImGuiDetails() override;

    // ”ò‚Ô•ûŒü‚ðŒˆ’è‚·‚é
    void SetDirection(DirectX::XMFLOAT3 hitNormal);

private:
    std::string parentName = "RootComponent";

    std::shared_ptr<SkeletalMeshComponent> meshComponent;
    std::shared_ptr<RotationComponent> rotationComponent;

};