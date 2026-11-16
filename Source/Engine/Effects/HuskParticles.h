#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include "Graphics/Core/ConstantBuffer.h"

struct HuskParticles
{
    size_t maxParticleCount;
    struct Particle
    {
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT3 velocity;
        float age{};
        int state{};
        float normalizedX{};
    };

    struct ParticleConstants
    {
        int particleCount{};
        float particleSize{ 0.008f };
        float particleOption{};
        float deltaTime{};
        float worldXMin{ 5.585f };
        float worldXMax{ 12.519f };
        float deathProgress{};
        float detachSpeed{ 0.35f };
        float gravity{ -9.8f };
        float lifetime{ 1.5f };
        float riseSpeed{ 0.5f };
        float maxStartDelay{ 0.28f };
        float riseSpeedMinMultiplier{ 0.6f };
        float riseSpeedMaxMultiplier{ 1.4f };
        float horizontalRandomSpeed{ 0.08f };
        float fadeStartRatio{ 0.75f };
        float lifetimeMinMultiplier{ 0.85f };
        float lifetimeMaxMultiplier{ 1.15f };
        float displayRatio{ 0.75f };
        float debugNormalizedX{}; 
        float boundaryWidth{ 0.01f };
        float boundaryEmissiveStrength{ 4.0f };
        float detachGlowDuration{ 0.2f };
        float detachGlowStrength{ 0.2f };
        DirectX::XMFLOAT3 bodyColorMultiplier{ 1.0f, 1.0f, 1.0f };
        float bodyBrightness{ 1.0f };
        float useSceneColorCapture{ 1.0f };
        float sceneColorDepthThreshold{ 0.000001f }; 
        float sceneColorCaptureReady{}; 
        float capturePadding{};
    };

#ifdef _DEBUG
    struct CaptureXMeasurement
    {
        bool attempted = false;
        bool valid = false;
        HRESULT result = S_OK;
        UINT particleCount = 0;
        UINT nonFiniteCount = 0;
        float minimum = 0.0f;
        float maximum = 0.0f;
    };
    CaptureXMeasurement capturedX;

    void MeasureCapturedWorldX(ID3D11DeviceContext* context, UINT count);
#endif
    ParticleConstants particleData;
    std::unique_ptr<ConstantBuffer<ParticleConstants>> particleCBuffer;

    Microsoft::WRL::ComPtr<ID3D11Buffer> particleBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> particleBackupBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleBufferUav;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleAppendBufferUav;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleBufferSrv;

    Microsoft::WRL::ComPtr<ID3D11Buffer> particleCountBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> particleCountReadbackBuffer;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometryShader;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> computeShader;

    HuskParticles(ID3D11Device* pDevice, size_t maxParticleCount = 3000000);
    HuskParticles(const HuskParticles&) = delete;
    HuskParticles& operator=(const HuskParticles&) = delete;
    HuskParticles(HuskParticles&&) noexcept = delete;
    HuskParticles& operator=(HuskParticles&&) noexcept = delete;
    virtual ~HuskParticles() = default;

    void Integrate(ID3D11DeviceContext* immediateContext, float deltaTime);
    void Render(ID3D11DeviceContext* immediateContext);
    void BackupParticles(ID3D11DeviceContext* immediateContext);
    void RestoreParticles(ID3D11DeviceContext* immediateContext);

    Microsoft::WRL::ComPtr<ID3D11PixelShader> accumulateHuskParticlesPs;
    void AccumulateHuskParticles(ID3D11DeviceContext* immediateContext, std::function<void(ID3D11PixelShader*)> drawcallback, ID3D11ShaderResourceView* sceneColor, ID3D11ShaderResourceView* sceneDepth);
};
