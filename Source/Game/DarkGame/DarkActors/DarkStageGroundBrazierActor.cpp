#include "pch.h"
#include "DarkStageGroundBrazierActor.h"

#include "Components/Effect/ParticleComponent.h"
#include "Components/Render/PointLightComponent.h"
#include "Engine/Scene/SceneBase.h"


void DarkStageGroundBrazierActor::SetModel(const std::shared_ptr<StageAsset>& stageAsset)
{
    std::string parentName = "candelabraMesh";

    brazierMeshComponent = this->AddComponent<InstanceMeshComponent>(parentName);
    brazierMeshComponent->model = stageAsset->model;
    brazierMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    brazierMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Furniture;
    PipeLineStateDesc pipeLineState;
    HRESULT hr = CreatePsFromCSO(Graphics::GetDevice(), "./Data/Shaders/GltfModelInstancedBatchingPS.cso", pipeLineState.pixelShader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    brazierMeshComponent->SetPipeLineState(pipeLineState);

    // 当たり判定を追加
    std::shared_ptr<BoxComponent> boxComponent = this->AddComponent<BoxComponent>("BoxComponent", parentName);
    DirectX::XMFLOAT3 size = brazierMeshComponent->GetModelSize();
    boxComponent->SetBoxExtent(size);
    boxComponent->SetLayer(CollisionLayer::WorldPropsNoRaycast);
    boxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Trigger);
    boxComponent->SetCollisionOffsetY(size.y * 0.5f);
    boxComponent->Initialize();


    // 炎のエフェクト
    auto frameEffect = this->AddComponent<ParticleComponent>("FireFrameEffect", parentName);
    frameEffect->Load("./Data/Effect/Files/DarkStageFrameEffect.json");
    frameEffect->SetRelativeLocationDirect({ 0.0f,0.8f,0.0f });
    frameEffect->Play();

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
        // エミッションを発生させるためにモデルを追加
        auto sphereMeshComponent = this->AddComponent<InstanceMeshComponent>("sphereMeshComponent", parentName);
        sphereMeshComponent->SetModel("./Data/Models/Primitives/frame.glb");
        sphereMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
        sphereMeshComponent->SetRelativeScaleDirect({ 0.01f,0.02f,0.01f });

        PipeLineStateDesc pipeLineState;
        HRESULT hr = CreatePsFromCSO(Graphics::GetDevice(), "./Data/Shaders/InstancePointLightModelPS.cso", pipeLineState.pixelShader.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        sphereMeshComponent->SetPipeLineState(pipeLineState);

        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
        pos.y += 0.1f;
        sphereMeshComponent->SetRelativeLocationDirect(pos);
        sphereMeshComponent->SetRelativeRotationDirect(point.worldRotation);
        sphereMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
        sphereMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;

    }
}
