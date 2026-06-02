#include "pch.h"
#include "PrimitiveShapeRenderer.h"

#include "ShapeRenderer.h"
#include "Graphics/Core/Shader.h"

// コンストラクタ
PrimitiveShapeRenderer::PrimitiveShapeRenderer(ID3D11Device* device)
{
	// 入力レイアウト
	D3D11_INPUT_ELEMENT_DESC wiredInputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	D3D11_INPUT_ELEMENT_DESC solidInputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	// 頂点シェーダー
	HRESULT hr = CreateVsFromCSO(
		device,
		"./Data/Shaders/WiredShapeRendererVS.cso",
		wiredVertexShader.GetAddressOf(),
		wiredInputLayout.GetAddressOf(),
		wiredInputElementDesc,
		_countof(wiredInputElementDesc)
		);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = CreateVsFromCSO(
		device,
		"./Data/Shaders/SolidShapeRendererVS.cso",
		solidVertexShader.GetAddressOf(),
		solidInputLayout.GetAddressOf(),
		solidInputElementDesc,
		_countof(solidInputElementDesc));
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// ピクセルシェーダー
	hr = CreatePsFromCSO(
		device,
		"./Data/Shaders/WiredShapeRendererPS.cso",
		wiredPixelShader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = CreatePsFromCSO(
		device,
		"./Data/Shaders/SolidShapeRendererPS.cso",
		solidPixelShader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// 定数バッファ
	wiredConstantBuffer = std::make_unique<ConstantBuffer<CbWiredMesh>>(device);
	solidConstantBuffer = std::make_unique<ConstantBuffer<CbSolidMesh>>(device);

	// 箱メッシュ生成
	CreateWiredBoxMesh(device, 1.0f, 1.0f, 1.0f);
	CreateSolidBoxMesh(device, 1.0f, 1.0f, 1.0f);

	// 球メッシュ生成
	CreateWiredSphereMesh(device, 1.0f, 32);
	CreateSolidSphereMesh(device, 1.0f, 32);

	// 半球メッシュ生成
	CreateWiredHalfSphereMesh(device, 1.0f, 32);
	CreateSolidHalfSphereMesh(device, 1.0f, 32);

	// 円柱メッシュ生成
	CreateWiredCylinderMesh(device, 1.0f, 1.0f, -0.5f, 1.0f, 32);
	CreateSolidCylinderMesh(device, 1.0f, 1.0f, -0.5f, 1.0f, 32, false);
}

// 箱描画
void PrimitiveShapeRenderer::DrawBox(
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT3& angle,
	const DirectX::XMFLOAT3& size,
	const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.wiredMesh = &wiredBoxMesh;
	instance.solidMesh = &solidBoxMesh;
	instance.color = color;

	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(size.x, size.y, size.z);
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * R * T);
}

void PrimitiveShapeRenderer::DrawBox(
	const DirectX::XMFLOAT4X4& transform,
	const DirectX::XMFLOAT3& size,
	const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.wiredMesh = &wiredBoxMesh;
	instance.solidMesh = &solidBoxMesh;
	instance.color = color;

	DirectX::XMMATRIX Transform = DirectX::XMLoadFloat4x4(&transform);
	Transform.r[0] = DirectX::XMVectorScale(Transform.r[0], size.x);
	Transform.r[1] = DirectX::XMVectorScale(Transform.r[1], size.y);
	Transform.r[2] = DirectX::XMVectorScale(Transform.r[2], size.z);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, Transform);
}

// 球描画
void PrimitiveShapeRenderer::DrawSphere(
	const DirectX::XMFLOAT3& position,
	float radius,
	const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.wiredMesh = &wiredSphereMesh;
	instance.solidMesh = &solidSphereMesh;
	instance.color = color;

	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(radius, radius, radius);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, S * T);
}

void PrimitiveShapeRenderer::DrawSphere(
	const DirectX::XMFLOAT4X4& transform,
	float radius,
	const DirectX::XMFLOAT4& color)
{
	Instance& instance = instances.emplace_back();
	instance.wiredMesh = &wiredSphereMesh;
	instance.solidMesh = &solidSphereMesh;
	instance.color = color;

	DirectX::XMMATRIX Transform = DirectX::XMLoadFloat4x4(&transform);
	Transform.r[0] = DirectX::XMVectorScale(Transform.r[0], radius);
	Transform.r[1] = DirectX::XMVectorScale(Transform.r[1], radius);
	Transform.r[2] = DirectX::XMVectorScale(Transform.r[2], radius);
	DirectX::XMStoreFloat4x4(&instance.worldTransform, Transform);
}

// カプセル描画
void PrimitiveShapeRenderer::DrawCapsule(
	const DirectX::XMFLOAT4X4& transform,
	float radius,
	float height,
	const DirectX::XMFLOAT4& color)
{
	DirectX::XMMATRIX Transform = DirectX::XMLoadFloat4x4(&transform);

	// 上半球
	{
		Instance& instance = instances.emplace_back();
		instance.wiredMesh = &wiredHalfSphereMesh;
		instance.solidMesh = &solidHalfSphereMesh;
		DirectX::XMVECTOR Position = DirectX::XMVector3Transform(DirectX::XMVectorSet(0, height * 0.5f, 0, 0), Transform);
		DirectX::XMMATRIX World = DirectX::XMMatrixScaling(radius, radius, radius);
		World = DirectX::XMMatrixMultiply(World, Transform);
		World.r[3] = DirectX::XMVectorSetW(Position, 1.0f);
		DirectX::XMStoreFloat4x4(&instance.worldTransform, World);
		instance.color = color;
	}
	// 円柱
	{
		Instance& instance = instances.emplace_back();
		instance.wiredMesh = &wiredCylinderMesh;
		instance.solidMesh = &solidCylinderMesh;
		DirectX::XMMATRIX World;
		World.r[0] = DirectX::XMVectorScale(Transform.r[0], radius);
		World.r[1] = DirectX::XMVectorScale(Transform.r[1], height);
		World.r[2] = DirectX::XMVectorScale(Transform.r[2], radius);
		World.r[3] = Transform.r[3];
		DirectX::XMStoreFloat4x4(&instance.worldTransform, World);
		instance.color = color;
	}
	// 下半球
	{
		Instance& instance = instances.emplace_back();
		instance.wiredMesh = &wiredHalfSphereMesh;
		instance.solidMesh = &solidHalfSphereMesh;
		DirectX::XMMATRIX World = DirectX::XMMatrixRotationX(DirectX::XM_PI);
		DirectX::XMVECTOR Position = DirectX::XMVector3Transform(DirectX::XMVectorSet(0, -height * 0.5f, 0, 0), Transform);
		Transform.r[3] = DirectX::XMVectorSet(0, 0, 0, 1);
		World = DirectX::XMMatrixMultiply(World, Transform);
		World.r[0] = DirectX::XMVectorScale(World.r[0], radius);
		World.r[1] = DirectX::XMVectorScale(World.r[1], radius);
		World.r[2] = DirectX::XMVectorScale(World.r[2], radius);
		World.r[3] = DirectX::XMVectorSetW(Position, 1.0f);
		DirectX::XMStoreFloat4x4(&instance.worldTransform, World);
		instance.color = color;
	}
}

// メッシュ生成
void PrimitiveShapeRenderer::CreateWiredMesh(ID3D11Device* device, const std::vector<DirectX::XMFLOAT3>& vertices, WiredMesh& mesh)
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = static_cast<UINT>(sizeof(DirectX::XMFLOAT3) * vertices.size());
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pSysMem = vertices.data();
	subresourceData.SysMemPitch = 0;
	subresourceData.SysMemSlicePitch = 0;

	HRESULT hr = device->CreateBuffer(&desc, &subresourceData, mesh.vertexBuffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	mesh.vertexCount = static_cast<UINT>(vertices.size());
}

void PrimitiveShapeRenderer::CreateSolidMesh(ID3D11Device* device, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices, SolidMesh& mesh)
{
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size());
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pSysMem = vertices.data();
		subresourceData.SysMemPitch = 0;
		subresourceData.SysMemSlicePitch = 0;

		HRESULT hr = device->CreateBuffer(&desc, &subresourceData, mesh.vertexBuffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = static_cast<UINT>(sizeof(uint16_t) * indices.size());
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pSysMem = indices.data();
		subresourceData.SysMemPitch = 0;
		subresourceData.SysMemSlicePitch = 0;

		HRESULT hr = device->CreateBuffer(&desc, &subresourceData, mesh.indexBuffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	mesh.indexCount = static_cast<UINT>(indices.size());
}

// 箱メッシュ作成
void PrimitiveShapeRenderer::CreateWiredBoxMesh(ID3D11Device* device, float width, float height, float depth)
{
	DirectX::XMFLOAT3 positions[8] =
	{
		// top
		{ -width,  height, -depth},
		{  width,  height, -depth},
		{  width,  height,  depth},
		{ -width,  height,  depth},
		// bottom
		{ -width, -height, -depth},
		{  width, -height, -depth},
		{  width, -height,  depth},
		{ -width, -height,  depth},
	};

	std::vector<DirectX::XMFLOAT3> vertices;
	vertices.resize(32);

	// top
	vertices.emplace_back(positions[0]);
	vertices.emplace_back(positions[1]);
	vertices.emplace_back(positions[1]);
	vertices.emplace_back(positions[2]);
	vertices.emplace_back(positions[2]);
	vertices.emplace_back(positions[3]);
	vertices.emplace_back(positions[3]);
	vertices.emplace_back(positions[0]);
	// bottom
	vertices.emplace_back(positions[4]);
	vertices.emplace_back(positions[5]);
	vertices.emplace_back(positions[5]);
	vertices.emplace_back(positions[6]);
	vertices.emplace_back(positions[6]);
	vertices.emplace_back(positions[7]);
	vertices.emplace_back(positions[7]);
	vertices.emplace_back(positions[4]);
	// side
	vertices.emplace_back(positions[0]);
	vertices.emplace_back(positions[4]);
	vertices.emplace_back(positions[1]);
	vertices.emplace_back(positions[5]);
	vertices.emplace_back(positions[2]);
	vertices.emplace_back(positions[6]);
	vertices.emplace_back(positions[3]);
	vertices.emplace_back(positions[7]);

	// メッシュ生成
	CreateWiredMesh(device, vertices, wiredBoxMesh);
}

void PrimitiveShapeRenderer::CreateSolidBoxMesh(ID3D11Device* device, float width, float height, float depth)
{
	// 頂点データの定義
	//     ___________
	//   ／|        ／|
	// ／__|______／  |
	// |   |_____|____|
	// |  ／     |  ／
	// |／_______|／
	//            
	// 頂点データに法線データを定義する
	std::vector<Vertex> vertices = {
		// 正面
		{ DirectX::XMFLOAT3(-width, +height, -depth), DirectX::XMFLOAT3(0, 0, -1) },
		{ DirectX::XMFLOAT3(+width, +height, -depth), DirectX::XMFLOAT3(0, 0, -1) },
		{ DirectX::XMFLOAT3(-width, -height, -depth), DirectX::XMFLOAT3(0, 0, -1) },
		{ DirectX::XMFLOAT3(+width, -height, -depth), DirectX::XMFLOAT3(0, 0, -1) },
		// 背面
		{ DirectX::XMFLOAT3(-width, +height, +depth), DirectX::XMFLOAT3(0, 0, 1) },
		{ DirectX::XMFLOAT3(+width, +height, +depth), DirectX::XMFLOAT3(0, 0, 1) },
		{ DirectX::XMFLOAT3(-width, -height, +depth), DirectX::XMFLOAT3(0, 0, 1) },
		{ DirectX::XMFLOAT3(+width, -height, +depth), DirectX::XMFLOAT3(0, 0, 1) },
		// 右面
		{ DirectX::XMFLOAT3(+width, +height, -depth), DirectX::XMFLOAT3(1, 0, 0) },
		{ DirectX::XMFLOAT3(+width, +height, +depth), DirectX::XMFLOAT3(1, 0, 0) },
		{ DirectX::XMFLOAT3(+width, -height, -depth), DirectX::XMFLOAT3(1, 0, 0) },
		{ DirectX::XMFLOAT3(+width, -height, +depth), DirectX::XMFLOAT3(1, 0, 0) },
		// 左面
		{ DirectX::XMFLOAT3(-width, +height, -depth), DirectX::XMFLOAT3(-1, 0, 0) },
		{ DirectX::XMFLOAT3(-width, +height, +depth), DirectX::XMFLOAT3(-1, 0, 0) },
		{ DirectX::XMFLOAT3(-width, -height, -depth), DirectX::XMFLOAT3(-1, 0, 0) },
		{ DirectX::XMFLOAT3(-width, -height, +depth), DirectX::XMFLOAT3(-1, 0, 0) },
		// 上面
		{ DirectX::XMFLOAT3(-width, +height, +depth), DirectX::XMFLOAT3(0, 1, 0) },
		{ DirectX::XMFLOAT3(+width, +height, +depth), DirectX::XMFLOAT3(0, 1, 0) },
		{ DirectX::XMFLOAT3(-width, +height, -depth), DirectX::XMFLOAT3(0, 1, 0) },
		{ DirectX::XMFLOAT3(+width, +height, -depth), DirectX::XMFLOAT3(0, 1, 0) },
		// 下面
		{ DirectX::XMFLOAT3(-width, -height, +depth), DirectX::XMFLOAT3(0, -1, 0) },
		{ DirectX::XMFLOAT3(+width, -height, +depth), DirectX::XMFLOAT3(0, -1, 0) },
		{ DirectX::XMFLOAT3(-width, -height, -depth), DirectX::XMFLOAT3(0, -1, 0) },
		{ DirectX::XMFLOAT3(+width, -height, -depth), DirectX::XMFLOAT3(0, -1, 0) },
	};

	// インデックスデータ
	std::vector<uint16_t> indices = {
		// 正面
		0, 1, 2,
		2, 1, 3,
		// 背面
		5, 4, 7,
		7, 4, 6,
		// 右面
		8, 9, 10,
		10, 9, 11,
		// 左面
		13, 12, 15,
		15, 12, 14,
		// 上面
		16, 17, 18,
		18, 17, 19,
		// 下面
		21, 20, 23,
		23, 20, 22,
	};

	CreateSolidMesh(device, vertices, indices, solidBoxMesh);
}


// 球メッシュ作成
void PrimitiveShapeRenderer::CreateWiredSphereMesh(ID3D11Device* device, float radius, int subdivisions)
{
	float step = DirectX::XM_2PI / subdivisions;

	std::vector<DirectX::XMFLOAT3> vertices;

	// XZ平面
	for (int i = 0; i < subdivisions; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& p = vertices.emplace_back();
			p.x = sinf(theta) * radius;
			p.y = 0.0f;
			p.z = cosf(theta) * radius;
		}
	}
	// XY平面
	for (int i = 0; i < subdivisions; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& p = vertices.emplace_back();
			p.x = sinf(theta) * radius;
			p.y = cosf(theta) * radius;
			p.z = 0.0f;
		}
	}
	// YZ平面
	for (int i = 0; i < subdivisions; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& p = vertices.emplace_back();
			p.x = 0.0f;
			p.y = sinf(theta) * radius;
			p.z = cosf(theta) * radius;
		}
	}

	// メッシュ生成
	CreateWiredMesh(device, vertices, wiredSphereMesh);
}

void PrimitiveShapeRenderer::CreateSolidSphereMesh(ID3D11Device* device, float radius, int subdivisions)
{
	int stacks = subdivisions;
	int slices = subdivisions;
	int vertex_count = slices * (stacks - 1) + 2;
	int index_count = (slices - 1) * 3
		+ 3
		+ (stacks - 2) * (slices - 1) * 6
		+ (stacks - 2) * 6
		+ (slices - 1) * 3
		+ 3;
	std::vector<Vertex> vertices(vertex_count);
	std::vector<uint16_t> indices(index_count);

	// caches sin cos.
	std::unique_ptr<float[]> cos_phi = std::make_unique<float[]>(stacks);
	std::unique_ptr<float[]> sin_phi = std::make_unique<float[]>(stacks);
	float phi_step = DirectX::XM_PI / stacks;

	std::unique_ptr<float[]> cos_theta = std::make_unique<float[]>(slices);
	std::unique_ptr<float[]> sin_theta = std::make_unique<float[]>(slices);
	float theta_step = DirectX::XM_2PI / slices;

	float phi = 0;
	for (int s = 0; s < stacks; s++, phi += phi_step)
	{
		sin_phi[s] = sinf(phi);
		cos_phi[s] = cosf(phi);
	}

	float theta = 0;
	for (int s = 0; s < slices; s++, theta += theta_step)
	{
		sin_theta[s] = sinf(theta);
		cos_theta[s] = cosf(theta);
	}

	Vertex* vertex = vertices.data();

	// north pole. 
	vertex->position.x = 0;
	vertex->position.y = radius;
	vertex->position.z = 0;

	vertex++;

	for (int s = 1; s < stacks; s++)
	{
		float y = radius * cos_phi[s];
		float r = radius * sin_phi[s];
		for (int l = 0; l < slices; l++)
		{
			vertex->position.x = r * sin_theta[l];
			vertex->position.y = y;
			vertex->position.z = r * cos_theta[l];
			vertex++;
		}
	}

	// south_pole
	vertex->position.x = 0;
	vertex->position.y = -radius;
	vertex->position.z = 0;

	for (Vertex& v : vertices)
	{
		DirectX::XMStoreFloat3(&v.normal, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&v.position)));
	}


	// 2l + (s-2) * 2l
	// 2l * ( 1 + s-2)
	// 2l * ( s- 1)

	uint16_t* index = indices.data();

	// create index of north pole cap.
	for (int l = 1; l < slices; l++)
	{
		index[0] = l + 1;
		index[1] = 0;
		index[2] = l;

		index += 3;
	}
	index[0] = 1;
	index[1] = 0;
	index[2] = slices;

	index += 3;

	for (int s = 0; s < (stacks - 2); s++)
	{
		int l = 1;
		for (; l < slices; l++)
		{
			index[0] = ((s + 1) * slices + l + 1);	// bottom right.
			index[1] = (s * slices + l + 1);		// top right.
			index[2] = (s * slices + l);			// top left.

			index[3] = ((s + 1) * slices + l);		// bottom left.
			index[4] = ((s + 1) * slices + l + 1);	// bottom right.
			index[5] = (s * slices + l);			// top left.

			index += 6;
		}

		index[0] = ((s + 1) * slices + 1);			// bottom right.
		index[1] = (s * slices + 1);				// top right.
		index[2] = (s * slices + slices);			// top left.


		index[3] = ((s + 1) * slices + slices);		// bottom left.
		index[4] = ((s + 1) * slices + 1);			// bottom right.
		index[5] = (s * slices + slices);			// top left.

		index += 6;
	}

	// create index for south pole cap.
	int base_index = slices * (stacks - 2);
	int last_index = vertex_count - 1;
	for (int l = 1; l < slices; l++)
	{
		index[0] = (base_index + l);
		index[1] = (last_index);
		index[2] = (base_index + l + 1);

		index += 3;
	}
	index[0] = (base_index + slices);
	index[1] = last_index;
	index[2] = base_index + 1;

	CreateSolidMesh(device, vertices, indices, solidSphereMesh);
}

// 半球メッシュ作成
void PrimitiveShapeRenderer::CreateWiredHalfSphereMesh(ID3D11Device* device, float radius, int subdivisions)
{
	std::vector<DirectX::XMFLOAT3> vertices;

	float theta_step = DirectX::XM_2PI / subdivisions;

	// XZ平面
	for (int i = 0; i < subdivisions; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = theta_step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& v = vertices.emplace_back();

			v.x = sinf(theta) * radius;
			v.y = 0.0f;
			v.z = cosf(theta) * radius;
		}
	}
	// XY平面
	for (int i = 0; i < subdivisions / 2; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = theta_step * ((i + j) % subdivisions) - DirectX::XM_PIDIV2;

			DirectX::XMFLOAT3& v = vertices.emplace_back();

			v.x = sinf(theta) * radius;
			v.y = cosf(theta) * radius;
			v.z = 0.0f;
		}
	}
	// YZ平面
	for (int i = 0; i < subdivisions / 2; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = theta_step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& v = vertices.emplace_back();

			v.x = 0.0f;
			v.y = sinf(theta) * radius;
			v.z = cosf(theta) * radius;
		}
	}

	// メッシュ生成
	CreateWiredMesh(device, vertices, wiredHalfSphereMesh);
}

void PrimitiveShapeRenderer::CreateSolidHalfSphereMesh(ID3D11Device* device, float radius, int subdivisions)
{
	int stacks = subdivisions;
	int slices = subdivisions;
	int vertex_count = slices * (stacks - 1) + 1;
	int index_count = (slices - 1) * 3
		+ 3
		+ (stacks - 2) * (slices - 1) * 6
		+ (stacks - 2) * 6;

	std::vector<Vertex> vertices(vertex_count);
	std::vector<uint16_t> indices(index_count);

	// caches sin cos.
	std::unique_ptr<float[]> cos_phi = std::make_unique<float[]>(stacks);
	std::unique_ptr<float[]> sin_phi = std::make_unique<float[]>(stacks);
	float phi_step = DirectX::XM_PI / (stacks - 1) * 0.5f;

	std::unique_ptr<float[]> cos_theta = std::make_unique<float[]>(slices);
	std::unique_ptr<float[]> sin_theta = std::make_unique<float[]>(slices);
	float theta_step = DirectX::XM_2PI / slices;

	float phi = 0;
	for (int s = 0; s < stacks; s++, phi += phi_step)
	{
		sin_phi[s] = sinf(phi);
		cos_phi[s] = cosf(phi);
	}

	float theta = 0;
	for (int s = 0; s < slices; s++, theta += theta_step)
	{
		sin_theta[s] = sinf(theta);
		cos_theta[s] = cosf(theta);
	}

	Vertex* vertex = vertices.data();

	// north pole. 
	vertex->position.x = 0;
	vertex->position.y = radius;
	vertex->position.z = 0;
	vertex++;

	for (int s = 1; s < stacks; s++)
	{
		float y = radius * cos_phi[s];
		float r = radius * sin_phi[s];
		for (int l = 0; l < slices; l++)
		{
			vertex->position.x = r * sin_theta[l];
			vertex->position.y = y;
			vertex->position.z = r * cos_theta[l];
			vertex++;
		}
	}

	for (Vertex& v : vertices)
	{
		DirectX::XMStoreFloat3(&v.normal, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&v.position)));
	}

	// 2l + (s-2) * 2l
	// 2l * ( 1 + s-2)
	// 2l * ( s- 1)

	uint16_t* index = indices.data();

	// create index of north pole cap.
	for (int l = 1; l < slices; l++)
	{
		index[0] = l + 1;
		index[1] = 0;
		index[2] = l;

		index += 3;
	}
	index[0] = 1;
	index[1] = 0;
	index[2] = slices;

	index += 3;

	for (int s = 0; s < (stacks - 2); s++)
	{
		int l = 1;
		for (; l < slices; l++)
		{
			index[0] = ((s + 1) * slices + l + 1);	// bottom right.
			index[1] = (s * slices + l + 1);		// top right.
			index[2] = (s * slices + l);			// top left.

			index[3] = ((s + 1) * slices + l);		// bottom left.
			index[4] = ((s + 1) * slices + l + 1);	// bottom right.
			index[5] = (s * slices + l);			// top left.

			index += 6;
		}

		index[0] = ((s + 1) * slices + 1);			// bottom right.
		index[1] = (s * slices + 1);				// top right.
		index[2] = (s * slices + slices);			// top left.


		index[3] = ((s + 1) * slices + slices);		// bottom left.
		index[4] = ((s + 1) * slices + 1);			// bottom right.
		index[5] = (s * slices + slices);			// top left.

		index += 6;
	}

	CreateSolidMesh(device, vertices, indices, solidHalfSphereMesh);
}

// 円柱
void PrimitiveShapeRenderer::CreateWiredCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int subdivisions)
{
	std::vector<DirectX::XMFLOAT3> vertices;

	float theta_step = DirectX::XM_2PI / subdivisions;

	// XZ平面
	for (int i = 0; i < subdivisions; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = theta_step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& v = vertices.emplace_back();

			v.x = sinf(theta) * radius1;
			v.y = start;
			v.z = cosf(theta) * radius1;
		}
	}
	for (int i = 0; i < subdivisions; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float theta = theta_step * ((i + j) % subdivisions);

			DirectX::XMFLOAT3& v = vertices.emplace_back();

			v.x = sinf(theta) * radius2;
			v.y = start + height;
			v.z = cosf(theta) * radius2;
		}
	}
	// XY平面
	{
		vertices.emplace_back(DirectX::XMFLOAT3(0.0f, start, radius1));
		vertices.emplace_back(DirectX::XMFLOAT3(0.0f, start + height, radius2));
		vertices.emplace_back(DirectX::XMFLOAT3(0.0f, start, -radius1));
		vertices.emplace_back(DirectX::XMFLOAT3(0.0f, start + height, -radius2));
	}
	// YZ平面
	{
		vertices.emplace_back(DirectX::XMFLOAT3(radius1, start, 0.0f));
		vertices.emplace_back(DirectX::XMFLOAT3(radius2, start + height, 0.0f));
		vertices.emplace_back(DirectX::XMFLOAT3(-radius1, start, 0.0f));
		vertices.emplace_back(DirectX::XMFLOAT3(-radius2, start + height, 0.0f));
	}

	// メッシュ生成
	CreateWiredMesh(device, vertices, wiredCylinderMesh);
}

void PrimitiveShapeRenderer::CreateSolidCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int subdivisions, bool cap)
{
	int stacks = 1;
	int slices = subdivisions;
	float stack_height = height / stacks;

	// Amount to increment radius as we move up each stack level from bottom to top.
	float radius_step = (radius2 - radius1) / stacks;

	int num_rings = stacks + 1;

	int vertex_count = num_rings * (slices + 1);
	int index_count = stacks * slices * 6;
	if (radius1 > 0)
	{
		vertex_count += slices + 1 + 1;
		index_count += slices * 3;
	}
	if (radius2 > 0)
	{
		vertex_count += slices + 1 + 1;
		index_count += slices * 6;
	}

	std::vector<Vertex> vertices(vertex_count);
	std::vector<uint16_t> indices(index_count);

	Vertex* vertex = vertices.data();

	int vcount = 0;

	// Compute vertices for each stack ring.
	for (int i = 0; i < num_rings; ++i)
	{
		float y = start + i * stack_height;
		float r = radius1 + i * radius_step;

		// Height and radius of next ring up.
		float y_next = start + (i + 1) * stack_height;
		float r_next = radius1 + (i + 1) * radius_step;

		// vertices of ring
		float d_theta = DirectX::XM_2PI / slices;
		for (int j = 0; j <= slices; ++j)
		{
			float c = cosf(j * d_theta);
			float s = sinf(j * d_theta);

			// Partial derivative in theta direction to get tangent vector (this is a unit vector).
			DirectX::XMFLOAT3 t = { -s, 0.0f, c };
			DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&t);

			// Compute tangent vector down the slope of the cone (if the top/bottom 
			// radii differ then we get a cone and not a true cylinder).
			DirectX::XMFLOAT3 p = { r * c, y, r * s };
			DirectX::XMFLOAT3 p_next = { r_next * c, y_next, r_next * s };
			DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&p);
			DirectX::XMVECTOR P_Next = DirectX::XMLoadFloat3(&p_next);
			DirectX::XMVECTOR B = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(P, P_Next));
			DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T, B));
			DirectX::XMStoreFloat3(&vertex->position, P);
			DirectX::XMStoreFloat3(&vertex->normal, N);

			// flip to right hand
			vertex->position.z *= -1;
			vertex->normal.z *= -1;

			vertex++;
			vcount++;
		}
	}

	int num_ring_vertices = slices + 1;

	uint16_t* index = indices.data();

	// Compute indices for each stack.
	for (int i = 0; i < stacks; ++i)
	{
		for (int j = 0; j < slices; ++j)
		{
			index[0] = (i * num_ring_vertices + j);
			index[1] = ((i + 1) * num_ring_vertices + j + 1);
			index[2] = ((i + 1) * num_ring_vertices + j);

			index[3] = (i * num_ring_vertices + j);
			index[4] = (i * num_ring_vertices + j + 1);
			index[5] = ((i + 1) * num_ring_vertices + j + 1);

			index += 6;
		}
	}

	if (cap)
	{
		// build bottom cap.
		if (radius1 > 0)
		{
			int base_index = vcount;
			// Duplicate cap vertices because the texture coordinates and normals differ.
			float y = start;

			// vertices of ring
			float d_theta = DirectX::XM_2PI / slices;
			for (int i = 0; i <= slices; ++i)
			{
				float x = radius1 * cosf(i * d_theta);
				float z = radius1 * sinf(i * d_theta);

				vertex->position.x = x;
				vertex->position.y = y;
				vertex->position.z = -z;

				vertex->normal.x = 0.0f;
				vertex->normal.y = -1.0f;
				vertex->normal.z = 0.0f;

				vertex++;
				vcount++;
			}


			// cap center vertex
			vertex->position.x = 0;
			vertex->position.y = y;
			vertex->position.z = 0;

			vertex->normal.x = 0.0f;
			vertex->normal.y = -1.0f;
			vertex->normal.z = 0.0f;

			vertex++;
			vcount++;

			// index of center vertex
			int center_index = vcount - 1;
			for (int i = 0; i < slices; ++i)
			{
				index[0] = center_index;
				index[1] = base_index + i + 1;
				index[2] = base_index + i;
				index += 3;
			}
		}

		// build top cap.
		if (radius2 > 0)
		{
			int base_index = vcount;

			// Duplicate cap vertices because the texture coordinates and normals differ.
			float y = start + height;

			// vertices of ring
			float d_theta = DirectX::XM_2PI / slices;
			for (int i = 0; i <= slices; ++i)
			{
				float x = radius2 * cosf(i * d_theta);
				float z = radius2 * sinf(i * d_theta);

				vertex->position.x = x;
				vertex->position.y = y;
				vertex->position.z = -z;

				vertex->normal.x = 0.0f;
				vertex->normal.y = 1.0f;
				vertex->normal.z = 0.0f;

				vertex++;
				vcount++;
			}

			// pos, norm, tex1 for cap center vertex
			vertex->position.x = 0;
			vertex->position.y = y;
			vertex->position.z = 0;

			vertex->normal.x = 0.0f;
			vertex->normal.y = 1.0f;
			vertex->normal.z = 0.0f;

			vertex++;
			vcount++;

			// index of center vertex
			int center_index = vcount - 1;
			for (int i = 0; i < slices; ++i)
			{
				index[0] = center_index;
				index[1] = base_index + i + 1;
				index[2] = base_index + i;

				index[3] = center_index;
				index[4] = base_index + i;
				index[5] = base_index + i + 1;

				index += 6;
			}
		}
	}
	else
	{
		vertex_count = static_cast<int>(vertex - vertices.data());
		index_count = static_cast<int>(index - indices.data());

		vertices.resize(vertex_count);
		indices.resize(index_count);
	}

	CreateSolidMesh(device, vertices, indices, solidCylinderMesh);
}

// 描画実行
void PrimitiveShapeRenderer::Render(
	ID3D11DeviceContext* dc,
	const DirectX::XMFLOAT4X4& view,
	const DirectX::XMFLOAT4X4& projection,
	const DirectX::XMFLOAT3& lightDirection)
{
	// ビュープロジェクション行列作成
	DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&view);
	DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&projection);
	DirectX::XMMATRIX VP = V * P;

	// ソリッド
	{
		// シェーダー設定
		dc->VSSetShader(solidVertexShader.Get(), nullptr, 0);
		dc->PSSetShader(solidPixelShader.Get(), nullptr, 0);
		dc->IASetInputLayout(solidInputLayout.Get());

		// 定数バッファ設定
		//dc->VSSetConstantBuffers(0, 1, solidConstantBuffer.GetAddressOf());
		//dc->PSSetConstantBuffers(0, 1, solidConstantBuffer.GetAddressOf());

		// プリミティブ設定
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		CbSolidMesh cbMesh;
		DirectX::XMStoreFloat4x4(&cbMesh.viewProjection, VP);
		cbMesh.lightDirection.x = lightDirection.x;
		cbMesh.lightDirection.y = lightDirection.y;
		cbMesh.lightDirection.z = lightDirection.z;
		for (const Instance& instance : instances)
		{
			// 定数バッファ更新
			cbMesh.world = instance.worldTransform;
			cbMesh.color = instance.color;
			//dc->UpdateSubresource(solidConstantBuffer.Get(), 0, 0, &cbMesh, 0, 0);
            solidConstantBuffer->data = cbMesh;
			// 定数バッファ設定
			solidConstantBuffer->Activate(dc, 0);

			// 頂点バッファ設定
			dc->IASetVertexBuffers(0, 1, instance.solidMesh->vertexBuffer.GetAddressOf(), &stride, &offset);
			dc->IASetIndexBuffer(instance.solidMesh->indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

			// 描画
			dc->DrawIndexed(instance.solidMesh->indexCount, 0, 0);
		}
	}

	// ワイヤーフレーム
	{
		// シェーダー設定
		dc->VSSetShader(wiredVertexShader.Get(), nullptr, 0);
		dc->PSSetShader(wiredPixelShader.Get(), nullptr, 0);
		dc->IASetInputLayout(wiredInputLayout.Get());

		// 定数バッファ設定
		//dc->VSSetConstantBuffers(0, 1, wiredConstantBuffer.GetAddressOf());
		//dc->PSSetConstantBuffers(0, 1, wiredConstantBuffer.GetAddressOf());

		// プリミティブ設定
		UINT stride = sizeof(DirectX::XMFLOAT3);
		UINT offset = 0;
		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		for (const Instance& instance : instances)
		{
			// ワールドビュープロジェクション行列作成
			DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&instance.worldTransform);
			DirectX::XMMATRIX WVP = W * VP;

			// 定数バッファ更新
			CbWiredMesh cbMesh;
			DirectX::XMStoreFloat4x4(&cbMesh.worldViewProjection, WVP);
			cbMesh.color = instance.color;
			cbMesh.color.w = 1.0f;
            wiredConstantBuffer->data = cbMesh;
            wiredConstantBuffer->Activate(dc, 0);
			//dc->UpdateSubresource(wiredConstantBuffer.Get(), 0, 0, &cbMesh, 0, 0);

			// 頂点バッファ設定
			dc->IASetVertexBuffers(0, 1, instance.wiredMesh->vertexBuffer.GetAddressOf(), &stride, &offset);

			// 描画
			dc->Draw(instance.wiredMesh->vertexCount, 0);
		}
	}
	instances.clear();
}
