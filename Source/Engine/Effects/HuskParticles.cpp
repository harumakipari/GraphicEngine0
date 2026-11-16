#include "pch.h"
#include "HuskParticles.h"

#ifdef _DEBUG
#include <limits>
#endif
#include "Graphics/Core/Shader.h"
#include "Engine/Utility/Win32Utils.h"

using namespace DirectX;

HuskParticles::HuskParticles(ID3D11Device* device, size_t maxParticleCount) : maxParticleCount(maxParticleCount)
{
    HRESULT hr{ S_OK };

    D3D11_BUFFER_DESC bufferDesc{};

    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Particle) * maxParticleCount);
    bufferDesc.StructureByteStride = sizeof(Particle);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    hr = device->CreateBuffer(&bufferDesc, NULL, particleBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = device->CreateBuffer(&bufferDesc, NULL, particleBackupBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_BUFFER_DESC particleCountBufferDesc{};
    particleCountBufferDesc.ByteWidth = sizeof(uint32_t);
    particleCountBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    particleCountBufferDesc.BindFlags = 0;
    particleCountBufferDesc.CPUAccessFlags = 0;
    particleCountBufferDesc.MiscFlags = 0;
    particleCountBufferDesc.StructureByteStride = 0;
    hr = device->CreateBuffer(&particleCountBufferDesc, NULL, particleCountBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_BUFFER_DESC particleCountReadbackBufferDesc{};
    particleCountReadbackBufferDesc.ByteWidth = sizeof(uint32_t);
    particleCountReadbackBufferDesc.Usage = D3D11_USAGE_STAGING;
    particleCountReadbackBufferDesc.BindFlags = 0;
    particleCountReadbackBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    particleCountReadbackBufferDesc.MiscFlags = 0;
    particleCountReadbackBufferDesc.StructureByteStride = 0;
    hr = device->CreateBuffer(&particleCountReadbackBufferDesc, NULL, particleCountReadbackBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    shaderResourceViewDesc.Buffer.ElementOffset = 0;
    shaderResourceViewDesc.Buffer.NumElements = static_cast<UINT>(maxParticleCount);
    hr = device->CreateShaderResourceView(particleBuffer.Get(), &shaderResourceViewDesc, particleBufferSrv.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc;
    unorderedAccessViewDesc.Format = DXGI_FORMAT_UNKNOWN;
    unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    unorderedAccessViewDesc.Buffer.FirstElement = 0;
    unorderedAccessViewDesc.Buffer.NumElements = static_cast<UINT>(maxParticleCount);
    unorderedAccessViewDesc.Buffer.Flags = 0;
    hr = device->CreateUnorderedAccessView(particleBuffer.Get(), &unorderedAccessViewDesc, particleBufferUav.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    unorderedAccessViewDesc.Format = DXGI_FORMAT_UNKNOWN;
    unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    unorderedAccessViewDesc.Buffer.FirstElement = 0;
    unorderedAccessViewDesc.Buffer.NumElements = static_cast<UINT>(maxParticleCount);
    unorderedAccessViewDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
    hr = device->CreateUnorderedAccessView(particleBuffer.Get(), &unorderedAccessViewDesc, particleAppendBufferUav.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    particleCBuffer = std::make_unique<ConstantBuffer<ParticleConstants>>(device);

    hr = CreateVsFromCSO(device, "./Data/Shaders/HuskParticlesVS.cso", vertexShader.ReleaseAndGetAddressOf(), nullptr, nullptr, 0);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/HuskParticlesPS.cso", pixelShader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateGsFromCSO(device, "./Data/Shaders/HuskParticlesGS.cso", geometryShader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/HuskParticlesCS.cso", computeShader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/AccumulateHuskParticlePS.cso", accumulateHuskParticlesPs.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

UINT align(UINT num, UINT alignment)
{
    return (num + (alignment - 1)) & ~(alignment - 1);
}

void HuskParticles::Integrate(ID3D11DeviceContext* immediateContext, float deltaTime)
{
    _ASSERT_EXPR(particleData.particleCount <= maxParticleCount, L"");
    if (particleData.particleCount == 0)
    {
        return;
    }

    immediateContext->CSSetUnorderedAccessViews(0, 1, particleBufferUav.GetAddressOf(), nullptr);

    particleData.deltaTime = deltaTime;

    particleCBuffer->data = particleData;
    particleCBuffer->Activate(immediateContext, 12);

    immediateContext->CSSetShader(computeShader.Get(), NULL, 0);

    UINT numThreads = align(particleData.particleCount, 256);
    immediateContext->Dispatch(numThreads / 256, 1, 1);

    ID3D11UnorderedAccessView* nullUnorderedAccessView{};
    immediateContext->CSSetUnorderedAccessViews(0, 1, &nullUnorderedAccessView, nullptr);
}

void HuskParticles::Render(ID3D11DeviceContext* immediateContext)
{
    _ASSERT_EXPR(particleData.particleCount <= maxParticleCount, L"");
    if (particleData.particleCount == 0)
    {
        return;
    }

    immediateContext->VSSetShader(vertexShader.Get(), NULL, 0);
    immediateContext->PSSetShader(pixelShader.Get(), NULL, 0);
    immediateContext->GSSetShader(geometryShader.Get(), NULL, 0);
    immediateContext->GSSetShaderResources(9, 1, particleBufferSrv.GetAddressOf());

    particleCBuffer->data = particleData;
    particleCBuffer->Activate(immediateContext, 12);

    immediateContext->IASetInputLayout(NULL);
    immediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
    immediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    immediateContext->Draw(static_cast<UINT>(particleData.particleCount), 0);

    ID3D11ShaderResourceView* nullShaderResourceView{};
    immediateContext->GSSetShaderResources(9, 1, &nullShaderResourceView);
    immediateContext->VSSetShader(NULL, NULL, 0);
    immediateContext->PSSetShader(NULL, NULL, 0);
    immediateContext->GSSetShader(NULL, NULL, 0);
}
void HuskParticles::BackupParticles(ID3D11DeviceContext* immediateContext)
{
    immediateContext->CopyResource(particleBackupBuffer.Get(), particleBuffer.Get());
}

void HuskParticles::RestoreParticles(ID3D11DeviceContext* immediateContext)
{
    immediateContext->CopyResource(particleBuffer.Get(), particleBackupBuffer.Get());
}

void HuskParticles::AccumulateHuskParticles(ID3D11DeviceContext* immediateContext, std::function<void(ID3D11PixelShader*)> drawcallback, ID3D11ShaderResourceView* sceneColor, ID3D11ShaderResourceView* sceneDepth)
{
    HRESULT hr{ S_OK };

    UINT viewportCount{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };
    D3D11_VIEWPORT cachedViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
    immediateContext->RSGetViewports(&viewportCount, cachedViewports);

    if (viewportCount > 0)
    {
        const float resolutionRatio = 1.0f;
        D3D11_VIEWPORT viewport;
        viewport.Width = cachedViewports[0].Width * resolutionRatio;
        viewport.Height = cachedViewports[0].Height * resolutionRatio;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        immediateContext->RSSetViewports(1, &viewport);
    }

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cachedRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cachedDepthStencilView;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> cachedUnorderedAccessView;
    immediateContext->OMGetRenderTargetsAndUnorderedAccessViews(1, cachedRenderTargetView.GetAddressOf(), cachedDepthStencilView.GetAddressOf(), 1, 1, cachedUnorderedAccessView.GetAddressOf());

    ID3D11RenderTargetView* nullRenderTargetView{};
    UINT initialCount{ 0 };
    immediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &nullRenderTargetView, NULL, 1, 1, particleAppendBufferUav.GetAddressOf(), &initialCount);

    ID3D11ShaderResourceView* captureViews[] = { sceneColor, sceneDepth };
    immediateContext->PSSetShaderResources(24, 2, captureViews);
    particleCBuffer->data = particleData;
    particleCBuffer->data.sceneColorCaptureReady = sceneColor && sceneDepth ? 1.0f : 0.0f;
    particleCBuffer->Activate(immediateContext, 12);
    drawcallback(accumulateHuskParticlesPs.Get());

    ID3D11ShaderResourceView* nullCaptureViews[2] = {};
    immediateContext->PSSetShaderResources(24, 2, nullCaptureViews);

    immediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, cachedRenderTargetView.GetAddressOf(), cachedDepthStencilView.Get(), 1, 1, cachedUnorderedAccessView.GetAddressOf(), NULL);

    if (!particleCountBuffer || !particleCountReadbackBuffer || !particleAppendBufferUav)
    {
        if (viewportCount > 0)
            immediateContext->RSSetViewports(viewportCount, cachedViewports);
        return;
    }

    immediateContext->CopyStructureCount(particleCountBuffer.Get(), 0, particleAppendBufferUav.Get());
    immediateContext->CopyResource(particleCountReadbackBuffer.Get(), particleCountBuffer.Get());
    D3D11_MAPPED_SUBRESOURCE mappedSubresource{};
    hr = immediateContext->Map(particleCountReadbackBuffer.Get(), 0, D3D11_MAP_READ, 0, &mappedSubresource);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    if (FAILED(hr))
    {
        if (viewportCount > 0)
            immediateContext->RSSetViewports(viewportCount, cachedViewports);
        return;
    }
    UINT count{ *reinterpret_cast<UINT*>(mappedSubresource.pData) };
    immediateContext->Unmap(particleCountReadbackBuffer.Get(), 0);

    particleData.particleCount = count;
#ifdef _DEBUG
    MeasureCapturedWorldX(immediateContext, count);
#endif

    if (viewportCount > 0)
        immediateContext->RSSetViewports(viewportCount, cachedViewports);
}

#ifdef _DEBUG
void HuskParticles::MeasureCapturedWorldX(ID3D11DeviceContext* context, UINT count)
{
    capturedX = {};
    capturedX.attempted = true;
    capturedX.particleCount = count;
    if (count == 0) return;
    if (count > maxParticleCount)
    {
        capturedX.result = E_INVALIDARG;
        return;
    }

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>(sizeof(Particle) * count);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Buffer> readback;
    capturedX.result = device->CreateBuffer(&desc, nullptr, readback.GetAddressOf());
    if (FAILED(capturedX.result)) return;

    const D3D11_BOX region{ 0, 0, 0, desc.ByteWidth, 1, 1 };
    context->CopySubresourceRegion(readback.Get(), 0, 0, 0, 0, particleBuffer.Get(), 0, &region);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    capturedX.result = context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(capturedX.result)) return;

    const auto* particles = static_cast<const Particle*>(mapped.pData);
    float minimum = (std::numeric_limits<float>::max)();
    float maximum = (std::numeric_limits<float>::lowest)();
    for (UINT i = 0; i < count; ++i)
    {
        const float x = particles[i].position.x;
        if (!std::isfinite(x))
        {
            ++capturedX.nonFiniteCount;
            continue;
        }
        minimum = (std::min)(minimum, x);
        maximum = (std::max)(maximum, x);
    }
    context->Unmap(readback.Get(), 0);

    capturedX.valid = capturedX.nonFiniteCount < count;
    if (capturedX.valid)
    {
        capturedX.minimum = minimum;
        capturedX.maximum = maximum;
    }
}
#endif
