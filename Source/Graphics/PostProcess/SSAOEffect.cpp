#include "pch.h"
#include "SSAOEffect.h"

#include <DirectXMath.h>
#include <random>

#include "imgui.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Utility/Win32Utils.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/Shader.h"
#include "Graphics/Resource/Texture.h"

void SSAOEffect::Initialize(ID3D11Device* device, uint32_t width, uint32_t height)
{
    fullScreenQuad = std::make_unique<FullScreenQuad>(device);
    ssaoBuffer = std::make_unique<FrameBuffer>(device, width / 2, height / 2, false, DXGI_FORMAT_R32_FLOAT);
    ssaoCBuffer = std::make_unique<ConstantBuffer<SSAOConstantBuffer>>(device);

    HRESULT hr = CreatePsFromCSO(device, "./Data/Shaders/SsaoPS.cso", ssaoPS.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // SSAOカーネルポイント用の構造化バッファを作成する
    std::mt19937 mt(std::random_device{}());
    std::uniform_real_distribution<float> dist_snorm(-1.0f, +1.0f);
    std::uniform_real_distribution<float> dist_unorm(-0.0f, +1.0f);
    std::vector<DirectX::XMFLOAT3> ssao_kernel_points_data(64);
    for (size_t kernel_index = 0; kernel_index < ssao_kernel_points_data.size(); ++kernel_index)
    {
        DirectX::XMFLOAT3 kernel = { dist_snorm(mt), dist_snorm(mt), dist_unorm(mt) };
        float scale = static_cast<float>(kernel_index) / ssao_kernel_points_data.size();
        scale = 0.1f + scale * (1.0f - 0.1f); // lerp
        DirectX::XMStoreFloat3(&kernel, DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&kernel)), dist_unorm(mt) * scale));
        ssao_kernel_points_data.at(kernel_index) = kernel;
    }
    create_structured_buffer_shader_resource_view<DirectX::XMFLOAT3>(device, ssao_kernel_points_data, ssaoKernelPoints.ReleaseAndGetAddressOf());
    // SSAOノイズ用の構造化バッファを作成する
    std::vector<DirectX::XMFLOAT3> ssao_noise_data(16);
    for (DirectX::XMFLOAT3& noise : ssao_noise_data)
    {
        noise = { dist_snorm(mt), dist_snorm(mt), 0.0f };
    }
    create_structured_buffer_shader_resource_view<DirectX::XMFLOAT3>(device, ssao_noise_data, ssaoNoise.ReleaseAndGetAddressOf());
}

void SSAOEffect::Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gbufferColor, ID3D11ShaderResourceView* gbufferNormal, ID3D11ShaderResourceView* gbufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap)
{
    auto& ssao = Scene::GetCurrentScene()->GetSceneSettings().ssaoConstantBuffer;

    ssaoCBuffer->data.radius = ssao.radius;
    ssaoCBuffer->data.power = ssao.power;
    ssaoCBuffer->data.bias = ssao.bias;

    ssaoCBuffer->Activate(immediateContext, 5);

    ssaoBuffer->Clear(immediateContext, 1, 1, 1, 1);    // SSAOは(1.0f,1.0f,1.0f,1.0f)でクリア
    ssaoBuffer->Activate(immediateContext);

    RenderState::BindBlendState(immediateContext, BLEND_STATE::NONE);
    RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
    RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
#if 0
    ID3D11ShaderResourceView* shaderResourceViews[]
    {
        gbufferDepth,       //depthMap
    };
#else
    ID3D11ShaderResourceView* shaderResourceViews[]
    {
        gbufferDepth,       //depthMap
        gbufferNormal,
        ssaoKernelPoints.Get(),
        ssaoNoise.Get()
    };
#endif // 0
    fullScreenQuad->Blit(immediateContext, shaderResourceViews, 0, _countof(shaderResourceViews), ssaoPS.Get());

    ssaoBuffer->Deactivate(immediateContext);

    ID3D11ShaderResourceView* null_shader_resource_views[] =
    {
        NULL, NULL, NULL, NULL
    };
    immediateContext->PSSetShaderResources(0, _countof(null_shader_resource_views), null_shader_resource_views);
}

void SSAOEffect::DrawDebugUI()
{
#ifdef USE_IMGUI
    auto& ssao = Scene::GetCurrentScene()->GetSceneSettings().ssaoConstantBuffer;


    ImGui::Checkbox("enable", &enabled);
    ImGui::SliderFloat("radius", &ssao.radius, 0.0f, +1.0f);
    ImGui::SliderFloat("bias", &ssao.bias, 0.0f, +1.0f);
    ImGui::SliderFloat("power", &ssao.power, 0.0f, +1.0f);
#endif
}

