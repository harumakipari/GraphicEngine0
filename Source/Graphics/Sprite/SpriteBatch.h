#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>

class SpriteBatch
{
public:
    SpriteBatch(ID3D11Device* device, const wchar_t* filename, size_t maxSprite);
    ~SpriteBatch();

    void Render(ID3D11DeviceContext* immediate_context,
        float dx, float dy,//矩形の左上の座標（スクリーン座標系)
        float dw, float dh,//矩形のサイズ（スクリーン座標系）
        float r, float g, float b, float a,//色
        float angle/*degree*/);

    void Render(ID3D11DeviceContext* immediate_context,
        float dx, float dy,//矩形の左上の座標（スクリーン座標系)
        float dw, float dh,//矩形のサイズ（スクリーン座標系）
        float r, float g, float b, float a,//色
        float angle/*degree*/,
        float sx, float sy, float sw, float sh);//左上座標とサイズ

    void Render(ID3D11DeviceContext* immediate_context,
        float dx, float dy,//矩形の左上の座標（スクリーン座標系)
        float dw, float dh);//矩形のサイズ（スクリーン座標系）

    void Begin(ID3D11DeviceContext* immediateContext);
    void End(ID3D11DeviceContext* immediateContext);

private:
    //頂点フォーマット
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        //テクスチャ座標変数
        DirectX::XMFLOAT2 texCoord;
    };

private:
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3D11InputLayout* inputLayout;
    ID3D11Buffer* vertexBuffer;//頂点バッファ
    ID3D11ShaderResourceView* shaderResourceView;
    D3D11_TEXTURE2D_DESC texture2dDesc;

    const size_t maxVertices;
    std::vector<Vertex> vertices;
};

