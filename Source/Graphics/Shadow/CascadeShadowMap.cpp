#include "pch.h"
#define NOMINMAX

#include "CascadeShadowMap.h"

#include <array>
#include <algorithm>

#include "imgui.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Utility/Win32Utils.h"

// ビュー行列 + プロジェクション行列からワールド空間でのフラスタム8頂点を取得する
std::array<DirectX::XMFLOAT4, 8> ExtractFrustumCorners(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
    const DirectX::XMMATRIX VP_ = XMMatrixInverse(NULL, XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection));

    std::array<DirectX::XMFLOAT4, 8> frustum_corners;
    size_t index = 0;
    for (size_t x = 0; x < 2; ++x)
    {
        for (size_t y = 0; y < 2; ++y)
        {
            for (size_t z = 0; z < 2; ++z)
            {
                DirectX::XMFLOAT4 pt = { 2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f };
                XMStoreFloat4(&pt, XMVector3TransformCoord(XMLoadFloat4(&pt), VP_));
                frustum_corners.at(index++) = pt;
            }
        }
    }
    return frustum_corners;


    // NDC空間の8頂点
    std::array<DirectX::XMFLOAT4, 8> frustumCorners =
    {
        DirectX::XMFLOAT4{-1.0f,-1.0f,-1.0f,1.0f},
        DirectX::XMFLOAT4{-1.0f,-1.0f, 1.0f,1.0f},
        DirectX::XMFLOAT4{-1.0f, 1.0f,-1.0f,1.0f},
        DirectX::XMFLOAT4{-1.0f, 1.0f, 1.0f,1.0f},
        DirectX::XMFLOAT4{ 1.0f,-1.0f,-1.0f,1.0f},
        DirectX::XMFLOAT4{ 1.0f,-1.0f, 1.0f,1.0f},
        DirectX::XMFLOAT4{ 1.0f, 1.0f,-1.0f,1.0f},
        DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f,1.0f},
    };

    // ViewProjection の逆行列
    const DirectX::XMMATRIX invViewProjection = DirectX::XMMatrixInverse(NULL, DirectX::XMLoadFloat4x4(&view) * DirectX::XMLoadFloat4x4(&projection));

    // ワールド空間へ変換
    for (std::array<DirectX::XMFLOAT4, 8>::reference frustumCorner : frustumCorners)
    {
        DirectX::XMStoreFloat4(&frustumCorner, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat4(&frustumCorner), invViewProjection));
    }
    return frustumCorners;
}


CascadedShadowMaps::CascadedShadowMaps(ID3D11Device* device, UINT width, UINT height, UINT cascadeCount) :cascadeCount(cascadeCount), cascadedMatrices(cascadeCount), cascadedPlaneDistances(cascadeCount + 1)
{
    HRESULT hr = S_OK;

    // シャドウマップ用 深度テクスチャ（Texture2DArray）を作成
    // cascadeCount 分のスライスを持つ 2D 配列テクスチャ
    D3D11_TEXTURE2D_DESC texture2dDesc = {};
    texture2dDesc.Width = width;
    texture2dDesc.Height = height;
    texture2dDesc.MipLevels = 1;
    texture2dDesc.ArraySize = cascadeCount;
    texture2dDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    texture2dDesc.SampleDesc.Count = 1;
    texture2dDesc.SampleDesc.Quality = 0;
    texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
    texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texture2dDesc.CPUAccessFlags = 0;
    texture2dDesc.MiscFlags = 0;
    hr = device->CreateTexture2D(&texture2dDesc, 0, shadowMapTexture.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // 深度ステンシルビュー（DSV）作成
    // テクスチャ配列全体をまとめて扱う
    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
    depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    depthStencilViewDesc.Texture2DArray.FirstArraySlice = 0;
    depthStencilViewDesc.Texture2DArray.ArraySize = static_cast<UINT>(cascadeCount);
    depthStencilViewDesc.Texture2DArray.MipSlice = 0;
    depthStencilViewDesc.Flags = 0;
    hr = device->CreateDepthStencilView(shadowMapTexture.Get(), &depthStencilViewDesc, shadowMapDSV.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // シェーダリソースビュー（SRV）作成
    // PixelShaderから参照するため
#if 1
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViesDesc = {};
    shaderResourceViesDesc.Format = DXGI_FORMAT_R32_FLOAT; // DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_UNORM
    shaderResourceViesDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    shaderResourceViesDesc.Texture2DArray.ArraySize = static_cast<UINT>(cascadeCount);
    shaderResourceViesDesc.Texture2DArray.MipLevels = 1;
    shaderResourceViesDesc.Texture2DArray.FirstArraySlice = 0;
    shaderResourceViesDesc.Texture2DArray.MostDetailedMip = 0;
    hr = device->CreateShaderResourceView(shadowMapTexture.Get(), &shaderResourceViesDesc, shadowMapSRV.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif // 0

    // シャドウ描画用ビューポート設定
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;

    // CSM用定数バッファ作成
    // 行列と分割距離をシェーダへ送る
    // 16byte境界に揃える
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = (sizeof(Constants) + 0x0f) & ~0x0f;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.CPUAccessFlags = 0;
    hr = device->CreateBuffer(&bufferDesc, NULL, csmConstantBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // デバッグ表示用：各カスケード単体SRV作成
    // ImGuiで個別に表示するため
    debugCascadeSRVs.resize(cascadeCount);

    for (UINT i = 0; i < cascadeCount; ++i)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R32_FLOAT;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        desc.Texture2D.MostDetailedMip = 0;

        // slice 指定
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray.FirstArraySlice = i;
        desc.Texture2DArray.ArraySize = 1;
        desc.Texture2DArray.MipLevels = 1;
        desc.Texture2DArray.MostDetailedMip = 0;

        hr = device->CreateShaderResourceView(shadowMapTexture.Get(), &desc, debugCascadeSRVs[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
}

void CascadedShadowMaps::Activate(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& cameraView, const DirectX::XMFLOAT4X4& cameraProjection, const DirectX::XMFLOAT4& lightDirection,
    const float criticalDepthValue/* この値が 0 の場合、カメラの遠方パネル距離が使用される。*/, const UINT cbSlot)
{
    auto& shadow = Scene::GetCurrentScene()->GetSceneSettings().cascadedShadowMapConstants;

    // 現在のレンダー状態を保存
    immediateContext->RSGetViewports(&savedViewportCount, savedViewports);
    immediateContext->OMGetRenderTargets(1, savedRenderTargetView.ReleaseAndGetAddressOf(), savedDepthStencilView.ReleaseAndGetAddressOf());

    // カメラの near / far をプロジェクション行列から取得
    float m33 = cameraProjection._33;
    float m43 = cameraProjection._43;
    float zn = -m43 / m33;                  // near
    float zf = (m33 * zn) / (m33 - 1);      // far
    // 任意の深度制限
    zf = criticalDepthValue > 0 ? std::min<float>(zf, criticalDepthValue) : zf;

    // カスケード分割距離計算（対数＋線形補間）
    for (size_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
    {
        float idc = cascadeIndex / static_cast<float>(cascadeCount);
        float logarithmicSplitScheme = zn * pow(zf / zn, idc);
        float uniformSplitScheme = zn + (zf - zn) * idc;
        cascadedPlaneDistances.at(cascadeIndex) = logarithmicSplitScheme * shadow.splitSchemeWeight + uniformSplitScheme * (1 - shadow.splitSchemeWeight);
    }
    cascadedPlaneDistances.at(0) = zn;
    cascadedPlaneDistances.at(cascadeCount) = zf;

    // 各カスケードのライト空間行列作成
    for (size_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
    {
        float nearPlane = shadow.fitToCascade ? cascadedPlaneDistances.at(cascadeIndex) : zn;
        float farPlane = cascadedPlaneDistances.at(cascadeIndex + 1);

        // カスケード用射影行列再構築
        DirectX::XMFLOAT4X4 cascadedProjection = cameraProjection;
        cascadedProjection._33 = farPlane / (farPlane - nearPlane);
        cascadedProjection._43 = -nearPlane * farPlane / (farPlane - nearPlane);

        // フラスタム8頂点取得
        std::array<DirectX::XMFLOAT4, 8> corners = ExtractFrustumCorners(cameraView, cascadedProjection);

        // フラスタム中心計算
        DirectX::XMFLOAT4 center = { 0, 0, 0, 1 };
        for (DirectX::XMFLOAT4 corner : corners)
        {
            center.x += corner.x;
            center.y += corner.y;
            center.z += corner.z;
        }
        center.x /= corners.size();
        center.y /= corners.size();
        center.z /= corners.size();
#if 0
        // ライトビュー行列作成
        DirectX::XMMATRIX V;
        V = DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(center.x - lightDirection.x, center.y - lightDirection.y, center.z - lightDirection.z, 1.0f),
            DirectX::XMVectorSet(center.x, center.y, center.z, 1.0f),
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

#else

        using namespace DirectX;
        DirectX::XMVECTOR LightDir = DirectX::XMVector3Normalize(XMLoadFloat4(&lightDirection));
        DirectX::XMMATRIX V = XMMatrixLookAtLH(
            XMVectorSet(center.x, center.y, center.z, 1.0f) - LightDir,
            XMVectorSet(center.x, center.y, center.z, 1.0f),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

        //XMVECTOR lightPos = XMVectorScale(lightDir, -1000.0f);

        //XMMATRIX V = XMMatrixLookAtLH(
        //    lightPos,
        //    XMVectorZero(),
        //    XMVectorSet(0, 1, 0, 0));
#endif // 0

        // AABB計算（ライト空間）
        float minX = (std::numeric_limits<float>::max)();
        float maxX = (std::numeric_limits<float>::lowest)();
        float minY = (std::numeric_limits<float>::max)();
        float maxY = (std::numeric_limits<float>::lowest)();
        float minZ = (std::numeric_limits<float>::max)();
        float maxZ = (std::numeric_limits<float>::lowest)();
        for (DirectX::XMFLOAT4 corner : corners)
        {
            DirectX::XMStoreFloat4(&corner, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat4(&corner), V));
            minX = std::min<float>(minX, corner.x);
            maxX = std::max<float>(maxX, corner.x);
            minY = std::min<float>(minY, corner.y);
            maxY = std::max<float>(maxY, corner.y);
            minZ = std::min<float>(minZ, corner.z);
            maxZ = std::max<float>(maxZ, corner.z);
        }


#if 0
        // ===== テクセルスナップ開始 =====

        float shadowMapResolution = 4096.0f;

        float cascadeWidth = maxX - minX;
        float cascadeHeight = maxY - minY;

        float texelSizeX = cascadeWidth / shadowMapResolution;
        float texelSizeY = cascadeHeight / shadowMapResolution;

        // ライト空間中心
        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;

        // テクセル単位でスナップ
        centerX = floor(centerX / texelSizeX) * texelSizeX;
        centerY = floor(centerY / texelSizeY) * texelSizeY;

        // min/max再計算
        minX = centerX - cascadeWidth * 0.5f;
        maxX = centerX + cascadeWidth * 0.5f;
        minY = centerY - cascadeHeight * 0.5f;
        maxY = centerY + cascadeHeight * 0.5f;

        // ===== テクセルスナップ終了 =====

#endif // 1
#if 1
        // Z拡張（シャドウ欠け防止）
        shadow.zDepthScale = std::max<float>(1.0f, shadow.zDepthScale);
        if (minZ < 0)
        {
            minZ *= shadow.zDepthScale;
        }
        else
        {
            minZ /= shadow.zDepthScale;
        }
        if (maxZ < 0)
        {
            maxZ /= shadow.zDepthScale;
        }
        else
        {
            maxZ *= shadow.zDepthScale;
        }
#endif

        // ライト射影行列
        DirectX::XMMATRIX P = DirectX::XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);
        DirectX::XMStoreFloat4x4(&cascadedMatrices.at(cascadeIndex), V * P);
    }

    // 定数バッファ更新
    Constants data;
    data.cascadedMatrices[0] = cascadedMatrices.at(0);
    data.cascadedMatrices[1] = cascadedMatrices.at(1);
    data.cascadedMatrices[2] = cascadedMatrices.at(2);
    data.cascadedMatrices[3] = cascadedMatrices.at(3);

    data.cascadedPlaneDistances[0] = cascadedPlaneDistances.at(1);
    data.cascadedPlaneDistances[1] = cascadedPlaneDistances.at(2);
    data.cascadedPlaneDistances[2] = cascadedPlaneDistances.at(3);
    data.cascadedPlaneDistances[3] = cascadedPlaneDistances.at(4);

    immediateContext->UpdateSubresource(csmConstantBuffer.Get(), 0, 0, &data, 0, 0);
    immediateContext->VSSetConstantBuffers(cbSlot, 1, csmConstantBuffer.GetAddressOf());
    immediateContext->PSSetConstantBuffers(cbSlot, 1, csmConstantBuffer.GetAddressOf());

    // シャドウ描画用にレンダーターゲット切替
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> nullRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> nullDepthStencilView;
    immediateContext->ClearDepthStencilView(shadowMapDSV.Get(), D3D11_CLEAR_DEPTH, 1, 0);
    immediateContext->OMSetRenderTargets(1, nullRenderTargetView.GetAddressOf(), shadowMapDSV.Get());
    immediateContext->RSSetViewports(1, &viewport);
}
void CascadedShadowMaps::Deactivate(ID3D11DeviceContext* immediateContext)
{
    // 保存していたレンダー状態を復元
    immediateContext->RSSetViewports(savedViewportCount, savedViewports);
    immediateContext->OMSetRenderTargets(1, savedRenderTargetView.GetAddressOf(), savedDepthStencilView.Get());

    savedRenderTargetView.Reset();
    savedDepthStencilView.Reset();
}

void CascadedShadowMaps::DrawImGui() const
{
#ifdef USE_IMGUI
    ImGui::Begin(U8("カスケードシャドウマップのデバッグ"));

    // 各カスケードの深度テクスチャを表示
    for (UINT i = 0; i < cascadeCount; ++i)
    {
        ImGui::Text(reinterpret_cast<const char*>(u8"カスケード %d"), i);
        ImGui::Image(
            debugCascadeSRVs[i].Get(),
            ImVec2(256, 256));
        ImGui::Separator();
    }

    ImGui::End();
#endif
}
