#include "pch.h"
#include "VolumetricCloudscapes.h"

#include <filesystem>
#include <DirectXTex.h>

#include "Graphics/Core/Shader.h"
#include "Graphics/Resource/Texture.h"
#include "Engine/Utility/Win32Utils.h"

void VolumetricCloudscapes::CreateNoiseTexture(ID3D11Device* device)
{
    HRESULT hr{ S_OK };

    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext;
    device->GetImmediateContext(immediateContext.GetAddressOf());

    const wchar_t* lowFreqPerlinWorleyFilename = L"volumetricCloudscapesTextures/low_freq_perlin_worley.dds";

    {
        Microsoft::WRL::ComPtr<ID3D11Texture3D> lowFreqPerlinWorleyTexture3d;
        D3D11_TEXTURE3D_DESC texture3dDesc{};
        texture3dDesc.Width = LOW_FREQ_PERLIN_WORLEY_DIMENSIONS;
        texture3dDesc.Height = LOW_FREQ_PERLIN_WORLEY_DIMENSIONS;
        texture3dDesc.Depth = LOW_FREQ_PERLIN_WORLEY_DIMENSIONS;
        texture3dDesc.MipLevels = 0;
        texture3dDesc.Format = format;
        texture3dDesc.Usage = D3D11_USAGE_DEFAULT;
        texture3dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
        texture3dDesc.CPUAccessFlags = 0;
        texture3dDesc.MiscFlags = 0;
        texture3dDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        hr = device->CreateTexture3D(&texture3dDesc, NULL, lowFreqPerlinWorleyTexture3d.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> lowFreqPerlinWorleyShaderResourceView;
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
        shaderResourceViewDesc.Format = texture3dDesc.Format;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        shaderResourceViewDesc.Texture3D.MipLevels = -1;
        shaderResourceViewDesc.Texture3D.MostDetailedMip = 0;
        hr = device->CreateShaderResourceView(lowFreqPerlinWorleyTexture3d.Get(), &shaderResourceViewDesc, lowFreqPerlinWorleyShaderResourceView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> lowFreqPerlinWorleyUnorderedAccessView;
        D3D11_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
        unorderedAccessViewDesc.Format = texture3dDesc.Format;
        unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
        unorderedAccessViewDesc.Texture3D.MipSlice = 0;
        unorderedAccessViewDesc.Texture3D.FirstWSlice = 0;
        unorderedAccessViewDesc.Texture3D.WSize = -1;
        hr = device->CreateUnorderedAccessView(lowFreqPerlinWorleyTexture3d.Get(), &unorderedAccessViewDesc, lowFreqPerlinWorleyUnorderedAccessView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID3D11ComputeShader> lowFreqPerlinWorleyCS;
        hr=CreateCsFromCSO(device, "./Data/Shaders/LowFreqPerlinWorleyCS.cso", lowFreqPerlinWorleyCS.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        immediateContext->CSSetUnorderedAccessViews(0, 1, lowFreqPerlinWorleyUnorderedAccessView.GetAddressOf(), NULL);
        immediateContext->CSSetShader(lowFreqPerlinWorleyCS.Get(), NULL, 0);
        UINT threadGroupCount = LOW_FREQ_PERLIN_WORLEY_DIMENSIONS / LOW_FREQ_PERLIN_WORLEY_NUMTHREADS;
        immediateContext->Dispatch(threadGroupCount, threadGroupCount, threadGroupCount);

        ID3D11UnorderedAccessView* nullUnorderedAccessView = NULL;
        immediateContext->CSSetUnorderedAccessViews(0, 1, &nullUnorderedAccessView, NULL);
        immediateContext->CSSetShader(NULL, NULL, 0);

        immediateContext->GenerateMips(lowFreqPerlinWorleyShaderResourceView.Get());
        DirectX::ScratchImage image;
        hr = CaptureTexture(device, immediateContext.Get(), lowFreqPerlinWorleyTexture3d.Get(), image);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::DDS_FLAGS_NONE, lowFreqPerlinWorleyFilename);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    const wchar_t* highFreqWorleyFilename = L"volumetricCloudscapesTextures/high_freq_worley.dds";

    {
        Microsoft::WRL::ComPtr<ID3D11Texture3D> highFreqWorleyTexture3D;
        D3D11_TEXTURE3D_DESC texture3dDesc{};
        texture3dDesc.Width = HIGH_FREQ_WORLEY_DIMENSIONS;
        texture3dDesc.Height = HIGH_FREQ_WORLEY_DIMENSIONS;
        texture3dDesc.Depth = HIGH_FREQ_WORLEY_DIMENSIONS;
        texture3dDesc.MipLevels = 0;
        texture3dDesc.Format = format;
        texture3dDesc.Usage = D3D11_USAGE_DEFAULT;
        texture3dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
        texture3dDesc.CPUAccessFlags = 0;
        texture3dDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        hr = device->CreateTexture3D(&texture3dDesc, NULL, highFreqWorleyTexture3D.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> highFreqWorleyShaderResourceView;
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
        shaderResourceViewDesc.Format = texture3dDesc.Format;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        shaderResourceViewDesc.Texture3D.MipLevels = -1;
        shaderResourceViewDesc.Texture3D.MostDetailedMip = 0;
        hr = device->CreateShaderResourceView(highFreqWorleyTexture3D.Get(), &shaderResourceViewDesc, highFreqWorleyShaderResourceView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> highFreqWorleyUnorderdAccessView;
        D3D11_UNORDERED_ACCESS_VIEW_DESC unorderdAccessViewDesc{};
        unorderdAccessViewDesc.Format = texture3dDesc.Format;
        unorderdAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
        unorderdAccessViewDesc.Texture3D.MipSlice = 0;
        unorderdAccessViewDesc.Texture3D.FirstWSlice = 0;
        unorderdAccessViewDesc.Texture3D.WSize = -1;
        hr = device->CreateUnorderedAccessView(highFreqWorleyTexture3D.Get(), &unorderdAccessViewDesc, highFreqWorleyUnorderdAccessView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        Microsoft::WRL::ComPtr<ID3D11ComputeShader> highFreqWorleyCS;
        hr=CreateCsFromCSO(device, "./Data/Shaders/LowFreqPerlinWorleyCS.cso", highFreqWorleyCS.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        immediateContext->CSSetUnorderedAccessViews(0, 1, highFreqWorleyUnorderdAccessView.GetAddressOf(), NULL);
        immediateContext->CSSetShader(highFreqWorleyCS.Get(), NULL, 0);
        UINT threadGroupCount = HIGH_FREQ_WORLEY_DIMENSIONS / HIGH_FREQ_WORLEY_NUMTHREADS;
        immediateContext->Dispatch(threadGroupCount, threadGroupCount, threadGroupCount);

        ID3D11UnorderedAccessView* nullUnorderedAccessView = NULL;
        immediateContext->CSSetUnorderedAccessViews(0, 1, &nullUnorderedAccessView, NULL);
        immediateContext->CSSetShader(NULL, NULL, 0);

        immediateContext->GenerateMips(highFreqWorleyShaderResourceView.Get());

        DirectX::ScratchImage image;
        hr = CaptureTexture(device, immediateContext.Get(), highFreqWorleyTexture3D.Get(), image);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::DDS_FLAGS_NONE, highFreqWorleyFilename);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
}

VolumetricCloudscapes::VolumetricCloudscapes(ID3D11Device* device, const wchar_t* filename/*weather map*/)
{
    HRESULT hr{ S_OK };

    const wchar_t* lowFreqPerlinWorleyFilename = L"./Assets/Environment/VolumetricCloudscapes/low_freq_perlin_worley.dds";
    _ASSERT_EXPR(std::filesystem::exists(lowFreqPerlinWorleyFilename), L"");
    {
        DirectX::TexMetadata metadata;
        DirectX::ScratchImage scratchImage;
        hr = DirectX::LoadFromDDSFile(lowFreqPerlinWorleyFilename, DirectX::DDS_FLAGS::DDS_FLAGS_NONE, &metadata, scratchImage);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        const DirectX::Image* images = scratchImage.GetImages();
        size_t numImages = scratchImage.GetImageCount();
        hr = DirectX::CreateShaderResourceViewEx(device, images, numImages, metadata, D3D11_USAGE_DEFAULT,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET, 0, 0, DirectX::CREATETEX_DEFAULT, lowFreqPerlinWorleyShaderResourceView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    const wchar_t* highFreqPerlinWorleyFilename = L"./Assets/Environment/VolumetricCloudscapes/high_freq_worley.dds";
    _ASSERT_EXPR(std::filesystem::exists(highFreqPerlinWorleyFilename), L"");
    {
        DirectX::TexMetadata metadata;
        DirectX::ScratchImage scratchImage;
        hr = DirectX::LoadFromDDSFile(highFreqPerlinWorleyFilename, DirectX::DDS_FLAGS::DDS_FLAGS_NONE, &metadata, scratchImage);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        const DirectX::Image* images = scratchImage.GetImages();
        size_t numImages = scratchImage.GetImageCount();
        hr = DirectX::CreateShaderResourceViewEx(device, images, numImages, metadata, D3D11_USAGE_DEFAULT,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET, 0, 0, DirectX::CREATETEX_DEFAULT, highFreqWorleyShaderResourceView.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    hr=LoadTextureFromFile(device, filename, weatherShaderResourceView.GetAddressOf(), NULL);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr=LoadTextureFromFile(device, L"./Assets/Environment/VolumetricCloudscapes/curl_noise.png", curlNoiseShaderResourceView.GetAddressOf(), NULL);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#if 0
    hr=LoadTextureFromFile(device, L"./Assets/Environment/VolumetricCloudscapes/gradient_stratus.png", gradientStratusShaderResouceView.GetAddressOf(), NULL);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr=LoadTextureFromFile(device, L"./Assets/Environment/VolumetricCloudscapes/gradient_cumulus.png", gradientCumulusShaderResouceView.GetAddressOf(), NULL);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr=LoadTextureFromFile(device, L"./Assets/Environment/VolumetricCloudscapes/gradient_cumulonimbus.png", gradientCumulonimbusShaderResouceView.GetAddressOf(), NULL);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif
    hr=CreatePsFromCSO(device, "./Data/Shaders/VolumetricCloudscapesPS.cso", volumetricCloudscapesPS.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    fullscreenQuad = std::make_unique<decltype(fullscreenQuad)::element_type>(device);

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = (sizeof(VolumetricCloudscapesConstants) + 0x0f) & ~0x0f;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&bufferDesc, NULL, constantBuffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void VolumetricCloudscapes::Blit(ID3D11DeviceContext* immediateContext)
{
    immediateContext->UpdateSubresource(constantBuffer.Get(), 0, 0, &constantData, 0, 0);
    immediateContext->PSSetConstantBuffers(5, 1, constantBuffer.GetAddressOf());

    ID3D11ShaderResourceView* shaderResourceViews[] =
    {
        lowFreqPerlinWorleyShaderResourceView.Get(),
        highFreqWorleyShaderResourceView.Get(),
        weatherShaderResourceView.Get(),
        curlNoiseShaderResourceView.Get(),
#if 0
        gradientStratusShaderResouceView.Get(),
        gradientCumulusShaderResouceView.Get(),
        gradientCumulonimbusShaderResouceView.Get(),
#endif
    };
    fullscreenQuad->Blit(immediateContext, shaderResourceViews, 0, _countof(shaderResourceViews), volumetricCloudscapesPS.Get());
}