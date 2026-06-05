#pragma once

#include <windows.h>
#include <tchar.h>
#include <sstream>

#include "Engine/Utility/Win32Utils.h"
#include "Engine/Utility/Time.h"


#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "../External/imgui/imgui.h"
#include "../External/imgui//imgui_internal.h"
#include "../External/imgui//imgui_impl_dx11.h"
#include "../External/imgui//imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];
#endif


#include <d3d11_1.h>
//COMオブジェクトをComPtrスマートポインターテンプレートを使った変数宣言に変更する
#include <wrl.h>
#include <dxgi1_6.h>

#define ENABLE_DIRECT2D
#ifdef ENABLE_DIRECT2D
#include <d2d1_1.h>
#include <dwrite.h>
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dwrite.lib")
#endif


//スマートポインタを使う
#include <memory>

#include "Graphics/Core/Graphics.h"
#include "Graphics/Core/RenderState.h"

#include "Graphics/Sprite/Sprite.h"
#include "Graphics/Sprite/SpriteBatch.h"
#include "Graphics/Resource/GeometricPrimitive.h"
#include "Engine/Effects/CoreComputeParticleSystem.h"

#ifndef _DEBUG
CONST LONG SCREEN_WIDTH{ 1920 };
CONST LONG SCREEN_HEIGHT{ 1080 };
#else
CONST LONG SCREEN_WIDTH{ 1280 };
CONST LONG SCREEN_HEIGHT{ 720 };
#endif
CONST LPCWSTR APPLICATION_NAME{ L"X3DGP" };

class Framework
{
public:
    //タイムスケール（ヒットストップやポーズでいじる）
    static inline const float defaultTimeScale = 1.0f;
    static inline float timeScale = 1.0f;

#ifdef USE_IMGUI

    ImFont* fontJP;

#endif
    CONST HWND hwnd;

    Framework(HWND hwnd, BOOL fullscreen);
    ~Framework();

    Framework(const Framework&) = delete;
    Framework& operator=(const Framework&) = delete;
    Framework(Framework&&) noexcept = delete;
    Framework& operator=(Framework&&) noexcept = delete;

    int run()
    {
        MSG msg{};

        if (!Initialize())
        {
            return 0;
        }

#ifdef USE_IMGUI
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f, nullptr, glyphRangesJapanese);
        // 日本語用（メイリオ or MSゴシック）
         fontJP = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\meiryo.ttc",
            //"C:\\Windows\\Fonts\\msgothic.ttc",
            16.0f,
            nullptr,
            ImGui::GetIO().Fonts->GetGlyphRangesJapanese()
        );
        ImGui::GetIO().FontGlobalScale = 1.5f;
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(Graphics::GetDevice(), Graphics::GetDeviceContext());
#if 1
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ImGui_ImplDX11_CreateDeviceObjects();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // ドッキングを有効にする
#endif // 1

        ImGui::StyleColorsDark();
#endif

        while (WM_QUIT != msg.message)
        {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                tictoc.Tick();
                calculate_frame_stats();

                // SCENE_TRANSITION
#ifdef USE_IMGUI
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();
#endif

                bool skipRendering = Update(tictoc.DeltaTime());

                Render(tictoc.DeltaTime(), skipRendering);
                //Render(tictoc.DeltaTime());

#ifdef USE_IMGUI
                ImGui::Render();
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif
                //UINT sync_interval{ vsync ? 1U : 0U };
                //UINT flags = (tearing_supported && !fullscreenMode && !vsync) ? DXGI_PRESENT_ALLOW_TEARING : 0;
                //Graphics::GetSwapChain()->Present(sync_interval, flags);
                UINT sync_interval{ 0 };
                Graphics::Present(sync_interval);
            }
        }

#ifdef USE_IMGUI
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
#endif

#if 0
        BOOL fullscreen = 0;
        Graphics::GetSwapChain()->GetFullscreenState(&fullscreen, 0);
        if (fullscreen)
        {
            Graphics::GetSwapChain()->SetFullscreenState(FALSE, 0);
        }
#endif

        return Uninitialize() ? static_cast<int>(msg.wParam) : 0;
    }

    LRESULT CALLBACK handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
#ifdef USE_IMGUI
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) { return true; }
#endif
        switch (msg)
        {
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);

            EndPaint(hwnd, &ps);
        }
        break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_CREATE:
            break;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE)
            {
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
            break;
        case WM_ENTERSIZEMOVE:
            tictoc.Stop();
            break;
        case WM_EXITSIZEMOVE:
            tictoc.Start();
            break;
        case WM_SIZE:
        {
#if 1
            RECT client_rect{};
            GetClientRect(hwnd, &client_rect);
            Graphics::OnSizeChanged(hwnd, static_cast<UINT64>(client_rect.right - client_rect.left), client_rect.bottom - client_rect.top);
#endif
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
        }
        return 0;
    }

    static inline bool isPaused = false;// Profiler用
    static void SetPause(bool pause)
    {
        isPaused = pause;
    }

private:
    bool Initialize();
    bool Update(float elapsed_time/*Elapsed seconds from last frame*/);
    void Render(float elapsed_time/*Elapsed seconds from last frame*/, bool renderable);
    bool Uninitialize();
private:
    Time tictoc;
    uint32_t frames{ 0 };
    float elapsed_time{ 0.0f };
    void calculate_frame_stats()
    {
        if (++frames, (tictoc.TimeStamp() - elapsed_time) >= 1.0f)
        {
            float fps = static_cast<float>(frames);
            std::wostringstream outs;
            outs.precision(6);
            outs << APPLICATION_NAME << L" : FPS : " << fps << L" / " << L"Frame Time : " << 1000.0f / fps << L" (ms)";
            SetWindowTextW(hwnd, outs.str().c_str());

            frames = 0;
            elapsed_time += 1.0f;
        }
    }


    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShaders[8];

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceViews[8];

private:
    BOOL fullscreenMode{ FALSE };
    BOOL vsync{ FALSE };
    BOOL tearingSupported{ FALSE };

    bool enableImGui = false;   // ImGuiの有効化フラグ

    RECT windowedRect;
    DWORD windowedStyle;

    std::unique_ptr<CoreComputeParticleSystem> particleSystem;

    std::shared_ptr<Sprite> back;
    std::shared_ptr<Sprite> backBoss;
    std::shared_ptr<Sprite> black;
};
