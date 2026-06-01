#include "pch.h"
#include "TargetPudding.h"
#include "Game/Actors/Dessert/Cherry.h"

void TargetPudding::Initialize(const Transform& transform)
{
    // 描画用コンポーネントを追加
    elasticComponent = this->AddComponent<ElasticMeshComponent>("elasticComponent");
    elasticComponent->SetModel("./Data/TeamModels/Enemy/YarnBigEnemy.glb");
    //elasticComponent->SetModel("./Data/Models/cherry_pudding/pudding.glb");

    std::shared_ptr<BoxComponent> boxComponent = this->AddComponent<class BoxComponent>("boxComponent", "elasticComponent");
    DirectX::XMFLOAT3 size = elasticComponent->GetModelSize();
    boxComponent->SetBoxExtent({ size.x * 0.5f,size.y * 0.5f,size.z * 0.5f });
    boxComponent->SetCollisionOffsetY(size.y);
    boxComponent->SetMass(40.0f);
    boxComponent->SetLayer(CollisionLayer::WorldStatic);
    boxComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
    boxComponent->Initialize();

    whip = this->AddComponent<SkeletalMeshComponent>("whip", "elasticComponent");
    whip->SetModel("./Data/Models/cherry_pudding/whip.glb");
    whip->SetRelativeLocationDirect({ 0.0f,size.y,0.0f });

    particleComponent = this->AddComponent<class ParticleComponent>("particleComponent", "elasticComponent");
    particleComponent->Load("./Data/Effect/Files/testEffect.json");

    SetPosition(transform.GetLocation());
    SetQuaternionRotation(transform.GetRotation());
    SetScale(transform.GetScale());

    elasticComponent->Initialize();
    elasticComponent->SetUseMouseInput(false); // マウス入力によって引っ張られない

    AddHitCallback([&](std::pair<CollisionComponent*, CollisionComponent*> hitPair)
        {
            if (auto cherry = std::dynamic_pointer_cast<Cherry>(hitPair.second->GetActor()))
            {
                elasticComponent->AddImpulse({0.0f,-0.5f,0.0f});
                if (particleComponent)
                {
                    particleComponent->Play();
                }
            }
        });
}
