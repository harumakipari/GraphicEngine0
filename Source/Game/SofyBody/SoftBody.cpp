#include "pch.h"
#include "SoftBody.h"
// INTERLEAVED_GLTF_MODEL
#include <functional>
#include <filesystem>
#include <fstream>

//#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "../External/imgui/imgui.h"
#endif

#include "Engine/Utility/Win32Utils.h"
#include "Engine/Debug/Assert.h"
#include "Engine/Serialization/DirectXSerializers.h"
#include "Graphics/Core/Shader.h"
#include "Graphics/Resource/Texture.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/PipelineState.h"

#include "Graphics/Core/Graphics.h"

#define NUMTHREAD_X 16

bool nullLoadImageData_(tinygltf::Image*, const int, std::string*, std::string*, int, int, const unsigned char*, int, void*)
{
    return true;
}


SoftBodySimulate::SoftBodySimulate(ID3D11Device* device, const std::string& filename) : filename(filename)
{
    std::filesystem::path cerealFilename(filename);
    cerealFilename.replace_extension("soft_cereal");
    if (std::filesystem::exists(cerealFilename.c_str()))
    {
        std::ifstream ifs(cerealFilename.c_str(), std::ios::binary);
        cereal::BinaryInputArchive deserialization(ifs);
        deserialization(
            cereal::make_nvp("scenes", scenes),
            cereal::make_nvp("defaultScene", defaultScene),
            cereal::make_nvp("nodes", nodes),
            cereal::make_nvp("materials", materials)
        );
        deserialization(cereal::make_nvp("meshes", meshes));
        deserialization(cereal::make_nvp("textures", textures), cereal::make_nvp("images", images));
    }
    else
    {
        tinygltf::TinyGLTF tinyGltf;
        tinyGltf.SetImageLoader(nullLoadImageData_, nullptr);

        std::string error, warning;
        bool succeeded = false;

        if (cachedGltfModels.find(filename) != cachedGltfModels.end() && !cachedGltfModels.at(filename).expired())
        {
            //キャッシュされたデータからモデルデータ取得
            gltfModel = cachedGltfModels.at(filename).lock();
        }
        else
        {
            gltfModel = std::make_shared<tinygltf::Model>();

            if (filename.find(".glb") != std::string::npos)
            {
                succeeded = tinyGltf.LoadBinaryFromFile(gltfModel.get(), &error, &warning, filename.c_str());
            }
            else if (filename.find(".gltf") != std::string::npos)
            {
                succeeded = tinyGltf.LoadASCIIFromFile(gltfModel.get(), &error, &warning, filename.c_str());
            }

            _ASSERT_EXPR_A(warning.empty(), warning.c_str());
            _ASSERT_EXPR_A(error.empty(), error.c_str());
            _ASSERT_EXPR_A(succeeded, L"Failed to load glTF file");

            //キャッシュ追加
            cachedGltfModels[filename] = gltfModel;
        }

        for (const tinygltf::Scene& gltfScene : gltfModel->scenes)
        {
            Scene& scene = scenes.emplace_back();
            scene.name = gltfScene.name;
            scene.nodes = gltfScene.nodes;
        }
        defaultScene = gltfModel->defaultScene < 0 ? 0 : gltfModel->defaultScene;

        FetchNodes(*gltfModel);
        FetchMaterials(device, *gltfModel);
        FetchTextures(device, *gltfModel);
        FetchMeshes(device, *gltfModel);

        std::ofstream ofs(cerealFilename.c_str(), std::ios::binary);
        cereal::BinaryOutputArchive serialization(ofs);
        serialization(
            cereal::make_nvp("scenes", scenes),
            cereal::make_nvp("defaultScene", defaultScene),
            cereal::make_nvp("nodes", nodes),
            cereal::make_nvp("materials", materials)
        );
        serialization(cereal::make_nvp("meshes", meshes));
        serialization(cereal::make_nvp("textures", textures), cereal::make_nvp("images", images));
    }
    cbuffer_ = std::make_unique<ConstantBuffer<ClothSimulateCBuffer>>(device);

    CreateAndUploadResources(device);
}


void SoftBodySimulate::FetchNodes(const tinygltf::Model& gltfModel)
{
    for (const tinygltf::Node& gltfNode : gltfModel.nodes)
    {
        Node& node = nodes.emplace_back();
        node.name = gltfNode.name;
        node.skin = gltfNode.skin;
        node.mesh = gltfNode.mesh;
        node.children = gltfNode.children;
        if (!gltfNode.matrix.empty())
        {
            DirectX::XMFLOAT4X4 matrix;
            for (size_t row = 0; row < 4; row++)
            {
                for (size_t column = 0; column < 4; column++)
                {
                    matrix(row, column) = static_cast<float>(gltfNode.matrix.at(4 * row + column));
                }
            }

            DirectX::XMVECTOR S, T, R;
            bool succeed = DirectX::XMMatrixDecompose(&S, &R, &T, DirectX::XMLoadFloat4x4(&matrix));
            _ASSERT_EXPR(succeed, L"Failed to decompose matrix.");

            DirectX::XMStoreFloat3(&node.scale, S);
            DirectX::XMStoreFloat4(&node.rotation, R);
            DirectX::XMStoreFloat3(&node.translation, T);
        }
        else
        {
            if (gltfNode.scale.size() > 0)
            {
                node.scale.x = static_cast<float>(gltfNode.scale.at(0));
                node.scale.y = static_cast<float>(gltfNode.scale.at(1));
                node.scale.z = static_cast<float>(gltfNode.scale.at(2));
            }
            if (gltfNode.translation.size() > 0)
            {
                node.translation.x = static_cast<float>(gltfNode.translation.at(0));
                node.translation.y = static_cast<float>(gltfNode.translation.at(1));
                node.translation.z = static_cast<float>(gltfNode.translation.at(2));
            }
            if (gltfNode.rotation.size() > 0)
            {
                node.rotation.x = static_cast<float>(gltfNode.rotation.at(0));
                node.rotation.y = static_cast<float>(gltfNode.rotation.at(1));
                node.rotation.z = static_cast<float>(gltfNode.rotation.at(2));
                node.rotation.w = static_cast<float>(gltfNode.rotation.at(3));
            }
        }
    }
    CumulateTransforms(nodes);
}

void SoftBodySimulate::CumulateTransforms(std::vector<Node>& nodes)
{
    std::function<void(int, int)> traverse = [&](int parentIndex, int nodeIndex)->void
        {
            DirectX::XMMATRIX P = parentIndex > -1 ? DirectX::XMLoadFloat4x4(&nodes.at(parentIndex).globalTransform) : DirectX::XMMatrixIdentity();

            Node& node = nodes.at(nodeIndex);
            DirectX::XMMATRIX S = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
            DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
            DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z);
            DirectX::XMStoreFloat4x4(&node.globalTransform, S * R * T * P);

            for (int childIndex : node.children)
            {
                traverse(nodeIndex, childIndex);
            }
        };
    for (int nodeIndex : scenes.at(defaultScene).nodes)
    {
        traverse(-1, nodeIndex);
    }
}

void SoftBodySimulate::FetchMeshes(ID3D11Device* device, const tinygltf::Model& gltfModel)
{
    for (const tinygltf::Mesh& gltfMesh : gltfModel.meshes)
    {
        Mesh& mesh = meshes.emplace_back();
        mesh.name = gltfMesh.name;

        for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
        {
            Mesh::Primitive& primitive = mesh.primitives.emplace_back();
            primitive.material = gltfPrimitive.material;

            // Create index buffer view
            if (gltfPrimitive.indices > -1)
            {
                const tinygltf::Accessor& gltfAccessor = gltfModel.accessors.at(gltfPrimitive.indices);
                const tinygltf::BufferView& gltfBufferView = gltfModel.bufferViews.at(gltfAccessor.bufferView);

                primitive.indexBufferView.format = DxgiFormat(gltfAccessor);
                primitive.indexBufferView.sizeInBytes = static_cast<UINT>(gltfAccessor.count) * sizeofComponent(primitive.indexBufferView.format);
                primitive.cachedIndices.resize(primitive.indexBufferView.sizeInBytes);
                const unsigned char* data = gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset;

                memcpy_s(primitive.cachedIndices.data(), primitive.cachedIndices.size(), data, primitive.indexBufferView.sizeInBytes);
            }

            // Create index buffer view
            if (gltfPrimitive.attributes.size() > 0 && gltfPrimitive.attributes.find("POSITION") != gltfPrimitive.attributes.end())
            {
                primitive.cachedVertices.resize(gltfModel.accessors.at(gltfPrimitive.attributes.at("POSITION")).count);
            }
            else
            {
                continue;
            }
            for (std::map<std::string, int>::const_reference gltfAttribute : gltfPrimitive.attributes)
            {
                const tinygltf::Accessor& gltfAccessor = gltfModel.accessors.at(gltfAttribute.second);
                const tinygltf::BufferView& gltfBufferView = gltfModel.bufferViews.at(gltfAccessor.bufferView);

                const unsigned char* sData = gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset;
                const size_t sStride = gltfAccessor.ByteStride(gltfBufferView);
                const size_t dStride = sizeof(Mesh::Vertex);
                if (gltfAttribute.first == "POSITION")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");
                    unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->position);

                    _Copy<DirectX::XMFLOAT3>(dData, dStride, sData, sStride, count);
                }
                else if (gltfAttribute.first == "NORMAL")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    unsigned char* d_data = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->normal);
                    _Copy<DirectX::XMFLOAT3>(d_data, dStride, sData, sStride, count);
                }
                else if (gltfAttribute.first == "TANGENT")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->tangent);
                    _Copy<DirectX::XMFLOAT4>(dData, dStride, sData, sStride, count);
                }
                else if (gltfAttribute.first == "TEXCOORD_0")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->texcoord);
                    _Copy<DirectX::XMFLOAT2>(dData, dStride, sData, sStride, count);
                }
                else
                {
                    //_ASSERT_EXPR(FALSE, L"This attribute is unsupported.");
                }
                primitive.attributes.emplace(gltfAttribute.first, DxgiFormat(gltfAccessor));
            }
            primitive.vertexBufferView.sizeInBytes = static_cast<UINT>(primitive.cachedVertices.size() * sizeof(Mesh::Vertex));
        }
    }

    // mesh に紐づく Node を後から更新
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        Node& node = nodes[i];
        if (node.mesh != -1 && node.mesh < meshes.size())
        {
            const Mesh& mesh = meshes[node.mesh];

            DirectX::XMVECTOR minVec = DirectX::XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.0f);
            DirectX::XMVECTOR maxVec = DirectX::XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);

            for (const auto& primitive : mesh.primitives)
            {
                if (primitive.cachedVertices.empty())
                {
                    OutputDebugStringA(("primitive empty: mesh=" + std::to_string(node.mesh) + "\n").c_str());
                    continue;
                }
                for (const auto& vertex : primitive.cachedVertices)
                {
                    DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&vertex.position);
                    minVec = DirectX::XMVectorMin(minVec, pos);
                    maxVec = DirectX::XMVectorMax(maxVec, pos);
                }
            }
            // minVec / maxVec が一度も更新されなかった場合、デバッグ出力
            DirectX::XMFLOAT3 minF, maxF;
            DirectX::XMStoreFloat3(&minF, minVec);
            DirectX::XMStoreFloat3(&maxF, maxVec);

            if (minF.x == FLT_MAX || maxF.x == -FLT_MAX)
            {
                OutputDebugStringA(("No vertices found for node " + std::to_string(i) + "\n").c_str());
            }
            DirectX::XMStoreFloat3(&node.minValue, minVec);
            DirectX::XMStoreFloat3(&node.maxValue, maxVec);
        }
    }
}

void SoftBodySimulate::FetchMaterials(ID3D11Device* device, const tinygltf::Model& gltfModel)
{
    for (const tinygltf::Material& gltfMaterial : gltfModel.materials)
    {
        std::vector<Material>::reference material = materials.emplace_back();

        material.name = gltfMaterial.name;

        material.data.emissiveFactor[0] = static_cast<float>(gltfMaterial.emissiveFactor.at(0));
        material.data.emissiveFactor[1] = static_cast<float>(gltfMaterial.emissiveFactor.at(1));
        material.data.emissiveFactor[2] = static_cast<float>(gltfMaterial.emissiveFactor.at(2));

        material.data.alphaMode = gltfMaterial.alphaMode == "OPAQUE" ? 0 : gltfMaterial.alphaMode == "MASK" ? 1 : gltfMaterial.alphaMode == "BLEND" ? 2 : 0;
        material.data.alphaCutoff = static_cast<float>(gltfMaterial.alphaCutoff);
        material.data.doubleSided = gltfMaterial.doubleSided ? 1 : 0;

        material.data.pbrMetallicRoughness.basecolorFactor[0] = static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor.at(0));
        material.data.pbrMetallicRoughness.basecolorFactor[1] = static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor.at(1));
        material.data.pbrMetallicRoughness.basecolorFactor[2] = static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor.at(2));
        material.data.pbrMetallicRoughness.basecolorFactor[3] = static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor.at(3));
        material.data.pbrMetallicRoughness.basecolorTexture.index = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
        material.data.pbrMetallicRoughness.basecolorTexture.texcoord = gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord;
        material.data.pbrMetallicRoughness.metallicFactor = static_cast<float>(gltfMaterial.pbrMetallicRoughness.metallicFactor);
        material.data.pbrMetallicRoughness.roughnessFactor = static_cast<float>(gltfMaterial.pbrMetallicRoughness.roughnessFactor);
        material.data.pbrMetallicRoughness.metallicRoughnessTexture.index = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
        material.data.pbrMetallicRoughness.metallicRoughnessTexture.texcoord = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;

        material.data.normalTexture.index = gltfMaterial.normalTexture.index;
        material.data.normalTexture.texcoord = gltfMaterial.normalTexture.texCoord;
        material.data.normalTexture.scale = static_cast<float>(gltfMaterial.normalTexture.scale);

        material.data.occlusionTexture.index = gltfMaterial.occlusionTexture.index;
        material.data.occlusionTexture.texcoord = gltfMaterial.occlusionTexture.texCoord;
        material.data.occlusionTexture.strength = static_cast<float>(gltfMaterial.occlusionTexture.strength);

        material.data.emissiveTexture.index = gltfMaterial.emissiveTexture.index;
        material.data.emissiveTexture.texcoord = gltfMaterial.emissiveTexture.texCoord;
    }
}
void SoftBodySimulate::FetchTextures(ID3D11Device* device, const tinygltf::Model& gltfModel)
{
    for (const tinygltf::Texture& gltfTexture : gltfModel.textures)
    {
        Texture& texture = textures.emplace_back();
        texture.name = gltfTexture.name;
        texture.source = gltfTexture.source;
    }
    for (const tinygltf::Image& gltfImage : gltfModel.images)
    {
        Image& image = images.emplace_back();
        image.name = gltfImage.name;
        image.width = gltfImage.width;
        image.height = gltfImage.height;
        image.component = gltfImage.component;
        image.bits = gltfImage.bits;
        image.pixelType = gltfImage.pixel_type;
        image.mimeType = gltfImage.mimeType;
        image.uri = gltfImage.uri;
        image.asIs = gltfImage.as_is;

        if (gltfImage.bufferView > -1)
        {
            const tinygltf::BufferView& bufferView = gltfModel.bufferViews.at(gltfImage.bufferView);
            const tinygltf::Buffer& buffer = gltfModel.buffers.at(bufferView.buffer);
            const unsigned char* data = buffer.data.data() + bufferView.byteOffset;
            image.cacheData.resize(bufferView.byteLength);
            memcpy_s(image.cacheData.data(), image.cacheData.size(), data, bufferView.byteLength);
        }
    }

}

void SoftBodySimulate::Update(float deltaTine)
{
    int currentPinMode = 0;

#ifdef USE_IMGUI

#endif


}


void SoftBodySimulate::CreateAndUploadResources(ID3D11Device* device)
{
    HRESULT hr;
    D3D11_BUFFER_DESC bufferDesc = {};
    D3D11_SUBRESOURCE_DATA subresourceData = {};

    // Create and upload vertex and index buffers on GPU
    {
        for (Mesh& mesh : meshes)
        {
            for (Mesh::Primitive& primitive : mesh.primitives)
            {
                if (primitive.indexBufferView.sizeInBytes > 0)
                {
                    primitive.indexBufferView.buffer = static_cast<int>(buffers.size());
                    bufferDesc.ByteWidth = primitive.indexBufferView.sizeInBytes;
                    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
                    bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                    bufferDesc.CPUAccessFlags = 0;
                    bufferDesc.MiscFlags = 0;
                    bufferDesc.StructureByteStride = 0;
                    subresourceData.pSysMem = primitive.cachedIndices.data();
                    subresourceData.SysMemPitch = 0;
                    subresourceData.SysMemSlicePitch = 0;
                    hr = device->CreateBuffer(&bufferDesc, &subresourceData, buffers.emplace_back().GetAddressOf());
                    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
                }

                if (primitive.vertexBufferView.sizeInBytes > 0)
                {
                    primitive.vertexBufferView.buffer = static_cast<int>(buffers.size());

                    bufferDesc.ByteWidth = primitive.vertexBufferView.sizeInBytes;
                    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
                    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                    //bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
                    bufferDesc.CPUAccessFlags = 0;
                    bufferDesc.MiscFlags = 0;
                    bufferDesc.StructureByteStride = 0;
                    subresourceData.pSysMem = primitive.cachedVertices.data();
                    subresourceData.SysMemPitch = 0;
                    subresourceData.SysMemSlicePitch = 0;
                    hr = device->CreateBuffer(&bufferDesc, &subresourceData, buffers.emplace_back().GetAddressOf());
                    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
                }
            }
        }
    }

    // Create and upload materials on GPU
    std::vector<Material::Cbuffer> materialData;
    for (const Material& material : materials)
    {
        materialData.emplace_back(material.data);
    }
    Microsoft::WRL::ComPtr<ID3D11Buffer> materialBuffer;
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Material::Cbuffer) * materialData.size());
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(Material::Cbuffer);
    subresourceData.pSysMem = materialData.data();
    subresourceData.SysMemPitch = 0;
    subresourceData.SysMemSlicePitch = 0;
    hr = device->CreateBuffer(&bufferDesc, &subresourceData, materialBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
    shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    shaderResourceViewDesc.Buffer.NumElements = static_cast<UINT>(materialData.size());
    hr = device->CreateShaderResourceView(materialBuffer.Get(), &shaderResourceViewDesc, materialResourceView.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // Create and upload textures on GPU
    for (Image& image : images)
    {
        if (image.cacheData.size() > 0)
        {
            ID3D11ShaderResourceView* textureResourceView = NULL;
            hr = LoadTextureFromMemory(device, image.cacheData.data(), image.cacheData.size(), &textureResourceView);
            if (hr == S_OK)
            {
                textureResourceViews.emplace_back().Attach(textureResourceView);
            }
            image.cacheData.clear();
        }
        else
        {
            const std::filesystem::path path(filename);
            ID3D11ShaderResourceView* shaderResourceView = NULL;
            std::wstring filename = path.parent_path().concat(L"/").wstring() + std::wstring(image.uri.begin(), image.uri.end());
            hr = LoadTextureFromFile(device, filename.c_str(), &shaderResourceView, NULL);
            if (hr == S_OK)
            {
                textureResourceViews.emplace_back().Attach(shaderResourceView);
            }
        }
    }

    {
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "VELOCITY",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "OLDPOSITION",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "OLDVELOCITY",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "ROTATION",0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "ISPINNED", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = CreateVsFromCSO(device, "./Data/Shaders/ClothSimulateVS.cso", vertexShader.ReleaseAndGetAddressOf(), inputLayout.ReleaseAndGetAddressOf(), inputElementDesc, _countof(inputElementDesc));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelCsmVS.cso", vertexShaderCSM.ReleaseAndGetAddressOf(), NULL, NULL, 0);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    }
    hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelDeferredPS.cso", pixelShader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateGsFromCSO(device, "./Data/Shaders/GltfModelCsmGS.cso", geometryShaderCSM.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/ClothUpdateCS.cso", clothUpdateCS.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/ClothConstraintCS.cso", clothConstraintCS.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    bufferDesc.ByteWidth = sizeof(PrimitiveConstants);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;
    hr = device->CreateBuffer(&bufferDesc, nullptr, primitiveCbuffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    bufferDesc.ByteWidth = sizeof(PrimitiveJointConstants);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;
    hr = device->CreateBuffer(&bufferDesc, NULL, primitiveJointCbuffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // UAVとSRVを作成
    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {// ここでデータを入れる
            primitive.clothVertexOffset = static_cast<uint32_t>(allVertices.size()); // オフセット保存

            allVertices.insert(allVertices.end(),
                primitive.cachedVertices.begin(),
                primitive.cachedVertices.end());
        }
    }

    // グローバルインデックスを一つにまとめる 
    for (auto& mesh : meshes)
    {
        for (auto& prim : mesh.primitives)
        {
            if (prim.indexBufferView.sizeInBytes == 0)
            {
                continue;
            }
            const UINT indexCount = prim.indexBufferView.sizeInBytes / sizeofComponent(prim.indexBufferView.format); // このプリミティブが開始する位置を保存しておく 
            prim.startIndexLocation = static_cast<UINT>(globalIndices.size());
            if (prim.indexBufferView.format == DXGI_FORMAT_R16_UINT)        // ここでキャストしておく
            {
                const uint16_t* src = reinterpret_cast<const uint16_t*>(prim.cachedIndices.data());
                for (UINT i = 0; i < indexCount; ++i)
                {
                    globalIndices.push_back(static_cast<uint32_t>(src[i]) + prim.clothVertexOffset);
                }
            }
            else // DXGI_FORMAT_R32_UINT 
            {
                const uint32_t* src = reinterpret_cast<const uint32_t*>(prim.cachedIndices.data());
                for (UINT i = 0; i < indexCount; ++i)
                {
                    globalIndices.push_back(src[i] + prim.clothVertexOffset);
                }
            }
            prim.indexCount = indexCount;
        }
    }

    // 各頂点に近い頂点８つを探してエッジを作る
    constexpr float maxDistance = 20.0f;
    std::vector<ClothEdge> fEdges;
    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
            // 頂点ごとのエッジのリスト
            std::vector<std::vector<ClothEdge>> vertexEdges(primitive.cachedVertices.size());
            const size_t MAX_EDGES = 8;

            for (size_t i = 0; i < primitive.cachedVertices.size(); i++)
            {
                // 候補を
                struct Candidate
                {
                    uint32_t neighbor;
                    float distance;
                    DirectX::XMFLOAT3 delta;
                };

                std::vector<Candidate> candidates;
                DirectX::XMFLOAT3 pi = primitive.cachedVertices[i].position;

                for (size_t j = 0; j < primitive.cachedVertices.size(); j++)
                {
                    if (i == j) continue;

                    DirectX::XMFLOAT3 pj = primitive.cachedVertices[j].position;
                    DirectX::XMFLOAT3 delta = { pj.x - pi.x, pj.y - pi.y, pj.z - pi.z };
                    float dist2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;

                    // 近すぎる or 遠すぎる頂点はスキップ
                    if (dist2 < 1e-8f || dist2 > maxDistance * maxDistance)
                        continue;

                    candidates.push_back({ (uint32_t)j, sqrtf(dist2), delta });
                }

                // 近い順にソート
                std::sort(candidates.begin(), candidates.end(),
                    [](auto& a, auto& b) { return a.distance < b.distance; });

                // 
                size_t maxEdges = std::min<size_t>(MAX_EDGES, candidates.size());
                for (size_t k = 0; k < maxEdges; ++k)
                {
                    ClothEdge edge;
                    edge.neighbor = candidates[k].neighbor;
                    edge.delta = candidates[k].delta;
                    edge.restLength = candidates[k].distance;
                    vertexEdges[i].push_back(edge);
                }
            }
            for (size_t v = 0; v < vertexEdges.size(); ++v)
            {
                std::vector<ClothEdge>& edges = vertexEdges[v];
                // 一個の長い配列にする   8個ごとに
                for (auto& e : edges)
                {
                    primitive.finalEdges.push_back(e);
                }
            }
        }
    }

    // 全頂点を Particle に変換する
    particles.resize(allVertices.size());
    for (size_t i = 0; i < allVertices.size(); i++)
    {
        particles[i].position = allVertices[i].position;
        particles[i].expectedPosition = allVertices[i].position;
        particles[i].velocity = { 0,0,0 };
        particles[i].force = { 0,0,0 };
        particles[i].invMass = (allVertices[i].isPinned ? 0.0f : 1.0f);
    }

    for (auto& mesh : meshes)
    {
        for (auto& prim : mesh.primitives)
        {
            uint32_t offset = prim.clothVertexOffset;

            for (size_t edgeIndex = 0; edgeIndex < prim.finalEdges.size(); ++edgeIndex)
            {
                const auto& e = prim.finalEdges[edgeIndex];

                DistanceConstraint c;
                c.i0 = offset + static_cast<uint32_t>(edgeIndex);
                c.i1 = offset + e.neighbor;
                c.restLength = e.restLength;

                distanceConstraints.push_back(c);
            }
        }
    }

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(sizeof(Particle) * particles.size());
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    desc.StructureByteStride = sizeof(Particle);
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = particles.data();

    device->CreateBuffer(&desc, &init, &particleBuffer);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = static_cast<UINT>(particles.size());

    device->CreateUnorderedAccessView(particleBuffer.Get(), &uavDesc, &particleUav);

    desc.ByteWidth = static_cast<UINT>(sizeof(DistanceConstraint) * distanceConstraints.size());
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.StructureByteStride = sizeof(DistanceConstraint);
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    init.pSysMem = distanceConstraints.data();

    device->CreateBuffer(&desc, &init, &distanceConstraintBuffer);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srvDesc.BufferEx.NumElements = static_cast<UINT>(distanceConstraints.size());

    device->CreateShaderResourceView(distanceConstraintBuffer.Get(), &srvDesc, &distanceConstraintSrv);

    cbuffer_->data.vertexCount = static_cast<int>(allVertices.size());

    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
            for (auto v : primitive.cachedVertices)
            {
                Particle p;
                p.position = v.position;
                p.expectedPosition = v.position;
                p.force = { 0.0f,0.0f,0.0f };
                p.invMass = 1.0f;
                p.velocity = { 0.0f,0.0f,0.0f };
                particles.push_back(p);
            }
        }
    }


#if 0
    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
            primitive.CreateClothPingPongBuffers(device);
        }
    }

    CreateClothPingPongBuffers(device, allVertices);

#endif // 0
}

void SoftBodySimulate::Simulate(ID3D11DeviceContext* immediateContext)
{
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    cbuffer_->Activate(immediateContext, 10);



#if 0
    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {

            immediateContext->VSSetShaderResources(0, 1, nullSRV);
            immediateContext->PSSetShaderResources(0, 1, nullSRV);
            //----Update
            // UAVをCSにバインド      u0
            immediateContext->CSSetUnorderedAccessViews(0, 1, primitive.clothUAV[b].GetAddressOf(), NULL);
            // SRVをCSにバインド
            immediateContext->CSSetShaderResources(0, 1, primitive.clothSRV[a].GetAddressOf());
            immediateContext->CSSetShaderResources(1, 1, primitive.clothEdgeSRV.GetAddressOf());
            // CSを設定
            immediateContext->CSSetShader(clothUpdateCS.Get(), NULL, 0);
            // ここ確認
            const UINT threadGroupCountX = Align(static_cast<UINT>(primitive.cachedVertices.size()), NUMTHREAD_X) / NUMTHREAD_X;

            //for (int i = 0; i < 10; i++)
            {
                immediateContext->Dispatch(threadGroupCountX, 1, 1);
            }
            // UAVを解除
            ID3D11UnorderedAccessView* nullUAV = {};
            immediateContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, NULL);
            // SRVを解除
            immediateContext->CSSetShaderResources(0, 1, nullSRV);
            immediateContext->CSSetShaderResources(1, 1, nullSRV);

            // bに情報あったけど、、
            // 読み込みと書き込みをswapする
            std::swap(a, b);
            //----Constraint
            {
                const int iteration = 5;
                //for (int i = 0; i < iteration; i++)
                {
                    // UAVをCSにバインド
                    immediateContext->CSSetUnorderedAccessViews(0, 1, primitive.clothUAV[b].GetAddressOf(), NULL);
                    // SRVをCSにバインド
                    immediateContext->CSSetShaderResources(0, 1, primitive.clothSRV[a].GetAddressOf());
                    immediateContext->CSSetShaderResources(1, 1, primitive.clothEdgeSRV.GetAddressOf());
                    // CSを設定
                    immediateContext->CSSetShader(clothConstraintCS.Get(), NULL, 0);
                    // ここ確認
                    immediateContext->Dispatch(threadGroupCountX, 1, 1);
                    // UAVを解除
                    immediateContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, NULL);
                    // SRVを解除
                    immediateContext->CSSetShaderResources(0, 1, nullSRV);
                    immediateContext->CSSetShaderResources(1, 1, nullSRV);
                    //if (i < iteration - 1)
                    {// 最後の一回のみ飛ばす
                        //std::swap(a, b);
                    }
                }
            }

            // ここでシミュレーションの結果のbのUAVを描画のVBにコピーする
            immediateContext->CopyResource(primitive.clothVB.Get(), primitive.clothBuffer[b].Get());

            // ここでスワップする
            std::swap(a, b);
        }
    }
#endif
}

void SoftBodySimulate::CreateClothPingPongBuffers(ID3D11Device* device, const std::vector<Mesh::Vertex>& vertices)
{
    HRESULT hr = S_OK; // バッファ記述 
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Mesh::Vertex) * vertices.size());
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.CPUAccessFlags = 0; // 構造化バッファとして扱う 
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(Mesh::Vertex);
    D3D11_SUBRESOURCE_DATA subresourceData = {};
    subresourceData.pSysMem = vertices.data(); // 2 個作る（ピンポン用） 
    subresourceData.SysMemPitch = 0;
    subresourceData.SysMemSlicePitch = 0;
#if 0
    for (int i = 0; i < 2; ++i)
    {
        hr = device->CreateBuffer(&bufferDesc, &subresourceData, clothBuffer[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        // SRV 作成 
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造化バッファは UNKNOWN 
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>(vertices.size());
        hr = device->CreateShaderResourceView(clothBuffer[i].Get(), &srvDesc, clothSRV[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        // UAV 作成 
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = static_cast<UINT>(vertices.size());
        uavDesc.Buffer.Flags = 0; hr = device->CreateUnorderedAccessView(clothBuffer[i].Get(), &uavDesc, clothUAV[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }


    // エッジ用のSRVを作成
    {
        bufferDesc.ByteWidth = static_cast<UINT>(sizeof(ClothEdge) * finalEdges.size());
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bufferDesc.CPUAccessFlags = 0; // 構造化バッファとして扱う 
        bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bufferDesc.StructureByteStride = sizeof(ClothEdge);
        subresourceData = {};
        subresourceData.pSysMem = finalEdges.data();
        subresourceData.SysMemPitch = 0;
        subresourceData.SysMemSlicePitch = 0;

        hr = device->CreateBuffer(&bufferDesc, &subresourceData, clothEdgeBuffer.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        // SRV 作成 
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造化バッファは UNKNOWN 
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>(finalEdges.size());
        hr = device->CreateShaderResourceView(clothEdgeBuffer.Get(), &srvDesc, clothEdgeSRV.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
#endif // 0

#if 0
    // 描画用のIAVertexBufferを作成
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Mesh::Vertex) * vertices.size());
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;
    subresourceData.pSysMem = vertices.data();
    subresourceData.SysMemPitch = 0;
    subresourceData.SysMemSlicePitch = 0;
    //hr = device->CreateBuffer(&bufferDesc, &subresourceData, clothVB[i].GetAddressOf());
    hr = device->CreateBuffer(&bufferDesc, &subresourceData, clothVB.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // 描画用のIAIndexBufferを作成
    {
        D3D11_BUFFER_DESC indexBufferDesc = {};
        indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * globalIndices.size());
        indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA subResource = {};
        subResource.pSysMem = globalIndices.data();
        device->CreateBuffer(&indexBufferDesc, &subResource, clothIB.GetAddressOf());
    }
#endif
}

void SoftBodySimulate::Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world)
{
    //Simulate(immediateContext);

    //immediateContext->PSSetShaderResources(0, 1, materialResourceView.GetAddressOf());
    //immediateContext->VSSetShader(vertexShader.Get(), nullptr, 0);
    //immediateContext->PSSetShader(pixelShader.Get(), nullptr, 0);
    //immediateContext->IASetInputLayout(inputLayout.Get());
    //immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    std::function<void(int)> traverse = [&](int nodeIndex)->void {
        const Node& node = nodes.at(nodeIndex);
        if (node.mesh > -1)
        {
            const Mesh& mesh = meshes.at(node.mesh);
            for (const Mesh::Primitive& primitive : mesh.primitives)
            {
                UINT stride = sizeof(Mesh::Vertex);
                UINT offset = 0;

                //immediateContext->IASetVertexBuffers(0, 1, buffers.at(primitive.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);
                //immediateContext->IASetVertexBuffers(0, 1, clothVB[b].GetAddressOf(), &stride, &offset);
                //immediateContext->IASetVertexBuffers(0, 1, clothVB.GetAddressOf(), &stride, &offset);
#if 0
                immediateContext->IASetVertexBuffers(0, 1, primitive.clothVB.GetAddressOf(), &stride, &offset);
#else
                immediateContext->IASetVertexBuffers(0, 1, buffers.at(primitive.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);
                //immediateContext->IASetVertexBuffers(0, 1, particleBuffer.GetAddressOf(), &stride, &offset);
#endif // 0

                PrimitiveConstants primitiveData = {};
                primitiveData.material = primitive.material;
                primitiveData.hasTangent = primitive.has("TANGENT");
                primitiveData.skin = node.skin;
                //座標系の変換を行う
                const DirectX::XMFLOAT4X4 coordinateSystemTransforms[]
                {
                    {//RHS Y-UP
                        -1,0,0,0,
                         0,1,0,0,
                         0,0,1,0,
                         0,0,0,1,
                    },
                    {//LHS Y-UP
                        1,0,0,0,
                        0,1,0,0,
                        0,0,1,0,
                        0,0,0,1,
                    },
                    {//RHS Z-UP
                        -1,0, 0,0,
                         0,0,-1,0,
                         0,1, 0,0,
                         0,0, 0,1,
                    },
                    {//LHS Z-UP
                        1,0,0,0,
                        0,0,1,0,
                        0,1,0,0,
                        0,0,0,1,
                    },
                };

                float scaleFactor;

                if (isModelInMeters)
                {
                    scaleFactor = 1.0f;//メートル単位の時
                }
                else
                {
                    scaleFactor = 0.01f;//㎝単位の時
                }
                DirectX::XMMATRIX C{ DirectX::XMLoadFloat4x4(&coordinateSystemTransforms[static_cast<int>(modelCoordinateSystem)]) * DirectX::XMMatrixScaling(scaleFactor,scaleFactor,scaleFactor) };

                DirectX::XMStoreFloat4x4(&primitiveData.world, DirectX::XMLoadFloat4x4(&node.globalTransform) * C * DirectX::XMLoadFloat4x4(&world));
                DirectX::XMMATRIX WorldMatrix = DirectX::XMLoadFloat4x4(&primitiveData.world);
                DirectX::XMMATRIX InvWorldMatrix = DirectX::XMMatrixInverse(nullptr, WorldMatrix);
                DirectX::XMStoreFloat4x4(&primitiveData.invWorld, InvWorldMatrix);
                //DirectX::XMStoreFloat4x4(&cbuffer->data.invWorld, InvWorldMatrix);
                immediateContext->CSSetConstantBuffers(0, 1, primitiveCbuffer.GetAddressOf());

                Simulate(immediateContext);

                immediateContext->PSSetShaderResources(0, 1, materialResourceView.GetAddressOf());
                immediateContext->VSSetShader(vertexShader.Get(), nullptr, 0);
                immediateContext->PSSetShader(pixelShader.Get(), nullptr, 0);
                immediateContext->IASetInputLayout(inputLayout.Get());
                immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


                immediateContext->UpdateSubresource(primitiveCbuffer.Get(), 0, 0, &primitiveData, 0, 0);
                immediateContext->VSSetConstantBuffers(0, 1, primitiveCbuffer.GetAddressOf());
                immediateContext->PSSetConstantBuffers(0, 1, primitiveCbuffer.GetAddressOf());
                //Simulate(immediateContext);


                const Material& material = materials.at(primitive.material);

                RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::WIREFRAME_CULL_NONE);
                //RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);

                const int textureIndices[] =
                {
                    material.data.pbrMetallicRoughness.basecolorTexture.index,
                    material.data.pbrMetallicRoughness.metallicRoughnessTexture.index,
                    material.data.normalTexture.index,
                    material.data.emissiveTexture.index,
                    material.data.occlusionTexture.index,
                };
                ID3D11ShaderResourceView* nullShaderResourceView = {};
                std::vector<ID3D11ShaderResourceView*> shaderResourceViews(_countof(textureIndices));
                for (int textureIndex = 0; textureIndex < shaderResourceViews.size(); ++textureIndex)
                {
                    shaderResourceViews.at(textureIndex) = textureIndices[textureIndex] > -1 ? textureResourceViews.at(textures.at(textureIndices[textureIndex]).source).Get() : nullShaderResourceView;
                }
                immediateContext->PSSetShaderResources(1, static_cast<UINT>(shaderResourceViews.size()), shaderResourceViews.data());

                if (primitive.indexBufferView.buffer > -1)
                {
                    immediateContext->IASetIndexBuffer(buffers.at(primitive.indexBufferView.buffer).Get(), primitive.indexBufferView.format, 0);
                    immediateContext->DrawIndexed(primitive.indexBufferView.sizeInBytes / sizeofComponent(primitive.indexBufferView.format), 0, 0);
                    //immediateContext->IASetIndexBuffer(clothIB.Get(), primitive.indexBufferView.format, 0);
                    //immediateContext->DrawIndexed(primitive.indexCount, primitive.startIndexLocation, 0);
                }
                else
                {
                    immediateContext->Draw(primitive.vertexBufferView.sizeInBytes / primitive.vertexBufferView.strideInBytes, 0);
                }
                RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);

            }
        }
        for (std::vector<int>::value_type childIndex : node.children)
        {
            traverse(childIndex);
        }
        };
    for (std::vector<int>::value_type nodeIndex : scenes.at(defaultScene).nodes)
    {
        traverse(nodeIndex);
    }
}
