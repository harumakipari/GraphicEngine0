#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>

// SkyMap クラス: スカイボックスや環境マップを描画するクラス
class SkyMap
{
public:
    // コンストラクタ: デバイスとテクスチャファイルを受け取る
    SkyMap(ID3D11Device* device, const wchar_t* filename, bool generateMips = false);
    virtual ~SkyMap() = default;

    // コピー禁止（オブジェクトの重複を防ぐ）
    SkyMap(const SkyMap&) = delete;
    SkyMap& operator =(const SkyMap&) = delete;
    SkyMap(SkyMap&&) noexcept = delete;
    SkyMap& operator =(SkyMap&&) noexcept = delete;

    // スカイマップを描画する関数
    void Blit(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& viewProjection);

    // ImGui を使ってスカイマップのパラメータを調整する関数
    void DrawImGui();

    // スカイマップのテクスチャ（シェーダーリソースビュー）
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
private:
    // 頂点シェーダー（SkyMap 用）
    Microsoft::WRL::ComPtr<ID3D11VertexShader> skyMapVs;
    // ピクセルシェーダー（通常の SkyMap 用）
    Microsoft::WRL::ComPtr<ID3D11PixelShader> skyMapPs;
    // ピクセルシェーダー（スカイボックス用）
    Microsoft::WRL::ComPtr<ID3D11PixelShader> skyBoxPs;

    // 定数バッファ（シェーダーに渡すデータ）
    struct SkyMapConstants
    {
        DirectX::XMFLOAT4X4 inverseViewProjection;// ビュープロジェクション行列の逆行列

        float	brightness = 0.0f;	// 明度調整（-1 は完全な黒、0 は変化なし、1 は完全な白）
        float	contrast = 0.7f;	// コントラスト調整（-1は完全な灰色、0は変化なし、1は最大コントラスト）
        float paddings[2];
    };
    SkyMapConstants data ;

    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;

    //テクスチャがキューブマップかどうかを判定するフラグ
    bool isTextureCube = false;
};