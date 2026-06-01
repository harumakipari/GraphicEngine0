#include "pch.h"
#include "DarkStageBarrelActor.h"

#include "Engine/Input/InputSystem.h"

void DarkStageBarrelActor::Initialize(const Transform& transform)
{
    std::string parentName = "barrelMesh";

    // ’M‚Ìƒ‚ƒfƒ‹‚ğ’Ç‰Á
    barrelMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    barrelMeshComponent->SetModel("./Data/Models/DarkStageAssets/Barrel/SM_Barrel_01.gltf");
    barrelMeshComponent->SetIsCastShadow(false);    // ‰e‚ğ—‚Æ‚³‚È‚¢‚æ‚¤‚É‚·‚é

    // ’M‚Ì‚ª‚ê‚«‚Ég—p‚·‚éƒ‚ƒfƒ‹
    auto barrelConvexMeshComponent = AddComponent<SkeletalMeshComponent>("barrelConvexMesh", parentName);
    barrelConvexMeshComponent->SetModel("./Data/Models/DarkStageAssets/Barrel_Convex/barrel_convex.gltf", true);
    barrelConvexMeshComponent->SetIsVisible(false);
    barrelConvexMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::NotSSR;   // SSR‚ğ‰e‹¿‚³‚¹‚½‚­‚È‚¢‚©‚ç

    // Å‰‚Ì‰ó‚ê‚é‘O‚Ì” ‚Ì“–‚½‚è”»’è
    preBoxComponent = AddComponent<BoxComponent>("boxComponent", parentName);
    DirectX::XMFLOAT3 size = barrelMeshComponent->model->GetModelSize();
    preBoxComponent->SetBoxExtent(size);
    float height = size.y * 0.5f;
    preBoxComponent->SetCollisionOffsetY(height);
    preBoxComponent->SetStatic(true);
    preBoxComponent->SetLayer(CollisionLayer::WorldProps);
    preBoxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
    preBoxComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
    preBoxComponent->Initialize();


    // ’M‚ÌŠ¢âI
    convexComponent = AddComponent<ConvexCollisionComponent>("convexComponent", parentName);
    convexComponent->SetLayer(CollisionLayer::Convex);
    convexComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
    convexComponent->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Block);
    convexComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
    convexComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
    convexComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
    convexComponent->SetActive(false);
    convexComponent->CreateConvexMeshFromModel(barrelConvexMeshComponent.get());
    convexComponent->SetKinematic(true);
}

void DarkStageBarrelActor::Update(float deltaTime)
{
    if (InputSystem::GetInputState("Z"))
    {
        BreakBarrel();
    }
}

void DarkStageBarrelActor::BreakBarrel()
{
    // Œ³X‚Ì” ‚Ì“–‚½‚è”»’è‚ğÁ‚·
    preBoxComponent->DisableCollision();
    RequestDestroyComponent("boxComponent");
    // Š¢âI‚ğ“–‚½‚è”»’è‚É“ü‚ê‚é
    if (convexComponent)
    {
        convexComponent->AddToScene(); // ‚±‚±‚Å physx ‚Ì scene ‚É’Ç‰Á‚·‚é@‚±‚±‚Ü‚Å‚Í•¨—‰‰Z‚Ìl—¶‚É“ü‚ê‚½‚­‚È‚¢‚©‚ç
        convexComponent->SetKinematic(false);
        convexComponent->SetActive(true);
    }

    // Š¢âI‚Ìƒ‚ƒfƒ‹‚ğ•\¦‚·‚é
    if (auto convexMesh = std::dynamic_pointer_cast<SkeletalMeshComponent>(FindComponentByName("barrelConvexMesh")))
    {
        convexMesh->SetIsVisible(true);
    }
    // Œ³‚Ìƒ‚ƒfƒ‹‚ğÁ‚·
    barrelMeshComponent->SetIsVisible(false);
}

void DarkStageBarrelActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    if (ImGui::Button(U8("”j‰ó")))
    {
        BreakBarrel();
    }
#endif
}