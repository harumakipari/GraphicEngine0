#include "pch.h"
#include "DarkStageCandelabraActor.h"

#include "Components/Effect/ParticleComponent.h"
#include "Engine/Scene/SceneBase.h"

void DarkStageCandelabraActor::Initialize(const Transform& transform)
{

}
void DarkStageCandelabraActor::SetModel(const std::shared_ptr<StageAsset>& stageAsset)
{
    std::string parentName = "candelabraMesh";

    // 燭台のモデルを追加
    candelabraMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    candelabraMeshComponent->model = stageAsset->model;
    candelabraMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする


#if 0
    // サイズを取得
    DirectX::XMFLOAT3 size = stageAsset->model->GetModelSize();
    std::shared_ptr<BoxComponent> boxComponent = AddComponent<BoxComponent>("collision", parentName);
    boxComponent->SetBoxExtent(size);
    boxComponent->SetCollisionOffsetY(size.y * 0.5f);
    boxComponent->SetCollisionOffsetX(-size.x * 0.5f);
    boxComponent->SetCollisionOffsetZ(-size.z * 0.5f);
    boxComponent->SetStatic(true);
    boxComponent->SetLayer(CollisionLayer::Interactable);
    boxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
    boxComponent->Initialize();
#endif // 0


    auto scene = dynamic_cast<SceneBase*>(Scene::GetCurrentScene());

    auto lightsData = candelabraMeshComponent->model->GetPointLights();
    // ポイントライトコンポーネントを追加
    for (int i = 0; i < static_cast<int>(lightsData.size()); ++i)
    {
        const auto& light = lightsData[i];
#if 1
        std::string compName = "pointLightComponent_" + std::to_string(i);
        auto pointLightComponent =
            this->AddComponent<PointLightComponent>(compName, parentName);

        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(light.worldPosition);
        pointLightComponent->SetRelativeLocationDirect(pos);
        // ライトの名前からライトマネージャーの共有ライトを取得して設定
        pointLightComponent->SetSharedLightName(light.name);

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

    for (auto point : stageAsset->spawnPoints)
    {
#if 1
#if 0
        // エミッションを発生させるためにモデルを追加
        auto sphereMeshComponent = this->AddComponent<SkeletalMeshComponent>("sphereMeshComponent", parentName);
        sphereMeshComponent->SetModel("./Data/Models/Primitives/Sphere.glb");
        sphereMeshComponent->overrideDeferredPipelineName = "pointLightSkeletalMesh";
        sphereMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
        sphereMeshComponent->SetRelativeScaleDirect({ 0.01f,0.02f,0.01f });
        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
        pos.y += 0.1f;
        sphereMeshComponent->SetRelativeLocationDirect(pos);
        sphereMeshComponent->SetRelativeRotationDirect(point.worldRotation);
        sphereMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0.2f,0,1 };
        sphereMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.0f;

        flameComponents.push_back(sphereMeshComponent.get());
        flameBasePositions.push_back(pos);
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

        flameComponents.push_back(sphereMeshComponent.get());
        flameBasePositions.push_back(pos);
#endif // 0
#else
        // 炎のエフェクト
        auto frameEffect = this->AddComponent<ParticleComponent>("FireFrameEffect", parentName);
        frameEffect->Load("./Data/Effect/Files/DarkStageFrameEffect.json");
        DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
        pos.y += 0.1f;
        frameEffect->SetRelativeLocationDirect(pos);
        // ループ再生設定
        ParticleComponent::AddSettings settings
        {
            .loop = true, // ループ再生
            //.startDelay = delay, // 再生開始遅延時間
        };
        frameEffect->SetAddSettings(settings);
        frameEffect->Play();
#endif // 0
    }

}

void DarkStageCandelabraActor::Update(float deltaTime)
{
    static float time = 0.0f;
    time += deltaTime;

    for (int i = 0; i < flameComponents.size(); ++i)
    {
        auto* flame = flameComponents[i];

        float id = (float)i;

        //  揺らぎ（擬似ノイズ）
        float flicker =
            1.0f +
            flameSettings.flickerAmp1 * sin(time * flameSettings.flickerSpeed1 + id) +
            flameSettings.flickerAmp2 * sin(time * flameSettings.flickerSpeed2 + id * 2.1f);
        // 明るさ
        flame->plusAlphaCBuffer->data.emissionPower = flameSettings.baseEmission * flicker;

        //  サイズ（縦に伸ばす）
        float scale = flameSettings.baseScale + flameSettings.scaleAmp * flicker;
        flame->SetRelativeScaleDirect({ scale, scale * flameSettings.heightMultiplier, scale });

        //  位置揺らし
        auto basePos = flameBasePositions[i];
        auto pos = basePos;
        pos.x += flameSettings.posAmpX * sin(time * 1.0f + id);
        pos.z += flameSettings.posAmpZ * cos(time * 1.0f + id);
        pos.y += flameSettings.posAmpY * sin(time * 1.0f + id);
        flame->SetRelativeLocationDirect(pos);

        float f = powf(flicker, 2.0f);

        float g = flameSettings.colorBaseG + flameSettings.colorAmpG * f;

        flame->plusAlphaCBuffer->data.cpuColor = {
            1.0f,
            g,
            0.0f,
            1.0f
        };
    }
}

void DarkStageCandelabraActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::Text("=== Flame Settings ===");

    ImGui::DragFloat("Base Emission", &flameSettings.baseEmission, 0.1f, 0.0f, 100.0f);

    ImGui::DragFloat("Flicker Speed1", &flameSettings.flickerSpeed1, 0.1f);
    ImGui::DragFloat("Flicker Speed2", &flameSettings.flickerSpeed2, 0.1f);
    ImGui::DragFloat("Flicker Amp1", &flameSettings.flickerAmp1, 0.01f);
    ImGui::DragFloat("Flicker Amp2", &flameSettings.flickerAmp2, 0.01f);

    ImGui::Separator();

    ImGui::DragFloat("Base Scale", &flameSettings.baseScale, 0.001f);
    ImGui::DragFloat("Scale Amp", &flameSettings.scaleAmp, 0.001f);
    ImGui::DragFloat("Height Mult", &flameSettings.heightMultiplier, 0.1f);

    ImGui::Separator();

    ImGui::DragFloat("Pos Amp X", &flameSettings.posAmpX, 0.001f);
    ImGui::DragFloat("Pos Amp Y", &flameSettings.posAmpY, 0.001f);
    ImGui::DragFloat("Pos Amp Z", &flameSettings.posAmpZ, 0.001f);

    ImGui::Separator();

    ImGui::DragFloat("Color Base G", &flameSettings.colorBaseG, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Color Amp G", &flameSettings.colorAmpG, 0.01f, 0.0f, 1.0f);
#endif
}
