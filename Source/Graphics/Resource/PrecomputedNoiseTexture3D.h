#pragma once

#pragma warning( once : 26451 )

#include <d3d11.h>
#include <wrl.h>

#include<vector>

#include"../External/FastNoiseLite-master/Cpp/FastNoiseLite.h"
#include "Engine/Utility/Win32Utils.h"

static HRESULT ProcomputedNoiseTexture3d(_In_ ID3D11Device* device, _In_ UINT dimension, _Out_ ID3D11ShaderResourceView** shaderResourceView)
{
    HRESULT hr{ S_OK };

    const UINT width = dimension;
    const UINT height = dimension;
    const UINT depth = dimension;
    const UINT butesPerPixel = 4;
    const UINT sliceSize = width * height * butesPerPixel;

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetRotationType3D(FastNoiseLite::RotationType3D::RotationType3D_None);
    noise.SetSeed(1337);
    noise.SetFrequency(0.1f);

    noise.SetFractalType(FastNoiseLite::FractalType::FractalType_FBm);
    noise.SetFractalOctaves(5);
    noise.SetFractalLacunarity(2.00f);
    noise.SetFractalGain(0.50f);

    std::vector<float> noiseData(width * height * depth);
    size_t index = 0;
    for (UINT z = 0; z < depth; z++)
    {
        for (UINT y = 0; y < height; y++)
        {
            for (UINT x = 0; x < width; x++)
            {
                noiseData.at(index++) = noise.GetNoise(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
            }
        }
    }

    std::vector<D3D11_SUBRESOURCE_DATA> initialData(depth);
    byte* p = reinterpret_cast<byte*>(noiseData.data());
    for (UINT z = 0; z < depth; z++)
    {
        initialData.at(z).pSysMem = static_cast<const void*>(p);
        initialData.at(z).SysMemPitch = width * butesPerPixel;
        initialData.at(z).SysMemSlicePitch = width * height * butesPerPixel;
        p += sliceSize;
    }
    D3D11_TEXTURE3D_DESC texture3dDesc{};
    texture3dDesc.Width = width;
    texture3dDesc.Height = height;
    texture3dDesc.Depth = depth;
    texture3dDesc.MipLevels = 1;
    texture3dDesc.Format = DXGI_FORMAT_R32_FLOAT;
    texture3dDesc.Usage = D3D11_USAGE_DEFAULT;
    texture3dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texture3dDesc.CPUAccessFlags = 0;
    texture3dDesc.MiscFlags = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture3D> texture3d;
    hr = device->CreateTexture3D(&texture3dDesc, initialData.data(), texture3d.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
    shaderResourceViewDesc.Format = texture3dDesc.Format;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    shaderResourceViewDesc.Texture3D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture3d.Get(), &shaderResourceViewDesc, shaderResourceView);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    return hr;
}