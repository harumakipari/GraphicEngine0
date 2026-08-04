#include "pch.h"
#include "GruxEnemyEyeActor.h"

#include "GruxEnemy.h"
#include "Engine/Scene/Scene.h"

void GruxEnemyEyeActor::Initialize(const Transform& transform)
{
    std::string parentName = GetRootComponentName();
    // 左目の描画用コンポーネントを追加　暗闇で光る目の表現用
    leftEyeMeshComponent = this->AddComponent<SkeletalMeshComponent>("leftEye", parentName);
    //leftEyeMeshComponent->SetModel("./Data/Models/Characters/EnemyEye/EnemyEyeModel.glb");
    leftEyeMeshComponent->SetModel("./Data/Models/Primitives/sphere.glb");
    leftEyeMeshComponent->overrideDeferredPipelineName = "EnemyEyeModelPS";
    leftEyeMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    leftEyeMeshComponent->SetRelativeScaleDirect(eyeInitScale);
    leftEyeMeshComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-0.1f });
    leftEyeMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
    leftEyeMeshComponent->plusAlphaCBuffer->data.emissionPower = 0.0f;
    leftEyeMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::EnemyEye;
    leftEyeMeshComponent->SetIsVisible(false);
    // 左目の描画用コンポーネントを追加　横に光るフレアの表現用
    leftEyeFlareMeshComponent = this->AddComponent<SkeletalMeshComponent>("leftEyeFlare", "leftEye");
#if 0
    leftEyeFlareMeshComponent->SetModel("./Data/Models/ParticleMesh/StarPlane.gltf");
    leftEyeFlareMeshComponent->overrideDeferredPipelineName = "StarEyeOpaquePS";
    leftEyeFlareMeshComponent->overrideForwardPipelineName = "StarEyeOpaquePS";
#else
    leftEyeFlareMeshComponent->SetModel("./Data/Models/Primitives/plane.glb");
    leftEyeFlareMeshComponent->overrideDeferredPipelineName = "EnemyEyeModelPS";
#endif // 0
    leftEyeFlareMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    leftEyeFlareMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
    leftEyeFlareMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;
    leftEyeFlareMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::EnemyEye;

    // 右目の描画用コンポーネントを追加　暗闇で光る目の表現用
    rightEyeMeshComponent = this->AddComponent<SkeletalMeshComponent>("rightEye", parentName);
    rightEyeMeshComponent->SetModel("./Data/Models/Primitives/sphere.glb");
    rightEyeMeshComponent->overrideDeferredPipelineName = "EnemyEyeModelPS";
    rightEyeMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    rightEyeMeshComponent->SetRelativeScaleDirect(eyeInitScale);
    rightEyeMeshComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-0.1f });
    rightEyeMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
    rightEyeMeshComponent->plusAlphaCBuffer->data.emissionPower = 0.0f;
    rightEyeMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::EnemyEye;
    rightEyeMeshComponent->SetIsVisible(false);
    // 右目の描画用コンポーネントを追加　横に光るフレアの表現用
    rightEyeFlareMeshComponent = this->AddComponent<SkeletalMeshComponent>("rightEyeFlare", "rightEye");
#if 0
    rightEyeFlareMeshComponent->SetModel("./Data/Models/ParticleMesh/StarPlane.gltf");
    rightEyeFlareMeshComponent->overrideDeferredPipelineName = "StarEyeOpaquePS";
    rightEyeFlareMeshComponent->overrideForwardPipelineName = "StarEyeOpaquePS";
#else
    rightEyeFlareMeshComponent->SetModel("./Data/Models/Primitives/plane.glb");
    rightEyeFlareMeshComponent->overrideDeferredPipelineName = "EnemyEyeModelPS";

#endif // 0
    rightEyeFlareMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    rightEyeFlareMeshComponent->SetRelativeScaleDirect({ 0.1f,0.1f,0.1f });
    rightEyeFlareMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
    rightEyeFlareMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;
    rightEyeFlareMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::EnemyEye;

    eyeFlareEasingComponent = std::make_unique<EasingRunner>();
    eyeEmissiveEasingComponent = std::make_unique<EasingRunner>();
    eyeEasingComponent = std::make_unique<EasingRunner>();
}

void GruxEnemyEyeActor::Update(float elapsedTime)
{
    eyeFlareEasingComponent->Tick(elapsedTime);
    eyeEmissiveEasingComponent->Tick(elapsedTime);
    eyeEasingComponent->Tick(elapsedTime);

    leftEyeMeshComponent->SetIsVisible(false);
    rightEyeMeshComponent->SetIsVisible(false);

    if (auto enemy = GetOwnerScene()->GetActorManager()->GetActorOfType<GruxEnemy>())
    {
        leftEyeMeshComponent->SetWorldLocationDirect(leftEyePosition);
        rightEyeMeshComponent->SetWorldLocationDirect(rightEyePosition);

        //leftEyePosition = MathHelper::Add(leftEyePosition, eyeFlareOffset);
        //rightEyePosition = MathHelper::Add(rightEyePosition, eyeFlareOffset);
        //leftEyeFlareMeshComponent->SetWorldLocationDirect(leftEyePosition);
        //rightEyeFlareMeshComponent->SetWorldLocationDirect(rightEyePosition);
    }

    // 目の色を設定する
    leftEyeMeshComponent->plusAlphaCBuffer->data.cpuColor = eyeColor;
    leftEyeFlareMeshComponent->plusAlphaCBuffer->data.cpuColor = eyeColor;
    rightEyeMeshComponent->plusAlphaCBuffer->data.cpuColor = eyeColor;
    rightEyeFlareMeshComponent->plusAlphaCBuffer->data.cpuColor = eyeColor;

    // 目の光量
    //emissionEyeFactor = std::lerp(140.0f, 208.0f, eyeEmissiveEasingFactor);

    leftEyeMeshComponent->plusAlphaCBuffer->data.emissionPower = emissionEyeFactor;
    rightEyeMeshComponent->plusAlphaCBuffer->data.emissionPower = emissionEyeFactor;
    leftEyeFlareMeshComponent->plusAlphaCBuffer->data.emissionPower = emissionEyeFlareFactor;
    rightEyeFlareMeshComponent->plusAlphaCBuffer->data.emissionPower = emissionEyeFlareFactor;

    // 目の角度を設定する
    leftEyeMeshComponent->SetWorldEulerRotationDirect(eyeDegreeAngle);
    rightEyeMeshComponent->SetWorldEulerRotationDirect(eyeDegreeAngle);

    // 目のスケールを設定する
    eyeScale = MathHelper::Lerp({ 0.0f,0.0f,0.0f }, eyeInitScale, eyeScaleEasingFactor);
    leftEyeMeshComponent->SetRelativeScaleDirect(eyeScale);
    rightEyeMeshComponent->SetRelativeScaleDirect(eyeScale);

    leftEyeMeshComponent->SetIsVisible(false);
    rightEyeMeshComponent->SetIsVisible(false);
    // 目のフレアのスケールを設定する
    // eyeFlareScale.x :0.0f -> 1.2fまで大きくする
    float maxScale = 1.2f;
    //eyeFlareScale.y *= 0.02f;
    //eyeFlareScale.z *= 0.02f;
    //eyeFlareScale.z = std::lerp(0.0f, maxScale, eyeFlareScaleEasingFactor);
    //leftEyeFlareMeshComponent->SetRelativeScaleDirect(eyeFlareScale);
    //rightEyeFlareMeshComponent->SetRelativeScaleDirect(eyeFlareScale);
}

void GruxEnemyEyeActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::ColorEdit3(U8("目の色"), &eyeColor.x);
    ImGui::DragFloat(U8("目の光の量"), &emissionEyeFactor);
    ImGui::DragFloat(U8("フレアのスケールが大きくなる時間"), &eyeFlareAddScaleTime, 0.1f, 0.0f, 2.0f);
    ImGui::DragFloat(U8("フレアのスケールを小さくするまでの維持時間"), &eyeFlareWaitScaleTime, 0.1f, 0.0f, 2.0f);
    ImGui::DragFloat(U8("フレアのスケールが小さくなる時間"), &eyeFlareSubtractScaleTime, 0.1f, 0.0f, 2.0f);

    ImGui::DragFloat3(U8("目のフレアのスケール"), &eyeFlareScale.x, 0.05f, 0.0f);
    ImGui::DragFloat3(U8("目のフレアの角度"), &eyeFlareDegreeAngle.x);
    ImGui::DragFloat3(U8("目のフレアのoffset"), &eyeFlareOffset.x);
    ImGui::DragFloat3(U8("右目の位置"), &rightEyePosition.x);
    ImGui::DragFloat3(U8("左目の位置"), &leftEyePosition.x);
    if (ImGui::Button(U8("目のフレアのスケール変更開始")))
    {
        StartEyeFlareScale();
    }
    if (ImGui::Button(U8("目の光る処理開始")))
    {
        StartEyeFlash();
    }

#endif
}

void GruxEnemyEyeActor::StartEyeFlash(const std::function<void()>& onFinished)
{
    this->onFinished = onFinished;

    float eyeEmissiveAddTime = 0.5f;

    // 目玉が光る処理
    {
        TestEasingHandler handler;

        handler.AddEasing(
            TestEaseType::InSine,
            0.0f,
            1.0f,
            eyeEmissiveAddTime
        );


        handler.SetCompletedFunction([this]()
            {
                StartEyeFlareScale();
                eyeEmissiveEasingFactor = 1.0f;
            });
        PropertyAccessor<float> accessor;

        accessor.getter = [this]() { return eyeEmissiveEasingFactor; };
        accessor.setter = [this](float t)
            {
                eyeEmissiveEasingFactor = t;
            };

        eyeEmissiveEasingComponent->StartHandler(handler, accessor);
    }
}

// 目のモデルを小さくする処理
void GruxEnemyEyeActor::ToSmallEyeModel(float duration, std::function<void()> finished)
{
    onFinished = finished;
    duration *= 0.5f;
    // 目玉が光る処理
    {
        TestEasingHandler handler;

        handler.AddEasing(
            TestEaseType::OutBounce,
            1.0f,
            0.0f,
            duration
        );

        handler.SetCompletedFunction([this]()
            {
                eyeScaleEasingFactor = 0.0f;
                leftEyeMeshComponent->SetIsVisible(false);
                rightEyeMeshComponent->SetIsVisible(false);
                leftEyeFlareMeshComponent->SetIsVisible(false);
                rightEyeFlareMeshComponent->SetIsVisible(false);

                if (onFinished)
                {
                    onFinished();
                }
                onFinished = nullptr;
            });
        PropertyAccessor<float> accessor;

        accessor.getter = [this]() { return eyeScaleEasingFactor; };
        accessor.setter = [this](float t)
            {
                eyeScaleEasingFactor = t;
            };

        eyeEasingComponent->StartHandler(handler, accessor);
    }

}

// 目のフレアのスケールが大きくなる処理を開始
void GruxEnemyEyeActor::StartEyeFlareScale()
{
    // フレアのスケール の easing
    {
        TestEasingHandler handler;

        handler.AddEasing(
            TestEaseType::InSine,
            0.0f,
            1.0f,
            eyeFlareAddScaleTime
        );

        handler.AddWait(eyeFlareWaitScaleTime);

        handler.AddEasing(
            TestEaseType::OutExp,
            1.0f,
            0.0f,
            eyeFlareSubtractScaleTime
        );


        handler.SetCompletedFunction([this]()
            {
                if (this->onFinished)
                {
                    this->onFinished();
                }
                this->onFinished = nullptr;
                eyeFlareScaleEasingFactor = 0.0f;

            });
        PropertyAccessor<float> accessor;

        accessor.getter = [this]() { return eyeFlareScaleEasingFactor; };
        accessor.setter = [this](float t)
            {
                eyeFlareScaleEasingFactor = t;
            };

        eyeFlareEasingComponent->StartHandler(handler, accessor);
    }

}