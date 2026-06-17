#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <cstdint>
#include "Engine/Utility/Win32Utils.h"

class RenderTarget
{
public:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

    void Create(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format)
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = device->CreateRenderTargetView(texture.Get(), nullptr, rtv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        hr = device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    };
};

class TemporalAA
{
public:
    void Initialize(ID3D11Device* device, UINT width, UINT height)
    {
        history[0].Create(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        history[1].Create(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        fullscreenQuad = std::make_unique<FullScreenQuad>(device);

        HRESULT hr = CreatePsFromCSO(device, "./Data/Shaders/TemporalAntiAliasingPS.cso", copyPs.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    void Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* sceneColor, ID3D11ShaderResourceView* velocity)
    {
        if (firstFrame)
        {
            ID3D11ShaderResourceView* srvs[]
            {
                sceneColor,
                velocity,
                sceneColor
            };

            fullscreenQuad->Blit(
                immediateContext,
                srvs,
                0,
                _countof(srvs),
                copyPs.Get());

            firstFrame = false;

            std::swap(current, previous);

            return;
        }

        UINT viewportCount{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };
        D3D11_VIEWPORT cachedViewPorts[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        immediateContext->RSGetViewports(&viewportCount, cachedViewPorts);

        // ビューポートの設定
        D3D11_VIEWPORT scene_viewport{};
        scene_viewport.TopLeftX = 0;
        scene_viewport.TopLeftY = 0;

#if _DEBUG
        scene_viewport.Width = 1920;// Graphics::GetScreenWidth();
        scene_viewport.Height = 1080;    // Graphics::GetScreenHeight();
#else
        scene_viewport.Width = Graphics::GetScreenWidth();
        scene_viewport.Height = Graphics::GetScreenHeight();
#endif

        scene_viewport.MinDepth = 0.0f;
        scene_viewport.MaxDepth = 1.0f;
        immediateContext->RSSetViewports(1, &scene_viewport);


        ID3D11RenderTargetView* oldRTV = nullptr;
        ID3D11DepthStencilView* oldDSV = nullptr;

        immediateContext->OMGetRenderTargets(
            1,
            &oldRTV,
            &oldDSV);

        ID3D11ShaderResourceView* nullSRVs[16] = {};
        immediateContext->PSSetShaderResources(0, 16, nullSRVs);

        ID3D11RenderTargetView* rtvs[]
        {
            history[current].rtv.Get()
        };

        immediateContext->OMSetRenderTargets(
            1,
            rtvs,
            nullptr);

        float clearColor[4] =
        {
            1,0,0,1
        };
        immediateContext->ClearRenderTargetView(
            history[current].rtv.Get(),
            clearColor);

        ID3D11ShaderResourceView* srvs[]
        {
            sceneColor,
            velocity,
            history[previous].srv.Get(),
        };

#if 1
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
        fullscreenQuad->Blit(
            immediateContext,
            srvs,
            0,
            _countof(srvs),
            copyPs.Get());
#else
        Microsoft::WRL::ComPtr<ID3D11Resource> srcResource;

        sceneColor->GetResource(srcResource.GetAddressOf());

        immediateContext->CopyResource(
            history[current].texture.Get(),
            srcResource.Get());
#endif // 0
        std::swap(current, previous);

        //immediateContext->RSSetViewports(viewportCount, cachedViewPorts);


        immediateContext->PSSetShaderResources(
            0,
            16,
            nullSRVs);

        immediateContext->RSSetViewports(viewportCount, cachedViewPorts);

        immediateContext->OMSetRenderTargets(
            1,
            &oldRTV,
            oldDSV);

        if (oldRTV)
        {
            oldRTV->Release();
            oldRTV = nullptr;
        }

        if (oldDSV)
        {
            oldDSV->Release();
            oldDSV = nullptr;
        }
    }

    RenderTarget history[2];

    int current = 0;
    int previous = 1;
    std::unique_ptr<FullScreenQuad> fullscreenQuad;

private:
    Microsoft::WRL::ComPtr<ID3D11PixelShader> copyPs;
    bool firstFrame = true;
};