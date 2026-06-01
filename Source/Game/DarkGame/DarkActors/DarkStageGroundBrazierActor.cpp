#include "pch.h"
#include "DarkStageGroundBrazierActor.h"

#include "Components/Effect/ParticleComponent.h"
#include "Components/Render/PointLightComponent.h"
#include "Engine/Scene/SceneBase.h"

void DarkStageGroundBrazierActor::Initialize(const Transform& transform)
{
    std::string parentName = "candelabraMesh";

    // 火鉢のモデルを追加
    brazierMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    brazierMeshComponent->SetModel("./Data/Models/DarkStageAssets/GroundBrazier/groundBrazier.gltf", false, true);
    brazierMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする


    // 炎のエフェクト
    auto frameEffect = this->AddComponent<ParticleComponent>("FireFrameEffect", parentName);
    frameEffect->Load("./Data/Effect/Files/DarkStageFrameEffect.json");
    int socketNode = brazierMeshComponent->model->FindNodeIndexByName("P_Fire");
    frameEffect->AttachToComponent(brazierMeshComponent, socketNode); // "P_Fire"
    frameEffect->Play();

#if 0
    // 炎の後の煙エフェクト
    auto afterFireEffect = this->AddComponent<ParticleComponent>("AfterFireFrameEffect", parentName);
    afterFireEffect->Load("./Data/Effect/Files/DarkStageAfterFrameEffect.json");
    afterFireEffect->SetRelativeLocationDirect({ 0.0f,1.0f,0.0f });
    // ループ再生設定
    float delay = 0.3f;
    ParticleComponent::AddSettings settings0
    {
        .loop = true, // ループ再生
        .startDelay = delay, // 再生開始遅延時間
    };
    afterFireEffect->SetAddSettings(settings0);
    afterFireEffect->Play();

#endif // 0


    auto scene = dynamic_cast<SceneBase*>(Scene::GetCurrentScene());


    auto lightsData = brazierMeshComponent->model->GetPointLights();
    // ポイントライトコンポーネントを追加
    for (int i = 0; i < static_cast<int>(lightsData.size()); ++i)
    {
        const auto& light = lightsData[i];

        std::string compName = "pointLightComponent_" + light.name + std::to_string(i);
        auto pointLightComponent =
            this->AddComponent<PointLightComponent>(compName, parentName);

        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(light.worldPosition);
        pointLightComponent->SetRelativeLocationDirect(pos);
        // ライトの名前からライトマネージャーの共有ライトを取得して設定
        pointLightComponent->SetSharedLightName(light.name);

    }

    for (auto point : brazierMeshComponent->model->spawnPoints)
    {
#if 0
        // エミッションを発生させるためにモデルを追加
        auto sphereMeshComponent = this->AddComponent<SkeletalMeshComponent>("sphereMeshComponent", parentName);
        sphereMeshComponent->SetModel("./Data/Models/Primitives/Sphere.glb");
        sphereMeshComponent->overrideDeferredPipelineName = "pointLightSkeletalMesh";
        sphereMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
        sphereMeshComponent->SetRelativeScaleDirect({ 0.01f,0.01f,0.01f });
        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
        pos.y += 0.1f;
        sphereMeshComponent->SetRelativeLocationDirect(pos);
        sphereMeshComponent->SetRelativeRotationDirect(point.worldRotation);
        sphereMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
        sphereMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;
#else
        // エミッションを発生させるためにモデルを追加
        auto sphereMeshComponent = this->AddComponent<InstanceMeshComponent>("sphereMeshComponent", parentName);
        sphereMeshComponent->SetModel("./Data/Models/Primitives/frame.glb");
        sphereMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
        sphereMeshComponent->SetRelativeScaleDirect({ 0.01f,0.02f,0.01f });
        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
        pos.y += 0.1f;
        sphereMeshComponent->SetRelativeLocationDirect(pos);
        sphereMeshComponent->SetRelativeRotationDirect(point.worldRotation);
        sphereMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
        sphereMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;
#endif // 0

    }
}


void DarkStageGroundBrazierActor::SetModel(const std::shared_ptr<StageAsset>& stageAsset)
{


}
