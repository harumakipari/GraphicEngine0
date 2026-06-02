#include "pch.h"
#include "SkyMap.h"
#include "Engine/Utility/Win32Utils.h"
#include "Graphics/Resource/Texture.h"
#include "Graphics/Core/Shader.h"

// SkyMap クラスのコンストラクタ
SkyMap::SkyMap(ID3D11Device* device, const wchar_t* filename, bool generateMips)
{
    HRESULT hr = S_OK;

    // テクスチャをファイルから読み込む
    D3D11_TEXTURE2D_DESC texture2dDesc;
   hr= LoadTextureFromFile(device, filename, shaderResourceView.GetAddressOf(), &texture2dDesc);
   _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // テクスチャがキューブマップかどうか判定
    if (texture2dDesc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
    {
        isTextureCube = true;
    }

    // 頂点シェーダーの読み込み
    hr = CreateVsFromCSO(device, "./Data/Shaders/SkyMapVS.cso", skyMapVs.GetAddressOf(), NULL, NULL, 0);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    // ピクセルシェーダーの読み込み（通常のスカイマップ用）
    hr = CreatePsFromCSO(device, "./Data/Shaders/SkyMapPS.cso", skyMapPs.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    // ピクセルシェーダーの読み込み（スカイボックス用）
    hr = CreatePsFromCSO(device, "./Data/Shaders/SkyBoxPS.cso", skyBoxPs.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // 定数バッファの作成（シェーダーに渡すデータを格納する）
    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(SkyMapConstants);// 定数バッファのサイズ
    bufferDesc.Usage = D3D11_USAGE_DEFAULT; // GPU からのみアクセス可能
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // 定数バッファとしてバインド
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;

    hr = device->CreateBuffer(&bufferDesc, nullptr, constantBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

// SkyMap を描画する関数
void SkyMap::Blit(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& viewProjection)
{
    // 頂点バッファを設定しない（スカイボックスは単純な四角形）
    immediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
    // プリミティブトポロジーをトライアングルストリップに設定
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    // 入力レイアウトを設定しない（このシェーダーでは不要）
    immediateContext->IASetInputLayout(NULL);
    // 頂点シェーダーをセット
    immediateContext->VSSetShader(skyMapVs.Get(), 0, 0);
    // ピクセルシェーダーをセット（キューブマップかどうかで切り替え）
    immediateContext->PSSetShader(isTextureCube ? skyBoxPs.Get() : skyMapPs.Get(), 0, 0);
    // スカイマップのテクスチャをシェーダーにセット
    immediateContext->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());
    // シェーダーに渡す行列データを作成
    //カメラの ViewProjection 行列の逆行列を求めて、スカイマップのシェーダー用に保存する処理 


    DirectX::XMStoreFloat4x4(&data.inverseViewProjection, DirectX::XMMatrixInverse(NULL, DirectX::XMLoadFloat4x4(&viewProjection)));

    // 定数バッファを更新（シェーダーに行列を渡す）
    immediateContext->UpdateSubresource(constantBuffer.Get(), 0, 0, &data, 0, 0);
    immediateContext->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    // 四角形を描画（4 頂点）
    immediateContext->Draw(4, 0);
    // 使用したシェーダーを解除（後の描画に影響を与えないようにする）
    immediateContext->VSSetShader(NULL, 0, 0);
    immediateContext->PSSetShader(NULL, 0, 0);
}

// ImGui を使ってスカイマップのパラメータを調整する関数
void SkyMap::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin(U8("スカイマップ"));
    ImGui::SliderFloat(U8("明度調整"), &data.brightness, -1.0f, +1.0f);
    ImGui::SliderFloat(U8("コントラスト調整"), &data.contrast, -1.0f, +1.0f);
    ImGui::End();
#endif
}