#include "pch.h"
// INTERLEAVED_GLTF_MODEL
#include "InterleavedGltfModel.h"
#include <functional>
#include <filesystem>
#include <fstream>

//#define TINYGLTF_IMPLEMENTATION 

#include <stack>
#include <functional>
#include "DDSTextureLoader.h"
#include "tiny_gltf.h"
#include "Engine/Utility/Win32Utils.h"
#include "Engine/Debug/Assert.h"
#include "Graphics/Core/Shader.h"
#include "Texture.h"
#include <cstddef>
#include <filesystem>
#include <ranges>


#include "Engine/Utility/Win32Utils.h"
#include "Engine/Debug/Assert.h"
#include "Engine/Serialization/DirectXSerializers.h"
#include "Graphics/Core/Shader.h"
#include "Texture.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GltfDxgiHelper.h"
#include "Graphics/Core/PipelineState.h"

#include "Components/Render/MeshComponent.h"


InterleavedGltfModel::InterleavedGltfModel(ID3D11Device* device, const std::string& filename, ModelTypes::ModelMode mode, bool isSaveVerticesData, bool convertToLHS) : filename(filename), mode(mode), isSaveVerticesData(isSaveVerticesData), converted_handedness(convertToLHS)
{
    std::filesystem::path cerealFilename(filename);
    cerealFilename.replace_extension(mode == ModelTypes::ModelMode::StaticMesh || mode == ModelTypes::ModelMode::InstancedStaticMesh ? "batchCereal" : "cereal");

    if (convertToLHS)
    {
        modelCoordinateSystem = CoordinateSystem::LH_Y_UP;
    }
#if 1
    if (std::filesystem::exists(cerealFilename.c_str()))
    {
        std::ifstream ifs(cerealFilename.c_str(), std::ios::binary);
        cereal::BinaryInputArchive deserialization(ifs);
        deserialization(
            cereal::make_nvp("scenes", scenes),
            cereal::make_nvp("defaultScene", defaultScene),
            cereal::make_nvp("nodes", nodes),
            cereal::make_nvp("materials", materials),
            cereal::make_nvp("gltfLights", gltfLights),
            cereal::make_nvp("pointLights", pointLights),
            cereal::make_nvp("spawnPoints", spawnPoints)
        );
        deserialization(cereal::make_nvp("batchMeshes", batchMeshes));
        deserialization(cereal::make_nvp("meshes", meshes));
        deserialization(cereal::make_nvp("textures", textures), cereal::make_nvp("images", images));
        deserialization(cereal::make_nvp("skins", skins), cereal::make_nvp("animations", animations));
    }
    else
#endif
    {
        tinygltf::TinyGLTF tinyGltf;
        tinyGltf.SetImageLoader(NullLoadImage, nullptr);

        //gltfModel = std::make_shared<tinygltf::Model>();
        //tinygltf::Model gltfModel;
        std::string error, warning;
        bool succeeded = false;

#if 1
        if (cachedGltfModels.find(filename) != cachedGltfModels.end() && !cachedGltfModels.at(filename).expired())
        {
            //キャッシュされたデータからモデルデータ取得
            gltfModel = cachedGltfModels.at(filename).lock();
        }
        else
#endif // 0
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

        FetchLights(*gltfModel);
        FetchLightNodes(*gltfModel);

        FetchMaterials(device, *gltfModel);
        FetchTextures(device, *gltfModel);

        if (mode == ModelTypes::ModelMode::StaticMesh || mode == ModelTypes::ModelMode::InstancedStaticMesh)
        {
            FetchAndBatchMeshes(device, *gltfModel);
        }
#if 0
        else
        {
            FetchMeshes(device, *gltfModel);
            if (convertToLHS)
            {
                for (auto& mesh : meshes)
                {
                    ConvertMeshAxisSystem(mesh);
                }
                modelCoordinateSystem = CoordinateSystem::LH_Y_UP;
            }
            ExtractAnimations(*gltfModel);
            if (convertToLHS)
            {
                for (auto& skin : skins)
                {
                    ConvertSkinAxisSystem(skin);

                }
                for (auto& anim : animations)
                {
                    ConvertAnimationAxisSystem(anim);
                }
            }
            //FetchAnimations(*gltfModel, animations); // 一個目のモデルはアニメーションをそのまま追加
            //FetchAnimations(*gltfModel);
        }
#else
        else
        {
            FetchMeshes(device, *gltfModel);
            ExtractAnimations(*gltfModel);
        }

        if (convertToLHS)
        {
            ConvertHandedness();
        }

#endif // 0


        std::ofstream ofs(cerealFilename.c_str(), std::ios::binary);
        cereal::BinaryOutputArchive serialization(ofs);
        serialization(
            cereal::make_nvp("scenes", scenes),
            cereal::make_nvp("defaultScene", defaultScene),
            cereal::make_nvp("nodes", nodes),
            cereal::make_nvp("materials", materials),
            cereal::make_nvp("gltfLights", gltfLights),
            cereal::make_nvp("pointLights", pointLights),
            cereal::make_nvp("spawnPoints", spawnPoints)
        );
        serialization(cereal::make_nvp("batchMeshes", batchMeshes));
        serialization(cereal::make_nvp("meshes", meshes));
        serialization(cereal::make_nvp("textures", textures), cereal::make_nvp("images", images));
        serialization(cereal::make_nvp("skins", skins), cereal::make_nvp("animations", animations));
    }
    //if (!staticBatching)
    //{// staticBatching じゃなければ
    //    //CompouteBoundingBox();
    //}
    CreateAndUploadResources(device);
}
void InterleavedGltfModel::FetchNodes(const tinygltf::Model& gltfModel)
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
#if 0
    for (Node& node : nodes)
    {
        ConvertNodeAxisSystem(node, mode);
    }
#endif // 0

    // globalTransform生成（LHSで作られる）
    CumulateTransforms(nodes);

    // スポーン場所を取得する
    ExtractSpawnPoints();
}

void InterleavedGltfModel::FetchLights(const tinygltf::Model& model)
{
    gltfLights.clear();

    for (const auto& light : model.lights)
    {
        GltfLight gl;
        gl.type = light.type; // "point", "spot", "directional"

        if (!light.color.empty())
        {
            gl.color = {
                static_cast<float>(light.color[0]),
                static_cast<float>(light.color[1]),
                static_cast<float>(light.color[2])
            };
        }
        else
        {
            gl.color = { 1, 1, 1 };
        }
        gl.name = light.name;

        gl.intensity = static_cast<float>(light.intensity);
        gl.range = static_cast<float>(light.range);

        gltfLights.push_back(gl);
    }
}

// ライト付きのノードを探す
void InterleavedGltfModel::FetchLightNodes(const tinygltf::Model& model)
{
    pointLights.clear();

    for (size_t i = 0; i < model.nodes.size(); ++i)
    {
        const auto& gltfNode = model.nodes[i];
        const Node& node = nodes[i]; // ← Cumulate済み

        auto it = gltfNode.extensions.find("KHR_lights_punctual");
        if (it == gltfNode.extensions.end())
            continue;

        int lightIndex = it->second.Get("light").Get<int>();
        const GltfLight& gl = gltfLights.at(lightIndex);

        if (gl.type != "point")
            continue;

        GltfPointLightData pl;

        // world transform 保存
        pl.worldTransform = node.globalTransform;

        DirectX::XMVECTOR S, R, T;

        bool ok = DirectX::XMMatrixDecompose(
            &S,
            &R,
            &T,
            DirectX::XMLoadFloat4x4(&node.globalTransform)
        );

        if (ok)
        {
            DirectX::XMStoreFloat3(&pl.worldScale, S);
            DirectX::XMStoreFloat4(&pl.worldRotation, R);
            DirectX::XMStoreFloat3(&pl.worldPosition, T);
        }

        pl.color = gl.color;
        pl.intensity = gl.intensity;
        pl.range = gl.range;
        pl.name = gl.name;

        pointLights.push_back(pl);
    }
}

// スポーン情報を取得する
void InterleavedGltfModel::ExtractSpawnPoints()
{
    spawnPoints.clear();

    for (const Node& node : nodes)
    {
        if (node.name.rfind("Spawn_", 0) == 0) // starts_with
        {
            SpawnPoint sp;
            sp.name = node.name;
            sp.worldTransform = node.globalTransform;

            DirectX::XMVECTOR S, R, T;

            bool ok = DirectX::XMMatrixDecompose(
                &S,
                &R,
                &T,
                DirectX::XMLoadFloat4x4(&node.globalTransform)
            );

            if (ok)
            {
                DirectX::XMStoreFloat3(&sp.worldScale, S);
                DirectX::XMStoreFloat4(&sp.worldRotation, R);
                DirectX::XMStoreFloat3(&sp.worldPosition, T);
            }

            spawnPoints.push_back(sp);
#ifdef _DEBUG
            Logger::Log(Logger::LogCategory::System, "Spawn found: " + sp.name);
#endif
        }
    }

}
AABB InterleavedGltfModel::GetAABB()const
{
    using namespace DirectX;

    XMVECTOR minVec = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.0f);
    XMVECTOR maxVec = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);

    for (const auto& node : nodes)
    {
        if (node.mesh < 0 || node.mesh >= static_cast<int>(meshes.size()))
            continue;

        // ローカルAABB
        XMFLOAT3 localMin = node.minValue;
        XMFLOAT3 localMax = node.maxValue;
        // コーナー8点
        XMVECTOR corners[8] =
        {
            XMVectorSet(localMin.x, localMin.y, localMin.z, 1.0f),
            XMVectorSet(localMin.x, localMin.y, localMax.z, 1.0f),
            XMVectorSet(localMin.x, localMax.y, localMin.z, 1.0f),
            XMVectorSet(localMin.x, localMax.y, localMax.z, 1.0f),
            XMVectorSet(localMax.x, localMin.y, localMin.z, 1.0f),
            XMVectorSet(localMax.x, localMin.y, localMax.z, 1.0f),
            XMVectorSet(localMax.x, localMax.y, localMin.z, 1.0f),
            XMVectorSet(localMax.x, localMax.y, localMax.z, 1.0f)
        };

        // ワールド行列を取得
        XMMATRIX world = XMLoadFloat4x4(&node.globalTransform);

        // 各コーナーを変換して min/max を更新
        for (int i = 0; i < 8; ++i)
        {
            XMVECTOR v = XMVector3TransformCoord(corners[i], world);
            minVec = XMVectorMin(minVec, v);
            maxVec = XMVectorMax(maxVec, v);
        }
    }

    AABB result;
    DirectX::XMStoreFloat3(&result.min, minVec);
    DirectX::XMStoreFloat3(&result.max, maxVec);
    return result;
}

DirectX::XMFLOAT3 InterleavedGltfModel::GetModelSize() const
{
    AABB box = GetAABB();
    DirectX::XMFLOAT3 size = { box.max.x - box.min.x,box.max.y - box.min.y,box.max.z - box.min.z };
    return size;
}

void InterleavedGltfModel::CumulateTransforms(std::vector<Node>& nodes) const
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
DXGI_FORMAT _DxgiFormat(const tinygltf::Accessor& accessor)
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
void InterleavedGltfModel::FetchMeshes(ID3D11Device* device, const tinygltf::Model& gltfModel)
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

                primitive.indexBufferView.format = _DxgiFormat(gltfAccessor);
                primitive.indexBufferView.sizeInBytes = static_cast<UINT>(gltfAccessor.count) * GetFormatByteSize(primitive.indexBufferView.format);
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
                else if (gltfAttribute.first == "JOINTS_0")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    {
                        unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->joints0);
                        _Copy<DirectX::XMINT4>(dData, dStride, sData, sStride, count);
                    }
                    else if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        const USHORT* data = reinterpret_cast<const USHORT*>(gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset);
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            primitive.cachedVertices.at(accessorIndex).joints0.x = static_cast<UINT>(data[accessorIndex * 4 + 0]);
                            primitive.cachedVertices.at(accessorIndex).joints0.y = static_cast<UINT>(data[accessorIndex * 4 + 1]);
                            primitive.cachedVertices.at(accessorIndex).joints0.z = static_cast<UINT>(data[accessorIndex * 4 + 2]);
                            primitive.cachedVertices.at(accessorIndex).joints0.w = static_cast<UINT>(data[accessorIndex * 4 + 3]);
                        }
                    }
                    else if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        const BYTE* data = reinterpret_cast<const BYTE*>(gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset);
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            primitive.cachedVertices.at(accessorIndex).joints0.x = static_cast<UINT>(data[accessorIndex * 4 + 0]);
                            primitive.cachedVertices.at(accessorIndex).joints0.y = static_cast<UINT>(data[accessorIndex * 4 + 1]);
                            primitive.cachedVertices.at(accessorIndex).joints0.z = static_cast<UINT>(data[accessorIndex * 4 + 2]);
                            primitive.cachedVertices.at(accessorIndex).joints0.w = static_cast<UINT>(data[accessorIndex * 4 + 3]);
                        }
                    }
                    else
                    {
                        _ASSERT_EXPR(FALSE, L"This component type is unsupported, please convert it yourself if necessary.");
                    }
                }
                else if (gltfAttribute.first == "JOINTS_1")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    {
                        unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->joints1);
                        _Copy<DirectX::XMINT4>(dData, dStride, sData, sStride, count);
                    }
                    else if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        const USHORT* data = reinterpret_cast<const USHORT*>(gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset);
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            primitive.cachedVertices.at(accessorIndex).joints1.x = static_cast<UINT>(data[accessorIndex * 4 + 0]);
                            primitive.cachedVertices.at(accessorIndex).joints1.y = static_cast<UINT>(data[accessorIndex * 4 + 1]);
                            primitive.cachedVertices.at(accessorIndex).joints1.z = static_cast<UINT>(data[accessorIndex * 4 + 2]);
                            primitive.cachedVertices.at(accessorIndex).joints1.w = static_cast<UINT>(data[accessorIndex * 4 + 3]);
                        }
                    }
                    else if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        const BYTE* data = reinterpret_cast<const BYTE*>(gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset);
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            primitive.cachedVertices.at(accessorIndex).joints1.x = static_cast<UINT>(data[accessorIndex * 4 + 0]);
                            primitive.cachedVertices.at(accessorIndex).joints1.y = static_cast<UINT>(data[accessorIndex * 4 + 1]);
                            primitive.cachedVertices.at(accessorIndex).joints1.z = static_cast<UINT>(data[accessorIndex * 4 + 2]);
                            primitive.cachedVertices.at(accessorIndex).joints1.w = static_cast<UINT>(data[accessorIndex * 4 + 3]);
                        }
                    }
                    else
                    {
                        _ASSERT_EXPR(FALSE, L"This component type is unsupported, please convert it yourself if necessary.");
                    }
                }
                if (gltfAttribute.first == "WEIGHTS_0")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                    {
                        unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->weights0);
                        _Copy<DirectX::XMFLOAT4>(dData, dStride, sData, sStride, count);
                    }
                    else if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        std::vector<FLOAT> weights0(gltfAccessor.count * 4);
                        const USHORT* data = reinterpret_cast<const USHORT*>(gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset);
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            primitive.cachedVertices.at(accessorIndex).weights0.x = static_cast<FLOAT>(data[accessorIndex * 4 + 0]) / 0xFFFF;
                            primitive.cachedVertices.at(accessorIndex).weights0.y = static_cast<FLOAT>(data[accessorIndex * 4 + 1]) / 0xFFFF;
                            primitive.cachedVertices.at(accessorIndex).weights0.z = static_cast<FLOAT>(data[accessorIndex * 4 + 2]) / 0xFFFF;
                            primitive.cachedVertices.at(accessorIndex).weights0.w = static_cast<FLOAT>(data[accessorIndex * 4 + 3]) / 0xFFFF;
                        }
                    }
                    else if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        std::vector<FLOAT> weights0(gltfAccessor.count * 4);
                        const BYTE* data = reinterpret_cast<const BYTE*>(gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset);
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            primitive.cachedVertices.at(accessorIndex).weights0.x = static_cast<FLOAT>(data[accessorIndex * 4 + 0]) / 0xFF;
                            primitive.cachedVertices.at(accessorIndex).weights0.y = static_cast<FLOAT>(data[accessorIndex * 4 + 1]) / 0xFF;
                            primitive.cachedVertices.at(accessorIndex).weights0.z = static_cast<FLOAT>(data[accessorIndex * 4 + 2]) / 0xFF;
                            primitive.cachedVertices.at(accessorIndex).weights0.w = static_cast<FLOAT>(data[accessorIndex * 4 + 3]) / 0xFF;
                        }
                    }
                    else
                    {
                        _ASSERT_EXPR(FALSE, L"This component type is unsupported, please convert it yourself if necessary.");
                    }
                }
                else if (gltfAttribute.first == "WEIGHTS_1")
                {
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == primitive.cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                    {
                        unsigned char* dData = reinterpret_cast<unsigned char*>(&primitive.cachedVertices.data()->weights1);
                        _Copy<DirectX::XMFLOAT4>(dData, dStride, sData, sStride, count);
                    }
                    else if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        std::vector<FLOAT> weights0(gltfAccessor.count * 4);
                        const USHORT* data = reinterpret_cast<const USHORT*>(gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset);
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            primitive.cachedVertices.at(accessorIndex).weights1.x = static_cast<FLOAT>(data[accessorIndex * 4 + 0]) / 0xFFFF;
                            primitive.cachedVertices.at(accessorIndex).weights1.y = static_cast<FLOAT>(data[accessorIndex * 4 + 1]) / 0xFFFF;
                            primitive.cachedVertices.at(accessorIndex).weights1.z = static_cast<FLOAT>(data[accessorIndex * 4 + 2]) / 0xFFFF;
                            primitive.cachedVertices.at(accessorIndex).weights1.w = static_cast<FLOAT>(data[accessorIndex * 4 + 3]) / 0xFFFF;
                        }
                    }
                    else if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        std::vector<FLOAT> weights0(gltfAccessor.count * 4);
                        const BYTE* data = reinterpret_cast<const BYTE*>(gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset);
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            primitive.cachedVertices.at(accessorIndex).weights1.x = static_cast<FLOAT>(data[accessorIndex * 4 + 0]) / 0xFF;
                            primitive.cachedVertices.at(accessorIndex).weights1.y = static_cast<FLOAT>(data[accessorIndex * 4 + 1]) / 0xFF;
                            primitive.cachedVertices.at(accessorIndex).weights1.z = static_cast<FLOAT>(data[accessorIndex * 4 + 2]) / 0xFF;
                            primitive.cachedVertices.at(accessorIndex).weights1.w = static_cast<FLOAT>(data[accessorIndex * 4 + 3]) / 0xFF;
                        }
                    }
                    else
                    {
                        _ASSERT_EXPR(FALSE, L"This component type is unsupported, please convert it yourself if necessary.");
                    }
                }
                else
                {
                    //_ASSERT_EXPR(FALSE, L"This attribute is unsupported.");
                }
                primitive.attributes.emplace(gltfAttribute.first, _DxgiFormat(gltfAccessor));
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

void InterleavedGltfModel::FetchAndBatchMeshes(ID3D11Device* device, const tinygltf::Model& gltfModel)
{
    batchMeshes.resize(gltfModel.materials.size());

    std::function<void(int)> traverse = [&](int node_index)->void {
        const Node& node = nodes.at(node_index);
        if (node.mesh > -1)
        {
            const DirectX::XMMATRIX globalTransform = DirectX::XMLoadFloat4x4(&node.globalTransform);

            const tinygltf::Mesh& gltfMesh = gltfModel.meshes.at(node.mesh);

            for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
            {
#if 1
                if (gltfPrimitive.material < 0)
                {
                    // マテリアルが貼られていないとき
                    continue;
                }
#endif

                BatchMesh& batchMesh = batchMeshes.at(gltfPrimitive.material);
                batchMesh.material = gltfPrimitive.material;
                batchMesh.indexBufferView.format = DXGI_FORMAT_R32_UINT;
                if (gltfPrimitive.indices > -1)
                {
                    const tinygltf::Accessor& gltfAccessor = gltfModel.accessors.at(gltfPrimitive.indices);
                    const tinygltf::BufferView& gltfBufferView = gltfModel.bufferViews.at(gltfAccessor.bufferView);

                    std::vector<UINT> cachedIndices(gltfAccessor.count);
                    const size_t vertexOffset = batchMesh.cachedVertices.size();
                    if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        const BYTE* data = gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset;
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            cachedIndices.at(accessorIndex) = static_cast<UINT>(data[accessorIndex] + vertexOffset);
                        }
                    }
                    else if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        const USHORT* data = reinterpret_cast<const USHORT*>(gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset);
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            cachedIndices.at(accessorIndex) = static_cast<UINT>(data[accessorIndex] + vertexOffset);
                        }
                    }
                    else if (gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    {
                        const UINT* data = reinterpret_cast<const UINT*>(gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset);
                        for (size_t accessorIndex = 0; accessorIndex < gltfAccessor.count; ++accessorIndex)
                        {
                            cachedIndices.at(accessorIndex) = static_cast<UINT>(data[accessorIndex] + vertexOffset);
                        }
                    }
                    else
                    {
                        _ASSERT_EXPR(false, L"This index format is not supported.");
                    }

                    batchMesh.cachedIndices.insert(batchMesh.cachedIndices.end(), cachedIndices.begin(), cachedIndices.end());
                    batchMesh.indexBufferView.sizeInBytes += static_cast<UINT>(gltfAccessor.count * sizeof(UINT));
                }

                std::vector<BatchMesh::Vertex> cachedVertices;
                if (gltfPrimitive.attributes.size() > 0 && gltfPrimitive.attributes.find("POSITION") != gltfPrimitive.attributes.end())
                {
                    cachedVertices.resize(gltfModel.accessors.at(gltfPrimitive.attributes.at("POSITION")).count);
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
                    const size_t dStride = sizeof(BatchMesh::Vertex);
                    const size_t count = gltfAccessor.count;
                    _ASSERT_EXPR(count == cachedVertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");
                    if (gltfAttribute.first == "POSITION")
                    {
                        unsigned char* dData = reinterpret_cast<unsigned char*>(&cachedVertices.data()->position);
                        _Copy<DirectX::XMFLOAT3>(dData, dStride, sData, sStride, count);
                    }
                    else if (gltfAttribute.first == "NORMAL")
                    {
                        unsigned char* dData = reinterpret_cast<unsigned char*>(&cachedVertices.data()->normal);
                        _Copy<DirectX::XMFLOAT3>(dData, dStride, sData, sStride, count);
                    }
                    else if (gltfAttribute.first == "TANGENT")
                    {
                        unsigned char* dData = reinterpret_cast<unsigned char*>(&cachedVertices.data()->tangent);
                        _Copy<DirectX::XMFLOAT4>(dData, dStride, sData, sStride, count);
                    }
                    else if (gltfAttribute.first == "TEXCOORD_0")
                    {
                        unsigned char* dData = reinterpret_cast<unsigned char*>(&cachedVertices.data()->texcoord);
                        _Copy<DirectX::XMFLOAT2>(dData, dStride, sData, sStride, count);
                    }
                    else if (gltfAttribute.first == "TEXCOORD_1")
                    {
                        unsigned char* dData = reinterpret_cast<unsigned char*>(&cachedVertices.data()->texcoord1);
                        _Copy<DirectX::XMFLOAT2>(dData, dStride, sData, sStride, count);
                    }
                    else
                    {
                        Logger::Error(gltfAttribute.first + " is an unsupported attribute.");
                        //OutputDebugStringA((gltfAttribute.first + " is an unsupported attribute.\n").c_str());
                        //_ASSERT_EXPR(FALSE, L"This attribute is unsupported.");
                    }
                    batchMesh.attributes.emplace(gltfAttribute.first, _DxgiFormat(gltfAccessor));
                }

                for (BatchMesh::Vertex& cachedVertex : cachedVertices)
                {
                    DirectX::XMStoreFloat3(&cachedVertex.position, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&cachedVertex.position), globalTransform));

                    DirectX::XMMATRIX inverseTransposeGlobalTransform = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(NULL, DirectX::XMLoadFloat4x4(&node.globalTransform)));

                    DirectX::XMStoreFloat3(&cachedVertex.normal, DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&cachedVertex.normal), inverseTransposeGlobalTransform)));

                    float sigma = cachedVertex.tangent.w;
                    cachedVertex.tangent.w = 0;
                    DirectX::XMStoreFloat4(&cachedVertex.tangent, DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat4(&cachedVertex.tangent), inverseTransposeGlobalTransform)));
                    cachedVertex.tangent.w = sigma;
                }

                batchMesh.cachedVertices.insert(batchMesh.cachedVertices.end(), cachedVertices.begin(), cachedVertices.end());
                batchMesh.vertexBufferView.sizeInBytes += static_cast<UINT>(cachedVertices.size() * sizeof(BatchMesh::Vertex));
                batchMesh.vertexBufferView.strideInBytes = static_cast<UINT>(sizeof(BatchMesh::Vertex));
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

void InterleavedGltfModel::FetchMaterials(ID3D11Device* device, const tinygltf::Model& gltfModel)
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
void InterleavedGltfModel::FetchTextures(ID3D11Device* device, const tinygltf::Model& gltfModel)
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

void InterleavedGltfModel::FetchAnimations(const tinygltf::Model& gltfModel, std::vector<Animation>& outAnimations)
{
    char buf[256];

    // skinsの状態を出力
    sprintf_s(buf, "Before emplace_back, skins size=%zu, capacity=%zu\n", skins.size(), skins.capacity());
    OutputDebugStringA(buf);
    for (const tinygltf::Skin& transmissionSkin : gltfModel.skins)
    {
        sprintf_s(buf, "Processing skin, transmissionSkin.inverseBindMatrices=%d\n", transmissionSkin.inverseBindMatrices);
        OutputDebugStringA(buf);
        Skin& skin = skins.emplace_back();
        sprintf_s(buf, "After emplace_back, skins size=%zu\n", skins.size());
        OutputDebugStringA(buf);
        const tinygltf::Accessor& gltfAccessor = gltfModel.accessors.at(transmissionSkin.inverseBindMatrices);
        const tinygltf::BufferView& gltfBufferView = gltfModel.bufferViews.at(gltfAccessor.bufferView);
        _ASSERT_EXPR(gltfAccessor.type == TINYGLTF_TYPE_MAT4, L"");

        skin.inverseBindMatrices.resize(gltfAccessor.count);
        memcpy(skin.inverseBindMatrices.data(), gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset, gltfAccessor.count * sizeof(DirectX::XMFLOAT4X4));

        skin.joints = transmissionSkin.joints;
    }

    for (const tinygltf::Animation& gltfAnimation : gltfModel.animations)
    {
        //Animation& animation = animations.emplace_back();
        Animation& animation = outAnimations.emplace_back();
        animation.name = gltfAnimation.name;
        for (const tinygltf::AnimationSampler& gltfSampler : gltfAnimation.samplers)
        {
            Animation::Sampler& sampler = animation.samplers.emplace_back();
            sampler.input = gltfSampler.input;
            sampler.output = gltfSampler.output;
            sampler.interpolation = gltfSampler.interpolation;

            const tinygltf::Accessor& gltfAccessor = gltfModel.accessors.at(gltfSampler.input);
            const tinygltf::BufferView& gltfBufferView = gltfModel.bufferViews.at(gltfAccessor.bufferView);
            _ASSERT_EXPR(gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
            _ASSERT_EXPR(gltfAccessor.type == TINYGLTF_TYPE_SCALAR, L"");
            const std::pair<std::unordered_map<int, std::vector<float>>::iterator, bool>& timelines = animation.timelines.emplace(gltfSampler.input, gltfAccessor.count);
            if (timelines.second)
            {
                memcpy(timelines.first->second.data(), gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset, gltfAccessor.count * sizeof(FLOAT));
            }
        }
        for (const tinygltf::AnimationChannel& gltfChannel : gltfAnimation.channels)
        {
            Animation::Channel& channel = animation.channels.emplace_back();
            channel.sampler = gltfChannel.sampler;
            channel.targetNode = gltfChannel.target_node;
            channel.targetPath = gltfChannel.target_path;

            const tinygltf::AnimationSampler& gltfSampler = gltfAnimation.samplers.at(gltfChannel.sampler);
            const tinygltf::Accessor& gltfAccessor = gltfModel.accessors.at(gltfSampler.output);
            const tinygltf::BufferView& gltfBufferView = gltfModel.bufferViews.at(gltfAccessor.bufferView);
            if (gltfChannel.target_path == "scale")
            {
                _ASSERT_EXPR(gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
                _ASSERT_EXPR(gltfAccessor.type == TINYGLTF_TYPE_VEC3, L"");

                const std::pair<std::unordered_map<int, std::vector<DirectX::XMFLOAT3>>::iterator, bool>& scales = animation.scales.emplace(gltfSampler.output, gltfAccessor.count);
                if (scales.second)
                {
                    memcpy(scales.first->second.data(), gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset, gltfAccessor.count * sizeof(DirectX::XMFLOAT3));
                }
            }
            else if (gltfChannel.target_path == "rotation")
            {
                _ASSERT_EXPR(gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
                _ASSERT_EXPR(gltfAccessor.type == TINYGLTF_TYPE_VEC4, L"");

                const std::pair<std::unordered_map<int, std::vector<DirectX::XMFLOAT4>>::iterator, bool>& rotations = animation.rotations.emplace(gltfSampler.output, gltfAccessor.count);
                if (rotations.second)
                {
                    memcpy(rotations.first->second.data(), gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset, gltfAccessor.count * sizeof(DirectX::XMFLOAT4));
                }
            }
            else if (gltfChannel.target_path == "translation")
            {
                _ASSERT_EXPR(gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
                _ASSERT_EXPR(gltfAccessor.type == TINYGLTF_TYPE_VEC3, L"");
                const std::pair<std::unordered_map<int, std::vector<DirectX::XMFLOAT3>>::iterator, bool>& translations = animation.translations.emplace(gltfSampler.output, gltfAccessor.count);
                if (translations.second)
                {
                    memcpy(translations.first->second.data(), gltfModel.buffers.at(gltfBufferView.buffer).data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset, gltfAccessor.count * sizeof(DirectX::XMFLOAT3));
                }
            }
            else if (gltfChannel.target_path == "weights")
            {
                //_ASSERT_EXPR(FALSE, L"");
            }
            else
            {
                _ASSERT_EXPR(FALSE, L"");
            }
        }
    }
    // Find a longest animation duration in timeline of each channel.
    for (decltype(animations)::reference animation : outAnimations)
    {
        for (decltype(animation.timelines)::reference timelines : animation.timelines)
        {
            animation.duration = std::max<float>(animation.duration, timelines.second.back());
        }
    }

}
void InterleavedGltfModel::FetchAnimations(const tinygltf::Model& gltf_model)
{
    for (const tinygltf::Skin& transmission_skin : gltf_model.skins)
    {
        Skin& skin = skins.emplace_back();
        const tinygltf::Accessor& gltf_accessor = gltf_model.accessors.at(transmission_skin.inverseBindMatrices);
        const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_accessor.bufferView);
        _ASSERT_EXPR(gltf_accessor.type == TINYGLTF_TYPE_MAT4, L"");

        skin.inverseBindMatrices.resize(gltf_accessor.count);
        memcpy(skin.inverseBindMatrices.data(), gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(DirectX::XMFLOAT4X4));

        skin.joints = transmission_skin.joints;
    }

    for (const tinygltf::Animation& gltf_animation : gltf_model.animations)
    {
        Animation& animation = animations.emplace_back();
        animation.name = gltf_animation.name;
        for (const tinygltf::AnimationSampler& gltf_sampler : gltf_animation.samplers)
        {
            Animation::Sampler& sampler = animation.samplers.emplace_back();
            sampler.input = gltf_sampler.input;
            sampler.output = gltf_sampler.output;
            sampler.interpolation = gltf_sampler.interpolation;

            const tinygltf::Accessor& gltf_accessor = gltf_model.accessors.at(gltf_sampler.input);
            const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_accessor.bufferView);
            _ASSERT_EXPR(gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
            _ASSERT_EXPR(gltf_accessor.type == TINYGLTF_TYPE_SCALAR, L"");
            const std::pair<std::unordered_map<int, std::vector<float>>::iterator, bool>& timelines = animation.timelines.emplace(gltf_sampler.input, gltf_accessor.count);
            if (timelines.second)
            {
                memcpy(timelines.first->second.data(), gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(FLOAT));
            }
        }
        for (const tinygltf::AnimationChannel& gltf_channel : gltf_animation.channels)
        {
            Animation::Channel& channel = animation.channels.emplace_back();
            channel.sampler = gltf_channel.sampler;
            channel.targetNode = gltf_channel.target_node;
            channel.targetPath = gltf_channel.target_path;

            const tinygltf::AnimationSampler& gltf_sampler = gltf_animation.samplers.at(gltf_channel.sampler);
            const tinygltf::Accessor& gltf_accessor = gltf_model.accessors.at(gltf_sampler.output);
            const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_accessor.bufferView);
            if (gltf_channel.target_path == "scale")
            {
                _ASSERT_EXPR(gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
                _ASSERT_EXPR(gltf_accessor.type == TINYGLTF_TYPE_VEC3, L"");

                const std::pair<std::unordered_map<int, std::vector<DirectX::XMFLOAT3>>::iterator, bool>& scales = animation.scales.emplace(gltf_sampler.output, gltf_accessor.count);
                if (scales.second)
                {
                    memcpy(scales.first->second.data(), gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(DirectX::XMFLOAT3));
                }
            }
            else if (gltf_channel.target_path == "rotation")
            {
                _ASSERT_EXPR(gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
                _ASSERT_EXPR(gltf_accessor.type == TINYGLTF_TYPE_VEC4, L"");

                const std::pair<std::unordered_map<int, std::vector<DirectX::XMFLOAT4>>::iterator, bool>& rotations = animation.rotations.emplace(gltf_sampler.output, gltf_accessor.count);
                if (rotations.second)
                {
                    memcpy(rotations.first->second.data(), gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(DirectX::XMFLOAT4));
                }
            }
            else if (gltf_channel.target_path == "translation")
            {
                _ASSERT_EXPR(gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
                _ASSERT_EXPR(gltf_accessor.type == TINYGLTF_TYPE_VEC3, L"");
                const std::pair<std::unordered_map<int, std::vector<DirectX::XMFLOAT3>>::iterator, bool>& translations = animation.translations.emplace(gltf_sampler.output, gltf_accessor.count);
                if (translations.second)
                {
                    memcpy(translations.first->second.data(), gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(DirectX::XMFLOAT3));
                }
            }
            else if (gltf_channel.target_path == "weights")
            {
                //_ASSERT_EXPR(FALSE, L"");
            }
            else
            {
                _ASSERT_EXPR(FALSE, L"");
            }
        }
    }
    // Find a longest animation duration in timeline of each channel.
    for (decltype(animations)::reference animation : animations)
    {
        for (decltype(animation.timelines)::reference timelines : animation.timelines)
        {
            animation.duration = std::max<float>(animation.duration, timelines.second.back());
        }
    }

}

//アニメーションをブレンドする関数
void InterleavedGltfModel::BlendAnimations(const std::vector<Node>& fromNodes, const std::vector<Node>& toNodes, float factor, std::vector<Node>& outNodes)
{
    using namespace DirectX;
    _ASSERT_EXPR(fromNodes.size() == toNodes.size(), "The size of the two node arrays must be the same.");

    size_t nodeCount{ fromNodes.size() };
    _ASSERT_EXPR(nodeCount == outNodes.size(), "The size of output nodes must be input nodes.");
    for (size_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
    {
        XMVECTOR S[2]{ XMLoadFloat3(&fromNodes.at(nodeIndex).scale), XMLoadFloat3(&toNodes.at(nodeIndex).scale) };
        DirectX::XMStoreFloat3(&outNodes.at(nodeIndex).scale, XMVectorLerp(S[0], S[1], factor));

        XMVECTOR R[2]{ XMLoadFloat4(&fromNodes.at(nodeIndex).rotation), XMLoadFloat4(&toNodes.at(nodeIndex).rotation) };
        XMStoreFloat4(&outNodes.at(nodeIndex).rotation, XMQuaternionSlerp(R[0], R[1], factor));

        XMVECTOR T[2]{ XMLoadFloat3(&fromNodes.at(nodeIndex).translation), DirectX::XMLoadFloat3(&toNodes.at(nodeIndex).translation) };
        DirectX::XMStoreFloat3(&outNodes.at(nodeIndex).translation, DirectX::XMVectorLerp(T[0], T[1], factor));
    }
    CumulateTransforms(outNodes);
}


void InterleavedGltfModel::Animate(size_t animationIndex, float time, std::vector<Node>& animatedNodes)
{
    _ASSERT_EXPR(animations.size() > animationIndex, L"");
    _ASSERT_EXPR(animatedNodes.size() == nodes.size(), L"");

    std::function<size_t(const std::vector<float>&, float, float&)> indexof = [](const std::vector<float>& timelines, float time, float& interpolationFactor)->size_t
        {
            const size_t keyframeCount = timelines.size();
            if (time > timelines.at(keyframeCount - 1))
            {
                interpolationFactor = 1.0f;
                if (keyframeCount < 2)
                {
                    interpolationFactor = 0.0f;
                    return 0;
                }
                return keyframeCount - 2;
            }
            else if (time < timelines.at(0))
            {
                interpolationFactor = 0.0f;
                return 0;
            }
            size_t keyframeIndex = 0;
            for (size_t timeIndex = 1; timeIndex < keyframeCount; ++timeIndex)
            {
                if (time < timelines.at(timeIndex))
                {
                    keyframeIndex = std::max<size_t>(0LL, timeIndex - 1);
                    break;
                }
            }
            interpolationFactor = (time - timelines.at(keyframeIndex + 0)) / (timelines.at(keyframeIndex + 1) - timelines.at(keyframeIndex + 0));
            return keyframeIndex;
        };

    if (animations.size() > 0)
    {
        const Animation& animation = animations.at(animationIndex);


        for (std::vector<Animation::Channel>::const_reference channel : animation.channels)
        {
            const Animation::Sampler& sampler = animation.samplers.at(channel.sampler);
            const std::vector<float>& timeline = animation.timelines.at(sampler.input);

            if (timeline.size() == 0 || channel.targetNode >= animatedNodes.size())
            {
                continue;
            }
            float interpolationFactor = {};
            size_t keyframeIndex = indexof(timeline, time, interpolationFactor);

            // 対象のプロパティに対して補完と適用を行う
            if (channel.targetPath == "scale")
            {
                const std::vector<DirectX::XMFLOAT3>& scales = animation.scales.at(sampler.output);

                DirectX::XMStoreFloat3(&animatedNodes.at(channel.targetNode).scale, DirectX::XMVectorLerp(XMLoadFloat3(&scales.at(keyframeIndex + 0)), DirectX::XMLoadFloat3(&scales.at(keyframeIndex + 1)), interpolationFactor));
            }
            else if (channel.targetPath == "rotation")
            {
                const std::vector<DirectX::XMFLOAT4>& rotations = animation.rotations.at(sampler.output);
                DirectX::XMStoreFloat4(&animatedNodes.at(channel.targetNode).rotation, DirectX::XMQuaternionNormalize(DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&rotations.at(keyframeIndex + 0)), DirectX::XMLoadFloat4(&rotations.at(keyframeIndex + 1)), interpolationFactor)));
            }
            else if (channel.targetPath == "translation")
            {
                const std::vector<DirectX::XMFLOAT3>& translations = animation.translations.at(sampler.output);
                DirectX::XMStoreFloat3(&animatedNodes.at(channel.targetNode).translation, DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&translations.at(keyframeIndex + 0)), DirectX::XMLoadFloat3(&translations.at(keyframeIndex + 1)), interpolationFactor));
            }
            else if (channel.targetPath == "weights")
            {

            }
            else
            {

            }
        }
        CumulateTransforms(animatedNodes);
    }
}

void InterleavedGltfModel::CreateAndUploadResources(ID3D11Device* device)
{
    HRESULT hr;
    D3D11_BUFFER_DESC bufferDesc = {};
    D3D11_SUBRESOURCE_DATA subresourceData = {};

    // GPU上で頂点バッファとインデックスバッファを作成してアップロードする
    if (mode == ModelTypes::ModelMode::StaticMesh || mode == ModelTypes::ModelMode::InstancedStaticMesh)
    {
        for (BatchMesh& batchMesh : batchMeshes)
        {
            if (batchMesh.indexBufferView.sizeInBytes > 0)
            {
                batchMesh.indexBufferView.buffer = static_cast<int>(buffers.size());
                bufferDesc.ByteWidth = batchMesh.indexBufferView.sizeInBytes;
                bufferDesc.Usage = D3D11_USAGE_DEFAULT;
                bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                bufferDesc.CPUAccessFlags = 0;
                bufferDesc.MiscFlags = 0;
                bufferDesc.StructureByteStride = 0;
                subresourceData.pSysMem = batchMesh.cachedIndices.data();
                subresourceData.SysMemPitch = 0;
                subresourceData.SysMemSlicePitch = 0;
                hr = device->CreateBuffer(&bufferDesc, &subresourceData, buffers.emplace_back().GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
                if (!isSaveVerticesData)
                {
                    batchMesh.cachedIndices.clear();
                }
            }

            if (batchMesh.vertexBufferView.sizeInBytes > 0)
            {
                batchMesh.vertexBufferView.buffer = static_cast<int>(buffers.size());
                bufferDesc.ByteWidth = batchMesh.vertexBufferView.sizeInBytes;
                bufferDesc.Usage = D3D11_USAGE_DEFAULT;
                bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                bufferDesc.CPUAccessFlags = 0;
                bufferDesc.MiscFlags = 0;
                bufferDesc.StructureByteStride = 0;
                subresourceData.pSysMem = batchMesh.cachedVertices.data();
                subresourceData.SysMemPitch = 0;
                subresourceData.SysMemSlicePitch = 0;
                hr = device->CreateBuffer(&bufferDesc, &subresourceData, buffers.emplace_back().GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
                if (!isSaveVerticesData)
                {
                    batchMesh.cachedVertices.clear();
                }
            }
        }
    }
    else
    {
        for (Mesh& mesh : meshes)
        {
            for (Mesh::Primitive& primitive : mesh.primitives)
            {
                if (primitive.indexBufferView.sizeInBytes > 0)
                {
                    // R8_UINT の場合は 16bit に変換
                    if (primitive.indexBufferView.format == DXGI_FORMAT_R8_UINT)
                    {
                        std::vector<uint16_t> indices16(primitive.cachedIndices.size());
                        const BYTE* src = primitive.cachedIndices.data();
                        for (size_t i = 0; i < primitive.cachedIndices.size(); ++i)
                            indices16[i] = static_cast<uint16_t>(src[i]);
                        primitive.cachedIndices.resize(sizeof(uint16_t) * indices16.size());
                        memcpy(primitive.cachedIndices.data(), indices16.data(), primitive.cachedIndices.size());
                        primitive.indexBufferView.format = DXGI_FORMAT_R16_UINT;
                    }


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
                    if (!isSaveVerticesData)
                    {
                        primitive.cachedIndices.clear();
                    }
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
                    if (!isSaveVerticesData)
                    {
                        primitive.cachedVertices.clear();
                    }
                }
            }
        }
    }

    // Instance Buffer を作る
    if (mode == ModelTypes::ModelMode::InstancedStaticMesh)
    {
        D3D11_BUFFER_DESC bufferDesc = {};
#if 1 // これ動くとき
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
#else
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.CPUAccessFlags = 0;
#endif
        bufferDesc.ByteWidth = static_cast<UINT>(sizeof(InstanceData) * 1000);
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        //D3D11_SUBRESOURCE_DATA subresourceData = {};
        //subresourceData.pSysMem = instanceMatrices_.data();
        HRESULT hr = device->CreateBuffer(&bufferDesc, NULL, instanceBuffer.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

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

    if (mode == ModelTypes::ModelMode::StaticMesh)
    {
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelStaticBatchingVS.cso", vertexShader.ReleaseAndGetAddressOf(), inputLayout.ReleaseAndGetAddressOf(), inputElementDesc, _countof(inputElementDesc));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelStaticBatchingCsmVS.cso", vertexShaderCSM.ReleaseAndGetAddressOf(), NULL, NULL, 0);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    }
    else if (mode == ModelTypes::ModelMode::SkeletalMesh)
    {
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "JOINTS", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "JOINTS", 1, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "WEIGHTS", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelVS.cso", vertexShader.ReleaseAndGetAddressOf(), inputLayout.ReleaseAndGetAddressOf(), inputElementDesc, _countof(inputElementDesc));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        //hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelCsmVS.cso", vertexShaderCSM.ReleaseAndGetAddressOf(), NULL, NULL, 0);
        hr = CreateVsFromCSO(device, "./Data/Shaders/ElasticBuildingCsmVS.cso", vertexShaderCSM.ReleaseAndGetAddressOf(), NULL, NULL, 0);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    }
    else if (mode == ModelTypes::ModelMode::InstancedStaticMesh)
    {
        D3D11_INPUT_ELEMENT_DESC instancedStaticMeshInputElementDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },

            { "INSTANCE_MATRIX", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_MATRIX", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_MATRIX", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_MATRIX", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },

            { "INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_EMISSIVE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},


        };
        hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelInstancedBatchingVS.cso", vertexShader.ReleaseAndGetAddressOf(), inputLayout.ReleaseAndGetAddressOf(), instancedStaticMeshInputElementDesc, _countof(instancedStaticMeshInputElementDesc));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelInstancedBatchingCsmVS.cso", vertexShaderCSM.ReleaseAndGetAddressOf(), NULL, NULL, 0);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelPS.cso", pixelShader.ReleaseAndGetAddressOf());
    //hr = CreatePsFromCSO(device, "./Shader/GltfModelDeferredPS.cso", pixelShader.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    hr = CreateGsFromCSO(device, "./Data/Shaders/GltfModelCsmGS.cso", geometryShaderCSM.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


    bufferDesc.ByteWidth = sizeof(PrimitiveConstants);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;
    hr = device->CreateBuffer(&bufferDesc, nullptr, primitiveCBuffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    bufferDesc.ByteWidth = sizeof(PrimitiveJointConstants);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;
    hr = device->CreateBuffer(&bufferDesc, NULL, primitiveJointCbuffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}


void InterleavedGltfModel::Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world, const std::vector<Node>& animated_nodes, RenderPass pass, const PipeLineStateDesc& pipeline)
{
    if (mode == ModelTypes::ModelMode::StaticMesh)
    {
        return BatchRender(immediateContext, world, pass, pipeline);
    }
    else if (mode == ModelTypes::ModelMode::InstancedStaticMesh)
    {
        return InstancedStaticBatchRender(immediateContext/*, world*/, pass, pipeline);
    }
    const std::vector<Node>& nodes = animated_nodes.size() > 0 ? animated_nodes : InterleavedGltfModel::nodes;

    immediateContext->PSSetShaderResources(0, 1, materialResourceView.GetAddressOf());

    immediateContext->VSSetShader(pipeline.vertexShader ? pipeline.vertexShader.Get() : vertexShader.Get(), nullptr, 0);
    immediateContext->IASetInputLayout(inputLayout.Get());
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    std::function<void(int)> traverse = [&](int nodeIndex)->void {
        const Node& node = nodes.at(nodeIndex);
        if (node.skin > -1)
        {
            const Skin& skin = skins.at(node.skin);
            _ASSERT_EXPR(skin.joints.size() <= PRIMITIVE_MAX_JOINTS, L"The size of the joint array is insufficient, please expand it.");
            PrimitiveJointConstants primitiveJointData{};
            for (size_t jointIndex = 0; jointIndex < skin.joints.size(); ++jointIndex)
            {
                DirectX::XMStoreFloat4x4(&primitiveJointData.matrices[jointIndex],
                    DirectX::XMLoadFloat4x4(&skin.inverseBindMatrices.at(jointIndex)) *
                    DirectX::XMLoadFloat4x4(&nodes.at(skin.joints.at(jointIndex)).globalTransform) *
                    DirectX::XMMatrixInverse(NULL, DirectX::XMLoadFloat4x4(&node.globalTransform))
                );
            }
            immediateContext->UpdateSubresource(primitiveJointCbuffer.Get(), 0, 0, &primitiveJointData, 0, 0);
            immediateContext->VSSetConstantBuffers(2, 1, primitiveJointCbuffer.GetAddressOf());
        }
        if (node.mesh > -1)
        {
            const Mesh& mesh = meshes.at(node.mesh);
            for (const Mesh::Primitive& primitive : mesh.primitives)
            {
                // INTERLEAVED_GLTF_MODEL
                UINT stride = sizeof(Mesh::Vertex);
                UINT offset = 0;
                immediateContext->IASetVertexBuffers(0, 1, buffers.at(primitive.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);

                PrimitiveConstants primitiveData = {};
                primitiveData.material = primitive.material;
                primitiveData.hasTangent = primitive.has("TANGENT");
                primitiveData.skin = node.skin;

                //primitiveData.color = { cpuColor.x,cpuColor.y,cpuColor.z,alpha };
                //primitiveData.emission = emission;
                //primitiveData.disolveFactor = disolveFactor;
                // ここでモデル座標系を変換する？
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
                DirectX::XMStoreFloat4x4(&primitiveData.inverseTransposeWorld, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(NULL, DirectX::XMLoadFloat4x4(&node.globalTransform) * DirectX::XMLoadFloat4x4(&world))));

                const Material& material = materials.at(primitive.material);
                //ここで設定
                if (material.replacedPixelShader)
                {
                    immediateContext->PSSetShader(material.replacedPixelShader.Get(), nullptr, 0);
                }
                else
                {
                    immediateContext->PSSetShader(pipeline.pixelShader ? pipeline.pixelShader.Get() : pixelShader.Get(), nullptr, 0);
                }

                // ここでマテリアル種類を渡す
                primitiveData.materialType = static_cast<int>(material.materialType);
                immediateContext->UpdateSubresource(primitiveCBuffer.Get(), 0, 0, &primitiveData, 0, 0);
                immediateContext->VSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());
                immediateContext->PSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());


                bool passed = false;
                switch (pass)
                {
                case RenderPass::Opaque:
                    if (material.data.alphaMode == 0/*OPAQUE*/)
                    {
                        RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE);
                        passed = true;
                    }
                    break;
                case RenderPass::Mask:
                    if (material.data.alphaMode == 1/*MASK*/)
                    {
                        RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE);
                        passed = true;
                    }
                    break;
                case RenderPass::Blend:
                    if (material.data.alphaMode == 2/*BLEND*/)
                    {
                        RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA);
                        passed = true;
                    }
                    break;
                case RenderPass::All:
                    passed = true;
                    break;
                }
                if (!passed)
                {
                    continue;
                }

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
                    // INTERLEAVED_GLTF_MODEL
                    immediateContext->IASetIndexBuffer(buffers.at(primitive.indexBufferView.buffer).Get(), primitive.indexBufferView.format, 0);
                    immediateContext->DrawIndexed(primitive.indexBufferView.sizeInBytes / GetFormatByteSize(primitive.indexBufferView.format), 0, 0);
                }
                else
                {
                    // INTERLEAVED_GLTF_MODEL
                    immediateContext->Draw(primitive.vertexBufferView.sizeInBytes / primitive.vertexBufferView.strideInBytes, 0);
                }
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

void InterleavedGltfModel::BatchRender(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world, RenderPass pass, const PipeLineStateDesc& pipeline)
{
    _ASSERT_EXPR(mode == ModelTypes::ModelMode::StaticMesh, L"This function only works with static_batching data.");

    immediateContext->PSSetShaderResources(0, 1, materialResourceView.GetAddressOf());

    immediateContext->VSSetShader(pipeline.vertexShader ? pipeline.vertexShader.Get() : vertexShader.Get(), nullptr, 0);
    immediateContext->PSSetShader(pipeline.pixelShader ? pipeline.pixelShader.Get() : pixelShader.Get(), nullptr, 0);
    immediateContext->IASetInputLayout(inputLayout.Get());
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (const BatchMesh& batchMesh : batchMeshes)
    {
        UINT stride = sizeof(BatchMesh::Vertex);
        UINT offset = 0;
        immediateContext->IASetVertexBuffers(0, 1, buffers.at(batchMesh.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);

        PrimitiveConstants primitiveData = {};
        primitiveData.material = batchMesh.material;
        primitiveData.hasTangent = batchMesh.has("TANGENT");
        primitiveData.skin = -1;
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
        DirectX::XMStoreFloat4x4(&primitiveData.world, C * DirectX::XMLoadFloat4x4(&world));
        DirectX::XMStoreFloat4x4(&primitiveData.inverseTransposeWorld, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(NULL, C * DirectX::XMLoadFloat4x4(&world))));

        immediateContext->UpdateSubresource(primitiveCBuffer.Get(), 0, 0, &primitiveData, 0, 0);
        immediateContext->VSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());
        immediateContext->PSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());

        const Material& material = materials.at(batchMesh.material);

        bool passed = false;
        switch (pass)
        {
        case RenderPass::Opaque:
            if (material.data.alphaMode == 0/*OPAQUE*/)
            {
                RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE);
                passed = true;
            }
            break;
        case RenderPass::Mask:
            if (material.data.alphaMode == 1/*MASK*/)
            {
                RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE);
                passed = true;
            }
            break;
        case RenderPass::Blend:
            if (material.data.alphaMode == 2/*BLEND*/)
            {
                RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA);
                passed = true;
            }
            break;
        }
        if (!passed)
        {
            continue;
        }

        const int textureIndices[]
        {
            material.data.pbrMetallicRoughness.basecolorTexture.index,
            material.data.pbrMetallicRoughness.metallicRoughnessTexture.index,
            material.data.normalTexture.index,
            material.data.emissiveTexture.index,
            material.data.occlusionTexture.index,
        };
        //RenderState::BindRasterizerState(immediateContext, RASTER_STATE::SOLID_CULL_NONE);
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_BACK);

        ID3D11ShaderResourceView* nullShaderResourceView{};
        std::vector<ID3D11ShaderResourceView*> shaderResourceViews(_countof(textureIndices));
        for (int textureIndex = 0; textureIndex < shaderResourceViews.size(); ++textureIndex)
        {
            shaderResourceViews.at(textureIndex) = textureIndices[textureIndex] > -1 ? textureResourceViews.at(textures.at(textureIndices[textureIndex]).source).Get() : nullShaderResourceView;
        }
        immediateContext->PSSetShaderResources(1, static_cast<UINT>(shaderResourceViews.size()), shaderResourceViews.data());

        if (batchMesh.indexBufferView.buffer > -1)
        {
            immediateContext->IASetIndexBuffer(buffers.at(batchMesh.indexBufferView.buffer).Get(), batchMesh.indexBufferView.format, 0);
            immediateContext->DrawIndexed(batchMesh.indexBufferView.sizeInBytes / GetFormatByteSize(batchMesh.indexBufferView.format), 0, 0);
        }
        else
        {
            immediateContext->Draw(batchMesh.vertexBufferView.sizeInBytes / batchMesh.vertexBufferView.strideInBytes, 0);
        }
    }
}

void InterleavedGltfModel::InstancedStaticBatchRender(ID3D11DeviceContext* immediateContext/*, const DirectX::XMFLOAT4X4& world*/, RenderPass pass, const PipeLineStateDesc& pipeline)
{
    _ASSERT_EXPR(mode == ModelTypes::ModelMode::InstancedStaticMesh, L"This function only works with instance_static_batching data.");

    immediateContext->PSSetShaderResources(0, 1, materialResourceView.GetAddressOf());

    immediateContext->VSSetShader(pipeline.vertexShader ? pipeline.vertexShader.Get() : vertexShader.Get(), nullptr, 0);
    immediateContext->PSSetShader(pipeline.pixelShader ? pipeline.pixelShader.Get() : pixelShader.Get(), nullptr, 0);
    immediateContext->IASetInputLayout(inputLayout.Get());
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


    UINT strideInstance = sizeof(InstanceData);
    UINT offsetInstance = 0;
    //immediateContext->IASetVertexBuffers(1, 1, instancedStaticMesh->buffer_.GetAddressOf(), &strideInstance, &offsetInstance);
    immediateContext->IASetVertexBuffers(1, 1, instanceBuffer.GetAddressOf(), &strideInstance, &offsetInstance);

    //int instanceCount = instancedStaticMesh->instanceCount();
    int instanceCount = this->instanceCount_;

    for (const BatchMesh& batchMesh : batchMeshes)
    {
        PrimitiveConstants primitiveData = {};
        primitiveData.material = batchMesh.material;
        primitiveData.hasTangent = batchMesh.has("TANGENT");
        primitiveData.skin = -1;
        //primitiveData.emission = emission;
        //primitiveData.world = world;
        immediateContext->UpdateSubresource(primitiveCBuffer.Get(), 0, 0, &primitiveData, 0, 0);
        immediateContext->VSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());
        immediateContext->PSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());
        UINT stride = sizeof(BatchMesh::Vertex);
        UINT offset = 0;
        immediateContext->IASetVertexBuffers(0, 1, buffers.at(batchMesh.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);
        const Material& material = materials.at(batchMesh.material);
        bool passed = false;
        switch (pass)
        {
        case RenderPass::Opaque:
            if (material.data.alphaMode == 0/*OPAQUE*/)
            {
                RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE);
                passed = true;
            }
            break;
        case RenderPass::Mask:
            if (material.data.alphaMode == 1/*MASK*/)
            {
                RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE);
                passed = true;
            }
            break;
        case RenderPass::Blend:
            if (material.data.alphaMode == 2/*BLEND*/)
            {
                RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA);
                passed = true;
            }
            break;
        case RenderPass::All:
            passed = true;
            break;
        }
        if (!passed)
        {
            continue;
        }

        const int textureIndices[]
        {
            material.data.pbrMetallicRoughness.basecolorTexture.index,
            material.data.pbrMetallicRoughness.metallicRoughnessTexture.index,
            material.data.normalTexture.index,
            material.data.emissiveTexture.index,
            material.data.occlusionTexture.index,
        };
        RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);

        ID3D11ShaderResourceView* nullShaderResourceView{};
        std::vector<ID3D11ShaderResourceView*> shaderResourceViews(_countof(textureIndices));
        for (int textureIndex = 0; textureIndex < shaderResourceViews.size(); ++textureIndex)
        {
            shaderResourceViews.at(textureIndex) = textureIndices[textureIndex] > -1 ? textureResourceViews.at(textures.at(textureIndices[textureIndex]).source).Get() : nullShaderResourceView;
        }
        immediateContext->PSSetShaderResources(1, static_cast<UINT>(shaderResourceViews.size()), shaderResourceViews.data());

        //if (batchMesh.indexBufferView.buffer > -1)
        {
            immediateContext->IASetIndexBuffer(buffers.at(batchMesh.indexBufferView.buffer).Get(), batchMesh.indexBufferView.format, 0);
            immediateContext->DrawIndexedInstanced(batchMesh.indexBufferView.sizeInBytes / GetFormatByteSize(batchMesh.indexBufferView.format), instanceCount, 0, 0, 0);
        }
        //else
        {
            //immediateContext->DrawInstanced(batchMesh.vertexBufferView.sizeInBytes / batchMesh.vertexBufferView.strideInBytes, instanceCount, 0, 0);
        }
    }
}

void InterleavedGltfModel::CastShadowBatch(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world)
{
    _ASSERT_EXPR(mode == ModelTypes::ModelMode::StaticMesh, L"This function only works with static_batching data.");

    immediateContext->PSSetShaderResources(0, 1, materialResourceView.GetAddressOf());

    // CASCADED_SHADOW_MAPS
    immediateContext->VSSetShader(vertexShaderCSM.Get(), nullptr, 0);
    immediateContext->GSSetShader(geometryShaderCSM.Get(), nullptr, 0);

    Microsoft::WRL::ComPtr <ID3D11PixelShader> nullPixelShader{ NULL };
    immediateContext->PSSetShader(nullPixelShader.Get()/*SHADOW*/, nullptr, 0);

    //immediate_context->PSSetShader(pixelShader.Get(), nullptr, 0);
    immediateContext->IASetInputLayout(inputLayout.Get());
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (const BatchMesh& batchMesh : batchMeshes)
    {
        UINT stride = sizeof(BatchMesh::Vertex);
        UINT offset = 0;
        immediateContext->IASetVertexBuffers(0, 1, buffers.at(batchMesh.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);

        PrimitiveConstants primitiveData = {};
        primitiveData.material = batchMesh.material;
        primitiveData.hasTangent = batchMesh.has("TANGENT");
        primitiveData.skin = -1;
        primitiveData.world = world;
        immediateContext->UpdateSubresource(primitiveCBuffer.Get(), 0, 0, &primitiveData, 0, 0);
        immediateContext->VSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());
        immediateContext->PSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());

        const Material& material = materials.at(batchMesh.material);
        const int textureIndices[]
        {
            material.data.pbrMetallicRoughness.basecolorTexture.index,
            material.data.pbrMetallicRoughness.metallicRoughnessTexture.index,
            material.data.normalTexture.index,
            material.data.emissiveTexture.index,
            material.data.occlusionTexture.index,
        };
        ID3D11ShaderResourceView* nullShaderResourceView{};
        std::vector<ID3D11ShaderResourceView*> shaderResourceViews(_countof(textureIndices));
        for (int textureIndex = 0; textureIndex < shaderResourceViews.size(); ++textureIndex)
        {
            shaderResourceViews.at(textureIndex) = textureIndices[textureIndex] > -1 ? textureResourceViews.at(textures.at(textureIndices[textureIndex]).source).Get() : nullShaderResourceView;
        }
        immediateContext->PSSetShaderResources(1, static_cast<UINT>(shaderResourceViews.size()), shaderResourceViews.data());

        if (batchMesh.indexBufferView.buffer > -1)
        {
            immediateContext->IASetIndexBuffer(buffers.at(batchMesh.indexBufferView.buffer).Get(), batchMesh.indexBufferView.format, 0);
            immediateContext->DrawIndexedInstanced(batchMesh.indexBufferView.sizeInBytes / GetFormatByteSize(batchMesh.indexBufferView.format), 4, 0, 0, 0);
        }
        else
        {
            immediateContext->DrawIndexedInstanced(batchMesh.vertexBufferView.sizeInBytes / batchMesh.vertexBufferView.strideInBytes, 4, 0, 0, 0);
        }
    }
    immediateContext->VSSetShader(NULL, NULL, 0);
    immediateContext->GSSetShader(NULL, NULL, 0);
    immediateContext->PSSetShader(NULL, NULL, 0);

}

void InterleavedGltfModel::CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world, const std::vector<Node>& animatedNodes)
{
    if (mode == ModelTypes::ModelMode::InstancedStaticMesh)
    {
        return;
    }
    if (mode == ModelTypes::ModelMode::StaticMesh)
    {
        return CastShadowBatch(immediateContext, world);
    }

    const std::vector<Node>& nodes{ animatedNodes.size() > 0 ? animatedNodes : InterleavedGltfModel::nodes };
    immediateContext->PSSetShaderResources(0, 1, materialResourceView.GetAddressOf());
    // CASCADED_SHADOW_MAPS

    immediateContext->VSSetShader(vertexShaderCSM.Get(), nullptr, 0);
    immediateContext->GSSetShader(geometryShaderCSM.Get(), nullptr, 0);

    Microsoft::WRL::ComPtr <ID3D11PixelShader> nullPixelShader{ NULL };
    immediateContext->PSSetShader(nullPixelShader.Get()/*SHADOW*/, nullptr, 0);


    immediateContext->IASetInputLayout(inputLayout.Get());
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    std::function<void(int)> traverse = [&](int nodeIndex)->void {
        const Node& node = nodes.at(nodeIndex);
        if (node.skin > -1)
        {
            const Skin& skin = skins.at(node.skin);
            _ASSERT_EXPR(skin.joints.size() <= PRIMITIVE_MAX_JOINTS, L"The size of the joint array is insufficient, please expand it.");
            PrimitiveJointConstants primitiveJointData{};
            for (size_t jointIndex = 0; jointIndex < skin.joints.size(); ++jointIndex)
            {
                DirectX::XMStoreFloat4x4(&primitiveJointData.matrices[jointIndex],
                    DirectX::XMLoadFloat4x4(&skin.inverseBindMatrices.at(jointIndex)) *
                    DirectX::XMLoadFloat4x4(&nodes.at(skin.joints.at(jointIndex)).globalTransform) *
                    DirectX::XMMatrixInverse(NULL, DirectX::XMLoadFloat4x4(&node.globalTransform))
                );
            }
            immediateContext->UpdateSubresource(primitiveJointCbuffer.Get(), 0, 0, &primitiveJointData, 0, 0);
            immediateContext->VSSetConstantBuffers(2, 1, primitiveJointCbuffer.GetAddressOf());
        }
        if (node.mesh > -1)
        {
            const Mesh& mesh = meshes.at(node.mesh);
            for (const Mesh::Primitive& primitive : mesh.primitives)
            {
                // INTERLEAVED_GLTF_MODEL
                UINT stride = sizeof(Mesh::Vertex);
                UINT offset = 0;
                immediateContext->IASetVertexBuffers(0, 1, buffers.at(primitive.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);

                PrimitiveConstants primitiveData = {};
                primitiveData.material = primitive.material;
                primitiveData.hasTangent = primitive.has("TANGENT");
                primitiveData.skin = node.skin;
                DirectX::XMStoreFloat4x4(&primitiveData.world, DirectX::XMLoadFloat4x4(&node.globalTransform) * DirectX::XMLoadFloat4x4(&world));
                immediateContext->UpdateSubresource(primitiveCBuffer.Get(), 0, 0, &primitiveData, 0, 0);
                immediateContext->VSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());
                immediateContext->PSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());

                // ここでモデル座標系を変換する？
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
                immediateContext->UpdateSubresource(primitiveCBuffer.Get(), 0, 0, &primitiveData, 0, 0);
                immediateContext->VSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());
                immediateContext->PSSetConstantBuffers(0, 1, primitiveCBuffer.GetAddressOf());

                //int materialIndex = primitive.GetCurrentMaterialIndex();

                //const Material& material = materials[materialIndex];
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
                    // INTERLEAVED_GLTF_MODEL
                    immediateContext->IASetIndexBuffer(buffers.at(primitive.indexBufferView.buffer).Get(), primitive.indexBufferView.format, 0);
                    immediateContext->DrawIndexedInstanced(primitive.indexBufferView.sizeInBytes / GetFormatByteSize(primitive.indexBufferView.format), 4, 0, 0, 0);
                }
                else
                {
                    // INTERLEAVED_GLTF_MODEL
                    //immediateContext->Draw(primitive.vertexBufferView.sizeInBytes / primitive.vertexBufferView.strideInBytes, 0);
                    immediateContext->DrawIndexedInstanced(primitive.vertexBufferView.sizeInBytes / primitive.vertexBufferView.strideInBytes, 4, 0, 0, 0);
                }
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

    immediateContext->VSSetShader(NULL, NULL, 0);
    immediateContext->GSSetShader(NULL, NULL, 0);
    immediateContext->PSSetShader(NULL, NULL, 0);
}

////モデルのジョイントのpositionを返す関数
//DirectX::XMFLOAT3 GltfModel::JointPosition(size_t nodeIndex, const std::vector<Node>& animatedNodes, const DirectX::XMFLOAT4X4& transform)
//{
//    DirectX::XMFLOAT3 position = { 0,0,0 };
//    const Node& node = animatedNodes.at(nodeIndex);
//    DirectX::XMMATRIX M = DirectX::XMLoadFloat4x4(&node.globalTransform) * DirectX::XMLoadFloat4x4(&transform);
//    DirectX::XMStoreFloat3(&position, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&position), M));
//    return position;
//}


// モデルのジョイントのワールド空間の position を返す関数
DirectX::XMFLOAT3 InterleavedGltfModel::GetJointWorldPosition(/*size_t nodeIndex,*/const std::string& name, const std::vector<Node>& animatedNodes, const DirectX::XMFLOAT4X4& transform)
{
    // キャッシュにあればそれを返す
    //auto nameToNodeWorldPosition = nameToNodeWorldPosition_.find(name);
    //if (nameToNodeWorldPosition != nameToNodeWorldPosition_.end())
    //{
    //    return nameToNodeWorldPosition->second;
    //}

    // 該当するノードを探す
    for (auto needNode : animatedNodes)
    {
        if (needNode.name == name)
        {
            DirectX::XMFLOAT3 position = { 0,0,0 };
            const Node& node = needNode;
            DirectX::XMMATRIX M = DirectX::XMLoadFloat4x4(&node.globalTransform) * DirectX::XMLoadFloat4x4(&transform);
            DirectX::XMStoreFloat3(&position, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&position), M));
            //nameToNodeWorldPosition_.emplace(name, position);
            return position;
        }
    }

    // もしなければ
    _ASSERT("Node's name is mistake or here is not your want nodes!!");

    return { 0.0f,0.0f,0.0f };
}

// モデルのジョイントの matrix を返す関数
DirectX::XMFLOAT4X4 InterleavedGltfModel::GetJointMatrix(
    const std::string& name,
    const std::vector<Node>& animatedNodes, const DirectX::XMFLOAT4X4& modelTransform)
{
    using namespace DirectX;

    for (const auto& node : animatedNodes)
    {
        if (node.name == name)
        {
            XMMATRIX bone = XMLoadFloat4x4(&node.globalTransform);
            XMMATRIX model = XMLoadFloat4x4(&modelTransform);

            XMMATRIX world = bone * model;
            //XMMATRIX world = bone;

            XMFLOAT4X4 result;
            XMStoreFloat4x4(&result, world);

            return result;
        }
    }

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    return identity;
}
// モデルのジョイントの matrix を返す関数
DirectX::XMFLOAT4X4 InterleavedGltfModel::GetJointMatrix(
    size_t nodeIndex,
    const std::vector<Node>& animatedNodes, const DirectX::XMFLOAT4X4& modelTransform)
{
    using namespace DirectX;
    const auto& node = animatedNodes[nodeIndex];
    {
        XMMATRIX bone = XMLoadFloat4x4(&node.globalTransform);
        XMMATRIX model = XMLoadFloat4x4(&modelTransform);

        XMMATRIX world = bone * model;
        //XMMATRIX world = bone;

        XMFLOAT4X4 result;
        XMStoreFloat4x4(&result, world);

        return result;
    }

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    return identity;
}
// モデルのジョイントのローカル空間の position を返す関数
DirectX::XMFLOAT3 InterleavedGltfModel::GetJointLocalPosition(/*size_t nodeIndex,*/const std::string& name, const std::vector<Node>& animatedNodes)
{
    for (const auto& node : animatedNodes)
    {
        if (node.name == name)
        {
            DirectX::XMFLOAT3 origin = { 0, 0, 0 };
            DirectX::XMMATRIX globalM = DirectX::XMLoadFloat4x4(&node.globalTransform);
            DirectX::XMVECTOR posVec = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&origin), globalM);

            DirectX::XMFLOAT3 position;
            DirectX::XMStoreFloat3(&position, posVec);
            return position;
        }
    }
    // もしなければ
    _ASSERT("Node's name is mistake or here is not your want nodes!!");

    return { 0.0f,0.0f,0.0f };
}

// モデルのジョイントのローカル空間の matrix を返す関数
DirectX::XMFLOAT4X4 InterleavedGltfModel::GetJointLocalMatrix(
    const std::string& name,
    const std::vector<Node>& animatedNodes)
{
    using namespace DirectX;

    for (const auto& node : animatedNodes)
    {
        if (node.name == name)
        {
            //return node.globalTransform;
            XMMATRIX T = XMMatrixTranslation(
                node.translation.x,
                node.translation.y,
                node.translation.z);

            XMVECTOR q = XMLoadFloat4(&node.rotation);
            XMMATRIX R = XMMatrixRotationQuaternion(q);

            XMMATRIX S = XMMatrixScaling(
                node.scale.x,
                node.scale.y,
                node.scale.z);

            XMMATRIX M = S * R * T;

            XMFLOAT4X4 result;
            XMStoreFloat4x4(&result, M);

            return result;
        }
    }

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    return identity;
}


// モデルのジョイントのローカル空間の matrix を返す関数
DirectX::XMFLOAT4X4 InterleavedGltfModel::GetJointLocalMatrix(
    size_t nodeIndex,
    const std::vector<Node>& animatedNodes)
{
    using namespace DirectX;
    const auto& node = animatedNodes[nodeIndex];
    {
        XMMATRIX T = XMMatrixTranslation(
            node.translation.x,
            node.translation.y,
            node.translation.z);

        XMVECTOR q = XMLoadFloat4(&node.rotation);
        XMMATRIX R = XMMatrixRotationQuaternion(q);

        XMMATRIX S = XMMatrixScaling(
            node.scale.x,
            node.scale.y,
            node.scale.z);

        XMMATRIX M = T * R * S;

        XMFLOAT4X4 result;
        XMStoreFloat4x4(&result, M);

        return result;
    }

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    return identity;
}

int InterleavedGltfModel::FindNodeIndexByName(const std::string& name) const
{
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        if (nodes[i].name == name)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// アニメーションを追加する関数
void InterleavedGltfModel::AddAnimations(const std::vector<std::string>& filenames)
{
    //animations.clear();
    for (std::vector<std::string>::const_reference filename : filenames)
    {
        AddAnimation(filename);
    }
}

void InterleavedGltfModel::AddAnimation(const std::string& filename)
{
    std::filesystem::path cerealFilename(filename);
    cerealFilename.replace_extension("animationCereal");
    //if (std::filesystem::exists(cerealFilename.c_str()))
    //{
    //    std::ifstream ifs(cerealFilename.c_str(), std::ios::binary);
    //    cereal::BinaryInputArchive deserialization(ifs);
    //    std::vector<Animation> cerealAnimations;
    //    //　読み込み時
    //    deserialization(cereal::make_nvp("animations", cerealAnimations));
    //    Logger::Log(Logger::LogCategory::System,"animations Count: " + std::to_string(animations.size()));
    //    animations.insert(animations.end(), cerealAnimations.begin(), cerealAnimations.end());
    //}
    //else
    {
        tinygltf::TinyGLTF tinyGltf;
        tinyGltf.SetImageLoader(NullLoadImage, nullptr);

        tinygltf::Model gltfModel;
        std::string error, warning;
        bool succeeded = false;
        if (filename.find(".glb") != std::string::npos)
        {
            succeeded = tinyGltf.LoadBinaryFromFile(&gltfModel, &error, &warning, filename.c_str());
        }
        else if (filename.find(".gltf") != std::string::npos)
        {
            succeeded = tinyGltf.LoadASCIIFromFile(&gltfModel, &error, &warning, filename.c_str());
        }

        _ASSERT_EXPR_A(warning.empty(), warning.c_str());
        _ASSERT_EXPR_A(error.empty(), error.c_str());
        _ASSERT_EXPR_A(succeeded, L"Failed to load glTF file");

        std::vector<Animation> newAnimations;
        FetchAnimations(gltfModel, newAnimations);
        animations.insert(animations.end(), newAnimations.begin(), newAnimations.end());

        std::ofstream ofs(cerealFilename.c_str(), std::ios::binary);
        cereal::BinaryOutputArchive serialization(ofs);
        // 書き込み時
        serialization(cereal::make_nvp("animations", newAnimations));
    }
}

void InterleavedGltfModel::ExtractAnimations(const tinygltf::Model& transmission_model)
{
    for (std::vector<tinygltf::Skin>::const_reference transmission_skin : transmission_model.skins)
    {
        Skin& skin = skins.emplace_back();
        const tinygltf::Accessor& transmission_accessor = transmission_model.accessors.at(transmission_skin.inverseBindMatrices);
        const tinygltf::BufferView& transmission_buffer_view = transmission_model.bufferViews.at(transmission_accessor.bufferView);
        _ASSERT_EXPR(transmission_accessor.type == TINYGLTF_TYPE_MAT4, L"");

        skin.inverseBindMatrices.resize(transmission_accessor.count);
        std::memcpy(skin.inverseBindMatrices.data(), transmission_model.buffers.at(transmission_buffer_view.buffer).data.data() + transmission_buffer_view.byteOffset + transmission_accessor.byteOffset, transmission_accessor.count * sizeof(DirectX::XMFLOAT4X4));

        skin.joints = transmission_skin.joints;
    }

    for (std::vector<tinygltf::Animation>::const_reference transmission_animation : transmission_model.animations)
    {
        std::vector<Animation>::reference animation = animations.emplace_back();
        animation.name = transmission_animation.name;

        for (std::vector<tinygltf::AnimationSampler>::const_reference transmission_sampler : transmission_animation.samplers)
        {
            Animation::Sampler& sampler = animation.samplers.emplace_back();
            sampler.input = transmission_sampler.input;
            sampler.output = transmission_sampler.output;
            sampler.interpolation = transmission_sampler.interpolation;

            const tinygltf::Accessor& transmission_accessor = transmission_model.accessors.at(transmission_sampler.input);
            const tinygltf::BufferView& transmission_buffer_view = transmission_model.bufferViews.at(transmission_accessor.bufferView);
            _ASSERT_EXPR(transmission_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
            _ASSERT_EXPR(transmission_accessor.type == TINYGLTF_TYPE_SCALAR, L"");
            const std::pair<std::unordered_map<int, std::vector<FLOAT>>::iterator, bool>& keyframe_timestamps = animation.timelines.emplace(transmission_sampler.input, transmission_accessor.count);
            if (keyframe_timestamps.second)
            {
                std::memcpy(keyframe_timestamps.first->second.data(), transmission_model.buffers.at(transmission_buffer_view.buffer).data.data() + transmission_buffer_view.byteOffset + transmission_accessor.byteOffset, transmission_accessor.count * sizeof(FLOAT));
            }
        }
        for (std::vector<tinygltf::AnimationChannel>::const_reference transmission_channel : transmission_animation.channels)
        {
            Animation::Channel& channel = animation.channels.emplace_back();
            channel.sampler = transmission_channel.sampler;
            channel.targetNode = transmission_channel.target_node;
            channel.targetPath = transmission_channel.target_path;

            std::vector<tinygltf::AnimationSampler>::const_reference transmission_sampler = transmission_animation.samplers.at(transmission_channel.sampler);

            const tinygltf::Accessor& transmission_accessor = transmission_model.accessors.at(transmission_sampler.output);
            const tinygltf::BufferView& transmission_buffer_view = transmission_model.bufferViews.at(transmission_accessor.bufferView);
            if (transmission_channel.target_path == "scale")
            {
                _ASSERT_EXPR(transmission_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
                _ASSERT_EXPR(transmission_accessor.type == TINYGLTF_TYPE_VEC3, L"");
                const std::pair<std::unordered_map<int, std::vector<DirectX::XMFLOAT3>>::iterator, bool>& keyframe_scales = animation.scales.emplace(transmission_sampler.output, transmission_accessor.count);
                if (keyframe_scales.second)
                {
                    std::memcpy(keyframe_scales.first->second.data(), transmission_model.buffers.at(transmission_buffer_view.buffer).data.data() + transmission_buffer_view.byteOffset + transmission_accessor.byteOffset, transmission_accessor.count * sizeof(DirectX::XMFLOAT3));
                }
            }
            else if (transmission_channel.target_path == "rotation")
            {
                _ASSERT_EXPR(transmission_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
                _ASSERT_EXPR(transmission_accessor.type == TINYGLTF_TYPE_VEC4, L"");
                const std::pair<std::unordered_map<int, std::vector<DirectX::XMFLOAT4>>::iterator, bool>& keyframe_rotations = animation.rotations.emplace(transmission_sampler.output, transmission_accessor.count);
                if (keyframe_rotations.second)
                {
                    std::memcpy(keyframe_rotations.first->second.data(), transmission_model.buffers.at(transmission_buffer_view.buffer).data.data() + transmission_buffer_view.byteOffset + transmission_accessor.byteOffset, transmission_accessor.count * sizeof(DirectX::XMFLOAT4));
                }
            }
            else if (transmission_channel.target_path == "translation")
            {
                _ASSERT_EXPR(transmission_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, L"");
                _ASSERT_EXPR(transmission_accessor.type == TINYGLTF_TYPE_VEC3, L"");
                const std::pair<std::unordered_map<int, std::vector<DirectX::XMFLOAT3>>::iterator, bool>& keyframe_translations = animation.translations.emplace(transmission_sampler.output, transmission_accessor.count);
                if (keyframe_translations.second)
                {
                    std::memcpy(keyframe_translations.first->second.data(), transmission_model.buffers.at(transmission_buffer_view.buffer).data.data() + transmission_buffer_view.byteOffset + transmission_accessor.byteOffset, transmission_accessor.count * sizeof(DirectX::XMFLOAT3));
                }
            }
            else if (transmission_channel.target_path == "weights")
            {
                //_ASSERT_EXPR(FALSE, L"Weighted animations have not yet been implemented.");
                OutputDebugStringA("Weighted animations have not yet been implemented.\n");

            }
            else
            {
                _ASSERT_EXPR(FALSE, L"");
            }
        }

        if (converted_handedness)
        {
            for (auto& translations : animation.translations | std::views::values)
            {
                for (auto& translation : translations)
                {
                    translation.x *= -1;
                }
            }
            for (auto& rotations : animation.rotations | std::views::values)
            {
                for (auto& rotation : rotations)
                {
                    rotation.y *= -1;
                    rotation.z *= -1;
                }
            }
        }
    }

    for (Animation& animation : animations)
    {
        // 各チャンネルのタイムスタンプから、最も長いアニメーションの再生時間を特定する。
        for (auto& val : animation.timelines | std::views::values)
        {
            animation.duration = std::max<float>(animation.duration, val.back());
        }
    }
}

void InterleavedGltfModel::UpdateInstanceBuffer(ID3D11DeviceContext* immediateContext, const std::vector<DirectX::XMFLOAT4X4>& matrices)
{
    if (matrices.empty())
    {
        instanceCount_ = 0;
        return;
    }

    HRESULT hr = S_OK;

    D3D11_MAPPED_SUBRESOURCE mapped = {};

    hr = immediateContext->Map(
        instanceBuffer.Get(),
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mapped);

    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    memcpy(
        mapped.pData,
        matrices.data(),
        sizeof(DirectX::XMFLOAT4X4) * matrices.size());

    immediateContext->Unmap(instanceBuffer.Get(), 0);

    instanceCount_ = static_cast<int>(matrices.size());
}


void InterleavedGltfModel::AppendAnimations(const std::vector<std::string>& filenames)
{
    for (std::vector<std::string>::const_reference filename : filenames)
    {
        tinygltf::TinyGLTF tiny_gltf;
#if 1
        tiny_gltf.SetImageLoader(NullLoadImage, nullptr);
#endif

        tinygltf::Model transmission_model;
        std::string error, warning;
        bool succeeded = false;
        if (filename.find(".glb") != std::string::npos)
        {
            succeeded = tiny_gltf.LoadBinaryFromFile(&transmission_model, &error, &warning, filename.c_str());
        }
        else if (filename.find(".gltf") != std::string::npos)
        {
            succeeded = tiny_gltf.LoadASCIIFromFile(&transmission_model, &error, &warning, filename.c_str());
        }
        if (!warning.empty())
        {
            OutputDebugStringA(warning.c_str());
        }
        _ASSERT_EXPR(error.empty() && succeeded, L"Failed to load glTF file");

        ExtractAnimations(transmission_model);
    }
}

void InterleavedGltfModel::ComputeAABBFromMesh(const InterleavedGltfModel::Node& node, const InterleavedGltfModel& model, DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax)
{
    using namespace DirectX;

    // 初期化：逆方向に最大・最小を設定
    XMVECTOR minVec = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.0f);
    XMVECTOR maxVec = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);

    if (node.mesh < 0 || node.mesh >= model.meshes.size()) {
        outMin = XMFLOAT3(0, 0, 0);
        outMax = XMFLOAT3(0, 0, 0);
        return;
    }

    const auto& mesh = model.meshes[node.mesh];
    for (const auto& primitive : mesh.primitives)
    {
        for (const auto& vertex : primitive.cachedVertices)
        {
            XMVECTOR pos = XMLoadFloat3(&vertex.position);
            minVec = XMVectorMin(minVec, pos);
            maxVec = XMVectorMax(maxVec, pos);
        }
    }

    DirectX::XMStoreFloat3(&outMin, minVec);
    DirectX::XMStoreFloat3(&outMax, maxVec);
}

// 座標系を変換する
void InterleavedGltfModel::ConvertHandedness()
{
    for (auto& node : nodes)
    {
        node.translation.x *= -1;
        node.rotation.y *= -1;
        node.rotation.z *= -1;
    }
    CumulateTransforms(nodes);

    for (auto& mesh : meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
            for (auto& vertex : primitive.cachedVertices)
            {
                vertex.position.x *= -1;
                vertex.normal.x *= -1;
                vertex.tangent.x *= -1;

            }
        }
    }

    for (auto& batch_mesh : batchMeshes)
    {
        for (auto& cached_vertex : batch_mesh.cachedVertices)
        {
            cached_vertex.position.x *= -1;
            cached_vertex.normal.x *= -1;
            cached_vertex.tangent.x *= -1;
        }
    }

    for (auto& skin : skins)
    {
        for (auto& inverse_bind_matrix : skin.inverseBindMatrices)
        {
            inverse_bind_matrix._12 *= -1;
            inverse_bind_matrix._13 *= -1;
            inverse_bind_matrix._14 *= -1;

            inverse_bind_matrix._21 *= -1;
            inverse_bind_matrix._31 *= -1;
            inverse_bind_matrix._41 *= -1;
        }
    }
}


// 座標系変換
void InterleavedGltfModel::ConvertPositionAxisSystem(DirectX::XMFLOAT3& v)
{
    v.x = -v.x;
}
// 座標系変換
void InterleavedGltfModel::ConvertPositionAxisSystem(DirectX::XMFLOAT4& v)
{
    v.x = -v.x;
}


void InterleavedGltfModel::ConvertRotationAxisSystem(DirectX::XMFLOAT4& q)
{
    q.x = -q.x;
    q.w = -q.w;
}

void InterleavedGltfModel::ConvertMatrixAxisSystem(DirectX::XMFLOAT4X4& m)
{
    m._12 = -m._12;
    m._13 = -m._13;
    m._21 = -m._21;
    m._31 = -m._31;
    m._41 = -m._41;
}

void InterleavedGltfModel::ConvertNodeAxisSystem(Node& node, ModelTypes::ModelMode mode_)
{
    if (mode_ == ModelTypes::ModelMode::SkeletalMesh)
    {
        ConvertPositionAxisSystem(node.translation);
        ConvertRotationAxisSystem(node.rotation);
    }
}

void InterleavedGltfModel::ConvertMeshAxisSystem(Mesh& mesh)
{
    for (Mesh::Primitive& primitive : mesh.primitives)
    {
        for (Mesh::Vertex& v : primitive.cachedVertices)
        {
            ConvertPositionAxisSystem(v.position);
            ConvertPositionAxisSystem(v.normal);
            ConvertPositionAxisSystem(v.tangent);
        }

#if 0
        for (size_t i = 0; i < primitive.cachedIndices.size(); i += 3)
        {
            uint32_t* p = &primitive.cachedIndices.at(i);
            uint32_t temp = p[1];
            p[1] = p[2];
            p[2] = temp;
        }
#else
        //uint32_t* indices = reinterpret_cast<uint32_t*>(primitive.cachedIndices.data());

        //size_t indexCount = primitive.cachedIndices.size() / 4;

        //for (size_t i = 0; i < indexCount; i += 3)
        //{
        //    std::swap(indices[i + 1], indices[i + 2]);
        //}

#endif // 0

    }
}

void InterleavedGltfModel::ConvertBatchMeshAxisSystem(BatchMesh& mesh)
{
    for (BatchMesh::Vertex& v : mesh.cachedVertices)
    {
        ConvertPositionAxisSystem(v.position);
        ConvertPositionAxisSystem(v.normal);
        ConvertPositionAxisSystem(v.tangent);

    }

    uint32_t* indices = reinterpret_cast<uint32_t*>(mesh.cachedIndices.data());

    size_t indexCount = mesh.cachedIndices.size() / 4;

    for (size_t i = 0; i < indexCount; i += 3)
    {
        std::swap(indices[i + 1], indices[i + 2]);
    }
}

void InterleavedGltfModel::ConvertSkinAxisSystem(Skin& skin)
{
    for (auto& m : skin.inverseBindMatrices)
    {
        ConvertMatrixAxisSystem(m);
    }
}

void InterleavedGltfModel::ConvertAnimationAxisSystem(Animation& animation)
{
    // translation
    for (auto& [key, vec] : animation.translations)
    {
        for (auto& v : vec)
        {
            ConvertPositionAxisSystem(v);
        }
    }

    // rotation
    for (auto& [key, vec] : animation.rotations)
    {
        for (auto& q : vec)
        {
            ConvertRotationAxisSystem(q);
        }
    }
}