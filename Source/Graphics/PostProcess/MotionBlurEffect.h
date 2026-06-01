#pragma once

#include <memory>

#include "FrameBuffer.h"
#include "SceneEffectBase.h"
#include "FullScreenQuad.h"
#include "Engine/Scene/SceneSetting.h"
#include "Graphics/Core/ConstantBuffer.h"

class MotionBlurEffect :public SceneEffectBase
{
public:
    struct MotionBlurConstantBuffer
    {
        int motion_blur_iteration = 5;
        float motion_blur_jitter = 1.0f;
        float motion_blur_exposure_time = 1000;
        float motion_blur_fps_rate = 1.0f / 60.0f;
    };

public:
    MotionBlurEffect() :SceneEffectBase("MotionBlurEffect") {}
    ~MotionBlurEffect() = default;

    // ポストエフェクト生成（リソース作成） 
    void Initialize(ID3D11Device* device, uint32_t width, uint32_t height) override;

    // 描画処理（入力：前フレームのレンダーターゲット）
    void Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gBufferColor, ID3D11ShaderResourceView* gBufferNormal,
        ID3D11ShaderResourceView* gBufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap)override;

    // 出力（次のエフェクトや最終合成に渡す用）
    ID3D11ShaderResourceView* GetOutputSRV()const override
    {
        return frameBuffer->shaderResourceViews[0].Get();
    }

    // UI 調整 (ImGui)
    void DrawDebugUI()override;

private:
    // 速度バッファを再構築する
    void ReconstractVelocityBuffer(float deltaTime, ID3D11DeviceContext* immediateContext);

    Microsoft::WRL::ComPtr<ID3D11PixelShader> motion_blur_pixel_shader;
    //  速度バッファをタイル化する目的で使用するブロックサイズ
    //  ここはシェーダー側と同期しておく必要がある
    static const int TileSize = 20;

    //	タイル毎の速度を求めてモーションブラー処理
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> tile_max_render_target_view;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tile_max_shader_resource_view;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> neighbor_max_render_target_view;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> neighbor_max_shader_resource_view;

    //	再構築
    Microsoft::WRL::ComPtr<ID3D11PixelShader> reconstract_velocity_buffer_ps[2];
    std::unique_ptr<ConstantBuffer<MotionBlurConstantBuffer>> motionBlurCBuffer;

    std::unique_ptr<FrameBuffer> frameBuffer;
    std::unique_ptr<FullScreenQuad> fullScreenQuad;
};

