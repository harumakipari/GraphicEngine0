#pragma once

#include <memory>

#include "FrameBuffer.h"
#include "SceneEffectBase.h"
#include "FullScreenQuad.h"
#include "Engine/Scene/SceneSetting.h"
#include "Graphics/Core/ConstantBuffer.h"

class BloomEffect :public SceneEffectBase
{
public:
    BloomEffect() :SceneEffectBase("BloomEffect") {}
    ~BloomEffect() = default;

    // ポストエフェクト生成（リソース作成） 
    void Initialize(ID3D11Device* device, uint32_t width, uint32_t height) override;

    // 描画処理（入力：前フレームのレンダーターゲット）
    void Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gBufferColor, ID3D11ShaderResourceView* gBufferNormal,
        ID3D11ShaderResourceView* gBufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap)override;

    // 出力（次のエフェクトや最終合成に渡す用）
    ID3D11ShaderResourceView* GetOutputSRV()const override
    {
        return glowExtraction->shaderResourceViews[0].Get();
    }

    // UI 調整 (ImGui)
    void DrawDebugUI()override;


private:
    std::unique_ptr<FullScreenQuad> bitBlockTransfer;
    std::unique_ptr<FrameBuffer> glowExtraction;

    static constexpr size_t downsampledCount = 6;
    std::unique_ptr<FrameBuffer> gaussianBlur[downsampledCount][2];

    Microsoft::WRL::ComPtr<ID3D11PixelShader> glowExtractionPs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussianBlurDownsamplingPs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussianBlurHorizontalPs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussianBlurVerticalPs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussianBlurUpsamplingPs;

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;


    std::unique_ptr<ConstantBuffer<BloomConstantBuffer>> bloomConstant;
};

