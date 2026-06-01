#include "pch.h"
#include "ShaderToy.h"

#include "Graphics/Core/Shader.h"
#include "Engine/Utility/Win32Utils.h"

ShaderToy::ShaderToy(ID3D11Device* device)
{
	//CreateVsFromCSO(device, "shadertoy_vs.cso", embedded_vertex_shader.ReleaseAndGetAddressOf(), nullptr, nullptr, 0);
	//CreatePsFromCSO(device, "shadertoy_ps.cso", embedded_pixel_shader.ReleaseAndGetAddressOf());

	HRESULT hr = S_OK;

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}
void ShaderToy::Blit(ID3D11DeviceContext* immediate_context/*, float time*/, ID3D11ShaderResourceView** shader_resource_view, uint32_t start_slot, uint32_t num_views, ID3D11PixelShader* replaced_pixel_shader)
{
	//constants data;
	D3D11_VIEWPORT viewport;
	UINT num_viewports{ 1 };
	immediate_context->RSGetViewports(&num_viewports, &viewport);
	constant.iResolution.x = viewport.Width;
	constant.iResolution.y = viewport.Height;
	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &constant, 0, 0);
	immediate_context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());

	immediate_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	immediate_context->IASetInputLayout(nullptr);

	immediate_context->VSSetShader(embedded_vertex_shader.Get(), 0, 0);
	replaced_pixel_shader ? immediate_context->PSSetShader(replaced_pixel_shader, 0, 0) : immediate_context->PSSetShader(embedded_pixel_shader.Get(), 0, 0);

	immediate_context->PSSetShaderResources(start_slot, num_views, shader_resource_view);

	immediate_context->Draw(4, 0);
}



