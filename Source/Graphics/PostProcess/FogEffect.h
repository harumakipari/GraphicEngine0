#pragma once

#include <memory>

#include "FrameBuffer.h"
#include "SceneEffectBase.h"
#include "FullScreenQuad.h"
#include "Engine/Scene/SceneSetting.h"
#include "Graphics/Core/ConstantBuffer.h"

class FogEffect :public SceneEffectBase
{

public:
    FogEffect() :SceneEffectBase("FogEffect") {}
    ~FogEffect() = default;

    // ポストエフェクト生成（リソース作成） 
    void Initialize(ID3D11Device* device, uint32_t width, uint32_t height) override;

    void Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gBufferColor, ID3D11ShaderResourceView* gbufferNormal,
        ID3D11ShaderResourceView* gbufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap) override;

    // 出力（次のエフェクトや最終合成に渡す用）
    ID3D11ShaderResourceView* GetOutputSRV()const override
    {
        return fogBuffer->shaderResourceViews[0].Get();
    }

    // UI 調整 (ImGui)
    void DrawDebugUI()override;

private:
    std::unique_ptr<ConstantBuffer<FogConstants>> fogCBuffer;
    std::unique_ptr<FrameBuffer> fogBuffer;
    std::unique_ptr<FullScreenQuad> fullScreenQuad;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> fogPS;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noise2d;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noise3d;

};
