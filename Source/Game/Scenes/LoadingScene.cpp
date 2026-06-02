#include "pch.h"
#include "LoadingScene.h"

#include "Engine/Framework/Framework.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "../External/imgui/imgui.h"
#endif

#include <magic_enum.hpp>

#include "Engine/Input/InputSystem.h"

#include "Graphics/Core/Shader.h"
#include "Graphics/Core/Graphics.h"
#include "Graphics/Resource/Texture.h"
#include "Graphics/Core/RenderState.h"
#include "Engine/Input/InputSystem.h"
#include "Core/ActorManager.h"
#include "Graphics/PostProcess/BloomEffect.h"


bool LoadingScene::Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
{
#if 0
    SceneBase::Initialize(device, width, height, props);

    auto mainCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<LoadingCamera>("mainLoadingCameraActor");
    auto mainCameraComponent = mainCameraActor->GetComponent<CameraComponent>();
    mainCameraActor->SetPosition({ -4.1f,1.9f,-4.3f });
    SetActiveCamera(mainCameraActor);
    Logger::Log(U8("ロードシーンのカメラ設定される。"));


    OutputDebugStringA((std::string("Scene::Initialize this=") + std::to_string(reinterpret_cast<uintptr_t>(this)) + "\n").c_str());
    OutputDebugStringA((std::string("_current_scene.get()=") + std::to_string(reinterpret_cast<uintptr_t>(this)) + "\n").c_str());
    OutputDebugStringA((std::string("actorManager_ ptr=") + std::to_string(reinterpret_cast<uintptr_t>(this->GetActorManager())) + "\n").c_str());
    //HRESULT hr;

    //D3D11_BUFFER_DESC bufferDesc{};
#else
    lightDirection = { 0.722f, -0.38f, -0.0211f, 0.9f };   // 上の窓からの光
    lightColor = { 1.0f, 0.8f, 1.0f, 2.6f };
    {
        sceneCBuffer = std::make_unique<ConstantBuffer<FrameConstants>>(device);
        shaderCBuffer = std::make_unique<ConstantBuffer<SceneShaderConstants>>(device);
        sceneCBuffer->data.elapsedTime = 0;//開始時に０にしておく

        // ライト
        {
            lightManager = std::make_unique<LightManager>();
            lightManager->Initialize(device);
            lightManager->SetDirectionalLight(this, lightDirection, lightColor);
        }

        {
            {
                Logger::Log(U8("シーンエフェクトを作成しました！"));
                sceneEffectManager = std::make_unique<SceneEffectManager>();
                sceneEffectManager->AddEffect(std::make_unique<BloomEffect>());
                sceneEffectManager->Initialize(device, static_cast<uint32_t>(width), height);
            }
        }

        HRESULT hr = { S_OK };

        //スカイマップ
        skyMap = std::make_unique<decltype(skyMap)::element_type>(device, L"./Data/Environment/Sky/sky/skybox.dds");
        fullscreenQuad = std::make_unique<FullScreenQuad>(device);

        frameBuffer = std::make_unique<FrameBuffer>(device, static_cast<uint32_t>(width), height, false);
        finalBuffer = std::make_unique<FrameBuffer>(device, static_cast<uint32_t>(width), height, false);
        imGuiGizmoBuffer = std::make_unique<FrameBuffer>(device, static_cast<uint32_t>(width), height, false);

        // GBUFFER
        gBufferRenderTarget = std::make_unique<decltype(gBufferRenderTarget)::element_type>(device, static_cast<uint32_t>(width), height);
        hr = CreatePsFromCSO(device, "./Data/Shaders/DeferredLightingPS.cso", deferredPs.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = CreatePsFromCSO(device, "./Data/Shaders/FinalPassPS.cso", finalPs.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        // カスケードシャドウマップ
        cascadedShadowMaps = std::make_unique<decltype(cascadedShadowMaps)::element_type>(device, 1024, 1024, 4);

        D3D11_TEXTURE2D_DESC texture2dDesc;
        // テクスチャをロード
        hr = LoadTextureFromFile(device, L"./Data/Environment/Sky/sky/lut_charlie.dds", environmentTextures[0].ReleaseAndGetAddressOf(), &texture2dDesc);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = LoadTextureFromFile(device, L"./Data/Environment/Sky/sky/diffuse_iem.dds", environmentTextures[1].ReleaseAndGetAddressOf(), &texture2dDesc);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = LoadTextureFromFile(device, L"./Data/Environment/Sky/sky/specular_pmrem.dds", environmentTextures[2].ReleaseAndGetAddressOf(), &texture2dDesc);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = LoadTextureFromFile(device, L"./Data/Environment/Sky/sky/lut_sheen_e.dds", environmentTextures[3].ReleaseAndGetAddressOf(), &texture2dDesc);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        // UIマネージャーを初期化
        uiManager = std::make_unique<UIManager>();

        // カメラマネージャー作成
        cameraManager = std::make_unique<CameraManager>();

        float screenWidth = static_cast<float>(Graphics::GetScreenWidth());
        float screenHeight = static_cast<float>(Graphics::GetScreenHeight());
        XMFLOAT2 imageSize = { screenWidth,screenHeight };
        XMFLOAT2 imageMin = { 0.0f,0.0f };

        InputSystem::SetViewportRect(
            imageMin.x,
            imageMin.y,
            imageSize.x,
            imageSize.y
        );
        Graphics::SetViewport(
            imageMin.x,
            imageMin.y,
            imageSize.x,
            imageSize.y
        );
        Logger::Log(U8("UI Render viewport ") + std::to_string(imageMin.x) + std::to_string(imageMin.y) + std::to_string(imageSize.x) + std::to_string(imageSize.y));
    }

#endif // 0

    preload_scene = props.at("preload");
    _async_preload_scene(device, width, height, preload_scene);


    loadingTime = 4.0f;   // ロードにかかる時間

    auto& param = SceneTransitionManager::Instance().GetParams();
    if (param.contains("fade"))
    {// ゲームオーバーだったら
        std::string name = param.at("fade");
        if (name == "0")
        {
            loadingSprite = std::make_shared<Sprite>(device, L"./Data/Textures/ScissorsUI/black.png");
            //gameOverSprite =std::make_shared<Sprite>(device, L"./Data/Textures/ScissorsUI/gameOver.png");
            loadingTime = 0.0f;
        }
    }
    return true;
}

void LoadingScene::Start()
{
    SetUpActors();

    RegisterRenderHook(RenderPass::UI, [&](ID3D11DeviceContext* immediateContext)
        {
            //if (const auto e = GetActorManager()->GetActorByName("LoadingEnemy"))
            //{
            //loadingSkewer->poleModel->RenderOpaque(immediateContext, loadingSkewer->GetWorldTransform());


            //enemy->skeltalMeshComponent->RenderOpaque(immediateContext, e->GetWorldTransform());
            //RenderState::BindBlendState(immediateContext, BLEND_STATE::ALPHA);
            //RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
            //backImage->Draw(immediateContext);
            chipsFrameImage->Draw(immediateContext);
            chipsImage->Draw(immediateContext);

            RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_ON_ZW_ON);
            RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_BACK);
            //loadingPlayerActor->skeletalMeshComponent->RenderOpaque(immediateContext, loadingPlayerActor->GetWorldTransform());
            if (loadingSprite)
            {
                loadingSprite->Render(immediateContext, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                //gameOverSprite->Render(immediateContext, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            }

            //}
        });

    float width = 1920.0f;
    float height = 1080.0f;

    auto bossSprite = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/ScissorsUI/scene_change_boss.png");
    //auto difficultSprite = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/ScissorsUI/scene_change_difficult.png");
    //auto firstSprite = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/ScissorsUI/scene_change_first.png");

    backWhiteImage = std::make_shared<UIImageComponent>("./Data/Textures/ScissorsUI/back.png", "back");
    backWhiteImage->SetSize({ 1920, 1080 });


    backImage = std::make_shared<UIImageComponent>("./Data/Textures/ScissorsUI/scene_change.png", "backGround");
    backImage->SetSize({ 1920, 1080 });
    auto& param = SceneTransitionManager::Instance().GetParams();
    if (param.contains("stage"))
    {
        std::string name = param.at("stage");
        if (name == "BOSS")
        {
            backImage->SetTexture(bossSprite);
        }
        else if (name=="FIRST")
        {
            
        }

    }

    std::shared_ptr<Sprite> chipSprite = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/ScissorsUI/Tips/scissors_hint.png");

    DirectX::XMFLOAT2 tipsSize = { 750.0f,168.0f };

    chipsImage = std::make_shared<UIImageComponent>("./Data/Textures/ScissorsUI/Tips/player_lore.png", "chipsImage");
    chipsImage->SetSize(tipsSize);
    chipsImage->SetWorldPosition(tipsPos);
    chipsImage->SetTexture(chipSprite);

    chipsFrameImage = std::make_shared<UIImageComponent>("./Data/Textures/ScissorsUI/Tips/tips_frame.png", "tips_frame");
    chipsFrameImage->SetSize({ 1000.0f,200.0f });
    chipsFrameImage->SetWorldPosition(tipsPos);


#if 0
    GetUIManager()->Add(sprite);
#else
    RegisterRenderHook(RenderPass::Sky, [&](ID3D11DeviceContext* immediateContext)
        {
            backWhiteImage->Draw(immediateContext);
            backImage->Draw(immediateContext);
        });


#endif // 0

    // シーンのライト設定などを設定する
    SceneSettings& settings = this->GetSceneSettings();
    auto& lightData = Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData;

    settings.cascadedShadowMapConstants =
    {
        17.021f,
        0.136f,
        21.643f,
        true,
    };
    settings.sceneShaderConstants =
    {
        0.75f,
        0.00011f,
        0.005f,
        0.0f,
        0.0f,
        0.04f,
        0.018f,
        0.16f,
        4.6f,
        0.0f,
        80.0f,
        1.0f,
        23.0f,
        0,
        1,
        0,
        1,
        1,
        1,
        1,
        1,
        0,
        0,
        0.0f,
        { 1.0f,1.0f,1.0f },
        0.0f,
    };
    lightData.sceneConstants =
    {
         { -0.99f, 0.15f, -0.95f, 0.85f/* w:attenuation Rate */},
         { 1.0f, 1.0f, 1.0f, 1.98f/*w colorPower*/ },
         0.278f,
         1,
         1,
         40,

         { 1.0f,1.0f,1.0f },
         1.466f,

         { 0.977f,0.71f,0.168f },
         0.0f,

         { 0.422f,0.333f,0.0f },
         0.0f,

         3.0f,
         1.0f,
         0.7f,
         1.8f,

         1.0f,
         0.3f,
         0.78f,
         0.15f,
    };
    this->SetSceneSettings(settings);
}

void LoadingScene::SetUpActors()
{
    Transform mainCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    //auto mainCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<FixedCamera>("fixedCameraActor", mainCameraTr);
    auto mainCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<TitleCamera>("fixedCameraActor", mainCameraTr);
    auto mainCameraComponent = mainCameraActor->GetComponent<TPSCameraComponent>();

    Transform cameraTargetTr(DirectX::XMFLOAT3{ -0.297f,3.197f,2.936f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    //Transform cameraTargetTr(DirectX::XMFLOAT3{ 2.2f,1.984f,2.753f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto cameraTargetActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<Actor>("cameraTargetActor", cameraTargetTr);

    mainCameraActor->SetTarget(cameraTargetActor->GetRootComponent());
    mainCameraComponent->SetPitch(DirectX::XMConvertToRadians(-11.0f));
    mainCameraComponent->SetYaw(DirectX::XMConvertToRadians(231.5f));
    mainCameraComponent->SetFov(DirectX::XMConvertToRadians(24.0f));
    mainCameraComponent->distance = 28.4f;

    SetActiveCamera(mainCameraActor);
    Logger::Log(U8("ロードシーンのカメラ設定される。"));
}

void LoadingScene::Update(float deltaTime)
{
    SceneBase::Update(deltaTime);

    DirectX::XMFLOAT2 tipsWordPos = { tipsPos.x + tipsWordOffset.x,tipsPos.y + tipsWordOffset.y };
    chipsImage->SetWorldPosition(tipsWordPos);

    chipsFrameImage->SetWorldPosition(tipsPos);


    loadingTime -= deltaTime;


    if (_has_finished_preloading() && loadingTime <= 0.0f)
    {
        _transition(preload_scene, {});
    }



}



bool LoadingScene::Uninitialize(ID3D11Device* device)
{
    SceneBase::Uninitialize(device);
    return true;
}

void LoadingScene::Render(ID3D11DeviceContext* immediateContext, float deltaTime)
{
#if 0
    //定数バッファをGPUに送信
    {
        //shaderToyCBuffer->Activate(immediateContext, 7);
    }
    SceneBase::Render(immediateContext, deltaTime);
    backImage->Draw(immediateContext);
    chipsFrameImage->Draw(immediateContext);
    chipsImage->Draw(immediateContext);
    if (loadingSprite)
    {
        loadingSprite->Render(immediateContext, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        //gameOverSprite->Render(immediateContext, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

#else
    RenderState::BindSamplerStates(immediateContext);
    RenderState::BindBlendState(immediateContext, BLEND_STATE::ALPHA);
    RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_ON_ZW_ON);
    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_BACK);

    // IBL
    immediateContext->PSSetShaderResources(32, 1, environmentTextures[0].GetAddressOf());
    immediateContext->PSSetShaderResources(33, 1, environmentTextures[1].GetAddressOf());
    immediateContext->PSSetShaderResources(34, 1, environmentTextures[2].GetAddressOf());
    immediateContext->PSSetShaderResources(35, 1, environmentTextures[3].GetAddressOf());

    D3D11_VIEWPORT viewport;
    UINT num_viewports{ 1 };
    immediateContext->RSGetViewports(&num_viewports, &viewport);

    // 定数バッファ更新
    {
        auto& shader = Scene::GetCurrentScene()->GetSceneSettings().sceneShaderConstants;

        shaderCBuffer->data.shadowColor = shader.shadowColor;
        shaderCBuffer->data.shadowDepthBias = shader.shadowDepthBias;
        shaderCBuffer->data.slopeBias = shader.slopeBias;
        shaderCBuffer->data.splitU = shader.splitU;

        shaderCBuffer->data.hueShift = shader.hueShift;
        shaderCBuffer->data.saturation = shader.saturation;
        shaderCBuffer->data.brightness = shader.brightness;
        shaderCBuffer->data.contrast = shader.contrast;

        shaderCBuffer->data.focusDistance = shader.focusDistance;
        shaderCBuffer->data.dofNearRange = shader.dofNearRange;
        shaderCBuffer->data.dofRange = shader.dofRange;
        shaderCBuffer->data.dofBlurStrength = shader.dofBlurStrength;

        shaderCBuffer->data.objectIblIntensity = shader.objectIblIntensity;
        //shaderCBuffer->data.renderStep = shader.renderStep; // これはImGuiで
        shaderCBuffer->data.enableToneMapping = shader.enableToneMapping;
        shaderCBuffer->data.enableSsao = shader.enableSsao;

        shaderCBuffer->data.enableCascadedShadowMaps = shader.enableCascadedShadowMaps;
        shaderCBuffer->data.enableSsr = shader.enableSsr;
        shaderCBuffer->data.enableFog = shader.enableFog;
        shaderCBuffer->data.enableBloom = shader.enableBloom;

        shaderCBuffer->data.enableBlur = shader.enableBlur;
        shaderCBuffer->data.enableDof = shader.enableDof;
        shaderCBuffer->data.colorizeCascadedLayer = shader.colorizeCascadedLayer;
        shaderCBuffer->data.toneMappingValue = shader.toneMappingValue;

        shaderCBuffer->data.colorMapRGB = shader.colorMapRGB;
        shaderCBuffer->data.pad3 = shader.pad3;

        sceneCBuffer->Activate(immediateContext, 1);
        shaderCBuffer->Activate(immediateContext, 9);
    }
    // シーンからポイントライト集める
    lightManager->CollectPointLightsFromScene(*this);
    lightManager->Apply(immediateContext, 11);

    // カメラのビュー定数を更新
    ViewConstants data = {};
    if (auto camera = cameraManager->GetRenderCamera(this))
    {
        data = camera->GetViewConstants();
        sceneRender.UpdateViewConstants(immediateContext, data);
    }
    else
    {
        Logger::Error(U8("カメラがない"));
    }

#ifdef USE_IMGUI
    imGuiGizmoBuffer->Clear(immediateContext);
    imGuiGizmoBuffer->Activate(immediateContext);
#endif


    // ディファードレンダリング
    gBufferRenderTarget->Clear(immediateContext);
    gBufferRenderTarget->Acticate(immediateContext);

    auto queues = sceneRender.BuildRenderQueues();

    RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_ON_ZW_ON);
    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
    sceneRender.currentRenderPath = RenderPath::Deferred;
    sceneRender.RenderOpaque(immediateContext, queues.meshes);
    ExecuteHooks(RenderPass::Opaque, immediateContext);

    sceneRender.RenderMask(immediateContext, queues.meshes);
    ExecuteHooks(RenderPass::Mask, immediateContext);

    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_BACK);
    gBufferRenderTarget->Deactivate(immediateContext);

    DirectX::XMFLOAT4X4 cameraView;
    DirectX::XMFLOAT4X4 cameraProjection;

    cameraView = data.view;
    cameraProjection = data.projection;

    // 影を作る処理
    auto& shadow = Scene::GetCurrentScene()->GetSceneSettings().cascadedShadowMapConstants;

    cascadedShadowMaps->Clear(immediateContext);
    cascadedShadowMaps->Activate(immediateContext, cameraView, cameraProjection, lightManager->GetLightDirection(), shadow.criticalDepthValue, 3/*cbSlot*/);
    RenderState::BindBlendState(immediateContext, BLEND_STATE::NONE);
    RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_ON_ZW_ON);
    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
    sceneRender.currentRenderPath = RenderPath::Shadow;
    sceneRender.CastShadowRender(immediateContext, queues.shadowCasters);
    cascadedShadowMaps->Deactivate(immediateContext);
    // ライティングのパス
    {
        frameBuffer->Clear(immediateContext);
        frameBuffer->Activate(immediateContext);

        // スカイマップを描画
        RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
        skyMap->Blit(immediateContext, data.viewProjection);
        ExecuteHooks(RenderPass::Sky, immediateContext);

        //dummyTexture->Draw(immediateContext);

        RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA);
        RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);

        ID3D11ShaderResourceView* shaderResourceViews[]
        {
            gBufferRenderTarget->renderTargetShaderResourceViews[static_cast<int>(SRV_SLOT::NORMAL)],  // normalMap
            gBufferRenderTarget->renderTargetShaderResourceViews[static_cast<int>(SRV_SLOT::PBR_VALUE)],   // msrMap
            gBufferRenderTarget->renderTargetShaderResourceViews[static_cast<int>(SRV_SLOT::COLOR)],   // colorMap
            gBufferRenderTarget->renderTargetShaderResourceViews[static_cast<int>(SRV_SLOT::POSITION)],   // positionMap
            gBufferRenderTarget->renderTargetShaderResourceViews[static_cast<int>(SRV_SLOT::EMISSIVE)],   // emissiveMap
        };
        // メインフレームバッファとブルームエフェクトを組み合わせて描画
        fullscreenQuad->Blit(immediateContext, shaderResourceViews, 0, _countof(shaderResourceViews), deferredPs.Get());
        frameBuffer->Deactivate(immediateContext);
    }

    frameBuffer->Activate(immediateContext, gBufferRenderTarget->depthStencilView);

#if 1
    RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA);
    RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_ON_ZW_OFF);
    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_FRONT);
    sceneRender.currentRenderPath = RenderPath::Forward;
    sceneRender.RenderBlend(immediateContext, queues.meshes); // ここで警告出る
    ExecuteHooks(RenderPass::ForwardBlend, immediateContext);

    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_BACK);
    sceneRender.currentRenderPath = RenderPath::Forward;
    sceneRender.RenderBlend(immediateContext, queues.meshes); // ここで警告出る
    ExecuteHooks(RenderPass::ForwardBlend, immediateContext);

#endif // 1

#if 0
    // PARTICLES
    {

        //深度ステンシルステート設定
        RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_ON_ZW_OFF, 1);
        //ラスタライザ設定
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);

        //定数バッファ更新

        // パーティクル描画
        EffectManager::Render(immediateContext);

        ExecuteHooks(RenderPass::Particle, immediateContext);
    }

#endif // 0

    // デバック描画
#if _DEBUG
    if (useDrawDebug)
    {
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_BACK);
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::WIREFRAME_CULL_NONE);
        //Physics::Instance().Render(cameraView, cameraProjection, { lightDirection.x,lightDirection.y,lightDirection.z });
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_BACK);
        DebugRender::Render(immediateContext);
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::WIREFRAME_CULL_NONE);
        DebugRender::WiredRender(immediateContext);
        ExecuteHooks(RenderPass::Debug, immediateContext);
    }
#endif
    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_BACK);

    frameBuffer->Deactivate(immediateContext);

#if 1
    sceneEffectManager->ApplyAll(immediateContext, frameBuffer->shaderResourceViews[0].Get(), gBufferRenderTarget->renderTargetShaderResourceViews[static_cast<int>(SRV_SLOT::NORMAL)],
        gBufferRenderTarget->depthStencilShaderResourceView, gBufferRenderTarget->renderTargetShaderResourceViews[static_cast<int>(SRV_SLOT::POSITION)], gBufferRenderTarget->renderTargetShaderResourceViews[static_cast<int>(SRV_SLOT::PBR_VALUE)], cascadedShadowMaps->depthMap().Get());

    ID3D11ShaderResourceView* nullSRVs[16] = {};
    immediateContext->PSSetShaderResources(0, 16, nullSRVs);
#endif

    // FINAL_PASS
    {
        RenderState::BindBlendState(immediateContext, BLEND_STATE::NONE);
        RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);

        ID3D11ShaderResourceView* shader_resource_views[]
        {
              frameBuffer->shaderResourceViews[0].Get(),//colorMap   こっちライティング済み
              gBufferRenderTarget->renderTargetShaderResourceViews[static_cast<int>(SRV_SLOT::POSITION)],   // positionMap
              gBufferRenderTarget->renderTargetShaderResourceViews[static_cast<int>(SRV_SLOT::NORMAL)],   // normalMap
              gBufferRenderTarget->depthStencilShaderResourceView,      //depthMap
              sceneEffectManager->GetOutput("BloomEffect"),
              cascadedShadowMaps->depthMap().Get(),   //cascadedShadowMaps
        };
        // メインフレームバッファとブルームエフェクトを組み合わせて描画
        fullscreenQuad->Blit(immediateContext, shader_resource_views, 0, _countof(shader_resource_views), finalPs.Get());
    }

    // UIの描画
    Draw(immediateContext);

    ExecuteHooks(RenderPass::UI, immediateContext);




#ifdef USE_IMGUI
    imGuiGizmoBuffer->Deactivate(immediateContext);
#endif

#endif
}


void LoadingScene::DrawGui()
{
#ifdef USE_IMGUI
    ImGui::Begin(U8("調整"));
    ImGui::DragFloat2("tipsPos", &tipsPos.x);
    ImGui::DragFloat2("tipsWordOffset", &tipsWordOffset.x);
    ImGui::End();
    SceneBase::DrawGui();
#endif

}

