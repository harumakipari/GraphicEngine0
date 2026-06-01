#ifndef SHADER_TOY_H
#define SHADER_TOY_H

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

class ShaderToy
{
public:
    ShaderToy(ID3D11Device* device);
    virtual ~ShaderToy() = default;

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> embedded_vertex_shader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> embedded_pixel_shader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
    struct constants
    {
        DirectX::XMFLOAT4 iResolution;
        DirectX::XMFLOAT4 iMouse;
        DirectX::XMFLOAT4 iChannelResolution[4];
        float iTime;
        float iFrame;
        float iPad0;
        float iPad1;
    };
public:
    constants constant{};
public:
    void Blit(ID3D11DeviceContext* immediate_contextbool/*, float time*/, ID3D11ShaderResourceView** shader_resource_view, uint32_t start_slot, uint32_t num_views, ID3D11PixelShader* replaced_pixel_shader = nullptr);
};


#endif // !SHADER_TOY_H
