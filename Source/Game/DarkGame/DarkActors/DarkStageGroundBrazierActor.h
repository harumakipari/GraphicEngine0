#pragma once
#include "DarkStageAsset.h"
#include "Core/Actor.h"

class DarkStageGroundBrazierActor :public Actor
{
public:
    DarkStageGroundBrazierActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStageGroundBrazierActor() = default;
    void Initialize(const Transform& transform)override;
    void SetModel(const std::shared_ptr<StageAsset>& stageAsset);

private:
    // ’n–Ê‚Ì‰Î”«‚Ìƒ‚ƒfƒ‹
    std::shared_ptr<MeshComponent> brazierMeshComponent;
};