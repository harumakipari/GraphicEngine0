#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>

#include <vector>
#include <functional>

class CascadedShadowMaps
{
public:
    // コンストラクタ
    // width / height : シャドウマップ解像度
    // cascadeCount   : カスケード数（通常4）
    CascadedShadowMaps(ID3D11Device* device, UINT width, UINT height, UINT cascadeCount = 4);
    virtual ~CascadedShadowMaps() = default;
    // コピー禁止
    CascadedShadowMaps(const CascadedShadowMaps&) = delete;
    CascadedShadowMaps& operator=(const CascadedShadowMaps&) = delete;
    // ムーブも禁止
    CascadedShadowMaps(CascadedShadowMaps&&) noexcept = delete;
    CascadedShadowMaps& operator=(CascadedShadowMaps&&)noexcept = delete;

private:
    // シャドウマップ本体（Texture2DArray）
    // cascadeCount 分のスライスを持つ
    Microsoft::WRL::ComPtr<ID3D11Texture2D> shadowMapTexture;
    // 深度ステンシルビュー（描画用）
    Microsoft::WRL::ComPtr <ID3D11DepthStencilView> shadowMapDSV;
    // シャドウ描画用ビューポート
    D3D11_VIEWPORT viewport;

    // 各カスケードのライトビュー射影行列
    std::vector<DirectX::XMFLOAT4X4> cascadedMatrices;
    // 各カスケードの分割距離（view空間Z）
    std::vector<float> cascadedPlaneDistances;
    // シャドウマップ参照用SRV（PSで使用）
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadowMapSRV;

    struct Constants
    {
        DirectX::XMFLOAT4X4 cascadedMatrices[4];// ライトVP行列
        float cascadedPlaneDistances[4]; // 分割距離
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> csmConstantBuffer;

    // ImGuiデバッグ用
    // 各カスケードを個別に表示するためのSRV
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> debugCascadeSRVs;

public:
    // シャドウ描画開始
    // ライト視点へ切り替え、深度のみ描画状態にする
    void Activate(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& cameraView, const DirectX::XMFLOAT4X4& cameraProjection, const DirectX::XMFLOAT4& lightDirection,
        float criticalDepthValue/* この値が 0 の場合、カメラの遠方パネル距離が使用される*/, UINT cbSlot);
    // シャドウ描画終了
    // 元のレンダーターゲットへ戻す
    void Deactivate(ID3D11DeviceContext* immediateContext);
    // 深度クリア
    void Clear(ID3D11DeviceContext* immediateContext) const
    {
        immediateContext->ClearDepthStencilView(shadowMapDSV.Get(), D3D11_CLEAR_DEPTH, 1, 0);
    }
    // シャドウマップSRV取得
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& depthMap()
    {
        return shadowMapSRV;
    }

    // デバッグ表示
    void DrawImGui() const;
public:
    // カスケード数（変更不可）
    const UINT cascadeCount;
    // 分割方法の重み

private:
    // レンダー状態保存用
    // Activate時に保存 → Deactivateで復元
    D3D11_VIEWPORT savedViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    UINT savedViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> savedRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> savedDepthStencilView;
};