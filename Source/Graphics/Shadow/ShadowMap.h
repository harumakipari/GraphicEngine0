#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>
#include <functional>
#include <memory>

#include "Graphics/Core/ConstantBuffer.h"

class cascaded_shadow_map
{

public:
	cascaded_shadow_map(ID3D11Device* device, UINT width, UINT height, UINT cascade_count = 4);
	virtual ~cascaded_shadow_map() = default;
	cascaded_shadow_map(const cascaded_shadow_map&) = delete;
	cascaded_shadow_map& operator =(const cascaded_shadow_map&) = delete;
	cascaded_shadow_map(cascaded_shadow_map&&) noexcept = delete;
	cascaded_shadow_map& operator =(cascaded_shadow_map&&) noexcept = delete;

	void make(ID3D11DeviceContext* immediate_context,
		const DirectX::XMFLOAT4X4& camera_view,
		const DirectX::XMFLOAT4X4& camera_projection,
		const DirectX::XMFLOAT4& light_direction,
		float critical_depth_value, // If this value is 0, the camera's far panel distance is used.
		std::function<void()> drawcallback);

private:
	Microsoft::WRL::ComPtr<ID3D11Texture2D> _depth_stencil_buffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> _depth_stencil_view;
	D3D11_VIEWPORT _viewport;

	std::vector<DirectX::XMFLOAT4X4> _view_projection;
	std::vector<float> _distances;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _shader_resource_view;

	struct constants
	{
		DirectX::XMFLOAT4X4 view_projection_matrices[4];// ÉâÉCÉgVPçsóÒ
		float cascade_plane_distances[4]; // ï™äÑãóó£

		//DirectX::XMFLOAT4X4 view_projection_matrices[16];
		//float cascade_plane_distances[16];
		//int cascade_count = 4;
	};
	std::unique_ptr<ConstantBuffer<constants>> _constants;


public:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& depth_map()
	{
		return _shader_resource_view;
	}
	void clear(ID3D11DeviceContext* immediate_context)
	{
		immediate_context->ClearDepthStencilView(_depth_stencil_view.Get(), D3D11_CLEAR_DEPTH, 1, 0);
	}

public:
	const UINT _cascade_count;
	float _split_scheme_weight = 0.7f; // logarithmic_split_scheme * _split_scheme_weight + uniform_split_scheme * (1 - _split_scheme_weight)

};
