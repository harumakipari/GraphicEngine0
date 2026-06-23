#pragma once
#include "Core/Actor.h"
#include "Components/Render/MeshComponent.h"
#include "Components/CollisionShape/ShapeComponent.h"
class Stage :public Actor
{
public:
    Stage(std::string modelName) :Actor(modelName)
    {
        //auto modelComponent = std::make_shared<StaticMeshComponent>(this, "..\\glTF-Sample-Models-main\\original\\ExampleStage.gltf");
        //AddComponent(modelComponent);

        //scale = { 10.0f,10.0f,10.0f };
        //position.y = -3.0f;
    }

    void Initialize(const Transform& transform)override
    {
        std::shared_ptr<StaticMeshComponent> staticMeshComponent = this->AddComponent<class StaticMeshComponent>("staticMeshComponent");
        staticMeshComponent->SetModel("./Data/Models/Stage/ExampleStage.gltf", true);
        staticMeshComponent->SetRelativeScaleDirect(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        staticMeshComponent->SetRelativeLocationDirect(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

        std::shared_ptr<TriangleMeshCollisionComponent> triangleMeshComponent = this->AddComponent<class TriangleMeshCollisionComponent>("triangleMeshComponent", "staticMeshComponent");
        triangleMeshComponent->CreateConvexMeshFromModel(staticMeshComponent.get());
    }

    void Update(float elapsedTime)override {}
};