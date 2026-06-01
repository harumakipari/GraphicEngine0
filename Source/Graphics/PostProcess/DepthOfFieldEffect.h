#pragma once

#include "FrameBuffer.h"
#include "FullScreenQuad.h"
#include "SceneEffectBase.h"
#include "Graphics/Core/ConstantBuffer.h"

class DepthOfFieldEffect :public SceneEffectBase
{

public:
    DepthOfFieldEffect() :SceneEffectBase("DepthOfFieldEffect") {}

    ~DepthOfFieldEffect() = default;

    // ポストエフェクト生成（リソース作成） 
    void Initialize(ID3D11Device* device, uint32_t width, uint32_t height) override;

    void Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gBufferColor, ID3D11ShaderResourceView* gBufferNormal,
        ID3D11ShaderResourceView* gBufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap) override;

    // 出力（次のエフェクトや最終合成に渡す用） 
    ID3D11ShaderResourceView* GetOutputSRV()const override
    {
        return depthOfFieldBuffer->shaderResourceViews[0].Get();
    }

    // UI 調整 (ImGui)
    void DrawDebugUI()override;

private:
    std::unique_ptr<FrameBuffer> depthOfFieldBuffer;
    std::unique_ptr<FrameBuffer> finalBokehBuffer;
    std::unique_ptr<FullScreenQuad> fullScreenQuad;
    static constexpr size_t downsampledCount = 6;
    std::unique_ptr<FrameBuffer> gaussianBlur[downsampledCount][2];
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;

    Microsoft::WRL::ComPtr<ID3D11PixelShader> bokehPS; // 被写界深度のエフェクトをかけるピクセルシェーダー
    Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussianBlurHorizontalPs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussianBlurVerticalPs;
};
