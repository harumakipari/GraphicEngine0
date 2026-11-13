#include "pch.h"
#include "PointLightComponent.h"

#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneBase.h"

void PointLightComponent::Initialize()
{
    enable = true;
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
        l.attenuationType = param.attenuationType;
    }

    return l;
}