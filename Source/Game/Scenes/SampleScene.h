#pragma once

#include "Engine/Scene/Scene.h"

#include <d3d11.h>
#include <wrl.h>
#include <memory>

#include "Core/ActorManager.h"
#include "Engine/Scene/SceneBase.h"


#include "Graphics/Renderer/SceneRenderer.h"

#include "Game/Actors/Camera/LoadingCamera.h"
#include "Game/Actors/Stage/ClothSimulate.h"
#include "Game/Actors/Stage/Stage.h"
#include "Game/Actors/WaterSphere/MorphModel.h"

#include "UI/Widgets/Widget.h"


class SampleScene : public SceneBase
{
public:
    bool Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override;

    void Start() override;

    void Update(float deltaTime) override;

    bool Uninitialize(ID3D11Device* device) override;

    void DrawGui() override;

    void SetUpActors()override;

    //ÉVÅ[ÉìÇÃé©ìÆìoò^
    static inline Scene::Autoenrollment<SampleScene> _autoenrollment;

private:
    std::shared_ptr<Stage>  title;

    std::unique_ptr<MorphModel> morphModel;

};
