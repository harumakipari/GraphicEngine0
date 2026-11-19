#include "pch.h"
#include "ClothSimulate.h"
#include <functional>
#include <filesystem>
#include <fstream>
#include <array>
#include <cmath>
#include <unordered_set>
#include <algorithm>
#include "tiny_gltf.h"

#include "Engine/Utility/Win32Utils.h"
#include "Engine/Serialization/DirectXSerializers.h"
#include "Graphics/Core/Shader.h"
#include "Graphics/Resource/Texture.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GltfDxgiHelper.h"
#include "Graphics/Core/Graphics.h"
#include "Math/MathHelper.h"

#define NUMTHREAD_X 16

ClothSimulate::ClothSimulate(ID3D11Device* device, const std::string& filename) : filename(filename)
{
    std::filesystem::path cerealFilename(filename);
    cerealFilename.replace_extension("clothCereal");
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
        tinyGltf.SetImageLoader(NullLoadImage, nullptr);

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

        if (gltfModel->meshes.size() != 1 || gltfModel->meshes.front().primitives.size() != 1)
        {
            Logger::Warning(Logger::LogCategory::System, "[Cloth] Unsupported model: exactly one mesh with one primitive is required.");
            enableClothRendering = false;
            enableClothSimulation = false;
            return;
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
    if (meshes.size() != 1 || meshes.front().primitives.size() != 1 ||
        meshes.front().primitives.front().cachedVertices.empty())
    {
        Logger::Warning(Logger::LogCategory::System,
            "[Cloth] Unsupported model/cache: exactly one non-empty primitive in one mesh is required.");
        enableClothRendering = false;
        enableClothSimulation = false;
        return;
    }

    Mesh::Primitive& primitive = meshes.front().primitives.front();
    const IndexBufferView& indexView = primitive.indexBufferView;
    if (indexView.format == DXGI_FORMAT_R8_UINT ||
        (indexView.sizeInBytes > 0 && indexView.format != DXGI_FORMAT_R16_UINT &&
            indexView.format != DXGI_FORMAT_R32_UINT))
    {
        Logger::Warning(Logger::LogCategory::System,
            "[Cloth] Unsupported index format: R8_UINT is unsupported; use R16_UINT or R32_UINT.");
        enableClothRendering = false;
        enableClothSimulation = false;
        return;
    }
    if (indexView.sizeInBytes > 0)
    {
        const UINT indexSize = GetFormatByteSize(indexView.format);
        if (indexView.sizeInBytes % indexSize != 0 ||
            primitive.cachedIndices.size() != indexView.sizeInBytes)
        {
            Logger::Warning(Logger::LogCategory::System, "[Cloth] Invalid index buffer size.");
            enableClothRendering = false;
            enableClothSimulation = false;
            return;
        }
    }
    else
    {
        primitive.indexBufferView.buffer = -1;
    }

    cbuffer = std::make_unique<ConstantBuffer<ClothSimulateCBuffer>>(device);
    clothTimeCBuffer = std::make_unique<ConstantBuffer<ClothTimeCBuffer>>(device);
    clothAlphaTestCBuffer = std::make_unique<ConstantBuffer<ClothAlphaTestCBuffer>>(device);

    CreateAndUploadResources(device);
}
void ClothSimulate::FetchNodes(const tinygltf::Model& gltfModel)
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

void ClothSimulate::CumulateTransforms(std::vector<Node>& nodes) const
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

void ClothSimulate::FetchMeshes(ID3D11Device* device, const tinygltf::Model& gltfModel)
{
    for (const tinygltf::Mesh& gltfMesh : gltfModel.meshes)
    {
        Mesh& mesh = meshes.emplace_back();
        mesh.name = gltfMesh.name;

        for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
        {
            Mesh::Primitive& primitive = mesh.primitives.emplace_back();
            primitive.material = gltfPrimitive.material;

            // インデックスバッファの作成
            if (gltfPrimitive.indices > -1)
            {
                const tinygltf::Accessor& gltfAccessor = gltfModel.accessors.at(gltfPrimitive.indices);
                const tinygltf::BufferView& gltfBufferView = gltfModel.bufferViews.at(gltfAccessor.bufferView);

                primitive.indexBufferView.format = ToDxgiFormat(gltfAccessor);
                primitive.indexBufferView.sizeInBytes = static_cast<UINT>(gltfAccessor.count) * GetFormatByteSize(primitive.indexBufferView.format);
                primitive.cachedIndices.resize(primitive.indexBufferView.sizeInBytes);
                const unsigned char* data = gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset;

                memcpy_s(primitive.cachedIndices.data(), primitive.cachedIndices.size(), data, primitive.indexBufferView.sizeInBytes);
            }

            // 頂点属性の読み込み
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
                    CopyStride<DirectX::XMFLOAT3>(dData, dStride, sData, sStride, count);
                }
                else if (gltfAttribute.first == "NORMAL")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    unsigned char* d_data = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->normal);
                    CopyStride<DirectX::XMFLOAT3>(d_data, dStride, sData, sStride, count);
                }
                else if (gltfAttribute.first == "TANGENT")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->tangent);
                    CopyStride<DirectX::XMFLOAT4>(dData, dStride, sData, sStride, count);
                }
                else if (gltfAttribute.first == "TEXCOORD_0")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->texcoord);
                    CopyStride<DirectX::XMFLOAT2>(dData, dStride, sData, sStride, count);
                }
                else
                {
                    //_ASSERT_EXPR(FALSE, L"This attribute is unsupported.");
                }
                primitive.attributes.emplace(gltfAttribute.first, ToDxgiFormat(gltfAccessor));
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

void ClothSimulate::FetchMaterials(ID3D11Device* device, const tinygltf::Model& gltfModel)
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
void ClothSimulate::FetchTextures(ID3D11Device* device, const tinygltf::Model& gltfModel)
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

void ClothSimulate::Update(float deltaTime)
{
    if (!cbuffer || !enableClothSimulation || !std::isfinite(deltaTime))
        return;

    const float maxAccumulatedTime = fixedDeltaTime * static_cast<float>(maxSubsteps);
    const float clampedFrameTime = (std::clamp)(deltaTime, 0.0f, maxAccumulatedTime);
    simulationAccumulator = (std::min)(simulationAccumulator + clampedFrameTime, maxAccumulatedTime);
}


void ClothSimulate::CreateAndUploadResources(ID3D11Device* device)
{
    if (!cbuffer)
        return;

    HRESULT hr;
    D3D11_BUFFER_DESC bufferDesc = {};
    D3D11_SUBRESOURCE_DATA subresourceData = {};

    // GPUにインデックスバッファをアップロードする
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

            }
        }
    }

    // マテリアルをGPUにアップロードする
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

    const std::filesystem::path opacityTexturePath = "Data/Models/Cloth/M_Cloth_Opacity.png";
    if (std::filesystem::exists(opacityTexturePath))
    {
        ID3D11ShaderResourceView* opacityTexture = nullptr;
        hr = LoadTextureFromFile(device, opacityTexturePath.wstring().c_str(), &opacityTexture, nullptr);
        if (SUCCEEDED(hr) && opacityTexture)
        {
            clothOpacityTextureSRV.Attach(opacityTexture);
            clothOpacityTextureLoaded = true;
            Logger::Log(Logger::LogCategory::System,
                "[Cloth] Loaded opacity texture: Data/Models/Cloth/M_Cloth_Opacity.png");
        }
        else
        {
            Logger::Warning(Logger::LogCategory::System,
                "[Cloth] Failed to load opacity texture; Alpha Test is disabled for Cloth.");
        }
    }
    else
    {
        Logger::Warning(Logger::LogCategory::System,
            "[Cloth] Opacity texture not found; Alpha Test is disabled for Cloth.");
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

    }
    hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelDeferredPS.cso", pixelShader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/ClothUpdateCS.cso", clothUpdateCS.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateCsFromCSO(device, "./Data/Shaders/ClothConstraintCS.cso", clothConstraintCS.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    bufferDesc.ByteWidth = sizeof(ComputePrimitiveConstants);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;
    hr = device->CreateBuffer(&bufferDesc, nullptr, computePrimitiveCbuffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    bufferDesc.ByteWidth = sizeof(GraphicsPrimitiveConstants);
    hr = device->CreateBuffer(&bufferDesc, nullptr, graphicsPrimitiveCbuffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


    // UAVとSRVを作成
    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {// ここでデータを入れる
            primitive.clothVertexOffset = (uint32_t)allVertices.size(); // オフセット保存

            // 固定点を決める
             // AABBを計算
            DirectX::XMFLOAT3 min = { FLT_MAX, FLT_MAX, FLT_MAX };
            DirectX::XMFLOAT3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

            for (auto& v : primitive.cachedVertices)
            {
                min.x = std::min<float>(min.x, v.position.x);
                min.y = std::min<float>(min.y, v.position.y);
                min.z = std::min<float>(min.z, v.position.z);

                max.x = std::max<float>(max.x, v.position.x);
                max.y = std::max<float>(max.y, v.position.y);
                max.z = std::max<float>(max.z, v.position.z);
            }

            const float yThreshold = 0.0001f;
            for (auto& v : primitive.cachedVertices)
            {
                if (fabs(v.position.y - max.y) < yThreshold)
                {
                    v.isPinned = 1;
                }
                else
                {
                    v.isPinned = 0;
                }
            }

            uint32_t clothSeed = static_cast<uint32_t>(&primitive - &mesh.primitives[0]);
            std::mt19937 rng(clothSeed);
            std::uniform_real_distribution<float> dist(-0.01f, 0.01f); // 1mm
            for (auto& v : primitive.cachedVertices)
            {
                if (v.isPinned)
                    continue;

                v.position.x += dist(rng);
                v.position.z += dist(rng);
            }


            allVertices.insert(allVertices.end(),
                primitive.cachedVertices.begin(),
                primitive.cachedVertices.end());
        }
    }

    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
            if (primitive.indexBufferView.sizeInBytes == 0)
                continue;

            primitive.startIndexLocation = 0;
            primitive.indexCount = primitive.indexBufferView.sizeInBytes / GetFormatByteSize(primitive.indexBufferView.format);
        }
    }

    constexpr float maxDistance = 20.0f;
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
                const size_t maxEdges =
                    std::min<size_t>(static_cast<size_t>(MAX_EDGES), candidates.size());

                for (size_t k = 0; k < maxEdges; ++k)
                {
                    ClothEdge edge;
                    edge.neighbor = candidates[k].neighbor;
                    edge.delta = candidates[k].delta;
                    edge.restLength = candidates[k].distance;
                    vertexEdges[i].push_back(edge);
                }
                while (vertexEdges[i].size() < MAX_EDGES)
                {
                    vertexEdges[i].push_back({ UINT32_MAX, { 0.0f, 0.0f, 0.0f }, 0.0f });
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

    cbuffer->data.vertexCount = static_cast<int>(allVertices.size());
    cbuffer->data.windPhaseOffset = windPhaseOffset;
    cbuffer->data.windBase = windBase;
    cbuffer->data.windEmitPosition = windEmitPosition;
    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
            primitive.CreateClothPingPongBuffers(device);
        }
    }


}


void ClothSimulate::Simulate(ID3D11DeviceContext* immediateContext)
{
    if (!cbuffer || !enableClothSimulation)
        return;

    const UINT substepCount = (std::min)(static_cast<UINT>(simulationAccumulator / fixedDeltaTime), static_cast<UINT>(maxSubsteps));
    if (substepCount == 0)
        return;

    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    ID3D11UnorderedAccessView* nullUAV = {};
    ID3D11Buffer* nullBuffer = nullptr;

    for (UINT substep = 0; substep < substepCount; ++substep)
    {
        cbuffer->data.enableDriveForce = enableDriveForce ? 1u : 0u;
        cbuffer->data.bottomWindExponent = bottomWindExponent;
        cbuffer->data.driveForceAmplitude = driveForceAmplitude;
        cbuffer->data.driveForceFrequency = driveForceFrequency;
        cbuffer->data.driveForcePhaseGradient = driveForcePhaseGradient;
        ID3D11Buffer* simulationBuffer = cbuffer->GetBuffer();
        immediateContext->UpdateSubresource(simulationBuffer, 0, nullptr, &cbuffer->data, 0, 0);
        immediateContext->CSSetConstantBuffers(10, 1, &simulationBuffer);
        clothTimeCBuffer->data.elapsedTime = simulationElapsedTime + fixedDeltaTime * static_cast<float>(substep);
        clothTimeCBuffer->data.deltaTime = fixedDeltaTime;
        clothTimeCBuffer->data.gravityScale = gravityScale;
        ID3D11Buffer* timeBuffer = clothTimeCBuffer->GetBuffer();
        immediateContext->UpdateSubresource(timeBuffer, 0, nullptr, &clothTimeCBuffer->data, 0, 0);
        immediateContext->CSSetConstantBuffers(11, 1, &timeBuffer);

        for (auto& mesh : meshes)
        {
            for (auto& primitive : mesh.primitives)
            {
                int& input = primitive.ping;
                int& output = primitive.pong;
                const UINT threadGroupCountX = Align(static_cast<UINT>(primitive.cachedVertices.size()), NUMTHREAD_X) / NUMTHREAD_X;

                immediateContext->CSSetUnorderedAccessViews(0, 1, primitive.clothUAV[output].GetAddressOf(), nullptr);
                immediateContext->CSSetShaderResources(0, 1, primitive.clothSRV[input].GetAddressOf());
                immediateContext->CSSetShaderResources(1, 1, primitive.clothEdgeSRV.GetAddressOf());
                immediateContext->CSSetShaderResources(2, 1, primitive.initialBottomWeightSRV.GetAddressOf());
                immediateContext->CSSetShader(clothUpdateCS.Get(), nullptr, 0);
                immediateContext->Dispatch(threadGroupCountX, 1, 1);
                immediateContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
                immediateContext->CSSetShaderResources(0, 1, nullSRV);
                immediateContext->CSSetShaderResources(1, 1, nullSRV);
                immediateContext->CSSetShaderResources(2, 1, nullSRV);

                std::swap(input, output);

                immediateContext->CSSetUnorderedAccessViews(0, 1, primitive.clothUAV[output].GetAddressOf(), nullptr);
                immediateContext->CSSetShaderResources(0, 1, primitive.clothSRV[input].GetAddressOf());
                immediateContext->CSSetShaderResources(1, 1, primitive.clothEdgeSRV.GetAddressOf());
                immediateContext->CSSetShader(clothConstraintCS.Get(), nullptr, 0);
                immediateContext->Dispatch(threadGroupCountX, 1, 1);
                immediateContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
                immediateContext->CSSetShaderResources(0, 1, nullSRV);
                immediateContext->CSSetShaderResources(1, 1, nullSRV);

                std::swap(input, output);
            }
        }
    }

    // This simulation owns only these CS bindings. Do not leak them into later passes.
    immediateContext->CSSetShaderResources(0, 1, nullSRV);
    immediateContext->CSSetShaderResources(1, 1, nullSRV);
    immediateContext->CSSetShaderResources(2, 1, nullSRV);
    immediateContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    immediateContext->CSSetShader(nullptr, nullptr, 0);
    immediateContext->CSSetConstantBuffers(0, 1, &nullBuffer);
    immediateContext->CSSetConstantBuffers(10, 1, &nullBuffer);
    immediateContext->CSSetConstantBuffers(11, 1, &nullBuffer);

    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
            immediateContext->CopyResource(primitive.clothVB.Get(), primitive.clothBuffer[primitive.ping].Get());
        }
    }
    simulationAccumulator -= fixedDeltaTime * static_cast<float>(substepCount);
    simulationElapsedTime += fixedDeltaTime * static_cast<float>(substepCount);
}

void ClothSimulate::Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world)
{
    if (!cbuffer || !enableClothRendering)
        return;
    std::function<void(int)> traverse = [&](int nodeIndex)->void {
        const Node& node = nodes.at(nodeIndex);
        if (node.mesh > -1)
        {
            Mesh& mesh = meshes.at(node.mesh);
            for (Mesh::Primitive& primitive : mesh.primitives)
            {
                UINT stride = sizeof(Mesh::Vertex);
                UINT offset = 0;

                immediateContext->IASetVertexBuffers(0, 1, primitive.clothVB.GetAddressOf(), &stride, &offset);

                ComputePrimitiveConstants computePrimitiveData = {};
                computePrimitiveData.material = primitive.material;
                computePrimitiveData.hasTangent = primitive.has("TANGENT");
                computePrimitiveData.skin = node.skin;
                //座標系の変換を行う
                const DirectX::XMFLOAT4X4 coordinateSystemTransform =
                {//LHS Y-UP
                    1,0,0,0,
                    0,1,0,0,
                    0,0,1,0,
                    0,0,0,1,
                };

                float scaleFactor = 1.0f;
                DirectX::XMMATRIX C{ DirectX::XMLoadFloat4x4(&coordinateSystemTransform) * DirectX::XMMatrixScaling(scaleFactor,scaleFactor,scaleFactor) };

                DirectX::XMStoreFloat4x4(&computePrimitiveData.world, DirectX::XMLoadFloat4x4(&node.globalTransform) * C * DirectX::XMLoadFloat4x4(&world));
                DirectX::XMMATRIX WorldMatrix = DirectX::XMLoadFloat4x4(&computePrimitiveData.world);
                DirectX::XMMATRIX InvWorldMatrix = DirectX::XMMatrixInverse(nullptr, WorldMatrix);
                DirectX::XMStoreFloat4x4(&computePrimitiveData.invWorld, InvWorldMatrix);
                immediateContext->UpdateSubresource(computePrimitiveCbuffer.Get(), 0, 0, &computePrimitiveData, 0, 0);
                immediateContext->CSSetConstantBuffers(0, 1, computePrimitiveCbuffer.GetAddressOf());

                Simulate(immediateContext);

                GraphicsPrimitiveConstants graphicsPrimitiveData = {};
                graphicsPrimitiveData.world = computePrimitiveData.world;
                graphicsPrimitiveData.previousWorld = primitive.hasPreviousGraphicsWorld ? primitive.previousGraphicsWorld : graphicsPrimitiveData.world;
                graphicsPrimitiveData.material = primitive.material;
                graphicsPrimitiveData.hasTangent = primitive.has("TANGENT");
                graphicsPrimitiveData.skin = node.skin;
                graphicsPrimitiveData.materialType = 99;
                DirectX::XMStoreFloat4x4(&graphicsPrimitiveData.inverseTransposeWorld,
                    DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, WorldMatrix)));
                immediateContext->UpdateSubresource(graphicsPrimitiveCbuffer.Get(), 0, 0, &graphicsPrimitiveData, 0, 0);
                clothAlphaTestCBuffer->data.enableAlphaTest = (enableAlphaTest && clothOpacityTextureLoaded) ? 1u : 0u;
                clothAlphaTestCBuffer->data.alphaThreshold = alphaThreshold;
                ID3D11Buffer* clothAlphaTestBuffer = clothAlphaTestCBuffer->GetBuffer();
                immediateContext->UpdateSubresource(clothAlphaTestBuffer, 0, nullptr, &clothAlphaTestCBuffer->data, 0, 0);
                immediateContext->PSSetConstantBuffers(13, 1, &clothAlphaTestBuffer);
                ID3D11ShaderResourceView* clothOpacitySRV = clothOpacityTextureLoaded ? clothOpacityTextureSRV.Get() : nullptr;
                immediateContext->PSSetShaderResources(16, 1, &clothOpacitySRV);
                immediateContext->PSSetShaderResources(0, 1, materialResourceView.GetAddressOf());
                immediateContext->VSSetShader(vertexShader.Get(), nullptr, 0);
                immediateContext->PSSetShader(pixelShader.Get(), nullptr, 0);
                immediateContext->IASetInputLayout(inputLayout.Get());
                immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


                immediateContext->VSSetConstantBuffers(0, 1, graphicsPrimitiveCbuffer.GetAddressOf());
                immediateContext->PSSetConstantBuffers(0, 1, graphicsPrimitiveCbuffer.GetAddressOf());

                const Material& material = materials.at(primitive.material);

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
                    static bool loggedFirstDrawIndexed = false;
                    if (!loggedFirstDrawIndexed)
                    {
                        loggedFirstDrawIndexed = true;
                    }
                    immediateContext->DrawIndexed(primitive.indexBufferView.sizeInBytes / GetFormatByteSize(primitive.indexBufferView.format), 0, 0);
                }
                else
                {
                    immediateContext->Draw(static_cast<UINT>(primitive.cachedVertices.size()), 0);
                }

                primitive.previousGraphicsWorld = graphicsPrimitiveData.world;
                primitive.hasPreviousGraphicsWorld = true;
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

    ID3D11Buffer* nullClothAlphaTestBuffer = nullptr;
    immediateContext->PSSetConstantBuffers(13, 1, &nullClothAlphaTestBuffer);
    ID3D11ShaderResourceView* nullClothOpacitySRV = nullptr;
    immediateContext->PSSetShaderResources(16, 1, &nullClothOpacitySRV);
}
