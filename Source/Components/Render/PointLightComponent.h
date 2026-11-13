#pragma once
#include "Components/Base/SceneComponent.h"
#include "Graphics/Core/LightManager.h"

class PointLightComponent :public SceneComponent
{
public:
    PointLightComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

    virtual ~PointLightComponent() = default;

    void Initialize() override;

    void SetSharedLightName(const std::string& name)
    {
        sharedLightName = name;
    }

    LightManager::PointLight ToRenderLight() const;

    bool  IsUsePointLight()const { return enable; }
    void SetUsePointLight(const bool enabled) { enable = enabled; }

private:
    bool enable = true;
    std::string sharedLightName = "PlayerPointLight"; // デフォルトで、playerのポイントライトを入れておく
};
