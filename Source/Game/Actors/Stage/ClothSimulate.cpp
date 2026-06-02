#include "pch.h"
// INTERLEAVED_GLTF_MODEL
#include "ClothSimulate.h"
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
#include "Engine/Debug/DebugRender.h"
#include "Engine/Serialization/DirectXSerializers.h"
#include "Graphics/Core/Shader.h"
#include "Graphics/Resource/Texture.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/PipelineState.h"

#include "Graphics/Core/Graphics.h"
#include "Math/MathHelper.h"

#define NUMTHREAD_X 16

UINT sizeofComponent(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8_UINT: return 1;
    case DXGI_FORMAT_R16_UINT: return 2;
    case DXGI_FORMAT_R32_UINT: return 4;
    case DXGI_FORMAT_R32G32_FLOAT: return 8;
    case DXGI_FORMAT_R32G32B32_FLOAT: return 12;
    case DXGI_FORMAT_R8G8B8A8_UINT: return 3;
    case DXGI_FORMAT_R16G16B16A16_UINT: return 8;
    case DXGI_FORMAT_R32G32B32A32_UINT: return 16;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
    }
    _ASSERT_EXPR(FALSE, L"Not supported");
    return 0;
}

bool nullLoadImageData(tinygltf::Image*, const int, std::string*, std::string*, int, int, const unsigned char*, int, void*)
{
    return true;
}

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
        tinyGltf.SetImageLoader(nullLoadImageData, nullptr);

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
    cbuffer = std::make_unique<ConstantBuffer<ClothSimulateCBuffer>>(device);

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

void ClothSimulate::CumulateTransforms(std::vector<Node>& nodes)
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
DXGI_FORMAT DxgiFormat(const tinygltf::Accessor& accessor)
{
    switch (accessor.type)
    {
    case TINYGLTF_TYPE_SCALAR:
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return DXGI_FORMAT_R8_UINT;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return DXGI_FORMAT_R16_UINT;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            return DXGI_FORMAT_R32_UINT;
        default:
            _ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
            return DXGI_FORMAT_UNKNOWN;
        }
    case TINYGLTF_TYPE_VEC2:
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;
        default:
            _ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
            return DXGI_FORMAT_UNKNOWN;
        }
    case TINYGLTF_TYPE_VEC3:
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        default:
            _ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
            return DXGI_FORMAT_UNKNOWN;
        }
    case TINYGLTF_TYPE_VEC4:
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default:
            _ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
            return DXGI_FORMAT_UNKNOWN;
        }
        break;
    default:
        _ASSERT_EXPR(FALSE, L"This accessor type is not supported.");
        return DXGI_FORMAT_UNKNOWN;
    }
}

template<class T>
static void _Copy(unsigned char* dData, const size_t dStride, const unsigned char* sData, const size_t sStride, size_t count)
{
    while (count-- > 0)
    {
        *reinterpret_cast<T*>(dData) = *reinterpret_cast<const T*>(sData);
        sData += sStride;
        dData += dStride;
    }
};
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
#if 0
                    const unsigned char* src = sData;
                    unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->position);

                    for (size_t i = 0; i < count; ++i)
                    {
                        // glTFのPOSITIONを読み取る
                        const DirectX::XMFLOAT3* srcPos = reinterpret_cast<const DirectX::XMFLOAT3*>(src + sStride * i);
                        DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(srcPos);

                        // ノードのTransformを適用
                        pos = DirectX::XMVector3TransformCoord(pos, transform);

                        // 結果をキャッシュに書き込む
                        DirectX::XMStoreFloat3(&primitive.cachedVertices[i].position, pos);
                    }
#else
                    unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->position);

                    _Copy<DirectX::XMFLOAT3>(dData, dStride, sData, sStride, count);
#endif
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

void ClothSimulate::Update(float deltaTine)
{
    int currentPinMode = 0;

#ifdef USE_IMGUI
    std::string windowName =
        "Cloth##" + std::to_string(reinterpret_cast<uintptr_t>(this));

    ImGui::Begin(windowName.c_str());

    if (ImGui::Button("four fix"))
    {
        SetupPinVertices(0);
        RecreateClothBuffers(Graphics::GetDevice());
    }
    if (ImGui::Button("up fix"))
    {
        SetupPinVertices(1);
        RecreateClothBuffers(Graphics::GetDevice());
    }
    if (ImGui::Button("no fix"))
    {
        SetupPinVertices(2);
        RecreateClothBuffers(Graphics::GetDevice());
    }
    if (ImGui::Button("curtain fix"))
    {
        SetupPinVertices(3);
        RecreateClothBuffers(Graphics::GetDevice());
    }

#if 0
    ImGui::DragFloat("windVariation", &windPhaseOffset, 0.5f);
    ImGui::DragFloat("windBase", &windBase, 0.5f);

    ImGui::DragFloat3("windEmitPosition", &windEmitPosition.x, 0.5f);

#endif // 0

    ImGui::End();
#endif
  //  DebugDrawManager::DrawSphere(cbuffer->data.windEmitPosition, 0.5f, { 1.0f,1.0f,0.0f,1.0f });

}

void ClothSimulate::RecreateClothBuffers(ID3D11Device* device)
{
    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
            primitive.clothEdgeBuffer.Reset();
            primitive.clothEdgeSRV.Reset();
            primitive.clothVB.Reset();
            primitive.clothIB.Reset();
            for (int i = 0; i < 2; i++)
            {
                primitive.clothSRV[i].Reset();
                primitive.clothUAV[i].Reset();
                primitive.clothBuffer[i].Reset();

            }

            primitive.CreateClothPingPongBuffers(device);
        }
    }
}

void ClothSimulate::SetupPinVertices(int curretPinMode)
{
    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
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

            // 四隅の判定
            if (curretPinMode == 0)
            {
                for (auto& v : primitive.cachedVertices)
                {
                    bool isCorner =
                        (fabs(v.position.x - min.x) < 0.001f || fabs(v.position.x - max.x) < 0.001f) &&
                        (fabs(v.position.z - min.z) < 0.001f || fabs(v.position.z - max.z) < 0.001f) &&
                        (fabs(v.position.y - max.y) < 0.001f); // 上辺

                    if (isCorner)
                    {
                        v.isPinned = 1;
                    }
                }
            }
            else if (curretPinMode == 1)
            {
                const float yThreshold = 0.001f;
                //const float yThreshold = 0.1f;
                for (auto& v : primitive.cachedVertices)
                {
                    if (fabs(v.position.y - min.y) < yThreshold)
                    {
                        v.isPinned = 1;
                    }
                    else
                    {
                        v.isPinned = 0;
                    }
                }
            }
            else if (curretPinMode == 2)
            {
                for (auto& v : primitive.cachedVertices)
                {
                    v.isPinned = 0;
                }
            }
            else if (curretPinMode == 3)
            {
                const float yThreshold = 0.1f;
                for (auto& v : primitive.cachedVertices)
                {
                    if (fabs(v.position.y - min.y) < yThreshold)
                    {
                        v.isPinned = 1;
                    }
                    else
                    {
                        v.isPinned = 0;
                    }
                }
            }
        }
    }
}
void ClothSimulate::CreateAndUploadResources(ID3D11Device* device)
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

            // 四隅の判定
#if 0
            for (auto& v : primitive.cachedVertices)
            {
                bool isCorner =
                    (fabs(v.position.x - min.x) < 0.001f || fabs(v.position.x - max.x) < 0.001f) &&
                    (fabs(v.position.z - min.z) < 0.001f || fabs(v.position.z - max.z) < 0.001f) &&
                    (fabs(v.position.y - max.y) < 0.001f); // 上辺

                if (isCorner)
                {
                    v.isPinned = 1;
                }
            }
            //for (auto& v : primitive.cachedVertices)
            //{
            //    v.isPinned = 0; // 初期化
            //    for (auto& pinPos : pinPositions)
            //    {
            //        float dx = v.position.x - pinPos.x;
            //        float dy = v.position.y - pinPos.y;
            //        float dz = v.position.z - pinPos.z;
            //        float distSq = dx * dx + dy * dy + dz * dz;

            //        if (distSq < pinThreshold * pinThreshold)
            //        {
            //            v.isPinned = 1;
            //            break;
            //        }
            //    }
            //}
#else

#if 1
            const float yThreshold = 0.0001f;
            //const float yThreshold = 10.5f;
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

#else
            const float yThreshold = 0.1f;
            for (auto& v : primitive.cachedVertices)
            {
                if (fabs(v.position.y - min.y) < yThreshold)
                {
                    v.isPinned = 1;
                }
                else
                {
                    v.isPinned = 0;
                }
            }
#endif // 0
#endif // 0

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

    auto MakeEdgeKey = [&](uint32_t a, uint32_t b)
        {
            if (a < b)
                return (uint64_t(a) << 32) | uint64_t(b);
            else
                return (uint64_t(b) << 32) | uint64_t(a);
        };

    // 対角線を探すために。。？
    std::unordered_map<uint64_t, std::vector<uint32_t>> edgeOpp;
    edgeOpp.reserve(globalIndices.size() / 2);

    //// 頂点ごとのエッジのリスト
    //std::vector<std::vector<ClothEdge>> vertexEdges(allVertices.size());
    //const size_t MAX_EDGES = 8;

#if 0
    for (size_t i = 0; i < globalIndices.size(); i += 3)
    {
        // 頂点バッファの値を取り出す
        uint32_t i0 = globalIndices[i + 0];
        uint32_t i1 = globalIndices[i + 1];
        uint32_t i2 = globalIndices[i + 2];

        edgeOpp[MakeEdgeKey(i0, i1)].push_back(i2);
        edgeOpp[MakeEdgeKey(i1, i2)].push_back(i0);
        edgeOpp[MakeEdgeKey(i2, i0)].push_back(i1);

        // 頂点インデックスごとにエッジを追加する
        auto AddEdge = [&](uint32_t a, uint32_t b)
            {
                DirectX::XMFLOAT3 pa = allVertices[a].position;
                DirectX::XMFLOAT3 pb = allVertices[b].position;
                DirectX::XMFLOAT3 delta = { pb.x - pa.x, pb.y - pa.y, pb.z - pa.z };
                float restLength = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

                ClothEdge edge;
                edge.neighbor = b;
                edge.delta = delta;
                edge.restLength = restLength;

                vertexEdges[a].push_back(edge);
            };

        // 相互に頂点を追加する
        AddEdge(i0, i1);
        AddEdge(i1, i0);
        AddEdge(i1, i2);
        AddEdge(i2, i1);
        AddEdge(i2, i0);
        AddEdge(i0, i2);
    }

    for (auto& kv : edgeOpp)
    {
        std::vector<uint32_t>& opps = kv.second;
        std::sort(opps.begin(), opps.end());
        opps.erase(
            std::unique(opps.begin(), opps.end()), opps.end());

        const size_t n = opps.size();
        if (n < 2)
        {
            continue;
        }

        for (size_t a = 0; a < n; ++a)
        {
            for (size_t b = a + 1; b < n; ++b)
            {
                uint32_t vA = opps[a];
                uint32_t vB = opps[b];
                if (vA == vB)
                    continue;
                // compute rest length and delta 
                const auto& pa = allVertices[vA].position;
                const auto& pb = allVertices[vB].position;
                DirectX::XMFLOAT3 delta = { pb.x - pa.x, pb.y - pa.y, pb.z - pa.z };
                float restLength = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                ClothEdge eAB;
                eAB.neighbor = vB;
                eAB.delta = delta;
                eAB.restLength = restLength;
                ClothEdge eBA;
                eBA.neighbor = vA;
                eBA.delta = DirectX::XMFLOAT3{ -delta.x, -delta.y, -delta.z };
                eBA.restLength = restLength;
                // push into per-vertex adjacency (bidirectional) 
                vertexEdges[vA].push_back(eAB);
                vertexEdges[vB].push_back(eBA);
            }
        }
    }

    // 重複しているのを削除する と　距離が短いものを優先して残す
    finalEdges.clear();
    finalEdges.reserve(allVertices.size() * MAX_EDGES);

    for (size_t v = 0; v < vertexEdges.size(); ++v)
    {
        std::vector<ClothEdge>& edges = vertexEdges[v];
        // 距離が短いのを残す
        std::sort(edges.begin(), edges.end(),
            [&](const ClothEdge& a, const ClothEdge& b)
            {
                if (a.neighbor != b.neighbor)
                    return a.neighbor < b.neighbor;
                return a.restLength < b.restLength;
            }
        );


        // 重複削除
        std::sort(edges.begin(), edges.end(),
            [](auto& a, auto& b) { return a.neighbor < b.neighbor; });
        edges.erase(std::unique(edges.begin(), edges.end(),
            [](auto& a, auto& b) { return a.neighbor == b.neighbor; }),
            edges.end());

        // 制限数に揃える
        if (edges.size() > MAX_EDGES)
            edges.resize(MAX_EDGES);

        while (edges.size() < MAX_EDGES)
        {
            edges.push_back({ UINT32_MAX, {0,0,0}, 0 }); // 無効エッジ
        }

        // 一個の長い配列にする   8個ごとに
        for (auto& e : edges)
        {
            finalEdges.push_back(e);
        }
    }
#else

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

#endif // 0
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

    CreateClothPingPongBuffers(device, allVertices);
}

void ClothSimulate::Simulate(ID3D11DeviceContext* immediateContext)
{
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    cbuffer->Activate(immediateContext, 10);

    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
            int& a = primitive.ping;
            int& b = primitive.pong;

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

}

void ClothSimulate::CreateClothPingPongBuffers(ID3D11Device* device, const std::vector<Mesh::Vertex>& vertices)
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

void ClothSimulate::Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world)
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
                immediateContext->IASetVertexBuffers(0, 1, primitive.clothVB.GetAddressOf(), &stride, &offset);

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

                //RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::WIREFRAME_CULL_NONE);
                RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);

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
