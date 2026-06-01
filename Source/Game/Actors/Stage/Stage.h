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
        SetEulerRotation(DirectX::XMFLOAT3(0.0f, 180.0f, 0.0f));
        staticMeshComponent->SetRelativeScaleDirect(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        staticMeshComponent->SetRelativeLocationDirect(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
        // 当たり判定のコンポーネントを追加
        //std::shared_ptr<BoxComponent> boxComponent = this->AddComponent<class BoxComponent>("boxComponent", "staticMeshComponent");
        //boxComponent->SetHalfBoxExtent(DirectX::XMFLOAT3(20.0f, 0.2f, 20.0f));
        ////boxComponent->SetRelativeLocationDirect(DirectX::XMFLOAT3(0.0f, 0.2f, 0.0f));
        //SetPosition(DirectX::XMFLOAT3(0.0f, -0.0f, 0.0f));
        //boxComponent->SetStatic(true);
        //boxComponent->SetLayer(CollisionLayer::WorldStatic);
        ////boxComponent->SetCollisionOffsetY(-0.4f);
        //boxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        //boxComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
        //boxComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
        //boxComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
        ////boxComponent->SetKinematic(false); // dynamic にする
        ////boxComponent->AddCollisionWith(CollisionLayer::Player);
        ////boxComponent->AddCollisionWith(CollisionLayer::WorldStatic);
        ////boxComponent->AddCollisionWith(CollisionLayer::Convex);
        //boxComponent->Initialize();
#if 0


        // 手前の壁当たり判定のコンポーネントを追加
        std::shared_ptr<BoxComponet> wallFrontComponent = this->AddComponent<class BoxComponet>("wallFrontComponent", "staticMeshComponent");
        wallFrontComponent->SetHalfBoxExtent(DirectX::XMFLOAT3(20.0f, 6.0f, 1.0f));
        wallFrontComponent->SetRelativeLocationDirect(DirectX::XMFLOAT3(0.0f, 0.0f, 15.6f));
        wallFrontComponent->SetModelHeight(6.0f * 0.5f);
        wallFrontComponent->SetStatic(true);
        wallFrontComponent->SetLayer(CollisionLayer::WorldStatic);
        wallFrontComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        wallFrontComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
        wallFrontComponent->SetResponseToLayer(CollisionLayer::PlayerSide, CollisionComponent::CollisionResponse::Block);
        wallFrontComponent->SetResponseToLayer(CollisionLayer::Bomb, CollisionComponent::CollisionResponse::Block);
        wallFrontComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
        wallFrontComponent->SetResponseToLayer(CollisionLayer::PickUpItem, CollisionComponent::CollisionResponse::Block);
        wallFrontComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
        wallFrontComponent->Initialize();

        // 奥の壁当たり判定のコンポーネントを追加
        std::shared_ptr<BoxComponet> wallBackComponent = this->AddComponent<class BoxComponet>("wallBackComponent", "staticMeshComponent");
        wallBackComponent->SetHalfBoxExtent(DirectX::XMFLOAT3(20.0f, 6.0f, 1.0f));
        wallBackComponent->SetRelativeLocationDirect(DirectX::XMFLOAT3(0.0f, 0.0f, -15.6f));
        wallBackComponent->SetModelHeight(6.0f * 0.5f);
        wallBackComponent->SetStatic(true);
        wallBackComponent->SetLayer(CollisionLayer::WorldStatic);
        wallBackComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        wallBackComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
        wallBackComponent->SetResponseToLayer(CollisionLayer::PlayerSide, CollisionComponent::CollisionResponse::Block);
        wallBackComponent->SetResponseToLayer(CollisionLayer::Bomb, CollisionComponent::CollisionResponse::Block);
        wallBackComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
        wallBackComponent->SetResponseToLayer(CollisionLayer::PickUpItem, CollisionComponent::CollisionResponse::Block);
        wallBackComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
        wallBackComponent->Initialize();

        // 右の壁当たり判定のコンポーネントを追加
        std::shared_ptr<BoxComponet> rightWallComponent = this->AddComponent<class BoxComponet>("rightWallComponent", "staticMeshComponent");
        rightWallComponent->SetHalfBoxExtent(DirectX::XMFLOAT3(1.0f, 6.0f, 16.0f));
        rightWallComponent->SetRelativeLocationDirect(DirectX::XMFLOAT3(20.5f, 0.0f, 0.6f));
        rightWallComponent->SetModelHeight(6.0f * 0.5f);
        rightWallComponent->SetStatic(true);
        rightWallComponent->SetLayer(CollisionLayer::WorldStatic);
        rightWallComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        rightWallComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
        rightWallComponent->SetResponseToLayer(CollisionLayer::PlayerSide, CollisionComponent::CollisionResponse::Block);
        rightWallComponent->SetResponseToLayer(CollisionLayer::Bomb, CollisionComponent::CollisionResponse::Block);
        rightWallComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
        rightWallComponent->SetResponseToLayer(CollisionLayer::PickUpItem, CollisionComponent::CollisionResponse::Block);
        rightWallComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
        rightWallComponent->Initialize();

        // 左の壁当たり判定のコンポーネントを追加
        std::shared_ptr<BoxComponet> leftWallComponent = this->AddComponent<class BoxComponet>("leftWallComponent", "staticMeshComponent");
        leftWallComponent->SetHalfBoxExtent(DirectX::XMFLOAT3(1.0f, 6.0f, 16.0f));
        leftWallComponent->SetRelativeLocationDirect(DirectX::XMFLOAT3(-20.5f, 0.0f, 0.6f));
        leftWallComponent->SetModelHeight(6.0f * 0.5f);
        leftWallComponent->SetStatic(true);
        leftWallComponent->SetLayer(CollisionLayer::WorldStatic);
        leftWallComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        leftWallComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
        leftWallComponent->SetResponseToLayer(CollisionLayer::PlayerSide, CollisionComponent::CollisionResponse::Block);
        leftWallComponent->SetResponseToLayer(CollisionLayer::Bomb, CollisionComponent::CollisionResponse::Block);
        leftWallComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
        leftWallComponent->SetResponseToLayer(CollisionLayer::PickUpItem, CollisionComponent::CollisionResponse::Block);
        leftWallComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
        leftWallComponent->Initialize();



#endif // 0

        std::shared_ptr<TriangleMeshCollisionComponent> triangleMeshComponent = this->AddComponent<class TriangleMeshCollisionComponent>("triangleMeshComponent", "staticMeshComponent");
        triangleMeshComponent->CreateConvexMeshFromModel(staticMeshComponent.get());
    }

    void Update(float elapsedTime)override {}
};