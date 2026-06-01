#pragma once

#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>

#include "Graphics/Core/ConstantBuffer.h"

class PrimitiveShapeRenderer
{
public:
	PrimitiveShapeRenderer(ID3D11Device* device);
	~PrimitiveShapeRenderer() {}

	// 箱描画
	void DrawBox(
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& angle,
		const DirectX::XMFLOAT3& size,
		const DirectX::XMFLOAT4& color);

	void DrawBox(
		const DirectX::XMFLOAT4X4& transform,
		const DirectX::XMFLOAT3& size,
		const DirectX::XMFLOAT4& color);

	// 球描画
	void DrawSphere(
		const DirectX::XMFLOAT3& position,
		float radius,
		const DirectX::XMFLOAT4& color);

	void DrawSphere(
		const DirectX::XMFLOAT4X4& transform,
		float radius,
		const DirectX::XMFLOAT4& color);

	// カプセル描画
	void DrawCapsule(
		const DirectX::XMFLOAT4X4& transform,
		float radius,
		float height,
		const DirectX::XMFLOAT4& color);

	// 描画実行
	void Render(
		ID3D11DeviceContext* dc,
		const DirectX::XMFLOAT4X4& view,
		const DirectX::XMFLOAT4X4& projection,
		const DirectX::XMFLOAT3& lightDirection);

private:
	struct Vertex
	{
		DirectX::XMFLOAT3		position;
		DirectX::XMFLOAT3		normal;
	};
	struct WiredMesh
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
		UINT									vertexCount;
	};
	struct SolidMesh
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>	indexBuffer;
		UINT									indexCount;
	};

	struct Instance
	{
		WiredMesh* wiredMesh;
		SolidMesh* solidMesh;
		DirectX::XMFLOAT4X4		worldTransform;
		DirectX::XMFLOAT4		color;
	};

	struct CbWiredMesh
	{
		DirectX::XMFLOAT4X4		worldViewProjection;
		DirectX::XMFLOAT4		color;
	};

	struct CbSolidMesh
	{
		DirectX::XMFLOAT4X4		world;
		DirectX::XMFLOAT4X4		viewProjection;
		DirectX::XMFLOAT4		lightDirection;
		DirectX::XMFLOAT4		color;
	};

	// メッシュ生成
	void CreateWiredMesh(ID3D11Device* device, const std::vector<DirectX::XMFLOAT3>& vertices, WiredMesh& mesh);
	void CreateSolidMesh(ID3D11Device* device, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices, SolidMesh& mesh);

	// 箱メッシュ作成
	void CreateWiredBoxMesh(ID3D11Device* device, float width, float height, float depth);
	void CreateSolidBoxMesh(ID3D11Device* device, float width, float height, float depth);

	// 球メッシュ作成
	void CreateWiredSphereMesh(ID3D11Device* device, float radius, int subdivisions);
	void CreateSolidSphereMesh(ID3D11Device* device, float radius, int subdivisions);

	// 半球メッシュ作成
	void CreateWiredHalfSphereMesh(ID3D11Device* device, float radius, int subdivisions);
	void CreateSolidHalfSphereMesh(ID3D11Device* device, float radius, int subdivisions);

	// 円柱
	void CreateWiredCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int subdivisions);
	void CreateSolidCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int subdivisions, bool cap);

private:
	WiredMesh									wiredBoxMesh;
	WiredMesh									wiredSphereMesh;
	WiredMesh									wiredHalfSphereMesh;
	WiredMesh									wiredCylinderMesh;
	SolidMesh									solidBoxMesh;
	SolidMesh									solidSphereMesh;
	SolidMesh									solidHalfSphereMesh;
	SolidMesh									solidCylinderMesh;
	std::vector<Instance>						instances;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>	wiredVertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	wiredPixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	wiredInputLayout;
	std::unique_ptr<ConstantBuffer<CbWiredMesh>> wiredConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>	solidVertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	solidPixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	solidInputLayout;
	std::unique_ptr<ConstantBuffer<CbSolidMesh>> solidConstantBuffer;
};
