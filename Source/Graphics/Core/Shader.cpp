#include "pch.h"
#include "Shader.h"

#include <sstream>
#include <memory>

#include "Engine/Utility/Win32Utils.h"
#include "Engine/Debug/Assert.h"
HRESULT CreateVsFromCSO(ID3D11Device* device,
    const char* csoName, ID3D11VertexShader** vertexShader,
    ID3D11InputLayout** inputLayout, D3D11_INPUT_ELEMENT_DESC* inputElementDesc,
    UINT numElements)
{
    // ファイルポインタを初期化
    FILE* fp{ nullptr };
    // CSOファイルを読み込むためにファイルをバイナリモードで開く
    fopen_s(&fp, csoName, "rb");
    _ASSERT_EXPR_A(fp, "CSO File not found");

    fseek(fp, 0, SEEK_END);     // ファイルの末尾に移動
    long cso_sz{ ftell(fp) };   // ファイルサイズを取得
    fseek(fp, 0, SEEK_SET);     // ファイルの先頭に移動

    // CSOデータを格納するためのメモリを確保
    std::unique_ptr<unsigned char[]> cso_data{ std::make_unique<unsigned char[]>(cso_sz) };
    fread(cso_data.get(), cso_sz, 1, fp);   // ファイルからデータを読み込む
    fclose(fp);     // ファイルを閉じる

    HRESULT hr{ S_OK };
    // 頂点シェーダーを作成
    hr = device->CreateVertexShader(cso_data.get(), cso_sz, nullptr, vertexShader);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    
    // 入力レイアウトが設定されていれば、それを作成
    if (inputLayout)
    {
        hr = device->CreateInputLayout(inputElementDesc, numElements,
            cso_data.get(), cso_sz, inputLayout);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    return hr;
}


HRESULT CreatePsFromCSO(ID3D11Device* device,
    const char* csoName, ID3D11PixelShader** pixelShader)
{
    // ファイルポインタを初期化
    FILE* fp{ nullptr };
    fopen_s(&fp, csoName, "rb");
    _ASSERT_EXPR_A(fp, "CSO File not found");

    fseek(fp, 0, SEEK_END);     // ファイルの末尾に移動
    long cso_sz{ ftell(fp) };   // ファイルサイズを取得
    fseek(fp, 0, SEEK_SET);     // ファイルの先頭に移動

    // CSOデータを格納するためのメモリを確保
    std::unique_ptr<unsigned char[]> cso_data{ std::make_unique<unsigned char[]>(cso_sz) };
    fread(cso_data.get(), cso_sz, 1, fp);   // ファイルからデータを読み込む
    fclose(fp);     // ファイルを閉じる

    HRESULT hr{ S_OK };
    // ピクセルシェーダーを作成
    hr = device->CreatePixelShader(cso_data.get(), cso_sz, nullptr, pixelShader);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    return hr;
}

HRESULT CreateGsFromCSO(ID3D11Device* device, const char* csoName, ID3D11GeometryShader** geometryShader)
{
    FILE* fp = nullptr;
    fopen_s(&fp, csoName, "rb");
    _ASSERT_EXPR_A(fp, "CSO File not found");

    fseek(fp, 0, SEEK_END);
    long csoSz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std::unique_ptr<unsigned char[]> csoData = std::make_unique<unsigned char[]>(csoSz);
    fread(csoData.get(), csoSz, 1, fp);
    fclose(fp);

    HRESULT hr = device->CreateGeometryShader(csoData.get(), csoSz, nullptr, geometryShader);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    return hr;
}

HRESULT CreateCsFromCSO(ID3D11Device* device, const char* csoName, ID3D11ComputeShader** computeShader)
{
    FILE* fp = nullptr;
    fopen_s(&fp, csoName, "rb");
    _ASSERT_EXPR_A(fp, "CSO File not found");

    fseek(fp, 0, SEEK_END);
    long csoSz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std::unique_ptr<unsigned char[]> csoData = std::make_unique<unsigned char[]>(csoSz);
    fread(csoData.get(), csoSz, 1, fp);
    fclose(fp);

    HRESULT hr = device->CreateComputeShader(csoData.get(), csoSz, nullptr, computeShader);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    return hr;
}
