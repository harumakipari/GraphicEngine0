#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>

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
		float normalizedHeight{};
	};

	struct particle_constants
	{
		int particle_count{};
		float particle_size{ 0.005f };
		float particle_option{};
		float delta_time{};
		float height_min{};
		float height_range{ 1.0f };
		float death_progress{};
		float detach_speed{ 0.35f };
		float gravity{ -9.8f };
		float lifetime{ 1.5f };
	};
	particle_constants particle_data;
	std::unique_ptr<ConstantBuffer<particle_constants>> particleCBuffer;

	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_buffer_uav;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_append_buffer_uav;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_buffer_srv;

	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_count_buffer;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometry_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> compute_shader;

	husk_particles(ID3D11Device* pDevice, size_t max_particle_count = 1000000);
	husk_particles(const husk_particles&) = delete;
	husk_particles& operator=(const husk_particles&) = delete;
	husk_particles(husk_particles&&) noexcept = delete;
	husk_particles& operator=(husk_particles&&) noexcept = delete;
	virtual ~husk_particles() = default;

	void integrate(ID3D11DeviceContext* immediate_context, float delta_time);
	void render(ID3D11DeviceContext* immediate_context);

	Microsoft::WRL::ComPtr<ID3D11PixelShader> accumulate_husk_particles_ps;
	void accumulate_husk_particles(ID3D11DeviceContext* immediate_context, std::function<void(ID3D11PixelShader*)> drawcallback);
};
