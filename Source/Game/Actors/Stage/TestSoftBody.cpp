// INTERLEAVED_GLTF_MODEL
#include "pch.h"
#include "TestSoftBody.h"
#include <functional>
#include <filesystem>
#include <fstream>

#include "tiny_gltf.h"
#include "Engine/Debug/Assert.h"

#include "Graphics/Core/Shader.h"
#include "Engine/Utility/Win32Utils.h"
#include "Graphics/Resource/Texture.h"
#include "Graphics/Core/GltfDxgiHelper.h"


soft_body::soft_body(ID3D11Device* device, const std::string& filename) : filename(filename)
{
#ifdef USE_CEREAL 
    std::filesystem::path cereal_filename(filename);
    cereal_filename.replace_extension("cereal");
    if (std::filesystem::exists(cereal_filename.c_str()))
    {
        std::ifstream ifs(cereal_filename.c_str(), std::ios::binary);
        cereal::BinaryInputArchive deserialization(ifs);
        deserialization(
            cereal::make_nvp("scenes", scenes),
            cereal::make_nvp("default_scene", default_scene),
            cereal::make_nvp("nodes", nodes),
            cereal::make_nvp("materials", materials)
        );
        deserialization(cereal::make_nvp("meshes", meshes));
        deserialization(cereal::make_nvp("textures", textures), cereal::make_nvp("images", images));
    }
    else
#endif
    {
        tinygltf::TinyGLTF tiny_gltf;
        tiny_gltf.SetImageLoader(NullLoadImage, nullptr);

        tinygltf::Model gltf_model;
        std::string error, warning;
        bool succeeded = false;
        if (filename.find(".glb") != std::string::npos)
        {
            succeeded = tiny_gltf.LoadBinaryFromFile(&gltf_model, &error, &warning, filename.c_str());
        }
        else if (filename.find(".gltf") != std::string::npos)
        {
            succeeded = tiny_gltf.LoadASCIIFromFile(&gltf_model, &error, &warning, filename.c_str());
        }

        _ASSERT_EXPR_A(warning.empty(), warning.c_str());
        _ASSERT_EXPR_A(error.empty(), error.c_str());
        _ASSERT_EXPR_A(succeeded, L"Failed to load glTF file");

        for (const tinygltf::Scene& gltf_scene : gltf_model.scenes)
        {
            scene& scene = scenes.emplace_back();
            scene.name = gltf_scene.name;
            scene.nodes = gltf_scene.nodes;
        }
        default_scene = gltf_model.defaultScene < 0 ? 0 : gltf_model.defaultScene;

        fetch_nodes(gltf_model);
        fetch_materials(device, gltf_model);
        fetch_textures(device, gltf_model);

        fetch_meshes(device, gltf_model);

#ifdef USE_CEREAL 
        std::ofstream ofs(cereal_filename.c_str(), std::ios::binary);
        cereal::BinaryOutputArchive serialization(ofs);
        serialization(
            cereal::make_nvp("scenes", scenes),
            cereal::make_nvp("default_scene", default_scene),
            cereal::make_nvp("nodes", nodes),
            cereal::make_nvp("materials", materials)
        );
        serialization(cereal::make_nvp("meshes", meshes));
        serialization(cereal::make_nvp("textures", textures), cereal::make_nvp("images", images));
#endif
    }
    create_and_upload_resources(device);
}
void soft_body::fetch_nodes(const tinygltf::Model& gltf_model)
{
    for (const tinygltf::Node& gltf_node : gltf_model.nodes)
    {
        node& node = nodes.emplace_back();
        node.name = gltf_node.name;
        node.skin = gltf_node.skin;
        node.mesh = gltf_node.mesh;
        node.children = gltf_node.children;
        if (!gltf_node.matrix.empty())
        {
            DirectX::XMFLOAT4X4 matrix;
            for (size_t row = 0; row < 4; row++)
            {
                for (size_t column = 0; column < 4; column++)
                {
                    matrix(row, column) = static_cast<float>(gltf_node.matrix.at(4 * row + column));
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
            if (gltf_node.scale.size() > 0)
            {
                node.scale.x = static_cast<float>(gltf_node.scale.at(0));
                node.scale.y = static_cast<float>(gltf_node.scale.at(1));
                node.scale.z = static_cast<float>(gltf_node.scale.at(2));
            }
            if (gltf_node.translation.size() > 0)
            {
                node.translation.x = static_cast<float>(gltf_node.translation.at(0));
                node.translation.y = static_cast<float>(gltf_node.translation.at(1));
                node.translation.z = static_cast<float>(gltf_node.translation.at(2));
            }
            if (gltf_node.rotation.size() > 0)
            {
                node.rotation.x = static_cast<float>(gltf_node.rotation.at(0));
                node.rotation.y = static_cast<float>(gltf_node.rotation.at(1));
                node.rotation.z = static_cast<float>(gltf_node.rotation.at(2));
                node.rotation.w = static_cast<float>(gltf_node.rotation.at(3));
            }
        }
    }
    cumulate_transforms(nodes);
}
void soft_body::cumulate_transforms(std::vector<node>& nodes)
{
    std::function<void(int, int)> traverse = [&](int parent_index, int node_index)->void
        {
            DirectX::XMMATRIX P = parent_index > -1 ? DirectX::XMLoadFloat4x4(&nodes.at(parent_index).global_transform) : DirectX::XMMatrixIdentity();

            node& node = nodes.at(node_index);
            DirectX::XMMATRIX S = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
            DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
            DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z);
            DirectX::XMStoreFloat4x4(&node.global_transform, S * R * T * P);

            for (int child_index : node.children)
            {
                traverse(node_index, child_index);
            }
        };
    for (int node_index : scenes.at(default_scene).nodes)
    {
        traverse(-1, node_index);
    }
}

UINT _sizeof_component(DXGI_FORMAT format)
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

// INTERLEAVED_GLTF_MODEL
void soft_body::fetch_meshes(ID3D11Device* device, const tinygltf::Model& gltf_model)
{
    for (const tinygltf::Mesh& gltf_mesh : gltf_model.meshes)
    {
        mesh& mesh = meshes.emplace_back();
        mesh.name = gltf_mesh.name;
        for (const tinygltf::Primitive& gltf_primitive : gltf_mesh.primitives)
        {
            mesh::primitive& primitive = mesh.primitives.emplace_back();
            primitive.material = gltf_primitive.material;

            // Create index buffer view
            if (gltf_primitive.indices > -1)
            {
                const tinygltf::Accessor& gltf_accessor = gltf_model.accessors.at(gltf_primitive.indices);
                const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_accessor.bufferView);

                // SOFT_BODY
                // The index buffer component format is fixed to DXGI_FORMAT_R32_UINT.
                primitive.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
                primitive.index_buffer_view.size_in_bytes = static_cast<std::uint32_t>(gltf_accessor.count) * 4/*sizeof DXGI_FORMAT_R32_UINT*/;
                primitive.cached_indices.resize(gltf_accessor.count);
                if (gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                {
                    const std::uint8_t* data = gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
                    for (size_t accessor_index = 0; accessor_index < gltf_accessor.count; ++accessor_index)
                    {
                        primitive.cached_indices.at(accessor_index) = static_cast<std::uint32_t>(data[accessor_index]);
                    }
                }
                if (gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    const std::uint16_t* data = reinterpret_cast<const std::uint16_t*>(gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset);
                    for (size_t accessor_index = 0; accessor_index < gltf_accessor.count; ++accessor_index)
                    {
                        primitive.cached_indices.at(accessor_index) = static_cast<std::uint32_t>(data[accessor_index]);
                    }
                }
                if (gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    const std::uint32_t* data = reinterpret_cast<const std::uint32_t*>(gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset);
                    for (size_t accessor_index = 0; accessor_index < gltf_accessor.count; ++accessor_index)
                    {
                        primitive.cached_indices.at(accessor_index) = data[accessor_index];
                    }
                }
                else
                {

                }
            }

            // Create vertex buffer view
            if (gltf_primitive.attributes.size() > 0 && gltf_primitive.attributes.find("POSITION") != gltf_primitive.attributes.end())
            {
                primitive.cached_vertices.resize(gltf_model.accessors.at(gltf_primitive.attributes.at("POSITION")).count);
            }
            else
            {
                continue;
            }
            for (std::map<std::string, int>::const_reference gltf_attribute : gltf_primitive.attributes)
            {
                const tinygltf::Accessor& gltf_accessor = gltf_model.accessors.at(gltf_attribute.second);
                const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews.at(gltf_accessor.bufferView);

                const unsigned char* s_data = gltf_model.buffers.at(gltf_buffer_view.buffer).data.data() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
                const size_t s_stride = gltf_accessor.ByteStride(gltf_buffer_view);
                const size_t d_stride = sizeof(vertex);
                if (gltf_attribute.first == "POSITION")
                {
                    const size_t count = gltf_accessor.count;
                    _ASSERT_EXPR(count == primitive.cached_vertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    unsigned char* d_data = reinterpret_cast<unsigned char*>(&primitive.cached_vertices.data()->position);
                    CopyStride<DirectX::XMFLOAT3>(d_data, d_stride, s_data, s_stride, count);
                }
                else if (gltf_attribute.first == "NORMAL")
                {
                    const size_t count = gltf_accessor.count;
                    _ASSERT_EXPR(count == primitive.cached_vertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    unsigned char* d_data = reinterpret_cast<unsigned char*>(&primitive.cached_vertices.data()->normal);
                    CopyStride<DirectX::XMFLOAT3>(d_data, d_stride, s_data, s_stride, count);
                }
                else if (gltf_attribute.first == "TANGENT")
                {
                    const size_t count = gltf_accessor.count;
                    _ASSERT_EXPR(count == primitive.cached_vertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    unsigned char* d_data = reinterpret_cast<unsigned char*>(&primitive.cached_vertices.data()->tangent);
                    CopyStride<DirectX::XMFLOAT4>(d_data, d_stride, s_data, s_stride, count);
                }
                else if (gltf_attribute.first == "TEXCOORD_0")
                {
                    const size_t count = gltf_accessor.count;
                    _ASSERT_EXPR(count == primitive.cached_vertices.size(), L"The number of components on all vertices comprising the mesh must be the same.");

                    unsigned char* d_data = reinterpret_cast<unsigned char*>(&primitive.cached_vertices.data()->texcoord);
                    CopyStride<DirectX::XMFLOAT2>(d_data, d_stride, s_data, s_stride, count);
                }
                else
                {
                    //_ASSERT_EXPR(FALSE, L"This attribute is unsupported.");
                }
                primitive.attributes.emplace(gltf_attribute.first, ToDxgiFormat(gltf_accessor));
            }
            primitive.vertex_buffer_view.size_in_bytes = static_cast<UINT>(primitive.cached_vertices.size() * sizeof(vertex));
            primitive.vertex_buffer_view.stride_in_bytes = static_cast<UINT>(sizeof(vertex));

            // SOFT_BODY
            vertex* vertices = primitive.cached_vertices.data();
            size_t vertex_count = primitive.cached_vertices.size();
            std::uint32_t* indices = primitive.cached_indices.data();
            size_t index_count = primitive.cached_indices.size();

            std::vector<std::vector<std::uint32_t>> edges(vertex_count);
            for (int i = 0; i < index_count; i += 3)
            {
                auto i0 = indices[i + 0];
                auto i1 = indices[i + 1];
                auto i2 = indices[i + 2];

                edges[i0].push_back(i1);
                edges[i0].push_back(i2);
                edges[i1].push_back(i0);
                edges[i1].push_back(i2);
                edges[i2].push_back(i0);
                edges[i2].push_back(i1);
            }

#if 1
            /*
                In this code, component represents a connected component of the mesh E
                that is, a group of vertices that are connected to each other through shared triangles.
            */
            std::vector<int> component(vertex_count, -1);
            int component_count = 0;
            std::vector<int> component_vertices;

            for (int i = 0; i < vertex_count; ++i)
            {
                if (component[i] != -1) continue;

                component[i] = component_count++;
                component_vertices.push_back(1);

                std::deque<int> queue;
                queue.push_back(i);

                while (!queue.empty())
                {
                    auto v = queue.front();
                    queue.pop_front();

                    for (auto e : edges[v])
                    {
                        if (component[e] == -1)
                        {
                            component[e] = component[v];
                            queue.push_back(e);
                            component_vertices.back()++;
                        }
                    }
                }
            }

            std::vector<int> component_triangles(component_count, 0);
            std::vector<DirectX::XMFLOAT3> avg_component_normal(component_count, { 0, 0, 0 });
            for (int i = 0; i < index_count; i += 3)
            {
                auto i0 = indices[i + 0];
                auto i1 = indices[i + 1];
                auto i2 = indices[i + 2];

                auto c = component[i0];

                component_triangles[c] += 1;

                auto p0 = vertices[i0].position;
                auto p1 = vertices[i1].position;
                auto p2 = vertices[i2].position;

                //auto n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                //avg_component_normal[c] += n;
                auto n = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&p1), DirectX::XMLoadFloat3(&p0)), DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&p2), DirectX::XMLoadFloat3(&p0))));
                DirectX::XMStoreFloat3(&avg_component_normal[c], DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&avg_component_normal[c]), n));
            }

            for (int c = 0; c < component_count; ++c)
            {
                if (component_triangles.at(c) > 0)
                {
                    //avg_component_normal[c] /= (1.0f * component_triangles[c]);
                    DirectX::XMStoreFloat3(&avg_component_normal[c], DirectX::XMVectorDivide(DirectX::XMLoadFloat3(&avg_component_normal[c]), DirectX::XMVectorReplicate(1.0f * component_triangles[c])));
                }
            }
#endif



#if 0
            // Sponza-specific heuristic: remove cloth mesh connected components which are
            //    too small or face negative Z

            std::vector<bool> front_facing(vertex_count, false);
            for (int i = 0; i < vertex_count; ++i)
            {
                int c = component[i];
                //front_facing[i] = !(component_vertices[c] < 256 || avg_component_normal[c].z < 0.0f);
                front_facing[i] = true;
            }

            for (auto& vertex_edges : edges)
            {
                std::sort(vertex_edges.begin(), vertex_edges.end());
                auto end = std::unique(vertex_edges.begin(), vertex_edges.end());
                end = std::remove_if(vertex_edges.begin(), end, [&](auto i) { return !front_facing.at(i); });
                vertex_edges.erase(end, vertex_edges.end());
            }

            std::vector<bool> disconnected(edges.size(), false);
            for (int i = 0; i < edges.size(); ++i)
            {
                auto& vertex_edges = edges[i];
                if (vertex_edges.size() == 2 && edges[vertex_edges[0]].size() == 2 && edges[vertex_edges[1]].size() == 2)
                {
                    disconnected[i] = true;
                }
            }

            float top_y = -std::numeric_limits<float>::infinity();
#else
            for (auto& vertex_edges : edges)
            {
                // Sort the vector to bring duplicates together
                std::sort(vertex_edges.begin(), vertex_edges.end());

                // Remove consecutive duplicates (keeps only one occurrence of each value)
                vertex_edges.erase(std::unique(vertex_edges.begin(), vertex_edges.end()), vertex_edges.end());
            }
#endif




            for (int i = 0; i < edges.size(); ++i)
            {
                std::vector<std::uint32_t>& vertex_edges = edges.at(i);

#if 0
                bool is_top_vertex = true;
                for (auto e : vertex_edges)
                {
                    if (primitive.cached_vertices.at(i).position.y < primitive.cached_vertices.at(e).position.y)
                    {
                        is_top_vertex = false;
                        break;
                    }
                }

                if (disconnected[i] || !front_facing[i])
                {
                    vertex_edges.assign(CLOTH_EDGES_PER_VERTEX, std::uint32_t(-1));
                    vertices[i].position = { 0.0f, 0.0f, 0.0f };
                }
                else if (is_top_vertex)
                {
                    vertex_edges.assign(CLOTH_EDGES_PER_VERTEX, -1);
                    top_y = std::max(top_y, primitive.cached_vertices.at(i).position.y);
                }
                else
#endif					


                    if (vertex_edges.size() > CLOTH_EDGES_PER_VERTEX)
                    {
                        std::cout << "WARNING: " << vertex_edges.size() << " cloth edges is clamped to " << CLOTH_EDGES_PER_VERTEX << std::endl;
                        vertex_edges.resize(CLOTH_EDGES_PER_VERTEX);
                    }
                    else while (vertex_edges.size() < CLOTH_EDGES_PER_VERTEX)
                    {
                        vertex_edges.push_back(-1);
                    }

                for (auto e : vertex_edges)
                {
                    cloth_edge edge;
                    edge.delta = { 0, 0, 0, 0 };
                    edge.id = e;
                    if (e != std::uint32_t(-1))
                    {
                        auto delta = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&primitive.cached_vertices.at(e).position), DirectX::XMLoadFloat3(&primitive.cached_vertices.at(i).position));
                        DirectX::XMStoreFloat4(&edge.delta, delta);
                        edge.delta.w = DirectX::XMVectorGetX(DirectX::XMVector3Length(delta));
                    }
                    primitive.cloth_edges.push_back(edge);
                }
            }

            for (int i = 0; i < vertex_count; ++i)
            {
                primitive.cloth_vertices.push_back({
                    {0, 0, 0}, 0,
                    {0, 0, 0}, 0,
                    primitive.cached_vertices.at(i).position, 0
                    });
            }
        }

    }
}

void soft_body::fetch_materials(ID3D11Device* device, const tinygltf::Model& gltf_model)
{
    for (const tinygltf::Material& gltf_material : gltf_model.materials)
    {
        std::vector<material>::reference material = materials.emplace_back();

        material.name = gltf_material.name;

        material.data.emissive_factor[0] = static_cast<float>(gltf_material.emissiveFactor.at(0));
        material.data.emissive_factor[1] = static_cast<float>(gltf_material.emissiveFactor.at(1));
        material.data.emissive_factor[2] = static_cast<float>(gltf_material.emissiveFactor.at(2));

        material.data.alpha_mode = gltf_material.alphaMode == "OPAQUE" ? 0 : gltf_material.alphaMode == "MASK" ? 1 : gltf_material.alphaMode == "BLEND" ? 2 : 0;
        material.data.alpha_cutoff = static_cast<float>(gltf_material.alphaCutoff);
        material.data.double_sided = gltf_material.doubleSided ? 1 : 0;

        material.data.pbr_metallic_roughness.basecolor_factor[0] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(0));
        material.data.pbr_metallic_roughness.basecolor_factor[1] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(1));
        material.data.pbr_metallic_roughness.basecolor_factor[2] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(2));
        material.data.pbr_metallic_roughness.basecolor_factor[3] = static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(3));
        material.data.pbr_metallic_roughness.basecolor_texture.index = gltf_material.pbrMetallicRoughness.baseColorTexture.index;
        material.data.pbr_metallic_roughness.basecolor_texture.texcoord = gltf_material.pbrMetallicRoughness.baseColorTexture.texCoord;
        material.data.pbr_metallic_roughness.metallic_factor = static_cast<float>(gltf_material.pbrMetallicRoughness.metallicFactor);
        material.data.pbr_metallic_roughness.roughness_factor = static_cast<float>(gltf_material.pbrMetallicRoughness.roughnessFactor);
        material.data.pbr_metallic_roughness.metallic_roughness_texture.index = gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index;
        material.data.pbr_metallic_roughness.metallic_roughness_texture.texcoord = gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;

        material.data.normal_texture.index = gltf_material.normalTexture.index;
        material.data.normal_texture.texcoord = gltf_material.normalTexture.texCoord;
        material.data.normal_texture.scale = static_cast<float>(gltf_material.normalTexture.scale);

        material.data.occlusion_texture.index = gltf_material.occlusionTexture.index;
        material.data.occlusion_texture.texcoord = gltf_material.occlusionTexture.texCoord;
        material.data.occlusion_texture.strength = static_cast<float>(gltf_material.occlusionTexture.strength);

        material.data.emissive_texture.index = gltf_material.emissiveTexture.index;
        material.data.emissive_texture.texcoord = gltf_material.emissiveTexture.texCoord;
    }
}
void soft_body::fetch_textures(ID3D11Device* device, const tinygltf::Model& gltf_model)
{
    for (const tinygltf::Texture& gltf_texture : gltf_model.textures)
    {
        texture& texture = textures.emplace_back();
        texture.name = gltf_texture.name;
        texture.source = gltf_texture.source;
    }
    for (const tinygltf::Image& gltf_image : gltf_model.images)
    {
        image& image = images.emplace_back();
        image.name = gltf_image.name;
        image.width = gltf_image.width;
        image.height = gltf_image.height;
        image.component = gltf_image.component;
        image.bits = gltf_image.bits;
        image.pixel_type = gltf_image.pixel_type;
        image.mime_type = gltf_image.mimeType;
        image.uri = gltf_image.uri;
        image.as_is = gltf_image.as_is;

        if (gltf_image.bufferView > -1)
        {
            const tinygltf::BufferView& buffer_view = gltf_model.bufferViews.at(gltf_image.bufferView);
            const tinygltf::Buffer& buffer = gltf_model.buffers.at(buffer_view.buffer);
            const unsigned char* data = buffer.data.data() + buffer_view.byteOffset;
            image.cache_data.resize(buffer_view.byteLength);
            memcpy_s(image.cache_data.data(), image.cache_data.size(), data, buffer_view.byteLength);
        }
    }

}

void soft_body::create_and_upload_resources(ID3D11Device* device)
{
    HRESULT hr;

    // Create and upload vertex and index buffers on GPU
    for (mesh& mesh : meshes)
    {
        for (mesh::primitive& primitive : mesh.primitives)
        {
            if (primitive.index_buffer_view.size_in_bytes > 0)
            {
                primitive.index_buffer_view.buffer = static_cast<int>(buffers.size());

                D3D11_BUFFER_DESC buffer_desc = {};
                buffer_desc.ByteWidth = primitive.index_buffer_view.size_in_bytes;
                buffer_desc.Usage = D3D11_USAGE_DEFAULT;
                buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                buffer_desc.CPUAccessFlags = 0;
                buffer_desc.MiscFlags = 0;
                buffer_desc.StructureByteStride = 0;
                D3D11_SUBRESOURCE_DATA subresource_data = {};
                subresource_data.pSysMem = primitive.cached_indices.data();
                subresource_data.SysMemPitch = 0;
                subresource_data.SysMemSlicePitch = 0;
                hr = device->CreateBuffer(&buffer_desc, &subresource_data, buffers.emplace_back().GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                primitive.cached_indices.clear();
            }

            if (primitive.vertex_buffer_view.size_in_bytes > 0)
            {
                Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;

                primitive.vertex_buffer_view.buffer = static_cast<int>(buffers.size());

                D3D11_BUFFER_DESC buffer_desc = {};
                buffer_desc.ByteWidth = primitive.vertex_buffer_view.size_in_bytes;
                buffer_desc.Usage = D3D11_USAGE_DEFAULT;
                buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE; // SOFT_BODY
                buffer_desc.CPUAccessFlags = 0;
                buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS; // SOFT_BODY
                buffer_desc.StructureByteStride = 0;
                D3D11_SUBRESOURCE_DATA subresource_data = {};
                subresource_data.pSysMem = primitive.cached_vertices.data();
                subresource_data.SysMemPitch = 0;
                subresource_data.SysMemSlicePitch = 0;
                hr = device->CreateBuffer(&buffer_desc, &subresource_data, vertex_buffer.GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                buffers.push_back(vertex_buffer);
                primitive.cached_vertices.clear();

                // SOFT_BODY
                D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_view_desc = {};
                unordered_access_view_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                unordered_access_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
                unordered_access_view_desc.Buffer.FirstElement = 0;
                unordered_access_view_desc.Buffer.NumElements = primitive.vertex_buffer_view.size_in_bytes / 4;
                unordered_access_view_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
                hr = device->CreateUnorderedAccessView(vertex_buffer.Get(), &unordered_access_view_desc, primitive.vertex_uav.ReleaseAndGetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                // SOFT_BODY
                D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc = {};
                shader_resource_view_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
                shader_resource_view_desc.BufferEx.FirstElement = 0;
                shader_resource_view_desc.BufferEx.NumElements = primitive.vertex_buffer_view.size_in_bytes / 4;
                shader_resource_view_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
                hr = device->CreateShaderResourceView(vertex_buffer.Get(), &shader_resource_view_desc, primitive.vertex_srv.ReleaseAndGetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                {
                    Microsoft::WRL::ComPtr<ID3D11Buffer> cloth_edge_buffer;

                    D3D11_BUFFER_DESC buffer_desc = {};
                    buffer_desc.ByteWidth = static_cast<UINT>(primitive.cloth_edges.size() * sizeof(cloth_edge));
                    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
                    buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    buffer_desc.CPUAccessFlags = 0;
                    buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                    buffer_desc.StructureByteStride = sizeof(cloth_edge);

                    D3D11_SUBRESOURCE_DATA subresource_data = {};
                    subresource_data.pSysMem = primitive.cloth_edges.data();

                    hr = device->CreateBuffer(&buffer_desc, &subresource_data, cloth_edge_buffer.ReleaseAndGetAddressOf());
                    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                    D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc = {};
                    shader_resource_view_desc.Format = DXGI_FORMAT_UNKNOWN;
                    shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
                    shader_resource_view_desc.Buffer.FirstElement = 0;
                    shader_resource_view_desc.Buffer.NumElements = static_cast<UINT>(primitive.cloth_edges.size());

                    hr = device->CreateShaderResourceView(cloth_edge_buffer.Get(), &shader_resource_view_desc, primitive.cloth_edge_srv.ReleaseAndGetAddressOf());
                    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                    //buffers.push_back(cloth_edge_buffer);
                    primitive.cloth_edges.clear();

                }
                {
                    Microsoft::WRL::ComPtr<ID3D11Buffer> cloth_vertex_buffer;

                    D3D11_BUFFER_DESC buffer_desc = {};
                    buffer_desc.ByteWidth = static_cast<UINT>(primitive.cloth_vertices.size() * sizeof(cloth_vertex));
                    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
                    buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
                    buffer_desc.CPUAccessFlags = 0;
                    buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                    buffer_desc.StructureByteStride = sizeof(cloth_vertex);

                    D3D11_SUBRESOURCE_DATA subresource_data = {};
                    subresource_data.pSysMem = primitive.cloth_vertices.data();

                    hr = device->CreateBuffer(&buffer_desc, &subresource_data, cloth_vertex_buffer.ReleaseAndGetAddressOf());
                    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                    D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc = {};
                    shader_resource_view_desc.Format = DXGI_FORMAT_UNKNOWN;
                    shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
                    shader_resource_view_desc.Buffer.FirstElement = 0;
                    shader_resource_view_desc.Buffer.NumElements = static_cast<UINT>(primitive.cloth_vertices.size());
                    hr = device->CreateShaderResourceView(cloth_vertex_buffer.Get(), &shader_resource_view_desc, primitive.cloth_vertex_srv.ReleaseAndGetAddressOf());
                    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                    D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_view_desc = {};
                    unordered_access_view_desc.Format = DXGI_FORMAT_UNKNOWN;
                    unordered_access_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
                    unordered_access_view_desc.Buffer.FirstElement = 0;
                    unordered_access_view_desc.Buffer.NumElements = static_cast<UINT>(primitive.cloth_vertices.size());
                    hr = device->CreateUnorderedAccessView(cloth_vertex_buffer.Get(), &unordered_access_view_desc, primitive.cloth_vertex_uav.ReleaseAndGetAddressOf());
                    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                    //buffers.push_back(cloth_edge_buffer);
                    primitive.cloth_vertices.clear();

                }
            }
        }
    }

    // Create and upload materials on GPU
    std::vector<material::buffer> material_data;
    for (const material& material : materials)
    {
        material_data.emplace_back(material.data);
    }
    Microsoft::WRL::ComPtr<ID3D11Buffer> material_buffer;
    D3D11_BUFFER_DESC buffer_desc = {};
    buffer_desc.ByteWidth = static_cast<UINT>(sizeof(material::buffer) * material_data.size());
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    buffer_desc.StructureByteStride = sizeof(material::buffer);
    D3D11_SUBRESOURCE_DATA subresource_data = {};
    subresource_data.pSysMem = material_data.data();
    subresource_data.SysMemPitch = 0;
    subresource_data.SysMemSlicePitch = 0;
    hr = device->CreateBuffer(&buffer_desc, &subresource_data, material_buffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc = {};
    shader_resource_view_desc.Format = DXGI_FORMAT_UNKNOWN;
    shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    shader_resource_view_desc.Buffer.NumElements = static_cast<UINT>(material_data.size());
    hr = device->CreateShaderResourceView(material_buffer.Get(), &shader_resource_view_desc, material_resource_view.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    // Create and upload textures on GPU
    for (image& image : images)
    {
        if (image.cache_data.size() > 0)
        {
            ID3D11ShaderResourceView* texture_resource_view = NULL;
            hr = LoadTextureFromMemory(device, image.cache_data.data(), image.cache_data.size(), &texture_resource_view);
            if (hr == S_OK)
            {
                texture_resource_views.emplace_back().Attach(texture_resource_view);
            }
            image.cache_data.clear();
        }
        else
        {
            const std::filesystem::path path(filename);
            ID3D11ShaderResourceView* shader_resource_view = NULL;
            std::wstring filename = path.parent_path().concat(L"/").wstring() + std::wstring(image.uri.begin(), image.uri.end());
            hr = LoadTextureFromFile(device, filename.c_str(), &shader_resource_view, NULL);
            if (hr == S_OK)
            {
                texture_resource_views.emplace_back().Attach(shader_resource_view);
            }
        }
    }

    D3D11_INPUT_ELEMENT_DESC input_element_desc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "ROTATION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = CreateVsFromCSO(device, "soft_body_vs.cso", vertex_shader.ReleaseAndGetAddressOf(), input_layout.ReleaseAndGetAddressOf(), input_element_desc, _countof(input_element_desc));
    hr = CreatePsFromCSO(device, "soft_body_ps.cso", pixel_shader.ReleaseAndGetAddressOf());
    // SOFT_BODY
    hr = CreateCsFromCSO(device, "simulate_cloth_cs.cso", simulate_cloth_cs.GetAddressOf());
    hr = CreateCsFromCSO(device, "simulate_cloth_copy_cs.cso", simulate_cloth_copy_cs.GetAddressOf());

    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = sizeof(primitive_constants);
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = 0;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;
        hr = device->CreateBuffer(&buffer_desc, nullptr, primitive_cbuffer.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = sizeof(simulation_constants);
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = 0;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;
        hr = device->CreateBuffer(&buffer_desc, nullptr, simulation_cbuffer.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
}
void soft_body::render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world)
{
    immediate_context->PSSetShaderResources(0, 1, material_resource_view.GetAddressOf());

    immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
    immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);
    immediate_context->IASetInputLayout(input_layout.Get());
    immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    std::function<void(int)> traverse = [&](int node_index)->void {
        const node& node = nodes.at(node_index);
        if (node.mesh > -1)
        {
            const mesh& mesh = meshes.at(node.mesh);
            for (const mesh::primitive& primitive : mesh.primitives)
            {
                // INTERLEAVED_GLTF_MODEL
                UINT stride = sizeof(vertex);
                UINT offset = 0;
                immediate_context->IASetVertexBuffers(0, 1, buffers.at(primitive.vertex_buffer_view.buffer).GetAddressOf(), &stride, &offset);

                primitive_constants primitive_data = {};
                primitive_data.material = primitive.material;
                primitive_data.has_tangent = primitive.has("TANGENT");
                primitive_data.skin = node.skin;
                DirectX::XMStoreFloat4x4(&primitive_data.world, DirectX::XMLoadFloat4x4(&node.global_transform) * DirectX::XMLoadFloat4x4(&world));
                immediate_context->UpdateSubresource(primitive_cbuffer.Get(), 0, 0, &primitive_data, 0, 0);
                immediate_context->VSSetConstantBuffers(0, 1, primitive_cbuffer.GetAddressOf());
                immediate_context->PSSetConstantBuffers(0, 1, primitive_cbuffer.GetAddressOf());

                const material& material = materials.at(primitive.material);
                const int texture_indices[] =
                {
                    material.data.pbr_metallic_roughness.basecolor_texture.index,
                    material.data.pbr_metallic_roughness.metallic_roughness_texture.index,
                    material.data.normal_texture.index,
                    material.data.emissive_texture.index,
                    material.data.occlusion_texture.index,
                };
                ID3D11ShaderResourceView* null_shader_resource_view = {};
                std::vector<ID3D11ShaderResourceView*> shader_resource_views(_countof(texture_indices));
                for (int texture_index = 0; texture_index < shader_resource_views.size(); ++texture_index)
                {
                    shader_resource_views.at(texture_index) = texture_indices[texture_index] > -1 ? texture_resource_views.at(textures.at(texture_indices[texture_index]).source).Get() : null_shader_resource_view;
                }
                immediate_context->PSSetShaderResources(1, static_cast<UINT>(shader_resource_views.size()), shader_resource_views.data());

                if (primitive.index_buffer_view.buffer > -1)
                {
                    // INTERLEAVED_GLTF_MODEL
                    immediate_context->IASetIndexBuffer(buffers.at(primitive.index_buffer_view.buffer).Get(), primitive.index_buffer_view.format, 0);
                    immediate_context->DrawIndexed(primitive.index_buffer_view.size_in_bytes / _sizeof_component(primitive.index_buffer_view.format), 0, 0);
                }
                else
                {
                    // INTERLEAVED_GLTF_MODEL
                    immediate_context->Draw(primitive.vertex_buffer_view.size_in_bytes / primitive.vertex_buffer_view.stride_in_bytes, 0);
                }
            }
        }
        for (std::vector<int>::value_type child_index : node.children)
        {
            traverse(child_index);
        }
        };
    for (std::vector<int>::value_type node_index : scenes.at(default_scene).nodes)
    {
        traverse(node_index);
    }

    // SOFT_BODY
    Microsoft::WRL::ComPtr<ID3D11Buffer> null_buffer;
    UINT stride = 0, offset = 0;
    immediate_context->IASetVertexBuffers(0, 1, null_buffer.GetAddressOf(), &stride, &offset);

}

// SOFT_BODY
void soft_body::simulate(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world, float delta_time)
{
    std::function<void(int)> traverse = [&](int node_index)->void {
        const node& node = nodes.at(node_index);
        if (node.mesh > -1)
        {
            const mesh& mesh = meshes.at(node.mesh);
            for (const mesh::primitive& primitive : mesh.primitives)
            {
                simulation_constants simulation_data = {};
                DirectX::XMStoreFloat4x4(&simulation_data.inv_world, DirectX::XMMatrixInverse(NULL, DirectX::XMLoadFloat4x4(&node.global_transform) * DirectX::XMLoadFloat4x4(&world)));
                simulation_data.dt = delta_time;
                immediate_context->UpdateSubresource(simulation_cbuffer.Get(), 0, 0, &simulation_data, 0, 0);
                immediate_context->CSSetConstantBuffers(0, 1, simulation_cbuffer.GetAddressOf());

                immediate_context->CSSetUnorderedAccessViews(0, 1, primitive.vertex_uav.GetAddressOf(), NULL);
                immediate_context->CSSetUnorderedAccessViews(1, 1, primitive.cloth_vertex_uav.GetAddressOf(), NULL);
                immediate_context->CSSetShaderResources(0, 1, primitive.cloth_edge_srv.GetAddressOf());

                UINT thread_group_count = static_cast<UINT>(primitive.vertex_buffer_view.size_in_bytes / primitive.vertex_buffer_view.stride_in_bytes);

                immediate_context->CSSetShader(simulate_cloth_cs.Get(), NULL, 0);
                immediate_context->Dispatch(thread_group_count, 1, 1);

                immediate_context->CSSetShader(simulate_cloth_copy_cs.Get(), NULL, 0);
                immediate_context->Dispatch(thread_group_count, 1, 1);

                ID3D11UnorderedAccessView* null_uav[2] = { NULL, NULL };
                immediate_context->CSSetUnorderedAccessViews(0, 2, null_uav, NULL);
                immediate_context->CSSetShader(NULL, NULL, 0);
            }
        }
        for (std::vector<int>::value_type child_index : node.children)
        {
            traverse(child_index);
        }
        };
    for (std::vector<int>::value_type node_index : scenes.at(default_scene).nodes)
    {
        traverse(node_index);
    }
}
