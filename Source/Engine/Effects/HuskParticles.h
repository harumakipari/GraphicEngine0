#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>
#include <cstddef>

#include "Graphics/Core/ConstantBuffer.h"
#include "Graphics/Resource/InterleavedGltfModel.h"

struct husk_particles
{
    size_t max_particle_count;
    struct particle
    {
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT3 velocity;
        float age{};
        int state{};
        float normalizedX{};
    };

    struct particle_constants
    {
        int particle_count{};
        float particle_size{ 0.008f };
        float particle_option{};
        float delta_time{};
        float world_x_min{ 6.695f };
        float world_x_max{ 12.519f };
        float death_progress{};
        float detach_speed{ 0.35f };
        float gravity{ -9.8f };
        float lifetime{ 1.5f };
        float rise_speed{ 0.5f };
        float max_start_delay{ 0.28f };
        float rise_speed_min_multiplier{ 0.6f };
        float rise_speed_max_multiplier{ 1.4f };
        float horizontal_random_speed{ 0.08f };
        float fade_start_ratio{ 0.75f };
        float lifetime_min_multiplier{ 0.85f };
        float lifetime_max_multiplier{ 1.15f };
        float display_ratio{ 0.75f };
        float debug_normalized_x{}; // Debug display only; reuses b12 padding.
        float boundary_width{ 0.01f };
        float boundary_emissive_strength{ 4.0f };
        float detach_glow_duration{ 0.2f };
        float detach_glow_strength{ 0.2f };
        DirectX::XMFLOAT3 body_color_multiplier{ 1.0f, 1.0f, 1.0f };
        float body_brightness{ 1.0f };
        float use_scene_color_capture{ 1.0f };
        float scene_color_depth_threshold{ 0.000001f }; // Device depth [0, 1].
        float scene_color_capture_ready{}; // Set only for the capture upload.
        float capture_padding{};
    };
    static_assert(sizeof(particle) == 64, "Husk particle GPU stride must stay unchanged");
    static_assert(offsetof(particle, normalizedX) == 60, "Husk normalizedX layout");
    static_assert(sizeof(particle_constants) == 128, "Husk b12 must occupy eight registers");
    static_assert(offsetof(particle_constants, world_x_min) == 16, "Husk b12 X range layout");
    static_assert(offsetof(particle_constants, death_progress) == 24, "Husk b12 progress layout");
    static_assert(offsetof(particle_constants, display_ratio) == 72, "Husk b12 display layout");
    static_assert(offsetof(particle_constants, debug_normalized_x) == 76, "Husk b12 debug layout");
    static_assert(offsetof(particle_constants, boundary_width) == 80, "Husk b12 glow layout");
    static_assert(offsetof(particle_constants, detach_glow_strength) == 92, "Husk b12 glow end layout");
    static_assert(offsetof(particle_constants, body_color_multiplier) == 96, "Husk b12 body color layout");
    static_assert(offsetof(particle_constants, body_brightness) == 108, "Husk b12 body brightness layout");
    static_assert(offsetof(particle_constants, use_scene_color_capture) == 112, "Husk b12 capture layout");
    static_assert(offsetof(particle_constants, scene_color_capture_ready) == 120, "Husk b12 capture ready layout");
#ifdef _DEBUG
    // CPU-only diagnostics. Never uploaded to a shader constant/particle buffer.
    struct capture_x_measurement
    {
        bool attempted = false;
        bool valid = false;
        HRESULT result = S_OK;
        UINT particle_count = 0;
        UINT non_finite_count = 0;
        float minimum = 0.0f;
        float maximum = 0.0f;
    };
    capture_x_measurement captured_x;
    void measure_captured_world_x(ID3D11DeviceContext* context, UINT count);
#endif
    particle_constants particle_data;
    std::unique_ptr<ConstantBuffer<particle_constants>> particleCBuffer;

    Microsoft::WRL::ComPtr<ID3D11Buffer> particle_buffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> particle_backup_buffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_buffer_uav;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_append_buffer_uav;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_buffer_srv;

    Microsoft::WRL::ComPtr<ID3D11Buffer> particle_count_buffer;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometry_shader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> compute_shader;

    husk_particles(ID3D11Device* pDevice, size_t max_particle_count = 3000000);
    husk_particles(const husk_particles&) = delete;
    husk_particles& operator=(const husk_particles&) = delete;
    husk_particles(husk_particles&&) noexcept = delete;
    husk_particles& operator=(husk_particles&&) noexcept = delete;
    virtual ~husk_particles() = default;

    void integrate(ID3D11DeviceContext* immediate_context, float delta_time);
    void render(ID3D11DeviceContext* immediate_context);
    void backup_particles(ID3D11DeviceContext* immediate_context);
    void restore_particles(ID3D11DeviceContext* immediate_context);

    Microsoft::WRL::ComPtr<ID3D11PixelShader> accumulate_husk_particles_ps;
    void accumulate_husk_particles(ID3D11DeviceContext* immediate_context, std::function<void(ID3D11PixelShader*)> drawcallback, ID3D11ShaderResourceView* sceneColor, ID3D11ShaderResourceView* sceneDepth);
};
