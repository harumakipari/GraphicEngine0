#include "pch.h"
#include "TrailRenderer.h"

void Trail::Initialize()
{
    HRESULT hr{ S_OK };

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(TrailVertex) * maxPoints);
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;
    hr = Graphics::GetDevice()->CreateBuffer(&bufferDesc, NULL, vertexBuffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_INPUT_ELEMENT_DESC inputElementDesc[]
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
         {"TEXCOORD",0,DXGI_FORMAT_R32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}, // alpha
        {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}, // uv
    };

    hr = CreateVsFromCSO(Graphics::GetDevice(), "./Shader/TrailVS.cso", vertexShader.GetAddressOf(), inputLayout.GetAddressOf(), inputElementDesc, ARRAYSIZE(inputElementDesc));
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(Graphics::GetDevice(), "./Shader/TrailPS.cso", pixelShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


}


void Trail::UpdateTrail(float deltaTime)
{
    for (auto& p : trailPoints)
        p.life -= deltaTime;

    trailPoints.erase(std::remove_if(trailPoints.begin(), trailPoints.end(),
        [](const TrailPoint& p) {return p.life <= 0.0f; }), trailPoints.end());

    vertices.clear();

    for (size_t i = 1; i < trailPoints.size(); i++)
    {
        auto& previent = trailPoints[i - 1];
        auto& current = trailPoints[i];

        // 進行方向
        DirectX::XMVECTOR p0 = XMLoadFloat3(&previent.position);
        DirectX::XMVECTOR p1 = XMLoadFloat3(&current.position);
        DirectX::XMVECTOR dir = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(p1, p0));

        // 横方向（XZ平面）
        DirectX::XMVECTOR side = DirectX::XMVector3Cross(dir, DirectX::XMVectorSet(0, 1, 0, 0));
        side = DirectX::XMVector3Normalize(side);

        float width = 0.3f; // 太さ
        float alpha = current.life / 0.5f;
        alpha = alpha * alpha;

        DirectX::XMFLOAT3 left, right;

        DirectX::XMVECTOR leftVec = DirectX::XMVectorAdd(p1, DirectX::XMVectorScale(side, width));
        DirectX::XMVECTOR rightVec = DirectX::XMVectorSubtract(p1, DirectX::XMVectorScale(side, width));

        XMStoreFloat3(&left, leftVec);
        XMStoreFloat3(&right, rightVec);


        float u = static_cast<float>(i) / (trailPoints.size() - 1);
        vertices.push_back({ left, alpha, {u, 0.0f} });
        vertices.push_back({ right, alpha, {u, 1.0f} });
    }
}

void Trail::Render(ID3D11DeviceContext* immediateContext)
{
    HRESULT hr{ S_OK };
    D3D11_MAPPED_SUBRESOURCE mappedSubresource{};
    hr = immediateContext->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    std::memcpy(mappedSubresource.pData, vertices.data(), sizeof(TrailVertex) * vertices.size());
    immediateContext->Unmap(vertexBuffer.Get(), 0);

    UINT stride{ sizeof(TrailVertex) };
    UINT offset{ 0 };
    immediateContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);

    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    immediateContext->VSSetShader(vertexShader.Get(), NULL, 0);
    immediateContext->PSSetShader(pixelShader.Get(), NULL, 0);
    immediateContext->IASetInputLayout(inputLayout.Get());

    immediateContext->Draw(static_cast<UINT>(vertices.size()), 0);
}
