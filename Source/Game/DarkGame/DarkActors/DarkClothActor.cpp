#include "pch.h"
#include "DarkClothActor.h"

void DarkClothActor::Initialize(const Transform& transform)
{
    std::string clothFilename = "./Data/Models/Cloth/ClothModel.gltf";
    // クロスシミュレーション
    clothSimulate = std::make_unique<ClothSimulate>(Graphics::GetDevice(), clothFilename);
    std::string parentName = "Cloth";

    // ポールモデルを追加する
    meshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    meshComponent->SetModel("./Data/Models/Cloth/ClothPoleModel.gltf", false, true);

    // 布の位置コンポーネントを追加する
    clothPoint = AddComponent<SceneComponent>("clothPoint", parentName);
    clothPoint->SetRelativeLocationDirect({ 0.0f,-1.8f,0.0f });
    clothPoint->SetRelativeEulerRotationDirect({ 90.0f,0.0f,0.0f });
}

void DarkClothActor::Update(float deltaTime)
{
    if (clothSimulate)
    {
        clothSimulate->Update(deltaTime);
    }
}

void DarkClothActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    if (clothSimulate)
    {
        clothSimulate->DrawImGui();
    }
#endif
}

void DarkClothActor::RenderCloth(ID3D11DeviceContext* immediateContext)
{
    if (clothSimulate)
    {
        clothSimulate->Render(immediateContext, clothPoint->GetComponentWorldTransform().ToWorldTransform());
    }
}

