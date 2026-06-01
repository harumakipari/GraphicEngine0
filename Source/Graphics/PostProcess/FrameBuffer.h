#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <cstdint>

class FrameBuffer
{
public:
    FrameBuffer(ID3D11Device* device, uint32_t width, uint32_t height,  bool withDepthStencil = true, DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT);
    virtual ~FrameBuffer() = default;
        
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceViews[2];
    D3D11_VIEWPORT viewport;
    
    void Clear(ID3D11DeviceContext* immediateContext,
        float r = 0, float g = 0, float b = 0, float a = 1, float depth = 1);
    void Activate(ID3D11DeviceContext* immediateContext, ID3D11DepthStencilView* adoptedDepthStencilView = NULL);
    void Deactivate(ID3D11DeviceContext* immediateContext);

private:
    UINT viewportCount{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };
    D3D11_VIEWPORT cachedViewPorts[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cachedRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cachedDepthStencilView;
};