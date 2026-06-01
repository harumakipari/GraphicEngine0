#pragma once
#include "Core/Actor.h"
#include "Components/Render/MeshComponent.h"
#include "Components/CollisionShape/ShapeComponent.h"

class WaterSphere :public Actor
{
public:
    WaterSphere(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override
    {
        std::shared_ptr<MorphMeshComponent> skeletalMesh = AddComponent<MorphMeshComponent>("morphMesh");
        //skeletalMesh->SetModel("./Data/Models/Morph/morphSphere.gltf", false);
        skeletalMesh->SetModel("./Data/Models/Morph/morphSphere_shape2.gltf", false);
        skeletalMesh->SetIsCastShadow(false);
    }


    void Update(float elapsedTime)override {}
};