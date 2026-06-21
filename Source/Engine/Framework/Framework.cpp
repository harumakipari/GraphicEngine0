#include "pch.h"
#include "Framework.h"

#include <profiler.h>

#include "Graphics/Core/Shader.h"
#include <dxgi1_3.h>
#include <memory>
#include "Graphics/Resource/Texture.h"

#include "Engine/Scene/SceneRegistry.h"
#include "Graphics/Core/RenderState.h"

#include "Engine/Input/InputSystem.h"
#include "Graphics/Renderer/ShapeRenderer.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Debug/DebugRender.h"
#include "Engine/Debug/Logger.h"
#include "Engine/Effects/EffectEditor.h"
#include "Engine/Effects/EffectManager.h"
#include "UI/Game/SceneTransitionManager.h"


//コンストラクタ：ウィンドウハンドルを受け取って初期化
Framework::Framework(HWND hwnd, BOOL fullscreen) : hwnd(hwnd), fullscreenMode(fullscreen), windowedStyle(static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)))
{
    //#ifndef _DEBUG
    fullscreenMode = true;
    //#endif
    Graphics::Initialize(hwnd, fullscreenMode);
    InputSystem::Initialize();
    RenderState::Initialize();
    CoreAudio::Initialize();
    // ログ初期化
#ifdef _DEBUG
    Logger::Initialize();
#endif

#ifdef USE_IMGUI
    enableImGui = true;
#endif
}

bool Framework::Initialize()
{
    ////デバイス・デバイスコンテクスト・スワップチェーンの作成
    ID3D11Device* device = Graphics::GetDevice();
    if (!device) 
    {
        assert("ModelComponent Error: device is null\n");
    }

    // ShapeRendererを初期化
    ShapeRenderer::Initialize(device);

    SceneTransitionManager::Instance().Initialize();

    //Scene::_boot(device, "MainScene", SCREEN_WIDTH, SCREEN_HEIGHT, {});
    //Scene::_boot(device, "TitleScene", SCREEN_WIDTH, SCREEN_HEIGHT, {});
    //Scene::_boot(device, "ResultScene", SCREEN_WIDTH, SCREEN_HEIGHT, {});
    //Scene::_boot(device, "GameScene", SCREEN_WIDTH, SCREEN_HEIGHT, {});
    Scene::_boot(device, "SampleScene", SCREEN_WIDTH, SCREEN_HEIGHT, {});

    //パーティクルシステム
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleTexture;
    HRESULT hr = LoadTextureFromFile(device, L"./Data/Effect/Textures/particle.png", particleTexture.GetAddressOf(), NULL);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    particleSystem = std::make_unique<CoreComputeParticleSystem>(device, 30000, particleTexture);

    // エフェクトマネージャー初期化
    EffectManager::Initialize();

    //エフェクトエディタ初期化
    EffectEditor::Initialize();

    // フォントを初期化
    FontManager::Initialize(Graphics::GetDevice(), "./Data/Font/BuildFont.fnt");

    //プロファイラ初期化
    ProfileInitialize(&isPaused, Framework::SetPause/*, ImGuiControl::Profiler::DefaultMaxThreads*/);
    ProfileThreadName(0, "Main Thread");

    //back = std::make_shared<Sprite>(device, L"./Data/Textures/UI/backGround.png");
    back = std::make_shared<Sprite>(device, L"./Data/Textures/ScissorsUI/scene_change.png");
    black = std::make_shared<Sprite>(device, L"./Data/Textures/ScissorsUI/black.png");
    backBoss = std::make_shared<Sprite>(device, L"./Data/Textures/ScissorsUI/scene_change_boss.png");

    return true;
}

bool Framework::Update(float deltaTime/*Elapsed seconds from last frame*/)
{
    //デバイスコンテクストを取得
    ID3D11DeviceContext* immediateContext = Graphics::GetDeviceContext();
    //オーディオ更新
    CoreAudio::Update(deltaTime);

    {
        ProfileScopedSection_2(0, "InputUpdate", ImGuiControl::Profiler::Green);
        //入力システム更新
        if (GetForegroundWindow() == Graphics::GetHwnd())
        {
            InputSystem::Update(Time::UnscaledDeltaTime());
        }
    }


    // デバックコマンド更新
    DebugRender::Tick(deltaTime);

    SceneTransitionManager::Instance().Update(deltaTime);

    bool skipRendering;
    // SCENE_TRANSITION
    {
        ProfileScopedSection_2(0, "SceneUpdate", ImGuiControl::Profiler::Blue);
        skipRendering = Scene::_update(immediateContext, deltaTime);
    }


#ifdef USE_IMGUI
    ProfileNewFrame();


#endif

    if ((GetAsyncKeyState(VK_RETURN) & 1) && (GetAsyncKeyState(VK_MENU) & 0x8000))
    {
        Graphics::StylizeWindow(!Graphics::fullscreenMode);
    }

#ifdef _DEBUG
    if (InputSystem::GetInputState("F1", InputStateMask::Trigger))
    {// ImGuiの有効化トグル
        enableImGui = !enableImGui;
    }
#endif
    //パーティクルシステム更新
    {
        ProfileScopedSection_2(0, "ComputeParticleSystem::Update", ImGuiControl::Profiler::Blue);
        particleSystem->Update(Graphics::GetDeviceContext(), deltaTime);

        // エフェクトマネージャ更新
        EffectManager::Update(deltaTime);
    }

    return skipRendering;

}

void Framework::Render(float elapsed_time/*Elapsed seconds from last frame*/, bool skipRendering)
{
    HRESULT hr{ S_OK };

    //デバイスコンテクストを取得
    ID3D11DeviceContext* immediateContext = Graphics::GetDeviceContext();
    // サンプラーステートを設定
    RenderState::SetSamplerState(immediateContext);

    ID3D11RenderTargetView* nullRenderTargetViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    immediateContext->OMSetRenderTargets(_countof(nullRenderTargetViews), nullRenderTargetViews, 0);
    ID3D11ShaderResourceView* nullShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
    immediateContext->VSSetShaderResources(0, _countof(nullShaderResourceViews), nullShaderResourceViews);
    immediateContext->PSSetShaderResources(0, _countof(nullShaderResourceViews), nullShaderResourceViews);

    //// 画面を初期化する（色を指定してレンダーターゲットをクリア）
    // 画面クリア
    Graphics::Clear(0.2f, 0.2f, 0.2f, 0.0f);

    // レンダーターゲット設定
    Graphics::SetRenderTargets();

    // SCENE_TRANSITION
    if (!skipRendering)
    {
        {
            ProfileScopedSection_2(0, "Render", ImGuiControl::Profiler::Red);
            Scene::_render(immediateContext, elapsed_time);//
        }
        //gameManager->GenerateOutputAll();
    }
    else
    {
        auto& param = SceneTransitionManager::Instance().GetParams();
        if (param.contains("fade"))
        {
            std::string name = param.at("fade");
            if (name == "0")
            {
                black->Render(immediateContext, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                Logger::Log(U8("blackを通った"));
            }
        }
        else
        {
            if (param.contains("stage"))
            {
                std::string stageName = param.at("stage");
                if (stageName == "BOSS")
                {
                    backBoss->Render(immediateContext, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                }
                else
                {
                    back->Render(immediateContext, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                }
            }
        }
        Logger::Log(U8("backを通った"));

    }
    if (enableImGui)
    {
#ifdef USE_IMGUI
        {
            ImGui::PushFont(fontJP);
            ProfileScopedSection_2(0, "ImGui", ImGuiControl::Profiler::Yellow);
            Scene::_drawGUI();
            ImGui::PopFont();

        }
        ImGui::Begin("ImGUI");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
#if 1
        ImGui::Text("Video memory usage %d MB", Graphics::VideoMemoryUsage());
#endif
        ImGui::Text("ALT+ENTER to change window mode");
        ImGui::Text("F1 ImGui on/off");
        ImGui::Text("F6 MovieCamera");
        ImGui::Text("F7 CinemaCamera");
        ImGui::Text("F8 DebugCamera");

        ImGui::End();
#endif


#if 0
#ifdef USE_IMGUI
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif

        //UINT sync_interval{ 0 };
        //swap_chain->Present(sync_interval, 0);
#endif
    }
}

bool Framework::Uninitialize()
{
    //プロファイラ終了
    ProfileShutdown();

    ID3D11Device* device = Graphics::GetDevice();

    // エフェクトマネージャー終了
    EffectManager::ClearAll();

    CoreAudio::ClearAll();

    //gameManager->UninitAll();
    // SCENE_TRANSITION
    Scene::_uninitialize(device);
    return true;
}

Framework::~Framework()
{
    ReleaseAllTextures();
}
