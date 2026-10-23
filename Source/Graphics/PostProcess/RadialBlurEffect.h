#pragma once

#include <memory>

#include <DirectXMath.h>

#include "Graphics/Core/ConstantBuffer.h"
#include "Graphics/PostProcess/FrameBuffer.h"
#include "Graphics/PostProcess/FullScreenQuad.h"

class RadialBlurEffect
{
public:
    struct RadialBlurConstantBuffer
    {
        DirectX::XMFLOAT2 blurCenter{ 0.5f, 0.5f };
        float blurRadiusPixels = 45.0f;
        float maskRadiusPixels = 300.0f;

        float blurIntensity = 0.8f;
        int samplingCount = 8;
        DirectX::XMFLOAT2 screenSize{};
    };
    static_assert(sizeof(RadialBlurConstantBuffer) % 16 == 0, "Radial blur constants must be 16-byte aligned.");

    void Initialize(ID3D11Device* device, uint32_t width, uint32_t height);
    void Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* sceneColor);

    bool IsEnabled() const { return manualPreviewEnabled || radialActive; }
    void SetRadialActive(bool active) { radialActive = active; }
    ID3D11ShaderResourceView* GetOutputSRV() const;
    void DrawDebugUI();

private:
    bool manualPreviewEnabled = false;
    bool radialActive = false;
    uint32_t width = 0;
    uint32_t height = 0;

    std::unique_ptr<FrameBuffer> outputBuffer;
    std::unique_ptr<FullScreenQuad> fullScreenQuad;
    std::unique_ptr<ConstantBuffer<RadialBlurConstantBuffer>> radialBlurCBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> radialBlurPs;
};
