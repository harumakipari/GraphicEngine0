#pragma once
#include "Game/Actors/Base/Character.h"

class TitlePlayer :public Character
{
public:
    explicit TitlePlayer(const std::string& modelName) :Character(modelName){}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void DrawImGuiDetails()override;

    void Finalize()override {}

private:
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<SkeletalMeshComponent> swordMeshComponent;
};
