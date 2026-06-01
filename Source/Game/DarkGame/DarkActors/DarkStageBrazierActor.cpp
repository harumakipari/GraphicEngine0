#include "pch.h"
#include "DarkStageBrazierActor.h"

#include "Components/Effect/ParticleComponent.h"
#include "Components/Render/PointLightComponent.h"
#include "Engine/Scene/SceneBase.h"

void DarkStageBrazierActor::SetModel(const std::shared_ptr<StageAsset>& stageAsset)
{
    std::string parentName = "candelabraMesh";

    // 火鉢のモデルを追加
    brazierMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    brazierMeshComponent->SetModel("./Data/Models/DarkStageAssets/Brazier/Brazier.gltf");
    brazierMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする

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

#if 1
    for (auto point : brazierMeshComponent->model->spawnPoints)
    {
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
    }
#else

    for (auto point : brazierMeshComponent->model->spawnPoints)
    {
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
    }

#endif // 0

}


void DarkStageMeltedWaxActor::SetModel(const std::shared_ptr<StageAsset>& stageAsset)
{
    std::string parentName = "metedWaxMesh";

    // 溶けた蝋のモデルを追加
    metedWaxMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    metedWaxMeshComponent->SetModel("./Data/Models/DarkStageAssets/MeltedWax/MeltedWax.gltf");
    metedWaxMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする

    auto scene = dynamic_cast<SceneBase*>(Scene::GetCurrentScene());

    auto lightsData = metedWaxMeshComponent->model->GetPointLights();
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

#if 0
    for (auto point : metedWaxMeshComponent->model->spawnPoints)
    {
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
    }
#else
    for (auto point : metedWaxMeshComponent->model->spawnPoints)
    {
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
    }
#endif // 0

}

// スタンド式火鉢のモデル
void DarkStageStandingBrazierActor::SetModel(const std::shared_ptr<StageAsset>& stageAsset)
{
    std::string parentName = "standingBrazierMesh";

    // スタンド式火鉢のモデルを追加
    standingBrazierMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    standingBrazierMeshComponent->SetModel("./Data/Models/DarkStageAssets/StandingBrazier/StandingBrazier.gltf");
    standingBrazierMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする

    auto scene = dynamic_cast<SceneBase*>(Scene::GetCurrentScene());


    auto lightsData = standingBrazierMeshComponent->model->GetPointLights();
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

#if 0
    for (auto point : standingBrazierMeshComponent->model->spawnPoints)
    {
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
    }
#else
    for (auto point : standingBrazierMeshComponent->model->spawnPoints)
    {
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
    }
#endif // 0

}