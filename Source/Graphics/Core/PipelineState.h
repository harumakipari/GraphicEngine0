#pragma once

#include <d3d11.h>
#include <wrl.h>

#include "Graphics/Core/RenderState.h"

struct PipeLineStateDesc
{
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader = nullptr;
    Microsoft::WRL::ComPtr <ID3D11VertexShader> vertexShader = nullptr;
    Microsoft::WRL::ComPtr <ID3D11HullShader> hullShader = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DomainShader> domainShader = nullptr;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometryShader = nullptr;
    
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout = nullptr;

    D3D11_PRIMITIVE_TOPOLOGY primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    BLEND_STATE               blendState = BLEND_STATE::NONE;
    DEPTH_STATE               depthState = DEPTH_STATE::ZT_ON_ZW_ON;
    RASTERIZE_STATE         rasterState = RASTERIZE_STATE::SOLID_CULL_BACK;
    SAMPLER_STATE             samplerState = SAMPLER_STATE::LINEAR;
};


