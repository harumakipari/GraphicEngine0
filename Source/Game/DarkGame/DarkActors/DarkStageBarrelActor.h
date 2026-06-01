#pragma once
#include "Core/Actor.h"

class DarkStageBarrelActor :public Actor
{
public:
    DarkStageBarrelActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStageBarrelActor() = default;
    void Initialize(const Transform& transform)override;

    void Update(float deltaTime) override;

    void DrawImGuiDetails() override;

private:
    // ’M‚ª‰ó‚ê‚éˆ—
    void BreakBarrel();
private:
    // ’M‚Ìƒ‚ƒfƒ‹
    std::shared_ptr<SkeletalMeshComponent> barrelMeshComponent;
    // Å‰‚Ì‰ó‚ê‚é‘O‚Ì” ‚Ì“–‚½‚è”»’è
    std::shared_ptr<BoxComponent> preBoxComponent;
    // ’M‚ª‰ó‚ê‚½Œã‚Ì“–‚½‚è”»’è
    std::shared_ptr<ConvexCollisionComponent> convexComponent;
};