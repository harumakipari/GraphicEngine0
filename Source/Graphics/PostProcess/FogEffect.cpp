#include "pch.h"
#include "FogEffect.h"

#include <DDSTextureLoader.h>

#include "imgui.h"
#include "Engine/Scene/Scene.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/Shader.h"
#include "Graphics/Resource/Texture.h"

void FogEffect::Initialize(ID3D11Device* device, uint32_t width, uint32_t height)
{
    fogCBuffer = std::make_unique<ConstantBuffer<FogConstants>>(device);
    fullScreenQuad = std::make_unique<FullScreenQuad>(device);
    fogBuffer = std::make_unique<FrameBuffer>(device, width / 2, height / 2, false, DXGI_FORMAT_R16_FLOAT);
    HRESULT hr = CreatePsFromCSO(device, "./Data/Shaders/VolumetricFogPS.cso", fogPS.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_TEXTURE2D_DESC texture2dDesc;
#if 0
    hr = LoadTextureFromFile(device, L"./Data/ShaderTextures/noise.png", noise2d.GetAddressOf(), &texture2dDesc);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#else
    hr = LoadTextureFromFile(device, L"./Data/ShaderTextures/noise1.png", noise2d.ReleaseAndGetAddressOf(), &texture2dDesc);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif

#if 1
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    hr = DirectX::CreateDDSTextureFromFile(device, L"./Data/ShaderTextures/_noise_3d.dds", resource.ReleaseAndGetAddressOf(), noise3d.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#else
    ////////PreComputeNoiseTexture3d(device.Get(), 64, noise3d.GetAddressOf());
#endif
}

void FogEffect::Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gBufferColor, ID3D11ShaderResourceView* gbufferNormal, ID3D11ShaderResourceView* gbufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap)
{
    auto& fog = Scene::GetCurrentScene()->GetSceneSettings().fogConstants;

    fogCBuffer->data.enableDither = static_cast<int>(fog.enableDither);
    fogCBuffer->data.fogDensity = fog.fogDensity;
    fogCBuffer->data.globalFogIntensity = fog.globalFogIntensity;
    fogCBuffer->data.fogColor = fog.fogColor;
    fogCBuffer->data.fogCutoffDistance = fog.fogCutoffDistance;
    fogCBuffer->data.fogHeightFalloff = fog.fogHeightFalloff;
    fogCBuffer->data.groundLevel = fog.groundLevel;
    fogCBuffer->data.mieScatteringFactor = fog.mieScatteringFactor;
    fogCBuffer->data.noiseScale = fog.noiseScale;
    fogCBuffer->data.timeScale = fog.timeScale;
    fogCBuffer->data.isWindowFog = fog.isWindowFog;
    fogCBuffer->data.fogNear = fog.fogNear;
    fogCBuffer->data.fogFar = fog.fogFar;
    fogCBuffer->data.distanceFogHeightFalloff = fog.distanceFogHeightFalloff;

    fogCBuffer->Activate(immediateContext, 8);

    fogBuffer->Clear(immediateContext, 0, 0, 0, 1);
    fogBuffer->Activate(immediateContext);

    immediateContext->PSSetShaderResources(30, 1, noise2d.GetAddressOf());
    immediateContext->PSSetShaderResources(31, 1, noise3d.GetAddressOf());


    RenderState::BindBlendState(immediateContext, BLEND_STATE::NONE);
    RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
    ID3D11ShaderResourceView* shaderResourceViews[]
    {
        gbufferDepth,       //depthMap
        gBufferPosition,    //worldPosition
        shadowMap,          //cascadedShadowMaps
    };
    fullScreenQuad->Blit(immediateContext, shaderResourceViews, 0, _countof(shaderResourceViews), fogPS.Get());

    fogBuffer->Deactivate(immediateContext);


}

void FogEffect::DrawDebugUI()
{
#ifdef USE_IMGUI
    ImGui::Checkbox("enable", &enabled);
    auto& fog = Scene::GetCurrentScene()->GetSceneSettings().fogConstants;
    ImGui::ColorEdit4("Fog Color", &fog.fogColor.x);
    ImGui::SliderFloat("Intensity", &fog.fogColor.w, 0.0f, 10.0f);
    ImGui::SliderFloat("Density", &fog.fogDensity, 0.0f, 10.0f, "%.6f");
    ImGui::SliderFloat("Height Falloff", &fog.fogHeightFalloff, 0.001f, 1.0f, "%.4f");
    ImGui::SliderFloat("Cutoff Distance", &fog.fogCutoffDistance, 0.0f, 1000.0f);
    ImGui::SliderFloat("Ground Level", &fog.groundLevel, -100.0f, 100.0f);
    ImGui::SliderFloat("Mie Scattering", &fog.mieScatteringFactor, 0.0f, 1.0f, "%.4f");
    ImGui::SliderFloat("Time Scale", &fog.timeScale, 0.0f, 1.0f, "%.4f");
    ImGui::SliderFloat("Noise Scale", &fog.noiseScale, 0.0f, 0.5f, "%.4f");
    ImGui::DragFloat("globalFogIntensity", &fog.globalFogIntensity, 0.001f, 0.0f, 0.5f, "%.4f");
    ImGui::SliderFloat("fogNear", &fog.fogNear, 0.1f, +100.0f);
    ImGui::SliderFloat("fogFar", &fog.fogFar, 0.1f, +500.0f);
    ImGui::SliderFloat("distanceFogHeightFalloff", &fog.distanceFogHeightFalloff, 0.1f, +50.0f);
    CheckboxInt("Enable Dither", &fog.enableDither);
    CheckboxInt("window fog", &fog.isWindowFog);
#endif
}