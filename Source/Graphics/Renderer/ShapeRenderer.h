#pragma once
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include "Graphics/Core/ConstantBuffer.h"
#include "Graphics/Resource/InterleavedGltfModel.h"

class ShapeRenderer
{
public:
    enum class Type :uint8_t
    {
        Line,// 無限に伸びる線
        Segment,// 線分
        Point// 点
    };

public:
    ShapeRenderer() = default;
    virtual ~ShapeRenderer() = default;

    static void Initialize(ID3D11Device* device);

    // 箱を描画する（箱の底面が原点になる）
    static void DrawBox(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color);

    // 箱を描画する（ワールド変換を直接指定）
    static void DrawBox(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color);

    // 箱を描画する（箱の中心が原点になる）
    static void DrawBoxCenter(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& angle, const DirectX::XMFLOAT3& size, const DirectX::XMFLOAT4& color);

    // 球を描画する
    static void DrawSphere(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, float radius, const DirectX::XMFLOAT4& color);

    // 球を描画する
    static void DrawDebugSphere(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& center, float radius,const DirectX::XMFLOAT4& color, int segments);

    // カプセルを描画する（位置のみ）
    static void DrawCapsule(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color);

    // カプセルを描画する（クォータニオンで回転を指定）
    static void DrawCapsule(ID3D11DeviceContext* immediateContext,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT4& rotation, 
        float radius, float height,
        const DirectX::XMFLOAT4& color);

    // カプセルを描画する（始点・終点で定義）
    static void DrawCapsule(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& startPosition, const DirectX::XMFLOAT3& endPosition, float radius, const DirectX::XMFLOAT4& color);

    // カプセルを描画する（ワールド変換を直接指定）
    static void DrawCapsule(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& worldTransform, float radius, float height, const DirectX::XMFLOAT4& color);

    // 汎用的な線描画（複数点）
    static void DrawSegment(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4& color, const std::vector<DirectX::XMFLOAT3>& points, Type type);

    // 点を描画する
    static void DrawPoint(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& color);

    // 線分を描画する（始点と終点を指定）
    static void DrawLineSegment(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& startPosition, const DirectX::XMFLOAT3& endPosition, const DirectX::XMFLOAT4& color);

    // 円柱を描画する
    static void DrawCylinder(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& position, float radius, float height, const DirectX::XMFLOAT4& color);

    // 数珠つなぎの線を描画する（デフォルト色・タイプ）
    static void DrawSegment(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT3& startPosition, const DirectX::XMFLOAT3& endPosition);
private:
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


    static inline constexpr size_t maxPoints = 1500; // 内部で扱える最大頂点数 

    // プリミティブ用のモデル
    static inline std::unique_ptr<InterleavedGltfModel> sphere = nullptr; // 球モデル 
    static inline std::unique_ptr<InterleavedGltfModel> circle = nullptr; // 円モデル 
    static inline std::unique_ptr<InterleavedGltfModel> topHalfSphere = nullptr; // 半球（上）モデル 
    static inline std::unique_ptr<InterleavedGltfModel> bottomHalfSphere = nullptr; // 半球（下）モデル 
    static inline std::unique_ptr<InterleavedGltfModel> cylinder = nullptr; // 円柱モデル 
    static inline std::unique_ptr<InterleavedGltfModel> capsule = nullptr; // カプセルモデル 
    static inline std::unique_ptr<InterleavedGltfModel> cube = nullptr; // 箱（底が原点）モデル 
    static inline std::unique_ptr<InterleavedGltfModel> cubeCenter = nullptr; // 箱（中心が原点）モデル 

};

class LineSegment
{
public:
    enum class Type
    {
        Line,// 無限に伸びる線
        Segment,// 区切られた線分
        Point// 点描画
    };

    LineSegment(ID3D11Device* device, size_t maxSegments);

    virtual ~LineSegment() = default;

    void Draw(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& viewProjection, const DirectX::XMFLOAT4& color, const std::vector<DirectX::XMFLOAT3>& points, Type type);
private:
    struct Constants
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4 color;
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader; 
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout; 

    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;

    const size_t maxPoints; // 内部で保持できる最大頂点数 

};
