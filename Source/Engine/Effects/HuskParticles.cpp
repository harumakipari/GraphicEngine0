#include "pch.h"
#include "HuskParticles.h"

#include <random>
#include "Graphics/Core/Shader.h"
#include "Engine/Utility/Win32Utils.h"

using namespace DirectX;

husk_particles::husk_particles(ID3D11Device* device, size_t max_particle_count) : max_particle_count(max_particle_count)
{
    HRESULT hr{ S_OK };

    D3D11_BUFFER_DESC buffer_desc{};

    buffer_desc.ByteWidth = static_cast<UINT>(sizeof(particle) * max_particle_count);
    buffer_desc.StructureByteStride = sizeof(particle);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    hr = device->CreateBuffer(&buffer_desc, NULL, particle_buffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t));
    buffer_desc.StructureByteStride = sizeof(uint32_t);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    buffer_desc.MiscFlags = 0;
    hr = device->CreateBuffer(&buffer_desc, NULL, particle_count_buffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc;
    shader_resource_view_desc.Format = DXGI_FORMAT_UNKNOWN;
    shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    shader_resource_view_desc.Buffer.ElementOffset = 0;
    shader_resource_view_desc.Buffer.NumElements = static_cast<UINT>(max_particle_count);
    hr = device->CreateShaderResourceView(particle_buffer.Get(), &shader_resource_view_desc, particle_buffer_srv.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_view_desc;
    unordered_access_view_desc.Format = DXGI_FORMAT_UNKNOWN;
    unordered_access_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    unordered_access_view_desc.Buffer.FirstElement = 0;
    unordered_access_view_desc.Buffer.NumElements = static_cast<UINT>(max_particle_count);
    unordered_access_view_desc.Buffer.Flags = 0;
    hr = device->CreateUnorderedAccessView(particle_buffer.Get(), &unordered_access_view_desc, particle_buffer_uav.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    unordered_access_view_desc.Format = DXGI_FORMAT_UNKNOWN;
    unordered_access_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    unordered_access_view_desc.Buffer.FirstElement = 0;
    unordered_access_view_desc.Buffer.NumElements = static_cast<UINT>(max_particle_count);
    unordered_access_view_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
    hr = device->CreateUnorderedAccessView(particle_buffer.Get(), &unordered_access_view_desc, particle_append_buffer_uav.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    buffer_desc.ByteWidth = sizeof(particle_constants);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;
    hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    hr = CreateVsFromCSO(device, "husk_particles_vs.cso", vertex_shader.ReleaseAndGetAddressOf(), nullptr, nullptr, 0);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "husk_particles_ps.cso", pixel_shader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateGsFromCSO(device, "husk_particles_gs.cso", geometry_shader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "husk_particles_cs.cso", compute_shader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "accumulate_husk_particles_ps.cso", accumulate_husk_particles_ps.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

UINT align(UINT num, UINT alignment)
{
    return (num + (alignment - 1)) & ~(alignment - 1);
}
void husk_particles::integrate(ID3D11DeviceContext* immediate_context, float delta_time)
{
    _ASSERT_EXPR(particle_data.particle_count <= max_particle_count, L"");
    if (particle_data.particle_count == 0)
    {
        return;
    }

    immediate_context->CSSetUnorderedAccessViews(0, 1, particle_buffer_uav.GetAddressOf(), nullptr);

    particle_data.delta_time = delta_time;
    immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &particle_data, 0, 0);
    immediate_context->CSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

    immediate_context->CSSetShader(compute_shader.Get(), NULL, 0);

    UINT num_threads = align(particle_data.particle_count, 256);
    immediate_context->Dispatch(num_threads / 256, 1, 1);

    ID3D11UnorderedAccessView* null_unordered_access_view{};
    immediate_context->CSSetUnorderedAccessViews(0, 1, &null_unordered_access_view, nullptr);
}

void husk_particles::render(ID3D11DeviceContext* immediate_context)
{
    _ASSERT_EXPR(particle_data.particle_count <= max_particle_count, L"");
    if (particle_data.particle_count == 0)
    {
        return;
    }

    immediate_context->VSSetShader(vertex_shader.Get(), NULL, 0);
    immediate_context->PSSetShader(pixel_shader.Get(), NULL, 0);
    immediate_context->GSSetShader(geometry_shader.Get(), NULL, 0);
    immediate_context->GSSetShaderResources(9, 1, particle_buffer_srv.GetAddressOf());

    immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &particle_data, 0, 0);
    immediate_context->VSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());
    immediate_context->PSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());
    immediate_context->GSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

    immediate_context->IASetInputLayout(NULL);
    immediate_context->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
    immediate_context->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
    immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    immediate_context->Draw(static_cast<UINT>(particle_data.particle_count), 0);

    ID3D11ShaderResourceView* null_shader_resource_view{};
    immediate_context->GSSetShaderResources(9, 1, &null_shader_resource_view);
    immediate_context->VSSetShader(NULL, NULL, 0);
    immediate_context->PSSetShader(NULL, NULL, 0);
    immediate_context->GSSetShader(NULL, NULL, 0);
}
void husk_particles::accumulate_husk_particles(ID3D11DeviceContext* immediate_context, std::function<void(ID3D11PixelShader*)> drawcallback)
{
    HRESULT hr{ S_OK };

#if 1
    UINT viewport_count{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };
    D3D11_VIEWPORT cached_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    immediate_context->RSGetViewports(&viewport_count, cached_viewports);

    const float resolution_ratio = 1.0f;
    D3D11_VIEWPORT viewport;
    viewport.Width = cached_viewports[0].Width * resolution_ratio;
    viewport.Height = cached_viewports[0].Height * resolution_ratio;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    immediate_context->RSSetViewports(1, &viewport);
#endif

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cached_render_target_view;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cached_depth_stencil_view;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> cached_unordered_access_view;
    immediate_context->OMGetRenderTargetsAndUnorderedAccessViews(
        1, cached_render_target_view.GetAddressOf(), cached_depth_stencil_view.GetAddressOf(),
        1, 1, cached_unordered_access_view.GetAddressOf()
    );

    ID3D11RenderTargetView* nulll_render_target_view{};
    UINT initial_count{ 0 };
    immediate_context->OMSetRenderTargetsAndUnorderedAccessViews(
        1, &nulll_render_target_view, NULL,
        1, 1, particle_append_buffer_uav.GetAddressOf(), &initial_count
    );

    drawcallback(accumulate_husk_particles_ps.Get());

    immediate_context->OMSetRenderTargetsAndUnorderedAccessViews(
        1, cached_render_target_view.GetAddressOf(), cached_depth_stencil_view.Get(),
        1, 1, cached_unordered_access_view.GetAddressOf(), NULL
    );

    immediate_context->CopyStructureCount(particle_count_buffer.Get(), 0, particle_append_buffer_uav.Get());
    D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
    hr = immediate_context->Map(particle_count_buffer.Get(), 0, D3D11_MAP_READ, 0, &mapped_subresource);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    UINT count{ *reinterpret_cast<UINT*>(mapped_subresource.pData) };
    immediate_context->Unmap(particle_count_buffer.Get(), 0);

    particle_data.particle_count = count;

#if 1
    immediate_context->RSSetViewports(viewport_count, cached_viewports);
#endif
}
