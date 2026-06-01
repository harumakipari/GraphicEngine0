#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <wrl.h>

class Sprite
{
public:
    Sprite(ID3D11Device* device,const wchar_t* filename);
    ~Sprite();

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

    void Render(ID3D11DeviceContext* immediate_context,
        float dx, float dy,
        float dw, float dh,
        DirectX::XMFLOAT2 pivot,
        DirectX::XMFLOAT4 color,
        float sx, float sy,
        float sw, float sh
    );
    void Render(ID3D11DeviceContext* immediate_context,
        float dx, float dy,
        float dw, float dh,
        DirectX::XMFLOAT4 color,
        float angle,
        float sx, float sy,
        float sw, float sh,
        float scaleX, float scaleY,
        float pivotX, float pivotY);
    //フォント画像ファイルを使用して任意の文字列を画面に出力する
    void Textout(ID3D11DeviceContext* immediateContext, std::string s,
        float x, float y, float w, float h, float r, float g, float b, float a);

    void SoftRender(ID3D11DeviceContext* immediate_context,
        DirectX::XMFLOAT2 leftTop, DirectX::XMFLOAT2 rightTop, DirectX::XMFLOAT2 leftBottom, DirectX::XMFLOAT2 rightBottom,float sx, float sy, float sw, float sh, float r, float g, float b, float a);

    DirectX::XMFLOAT2 GetTextureSize() const
    {
        DirectX::XMFLOAT2 size;
        size.x = static_cast<float>(texture2dDesc.Width);
        size.y = static_cast<float>(texture2dDesc.Height);
        return size;
    }
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
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;//頂点バッファ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    D3D11_TEXTURE2D_DESC texture2dDesc;
};

