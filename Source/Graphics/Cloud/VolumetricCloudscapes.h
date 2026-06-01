#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include "Graphics/PostProcess/FullScreenQuad.h"

#define HIGH_FREQ_WORLEY_DIMENSIONS 32
#define HIGH_FREQ_WORLEY_NUMTHREADS 8
#define LOW_FREQ_PERLIN_WORLEY_DIMENSIONS 128
#define LOW_FREQ_PERLIN_WORLEY_NUMTHREADS 8

struct VolumetricCloudscapesConstants
{
    DirectX::XMFLOAT2 windDirection{ 1.0f,0.0f };
    DirectX::XMFLOAT2 cloudAltitudesMinMax{ 6001500.0f,6004000.0f };    // highest and lowest altitudes at which clouds are distributed

    float windSpeed = 1.0f; // [0.0, 20.0]

    float densityScale = 0.05f; // [0.01,0.2]
    float cloudCoverageScale = 0.2f;  // [0.1,1.0]
    float rainCloudAbsorptionScale = 0.0f;
    float cloudTypeScale = 1.0f;

    float earthRadius = 6000000.0f; // earth radius
    float horizonDistanceScale = 1.0f;
    float lowFrequencyPerlinWorleySamplingScale = 0.00008f;
    float highFrequencyWorleySamplingScale = 0.001f;
    float cloudDensityLongDistanceScale = 18.0;
    int enablePowderedSugarEffect = false;

    int rayMarchingSteps = 128;
    int autoRayMarchingSteps = false;
};

class VolumetricCloudscapes
{
public:
    VolumetricCloudscapes(ID3D11Device* device, const wchar_t* filename/*weather map*/);
    VolumetricCloudscapes(const VolumetricCloudscapes& rhs) = delete;
    VolumetricCloudscapes& operator=(const VolumetricCloudscapes& rhs) = delete;
    VolumetricCloudscapes(VolumetricCloudscapes&&)noexcept = delete;
    VolumetricCloudscapes& operator=(VolumetricCloudscapes&&) noexcept = delete;
    virtual ~VolumetricCloudscapes() = default;

    static void CreateNoiseTexture(ID3D11Device* device);

    void Blit(ID3D11DeviceContext* immediateContext);

    VolumetricCloudscapesConstants constantData;

private:
    Microsoft::WRL::ComPtr<ID3D11PixelShader> volumetricCloudscapesPS;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> lowFreqPerlinWorleyShaderResourceView;
    
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> highFreqWorleyShaderResourceView;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> weatherShaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> curlNoiseShaderResourceView;

#if 0
    // density-height functions
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gradientCumulonimbusShaderResouceView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gradientCumulusShaderResouceView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gradientStratusShaderResouceView;
#endif

    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;

    std::unique_ptr<FullScreenQuad> fullscreenQuad;

};
