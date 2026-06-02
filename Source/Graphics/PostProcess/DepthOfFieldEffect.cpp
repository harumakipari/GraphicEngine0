#include "pch.h"
#include "DepthOfFieldEffect.h"

#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/Shader.h"
#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "imgui.h"
#endif


void DepthOfFieldEffect::Initialize(ID3D11Device* device, uint32_t width, uint32_t height)
{
    fullScreenQuad = std::make_unique<FullScreenQuad>(device);
    depthOfFieldBuffer = std::make_unique<FrameBuffer>(device, width / 4, height / 4, false);
    finalBokehBuffer = std::make_unique<FrameBuffer>(device, width / 4, height / 4, false);

    for (size_t downsampled_index = 0; downsampled_index < downsampledCount; ++downsampled_index)
    {
        gaussianBlur[downsampled_index][0] = std::make_unique<FrameBuffer>(device, width >> downsampled_index, height >> downsampled_index, false);
        gaussianBlur[downsampled_index][1] = std::make_unique<FrameBuffer>(device, width >> downsampled_index, height >> downsampled_index, false);
    }

    HRESULT hr = CreatePsFromCSO(device, "./Data/Shaders/DepthOfFieldPS.cso", bokehPS.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    hr = CreatePsFromCSO(device, "./Data/Shaders/GaussianBlurHorizontalPS.cso", gaussianBlurHorizontalPs.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/GaussianBlurVerticalPS.cso", gaussianBlurVerticalPs.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_BACK;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = 0;
    rasterizerDesc.DepthBiasClamp = 0;
    rasterizerDesc.SlopeScaledDepthBias = 0;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.ScissorEnable = FALSE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerState.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    hr = device->CreateDepthStencilState(&depthStencilDesc, depthStencilState.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = device->CreateBlendState(&blendDesc, blendState.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


}

void DepthOfFieldEffect::Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gBufferColor, ID3D11ShaderResourceView* gBufferNormal, ID3D11ShaderResourceView* gBufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap)
{
    // Store current states
    ID3D11ShaderResourceView* nullShaderResourceView{};
    ID3D11ShaderResourceView* cachedShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
    immediateContext->PSGetShaderResources(0, downsampledCount, cachedShaderResourceViews);

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  cachedDepthStencilState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>  cachedRasterizerState;
    Microsoft::WRL::ComPtr<ID3D11BlendState>  cachedBlendState;
    FLOAT blendFactor[4];
    UINT sampleMask;
    immediateContext->OMGetDepthStencilState(cachedDepthStencilState.GetAddressOf(), 0);
    immediateContext->RSGetState(cachedRasterizerState.GetAddressOf());
    immediateContext->OMGetBlendState(cachedBlendState.GetAddressOf(), blendFactor, &sampleMask);

    Microsoft::WRL::ComPtr<ID3D11Buffer>  cachedConstantBuffer;
    immediateContext->PSGetConstantBuffers(8, 1, cachedConstantBuffer.GetAddressOf());

    // Bind states
    immediateContext->OMSetDepthStencilState(depthStencilState.Get(), 0);
    immediateContext->RSSetState(rasterizerState.Get());
    immediateContext->OMSetBlendState(blendState.Get(), nullptr, 0xFFFFFFFF);


    // Gaussian blur シーンのライト済みテクスチャをダウンサンプリングしてからブラーをかける
    gaussianBlur[0][1]->Clear(immediateContext, 0, 0, 0, 1);
    gaussianBlur[0][1]->Activate(immediateContext);
    fullScreenQuad->Blit(immediateContext,&gBufferColor, 0, 1, gaussianBlurHorizontalPs.Get());
    gaussianBlur[0][1]->Deactivate(immediateContext);
    immediateContext->PSSetShaderResources(0, 1, &nullShaderResourceView);

    depthOfFieldBuffer->Clear(immediateContext, 0, 0, 0, 1);
    depthOfFieldBuffer->Activate(immediateContext);
    fullScreenQuad->Blit(immediateContext, gaussianBlur[0][1]->shaderResourceViews[0].GetAddressOf(), 0, 1, gaussianBlurVerticalPs.Get());
    depthOfFieldBuffer->Deactivate(immediateContext);
    immediateContext->PSSetShaderResources(0, 1, &nullShaderResourceView);


    finalBokehBuffer->Clear(immediateContext, 1, 1, 1, 1);
    finalBokehBuffer->Activate(immediateContext);

    RenderState::BindBlendState(immediateContext, BLEND_STATE::NONE);
    RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
    ID3D11ShaderResourceView* shaderResourceViews[]
    {
        gBufferColor,       //colorMap
        depthOfFieldBuffer->shaderResourceViews[0].Get(),   //bokehMap (ガウシアンブラーの結果を渡す予定)
        gBufferDepth,       //depthMap
    };
    fullScreenQuad->Blit(immediateContext, shaderResourceViews, 0, _countof(shaderResourceViews), bokehPS.Get());

    finalBokehBuffer->Deactivate(immediateContext);

    ID3D11ShaderResourceView* null_shader_resource_views[] =
    {
        NULL, NULL, NULL,
    };
    immediateContext->PSSetShaderResources(0, _countof(null_shader_resource_views), null_shader_resource_views);

    immediateContext->PSSetShaderResources(0, downsampledCount, cachedShaderResourceViews);

    for (UINT i = 0; i < downsampledCount; ++i)
    {
        if (cachedShaderResourceViews[i])
        {
            cachedShaderResourceViews[i]->Release();
        }
    }
}

void DepthOfFieldEffect::DrawDebugUI()
{
#ifdef USE_IMGUI
    ImGui::Image(depthOfFieldBuffer->shaderResourceViews[0].Get(), { 256, 256 }, { 0, 0 }, { 1, 1 });
    ImGui::Image(finalBokehBuffer->shaderResourceViews[0].Get(), { 256, 256 }, { 0, 0 }, { 1, 1 });
#endif
}
