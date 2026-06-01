#pragma once

#include <d3d11_1.h>
#include <wrl.h>
#include <memory>
#include <dxgi1_6.h>

#include "Graphics/Renderer/PrimitiveRenderer.h"
#include "Graphics/Renderer/PrimitiveShapeRenderer.h"

//Direct2D関連を有効にするマクロ
#define ENABLE_DIRECT2D
#ifdef ENABLE_DIRECT2D
#include <d2d1_1.h>		  // Direct2Dを使うためのヘッダ
#include <dwrite.h>		  // DirectWrite（テキストレンダリング）のヘッダ
#pragma comment(lib,"d2d1.lib")// Direct2Dライブラリをリンク
#pragma comment(lib,"dwrite.lib")// DirectWriteライブラリをリンク
#endif


class Graphics
{
private:
    Graphics() = default;
    ~Graphics() = default;
public:
    static size_t VideoMemoryUsage()
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO video_memory_info;
        adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &video_memory_info);
        return video_memory_info.CurrentUsage / 1024 / 1024;
    }

    // 初期化
    static void Initialize(HWND hWnd, BOOL fullscreen);

    // クリア
    static void Clear(float r, float g, float b, float a);

    // レンダーターゲット設定
    static void SetRenderTargets();

    // 画面表示
    static void Present(UINT syncInterval);

    // ウインドウハンドル取得
    static HWND GetWindowHandle() { return hWnd; }

    // デバイス取得
    static ID3D11Device* GetDevice() { return device.Get(); }

    // デバイスコンテキスト取得
    static ID3D11DeviceContext* GetDeviceContext() { return immediateContext.Get(); }

    //スワップチェーンを取得
    static IDXGISwapChain* GetSwapChain() { return swapChain.Get(); }

    // スクリーン幅取得
    static float GetScreenWidth() { return screenWidth; }

    // スクリーン高さ取得
    static float GetScreenHeight() { return screenHeight; }

    // スワップチェーンを作成する
    static void CreateSwapChain(HWND hwnd, IDXGIFactory6* dxgi_factory6);

    // ウィンドウの外観をフルスクリーンに設定する
    static void StylizeWindow(BOOL fullscreen);

    // ウィンドウサイズ変更時の処理を行う
    static void OnSizeChanged(HWND hwnd, UINT64 width, UINT height);

    // プリミティブレンダラ取得
    static PrimitiveRenderer* GetPrimitiveRenderer() { return primitiveRenderer.get(); }

    // シェイプレンダラ取得
    static PrimitiveShapeRenderer* GetShapeRenderer() { return shapeRenderer.get(); }

    static HWND GetHwnd() { return hWnd; }

    static void SetViewport(float x, float y, float w, float h)
    {
        viewportX = x;
        viewportY = y;
        viewportW = w;
        viewportH = h;
    }

    static void GetViewport(float& x, float& y, float& w, float& h)
    {
        x = viewportX;
        y = viewportY;
        w = viewportW;
        h = viewportH;
    }

    static void GetScreenSize(float& w, float& h)
    {
        w = viewportW;
        h = viewportH;
    }

    static inline BOOL fullscreenMode{ FALSE };// フルスクリーンモードかどうか
private:
    static inline HWND hWnd = nullptr;// ウィンドウのハンドル
    static inline SIZE framebufferDimensions;// フレームバッファのサイズ（画面解像度）
    static inline Microsoft::WRL::ComPtr<ID3D11Device>			device;// Direct3Dのデバイス（GPU）
    static inline Microsoft::WRL::ComPtr<ID3D11DeviceContext>		immediateContext;// デバイスコンテキスト（描画の管理）
    static inline Microsoft::WRL::ComPtr<IDXGISwapChain1>			swapChain;// スワップチェーン（画面表示のバッファ管理）
    static inline Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	renderTargetView;// レンダーターゲットビュー（描画先）
    static inline Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	depthStencilView;// 深度ステンシルビュー（3D描画の深度管理）
    static inline D3D11_VIEWPORT									viewport; // ビューポート（描画範囲）
    static inline Microsoft::WRL::ComPtr<IDXGIFactory6> dxgi_factory6;

#ifdef ENABLE_DIRECT2D
    static inline Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2d1DeviceContext;
    static inline Microsoft::WRL::ComPtr<IDWriteTextFormat> dwriteTextFormats[8];// テキストフォーマット
    static inline Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> d2dSolidColorBrushes[8]; // ソリッドカラーのブラシ
    void CreateDirect2dObjects(); // Direct2Dのオブジェクトを作成するメソッド
#endif

    static inline float	screenWidth = 0;// スクリーンの幅
    static inline float	screenHeight = 0;// スクリーンの高さ

    static inline BOOL vsync{ FALSE };// 垂直同期の設定
    static inline BOOL tearingSupported{ FALSE };// テアリング（画面のちらつき）をサポートしているか

    static inline RECT windowedRect;// ウィンドウモードの際のウィンドウ位置・サイズ
    static inline DWORD windowedStyle; // ウィンドウスタイル（通常時）

    static inline Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;// GPUアダプタ（実際のGPU情報）

    static inline std::unique_ptr<PrimitiveRenderer>				primitiveRenderer;
    static inline std::unique_ptr<PrimitiveShapeRenderer>			shapeRenderer;

    static inline float viewportX = 0;
    static inline float viewportY = 0;
    static inline float viewportW = 0;
    static inline float viewportH = 0;
};