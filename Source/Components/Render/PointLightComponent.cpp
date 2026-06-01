#include "pch.h"
#include "PointLightComponent.h"

#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneBase.h"

void PointLightComponent::Initialize()
{
    enable = true;
}

void PointLightComponent::Tick(float deltaTime)
{
#if _DEBUG
#if 0
    DirectX::XMFLOAT3 pos = GetComponentLocation();
    DebugRender::DrawSphere(
        pos,
        0.1f,
        DirectX::XMFLOAT4(sharedParam->color.x, sharedParam->color.y, sharedParam->color.z, 1.0f),
        0.0f
    );

    DebugRender::DrawCylinder(
        pos,
        sharedParam->range, 0.5f,
        DirectX::XMFLOAT4(sharedParam->color.x, sharedParam->color.y, sharedParam->color.z, 1.0f),
        0.0f
    );
#endif // 0


#endif

}

void PointLightComponent::DrawImGuiInspector()
{
#ifdef USE_IMGUI

    SceneComponent::DrawImGuiInspector();
    auto& sharedLights =
        Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData.sharedLights;

    if (sharedLights.contains(sharedLightName))
    {
        auto& param = sharedLights[sharedLightName];

        ImGui::SliderFloat("range", &param.range, 0.0f, 10.0f);
        ImGui::ColorEdit3("Color", &param.color.x);
        ImGui::SliderFloat("Intensity", &param.color.w, 0.0f, 30.0f);
    }
#endif
}


LightManager::PointLight PointLightComponent::ToRenderLight() const
{
    LightManager::PointLight l{};

    l.position = {
        GetComponentLocation().x,
        GetComponentLocation().y,
        GetComponentLocation().z,
        1.0f
    };

    auto& sharedLights =
        Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData.sharedLights;

    if (sharedLights.contains(sharedLightName))
    {
        const auto& param = sharedLights.at(sharedLightName);
        l.color = param.color;
        l.range = param.range;
    }

    return l;
}