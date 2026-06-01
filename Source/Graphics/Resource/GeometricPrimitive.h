#pragma once
#include <directXmath.h>
//COMオブジェクトをComPtrスマートポインターテンプレートを使った変数宣言に変更する
#include <wrl.h>

///ID3D11
#include <d3d11.h>

class GeometricPrimitive
{
public:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
    };
    struct Constants
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4 materialColor;
    };

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;

public:
    virtual ~GeometricPrimitive() = default;

    void Render(DirectX::XMFLOAT4X4& world, DirectX::XMFLOAT4& materialColor, std::function<void()> callbackFunc = nullptr);

protected:
    GeometricPrimitive();

    //頂点情報と頂点の順番をGPUに伝える
    void CreateComBuffers(Vertex* vertices, size_t vertexCount,
        uint32_t* indices, size_t indexCount);
};

class GeometricCube :public GeometricPrimitive
{
public:
    GeometricCube();
};

class GeometricCylinder : public GeometricPrimitive
{
public:
    GeometricCylinder(uint32_t slices);
};

class GeometricSphere : public GeometricPrimitive
{
public:
    GeometricSphere(DirectX::XMFLOAT3 center, float radius, uint32_t slices, uint32_t stacks);
    GeometricSphere(float radius, uint32_t slices, uint32_t stacks);
};

class GeometricCapsule : public GeometricPrimitive
{
public:
    GeometricCapsule(float mantle_height, const DirectX::XMFLOAT3& radius, uint32_t slices, uint32_t ellipsoid_stacks, uint32_t mantle_stacks);
};
