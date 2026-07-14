#include "pch.h"
#include "LanternActor.h"

void LanternActor::Initialize(const Transform& transform)
{
    meshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    meshComponent->SetModel("./Data/Models/DarkStageAssets/Lantern/scene.gltf");
}

void LanternActor::Update(float elapsedTime)
{
    
}

void LanternActor::DrawImGuiDetails()
{
    
}
