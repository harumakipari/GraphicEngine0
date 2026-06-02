#include "pch.h"
#include "BloomEffect.h"
#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "imgui.h"
#endif

#include<vector>

#include "Engine/Scene/Scene.h"
#include "Graphics/Core/Shader.h"
#include "Engine/Utility/Win32Utils.h"

void BloomEffect::Initialize(ID3D11Device* device, uint32_t width, uint32_t height)
{
    bitBlockTransfer = std::make_unique<FullScreenQuad>(device);
    HRESULT hr{ S_OK };

    glowExtraction = std::make_unique<FrameBuffer>(device, width, height, false);
    for (size_t downsampled_index = 0; downsampled_index < downsampledCount; ++downsampled_index)
    {
        gaussianBlur[downsampled_index][0] = std::make_unique<FrameBuffer>(device, width >> downsampled_index, height >> downsampled_index, false);
        gaussianBlur[downsampled_index][1] = std::make_unique<FrameBuffer>(device, width >> downsampled_index, height >> downsampled_index, false);
    }
    hr = CreatePsFromCSO(device, "./Data/Shaders/GlowExtractionPS.cso", glowExtractionPs.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/GaussianBlurDownSamplingPS.cso", gaussianBlurDownsamplingPs.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/GaussianBlurHorizontalPS.cso", gaussianBlurHorizontalPs.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/GaussianBlurVerticalPS.cso", gaussianBlurVerticalPs.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/GaussianBlurUpsamplingPS.cso", gaussianBlurUpsamplingPs.ReleaseAndGetAddressOf());
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

    bloomConstant = std::make_unique<ConstantBuffer<BloomConstantBuffer>>(device);

}

void BloomEffect::Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gBufferColor, ID3D11ShaderResourceView* gBufferNormal,
    ID3D11ShaderResourceView* gBufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap)
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

    auto& bloom = Scene::GetCurrentScene()->GetSceneSettings().bloomConstantBuffer;
    bloomConstant->data.bloomExtractionThreshold = bloom.bloomExtractionThreshold;
    bloomConstant->data.bloomIntensity = bloom.bloomIntensity;
    bloomConstant->Activate(immediateContext, 8);

    // Extracting bright color
    glowExtraction->Clear(immediateContext, 0, 0, 0, 1);
    glowExtraction->Activate(immediateContext);
    bitBlockTransfer->Blit(immediateContext, &gBufferColor, 0, 1, glowExtractionPs.Get());
    glowExtraction->Deactivate(immediateContext);
    immediateContext->PSSetShaderResources(0, 1, &nullShaderResourceView);

    // Gaussian blur
    // Efficient Gaussian blur with linear sampling
    // Downsampling
    gaussianBlur[0][0]->Clear(immediateContext, 0, 0, 0, 1);
    gaussianBlur[0][0]->Activate(immediateContext);
    bitBlockTransfer->Blit(immediateContext, glowExtraction->shaderResourceViews[0].GetAddressOf(), 0, 1, gaussianBlurDownsamplingPs.Get());
    gaussianBlur[0][0]->Deactivate(immediateContext);
    immediateContext->PSSetShaderResources(0, 1, &nullShaderResourceView);

    // Ping-pong gaussian blur
    gaussianBlur[0][1]->Clear(immediateContext, 0, 0, 0, 1);
    gaussianBlur[0][1]->Activate(immediateContext);
    bitBlockTransfer->Blit(immediateContext, gaussianBlur[0][0]->shaderResourceViews[0].GetAddressOf(), 0, 1, gaussianBlurHorizontalPs.Get());
    gaussianBlur[0][1]->Deactivate(immediateContext);
    immediateContext->PSSetShaderResources(0, 1, &nullShaderResourceView);

    gaussianBlur[0][0]->Clear(immediateContext, 0, 0, 0, 1);
    gaussianBlur[0][0]->Activate(immediateContext);
    bitBlockTransfer->Blit(immediateContext, gaussianBlur[0][1]->shaderResourceViews[0].GetAddressOf(), 0, 1, gaussianBlurVerticalPs.Get());
    gaussianBlur[0][0]->Deactivate(immediateContext);
    immediateContext->PSSetShaderResources(0, 1, &nullShaderResourceView);

    for (size_t downsampled_index = 1; downsampled_index < downsampledCount; ++downsampled_index)
    {
        // Downsampling
        gaussianBlur[downsampled_index][0]->Clear(immediateContext, 0, 0, 0, 1);
        gaussianBlur[downsampled_index][0]->Activate(immediateContext);
        bitBlockTransfer->Blit(immediateContext, gaussianBlur[downsampled_index - 1][0]->shaderResourceViews[0].GetAddressOf(), 0, 1, gaussianBlurDownsamplingPs.Get());
        gaussianBlur[downsampled_index][0]->Deactivate(immediateContext);
        immediateContext->PSSetShaderResources(0, 1, &nullShaderResourceView);

        // Ping-pong gaussian blur
        gaussianBlur[downsampled_index][1]->Clear(immediateContext, 0, 0, 0, 1);
        gaussianBlur[downsampled_index][1]->Activate(immediateContext);
        bitBlockTransfer->Blit(immediateContext, gaussianBlur[downsampled_index][0]->shaderResourceViews[0].GetAddressOf(), 0, 1, gaussianBlurHorizontalPs.Get());
        gaussianBlur[downsampled_index][1]->Deactivate(immediateContext);
        immediateContext->PSSetShaderResources(0, 1, &nullShaderResourceView);

        gaussianBlur[downsampled_index][0]->Clear(immediateContext, 0, 0, 0, 1);
        gaussianBlur[downsampled_index][0]->Activate(immediateContext);
        bitBlockTransfer->Blit(immediateContext, gaussianBlur[downsampled_index][1]->shaderResourceViews[0].GetAddressOf(), 0, 1, gaussianBlurVerticalPs.Get());
        gaussianBlur[downsampled_index][0]->Deactivate(immediateContext);
        immediateContext->PSSetShaderResources(0, 1, &nullShaderResourceView);
    }

    // Downsampling
    glowExtraction->Clear(immediateContext, 0, 0, 0, 1);
    glowExtraction->Activate(immediateContext);
    std::vector<ID3D11ShaderResourceView*> shader_resource_views;
    for (size_t downsampled_index = 0; downsampled_index < downsampledCount; ++downsampled_index)
    {
        shader_resource_views.push_back(gaussianBlur[downsampled_index][0]->shaderResourceViews[0].Get());
    }
    bitBlockTransfer->Blit(immediateContext, shader_resource_views.data(), 0, downsampledCount, gaussianBlurUpsamplingPs.Get());
    glowExtraction->Deactivate(immediateContext);
    immediateContext->PSSetShaderResources(0, 1, &nullShaderResourceView);

    // Restore states
    immediateContext->PSSetConstantBuffers(8, 1, cachedConstantBuffer.GetAddressOf());

    immediateContext->OMSetDepthStencilState(cachedDepthStencilState.Get(), 0);
    immediateContext->RSSetState(cachedRasterizerState.Get());
    immediateContext->OMSetBlendState(cachedBlendState.Get(), blendFactor, sampleMask);

    immediateContext->PSSetShaderResources(0, downsampledCount, cachedShaderResourceViews);
    for (ID3D11ShaderResourceView* cached_shader_resource_view : cachedShaderResourceViews)
    {
        if (cached_shader_resource_view) cached_shader_resource_view->Release();
    }
}

void BloomEffect::DrawDebugUI()
{
#ifdef USE_IMGUI
    auto& bloom = Scene::GetCurrentScene()->GetSceneSettings().bloomConstantBuffer;
    ImGui::SliderFloat(U8("ãPÇ´íäèoÇÃËáíl"), &bloom.bloomExtractionThreshold, 0.0f, 20.0f);
    ImGui::SliderFloat(U8("ãPÇ´ÇÃã≠ìx"), &bloom.bloomIntensity, 0.0f, 5.0f);
#endif
}