#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <assert.h>
#include "Engine/Utility/Win32Utils.h"

template<class T>
class ConstantBuffer
{
public:
    T data;
    ConstantBuffer(ID3D11Device* device)
    {
        HRESULT hr = S_OK;

        D3D11_BUFFER_DESC bufferDesc{};
        bufferDesc.ByteWidth = (sizeof(T) + 15) & ~15;
        //bufferDesc.ByteWidth = (sizeof(T) + 0x0f) & ~0x0f;
        //bufferDesc.ByteWidth = sizeof(T);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
#if 0
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
#else
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.CPUAccessFlags = 0;
#endif // 0
        bufferDesc.MiscFlags = 0;
        bufferDesc.StructureByteStride = 0;
        hr = device->CreateBuffer(&bufferDesc, nullptr, constantBuffer.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    ~ConstantBuffer() = default;

    void Activate(ID3D11DeviceContext* immediateContext, int slot)
    {
        HRESULT hr = S_OK;
#if 0
        D3D11_MAPPED_SUBRESOURCE mappedSubresource = {};
        hr = immediateContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        memcpy_s(mappedSubresource.pData, sizeof(T), &data, sizeof(T));
        immediateContext->Unmap(constantBuffer.Get(), 0);
        immediateContext->PSSetConstantBuffers(slot, 1, constantBuffer.GetAddressOf());
        immediateContext->VSSetConstantBuffers(slot, 1, constantBuffer.GetAddressOf());
        immediateContext->GSSetConstantBuffers(slot, 1, constantBuffer.GetAddressOf());
        immediateContext->CSSetConstantBuffers(slot, 1, constantBuffer.GetAddressOf());
        immediateContext->HSSetConstantBuffers(slot, 1, constantBuffer.GetAddressOf());
#else
        immediateContext->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &data, 0, 0);
        immediateContext->PSSetConstantBuffers(slot, 1, constantBuffer.GetAddressOf());
        immediateContext->VSSetConstantBuffers(slot, 1, constantBuffer.GetAddressOf());
        immediateContext->GSSetConstantBuffers(slot, 1, constantBuffer.GetAddressOf());
        immediateContext->CSSetConstantBuffers(slot, 1, constantBuffer.GetAddressOf());
        immediateContext->HSSetConstantBuffers(slot, 1, constantBuffer.GetAddressOf());
        // TODO
#endif
    }
private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
};


