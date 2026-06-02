#include "pch.h"
#include "SpriteBatch.h"
#include <sstream>
//画像ファイルのロード
#include <WICTextureLoader.h>

#include "Engine/Utility/Win32Utils.h"
#include "Graphics/Resource/Texture.h"
#include "Graphics/Core/Shader.h"

SpriteBatch::SpriteBatch(ID3D11Device* device, const wchar_t* filename, size_t maxSprites)
    :maxVertices(maxSprites * 6)
{
    HRESULT hr{ S_OK };

    //頂点情報
    //Vertex vertices[]
    //{
    //    { { -0.5, +0.5, 0 }, { 1, 1, 1, 1 },{0,0} },
    //    { { +0.5, +0.5, 0 }, { 1, 0, 0, 1 },{1,0} },
    //    { { -0.5, -0.5, 0 }, { 0, 1, 0, 1 },{0,1} },
    //    { { +0.5, -0.5, 0 }, { 0, 0, 1, 1 },{1,1} },
    //};

    //頂点バッファオブジェクトを生成
    {
        D3D11_BUFFER_DESC buffer_desc{};
        buffer_desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * maxVertices);
        //buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;
        hr = device->CreateBuffer(&buffer_desc, NULL, &vertexBuffer);
        //D3D11_SUBRESOURCE_DATA subresource_data{};
        //subresource_data.pSysMem = vertices.data();
        //subresource_data.SysMemPitch = 0;
        //subresource_data.SysMemSlicePitch = 0;
        //hr = device->CreateBuffer(&buffer_desc, NULL, &vertex_buffer);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    //入力レイアウトオブジェクトを生成
    D3D11_INPUT_ELEMENT_DESC input_element_desc[]
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
        D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
    };

    //頂点シェーダーオブジェクトを生成
    hr = CreateVsFromCSO(device, "./Data/Shaders/sprite_vs.cso", &vertexShader, &inputLayout, input_element_desc, _countof(input_element_desc));
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    //ピクセルシェーダーオブジェクトを生成
    hr = CreatePsFromCSO(device, "./Data/Shaders/sprite_ps.cso", &pixelShader);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    //テクスチャのロード
    hr = LoadTextureFromFile(device, filename, &shaderResourceView, &texture2dDesc);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

}

SpriteBatch::~SpriteBatch()
{
    vertexShader->Release();
    pixelShader->Release();
    inputLayout->Release();
    vertexBuffer->Release();//頂点バッファ
    shaderResourceView->Release();
}

void SpriteBatch::Begin(ID3D11DeviceContext* immediateContext)
{
    vertices.clear();
    immediateContext->VSSetShader(vertexShader, nullptr, 0);
    immediateContext->PSSetShader(pixelShader, nullptr, 0);
    immediateContext->PSSetShaderResources(0, 1, &shaderResourceView);
}

void SpriteBatch::End(ID3D11DeviceContext* immediateContext)
{
    //計算結果で頂点バッファオブジェクトを更新する
    HRESULT hr{ S_OK };
    D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
    hr = immediateContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD,
        0, &mapped_subresource);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    size_t vertexCount = vertices.size();
    _ASSERT_EXPR(maxVertices >= vertexCount, "Buffer overflow");
    Vertex* data{ reinterpret_cast<Vertex*>(mapped_subresource.pData) };
    if (data != nullptr)
    {
        const Vertex* p = vertices.data();
        memcpy_s(data, maxVertices * sizeof(Vertex), p, vertexCount * sizeof(Vertex));
    }
    immediateContext->Unmap(vertexBuffer, 0);


    //頂点バッファのバインド(接続）
    UINT stride{ sizeof(Vertex) };
    UINT offset{ 0 };
    immediateContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    //プリミティブタイプおよびデータの順序に関する情報のバインド
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //入力レイアウトオブジェクトのバインド
    immediateContext->IASetInputLayout(inputLayout);
    //プリミティブの描画
    immediateContext->Draw(static_cast<UINT>(vertexCount), 0);
}

void SpriteBatch::Render(ID3D11DeviceContext* immediate_context,
    float dx, float dy,//矩形の左上の座標（スクリーン座標系)
    float dw, float dh,//矩形のサイズ（スクリーン座標系）
    float r, float g, float b, float a,//色
    float angle/*degree*/)
{
    Render(immediate_context, dx, dy, dw, dh, r, g, b, a, angle, 0, 0, static_cast<float>(texture2dDesc.Width), static_cast<float>(texture2dDesc.Height));
}

void SpriteBatch::Render(ID3D11DeviceContext* immediate_context,
    float dx, float dy,//矩形の左上の座標（スクリーン座標系)
    float dw, float dh)//矩形のサイズ（スクリーン座標系）
{
    Render(immediate_context, dx, dy, dw, dh, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0, 0, static_cast<float>(texture2dDesc.Width), static_cast<float>(texture2dDesc.Height));
}

void SpriteBatch::Render(ID3D11DeviceContext* immediate_context,
    float dx, float dy,//矩形の左上の座標（スクリーン座標系)
    float dw, float dh,//矩形のサイズ（スクリーン座標系）
    float r, float g, float b, float a,//色
    float angle/*degree*/,
    float sx, float sy, float sw, float sh)//左上座標とサイズ
{
    //スクリーン（ビューポート）のサイズを取得する
    D3D11_VIEWPORT viewport{};
    UINT num_viewports{ 1 };
    immediate_context->RSGetViewports(&num_viewports, &viewport);

    //x,yがcx,cyを中心にangleで回転した時の座標を計算する関数
    auto rotate = [](float& x, float& y, float cx, float cy, float angle)
        {
            x -= cx;
            y -= cy;

            float cos{ cosf(DirectX::XMConvertToRadians(angle)) };
            float sin{ sinf(DirectX::XMConvertToRadians(angle)) };
            float tx{ x }, ty{ y };
            x = cos * tx + -sin * ty;
            y = sin * tx + cos * ty;

            x += cx;
            y += cy;
        };



    //renderメンバ関数の引数から矩形の各頂点の位置（スクリーン座標系）計算する
    //  (x0, y0) *----* (x1, y1)  
    //           |   /|
    //           |  / |
    //           | /  |
    //           |/   |
    //  (x2, y2) *----* (x3, y3) 

    //leftTop
    float x0{ dx };
    float y0{ dy };
    //rightTop
    float x1{ dx + dw };
    float y1{ dy };
    //leftBottom
    float x2{ dx };
    float y2{ dy + dh };
    //rightBottom
    float x3{ dx + dw };
    float y3{ dy + dh };

    //回転の中心を矩形の中心点にした場合の座標を求める
    float cx = dx + dw * 0.5f;
    float cy = dy + dh * 0.5f;
    rotate(x0, y0, cx, cy, angle);
    rotate(x1, y1, cx, cy, angle);
    rotate(x2, y2, cx, cy, angle);
    rotate(x3, y3, cx, cy, angle);

    //スクリーン座標からNDCへの座標変換を行う
    x0 = 2.0f * x0 / viewport.Width - 1.0f;
    y0 = 1.0f - 2.0f * y0 / viewport.Height;
    x1 = 2.0f * x1 / viewport.Width - 1.0f;
    y1 = 1.0f - 2.0f * y1 / viewport.Height;
    x2 = 2.0f * x2 / viewport.Width - 1.0f;
    y2 = 1.0f - 2.0f * y2 / viewport.Height;
    x3 = 2.0f * x3 / viewport.Width - 1.0f;
    y3 = 1.0f - 2.0f * y3 / viewport.Height;

    float u0{ sx / texture2dDesc.Width };
    float v0{ sy / texture2dDesc.Height };
    float u1{ (sx + sw) / texture2dDesc.Width };
    float v1{ (sy + sh) / texture2dDesc.Height };

    vertices.push_back({ {x0,y0,0},{r,g,b,a},{u0,v0} });
    vertices.push_back({ {x1,y1,0},{r,g,b,a},{u1,v0} });
    vertices.push_back({ {x2,y2,0},{r,g,b,a},{u0,v1} });
    vertices.push_back({ {x2,y2,0},{r,g,b,a},{u0,v1} });
    vertices.push_back({ {x1,y1,0},{r,g,b,a},{u1,v0} });
    vertices.push_back({ {x3,y3,0},{r,g,b,a},{u1,v1} });

    ////テクセル座標からUV座標系に変換
    //float textureWidth = texture2dDesc.Width;
    //float textureHeight = texture2dDesc.Height;
    //float u1 = sx / textureWidth;//テクスチャの左上
    //float v1 = sy / textureHeight;
    //float u2 = (sx + sw) / textureWidth;//テクスチャの右下
    //float v2 = (sy + sh) / textureHeight;

}

