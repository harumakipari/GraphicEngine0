#include "pch.h"
#include "FullScreenQuad.h"
#include "Graphics/Core/Shader.h"

#include "Engine/Utility/Win32Utils.h"


FullScreenQuad::FullScreenQuad(ID3D11Device* device)
{
    HRESULT hr{ S_OK };
    hr = CreateVsFromCSO(device, "./Data/Shaders/FullScreenQuadVS.cso", embeddedVertexShader.ReleaseAndGetAddressOf(),
        nullptr, nullptr, 0);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/FullScreenQuadPS.cso", embeddedPixelShader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void FullScreenQuad::Blit(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView** shaderResourceView,
    uint32_t startSlot, uint32_t numViews, ID3D11PixelShader* replacedPixelShader)
{
    immediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    immediateContext->IASetInputLayout(nullptr);

    immediateContext->VSSetShader(embeddedVertexShader.Get(), 0, 0);
    replacedPixelShader ? immediateContext->PSSetShader(replacedPixelShader, 0, 0) :
        immediateContext->PSSetShader(embeddedPixelShader.Get(), 0, 0);

    immediateContext->PSSetShaderResources(startSlot, numViews, shaderResourceView);

    immediateContext->Draw(4, 0);
}

