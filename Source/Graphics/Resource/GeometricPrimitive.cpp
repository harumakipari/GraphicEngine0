#include "pch.h"
#include "GeometricPrimitive.h"
#include <cmath>
#include "Graphics/Core/Shader.h"
#include "Graphics/Core/Graphics.h"
#include "Engine/Utility/Win32Utils.h"
GeometricPrimitive::GeometricPrimitive()
{
    HRESULT hr{ S_OK };
    ID3D11Device* device = Graphics::GetDevice();

    D3D11_INPUT_ELEMENT_DESC inputElementDesc[]
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0, D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
    };

    hr=CreateVsFromCSO(device, "./Data/Shaders/geometricPrimitiveVS.cso", vertexShader.GetAddressOf(),
        inputLayout.GetAddressOf(), inputElementDesc, ARRAYSIZE(inputElementDesc));
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr=CreatePsFromCSO(device, "./Data/Shaders/geometricPrimitivePS.cso", pixelShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(Constants);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&bufferDesc, nullptr, constantBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void GeometricPrimitive::CreateComBuffers(Vertex* vertices, size_t vertexCount,
    uint32_t* indices, size_t indexCount)
{
    HRESULT hr{ S_OK };
    ID3D11Device* device = Graphics::GetDevice();

    D3D11_BUFFER_DESC bufferDesc{};
    D3D11_SUBRESOURCE_DATA subsorceData{};
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertexCount);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;
    subsorceData.pSysMem = vertices;
    subsorceData.SysMemPitch = 0;
    subsorceData.SysMemSlicePitch = 0;
    hr = device->CreateBuffer(&bufferDesc, &subsorceData, vertexBuffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * indexCount);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    subsorceData.pSysMem = indices;
    hr = device->CreateBuffer(&bufferDesc, &subsorceData, indexBuffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void GeometricPrimitive::Render(DirectX::XMFLOAT4X4& world, DirectX::XMFLOAT4& materialColor,std::function<void()> callbackFunc)
{
    ID3D11DeviceContext* immediateContext = Graphics::GetDeviceContext();
    uint32_t stride{ sizeof(Vertex) };
    uint32_t offset{ 0 };
    immediateContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    immediateContext->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    immediateContext->IASetInputLayout(inputLayout.Get());

    immediateContext->VSSetShader(vertexShader.Get(), nullptr, 0);
    immediateContext->PSSetShader(pixelShader.Get(), nullptr, 0);

    if (callbackFunc)
    {// 
        callbackFunc();
    }

    Constants data{ world,materialColor };
    immediateContext->UpdateSubresource(constantBuffer.Get(), 0, 0, &data, 0, 0);
    immediateContext->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

    D3D11_BUFFER_DESC bufferDesc{};
    indexBuffer->GetDesc(&bufferDesc);
    immediateContext->DrawIndexed(bufferDesc.ByteWidth / sizeof(uint32_t), 0, 0);
}

GeometricCube::GeometricCube() 
{
    Vertex vertices[24]{};
    //サイズが1.0の正立方体のデータを作成する（重心を原点にする）。
    // 正立方体のコントロールポイントは8個
    //前面
    vertices[0].position = { -0.5f,0.5f,-0.5f };    //右上
    vertices[0].normal = { 0,0,-1 };
    vertices[1].position = { 0.5f,0.5f,-0.5f };     //左上
    vertices[1].normal = { 0,0,-1 };
    vertices[2].position = { -0.5f,-0.5f,-0.5f };   //左下
    vertices[2].normal = { 0,0,-1 };
    vertices[3].position = { 0.5f,-0.5f,-0.5f };    //右下
    vertices[3].normal = { 0,0,-1 };
    //上面
    vertices[4].position = { -0.5f,0.5f,0.5f };     //左奥
    vertices[4].normal = { 0,1,0 };
    vertices[5].position = { 0.5f,0.5f,0.5f };      //右奥
    vertices[5].normal = { 0,1,0 };
    vertices[6].position = { -0.5f,0.5f,-0.5f };    //左手前
    vertices[6].normal = { 0,1,0 };
    vertices[7].position = { 0.5f,0.5f,-0.5f };     //右手前
    vertices[7].normal = { 0,1,0 };
    //右面
    vertices[8].position = { 0.5f,0.5f,0.5f };      //上奥
    vertices[8].normal = { 1,0,0 };
    vertices[9].position = { 0.5f,-0.5f,0.5f };     //下奥
    vertices[9].normal = { 1,0,0 };
    vertices[10].position = { 0.5f,0.5f,-0.5f };     //上手前
    vertices[10].normal = { 1,0,0 };
    vertices[11].position = { 0.5f,-0.5f,-0.5f };   //下手前
    vertices[11].normal = { 1,0,0 };
    //左面
    vertices[12].position = { -0.5f,0.5f,-0.5f };
    vertices[12].normal = { -1,0,0 };
    vertices[13].position = { -0.5f,0.5f,0.5f };
    vertices[13].normal = { -1,0,0 };
    vertices[14].position = { -0.5f,-0.5f,-0.5f };
    vertices[14].normal = { -1,0,0 };
    vertices[15].position = { -0.5f,-0.5f,0.5f };
    vertices[15].normal = { -1,0,0 };
    //底面
    vertices[16].position = { -0.5f,-0.5f,0.5f };
    vertices[16].normal = { 0,-1,0 };
    vertices[17].position = { 0.5f,-0.5f,0.5f };
    vertices[17].normal = { 0,-1,0 };
    vertices[18].position = { -0.5f,-0.5f,-0.5f };
    vertices[18].normal = { 0,-1,0 };
    vertices[19].position = { 0.5f,-0.5f,-0.5f };
    vertices[19].normal = { 0,-1,0 };
    //奥面
    vertices[20].position = { -0.5f,0.5f,0.5f };
    vertices[20].normal = { 0,0,1 };
    vertices[21].position = { 0.5f,0.5f,0.5f };
    vertices[21].normal = { 0,0,1 };
    vertices[22].position = { -0.5f,-0.5f,0.5f };
    vertices[22].normal = { 0,0,1 };
    vertices[23].position = { 0.5f,-0.5f,0.5f };
    vertices[23].normal = { 0,0,1 };


    // 1つのコントロールポイントの位置には法線の向きが違う頂点が３つあるため、頂点情報の
    // 総数は8x3の24個、頂点情報配列(vertices)に全ての頂点の位置、法線情報を格納する。

    uint32_t indices[36]{};

    //前面　左
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    //前面　右
    indices[3] = 1;
    indices[4] = 3;
    indices[5] = 2;
    //上面　左
    indices[6] = 4;
    indices[7] = 5;
    indices[8] = 6;
    //上面　右
    indices[9] = 5;
    indices[10] = 7;
    indices[11] = 6;
    //右面　手前
    indices[12] = 10;
    indices[13] = 8;
    indices[14] = 11;
    //右面　奥
    indices[15] = 8;
    indices[16] = 9;
    indices[17] = 11;
    //左面  手前
    indices[18] = 13;
    indices[19] = 12;
    indices[20] = 14;
    //左面  奥
    indices[21] = 13;
    indices[22] = 14;
    indices[23] = 15;
    //底面　左
    indices[24] = 16;
    indices[25] = 18;
    indices[26] = 17;
    //底面　右
    indices[27] = 17;
    indices[28] = 18;
    indices[29] = 19;
    //奥面　左
    indices[30] = 20;
    indices[31] = 22;
    indices[32] = 21;
    //奥面　右
    indices[33] = 21;
    indices[34] = 22;
    indices[35] = 23;

    // 正立方体は6面持ち、1つの面は2つの3角形ポリゴンで構成されるので3角形ポリゴンの総数は6x2=12個、 
    // 正立方体を描画するために12回の3角形ポリゴン描画が必要、よって参照される頂点情報は12x3=36回、 
    // 3角形ポリゴンが参照する頂点情報のインデックス（頂点番号）を描画順に配列（indices）に格納する。 
    // 時計回りが表面になるように格納すること。

    CreateComBuffers(vertices, 24, indices, 36);
}

#include <vector>
GeometricCylinder::GeometricCylinder( uint32_t slices) 
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float d{ 2.0f * DirectX::XM_PI / slices };
    float r{ 0.5f };

    Vertex vertex{};
    uint32_t base_index{ 0 };

    // top cap centre
    vertex.position = { 0.0f, +0.5f, 0.0f };
    vertex.normal = { 0.0f, +1.0f, 0.0f };
    vertices.emplace_back(vertex);
    // top cap ring
    for (uint32_t i = 0; i < slices; ++i)
    {
        float x{ r * cosf(i * d) };
        float z{ r * sinf(i * d) };
        vertex.position = { x, +0.5f, z };
        vertex.normal = { 0.0f, +1.0f, 0.0f };
        vertices.emplace_back(vertex);
    }
    base_index = 0;
    for (uint32_t i = 0; i < slices - 1; ++i)
    {
        indices.emplace_back(base_index + 0);
        indices.emplace_back(base_index + i + 2);
        indices.emplace_back(base_index + i + 1);
    }
    indices.emplace_back(base_index + 0);
    indices.emplace_back(base_index + 1);
    indices.emplace_back(base_index + slices);

    // bottom cap centre
    vertex.position = { 0.0f, -0.5f, 0.0f };
    vertex.normal = { 0.0f, -1.0f, 0.0f };
    vertices.emplace_back(vertex);
    // bottom cap ring
    for (uint32_t i = 0; i < slices; ++i)
    {
        float x = r * cosf(i * d);
        float z = r * sinf(i * d);
        vertex.position = { x, -0.5f, z };
        vertex.normal = { 0.0f, -1.0f, 0.0f };
        vertices.emplace_back(vertex);
    }
    base_index = slices + 1;
    for (uint32_t i = 0; i < slices - 1; ++i)
    {
        indices.emplace_back(base_index + 0);
        indices.emplace_back(base_index + i + 1);
        indices.emplace_back(base_index + i + 2);
    }
    indices.emplace_back(base_index + 0);
    indices.emplace_back(base_index + (slices - 1) + 1);
    indices.emplace_back(base_index + (0) + 1);

    // side rectangle
    for (uint32_t i = 0; i < slices; ++i)
    {
        float x = r * cosf(i * d);
        float z = r * sinf(i * d);

        vertex.position = { x, +0.5f, z };
        vertex.normal = { x, 0.0f, z };
        vertices.emplace_back(vertex);

        vertex.position = { x, -0.5f, z };
        vertex.normal = { x, 0.0f, z };
        vertices.emplace_back(vertex);
    }
    base_index = slices * 2 + 2;
    for (uint32_t i = 0; i < slices - 1; ++i)
    {
        indices.emplace_back(base_index + i * 2 + 0);
        indices.emplace_back(base_index + i * 2 + 2);
        indices.emplace_back(base_index + i * 2 + 1);

        indices.emplace_back(base_index + i * 2 + 1);
        indices.emplace_back(base_index + i * 2 + 2);
        indices.emplace_back(base_index + i * 2 + 3);
    }
    indices.emplace_back(base_index + (slices - 1) * 2 + 0);
    indices.emplace_back(base_index + (0) * 2 + 0);
    indices.emplace_back(base_index + (slices - 1) * 2 + 1);

    indices.emplace_back(base_index + (slices - 1) * 2 + 1);
    indices.emplace_back(base_index + (0) * 2 + 0);
    indices.emplace_back(base_index + (0) * 2 + 1);

    CreateComBuffers(vertices.data(), vertices.size(), indices.data(), indices.size());
}

//slices水平方向の分割数     stacks垂直方向の分割数
GeometricSphere::GeometricSphere(DirectX::XMFLOAT3 center, float radius, uint32_t slices, uint32_t stacks) 
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float r{ radius };

    //
    // Compute the vertices stating at the top pole and moving down the stacks.
    //

    // Poles: note that there will be texture coordinate distortion as there is
    // not a unique point on the texture map to assign to the pole when mapping
    // a rectangular texture onto a sphere.
    Vertex top_vertex{};
    top_vertex.position = { center.x, center.y + r, center.z };
    top_vertex.normal = { 0.0f, +1.0f, 0.0f };

    Vertex bottom_vertex{};
    bottom_vertex.position = { center.x,center.y -r, center.z };
    bottom_vertex.normal = { 0.0f, -1.0f, 0.0f };

    vertices.emplace_back(top_vertex);

    float phi_step{ DirectX::XM_PI / stacks };
    float theta_step{ 2.0f * DirectX::XM_PI / slices };

    // Compute vertices for each stack ring (do not count the poles as rings).
    for (uint32_t i = 1; i <= stacks - 1; ++i)
    {
        float phi{ i * phi_step };

        // Vertices of ring.
        for (uint32_t j = 0; j <= slices; ++j)
        {
            float theta{ j * theta_step };

            Vertex v{};

            // 球面座標系からデカルト座標系へ変換
            v.position.x = center.x + r * sinf(phi) * cosf(theta);
            v.position.y = center.y + r * cosf(phi);
            v.position.z = center.z + r * sinf(phi) * sinf(theta);

            DirectX::XMVECTOR p{ XMLoadFloat3(&v.position) };
            DirectX::XMStoreFloat3(&v.normal, DirectX::XMVector3Normalize(p));

            vertices.emplace_back(v);
        }
    }

    vertices.emplace_back(bottom_vertex);

    //
    // Compute indices for top stack.  The top stack was written first to the vertex buffer
    // and connects the top pole to the first ring.
    //
    for (uint32_t i = 1; i <= slices; ++i)
    {
        indices.emplace_back(0);
        indices.emplace_back(i + 1);
        indices.emplace_back(i);
    }

    //
    // Compute indices for inner stacks (not connected to poles).
    //

    // Offset the indices to the index of the first vertex in the first ring.
    // This is just skipping the top pole vertex.
    uint32_t base_index{ 1 };
    uint32_t ring_vertex_count{ slices + 1 };
    for (uint32_t i = 0; i < stacks - 2; ++i)
    {
        for (uint32_t j = 0; j < slices; ++j)
        {
            indices.emplace_back(base_index + i * ring_vertex_count + j);
            indices.emplace_back(base_index + i * ring_vertex_count + j + 1);
            indices.emplace_back(base_index + (i + 1) * ring_vertex_count + j);

            indices.emplace_back(base_index + (i + 1) * ring_vertex_count + j);
            indices.emplace_back(base_index + i * ring_vertex_count + j + 1);
            indices.emplace_back(base_index + (i + 1) * ring_vertex_count + j + 1);
        }
    }

    //
    // Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
    // and connects the bottom pole to the bottom ring.
    //

    // South pole vertex was added last.
    uint32_t south_pole_index{ static_cast<uint32_t>(vertices.size() - 1) };

    // Offset the indices to the index of the first vertex in the last ring.
    base_index = south_pole_index - ring_vertex_count;

    for (uint32_t i = 0; i < slices; ++i)
    {
        indices.emplace_back(south_pole_index);
        indices.emplace_back(base_index + i);
        indices.emplace_back(base_index + i + 1);
    }
    CreateComBuffers(vertices.data(), vertices.size(), indices.data(), indices.size());
}

//slices水平方向の分割数     stacks垂直方向の分割数
GeometricSphere::GeometricSphere(float radius, uint32_t slices, uint32_t stacks) 
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float r{ radius };

    //
    // Compute the vertices stating at the top pole and moving down the stacks.
    //

    // Poles: note that there will be texture coordinate distortion as there is
    // not a unique point on the texture map to assign to the pole when mapping
    // a rectangular texture onto a sphere.
    Vertex top_vertex{};
    top_vertex.position = { 0,  r, 0 };
    top_vertex.normal = { 0.0f, +1.0f, 0.0f };

    Vertex bottom_vertex{};
    bottom_vertex.position = { 0,r, 0 };
    bottom_vertex.normal = { 0.0f, -1.0f, 0.0f };

    vertices.emplace_back(top_vertex);

    float phi_step{ DirectX::XM_PI / stacks };
    float theta_step{ 2.0f * DirectX::XM_PI / slices };

    // Compute vertices for each stack ring (do not count the poles as rings).
    for (uint32_t i = 1; i <= stacks - 1; ++i)
    {
        float phi{ i * phi_step };

        // Vertices of ring.
        for (uint32_t j = 0; j <= slices; ++j)
        {
            float theta{ j * theta_step };

            Vertex v{};

            // 球面座標系からデカルト座標系へ変換
            v.position.x =  r * sinf(phi) * cosf(theta);
            v.position.y =  r * cosf(phi);
            v.position.z =  r * sinf(phi) * sinf(theta);

            DirectX::XMVECTOR p{ XMLoadFloat3(&v.position) };
            DirectX::XMStoreFloat3(&v.normal, DirectX::XMVector3Normalize(p));

            vertices.emplace_back(v);
        }
    }

    vertices.emplace_back(bottom_vertex);

    //
    // Compute indices for top stack.  The top stack was written first to the vertex buffer
    // and connects the top pole to the first ring.
    //
    for (uint32_t i = 1; i <= slices; ++i)
    {
        indices.emplace_back(0);
        indices.emplace_back(i + 1);
        indices.emplace_back(i);
    }

    //
    // Compute indices for inner stacks (not connected to poles).
    //

    // Offset the indices to the index of the first vertex in the first ring.
    // This is just skipping the top pole vertex.
    uint32_t base_index{ 1 };
    uint32_t ring_vertex_count{ slices + 1 };
    for (uint32_t i = 0; i < stacks - 2; ++i)
    {
        for (uint32_t j = 0; j < slices; ++j)
        {
            indices.emplace_back(base_index + i * ring_vertex_count + j);
            indices.emplace_back(base_index + i * ring_vertex_count + j + 1);
            indices.emplace_back(base_index + (i + 1) * ring_vertex_count + j);

            indices.emplace_back(base_index + (i + 1) * ring_vertex_count + j);
            indices.emplace_back(base_index + i * ring_vertex_count + j + 1);
            indices.emplace_back(base_index + (i + 1) * ring_vertex_count + j + 1);
        }
    }

    //
    // Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
    // and connects the bottom pole to the bottom ring.
    //

    // South pole vertex was added last.
    uint32_t south_pole_index{ static_cast<uint32_t>(vertices.size() - 1) };

    // Offset the indices to the index of the first vertex in the last ring.
    base_index = south_pole_index - ring_vertex_count;

    for (uint32_t i = 0; i < slices; ++i)
    {
        indices.emplace_back(south_pole_index);
        indices.emplace_back(base_index + i);
        indices.emplace_back(base_index + i + 1);
    }
    CreateComBuffers(vertices.data(), vertices.size(), indices.data(), indices.size());
}

// UNIT.12
GeometricCapsule::GeometricCapsule(float mantle_height, const DirectX::XMFLOAT3& radius, uint32_t slices, uint32_t ellipsoid_stacks, uint32_t mantle_stacks) 
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const int base_offset = 0;

    slices = std::max<uint32_t>(3u, slices);
    mantle_stacks = std::max<uint32_t>(1u, mantle_stacks);
    ellipsoid_stacks = std::max<uint32_t>(2u, ellipsoid_stacks);

    const float inv_slices = 1.0f / static_cast<float>(slices);
    const float inv_mantle_stacks = 1.0f / static_cast<float>(mantle_stacks);
    const float inv_ellipsoid_stacks = 1.0f / static_cast<float>(ellipsoid_stacks);

    const float pi_2{ 3.14159265358979f * 2.0f };
    const float pi_0_5{ 3.14159265358979f * 0.5f };
    const float angle_steps = inv_slices * pi_2;
    const float half_height = mantle_height * 0.5f;

    /* Generate mantle vertices */
    struct spherical {
        float radius, theta, phi;
    } point{ 1, 0, 0 };
    DirectX::XMFLOAT3 position, normal;
    DirectX::XMFLOAT2 texcoord;

    float angle = 0.0f;
    for (uint32_t u = 0; u <= slices; ++u)
    {
        /* Compute X- and Z coordinates */
        texcoord.x = sinf(angle);
        texcoord.y = cosf(angle);

        position.x = texcoord.x * radius.x;
        position.z = texcoord.y * radius.z;

        /* Compute normal vector */
        normal.x = texcoord.x;
        normal.y = 0;
        normal.z = texcoord.y;

        float magnitude = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        normal.x = normal.x / magnitude;
        normal.y = normal.y / magnitude;
        normal.z = normal.z / magnitude;

        /* Add top and bottom vertex */
        texcoord.x = static_cast<float>(slices - u) * inv_slices;

        for (uint32_t v = 0; v <= mantle_stacks; ++v)
        {
            texcoord.y = static_cast<float>(v) * inv_mantle_stacks;
#if _HAS_CXX20
            position.y = std::lerp(half_height, -half_height, texcoord.y);
#else
            position.y = half_height * (1 - texcoord.y) + -half_height * texcoord.y;
#endif
            vertices.push_back({ position, normal });
        }

        /* Increase angle for the next iteration */
        angle += angle_steps;
    }

    /* Generate bottom and top cover vertices */
    const float cover_side[2] = { 1, -1 };
    uint32_t base_offset_ellipsoid[2] = { 0 };
    for (size_t i = 0; i < 2; ++i)
    {
        base_offset_ellipsoid[i] = static_cast<uint32_t>(vertices.size());

        for (uint32_t v = 0; v <= ellipsoid_stacks; ++v)
        {
            /* Compute theta of spherical coordinate */
            texcoord.y = static_cast<float>(v) * inv_ellipsoid_stacks;
            point.theta = texcoord.y * pi_0_5;

            for (uint32_t u = 0; u <= slices; ++u)
            {
                /* Compute phi of spherical coordinate */
                texcoord.x = static_cast<float>(u) * inv_slices;
                point.phi = texcoord.x * pi_2 * cover_side[i] + pi_0_5;

                /* Convert spherical coordinate into cartesian coordinate and set normal by coordinate */
                const float sin_theta = sinf(point.theta);
                position.x = point.radius * cosf(point.phi) * sin_theta;
                position.y = point.radius * sinf(point.phi) * sin_theta;
                position.z = point.radius * cosf(point.theta);

                std::swap(position.y, position.z);
                position.y *= cover_side[i];

                /* Get normal and move half-sphere */
                float magnitude = sqrtf(position.x * position.x + position.y * position.y + position.z * position.z);
                normal.x = position.x / magnitude;
                normal.y = position.y / magnitude;
                normal.z = position.z / magnitude;

                /* Transform coordiante with radius and height */
                position.x *= radius.x;
                position.y *= radius.y;
                position.z *= radius.z;
                position.y += half_height * cover_side[i];

                //TODO: texCoord wrong for bottom half-sphere!!!
                /* Add new vertex */
                vertices.push_back({ position, normal });
            }
        }
    }

    /* Generate indices for the mantle */
    int offset = base_offset;
    for (uint32_t u = 0; u < slices; ++u)
    {
        for (uint32_t v = 0; v < mantle_stacks; ++v)
        {
            auto i0 = v + 1 + mantle_stacks;
            auto i1 = v;
            auto i2 = v + 1;
            auto i3 = v + 2 + mantle_stacks;

            indices.emplace_back(i0 + offset);
            indices.emplace_back(i1 + offset);
            indices.emplace_back(i3 + offset);
            indices.emplace_back(i1 + offset);
            indices.emplace_back(i2 + offset);
            indices.emplace_back(i3 + offset);
        }
        offset += (1 + mantle_stacks);
    }

    /* Generate indices for the top and bottom */
    for (size_t i = 0; i < 2; ++i)
    {
        for (uint32_t v = 0; v < ellipsoid_stacks; ++v)
        {
            for (uint32_t u = 0; u < slices; ++u)
            {
                /* Compute indices for current face */
                auto i0 = v * (slices + 1) + u;
                auto i1 = v * (slices + 1) + (u + 1);

                auto i2 = (v + 1) * (slices + 1) + (u + 1);
                auto i3 = (v + 1) * (slices + 1) + u;

                /* Add new indices */
                indices.emplace_back(i0 + base_offset_ellipsoid[i]);
                indices.emplace_back(i1 + base_offset_ellipsoid[i]);
                indices.emplace_back(i3 + base_offset_ellipsoid[i]);
                indices.emplace_back(i1 + base_offset_ellipsoid[i]);
                indices.emplace_back(i2 + base_offset_ellipsoid[i]);
                indices.emplace_back(i3 + base_offset_ellipsoid[i]);
            }
        }
    }
    CreateComBuffers(vertices.data(), vertices.size(), indices.data(), indices.size());
}
