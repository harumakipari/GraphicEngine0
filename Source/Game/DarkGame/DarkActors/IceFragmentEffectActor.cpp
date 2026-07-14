#include "pch.h"
#include "IceFragmentEffectActor.h"

void IceFragmentEffectActor::Initialize(const Transform& transform)
{
    meshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    meshComponent->SetModel("./Data/Models/ParticleMesh/Fragment/SM_Aurora_Fragment.gltf");

    rotationComponent = AddComponent<RotationComponent>("rotationComp", parentName);

}

void IceFragmentEffectActor::Update(float elapsedTime)
{

}

void IceFragmentEffectActor::DrawImGuiDetails()
{

}

// ”ò‚Ô•ûŒü‚ðŒˆ’è‚·‚é
void IceFragmentEffectActor::SetDirection(DirectX::XMFLOAT3 hitNormal)
{
    if (rotationComponent)
    {
        rotationComponent->SetDirection(hitNormal);
    }
}