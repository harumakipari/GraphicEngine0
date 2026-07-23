#pragma once

#include "Engine/Scene/Scene.h"

#include <d3d11.h>
#include <wrl.h>

#include "Graphics/PostProcess/FullScreenQuad.h"
#include "Graphics/Core/ConstantBuffer.h"
#include "Graphics/Sprite/Sprite.h"
#include "Graphics/Core/Shader.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Resource/ShaderToy.h"

#include "Graphics/Environment/SkyMap.h"
#include "Graphics/Shadow/CascadeShadowMap.h"
#include "Graphics/PostProcess/MultipleRenderTargets.h"

#include "Core/Actor.h"
#include "Core/ActorManager.h"
#include "Engine/Scene/SceneBase.h"

#include "Game/Actors/Camera/LoadingCamera.h"

class LoadingScene : public SceneBase
{
    std::string preload_scene;// 次のシーンの名前

    void SetUpActors() override;

public:
    size_t type = 1;
    bool Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override;

    void Update(float deltaTime) override;

    float time = 0;
    void Start() override;

    void Render(ID3D11DeviceContext* immediate_context, float deltaTime) override;

    bool Uninitialize(ID3D11Device* device) override;

    void DrawGuiPlusAlpha() override;

    //シーンの自動登録
    static inline Scene::Autoenrollment<LoadingScene> _autoenrollment;


private:
    std::shared_ptr<MainCamera> mainCameraActor = nullptr;

    // ImGuiで使用する
    std::shared_ptr<Actor> selectedActor_;  // 選択中のアクターを保持

    DirectX::XMFLOAT3 cameraTarget = { 0.0f,0.0f,0.0f };

    SceneRenderer sceneRender;
    float loadingTime = 1.5f;   // ロードにかかる時間


    Microsoft::WRL::ComPtr<ID3D11PixelShader> loadingPs;

    std::shared_ptr<UIImageComponent> imageUiComponent;
};
