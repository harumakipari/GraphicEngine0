#include "pch.h"
#include "RadialBlurEffect.h"

#include <algorithm>

#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/Shader.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace
{
    constexpr int kMinimumSamplingCount = 1;
    constexpr int kMaximumSamplingCount = 32;
}

void RadialBlurEffect::Initialize(ID3D11Device* device, uint32_t inWidth, uint32_t inHeight)
{
    width = inWidth;
    height = inHeight;

    radialBlurCBuffer = std::make_unique<ConstantBuffer<RadialBlurConstantBuffer>>(device);
    fullScreenQuad = std::make_unique<FullScreenQuad>(device);
    outputBuffer = std::make_unique<FrameBuffer>(device, width, height, false);

    const HRESULT hr = CreatePsFromCSO(device, "./Data/Shaders/RadialBlurPS.cso", radialBlurPs.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void RadialBlurEffect::Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* sceneColor)
{
    if (!IsEnabled() || !sceneColor || !outputBuffer || !fullScreenQuad || !radialBlurCBuffer || !radialBlurPs)
    {
        return;
    }

    RadialBlurConstantBuffer& constants = radialBlurCBuffer->data;
    constants.blurCenter.x = std::clamp(constants.blurCenter.x, 0.0f, 1.0f);
    constants.blurCenter.y = std::clamp(constants.blurCenter.y, 0.0f, 1.0f);
    constants.blurRadiusPixels = std::clamp(constants.blurRadiusPixels, 0.0f, 150.0f);
    constants.maskRadiusPixels = std::clamp(constants.maskRadiusPixels, 0.0f, 400.0f);
    constants.blurIntensity = std::clamp(constants.blurIntensity, 0.0f, 1.0f);
    constants.samplingCount = std::clamp(constants.samplingCount, kMinimumSamplingCount, kMaximumSamplingCount);
    constants.screenSize = { static_cast<float>(width), static_cast<float>(height) };

    Microsoft::WRL::ComPtr<ID3D11Buffer> previousPixelShaderConstantBuffer;
    immediateContext->PSGetConstantBuffers(8, 1, previousPixelShaderConstantBuffer.GetAddressOf());

    outputBuffer->Clear(immediateContext);
    outputBuffer->Activate(immediateContext);

    RenderState::BindBlendState(immediateContext, BLEND_STATE::NONE);
    RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);

    radialBlurCBuffer->Activate(immediateContext, 8);
    ID3D11ShaderResourceView* shaderResourceViews[] = { sceneColor };
    fullScreenQuad->Blit(immediateContext, shaderResourceViews, 0, _countof(shaderResourceViews), radialBlurPs.Get());

    outputBuffer->Deactivate(immediateContext);

    ID3D11ShaderResourceView* nullShaderResourceViews[] = { nullptr };
    immediateContext->PSSetShaderResources(0, _countof(nullShaderResourceViews), nullShaderResourceViews);
    immediateContext->PSSetConstantBuffers(8, 1, previousPixelShaderConstantBuffer.GetAddressOf());
}

ID3D11ShaderResourceView* RadialBlurEffect::GetOutputSRV() const
{
    return outputBuffer ? outputBuffer->shaderResourceViews[0].Get() : nullptr;
}

void RadialBlurEffect::DrawDebugUI()
{
#ifdef USE_IMGUI
    if (!radialBlurCBuffer)
    {
        return;
    }

    RadialBlurConstantBuffer& constants = radialBlurCBuffer->data;
    ImGui::Begin("Radial Blur");
    ImGui::Checkbox("Enable (Manual Preview)", &manualPreviewEnabled);
    ImGui::Text("Just Dodge Active: %s", justDodgeActive ? "true" : "false");
    ImGui::SliderFloat("Blur Radius", &constants.blurRadiusPixels, 0.0f, 150.0f);
    ImGui::SliderInt("Sampling Count", &constants.samplingCount, kMinimumSamplingCount, kMaximumSamplingCount);
    ImGui::SliderFloat2("Blur Center", &constants.blurCenter.x, 0.0f, 1.0f);
    ImGui::SliderFloat("Mask Radius", &constants.maskRadiusPixels, 0.0f, 400.0f);
    ImGui::SliderFloat("Blur Intensity", &constants.blurIntensity, 0.0f, 1.0f);
    ImGui::End();
#endif
}
