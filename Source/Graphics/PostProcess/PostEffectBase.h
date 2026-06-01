#pragma once

#include <d3d11.h>
#include <memory>
#include <string>

#include "FrameBuffer.h"

class PostEffectBase
{
public:
    PostEffectBase(std::string className) :name(std::move(className)) {}

    virtual ~PostEffectBase()
    {
    }

    // ポストエフェクト生成（リソース作成） 
    virtual void Initialize(ID3D11Device* device, uint32_t width, uint32_t height) = 0;

    // 描画処理（入力：前フレームのレンダーターゲット）
    virtual void Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* inputSrv) = 0;

    // 出力（次のエフェクトや最終合成に渡す用）
    virtual ID3D11ShaderResourceView* GetOutputSRV()const = 0;

    virtual const std::string& GetName() const { return name; }

    // UI 調整 (ImGui)
    virtual void DrawDebugUI() {}

protected:
    std::unique_ptr<FrameBuffer> outputBuffer;  // 出力先
    std::string name = "";
};


