#include "pch.h"
#include "Graphics.h"

#include <string>


#include "Engine/Utility/Win32Utils.h"
#include "Engine/Scene/Scene.h"

CONST LONG SCREEN_WIDTH{ 1280 };
CONST LONG SCREEN_HEIGHT{ 720 };
CONST BOOL FULLSCREEN{ FALSE };
//CONST BOOL FULLSCREEN{ TRUE };

void AcquireHighPerformanceAdapter(IDXGIFactory6* dxgi_factory6, IDXGIAdapter3** dxgi_adapter3)
{
    HRESULT hr{ S_OK };

    Microsoft::WRL::ComPtr<IDXGIAdapter3> enumerated_adapter;
    for (UINT adapter_index = 0; DXGI_ERROR_NOT_FOUND != dxgi_factory6->EnumAdapterByGpuPreference(adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(enumerated_adapter.ReleaseAndGetAddressOf())); ++adapter_index)
    {
        DXGI_ADAPTER_DESC1 adapter_desc;
        hr = enumerated_adapter->GetDesc1(&adapter_desc);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        if (adapter_desc.VendorId == 0x1002/*AMD*/ || adapter_desc.VendorId == 0x10DE/*NVIDIA*/)
        {
            OutputDebugStringW((std::wstring(adapter_desc.Description) + L" has been selected.\n").c_str());
            OutputDebugStringA(std::string("\tVendorId:" + std::to_string(adapter_desc.VendorId) + '\n').c_str());
            OutputDebugStringA(std::string("\tDeviceId:" + std::to_string(adapter_desc.DeviceId) + '\n').c_str());
            OutputDebugStringA(std::string("\tSubSysId:" + std::to_string(adapter_desc.SubSysId) + '\n').c_str());
            OutputDebugStringA(std::string("\tRevision:" + std::to_string(adapter_desc.Revision) + '\n').c_str());
            OutputDebugStringA(std::string("\tDedicatedVideoMemory:" + std::to_string(adapter_desc.DedicatedVideoMemory) + '\n').c_str());
            OutputDebugStringA(std::string("\tDedicatedSystemMemory:" + std::to_string(adapter_desc.DedicatedSystemMemory) + '\n').c_str());
            OutputDebugStringA(std::string("\tSharedSystemMemory:" + std::to_string(adapter_desc.SharedSystemMemory) + '\n').c_str());
            OutputDebugStringA(std::string("\tAdapterLuid.HighPart:" + std::to_string(adapter_desc.AdapterLuid.HighPart) + '\n').c_str());
            OutputDebugStringA(std::string("\tAdapterLuid.LowPart:" + std::to_string(adapter_desc.AdapterLuid.LowPart) + '\n').c_str());
            OutputDebugStringA(std::string("\tFlags:" + std::to_string(adapter_desc.Flags) + '\n').c_str());
            break;
        }
    }
    *dxgi_adapter3 = enumerated_adapter.Detach();
}


void Graphics::Initialize(HWND hwnd, BOOL fullscreen)
{
    hWnd = hwnd;

    fullscreenMode = fullscreen;

    if (fullscreen)
    {
        StylizeWindow(fullscreen);
    }

    RECT client_rect;
    GetClientRect(hwnd, &client_rect);
    framebufferDimensions.cx = client_rect.right - client_rect.left;
    framebufferDimensions.cy = client_rect.bottom - client_rect.top;

    screenWidth = static_cast<float>(framebufferDimensions.cx);
    screenHeight = static_cast<float>(framebufferDimensions.cy);

    HRESULT hr{ S_OK };

    UINT create_factory_flags{};
#ifdef _DEBUG
    create_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    hr = CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(dxgi_factory6.GetAddressOf()));
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    AcquireHighPerformanceAdapter(dxgi_factory6.Get(), adapter.GetAddressOf());

    UINT createDeviceFlags{ 0 };
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
#ifdef ENABLE_DIRECT2D
    createDeviceFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#endif

    D3D_FEATURE_LEVEL featureLevels{ D3D_FEATURE_LEVEL_11_1 };
    hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, 0, createDeviceFlags, &featureLevels, 1, D3D11_SDK_VERSION, &device, NULL, &immediateContext);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    CreateSwapChain(hwnd, dxgi_factory6.Get());

    // レンダラ生成
    primitiveRenderer = std::make_unique<PrimitiveRenderer>(device.Get());
    shapeRenderer = std::make_unique<PrimitiveShapeRenderer>(device.Get());
}



// クリア  画面の初期化
void Graphics::Clear(float r, float g, float b, float a)
{
    // 画面を初期化する（色を指定してレンダーターゲットをクリア）
    FLOAT color[]{ r, g, b, a };
    immediateContext->ClearRenderTargetView(renderTargetView.Get(), color);
    // 深度ステンシルビューをクリア
    immediateContext->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

// レンダーターゲット設定
void Graphics::SetRenderTargets()
{
    // レンダーターゲットと深度ステンシルビューを設定  　描画ターゲットを設定
    immediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());
}

// 画面表示
void Graphics::Present(UINT syncInterval)
{
    swapChain->Present(syncInterval, DXGI_PRESENT_ALLOW_TEARING);// フレームをスクリーンに表示
}


void Graphics::CreateSwapChain(HWND hwnd, IDXGIFactory6* dxgi_factory6)
{
    HRESULT hr{ S_OK };

    if (swapChain)
    {
        ID3D11RenderTargetView* null_render_target_view{};
        immediateContext->OMSetRenderTargets(1, &null_render_target_view, NULL);
        renderTargetView.Reset();
#if 0
        immediate_context->Flush();
        immediate_context->ClearState();
#endif
        DXGI_SWAP_CHAIN_DESC swapChainDesc{};
        swapChain->GetDesc(&swapChainDesc);
        hr = swapChain->ResizeBuffers(swapChainDesc.BufferCount, framebufferDimensions.cx, framebufferDimensions.cy, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTargetBuffer;
        hr = swapChain->GetBuffer(0, IID_PPV_ARGS(renderTargetBuffer.GetAddressOf()));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        D3D11_TEXTURE2D_DESC texture2dDesc;
        renderTargetBuffer->GetDesc(&texture2dDesc);

        hr = device->CreateRenderTargetView(renderTargetBuffer.Get(), NULL, renderTargetView.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    else
    {
        BOOL allowTearing = FALSE;
        if (SUCCEEDED(hr))
        {
            hr = dxgi_factory6->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        }
        tearingSupported = SUCCEEDED(hr) && allowTearing;

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc1{};
        swapChainDesc1.Width = framebufferDimensions.cx;
        swapChainDesc1.Height = framebufferDimensions.cy;
        swapChainDesc1.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDesc1.Stereo = FALSE;
        swapChainDesc1.SampleDesc.Count = 1;
        swapChainDesc1.SampleDesc.Quality = 0;
        swapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc1.BufferCount = 2;
        swapChainDesc1.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc1.Flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        hr = dxgi_factory6->CreateSwapChainForHwnd(device.Get(), hwnd, &swapChainDesc1, NULL, NULL, swapChain.ReleaseAndGetAddressOf());
#if 0
        swap_chain_desc1.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
#endif
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = dxgi_factory6->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID3D11Texture2D> render_target_buffer;
        hr = swapChain->GetBuffer(0, IID_PPV_ARGS(render_target_buffer.GetAddressOf()));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = device->CreateRenderTargetView(render_target_buffer.Get(), NULL, renderTargetView.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

#if 1
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer{};
    D3D11_TEXTURE2D_DESC texture2dDesc{};
    texture2dDesc.Width = framebufferDimensions.cx;
    texture2dDesc.Height = framebufferDimensions.cy;
    texture2dDesc.MipLevels = 1;
    texture2dDesc.ArraySize = 1;
    texture2dDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    texture2dDesc.SampleDesc.Count = 1;
    texture2dDesc.SampleDesc.Quality = 0;
    texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
    texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    texture2dDesc.CPUAccessFlags = 0;
    texture2dDesc.MiscFlags = 0;
    hr = device->CreateTexture2D(&texture2dDesc, NULL, depthStencilBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
    depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Texture2D.MipSlice = 0;
    hr = device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilViewDesc, depthStencilView.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#else
    Microsoft::WRL::ComPtr<ID3D11Texture2D> render_target_buffer;
    D3D11_TEXTURE2D_DESC texture2d_desc{};
    texture2d_desc.Width = framebufferDimensions.cx;
    texture2d_desc.Height = framebufferDimensions.cy;
    texture2d_desc.MipLevels = 1;
    texture2d_desc.ArraySize = 1;
    texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // DXGI_FORMAT_R8G8B8A8_UNORM
    texture2d_desc.SampleDesc.Count = 1;
    texture2d_desc.SampleDesc.Quality = 0;
    texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
    texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texture2d_desc.CPUAccessFlags = 0;
    texture2d_desc.MiscFlags = 0;
    hr = device->CreateTexture2D(&texture2d_desc, 0, render_target_buffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc{};
    render_target_view_desc.Format = texture2d_desc.Format;
    render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = device->CreateRenderTargetView(render_target_buffer.Get(), &render_target_view_desc, renderTargetView.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


    Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_stencil_buffer;
    texture2d_desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    hr = device->CreateTexture2D(&texture2d_desc, 0, depth_stencil_buffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
    depth_stencil_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depth_stencil_view_desc.Flags = 0;
    hr = device->CreateDepthStencilView(depth_stencil_buffer.Get(), &depth_stencil_view_desc, depthStencilView.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


#endif
    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(framebufferDimensions.cx);
    viewport.Height = static_cast<float>(framebufferDimensions.cy);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    immediateContext->RSSetViewports(1, &viewport);

}

void Graphics::OnSizeChanged(HWND hwnd, UINT64 width, UINT height)
{
    HRESULT hr{ S_OK };

    if (width == 0 || height == 0)
    {
        return;
    }

    if (width > 0 && height > 0 && (width != framebufferDimensions.cx || height != framebufferDimensions.cy))
    {
        framebufferDimensions.cx = static_cast<LONG>(width);
        framebufferDimensions.cy = height;
        //追加
        screenWidth = static_cast<float>(framebufferDimensions.cx);
        screenHeight = static_cast<float>(framebufferDimensions.cy);

        Scene::_on_size_changed(device.Get(), framebufferDimensions.cx, framebufferDimensions.cy);

        immediateContext->Flush();
        immediateContext->ClearState();


        CreateSwapChain(hwnd, dxgi_factory6.Get());
#if 0
        Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory2;
        hr = swapChain->GetParent(IID_PPV_ARGS(dxgi_factory2.GetAddressOf()));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc1{};
        swap_chain_desc1.Width = static_cast<UINT>(framebufferDimensions.cx);
        swap_chain_desc1.Height = framebufferDimensions.cy;
        swap_chain_desc1.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swap_chain_desc1.Stereo = FALSE;
        swap_chain_desc1.SampleDesc.Count = 1;
        swap_chain_desc1.SampleDesc.Quality = 0;
        swap_chain_desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc1.BufferCount = 2;
        swap_chain_desc1.Scaling = DXGI_SCALING_STRETCH;
        swap_chain_desc1.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swap_chain_desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        hr = dxgi_factory2->CreateSwapChainForHwnd(device.Get(), hwnd, &swap_chain_desc1, nullptr, nullptr, swapChain.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#if 1
        hr = dxgi_factory2->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif

        Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
        hr = swapChain->GetParent(IID_PPV_ARGS(dxgi_factory.GetAddressOf()));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        // DXGI_MWA_NO_WINDOW_CHANGES - Prevent DXGI from monitoring an applications message queue; this makes DXGI unable to respond to mode changes.
        // DXGI_MWA_NO_ALT_ENTER - Prevent DXGI from responding to an alt - enter sequence.
        // DXGI_MWA_NO_PRINT_SCREEN - Prevent DXGI from responding to a print - scree
        hr = dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
        hr = swapChain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = device->CreateRenderTargetView(back_buffer.Get(), NULL, renderTargetView.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_stencil_buffer;
        D3D11_TEXTURE2D_DESC texture2d_desc{};
        texture2d_desc.Width = static_cast<UINT>(framebufferDimensions.cx);
        texture2d_desc.Height = framebufferDimensions.cy;
        texture2d_desc.MipLevels = 1;
        texture2d_desc.ArraySize = 1;
        texture2d_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        texture2d_desc.SampleDesc.Count = 1;
        texture2d_desc.SampleDesc.Quality = 0;
        texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
        texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        texture2d_desc.CPUAccessFlags = 0;
        texture2d_desc.MiscFlags = 0;
        hr = device->CreateTexture2D(&texture2d_desc, NULL, depth_stencil_buffer.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
        depth_stencil_view_desc.Format = texture2d_desc.Format;
        depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depth_stencil_view_desc.Texture2D.MipSlice = 0;
        hr = device->CreateDepthStencilView(depth_stencil_buffer.Get(), &depth_stencil_view_desc, depthStencilView.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = static_cast<float>(framebufferDimensions.cx);
        viewport.Height = static_cast<float>(framebufferDimensions.cy);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        immediateContext->RSSetViewports(1, &viewport);

#ifdef ENABLE_DIRECT2D
        Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
        hr = device.As(&dxgi_device2);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID2D1Factory1> d2d_factory1;
        D2D1_FACTORY_OPTIONS factory_options{};
#ifdef _DEBUG
        factory_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_options, d2d_factory1.GetAddressOf());

        Microsoft::WRL::ComPtr<ID2D1Device> d2d_device;
        hr = d2d_factory1->CreateDevice(dxgi_device2.Get(), d2d_device.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d1DeviceContext.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = dxgi_device2->SetMaximumFrameLatency(1);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<IDXGISurface2> dxgi_surface2;
        hr = swapChain->GetBuffer(0, IID_PPV_ARGS(dxgi_surface2.GetAddressOf()));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_bitmap1;
        hr = d2d1DeviceContext->CreateBitmapFromDxgiSurface(dxgi_surface2.Get(),
            D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)), d2d_bitmap1.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        d2d1DeviceContext->SetTarget(d2d_bitmap1.Get());

        // UNIT.99:Create COM object for DirectWrite.
        Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dwrite_factory.GetAddressOf()));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        //hr = dwrite_factory->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12, L"", dwrite_text_format.ReleaseAndGetAddressOf());


        //hr = dwrite_factory->CreateTextFormat(L"Meiryo", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11, L"", dwrite_text_format.ReleaseAndGetAddressOf());
        //_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        //hr = dwrite_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        //_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        //hr = d2d1_device_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), d2d_solid_color_brush.ReleaseAndGetAddressOf());
        //_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif


#endif

    }

}

#ifdef ENABLE_DIRECT2D
void Graphics::CreateDirect2dObjects()
{
    HRESULT hr{ S_OK };

    Microsoft::WRL::ComPtr<IDXGIDevice2> dxgiDevice2;
    hr = device.As(&dxgiDevice2);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory1;
    D2D1_FACTORY_OPTIONS factory_options{};
#ifdef _DEBUG
    factory_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_options, d2dFactory1.GetAddressOf());

    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice;
    hr = d2dFactory1->CreateDevice(dxgiDevice2.Get(), d2dDevice.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d1DeviceContext.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    hr = dxgiDevice2->SetMaximumFrameLatency(1);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    Microsoft::WRL::ComPtr<IDXGISurface2> dxgi_surface2;
    hr = swapChain->GetBuffer(0, IID_PPV_ARGS(dxgi_surface2.GetAddressOf()));
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_bitmap1;
    hr = d2d1DeviceContext->CreateBitmapFromDxgiSurface(dxgi_surface2.Get(),
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)), d2d_bitmap1.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    d2d1DeviceContext->SetTarget(d2d_bitmap1.Get());


    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dwrite_factory.GetAddressOf()));
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    hr = dwrite_factory->CreateTextFormat(L"Meiryo", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11, L"", dwriteTextFormats[0].ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = dwriteTextFormats[0]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    hr = dwrite_factory->CreateTextFormat(L"Impact", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24, L"", dwriteTextFormats[1].ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = dwriteTextFormats[1]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    hr = d2d1DeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), d2dSolidColorBrushes[0].ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = d2d1DeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::CornflowerBlue), d2dSolidColorBrushes[1].ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}
#endif

void Graphics::StylizeWindow(BOOL fullscreen)
{
    fullscreenMode = fullscreen;
    if (fullscreen)
    {
        GetWindowRect(hWnd, &windowedRect);
        SetWindowLongPtrA(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

        RECT fullscreen_window_rect;

        HRESULT hr{ E_FAIL };
        if (swapChain)
        {
            Microsoft::WRL::ComPtr<IDXGIOutput> dxgi_output;
            hr = swapChain->GetContainingOutput(&dxgi_output);
            if (hr == S_OK)
            {
                DXGI_OUTPUT_DESC output_desc;
                hr = dxgi_output->GetDesc(&output_desc);
                if (hr == S_OK)
                {
                    fullscreen_window_rect = output_desc.DesktopCoordinates;
                }
            }
        }
        if (hr != S_OK)
        {
            DEVMODE devmode = {};
            devmode.dmSize = sizeof(DEVMODE);
            EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);

            fullscreen_window_rect = {
                devmode.dmPosition.x,
                devmode.dmPosition.y,
                devmode.dmPosition.x + static_cast<LONG>(devmode.dmPelsWidth),
                devmode.dmPosition.y + static_cast<LONG>(devmode.dmPelsHeight)
            };
        }
        SetWindowPos(
            hWnd,
            NULL,
            fullscreen_window_rect.left,
            fullscreen_window_rect.top,
            fullscreen_window_rect.right,
            fullscreen_window_rect.bottom,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(hWnd, SW_MAXIMIZE);
    }
    else
    {
        //SetWindowLongPtrA(hwnd, GWL_STYLE, windowedStyle);
        SetWindowLongPtrA(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW ^ WS_MAXIMIZEBOX ^ WS_THICKFRAME | WS_VISIBLE);
        SetWindowPos(
            hWnd,
            HWND_NOTOPMOST,
            windowedRect.left,
            windowedRect.top,
            windowedRect.right - windowedRect.left,
            windowedRect.bottom - windowedRect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(hWnd, SW_NORMAL);
    }
}
