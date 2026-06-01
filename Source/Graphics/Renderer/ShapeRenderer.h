#pragma once
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include "Graphics/Core/ConstantBuffer.h"
#include "Graphics/Resource/InterleavedGltfModel.h"

/**
 * @file ShapeRenderer.h
 * @brief デバッグ用の形状描画ユーティリティ定義
 */
class ShapeRenderer
{
public:
    /**
    * @brief 描画プリミティブの種類
    */
    enum class Type :uint8_t
    {
        /// 無限に伸びる線
        Line,
        /// 線分
        Segment,
        /// 点
        Point
    };

public:
    ShapeRenderer() = default;
    virtual ~ShapeRenderer() = default;

    /**
    * @brief シェイプレンダラーの初期化を行う
    * @param device 描画に使用するD3D11デバイス
    */
    static void Initialize(ID3D11Device* device);

    /**
    * @brief 箱を描画する（箱の底面が原点になる）
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param position 箱の位置（底面が原点の基準位置）
    * @param angle 回転（ラジアン）を含むXYZ角
    * @param size 箱のサイズ（幅・高さ・奥行）
    * @param color 描画色（RGBA）
    */
    static void DrawBox(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color);

    /**
    * @brief 箱を描画する（ワールド変換を直接指定）
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param transform ワールド変換行列
    * @param size 箱のサイズ
    * @param color 描画色（RGBA）
    */
    static void DrawBox(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color);

    /**
    * @brief 箱を描画する（箱の中心が原点になる）
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param position 箱の中心位置
    * @param angle 回転（ラジアン）を含むXYZ角
    * @param size 箱のサイズ
    * @param color 描画色（RGBA）
    */
    static void DrawBoxCenter(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color);

    /**
    * @brief 球を描画する
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param position 球の中心位置
    * @param radius 半径
    * @param color 描画色（RGBA）
    */
    static void DrawSphere(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, float radius, const DirectX::XMFLOAT4& color);
    static void DrawDebugSphere(ID3D11DeviceContext* context, const DirectX::XMFLOAT3& center, float radius,
        const DirectX::XMFLOAT4& color, int segments);

    /**
    * @brief カプセルを描画する（位置のみ）
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param position カプセルの位置
    * @param radius カプセルの半径
    * @param height カプセルの高さ（シリンダ部分の長さ）
    * @param color 描画色（RGBA）
    */
    static void DrawCapsule(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color);

    /**
    * @brief カプセルを描画する（クォータニオンで回転を指定）
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param position カプセルの位置
    * @param rotation クォータニオンによる回転（X,Y,Z,W）
    * @param radius カプセルの半径
    * @param height カプセルの高さ
    * @param color 描画色（RGBA）
    */
    static void DrawCapsule(ID3D11DeviceContext* immediateContext,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT4& rotation, // ← クォータニオン追加
        float radius, float height,
        const DirectX::XMFLOAT4& color);

    /**
    * @brief カプセルを描画する（始点・終点で定義）
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param startPosition カプセルの始点
    * @param endPosition カプセルの終点
    * @param radius カプセルの半径
    * @param color 描画色（RGBA）
    */
    static void DrawCapsule(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& startPosition, const DirectX::XMFLOAT3& endPosition, float radius, const DirectX::XMFLOAT4& color);

    /**
    * @brief カプセルを描画する（ワールド変換を直接指定）
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param worldTransform ワールド変換行列
    * @param radius カプセルの半径
    * @param height カプセルの高さ
    * @param color 描画色（RGBA）
    */
    static void DrawCapsule(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& worldTransform, float radius, float height, const DirectX::XMFLOAT4& color);

    /**
    * @brief 汎用的な線描画（複数点）
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param color 線の色
    * @param points 頂点配列
    * @param type 描画タイプ（Line/Segment/Point）
    */
    static void DrawSegment(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4& color, const std::vector<DirectX::XMFLOAT3>& points, Type type);

    /**
    * @brief 点を描画する
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param position 点の位置
    * @param color 点の色
    */
    static void DrawPoint(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& color);

    /**
    * @brief 線分を描画する（始点と終点を指定）
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param startPosition 始点
    * @param endPosition 終点
    * @param color 線の色
    */
    static void DrawLineSegment(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& startPosition, const DirectX::XMFLOAT3& endPosition, const DirectX::XMFLOAT4& color);

    /**
    * @brief 円柱を描画する
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param position 円柱の中心位置
    * @param radius 半径
    * @param height 高さ
    * @param color 描画色（RGBA）
    */
    static void DrawCylinder(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color);

    /**
    * @brief 数珠つなぎの線を描画する（デフォルト色・タイプ）
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param startPosition 始点
    * @param endPosition 終点
    */
    static void DrawSegment(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& startPosition, const DirectX::XMFLOAT3& endPosition);
private:
    /**
    * @brief シェーダに渡すデバッグ定数バッファ構造体
    */
    struct DebugConstants
    {
        DirectX::XMFLOAT4 cpuColor; // 色をCPU側で指定する用　（ダメージ当たったときとか）

        float	hueShift;	// 色相調整
        float	saturation;	// 彩度調整
        float	brightness;	// 明度調整
        float   dissolve;   // ディゾルブ用

        DirectX::XMFLOAT4 morphWeights = { 0.0f,0.0f,0.0f,0.0f };  // モーフモデルに使用する weight 0.0f ~ 1.0f
        float emissionPower; // 自己発光の強さ
    };
    static inline DebugConstants debugConstants{};

    static inline std::unique_ptr<ConstantBuffer<DebugConstants>> debugConstantCBuffer;

    // バッファ/シェーダ/入力レイアウト
    static inline Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;

    static inline Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    static inline Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    static inline Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;


    static inline constexpr size_t maxPoints = 1500; /**< 内部で扱える最大頂点数 */

    // プリミティブ用のモデル
    static inline std::unique_ptr<InterleavedGltfModel> sphere = nullptr; /**< 球モデル */
    static inline std::unique_ptr<InterleavedGltfModel> circle = nullptr; /**< 円モデル */
    static inline std::unique_ptr<InterleavedGltfModel> topHalfSphere = nullptr; /**< 半球（上）モデル */
    static inline std::unique_ptr<InterleavedGltfModel> bottomHalfSphere = nullptr; /**< 半球（下）モデル */
    static inline std::unique_ptr<InterleavedGltfModel> cylinder = nullptr; /**< 円柱モデル */
    static inline std::unique_ptr<InterleavedGltfModel> capsule = nullptr; /**< カプセルモデル */
    static inline std::unique_ptr<InterleavedGltfModel> cube = nullptr; /**< 箱（底が原点）モデル */
    static inline std::unique_ptr<InterleavedGltfModel> cubeCenter = nullptr; /**< 箱（中心が原点）モデル */



};


// デバッグ用の直線・線分・点を描画するユーティリティクラス
/**
 * @brief 可変長のライン/セグメント描画を行うヘルパークラス
 */
class LineSegment
{
public:
    /**
    * @brief 描画タイプ
    */
    enum class Type
    {
        /// 無限に伸びる線
        Line,
        /// 区切られた線分
        Segment,
        /// 点描画
        Point
    };

    /**
    * @brief コンストラクタ
    * @param device D3D11デバイス
    * @param maxSegments 確保する最大セグメント数（内部バッファサイズ）
    */
    LineSegment(ID3D11Device* device, size_t maxSegments);

    virtual ~LineSegment() = default;

    /**
    * @brief ベクタ点列を与えて描画する
    * @param immediateContext 描画に使用するデバイスコンテキスト
    * @param viewProjection ビュープロジェクション行列
    * @param color 線/点の色
    * @param points 頂点配列
    * @param type 描画タイプ
    */
    void Draw(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& viewProjection, const DirectX::XMFLOAT4& color, const std::vector<DirectX::XMFLOAT3>& points, Type type);
private:
    /**
    * @brief 描画用定数バッファ構造体
    */
    struct Constants
    {
        DirectX::XMFLOAT4X4 viewProjection; /**< ビュープロジェクション行列 */
        DirectX::XMFLOAT4 color; /**< 描画色 */
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer; /**< 頂点バッファ */

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader; /**< 頂点シェーダ */
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader; /**< ピクセルシェーダ */
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout; /**< 入力レイアウト */

    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer; /**< 定数バッファ */

    const size_t max_points; /**< 内部で保持できる最大頂点数 */

};
