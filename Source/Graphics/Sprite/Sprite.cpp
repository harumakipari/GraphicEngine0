#include "pch.h"
#include "Sprite.h"
#include <sstream>
//画像ファイルのロード
#include <WICTextureLoader.h>

//モジュール化する
#include "Engine/Utility/Win32Utils.h"
#include "Graphics/Core/Graphics.h"
#include "Graphics/Resource/Texture.h"
#include "Graphics/Core/Shader.h"

Sprite::Sprite(ID3D11Device* device, const wchar_t* filename)
{
    HRESULT hr{ S_OK };

    //頂点情報
    Vertex vertices[]
    {
        { { -0.5, +0.5, 0 }, { 1, 1, 1, 1 },{0,0} },
        { { +0.5, +0.5, 0 }, { 1, 0, 0, 1 },{1,0} },
        { { -0.5, -0.5, 0 }, { 0, 1, 0, 1 },{0,1} },
        { { +0.5, -0.5, 0 }, { 0, 0, 1, 1 },{1,1} },
    };

    //頂点バッファオブジェクトを生成
    {
        D3D11_BUFFER_DESC buffer_desc{};
        buffer_desc.ByteWidth = sizeof(vertices);
        //buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        //buffer_desc.CPUAccessFlags = 0;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;
        D3D11_SUBRESOURCE_DATA subresource_data{};
        subresource_data.pSysMem = vertices;
        subresource_data.SysMemPitch = 0;
        subresource_data.SysMemSlicePitch = 0;
        hr = device->CreateBuffer(&buffer_desc, &subresource_data, vertex_buffer.GetAddressOf());
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
    hr = CreateVsFromCSO(device, "./Data/Shaders/sprite_vs.cso", vertex_shader.ReleaseAndGetAddressOf(), input_layout.ReleaseAndGetAddressOf(), input_element_desc, _countof(input_element_desc));
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    //ピクセルシェーダーオブジェクトを生成
    hr = CreatePsFromCSO(device, "./Data/Shaders/sprite_ps.cso", pixel_shader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    //テクスチャのロード
    hr = LoadTextureFromFile(device, filename, shaderResourceView.ReleaseAndGetAddressOf(), &texture2dDesc);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


#if 0//以前の
    //頂点シェーダーオブジェクトを生成
    {
        const char* cso_name{ "sprite_vs.cso" };

        FILE* fp{};
        fopen_s(&fp, cso_name, "rb");
        _ASSERT_EXPR_A(fp, "CSO File not found");

        fseek(fp, 0, SEEK_END);
        long cso_sz{ ftell(fp) };
        fseek(fp, 0, SEEK_SET);

        std::unique_ptr<unsigned char[]> cso_data{ std::make_unique<unsigned char[]>(cso_sz) };
        fread(cso_data.get(), cso_sz, 1, fp);
        fclose(fp);
        hr = device->CreateVertexShader(cso_data.get(), cso_sz, nullptr, &vertex_shader);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        //HRESULT hr{ S_OK };
        hr = device->CreateInputLayout(input_element_desc, _countof(input_element_desc),
            cso_data.get(), cso_sz, &input_layout);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    //ピクセルシェーダーオブジェクトの生成
    {
        const char* cso_name{ "sprite_ps.cso" };

        FILE* fp{};
        fopen_s(&fp, cso_name, "rb");
        _ASSERT_EXPR_A(fp, "CSO File not found");

        fseek(fp, 0, SEEK_END);
        long cso_sz{ ftell(fp) };
        fseek(fp, 0, SEEK_SET);

        std::unique_ptr<unsigned char[]> cso_data{ std::make_unique<unsigned char[]>(cso_sz) };
        fread(cso_data.get(), cso_sz, 1, fp);
        fclose(fp);
        hr = device->CreatePixelShader(cso_data.get(), cso_sz, nullptr, &pixel_shader);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    //画像ファイルのロードとシェーダーリソースビューオブジェクト生成
    ID3D11Resource* resource{};
    hr = DirectX::CreateWICTextureFromFile(device, filename, &resource, &shaderResourceView);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    resource->Release();

    //テクスチャ情報(D3D11_TEXTURE2D_DESC)の取得
    ID3D11Texture2D* texture2d{};
    hr = resource->QueryInterface<ID3D11Texture2D>(&texture2d);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    texture2d->GetDesc(&texture2dDesc);
    texture2d->Release();
#endif
}

Sprite::~Sprite()
{
}

void Sprite::Render(ID3D11DeviceContext* immediate_context,
    float dx, float dy,//矩形の左上の座標（スクリーン座標系)
    float dw, float dh,//矩形のサイズ（スクリーン座標系）
    float r, float g, float b, float a,//色
    float angle/*degree*/)
{
    Render(immediate_context, dx, dy, dw, dh, r, g, b, a, angle, 0, 0, static_cast<float>(texture2dDesc.Width), static_cast<float>(texture2dDesc.Height));
}

void Sprite::Render(ID3D11DeviceContext* immediate_context,
    float dx, float dy,//矩形の左上の座標（スクリーン座標系)
    float dw, float dh)//矩形のサイズ（スクリーン座標系）
{
    Render(immediate_context, dx, dy, dw, dh, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0, 0, static_cast<float>(texture2dDesc.Width), static_cast<float>(texture2dDesc.Height));
}

void Sprite::Render(ID3D11DeviceContext* immediate_context,
    float dx, float dy,
    float dw, float dh,
    DirectX::XMFLOAT2 pivot,
    DirectX::XMFLOAT4 color,
    float sx, float sy,
    float sw, float sh
)
{
    //スクリーン（ビューポート）のサイズを取得する
#if 0
    D3D11_VIEWPORT viewport{};
    UINT num_viewports{ 1 };
    immediate_context->RSGetViewports(&num_viewports, &viewport);

    float viewportWidth, viewportHeight;
    viewportWidth = viewport.Width;
    viewportHeight = viewport.Height;


#else
    float viewX, viewY, viewportWidth, viewportHeight;
    Graphics::GetViewport(viewX, viewY, viewportWidth, viewportHeight);

    // =============================
    // UI 論理解像度 → 実解像度スケール
    // =============================
#ifdef _DEBUG
    constexpr float DESIGN_W = 1920.0f;
    constexpr float DESIGN_H = 1080.0f;
#else
    constexpr float DESIGN_W = 1920.0f;
    constexpr float DESIGN_H = 1080.0f;
#endif

    float scaleX_ui = viewportWidth / DESIGN_W;
    float scaleY_ui = viewportHeight / DESIGN_H;
    float scale_ui = std::min<float>(scaleX_ui, scaleY_ui);

    // dx, dy, dw, dh をスケーリング
    dx *= scale_ui;
    dy *= scale_ui;
    dw *= scale_ui;
    dh *= scale_ui;
#endif // 0

    //引数から矩形の各頂点の位置（スクリーン座標系）を計算する
    // left-top
    float x0{ dx - dw * pivot.x };
    float y0{ dy - dh * pivot.y };
    // right-top
    float x1{ dx + dw * (1.f - pivot.x) };
    float y1{ dy - dh * pivot.y };
    // left-bottom
    float x2{ dx - dw * pivot.x };
    float y2{ dy + dh * (1.f - pivot.y) };
    // right-bottom
    float x3{ dx + dw * (1.f - pivot.x) };
    float y3{ dy + dh * (1.f - pivot.y) };

    //切り取り位置
    //left-top
    float tx0{ sx };
    float ty0{ sy };
    //right-top
    float tx1{ sx + sw };
    float ty1{ sy };
    //left-bottom
    float tx2{ sx };
    float ty2{ sy + sh };
    //right-bottom
    float tx3{ sx + sw };
    float ty3{ sy + sh };

    //スクリーン座標系からNDCへの座標変換を行う
    x0 = 2.0f * x0 / viewportWidth - 1.0f;
    y0 = 1.0f - 2.0f * y0 / viewportHeight;
    x1 = 2.0f * x1 / viewportWidth - 1.0f;
    y1 = 1.0f - 2.0f * y1 / viewportHeight;
    x2 = 2.0f * x2 / viewportWidth - 1.0f;
    y2 = 1.0f - 2.0f * y2 / viewportHeight;
    x3 = 2.0f * x3 / viewportWidth - 1.0f;
    y3 = 1.0f - 2.0f * y3 / viewportHeight;

    //計算結果で頂点バッファオブジェクトを更新する
    HRESULT hr{ S_OK };
    D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
    hr = immediate_context->Map(vertex_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    Vertex* vertices{ reinterpret_cast<Vertex*>(mapped_subresource.pData) };
    if (vertices != nullptr)
    {
        vertices[0].position = { x0, y0, 0 };
        vertices[1].position = { x1, y1, 0 };
        vertices[2].position = { x2, y2, 0 };
        vertices[3].position = { x3, y3, 0 };
        vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = { color.x,color.y,color.z,color.w };

        vertices[0].texCoord = { tx0 / texture2dDesc.Width,ty0 / texture2dDesc.Height };
        vertices[1].texCoord = { tx1 / texture2dDesc.Width,ty1 / texture2dDesc.Height };
        vertices[2].texCoord = { tx2 / texture2dDesc.Width,ty2 / texture2dDesc.Height };
        vertices[3].texCoord = { tx3 / texture2dDesc.Width,ty3 / texture2dDesc.Height };
    }
    immediate_context->Unmap(vertex_buffer.Get(), 0);

    if (shaderResourceView.Get() != nullptr) {
        //シェーダーリソースのバインド
        immediate_context->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());
    }

    //頂点バッファのバインド
    UINT stride{ sizeof(Vertex) };
    UINT offset{ 0 };
    immediate_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);

    //プリミティブタイプおよびデータの順序に関する情報のバインド
    immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    //入力レイアウトオブジェクトのバインド
    immediate_context->IASetInputLayout(input_layout.Get());

    //シェーダーのバインド
    immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
    immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);

    //プリミティブの描画
    immediate_context->Draw(4, 0);
}
inline void Rotate(float& x, float& y, float cx, float cy, float angle)
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
}

void Sprite::Render(ID3D11DeviceContext* immediate_context,
    float dx, float dy,
    float dw, float dh,
    DirectX::XMFLOAT4 color,
    float angle,
    float sx, float sy,
    float sw, float sh,
    float scaleX, float scaleY,
    float pivotX, float pivotY)
{
    //スクリーン（ビューポート）のサイズを取得する
#if 0
    D3D11_VIEWPORT viewport{};
    UINT num_viewports{ 1 };
    immediate_context->RSGetViewports(&num_viewports, &viewport);

    float viewportWidth, viewportHeight;
    viewportWidth = viewport.Width;
    viewportHeight = viewport.Height;
#else
    float viewX, viewY, viewportWidth, viewportHeight;
    Graphics::GetViewport(viewX, viewY, viewportWidth, viewportHeight);
 //   Logger::Log(U8("UI Render viewport ") + std::to_string(viewX) + std::to_string(viewY) + std::to_string(viewportWidth) + std::to_string(viewportHeight));
    if (viewportWidth <= 1.0f || viewportHeight <= 1.0f)
    {
        Logger::Log(U8("UI skipped: invalid viewport"));
        return;
    }
    // =============================
    // UI 論理解像度 → 実解像度スケール
    // =============================
#ifdef _DEBUG
    constexpr float DESIGN_W = 1920.0f;
    constexpr float DESIGN_H = 1080.0f;
#else
    constexpr float DESIGN_W = 1920.0f;
    constexpr float DESIGN_H = 1080.0f;
#endif

    float scaleX_ui = viewportWidth / DESIGN_W;
    float scaleY_ui = viewportHeight / DESIGN_H;
    float scale_ui = std::min<float>(scaleX_ui, scaleY_ui);
    // dx, dy, dw, dh をスケーリング
    dx *= scale_ui;
    dy *= scale_ui;
    dw *= scale_ui;
    dh *= scale_ui;

  //  Logger::Log(U8("scale_ui = %f  dx=%f dy=%f") + std::to_string(scale_ui) + std::to_string(dx) + std::to_string(dh));

#endif // 0

    // 無効なスケーリングなら描画しない
    if (scaleX == 0.0f || scaleY == 0.0f) return;

    // sw, sh が負なら、テクスチャ全体を使う（FLT_MINの代替）
    if (sw < 0.0f && sh < 0.0f)
    {
        sw = static_cast<float>(texture2dDesc.Width);
        sh = static_cast<float>(texture2dDesc.Height);
    }

    // ピボット考慮した頂点座標
    float x0 = dx - pivotX * dw * scaleX;
    float y0 = dy - pivotY * dh * scaleY;
    float x1 = x0 + dw * scaleX;
    float y1 = y0;
    float x2 = x0;
    float y2 = y0 + dh * scaleY;
    float x3 = x0 + dw * scaleX;
    float y3 = y0 + dh * scaleY;

    // UV座標の計算（ピクセル → テクスチャ座標系）
    float texW = static_cast<float>(texture2dDesc.Width);
    float texH = static_cast<float>(texture2dDesc.Height);

    float tx0 = sx;
    float ty0 = sy;
    float tx1 = sx + sw;
    float ty1 = sy;
    float tx2 = sx;
    float ty2 = sy + sh;
    float tx3 = sx + sw;
    float ty3 = sy + sh;

    // 回転中心は描画位置(dx, dy)
    float cx = dx;
    float cy = dy;

    // 回転処理（Rotate関数がラジアン想定とする）
    Rotate(x0, y0, cx, cy, angle);
    Rotate(x1, y1, cx, cy, angle);
    Rotate(x2, y2, cx, cy, angle);
    Rotate(x3, y3, cx, cy, angle);

    // スクリーン座標系 → NDC座標系
    auto toNDC = [&](float& x, float& y) {
        x = 2.0f * x / viewportWidth - 1.0f;
        y = 1.0f - 2.0f * y / viewportHeight;
        };
    toNDC(x0, y0); toNDC(x1, y1); toNDC(x2, y2); toNDC(x3, y3);

    // 頂点バッファ更新
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = immediate_context->Map(vertex_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        Vertex* vertices = reinterpret_cast<Vertex*>(mapped.pData);
        if (vertices)
        {
            vertices[0].position = { x0, y0, 0 };
            vertices[1].position = { x1, y1, 0 };
            vertices[2].position = { x2, y2, 0 };
            vertices[3].position = { x3, y3, 0 };

            vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = { color.x, color.y, color.z, color.w };

            vertices[0].texCoord = { tx0 / texW, ty0 / texH };
            vertices[1].texCoord = { tx1 / texW, ty1 / texH };
            vertices[2].texCoord = { tx2 / texW, ty2 / texH };
            vertices[3].texCoord = { tx3 / texW, ty3 / texH };
        }
        immediate_context->Unmap(vertex_buffer.Get(), 0);
    }

    // テクスチャが無い場合は描画しない
    if (!shaderResourceView) return;

    // リソース・パイプライン設定
    immediate_context->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    immediate_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
    immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    immediate_context->IASetInputLayout(input_layout.Get());

    immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
    immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);

    immediate_context->Draw(4, 0);
}


void Sprite::Render(ID3D11DeviceContext* immediate_context,
    float dx, float dy,//矩形の左上の座標（スクリーン座標系)
    float dw, float dh,//矩形のサイズ（スクリーン座標系）
    float r, float g, float b, float a,//色
    float angle/*degree*/,
    float sx, float sy, float sw, float sh)//左上座標とサイズ
{
    //スクリーン（ビューポート）のサイズを取得する
#if 0
    D3D11_VIEWPORT viewport{};
    UINT num_viewports{ 1 };
    immediate_context->RSGetViewports(&num_viewports, &viewport);
    float viewportWidth, viewportHeight;
    viewportWidth = viewport.Width;
    viewportHeight = viewport.Height;

#else
    float viewX, viewY, viewportWidth, viewportHeight;
    Graphics::GetViewport(viewX, viewY, viewportWidth, viewportHeight);
    // =============================
    // UI 論理解像度 → 実解像度スケール
    // =============================

#ifdef _DEBUG
    constexpr float DESIGN_W = 1920.0f;
    constexpr float DESIGN_H = 1080.0f;
#else
    constexpr float DESIGN_W = 1920.0f;
    constexpr float DESIGN_H = 1080.0f;
#endif

    float scaleX_ui = viewportWidth / DESIGN_W;
    float scaleY_ui = viewportHeight / DESIGN_H;
    float scale_ui = std::min<float>(scaleX_ui, scaleY_ui);
    scale_ui = 1.0f;

    // dx, dy, dw, dh をスケーリング
    dx *= scale_ui;
    dy *= scale_ui;
    dw *= scale_ui;
    dh *= scale_ui;
#endif // 0

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
    x0 = 2.0f * x0 / viewportWidth - 1.0f;
    y0 = 1.0f - 2.0f * y0 / viewportHeight;
    x1 = 2.0f * x1 / viewportWidth - 1.0f;
    y1 = 1.0f - 2.0f * y1 / viewportHeight;
    x2 = 2.0f * x2 / viewportWidth - 1.0f;
    y2 = 1.0f - 2.0f * y2 / viewportHeight;
    x3 = 2.0f * x3 / viewportWidth - 1.0f;
    y3 = 1.0f - 2.0f * y3 / viewportHeight;

    //テクセル座標からUV座標系に変換
    float textureWidth = static_cast<float>(texture2dDesc.Width);
    float textureHeight = static_cast<float>(texture2dDesc.Height);
    float u1 = sx / textureWidth;//テクスチャの左上
    float v1 = sy / textureHeight;
    float u2 = (sx + sw) / textureWidth;//テクスチャの右下
    float v2 = (sy + sh) / textureHeight;

    //計算結果で頂点バッファオブジェクトを更新する
    HRESULT hr{ S_OK };
    D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
    hr = immediate_context->Map(vertex_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
        0, &mapped_subresource);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


    Vertex* vertices{ reinterpret_cast<Vertex*>(mapped_subresource.pData) };
    if (vertices != nullptr)
    {
        vertices[0].position = { x0,y0,0 };
        vertices[2].position = { x1,y1,0 };
        vertices[1].position = { x2,y2,0 };
        vertices[3].position = { x3,y3,0 };
        vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = { r,g,b,a };

        vertices[0].texCoord = { u1,v1 };
        vertices[1].texCoord = { u1,v2 };
        vertices[2].texCoord = { u2,v1 };
        vertices[3].texCoord = { u2,v2 };
    }

    immediate_context->Unmap(vertex_buffer.Get(), 0);


    //頂点バッファのバインド(接続）
    UINT stride{ sizeof(Vertex) };
    UINT offset{ 0 };
    immediate_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);

    //プリミティブタイプおよびデータの順序に関する情報のバインド
    immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    //入力レイアウトオブジェクトのバインド
    immediate_context->IASetInputLayout(input_layout.Get());

    //シェーダーのバインド
    immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
    immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);

    //シェーダーリソースのバインド
    immediate_context->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());

    //プリミティブの描画
    immediate_context->Draw(4, 0);

}

void Sprite::SoftRender(ID3D11DeviceContext* immediate_context,
    DirectX::XMFLOAT2 leftTop, DirectX::XMFLOAT2 rightTop, DirectX::XMFLOAT2 leftBottom, DirectX::XMFLOAT2 rightBottom,
    float sx, float sy, float sw, float sh, float r, float g, float b, float a)//左上座標とサイズ
{
    //スクリーン（ビューポート）のサイズを取得する
    D3D11_VIEWPORT viewport{};
    UINT num_viewports{ 1 };
    immediate_context->RSGetViewports(&num_viewports, &viewport);

    //leftTop
    float x0{ leftTop.x };
    float y0{ leftTop.y };
    //rightTop
    float x1{ rightTop.x };
    float y1{ rightTop.y };
    //leftBottom
    float x2{ leftBottom.x };
    float y2{ leftBottom.y };
    //rightBottom
    float x3{ rightBottom.x };
    float y3{ rightBottom.y };

    //スクリーン座標からNDCへの座標変換を行う
    x0 = 2.0f * x0 / viewport.Width - 1.0f;
    y0 = 1.0f - 2.0f * y0 / viewport.Height;
    x1 = 2.0f * x1 / viewport.Width - 1.0f;
    y1 = 1.0f - 2.0f * y1 / viewport.Height;
    x2 = 2.0f * x2 / viewport.Width - 1.0f;
    y2 = 1.0f - 2.0f * y2 / viewport.Height;
    x3 = 2.0f * x3 / viewport.Width - 1.0f;
    y3 = 1.0f - 2.0f * y3 / viewport.Height;

    //テクセル座標からUV座標系に変換
    float textureWidth = static_cast<float>(texture2dDesc.Width);
    float textureHeight = static_cast<float>(texture2dDesc.Height);
    float u1 = sx / textureWidth;//テクスチャの左上
    float v1 = sy / textureHeight;
    float u2 = (sx + sw) / textureWidth;//テクスチャの右下
    float v2 = (sy + sh) / textureHeight;

    //計算結果で頂点バッファオブジェクトを更新する
    HRESULT hr{ S_OK };
    D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
    hr = immediate_context->Map(vertex_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
        0, &mapped_subresource);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    Vertex* vertices{ reinterpret_cast<Vertex*>(mapped_subresource.pData) };
    if (vertices != nullptr)
    {
        vertices[0].position = { x0,y0,0 };
        vertices[2].position = { x1,y1,0 };
        vertices[1].position = { x2,y2,0 };
        vertices[3].position = { x3,y3,0 };
        //vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = { r,g,b,a };
        vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = { r,g,b,a };

        vertices[0].texCoord = { u1,v1 };
        vertices[1].texCoord = { u1,v2 };
        vertices[2].texCoord = { u2,v1 };
        vertices[3].texCoord = { u2,v2 };
    }

    immediate_context->Unmap(vertex_buffer.Get(), 0);


    //頂点バッファのバインド(接続）
    UINT stride{ sizeof(Vertex) };
    UINT offset{ 0 };
    immediate_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);

    //プリミティブタイプおよびデータの順序に関する情報のバインド
    immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    //入力レイアウトオブジェクトのバインド
    immediate_context->IASetInputLayout(input_layout.Get());

    //シェーダーのバインド
    immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
    immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);

    //シェーダーリソースのバインド
    immediate_context->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());

    //プリミティブの描画
    immediate_context->Draw(4, 0);

}

//フォント画像ファイルを使用して任意の文字列を画面に出力する
void Sprite::Textout(ID3D11DeviceContext* immediateContext, std::string s,
    float x, float y, float w, float h, float r, float g, float b, float a)
{
    float sw = static_cast<float>(texture2dDesc.Width / 16);
    float sh = static_cast<float>(texture2dDesc.Height / 16);
    float carriage = 0;
    for (const char c : s)
    {
        Render(immediateContext, x + carriage, y, w, h, r, g, b, a, 0,
            sw * (c & 0x0F), sh * (c >> 4), sw, sh);
        carriage += w;
    }
}
