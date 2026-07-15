#include "pch.h"
#include "LanternActor.h"

void LanternActor::Initialize(const Transform& transform)
{
    meshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    meshComponent->SetModel("./Data/Models/DarkStageAssets/Lantern/Lantern.gltf");
    //meshComponent->SetModel("./Data/Models/DarkStageAssets/Lantern/scene.gltf");

    // ポイントライトコンポーネントを追加
    auto pointLightComponent = this->AddComponent<PointLightComponent>("LanternLight", parentName);
    pointLightComponent->SetRelativeLocationDirect({ 0.0f, -0.8f, -0.0f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    pointLightComponent->SetSharedLightName("LanternLight");

    // タイトルの部屋のライト
    titleRoomLightComponent0 = this->AddComponent<PointLightComponent>("titleRoomLightComponent0", parentName);
    titleRoomLightComponent0->SetRelativeLocationDirect({ -1.8f, 0.1f, 3.7f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    titleRoomLightComponent0->SetSharedLightName("TitleRoomLight");

    // タイトルの部屋のライト
    titleRoomLightComponent1 = this->AddComponent<PointLightComponent>("titleRoomLightComponent1", parentName);
    titleRoomLightComponent1->SetRelativeLocationDirect({ -0.1f, -0.5f, 3.2f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    titleRoomLightComponent1->SetSharedLightName("TitleRoomLight");
}

void LanternActor::Update(float deltaTime)
{
    if (titleRoomLightComponent0)
    {
        DebugRender::DrawSphere(titleRoomLightComponent0->GetComponentLocation(), 0.1f, { 1,0,1,1 }, 0);
    }
    if (titleRoomLightComponent1)
    {
        DebugRender::DrawSphere(titleRoomLightComponent1->GetComponentLocation(), 0.1f, { 1,0,1,1 }, 0);
    }

    swingTime += deltaTime;
    float angle = sinf(swingTime * swingSpeed) * swingAngle;
    DirectX::XMFLOAT3 rot = { 0.0f, 0.0f ,angle }; // Z軸回転で揺らす
    meshComponent->SetRelativeEulerRotationDirect(rot);
}

void LanternActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::DragFloat(U8("揺らすスピード"), &swingSpeed, 0.1f, 0.0f, 2.0f);
    ImGui::DragFloat(U8("揺らす角度"), &swingAngle, 0.1f, 0.0f, 360.0f);
#endif
}
