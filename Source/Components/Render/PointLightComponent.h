#pragma once
#include "Components/Base/SceneComponent.h"
#include "Engine/Debug/DebugRender.h"
#include "Graphics/Core/LightManager.h"

class PointLightComponent :public SceneComponent
{
public:
    PointLightComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

    virtual ~PointLightComponent() = default;

    void Initialize() override;

    void Tick(float deltaTime)override;

    void SetSharedLightName(const std::string& name)
    {
        sharedLightName = name;
    }

    void DrawImGuiInspector() override;

    LightManager::PointLight ToRenderLight() const;


    bool  IsUsePointLight()const { return enable; }

private:
    bool enable = true;

    std::string sharedLightName = "PlayerPointLight"; // 一旦デフォルトで、playerのポイントライトを入れておく
};
