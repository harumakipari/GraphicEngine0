// INTERLEAVED_GLTF_MODEL
#pragma once
#define NOMINMAX

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>
#include <unordered_map>
#include <optional>

//#define TINYGLTF_NO_EXTERNAL_IMAGE
//#define TINYGLTF_NO_STB_IMAGE
//#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/unordered_map.hpp>


#include "Physics/Collider.h"
#include "Graphics/Core/PipelineState.h"
#include "Graphics/Resource/InterleavedGltfModel.h"


class MeshComponent;

class MorphModel :public InterleavedGltfModel
{
    //リソースキャッシュ
    std::shared_ptr<tinygltf::Model> gltfModel;
    static inline std::unordered_map<std::string, std::weak_ptr<tinygltf::Model>> cachedGltfModels;

    MeshComponent* meshComponent;
    std::string filename;
    bool converted_handedness = false;    // 座標系の変換
public:
    MorphModel(ID3D11Device* device, const std::string& filename, ModelTypes::ModelMode mode = ModelTypes::ModelMode::SkeletalMesh, bool isSaveVerticesData = false, bool convertToLHS = false);
    virtual ~MorphModel() = default;

    // Instance で使用する
    void SetMeshComponent(MeshComponent* mesh) { this->meshComponent = mesh; }

    AABB GetAABB()const;

    DirectX::XMFLOAT3 GetModelSize() const;

    struct Scene
    {
        std::string name;
        std::vector<int> nodes; // Array of 'root' nodes

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("name", name),
                cereal::make_nvp("nodes", nodes)
            );
        }
    };
    std::vector<Scene> scenes;
    int defaultScene = 0;

    struct Node
    {
        std::string name;
        int skin = -1;
        int mesh = -1;

        std::vector<int> children;

        // ローカル行列
        DirectX::XMFLOAT4 rotation = { 0, 0, 0, 1 };
        DirectX::XMFLOAT3 scale = { 1, 1, 1 };
        DirectX::XMFLOAT3 translation = { 0, 0, 0 };

        DirectX::XMFLOAT4X4 globalTransform = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };


        DirectX::XMFLOAT3 maxValue = { +D3D11_FLOAT32_MAX, +D3D11_FLOAT32_MAX, +D3D11_FLOAT32_MAX };
        DirectX::XMFLOAT3 minValue = { -D3D11_FLOAT32_MAX, -D3D11_FLOAT32_MAX, -D3D11_FLOAT32_MAX };

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("name", name),
                cereal::make_nvp("skin", skin),
                cereal::make_nvp("mesh", mesh),
                cereal::make_nvp("children", children),
                cereal::make_nvp("rotation", rotation),
                cereal::make_nvp("scale", scale),
                cereal::make_nvp("translation", translation),
                cereal::make_nvp("globalTransform", globalTransform),
                cereal::make_nvp("maxValue", maxValue),
                cereal::make_nvp("minValue", minValue)
            );
        }
    };

    // ノードを取得する関数
    const std::vector<Node>& GetNodes() const { return nodes; }

private:
    std::vector<Node> nodes;
public:

    struct IndexBufferView
    {
        int buffer = -1;
        UINT sizeInBytes = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("buffer", buffer),
                cereal::make_nvp("sizeInBytes", sizeInBytes),
                cereal::make_nvp("format", format)
            );
        }
    };
    struct VertexBufferView
    {
        int buffer = -1;
        UINT sizeInBytes = 0;
        UINT strideInBytes = 0;

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("buffer", buffer),
                cereal::make_nvp("sizeInBytes", sizeInBytes),
                cereal::make_nvp("strideInBytes", strideInBytes)
            );
        }
    };
    struct Mesh
    {
        struct Vertex
        {
            DirectX::XMFLOAT3 position = { 0, 0, 0 };
            DirectX::XMFLOAT3 normal = { 0, 0, 1 };
            DirectX::XMFLOAT4 tangent = { 1, 0, 0, 1 };
            DirectX::XMFLOAT2 texcoord = { 0, 0 };
            DirectX::XMUINT4 joints0 = { 0, 0, 0, 0 };
            DirectX::XMUINT4 joints1 = { 0, 0, 0, 0 };
            DirectX::XMFLOAT4 weights0 = { 1, 0, 0, 0 };
            DirectX::XMFLOAT4 weights1 = { 0, 0, 0, 0 };

            DirectX::XMFLOAT3 morphPosition[4]; // delta
            DirectX::XMFLOAT3 morphNormal[4];   // delta

            template<class T>
            void serialize(T& archive)
            {
                archive(
                    cereal::make_nvp("position", position),
                    cereal::make_nvp("normal", normal),
                    cereal::make_nvp("tangent", tangent),
                    cereal::make_nvp("texcoord", texcoord),
                    cereal::make_nvp("joints0", joints0),
                    cereal::make_nvp("joints1", joints1),
                    cereal::make_nvp("weights0", weights0),
                    cereal::make_nvp("weights1", weights1),
                    cereal::make_nvp("morphPosition", morphPosition),
                    cereal::make_nvp("morphNormal", morphNormal)
                );
            }
        };

        std::string name;

        struct Primitive
        {
            //std::vector<int> materialIndices; // 複数のマテリアル候補
            int material;// 現在使用しているマテリアルの index

            std::vector<unsigned char> cachedIndices;
            IndexBufferView indexBufferView;

            std::vector<Vertex> cachedVertices;
            VertexBufferView vertexBufferView;

            std::unordered_map<std::string, DXGI_FORMAT> attributes;

            bool has(const char* attribute) const
            {
                return attributes.find(attribute) != attributes.end();
            }

            template<class T>
            void serialize(T& archive)
            {
                archive(
                    cereal::make_nvp("material", material),
                    cereal::make_nvp("cachedIndices", cachedIndices),
                    cereal::make_nvp("indexBufferView", indexBufferView),
                    cereal::make_nvp("cachedVertices", cachedVertices),
                    cereal::make_nvp("vertexBufferView", vertexBufferView),
                    cereal::make_nvp("attributes", attributes)
                );
            }
        };
        std::vector<Primitive> primitives;


        template<class T>
        void serialize(T& archive)
        {
            archive(cereal::make_nvp("name", name), cereal::make_nvp("primitives", primitives)/*, cereal::make_nvp("boundingBox", boundingBox)*/);
        }
    };
    std::vector<Mesh> meshes;


    // INTERLEAVED_GLTF_MODEL
    struct BatchMesh
    {
        struct Vertex
        {
            DirectX::XMFLOAT3 position = { 0, 0, 0 };
            DirectX::XMFLOAT3 normal = { 0, 0, 1 };
            DirectX::XMFLOAT4 tangent = { 1, 0, 0, 1 };
            DirectX::XMFLOAT2 texcoord = { 0, 0 };

            template<class T>
            void serialize(T& archive)
            {
                archive(
                    cereal::make_nvp("position", position),
                    cereal::make_nvp("normal", normal),
                    cereal::make_nvp("tangent", tangent),
                    cereal::make_nvp("texcoord", texcoord)
                );
            }
        };

        int material;

        std::vector<UINT> cachedIndices;
        IndexBufferView indexBufferView;

        std::vector<Vertex> cachedVertices;
        VertexBufferView vertexBufferView;

        std::unordered_map<std::string, DXGI_FORMAT> attributes;

        bool has(const char* attribute) const
        {
            return attributes.find(attribute) != attributes.end();
        }

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("material", material),
                cereal::make_nvp("cachedIndices", cachedIndices),
                cereal::make_nvp("indexBufferView", indexBufferView),
                cereal::make_nvp("cachedVertices", cachedVertices),
                cereal::make_nvp("vertexBufferView", vertexBufferView),
                cereal::make_nvp("attributes", attributes)
            );
        }
    };
    std::vector<BatchMesh> batchMeshes;
    //const bool staticBatching;
    ModelTypes::ModelMode mode = ModelTypes::ModelMode::SkeletalMesh;

    
    std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> buffers;

    void Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world, const std::vector<Node>& animated_nodes, RenderPass pass, const PipeLineStateDesc& pipeline = {});
    // INTERLEAVED_GLTF_MODEL
    void BatchRender(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world, RenderPass pass, const PipeLineStateDesc& pipeline);

    void InstancedStaticBatchRender(ID3D11DeviceContext* immediate_context/*, const DirectX::XMFLOAT4X4& world*/, RenderPass pass, const PipeLineStateDesc& pipeline = {});


    struct TextureInfo
    {
        int index = -1; // required.
        int texcoord = 0; // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("index", index),
                cereal::make_nvp("texcoord", texcoord)
            );
        }
    };

    struct NormalTextureInfo
    {
        int index = -1;
        int texcoord = 0;    // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
        float scale = 1;    // scaledNormal = normalize((<sampled normal texture value> * 2.0 - 1.0) * vec3(<normal scale>, <normal scale>, 1.0))

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("index", index),
                cereal::make_nvp("texcoord", texcoord),
                cereal::make_nvp("scale", scale)
            );
        }
    };

    struct OcclusionTextureInfo
    {
        int index = -1;   // required
        int texcoord = 0;     // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
        float strength = 1;  // A scalar parameter controlling the amount of occlusion applied. A value of `0.0` means no occlusion. A value of `1.0` means full occlusion. This value affects the final occlusion value as: `1.0 + strength * (<sampled occlusion texture value> - 1.0)`.

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("index", index),
                cereal::make_nvp("texcoord", texcoord),
                cereal::make_nvp("strength", strength)
            );
        }
    };

    struct PbrMetallicRoughness
    {
        float basecolorFactor[4] = { 1, 1, 1, 1 };  // len = 4. default [1,1,1,1]
        TextureInfo basecolorTexture;
        float metallicFactor = 1;   // default 1
        float roughnessFactor = 1;  // default 1
        TextureInfo metallicRoughnessTexture;

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("baseColorFactor", basecolorFactor),
                cereal::make_nvp("basecolorTexture", basecolorTexture),
                cereal::make_nvp("metallicFactor", metallicFactor),
                cereal::make_nvp("roughnessFactor", roughnessFactor),
                cereal::make_nvp("metallicRoughnessTexture", metallicRoughnessTexture)
            );
        }
    };

    struct Material
    {
        std::string name;

        Microsoft::WRL::ComPtr<ID3D11PixelShader> replacedPixelShader{ nullptr };// カスタムシェーダー
        // nullptr なら通常のパスから自動的に決定される
        // 指定があればこの文字列のパイプラインステートを使用する
        std::optional<std::string> overridePipelineName;

        struct Cbuffer
        {
            float emissiveFactor[3] = { 0, 0, 0 };  // length 3. default [0, 0, 0]
            int alphaMode = 0;	// "OPAQUE" : 0, "MASK" : 1, "BLEND" : 2
            float alphaCutoff = 0.5f; // default 0.5
            int doubleSided = 0; // default false;

            PbrMetallicRoughness pbrMetallicRoughness;

            NormalTextureInfo normalTexture;
            OcclusionTextureInfo occlusionTexture;
            TextureInfo emissiveTexture;

            template<class T>
            void serialize(T& archive)
            {
                archive(
                    cereal::make_nvp("emissiveFactor", emissiveFactor),
                    cereal::make_nvp("alphaMode", alphaMode),
                    cereal::make_nvp("alphaCutoff", alphaCutoff),
                    cereal::make_nvp("doubleSided", doubleSided),
                    cereal::make_nvp("pbrMetallicRoughness", pbrMetallicRoughness),
                    cereal::make_nvp("normalTexture", normalTexture),
                    cereal::make_nvp("occlusionTexture", occlusionTexture),
                    cereal::make_nvp("emissiveTexture", emissiveTexture)
                );
            }
        };
        Cbuffer data;

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("name", name),
                cereal::make_nvp("data", data)
            );
        }
    };
    std::vector<Material> materials;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> materialResourceView;

    struct Texture
    {
        std::string name;
        int source = -1;

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("name", name),
                cereal::make_nvp("source", source)
            );
        }
    };
    std::vector<Texture> textures;
    struct Image
    {
        std::string name;
        int width = -1;
        int height = -1;
        int component = -1;
        int bits = -1;			// bit depth per channel. 8(byte), 16 or 32.
        int pixelType = -1;	// pixel type(TINYGLTF_COMPONENT_TYPE_***). usually UBYTE(bits = 8) or USHORT(bits = 16)
        std::string mimeType;	// (required if no uri) ["image/jpeg", "image/png", "image/bmp", "image/gif"]
        std::string uri;		// (required if no mimeType) uri is not decoded(e.g. whitespace may be represented as %20)

        // When this flag is true, data is stored to `image` in as-is format(e.g. jpeg
        // compressed for "image/jpeg" mime) This feature is good if you use custom
        // image loader function. (e.g. delayed decoding of images for faster glTF
        // parsing) Default parser for Image does not provide as-is loading feature at
        // the moment. (You can manipulate this by providing your own LoadImageData
        // function)
        bool asIs = false;

        std::vector<unsigned char> cacheData;

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("name", name),
                cereal::make_nvp("width", width),
                cereal::make_nvp("height", height),
                cereal::make_nvp("component", component),
                cereal::make_nvp("bits", bits),
                cereal::make_nvp("pixelType", pixelType),
                cereal::make_nvp("mimeType", mimeType),
                cereal::make_nvp("uri", uri),
                cereal::make_nvp("asIs", asIs),
                cereal::make_nvp("cacheData", cacheData)
            );
        }
    };
    std::vector<Image> images;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textureResourceViews;

    struct Skin
    {
        std::vector<DirectX::XMFLOAT4X4> inverseBindMatrices;
        std::vector<int> joints;

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("inverseBindMatrices", inverseBindMatrices),
                cereal::make_nvp("joints", joints)
            );
        }
    };
    std::vector<Skin> skins;

    struct Animation
    {
        std::string name;
        float duration = 0.0f;

        struct Channel
        {
            int sampler = -1; // required
            int targetNode = -1; // required (index of the node to target)
            std::string targetPath; // required in ["translation", "rotation", "scale", "weights"]

            template<class T>
            void serialize(T& archive)
            {
                archive(
                    cereal::make_nvp("sampler", sampler),
                    cereal::make_nvp("targetNode", targetNode),
                    cereal::make_nvp("targetPath", targetPath)
                );
            }
        };
        std::vector<Channel> channels;

        struct Sampler
        {
            int input = -1;
            int output = -1;
            std::string interpolation;

            template<class T>
            void serialize(T& archive)
            {
                archive(
                    cereal::make_nvp("input", input),
                    cereal::make_nvp("output", output),
                    cereal::make_nvp("interpolation", interpolation)
                );
            }
        };
        std::vector<Sampler> samplers;

        std::unordered_map<int/*sampler.input*/, std::vector<float>> timelines;
        std::unordered_map<int/*sampler.output*/, std::vector<DirectX::XMFLOAT3>> scales;
        std::unordered_map<int/*sampler.output*/, std::vector<DirectX::XMFLOAT4>> rotations;
        std::unordered_map<int/*sampler.output*/, std::vector<DirectX::XMFLOAT3>> translations;

        template<class T>
        void serialize(T& archive)
        {
            archive(
                cereal::make_nvp("name", name),
                cereal::make_nvp("duration", duration),
                cereal::make_nvp("channels", channels),
                cereal::make_nvp("samplers", samplers),
                cereal::make_nvp("timelines", timelines),
                cereal::make_nvp("scales", scales),
                cereal::make_nvp("rotations", rotations),
                cereal::make_nvp("translations", translations)
            );
        }
    };
    std::vector<Animation> animations;
public:
    //void GetBoundingBox(size_t nodeIndex, DirectX::FXMMATRIX transform) const
    //{
    //    const Node& node = nodes.at(nodeIndex);

    //    DirectX::XMVECTOR minValue = DirectX::XMLoadFloat3(&node.minValue);
    //    DirectX::XMVECTOR maxValue = DirectX::XMLoadFloat3(&node.maxValue);
    //}
private:
    void FetchNodes(const tinygltf::Model& gltf_model);
    void CumulateTransforms(std::vector<Node>& nodes) const;
    void FetchMeshes(ID3D11Device* device, const tinygltf::Model& gltf_model);
    // INTERLEAVED_GLTF_MODEL
    void FetchAndBatchMeshes(ID3D11Device* device, const tinygltf::Model& gltf_model);

public:
    // CascadedShadowMaps
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShaderCSM;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometryShaderCSM;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    struct PrimitiveConstants
    {
        DirectX::XMFLOAT4X4 world;

        int material{ -1 };
        int hasTangent{ 0 };
        int skin{ -1 };
        int pad;

        DirectX::XMFLOAT4X4 inverseTransposeWorld;  // 法線変換行列
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> primitiveCbuffer;

    void FetchMaterials(ID3D11Device* device, const tinygltf::Model& gltf_model);
    void FetchTextures(ID3D11Device* device, const tinygltf::Model& gltf_model);
    void FetchAnimations(const tinygltf::Model& gltf_model);
    void FetchAnimations(const tinygltf::Model& gltfModel, std::vector<Animation>& outAnimations);

    static const size_t PRIMITIVE_MAX_JOINTS = 512;
    struct PrimitiveJointConstants
    {
        DirectX::XMFLOAT4X4 matrices[PRIMITIVE_MAX_JOINTS];
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> primitiveJointCbuffer;

    void CreateAndUploadResources(ID3D11Device* device);

    void CastShadowBatch(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world);

    void AddAnimation(const std::string& filename);

    void ExtractAnimations(const tinygltf::Model& transmission_model);


public:
    void AppendAnimations(const std::vector<std::string>& filenames);

    // アニメーションを追加する関数
    void AddAnimations(const std::vector<std::string>& filenames);

    // モデルのジョイントのワールド空間の position を返す関数
    DirectX::XMFLOAT3 GetJointWorldPosition(/*size_t nodeIndex,*/const std::string& name, const std::vector<Node>& animatedNodes, const DirectX::XMFLOAT4X4& transform);

    // モデルのジョイントのローカル空間の position を返す関数
    DirectX::XMFLOAT3 GetJointLocalPosition(/*size_t nodeIndex,*/const std::string& name, const std::vector<Node>& animatedNodes);

    //アニメーションをブレンドする関数
    void BlendAnimations(const std::vector<Node>& fromNodes, const std::vector<Node>& toNodes, float factor, std::vector<Node>& outNodes);

    void Animate(size_t animation_index, float time, std::vector<Node>& animated_nodes);

    void CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world, const std::vector<Node>& animatedNodes);

    //// インスタンスを追加
    //int AddInstance(const Transform& transform)
    //{
    //    DirectX::XMFLOAT4X4 instanceMatrix;
    //    DirectX::XMStoreFloat4x4(&instanceMatrix, transform.ToMatrix());
    //    instanceMatrices_.push_back(instanceMatrix);
    //    return static_cast<int>(instanceMatrices_.size()) - 1;
    //}
private:
    void ComputeAABBFromMesh(const MorphModel::Node& node, const MorphModel& model, DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax);

    // 名前とワールドのnode座標をキャッシュしておく
    std::unordered_map<std::string, DirectX::XMFLOAT3> nameToNodeWorldPosition_;

    // 頂点のデータを残しておくか
    bool isSaveVerticesData = false;

public:
    // インスタンス用のバッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffer;
    // インスタンス用の行列
    std::vector<DirectX::XMFLOAT4X4> instanceMatrices_;

    int instanceCount_ = 0;
};
