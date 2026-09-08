#include "pch.h"
#include "HuskParticles.h"

#include <random>
#ifdef _DEBUG
#include <cmath>
#include <limits>
#endif
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
    hr = device->CreateBuffer(&buffer_desc, NULL, particle_backup_buffer.GetAddressOf());
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


    particleCBuffer = std::make_unique<ConstantBuffer<particle_constants>>(device);

    hr = CreateVsFromCSO(device, "./Data/Shaders/HuskParticlesVS.cso", vertex_shader.ReleaseAndGetAddressOf(), nullptr, nullptr, 0);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/HuskParticlesPS.cso", pixel_shader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateGsFromCSO(device, "./Data/Shaders/HuskParticlesGS.cso", geometry_shader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/HuskParticlesCS.cso", compute_shader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreatePsFromCSO(device, "./Data/Shaders/AccumulateHuskParticlePS.cso", accumulate_husk_particles_ps.ReleaseAndGetAddressOf());
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

    particleCBuffer->data = particle_data;
    particleCBuffer->Activate(immediate_context, 12);

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

    particleCBuffer->data = particle_data;
    particleCBuffer->Activate(immediate_context, 12);

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
void husk_particles::backup_particles(ID3D11DeviceContext* immediate_context)
{
    immediate_context->CopyResource(particle_backup_buffer.Get(), particle_buffer.Get());
}

void husk_particles::restore_particles(ID3D11DeviceContext* immediate_context)
{
    immediate_context->CopyResource(particle_buffer.Get(), particle_backup_buffer.Get());
}

void husk_particles::accumulate_husk_particles(ID3D11DeviceContext* immediate_context, std::function<void(ID3D11PixelShader*)> drawcallback, ID3D11ShaderResourceView* sceneColor, ID3D11ShaderResourceView* sceneDepth)
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

    // RTV/DSV are detached above. Material textures occupy t0-t5; IBL uses t32-t35.
    ID3D11ShaderResourceView* captureViews[] = { sceneColor, sceneDepth };
    immediate_context->PSSetShaderResources(24, 2, captureViews);
    // Capture must see current b12 even before the first integrate/render.
    particleCBuffer->data = particle_data;
    particleCBuffer->data.scene_color_capture_ready = sceneColor && sceneDepth ? 1.0f : 0.0f;
    particleCBuffer->Activate(immediate_context, 12);
    drawcallback(accumulate_husk_particles_ps.Get());

    // Unbind both reads before restoring the color RTV and scene DSV.
    ID3D11ShaderResourceView* nullCaptureViews[2] = {};
    immediate_context->PSSetShaderResources(24, 2, nullCaptureViews);

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
#ifdef _DEBUG
    // Once per completed capture, before any integration or display filtering.
    measure_captured_world_x(immediate_context, count);
#endif

#if 1
    immediate_context->RSSetViewports(viewport_count, cached_viewports);
#endif
}

#ifdef _DEBUG
void husk_particles::measure_captured_world_x(ID3D11DeviceContext* context, UINT count)
{
    captured_x = {};
    captured_x.attempted = true;
    captured_x.particle_count = count;
    if (count == 0) return;
    // Do not scan unallocated entries or report an incomplete range as a full capture.
    if (count > max_particle_count)
    {
        captured_x.result = E_INVALIDARG;
        return;
    }

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>(sizeof(particle) * count);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Buffer> readback;
    captured_x.result = device->CreateBuffer(&desc, nullptr, readback.GetAddressOf());
    if (FAILED(captured_x.result)) return;

    // Buffer boxes use byte offsets. Copy only the appended prefix, not full capacity.
    const D3D11_BOX region{ 0, 0, 0, desc.ByteWidth, 1, 1 };
    context->CopySubresourceRegion(readback.Get(), 0, 0, 0, 0, particle_buffer.Get(), 0, &region);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    captured_x.result = context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(captured_x.result)) return;

    const auto* particles = static_cast<const particle*>(mapped.pData);
    float minimum = (std::numeric_limits<float>::max)();
    float maximum = (std::numeric_limits<float>::lowest)();
    for (UINT i = 0; i < count; ++i)
    {
        const float x = particles[i].position.x;
        if (!std::isfinite(x))
        {
            ++captured_x.non_finite_count;
            continue;
        }
        minimum = (std::min)(minimum, x);
        maximum = (std::max)(maximum, x);
    }
    context->Unmap(readback.Get(), 0);

    captured_x.valid = captured_x.non_finite_count < count;
    if (captured_x.valid)
    {
        captured_x.minimum = minimum;
        captured_x.maximum = maximum;
    }
    // The temporary staging allocation is released here; only the summary survives.
}
#endif