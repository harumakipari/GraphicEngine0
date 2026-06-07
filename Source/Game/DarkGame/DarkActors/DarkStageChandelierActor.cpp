#include "pch.h"
#include "DarkStageChandelierActor.h"

#include "Components/Effect/ParticleComponent.h"
#include "Engine/Scene/SceneBase.h"

void DarkStageChandelierActor::Initialize(const Transform& transform)
{
    std::string parentName = "chandelierMesh";

    // シャンデリアのモデルを追加
    chandelierMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    chandelierMeshComponent->SetModel("./Data/Models/DarkStageAssets/Chandelier/Chandelier.gltf");
    chandelierMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする

    auto scene = dynamic_cast<SceneBase*>(Scene::GetCurrentScene());

    auto lightsData = chandelierMeshComponent->model->GetPointLights();
    // ポイントライトコンポーネントを追加
    for (int i = 0; i < static_cast<int>(lightsData.size()); ++i)
    {
        const auto& light = lightsData[i];
#if 1
        std::string compName = "pointLightComponent_" + std::to_string(i);
        auto pointLightComponent =
            this->AddComponent<PointLightComponent>(compName, parentName);

        // ライトの名前からライトマネージャーの共有ライトを取得して設定
        pointLightComponent->SetSharedLightName(light.name);

        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(light.worldPosition);
        pointLightComponent->SetRelativeLocationDirect(pos);
        //pointLightComponent->SetColor(light.color);
        //pointLightComponent->SetRange(light.range);
        //pointLightComponent->SetIntensity(light.intensity);
#else
        DirectX::XMFLOAT3 pos = convertRHtoLh(light.worldPosition);

        Transform pointLightTr{
            pos,
            light.worldRotation,
            light.worldScale
        };
        auto pointLightActor = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStagePointLightActor>("pointLight", pointLightTr);
        pointLightActor->SetPointLightData(pos, light.color, light.intensity, light.range);
#endif // 0
    }
#if 1
    for (auto point : chandelierMeshComponent->model->spawnPoints)
    {
        // エミッションを発生させるためにモデルを追加
        auto sphereMeshComponent = this->AddComponent<InstanceMeshComponent>("sphereMeshComponent", parentName);
        sphereMeshComponent->SetModel("./Data/Models/Primitives/frame.glb");
        sphereMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
        sphereMeshComponent->SetRelativeScaleDirect({ 0.01f,0.02f,0.01f });
        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
        pos.y = -1.5f; // ろうそくの位置に合わせて微調整
        sphereMeshComponent->SetRelativeLocationDirect(pos);
        sphereMeshComponent->SetRelativeRotationDirect(point.worldRotation);
        sphereMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
        sphereMeshComponent->plusAlphaCBuffer->data.emissionPower = 30.0f;
    }
#else
    for (auto point : chandelierMeshComponent->model->spawnPoints)
    {
        // エミッションを発生させるためにモデルを追加
        auto sphereMeshComponent = this->AddComponent<SkeletalMeshComponent>("sphereMeshComponent", parentName);
        sphereMeshComponent->SetModel("./Data/Models/Primitives/Sphere.glb");
        sphereMeshComponent->overrideDeferredPipelineName = "pointLightSkeletalMesh";
        sphereMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
        sphereMeshComponent->SetRelativeScaleDirect({ 0.01f,0.02f,0.01f });
        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
        pos.y = -1.5f; // ろうそくの位置に合わせて微調整
        sphereMeshComponent->SetRelativeLocationDirect(pos);
        sphereMeshComponent->SetRelativeRotationDirect(point.worldRotation);
        sphereMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
        sphereMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;
    }
#endif // 0

}

void DarkStageChandelierActor::Update(float deltaTime)
{
    swingTime += deltaTime;

    float angle = sinf(swingTime * swingSpeed) * swingAngle;
    DirectX::XMFLOAT3 rot = { angle, 0.0f, 0.0f }; // X軸回転で揺らす
    chandelierMeshComponent->SetRelativeEulerRotationDirect(rot);
}



void DarkStageFireBowlActor::Initialize(const Transform& transform)
{
    std::string parentName = "fireBowlMesh";

    // モデルを追加
    meshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    meshComponent->SetModel("./Data/Models/DarkStageAssets/FireBowl/FireBowl.gltf");
    meshComponent->SetIsCastShadow(false);    // 影を落とさないようにする

    auto scene = dynamic_cast<SceneBase*>(Scene::GetCurrentScene());


    auto lightsData = meshComponent->model->GetPointLights();
    // ポイントライトコンポーネントを追加
    for (int i = 0; i < static_cast<int>(lightsData.size()); ++i)
    {
        const auto& light = lightsData[i];

#if 1
        std::string compName = "pointLightComponent_" + std::to_string(i);
        auto pointLightComponent =
            this->AddComponent<PointLightComponent>(compName, parentName);

        // ライトの名前からライトマネージャーの共有ライトを取得して設定
        pointLightComponent->SetSharedLightName(light.name);
        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(light.worldPosition);
        pointLightComponent->SetRelativeLocationDirect(pos);
#else
        DirectX::XMFLOAT3 pos = convertRHtoLh(light.worldPosition);

        Transform pointLightTr{
            pos,
            light.worldRotation,
            light.worldScale
        };
        auto pointLightActor = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStagePointLightActor>("pointLight", pointLightTr);
        pointLightActor->SetPointLightData(pos, light.color, light.intensity, light.range);
#endif // 0


        // 炎のエフェクト
        auto frameEffect = this->AddComponent<ParticleComponent>("FireFrameEffect", parentName);
        frameEffect->Load("./Data/Effect/Files/DarkStageFrameEffect.json");
        frameEffect->SetRelativeLocationDirect({ 0.0f,1.5f,0.0f });
        // ループ再生設定
        //float delay = 0.1f * i; // 0.2秒ずつ遅らせる
        ParticleComponent::AddSettings settings
        {
            .loop = true, // ループ再生
            //.startDelay = delay, // 再生開始遅延時間
        };
        frameEffect->SetAddSettings(settings);
        frameEffect->Play();

    }

}

void DarkStageTorchSconceActor::Initialize(const Transform& transform)
{
    std::string parentName = "TorchSconce";

    // モデルを追加
    meshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    meshComponent->SetModel("./Data/Models/DarkStageAssets/TorchSconce/TorchSconce.gltf");
    meshComponent->SetIsCastShadow(false);    // 影を落とさないようにする

    auto scene = dynamic_cast<SceneBase*>(Scene::GetCurrentScene());


    auto lightsData = meshComponent->model->GetPointLights();
    // ポイントライトコンポーネントを追加
    for (int i = 0; i < static_cast<int>(lightsData.size()); ++i)
    {
        const auto& light = lightsData[i];

#if 1
        std::string compName = "pointLightComponent_" + std::to_string(i);
        auto pointLightComponent =
            this->AddComponent<PointLightComponent>(compName, parentName);

        // ライトの名前からライトマネージャーの共有ライトを取得して設定
        pointLightComponent->SetSharedLightName(light.name);
        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(light.worldPosition);
        pointLightComponent->SetRelativeLocationDirect(pos);
#else
        DirectX::XMFLOAT3 pos = convertRHtoLh(light.worldPosition);

        Transform pointLightTr{
            pos,
            light.worldRotation,
            light.worldScale
        };
        auto pointLightActor = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStagePointLightActor>("pointLight", pointLightTr);
        pointLightActor->SetPointLightData(pos, light.color, light.intensity, light.range);
#endif // 0



    }
    // 炎のエフェクト
    auto frameEffect = this->AddComponent<ParticleComponent>("FireFrameEffect", parentName);
    frameEffect->Load("./Data/Effect/Files/DarkStageFrameEffect.json");
    int socketNode = meshComponent->model->FindNodeIndexByName("TorchLight2");
    frameEffect->AttachToComponent(meshComponent, socketNode); // "TorchLight2"
    frameEffect->SetRelativeLocationDirect({ 0.25f,0.0f,0.0f });
    frameEffect->Play();

    // 煙のエフェクト
    auto smokeEffect = this->AddComponent<ParticleComponent>("SmokeEffect", parentName);
    smokeEffect->Load("./Data/Effect/Files/DarkStageAfterFrameEffect.json");
    smokeEffect->AttachToComponent(meshComponent, socketNode); // "TorchLight2"
    smokeEffect->SetRelativeLocationDirect({ 0.25f,0.0f,0.0f });
    smokeEffect->Play();

}



void DarkStagePaintingActor::Initialize(const Transform& transform)
{
    std::string parentName = "PaintingMesh";

    // モデルを追加
    meshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    meshComponent->SetModel("./Data/Models/DarkStageAssets/Painting/scene.gltf");
    meshComponent->SetIsCastShadow(false);    // 影を落とさないようにする

}
