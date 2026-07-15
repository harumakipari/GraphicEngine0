#pragma once
#include "Core/Actor.h"
#include "Components/Render/PointLightComponent.h"

class LanternActor :public Actor
{
public:
    explicit LanternActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void DrawImGuiDetails() override;

private:
    std::string parentName = "RootComponent";
    std::shared_ptr<SkeletalMeshComponent> meshComponent;

    std::shared_ptr<PointLightComponent> titleRoomLightComponent0;
    std::shared_ptr<PointLightComponent> titleRoomLightComponent1;

    float swingTime = 0.0f;
    float swingSpeed = 0.9f;     // 揺れる速さ
    float swingAngle = 5.0f;    // 最大角度（度）
};