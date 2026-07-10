#pragma once

#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include "Graphics/Core/Graphics.h"
#include "Graphics/Core/Shader.h"

class Trail
{
public:
    // 軌跡構造　CPUで更新するもの
    struct TrailPoint
    {
        DirectX::XMFLOAT3 tip;
        DirectX::XMFLOAT3 root;
        float life = 0.5f; // 残り時間
    };
    std::vector<TrailPoint> trailPoints;

    void Initialize();

    void UpdateTrail(float deltaTime);

    void Render(ID3D11DeviceContext* immediateContext);

private:
    // 頂点構造体　GPUに送るもの
    struct TrailVertex
    {
        DirectX::XMFLOAT3 position;
        float alpha;
        DirectX::XMFLOAT2 uv;
    };
    std::vector<TrailVertex> vertices;
    size_t maxPoints = 1500; /**< 内部で扱える最大頂点数 */

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noise2d;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noise3d;

    // バッファ/シェーダ/入力レイアウト
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
};
