#include "pch.h"
#include "MotionBlurEffect.h"


#include "imgui.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Scene/Scene.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/Shader.h"
#include "Graphics/Resource/Texture.h"

void MotionBlurEffect::Initialize(ID3D11Device* device, uint32_t width, uint32_t height)
{
    //	motion blur用定数バッファ
    motionBlurCBuffer = std::make_unique<ConstantBuffer<MotionBlurConstantBuffer>>(device);
    fullScreenQuad = std::make_unique<FullScreenQuad>(device);
    frameBuffer = std::make_unique<FrameBuffer>(device, width / 2, height / 2, false, DXGI_FORMAT_R16_FLOAT);
    //	モーションブラーシェーダー
    HRESULT hr = CreatePsFromCSO(device, "./Data/Shaders/MotionBlurPS.cso", motion_blur_pixel_shader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/VelocityTileMaxPS.cso", reconstract_velocity_buffer_ps[0].GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/VelocityNeighborMaxPS.cso", reconstract_velocity_buffer_ps[1].GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    //	ブラー用速度バッファを生成
    {
        D3D11_TEXTURE2D_DESC texture2d_desc{};
        texture2d_desc.Width = (SCREEN_WIDTH + TileSize - 1) / TileSize;
        texture2d_desc.Height = (SCREEN_HEIGHT + TileSize - 1) / TileSize;
        texture2d_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        texture2d_desc.MipLevels = 1;
        texture2d_desc.ArraySize = 1;
        texture2d_desc.SampleDesc.Count = 1;
        texture2d_desc.SampleDesc.Quality = 0;
        texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
        texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texture2d_desc.CPUAccessFlags = 0;
        texture2d_desc.MiscFlags = 0;

        //	タイル最大速度記録バッファ
        {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> color_buffer{};
            hr = device->CreateTexture2D(&texture2d_desc, NULL, color_buffer.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            hr = device->CreateRenderTargetView(color_buffer.Get(), NULL, tile_max_render_target_view.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            hr = device->CreateShaderResourceView(color_buffer.Get(), NULL, tile_max_shader_resource_view.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        }
        //	近傍最大速度記録バッファ
        {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> color_buffer{};
            hr = device->CreateTexture2D(&texture2d_desc, NULL, color_buffer.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            hr = device->CreateRenderTargetView(color_buffer.Get(), NULL, neighbor_max_render_target_view.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            hr = device->CreateShaderResourceView(color_buffer.Get(), NULL, neighbor_max_shader_resource_view.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        }
    }
}

void MotionBlurEffect::Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gBufferColor, ID3D11ShaderResourceView* gbufferNormal, ID3D11ShaderResourceView* gbufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap)
{
    //frameBuffer->Clear(immediateContext, 0, 0, 0, 1);
    //frameBuffer->Activate(immediateContext);

    ID3D11ShaderResourceView* shaderResourceViews[]
    {
        gbufferDepth,       //gbufferVelocity
        tile_max_shader_resource_view.Get(),
    };


    if (motionBlurCBuffer->data.motion_blur_iteration <= 0)
        return;

    //	ビューポートを退避
    UINT reserve_num_viewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_VIEWPORT reserve_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    immediateContext->RSGetViewports(&reserve_num_viewports, reserve_viewports);
    //	ビューポート設定
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(SCREEN_WIDTH / TileSize);
    viewport.Height = static_cast<float>(SCREEN_HEIGHT / TileSize);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    immediateContext->RSSetViewports(1, &viewport);

    //	速度バッファ内の速度をタイル化する
    {
        // シェーダーリソースにGBufferを設定


        //	出力先をタイルバッファに変更
        {
            FLOAT clear_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
            immediateContext->ClearRenderTargetView(tile_max_render_target_view.Get(), clear_color);
            immediateContext->OMSetRenderTargets(1, tile_max_render_target_view.GetAddressOf(), nullptr);
        }
        {
            RenderState::BindBlendState(immediateContext, BLEND_STATE::NONE);
            RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
            RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);

            fullScreenQuad->Blit(immediateContext, shaderResourceViews, 0, _countof(shaderResourceViews), reconstract_velocity_buffer_ps[0].Get());
        }
    }

    //	タイルバッファ内に記録した速度から、周辺を調べて最大速度を記録
    {
        //	出力先を近傍バッファに変更
        {
            FLOAT clear_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
            immediateContext->ClearRenderTargetView(neighbor_max_render_target_view.Get(), clear_color);
            immediateContext->OMSetRenderTargets(1, neighbor_max_render_target_view.GetAddressOf(), nullptr);
        }
        {
            static constexpr int TileMaxVelocitySRVIndex = 1;
            immediateContext->PSSetShaderResources(TileMaxVelocitySRVIndex, 1, tile_max_shader_resource_view.GetAddressOf());

            RenderState::BindBlendState(immediateContext, BLEND_STATE::NONE);
            RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_OFF_ZW_OFF);
            RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
            fullScreenQuad->Blit(immediateContext, shaderResourceViews, 0, _countof(shaderResourceViews), reconstract_velocity_buffer_ps[2].Get());
        }
    }
    //	退避した物を再設定
    immediateContext->RSSetViewports(reserve_num_viewports, reserve_viewports);

    //frameBuffer->Deactivate(immediateContext);
}

// 速度バッファを再構築する
void MotionBlurEffect::ReconstractVelocityBuffer(float deltaTime, ID3D11DeviceContext* immediateContext)
{

}

void MotionBlurEffect::DrawDebugUI()
{
#ifdef USE_IMGUI
#endif
}

