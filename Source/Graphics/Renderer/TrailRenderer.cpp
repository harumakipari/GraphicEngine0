#include "pch.h"
#include "TrailRenderer.h"

// Catmull- Rom ï‚äÆä÷êî
static DirectX::XMVECTOR CatmullRom(DirectX::XMVECTOR p0,DirectX::XMVECTOR p1,DirectX::XMVECTOR p2,DirectX::XMVECTOR p3,float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    return DirectX::XMVectorScale(
    DirectX::XMVectorAdd(
            DirectX::XMVectorAdd(
                DirectX::XMVectorScale(p1, 2.0f),
                DirectX::XMVectorScale(DirectX::XMVectorSubtract(p2, p0), t)),
            DirectX::XMVectorAdd(
                DirectX::XMVectorScale(
                    DirectX::XMVectorAdd(
                        DirectX::XMVectorAdd(
                            DirectX::XMVectorScale(p0, 2.0f),
                            DirectX::XMVectorScale(p2, 4.0f)),
                        DirectX::XMVectorAdd(
                            DirectX::XMVectorScale(p1, -5.0f),
                            DirectX::XMVectorScale(p3, -1.0f))),
                    t2),
                DirectX::XMVectorScale(
                    DirectX::XMVectorAdd(
                        DirectX::XMVectorAdd(
                            DirectX::XMVectorScale(p1, 3.0f),
                            DirectX::XMVectorScale(p3, 1.0f)),
                        DirectX::XMVectorAdd(
                            DirectX::XMVectorScale(p0, -1.0f),
                            DirectX::XMVectorScale(p2, -3.0f))),
                    t3))),
        0.5f);
}


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

    hr = CreateVsFromCSO(Graphics::GetDevice(), "./Data/Shaders/TrailVS.cso", vertexShader.GetAddressOf(), inputLayout.GetAddressOf(), inputElementDesc, ARRAYSIZE(inputElementDesc));
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(Graphics::GetDevice(), "./Data/Shaders/TrailPS.cso", pixelShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


}


void Trail::UpdateTrail(float deltaTime)
{
    for (auto& p : trailPoints)
        p.life -= deltaTime;

    trailPoints.erase(std::remove_if(trailPoints.begin(), trailPoints.end(),
        [](const TrailPoint& p) {return p.life <= 0.0f; }), trailPoints.end());

    vertices.clear();

    if (trailPoints.size() < 2)
        return;

    const float maxLife = 0.5f;

#if 0
    for (size_t i = 0; i < trailPoints.size(); i++)
    {
        auto& point = trailPoints[i];

        float alpha = point.life / maxLife;
        alpha *= alpha;

        float u = static_cast<float>(i) / (trailPoints.size() - 1);

        // åïêÊ
        vertices.push_back({ point.tip,alpha,{u, 0.0f} });
        // åïÇÃç™å≥
        vertices.push_back({ point.root,alpha,{u, 1.0f} });
    }
#else
    if (trailPoints.size() < 4)
        return;

    const int samplesPerSegment = 6;

    for (size_t i = 0; i + 3 < trailPoints.size(); ++i)
    {
        auto& p0 = trailPoints[i + 0];
        auto& p1 = trailPoints[i + 1];
        auto& p2 = trailPoints[i + 2];
        auto& p3 = trailPoints[i + 3];

        DirectX::XMVECTOR tip0 = XMLoadFloat3(&p0.tip);
        DirectX::XMVECTOR tip1 = XMLoadFloat3(&p1.tip);
        DirectX::XMVECTOR tip2 = XMLoadFloat3(&p2.tip);
        DirectX::XMVECTOR tip3 = XMLoadFloat3(&p3.tip);

        DirectX::XMVECTOR root0 = XMLoadFloat3(&p0.root);
        DirectX::XMVECTOR root1 = XMLoadFloat3(&p1.root);
        DirectX::XMVECTOR root2 = XMLoadFloat3(&p2.root);
        DirectX::XMVECTOR root3 = XMLoadFloat3(&p3.root);

        for (int s = 0; s < samplesPerSegment; ++s)
        {
            float t = static_cast<float>(s) / (samplesPerSegment - 1);

            DirectX::XMVECTOR tip = CatmullRom(tip0, tip1, tip2, tip3, t);
            DirectX::XMVECTOR root = CatmullRom(root0, root1, root2, root3, t);

            DirectX::XMFLOAT3 tipPos;
            DirectX::XMFLOAT3 rootPos;

            XMStoreFloat3(&tipPos, tip);
            XMStoreFloat3(&rootPos, root);

            // ÉøÇÕ p1 Å® p2 ÇÃä‘Çï‚ä‘
            float alpha = std::lerp(
                p1.life / maxLife,
                p2.life / maxLife,
                t);

            alpha = alpha * alpha;

            float u =
                (static_cast<float>(i) + t) /
                static_cast<float>(trailPoints.size() - 1);

            vertices.push_back({
                tipPos,
                alpha,
                {u, 0.0f}
                });

            vertices.push_back({
                rootPos,
                alpha,
                {u, 1.0f}
                });
        }
    }
#endif // 0

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
