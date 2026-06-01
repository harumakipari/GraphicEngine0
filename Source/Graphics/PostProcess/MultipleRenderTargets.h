#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <cstdint>

#include "Engine/Utility/Win32Utils.h"

enum class M_SRV_SLOT :uint8_t
{
    COLOR,
    POSITION,
    NORMAL,
};

class MultipleRenderTargets
{
public:
    MultipleRenderTargets(ID3D11Device* device, UINT width, UINT height, UINT bufferCount, const DXGI_FORMAT* formats = nullptr) :bufferCount(bufferCount)
    {
        _ASSERT_EXPR(bufferCount <= maxBufferCount, L"The maximum number of buffers has been exceded.");

        HRESULT hr = S_OK;

        for (UINT bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
        {
            DXGI_FORMAT format = formats ? formats[bufferIndex] : DXGI_FORMAT_R32G32B32A32_FLOAT;
            renderTargetFormats[bufferIndex] = format;  // 保存

            Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTargetBuffer;

            D3D11_TEXTURE2D_DESC texture2dDesc = {};
            texture2dDesc.Width = width;
            texture2dDesc.Height = height;
            texture2dDesc.MipLevels = 1;
            texture2dDesc.ArraySize = 1;
            //texture2dDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;  // DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R16G16B16A16FLOAT : DXGI_FORMAT_R32G32B32A332_FLOAT
            texture2dDesc.Format = format;  // DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R16G16B16A16FLOAT : DXGI_FORMAT_R32G32B32A332_FLOAT
            texture2dDesc.SampleDesc.Count = 1;
            texture2dDesc.SampleDesc.Quality = 0;
            texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
            texture2dDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            texture2dDesc.CPUAccessFlags = 0;
            texture2dDesc.MiscFlags = 0;
            hr = device->CreateTexture2D(&texture2dDesc, 0, renderTargetBuffer.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
            renderTargetViewDesc.Format = texture2dDesc.Format;
            renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            hr = device->CreateRenderTargetView(renderTargetBuffer.Get(), &renderTargetViewDesc, &renderTargetViews[bufferIndex]);
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
            shaderResourceViewDesc.Format = texture2dDesc.Format;
            shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            shaderResourceViewDesc.Texture2D.MipLevels = 1;
            hr = device->CreateShaderResourceView(renderTargetBuffer.Get(), &shaderResourceViewDesc, &renderTargetShaderResourceViews[bufferIndex]);
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;

        D3D11_TEXTURE2D_DESC texture2dDesc = {};
        texture2dDesc.Width = width;
        texture2dDesc.Height = height;
        texture2dDesc.MipLevels = 1;
        texture2dDesc.ArraySize = 1;
        texture2dDesc.Format = DXGI_FORMAT_R32_TYPELESS;    // DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_R32_TYPELESS
        texture2dDesc.SampleDesc.Count = 1;
        texture2dDesc.SampleDesc.Quality = 0;
        texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
        texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        texture2dDesc.CPUAccessFlags = 0;
        texture2dDesc.MiscFlags = 0;
        hr = device->CreateTexture2D(&texture2dDesc, 0, depthStencilBuffer.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
        depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depthStencilViewDesc.Flags = 0;
        hr = device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilViewDesc, &depthStencilView);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
        shaderResourceViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;
        hr = device->CreateShaderResourceView(depthStencilBuffer.Get(), &shaderResourceViewDesc, &depthStencilShaderResourceView);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        for (UINT bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
        {
            viewports[bufferIndex].Width = static_cast<float>(width);
            viewports[bufferIndex].Height = static_cast<float>(height);
            viewports[bufferIndex].MinDepth = 0.0f;
            viewports[bufferIndex].MaxDepth = 1.0f;
            viewports[bufferIndex].TopLeftX = 0.0f;
            viewports[bufferIndex].TopLeftY = 0.0f;
        }
    }

    virtual ~MultipleRenderTargets()
    {
        for (UINT bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
        {
            if (renderTargetViews[bufferIndex])
            {
                renderTargetViews[bufferIndex]->Release();
            }
            if (renderTargetShaderResourceViews[bufferIndex])
            {
                renderTargetShaderResourceViews[bufferIndex]->Release();
            }
        }
        if (depthStencilView)
        {
            depthStencilView->Release();
        }
        if (depthStencilShaderResourceView)
        {
            depthStencilShaderResourceView->Release();
        }
    }
    const UINT bufferCount;

    //スマートポインタに？
    static const UINT maxBufferCount = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
    ID3D11RenderTargetView* renderTargetViews[maxBufferCount];
    ID3D11ShaderResourceView* renderTargetShaderResourceViews[maxBufferCount];
    ID3D11DepthStencilView* depthStencilView;
    ID3D11ShaderResourceView* depthStencilShaderResourceView;
    D3D11_VIEWPORT viewports[maxBufferCount];

    void Clear(ID3D11DeviceContext* immediateContext, float r = 0, float g = 0, float b = 0, float a = 1, float depth = 1)
    {
        float color[4]{ r,g,b,a };
        for (UINT bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
        {
            immediateContext->ClearRenderTargetView(renderTargetViews[bufferIndex], color);
        }
        immediateContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, depth, 0);
    }
    void Activate(ID3D11DeviceContext* immediateContext, ID3D11DepthStencilView* adopted_depth_stencil_view = nullptr)
    {
        viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        immediateContext->RSGetViewports(&viewportCount, catchedViewports);
        immediateContext->OMGetRenderTargets(bufferCount, catchedRenderTargetViews, &catchedDepthStencilView);

        immediateContext->RSSetViewports(bufferCount, viewports);
        immediateContext->OMSetRenderTargets(bufferCount, renderTargetViews, depthStencilView);
    }
    void Deactivate(ID3D11DeviceContext* immediateContext)
    {
#if 0
        // None that unlike some other reource methods in Direct3D, all currently bound render targets will be unbound by calling OMSetRenderTargets(0, nullptr, nullptr);
        immediateContext->OMSetRenderTargets(0, NULL, NULL);
#endif
        immediateContext->RSSetViewports(viewportCount, catchedViewports);
        immediateContext->OMSetRenderTargets(bufferCount, catchedRenderTargetViews, catchedDepthStencilView);
        for (UINT bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
        {
            if (catchedRenderTargetViews[bufferIndex])
            {
                catchedRenderTargetViews[bufferIndex]->Release();
            }
        }
        if (catchedDepthStencilView)
        {
            catchedDepthStencilView->Release();
        }
    }

private:
    UINT viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_VIEWPORT catchedViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    ID3D11RenderTargetView* catchedRenderTargetViews[maxBufferCount];
    ID3D11DepthStencilView* catchedDepthStencilView;
    DXGI_FORMAT renderTargetFormats[maxBufferCount];
};