#pragma once

#include "FrameBuffer.h"
#include "FullScreenQuad.h"
#include "SceneEffectBase.h"
#include "Engine/Scene/SceneSetting.h"
#include "Graphics/Core/ConstantBuffer.h"

class SSAOEffect :public SceneEffectBase
{
public:
    SSAOEffect() :SceneEffectBase("SSAOEffect") {}

    ~SSAOEffect() = default;

    // ポストエフェクト生成（リソース作成） 
    void Initialize(ID3D11Device* device, uint32_t width, uint32_t height) override;

    void Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gbufferColor, ID3D11ShaderResourceView* gbufferNormal,
        ID3D11ShaderResourceView* gbufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap) override;

    // 出力（次のエフェクトや最終合成に渡す用）
    ID3D11ShaderResourceView* GetOutputSRV()const override
    {
        return ssaoBuffer->shaderResourceViews[0].Get();
    }

    // UI 調整 (ImGui)
    void DrawDebugUI()override;

private:
    std::unique_ptr<FrameBuffer> ssaoBuffer;
    std::unique_ptr<FullScreenQuad> fullScreenQuad;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ssaoPS;
    std::unique_ptr<ConstantBuffer<SSAOConstantBuffer>> ssaoCBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ssaoKernelPoints;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ssaoNoise;


};
