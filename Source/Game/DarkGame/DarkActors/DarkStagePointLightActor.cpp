#include "pch.h"
#include "DarkStagePointLightActor.h"

void DarkStagePointLightActor::Initialize(const Transform& transform)
{
    std::string parentName = "pointLight";
    // ポイントライトコンポーネントを追加
    pointLightComponent = this->AddComponent<PointLightComponent>(parentName);

#if 0
    // エミッションを発生させるためにモデルを追加
    sphereMeshComponent = this->AddComponent<SkeletalMeshComponent>("sphereMeshComponent", parentName);
    sphereMeshComponent->SetModel("./Data/Models/Primitives/Sphere.glb");
    sphereMeshComponent->overrideDeferredPipelineName = "pointLightSkeletalMesh";
    sphereMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    sphereMeshComponent->SetRelativeScaleDirect({ 0.01f,0.01f,0.01f });
    sphereMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
    sphereMeshComponent->plusAlphaCBuffer->data.emissionPower = 10.0f;
#else
    // エミッションを発生させるためにモデルを追加
    sphereMeshComponent = this->AddComponent<InstanceMeshComponent>("sphereMeshComponent", parentName);
    sphereMeshComponent->SetModel("./Data/Models/Primitives/frame.glb");
    sphereMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    sphereMeshComponent->SetRelativeScaleDirect({ 0.01f,0.02f,0.01f });
    sphereMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
    sphereMeshComponent->plusAlphaCBuffer->data.emissionPower = 10.0f;
#endif // 0

}


