#pragma once
#include "Core/Actor.h"
#include "Components/Render/PointLightComponent.h"

class DarkStagePointLightActor :public Actor
{
public:
    DarkStagePointLightActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStagePointLightActor() = default;
    void Initialize(const Transform& transform)override;

    std::shared_ptr<PointLightComponent> GetPointLightComponent() const { return pointLightComponent; }
private:
    std::shared_ptr<PointLightComponent> pointLightComponent;
    std::shared_ptr<MeshComponent> sphereMeshComponent;
};