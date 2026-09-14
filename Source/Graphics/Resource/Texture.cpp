#include "pch.h"
#include "Texture.h"
#include <memory>
#include <filesystem>
#include <DDSTextureLoader.h>

#include <WICTextureLoader.h>
#include <wrl.h>
#include <map>
#include <string>   

#include "Engine/Utility/Win32Utils.h"
#include "Graphics/Core/Graphics.h"
using namespace DirectX;

using namespace std;

#include <wrl/client.h>
using namespace Microsoft::WRL;

static map<wstring, ComPtr<ID3D11ShaderResourceView>> resources;


HRESULT LoadTextureFromFile(
    ID3D11Device* device,
    const wchar_t* filename,
    ID3D11ShaderResourceView** shaderResourceView,
    D3D11_TEXTURE2D_DESC* texture2dDesc)
{
    HRESULT hr = S_OK;

    ComPtr<ID3D11Resource> resource;

    auto it = resources.find(filename);

    if (it != resources.end())
    {
        *shaderResourceView = it->second.Get();
        (*shaderResourceView)->AddRef();

        (*shaderResourceView)->GetResource(
            resource.GetAddressOf());
    }
    else
    {
        std::filesystem::path ddsFilename(filename);
        ddsFilename.replace_extension("dds");

        if (std::filesystem::exists(ddsFilename))
        {
            OutputDebugStringW(L"[Texture] DDS : ");
            OutputDebugStringW(ddsFilename.c_str());
            OutputDebugStringW(L"\n");

            hr = CreateDDSTextureFromFile(
                device,
                ddsFilename.c_str(),
                resource.GetAddressOf(),
                shaderResourceView);
        }
        else
        {
            OutputDebugStringW(L"[Texture] WIC : ");
            OutputDebugStringW(filename);
            OutputDebugStringW(L"\n");

            hr = CreateWICTextureFromFile(
                device,
                filename,
                resource.GetAddressOf(),
                shaderResourceView);
        }

        if (FAILED(hr))
        {
            _ASSERT_EXPR(false, hr_trace(hr));
            return hr;
        }

        resources.insert(
            make_pair(filename, *shaderResourceView));
    }

    if (texture2dDesc)
    {
        if (!resource)
            return E_FAIL;

        D3D11_RESOURCE_DIMENSION dimension;
        resource->GetType(&dimension);

        if (dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        {
            char buffer[128];

            sprintf_s(
                buffer,
                "[Texture] Not Texture2D. dimension = %d\n",
                static_cast<int>(dimension));

            OutputDebugStringA(buffer);

            return E_NOINTERFACE;
        }

        ComPtr<ID3D11Texture2D> texture2d;

        hr = resource.As(&texture2d);

        if (FAILED(hr))
        {
            _ASSERT_EXPR(false, hr_trace(hr));
            return hr;
        }

        texture2d->GetDesc(texture2dDesc);
    }

    return S_OK;
}

//ダミーテクスチャの作成
HRESULT MakeDummyTexture(ID3D11Device* device, ID3D11ShaderResourceView** shaderResourceView,
    DWORD value/*0xAABBGGRR*/, UINT dimension)
{
    HRESULT hr{ S_OK };

    D3D11_TEXTURE2D_DESC texture2dDesc{};
    texture2dDesc.Width = dimension;
    texture2dDesc.Height = dimension;
    texture2dDesc.MipLevels = 1;
    texture2dDesc.ArraySize = 1;
    texture2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture2dDesc.SampleDesc.Count = 1;
    texture2dDesc.SampleDesc.Quality = 0;
    texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
    texture2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    size_t texels = dimension * dimension;
    std::unique_ptr<DWORD[]> sysmem{ std::make_unique<DWORD[]>(texels) };
    for (size_t i = 0; i < texels; i++)
    {
        sysmem[i] = value;
    }

    D3D11_SUBRESOURCE_DATA subresourceData{};
    subresourceData.pSysMem = sysmem.get();
    subresourceData.SysMemPitch = sizeof(DWORD) * dimension;

    ComPtr<ID3D11Texture2D> texture2d;
    hr = device->CreateTexture2D(&texture2dDesc, &subresourceData, &texture2d);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
    shaderResourceViewDesc.Format = texture2dDesc.Format;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(texture2d.Get(), &shaderResourceViewDesc, shaderResourceView);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    return hr;
}


HRESULT LoadTextureFromMemory(ID3D11Device* device, const void* data, size_t size, ID3D11ShaderResourceView** shaderResourceView)
{
    HRESULT hr{ S_OK };
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;

    hr = CreateDDSTextureFromMemory(device, reinterpret_cast<const uint8_t*>(data), size, resource.GetAddressOf(), shaderResourceView);
    if (hr != S_OK)
    {
        hr = CreateWICTextureFromMemory(device, reinterpret_cast<const uint8_t*>(data), size, resource.GetAddressOf(), shaderResourceView);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    return hr;
}

bool Texture::LoadFromFile(const std::string& filePath)
{
    _path = filePath;
    auto device = Graphics::GetDevice();
    if (filePath.empty())
        return MakeDummy(device);

    return Load(device, std::wstring(filePath.begin(), filePath.end()));
}

bool Texture::Load(ID3D11Device* device, const std::wstring& filePath)
{
    HRESULT hr = LoadTextureFromFile(device, filePath.c_str(), m_Srv.ReleaseAndGetAddressOf(), &m_Desc);
    return SUCCEEDED(hr);
}

bool Texture::MakeDummy(ID3D11Device* device, DWORD value, UINT dimension)
{
    HRESULT hr = MakeDummyTexture(device, m_Srv.ReleaseAndGetAddressOf(), value, dimension);
    return SUCCEEDED(hr);
}

void Texture::Release()
{
    m_Srv.Reset();
}

// キャッシュされたすべてのテクスチャを解放する関数
void ReleaseAllTextures()
{
    resources.clear();// テクスチャキャッシュをクリアする
}