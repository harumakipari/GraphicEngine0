#include "pch.h"
#include "GruxEnemyEyeActor.h"

#include "GruxEnemy.h"
#include "Engine/Scene/Scene.h"

void GruxEnemyEyeActor::Initialize(const Transform& transform)
{
    std::string parentName = GetRootComponentName();
    // 左目の描画用コンポーネントを追加　暗闇で光る目の表現用
    leftEyeMeshComponent = this->AddComponent<SkeletalMeshComponent>("leftEye", parentName);
    leftEyeMeshComponent->SetModel("./Data/Models/Primitives/Sphere.glb");
    leftEyeMeshComponent->overrideDeferredPipelineName = "EnemyEyeModelPS";
    leftEyeMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    leftEyeMeshComponent->SetRelativeScaleDirect({ 0.03f,0.02f,0.02f });
    leftEyeMeshComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-0.1f });
    leftEyeMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
    leftEyeMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;
    leftEyeMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::EnemyEye;

    // 左目の描画用コンポーネントを追加　横に光るフレアの表現用
    leftEyeFlareMeshComponent = this->AddComponent<SkeletalMeshComponent>("leftEyeFlare", "leftEye");
    leftEyeFlareMeshComponent->SetModel("./Data/Models/Primitives/plane.glb");
    leftEyeFlareMeshComponent->overrideDeferredPipelineName = "EnemyEyeModelPS";
    leftEyeFlareMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    leftEyeFlareMeshComponent->SetRelativeScaleDirect({ 0.1f,0.1f,0.1f });
    leftEyeFlareMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
    leftEyeFlareMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;
    leftEyeFlareMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::EnemyEye;

    // 右目の描画用コンポーネントを追加　暗闇で光る目の表現用
    rightEyeMeshComponent = this->AddComponent<SkeletalMeshComponent>("rightEye", parentName);
    rightEyeMeshComponent->SetModel("./Data/Models/Primitives/Sphere.glb");
    rightEyeMeshComponent->overrideDeferredPipelineName = "EnemyEyeModelPS";
    rightEyeMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    rightEyeMeshComponent->SetRelativeScaleDirect({ 0.03f,0.02f,0.02f });
    rightEyeMeshComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-0.1f });
    rightEyeMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
    rightEyeMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;
    rightEyeMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::EnemyEye;

    // 右目の描画用コンポーネントを追加　横に光るフレアの表現用
    rightEyeFlareMeshComponent = this->AddComponent<SkeletalMeshComponent>("rightEyeFlare", "rightEye");
    rightEyeFlareMeshComponent->SetModel("./Data/Models/Primitives/plane.glb");
    rightEyeFlareMeshComponent->overrideDeferredPipelineName = "EnemyEyeModelPS";
    rightEyeFlareMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    rightEyeFlareMeshComponent->SetRelativeScaleDirect({ 0.1f,0.1f,0.1f });
    rightEyeFlareMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
    rightEyeFlareMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;
    rightEyeFlareMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::EnemyEye;
}

void GruxEnemyEyeActor::Update(float elapsedTime)
{
    if (auto enemy = GetOwnerScene()->GetActorManager()->GetActorOfType<GruxEnemy>())
    {
        DirectX::XMFLOAT3 leftEyePosition = enemy->leftEyeSceneComponent->GetComponentLocation();
        DirectX::XMFLOAT3 rightEyePosition = enemy->rightEyeSceneComponent->GetComponentLocation();

        leftEyeMeshComponent->SetWorldLocationDirect(leftEyePosition);
        leftEyeFlareMeshComponent->SetWorldLocationDirect(leftEyePosition);
        rightEyeMeshComponent->SetWorldLocationDirect(rightEyePosition);
        rightEyeFlareMeshComponent->SetWorldLocationDirect(rightEyePosition);
    }

    // 目の色を設定する
    leftEyeMeshComponent->plusAlphaCBuffer->data.cpuColor = eyeColor;
    leftEyeFlareMeshComponent->plusAlphaCBuffer->data.cpuColor = eyeColor;
    rightEyeMeshComponent->plusAlphaCBuffer->data.cpuColor = eyeColor;
    rightEyeFlareMeshComponent->plusAlphaCBuffer->data.cpuColor = eyeColor;
    // 目の光量
    leftEyeMeshComponent->plusAlphaCBuffer->data.emissionPower = emissionFactor;
    leftEyeFlareMeshComponent->plusAlphaCBuffer->data.emissionPower = emissionFactor;
    rightEyeMeshComponent->plusAlphaCBuffer->data.emissionPower = emissionFactor;
    rightEyeFlareMeshComponent->plusAlphaCBuffer->data.emissionPower = emissionFactor;
    // 目の角度を設定する
    leftEyeFlareMeshComponent->SetWorldEulerRotationDirect(eyeFlareDegreeAngle);
    rightEyeFlareMeshComponent->SetWorldEulerRotationDirect(eyeFlareDegreeAngle);

    // 目のフレアのスケールを設定する
    // eyeFlareScale.x :0.0f -> 9.0fまで大きくする
    leftEyeFlareMeshComponent->SetRelativeScaleDirect(eyeFlareScale);
    rightEyeFlareMeshComponent->SetRelativeScaleDirect(eyeFlareScale);

}

void GruxEnemyEyeActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::ColorEdit3(U8("目の色"), &eyeColor.x);
    ImGui::DragFloat(U8("目の光の量"), &emissionFactor);
    ImGui::DragFloat3(U8("目のフレアのスケール"), &eyeFlareScale.x, 0.05f, 0.0f);
    ImGui::DragFloat3(U8("目のフレアの角度"), &eyeFlareDegreeAngle.x);
#endif
}

