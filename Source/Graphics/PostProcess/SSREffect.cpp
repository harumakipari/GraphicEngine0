#include "pch.h"
#include "SSREffect.h"

#include "imgui.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Utility/Win32Utils.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/Shader.h"


void SSREffect::Initialize(ID3D11Device* device, uint32_t width, uint32_t height)
{
    fullScreenQuad = std::make_unique<FullScreenQuad>(device);
    ssrBuffer = std::make_unique<FrameBuffer>(device, width / 2, height / 2, false);
    ssrCBuffer = std::make_unique<ConstantBuffer<SSRConstantBuffer>>(device);

    HRESULT hr = CreatePsFromCSO(device, "./Data/Shaders/SSRPS.cso", ssrPS.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void SSREffect::Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gbufferColor, ID3D11ShaderResourceView* gbufferNormal, ID3D11ShaderResourceView* gbufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap)
{
    auto& ssr = Scene::GetCurrentScene()->GetSceneSettings().ssrConstantBuffer;

    ssrCBuffer->data.reflectionIntensity = ssr.reflectionIntensity;
    ssrCBuffer->data.maxDistance = ssr.maxDistance;
    ssrCBuffer->data.resolution = ssr.resolution;
    ssrCBuffer->data.steps = ssr.steps;
    ssrCBuffer->data.thickness = ssr.thickness;
    ssrCBuffer->Activate(immediateContext, 5);

    ssrBuffer->Clear(immediateContext, 0, 0, 0, 0);
    ssrBuffer->Activate(immediateContext);

    RenderState::BindBlendState(immediateContext, BLEND_STATE::NONE);
    RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
    ID3D11ShaderResourceView* shaderResourceViews[]
    {
        gBufferPosition,        //gBufferPosition
        gbufferNormal,          //gbufferNormal
        gbufferColor,           //gbufferColor
        gBufferPbrValue         //gBufferPbrValue
    };
    fullScreenQuad->Blit(immediateContext, shaderResourceViews, 0, _countof(shaderResourceViews), ssrPS.Get());

    ssrBuffer->Deactivate(immediateContext);
}

void SSREffect::DrawDebugUI()
{
#ifdef USE_IMGUI
    auto& ssr = Scene::GetCurrentScene()->GetSceneSettings().ssrConstantBuffer;

    ImGui::Checkbox("enable", &enabled);
    ImGui::SliderFloat(U8("反射の強度"), &ssr.reflectionIntensity, 0.0f, 1.0f);
    ImGui::SliderFloat(U8("最大反射距離"), &ssr.maxDistance, 0.0f, 30.0f);
    ImGui::SliderFloat(U8("解像度・探索の粗さ"), &ssr.resolution, 0.0f, 1.0f);
    ImGui::SliderInt(U8("ステップ数"), &ssr.steps, 0, 20);
    ImGui::SliderFloat(U8("厚みの許容範囲"), &ssr.thickness, 0.0f, 1.0f);
#endif
}