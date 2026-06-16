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

        HRESULT hr = CreatePsFromCSO(device, "./Data/Shaders/FullScreenCopyPS.cso", copyPs.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    void Apply(
        ID3D11DeviceContext* immediateContext,
        ID3D11ShaderResourceView* sceneColor,
        ID3D11ShaderResourceView* velocity)
    {
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
            sceneColor
        };

#if 1
        fullscreenQuad->Blit(
            immediateContext,
            srvs,
            0,
            1,
            copyPs.Get());
#else
        Microsoft::WRL::ComPtr<ID3D11Resource> srcResource;

        sceneColor->GetResource(srcResource.GetAddressOf());

        immediateContext->CopyResource(
            history[current].texture.Get(),
            srcResource.Get()); 
#endif // 0
        std::swap(current, previous);

        immediateContext->PSSetShaderResources(
            0,
            16,
            nullSRVs);


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

    Microsoft::WRL::ComPtr<ID3D11PixelShader> copyPs;
};