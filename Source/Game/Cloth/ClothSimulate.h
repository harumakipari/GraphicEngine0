#pragma once
#define NOMINMAX

#include <d3d11.h>
#include <directxmath.h>

#include <vector>
#include <unordered_map>
#include <string>
#include <cstddef>

#include "tiny_gltf.h"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/unordered_map.hpp>
#include "Graphics/Core/ConstantBuffer.h"


class ClothSimulate
{
public:
    struct ClothSimulateCBuffer
    {
        float gravity = -0.98f;
        int vertexCount = 0;
        float windPhaseOffset = 0.0f;
        float windBase = 5.0f;

        DirectX::XMFLOAT3 windEmitPosition = { 0.0f,0.0f,0.0f };    // 風が発生している場所
        uint32_t enableDriveForce = 1;
        float bottomWindExponent = 3.0f;
        float driveForceAmplitude = 0.8f;
        float driveForceFrequency = 1.5f;
        float driveForcePhaseGradient = 1.8f;
    };

    struct ClothTimeCBuffer
    {
        float elapsedTime = 0.0f;
        float deltaTime = 1.0f / 120.0f;
        float gravityScale = 0.25f;
        float clothTimePadding = 0.0f;
    };

    struct ClothAlphaTestCBuffer
    {
        uint32_t enableAlphaTest = 1;
        float alphaThreshold = 0.5f;
        float padding[2] = {};
    };

    struct ClothEdge
    {
        uint32_t neighbor;  // 隣接頂点のインデックス
        DirectX::XMFLOAT3 delta;    // 初期の相対ベクトル
        float restLength;   // 初期距離
    };

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
            DirectX::XMFLOAT3 velocity = { 0.0f,0.0f,0.0f };
            DirectX::XMFLOAT3 oldPosition = { 0,0,0 };
            DirectX::XMFLOAT3 oldVelocity = { 0.0f,0.0f,0.0f };
            DirectX::XMFLOAT4 rotation = { 0.0f,0.0f,0.0f,1.0f };
            int isPinned = 0;

            template<class T>
            void serialize(T& archive)
            {
                archive(
                    cereal::make_nvp("position", position),
                    cereal::make_nvp("normal", normal),
                    cereal::make_nvp("tangent", tangent),
                    cereal::make_nvp("texcoord", texcoord),
                    cereal::make_nvp("velocity", velocity),
                    cereal::make_nvp("oldPosition", oldPosition),
                    cereal::make_nvp("oldVelocity", oldVelocity),
                    cereal::make_nvp("rotation", rotation),
                    cereal::make_nvp("isPinned", isPinned)
                );
            }
        };

        std::string name;

        struct Primitive
        {
            int material;// 現在使用しているマテリアルの index
            std::vector<unsigned char> cachedIndices;
            IndexBufferView indexBufferView;

            std::vector<Vertex> cachedVertices;
            VertexBufferView vertexBufferView;

            std::unordered_map<std::string, DXGI_FORMAT> attributes;
            uint32_t clothVertexOffset = 0; // index をまとめる時に必要
            UINT startIndexLocation = 0;// 描画時に必要
            UINT indexCount = 0;
            std::vector<ClothEdge> finalEdges;
            int ping = 0;
            int pong = 1;
            DirectX::XMFLOAT4X4 previousGraphicsWorld{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
            bool hasPreviousGraphicsWorld = false;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clothSRV[2];
            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clothUAV[2];
            Microsoft::WRL::ComPtr<ID3D11Buffer> clothBuffer[2];
            Microsoft::WRL::ComPtr<ID3D11Buffer> initialBottomWeightBuffer;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> initialBottomWeightSRV;
            // 描画時に使用する
            Microsoft::WRL::ComPtr<ID3D11Buffer> clothVB;   // 頂点バッファ
            Microsoft::WRL::ComPtr<ID3D11Buffer> clothEdgeBuffer;   // エッジ用のバッファ
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clothEdgeSRV;

            void CreateClothPingPongBuffers(ID3D11Device* device);

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
                    cereal::make_nvp("attributes", attributes),
                    cereal::make_nvp("clothVertexOffset", clothVertexOffset),
                    cereal::make_nvp("startIndexLocation", startIndexLocation),
                    cereal::make_nvp("indexCount", indexCount),
                    cereal::make_nvp("ping", ping),
                    cereal::make_nvp("pong", pong)
                );
            }
        };
        std::vector<Primitive> primitives;

        template<class T>
        void serialize(T& archive)
        {
            archive(cereal::make_nvp("name", name), cereal::make_nvp("primitives", primitives));
        }
    };

    struct TextureInfo
    {
        int index = -1;
        int texcoord = 0;

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
        int texcoord = 0;
        float scale = 1;

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
        int index = -1;
        int texcoord = 0;
        float strength = 1;

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

            DirectX::XMFLOAT3 rimColor = { 0.0f, 0.0f, 0.0f };
            float rimPower = 0.0f;

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
        static_assert(sizeof(Cbuffer) == 112, "Material Cbuffer must match HLSL MaterialConstants stride.");
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

    struct Image
    {
        std::string name;
        int width = -1;
        int height = -1;
        int component = -1;
        int bits = -1;
        int pixelType = -1;
        std::string mimeType;
        std::string uri;

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

    struct ComputePrimitiveConstants
    {
        DirectX::XMFLOAT4X4 world;
        int material{ -1 };
        int hasTangent{ 0 };
        int skin{ -1 };
        int pad{ 0 };
        DirectX::XMFLOAT4X4 invWorld;
    };

    struct GraphicsPrimitiveConstants
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 previousWorld;
        int material{ -1 };
        int hasTangent{ 0 };
        int skin{ -1 };
        int materialType{ 0 };
        DirectX::XMFLOAT4X4 inverseTransposeWorld;
    };

public:
    ClothSimulate(ID3D11Device* device, const std::string& filename);
    virtual ~ClothSimulate() = default;

    void Update(float deltaTime);

    void Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world);

    bool IsClothRenderingEnabled() const { return enableClothRendering; }

private:
    void Simulate(ID3D11DeviceContext* immediateContext);

    void FetchNodes(const tinygltf::Model& gltf_model);
    void FetchMeshes(ID3D11Device* device, const tinygltf::Model& gltf_model);
    void FetchMaterials(ID3D11Device* device, const tinygltf::Model& gltf_model);
    void FetchTextures(ID3D11Device* device, const tinygltf::Model& gltf_model);

    void CumulateTransforms(std::vector<Node>& nodes) const;
    void CreateAndUploadResources(ID3D11Device* device);

    // 指定されたアライメントに合わせて数値を調整する関数
    UINT Align(UINT num, UINT alignment)
    {
        return (num + (alignment - 1)) & ~(alignment - 1);
    }

private:
    //リソースキャッシュ
    std::shared_ptr<tinygltf::Model> gltfModel;
    static inline std::unordered_map<std::string, std::weak_ptr<tinygltf::Model>> cachedGltfModels;
    std::string filename;

    std::vector<Scene> scenes;
    int defaultScene = 0;

    std::unique_ptr<ConstantBuffer<ClothSimulateCBuffer>> cBuffer;
    std::unique_ptr<ConstantBuffer<ClothTimeCBuffer>> clothTimeCBuffer;
    std::unique_ptr<ConstantBuffer<ClothAlphaTestCBuffer>> clothAlphaTestCBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clothOpacityTextureSRV;

    bool clothOpacityTextureLoaded = false;
    bool enableAlphaTest = true;
    float alphaThreshold = 0.5f;

    float windPhaseOffset = 0.0f;
    float windBase = 0.0f;
    DirectX::XMFLOAT3 windEmitPosition = {};
    bool enableDriveForce = true;
    float driveForceAmplitude = 1.0f;
    float driveForceFrequency = 5.0f;
    float driveForcePhaseGradient = 4.5f;
    float gravityScale = 0.0f;
    float bottomWindExponent = 1.0f;

    bool enableClothRendering = true;
    bool enableClothSimulation = true;

    float fixedDeltaTime = 1.0f / 120.0f;
    int maxSubsteps = 8;
    float simulationAccumulator = 0.0f;
    float simulationElapsedTime = 0.0f;

    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> buffers;
    std::vector<Material> materials;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> materialResourceView;
    std::vector<Texture> textures;
    std::vector<Image> images;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textureResourceViews;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> clothUpdateCS;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> clothConstraintCS;
    Microsoft::WRL::ComPtr<ID3D11Buffer> computePrimitiveCbuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> graphicsPrimitiveCbuffer;

    std::vector<Mesh::Vertex> allVertices;
};
