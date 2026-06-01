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
#include "Graphics/Core/ConstantBuffer.h"


class ClothSimulate
{
    //リソースキャッシュ
    std::shared_ptr<tinygltf::Model> gltfModel;
    static inline std::unordered_map<std::string, std::weak_ptr<tinygltf::Model>> cachedGltfModels;
    std::string filename;
public:
    //モデルの座標系
    enum class CoordinateSystem
    {
        RH_Y_UP,
        LH_Y_UP,
        RH_Z_UP,
        LH_Z_UP
    };

    //モデルがメートル単位か cm単位の時はfalseにする
    bool isModelInMeters = true;

    //モデル固有の座標系 //初期　LH_Y_UP
    CoordinateSystem modelCoordinateSystem = CoordinateSystem::LH_Y_UP;

    ClothSimulate(ID3D11Device* device, const std::string& filename);
    virtual ~ClothSimulate() = default;

    void SetupPinVertices(int curretPinMode);

    void RecreateClothBuffers(ID3D11Device* device);

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

    void Update(float deltaTime);

    void Simulate(ID3D11DeviceContext* immediateContext);

    struct ClothSimulateCBuffer
    {
        float gravity = -0.98f;
        int vertexCount = 0;
        float windPhaseOffset = 0.0f;
        float windBase = 5.0f;

        DirectX::XMFLOAT3 windEmitPosition={0.0f,0.0f,0.0f};    // 風が発生している場所
    };
    std::unique_ptr<ConstantBuffer<ClothSimulateCBuffer>> cbuffer;

    float windPhaseOffset = 5.0f;
    float windBase = 6.0f;
    DirectX::XMFLOAT3 windEmitPosition = {};


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

    struct ClothEdge
    {
        uint32_t neighbor;  // 隣接頂点のインデックス
        DirectX::XMFLOAT3 delta;    // 初期の相対ベクトル
        float restLength;   // 初期距離
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
            //std::vector<int> materialIndices; // 複数のマテリアル候補
            int material;// 現在使用しているマテリアルの index

            std::vector<unsigned char> cachedIndices;
            IndexBufferView indexBufferView;

            std::vector<Vertex> cachedVertices;
            VertexBufferView vertexBufferView;

            std::unordered_map<std::string, DXGI_FORMAT> attributes;

            // index をまとめる時に必要
            uint32_t clothVertexOffset = 0;
            // 描画時に必要
            UINT startIndexLocation = 0;
            UINT indexCount = 0;
            std::vector<ClothEdge> finalEdges;
            int ping = 0;
            int pong = 1;


            void CreateClothPingPongBuffers(ID3D11Device* device)
            {
                HRESULT hr = S_OK; // バッファ記述 
                D3D11_BUFFER_DESC bufferDesc = {};
                bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Mesh::Vertex) * cachedVertices.size());
                bufferDesc.Usage = D3D11_USAGE_DEFAULT;
                bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
                bufferDesc.CPUAccessFlags = 0; // 構造化バッファとして扱う 
                bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                bufferDesc.StructureByteStride = sizeof(Mesh::Vertex);
                D3D11_SUBRESOURCE_DATA subresourceData = {};
                subresourceData.pSysMem = cachedVertices.data(); // 2 個作る（ピンポン用） 
                subresourceData.SysMemPitch = 0;
                subresourceData.SysMemSlicePitch = 0;
                for (int i = 0; i < 2; ++i)
                {
                    hr = device->CreateBuffer(&bufferDesc, &subresourceData, clothBuffer[i].GetAddressOf());
                    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                    // SRV 作成 
                    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造化バッファは UNKNOWN 
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
                    srvDesc.Buffer.FirstElement = 0;
                    srvDesc.Buffer.NumElements = static_cast<UINT>(cachedVertices.size());
                    hr = device->CreateShaderResourceView(clothBuffer[i].Get(), &srvDesc, clothSRV[i].GetAddressOf());
                    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                    // UAV 作成 
                    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
                    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
                    uavDesc.Buffer.FirstElement = 0;
                    uavDesc.Buffer.NumElements = static_cast<UINT>(cachedVertices.size());
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

                // 描画用のIAVertexBufferを作成
                bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Mesh::Vertex) * cachedVertices.size());
                bufferDesc.Usage = D3D11_USAGE_DEFAULT;
                bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                bufferDesc.CPUAccessFlags = 0;
                bufferDesc.MiscFlags = 0;
                bufferDesc.StructureByteStride = 0;
                subresourceData.pSysMem = cachedVertices.data();
                subresourceData.SysMemPitch = 0;
                subresourceData.SysMemSlicePitch = 0;
                //hr = device->CreateBuffer(&bufferDesc, &subresourceData, clothVB[i].GetAddressOf());
                hr = device->CreateBuffer(&bufferDesc, &subresourceData, clothVB.GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                // 描画用のIAIndexBufferを作成
                {
                    D3D11_BUFFER_DESC indexBufferDesc = {};
                    indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * cachedVertices.size());
                    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
                    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                    D3D11_SUBRESOURCE_DATA subResource = {};
                    subResource.pSysMem = cachedIndices.data();
                    device->CreateBuffer(&indexBufferDesc, &subResource, clothIB.GetAddressOf());
                }
            }

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clothSRV[2];
            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clothUAV[2];
            Microsoft::WRL::ComPtr<ID3D11Buffer> clothBuffer[2];
            // 描画時に使用する
            Microsoft::WRL::ComPtr<ID3D11Buffer> clothVB;   // 頂点バッファ
            Microsoft::WRL::ComPtr<ID3D11Buffer> clothIB;   // インデックスバッファ
            Microsoft::WRL::ComPtr<ID3D11Buffer> clothEdgeBuffer;   // エッジ用のバッファ
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clothEdgeSRV;

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
    std::vector<Mesh> meshes;

    // INTERLEAVED_GLTF_MODEL
    std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> buffers;

    void CreateClothPingPongBuffers(ID3D11Device* device, const std::vector<Mesh::Vertex>& vertices);

    void Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world);

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

private:
    void FetchNodes(const tinygltf::Model& gltf_model);
    void CumulateTransforms(std::vector<Node>& nodes);
    void FetchMeshes(ID3D11Device* device, const tinygltf::Model& gltf_model);
public:
    // CascadedShadowMaps
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShaderCSM;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometryShaderCSM;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

    //Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clothSRV[2];
    //Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clothUAV[2];
    //Microsoft::WRL::ComPtr<ID3D11Buffer> clothBuffer[2];
    //// 描画時に使用する
    //Microsoft::WRL::ComPtr<ID3D11Buffer> clothVB;   // 頂点バッファ
    //Microsoft::WRL::ComPtr<ID3D11Buffer> clothIB;   // インデックスバッファ
    uint32_t clothVertexOffset;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> clothUpdateCS;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> clothConstraintCS;

    //Microsoft::WRL::ComPtr<ID3D11Buffer> clothEdgeBuffer;   // エッジ用のバッファ
    //Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clothEdgeSRV;  

    //struct ClothEdge
    //{
    //    uint32_t neighbor;  // 隣接頂点のインデックス
    //    DirectX::XMFLOAT3 delta;    // 初期の相対ベクトル
    //    float restLength;   // 初期距離
    //};
    std::vector<Mesh::Vertex> allVertices;
    std::vector<uint32_t> globalIndices;
    //std::vector<ClothEdge> finalEdges;


    struct PrimitiveConstants
    {
        DirectX::XMFLOAT4X4 world;

        int material{ -1 };
        int hasTangent{ 0 };
        int skin{ -1 };
        int pad;

        //DirectX::XMFLOAT4X4 inverseTransposeWorld;  // 法線変換行列
        DirectX::XMFLOAT4X4 invWorld;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> primitiveCbuffer;

    void FetchMaterials(ID3D11Device* device, const tinygltf::Model& gltf_model);
    void FetchTextures(ID3D11Device* device, const tinygltf::Model& gltf_model);
    //void FetchAnimations(const tinygltf::Model& gltf_model);

    static const size_t PRIMITIVE_MAX_JOINTS = 512;
    struct PrimitiveJointConstants
    {
        DirectX::XMFLOAT4X4 matrices[PRIMITIVE_MAX_JOINTS];
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> primitiveJointCbuffer;

    void CreateAndUploadResources(ID3D11Device* device);

    // 指定されたアライメントに合わせて数値を調整する関数
    UINT Align(UINT num, UINT alignment)
    {
        return (num + (alignment - 1)) & ~(alignment - 1);
    }

public:
    // 名前とワールドのnode座標をキャッシュしておく
    std::unordered_map<std::string, DirectX::XMFLOAT3> nameToNodeWorldPosition_;

};
