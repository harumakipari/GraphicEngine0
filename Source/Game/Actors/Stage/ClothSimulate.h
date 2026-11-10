#pragma once
#define NOMINMAX

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>
#include <unordered_map>
#include <optional>
#include <string>
#include <cstddef>

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
    enum class SimulationStage
    {
        InitialMesh = 0,
        UpdateCSOnly,
        UpdateAndConstraint,
    };

    //モデルの座標系
    enum class CoordinateSystem
    {
        RH_Y_UP,
        LH_Y_UP,
        RH_Z_UP,
        LH_Z_UP
    };

    //モデルがメートル単位か cm単位の時はfalseにする
private:
    bool isModelInMeters = true;

    //モデル固有の座標系 //初期　LH_Y_UP
    CoordinateSystem modelCoordinateSystem = CoordinateSystem::LH_Y_UP;

public:
    ClothSimulate(ID3D11Device* device, const std::string& filename);
    virtual ~ClothSimulate() = default;

private:
    void RecreateClothBuffers(ID3D11Device* device);

public:
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
private:
    std::vector<Scene> scenes;
    int defaultScene = 0;

public:
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

private:
    void Simulate(ID3D11DeviceContext* immediateContext);

public:
    struct ClothSimulateCBuffer
    {
        float gravity = -0.98f;
        int vertexCount = 0;
        float windPhaseOffset = 0.0f;
        float windBase = 5.0f;

        DirectX::XMFLOAT3 windEmitPosition={0.0f,0.0f,0.0f};    // 風が発生している場所
        uint32_t enableDriveForce = 1;
        float bottomWindExponent = 3.0f;
        float driveForceAmplitude = 0.8f;
        float driveForceFrequency = 1.5f;
        float driveForcePhaseGradient = 1.8f;
    };
private:
    std::unique_ptr<ConstantBuffer<ClothSimulateCBuffer>> cbuffer;

public:
    struct ClothTimeCBuffer
    {
        float elapsedTime = 0.0f;
        float deltaTime = 1.0f / 120.0f;
        float gravityScale = 0.25f;
        float clothTimePadding = 0.0f;
    };
private:
    std::unique_ptr<ConstantBuffer<ClothTimeCBuffer>> clothTimeCBuffer;

public:
    struct ClothAlphaTestCBuffer { uint32_t enableAlphaTest = 1; float alphaThreshold = 0.5f; float padding[2] = {}; };
private:
    std::unique_ptr<ConstantBuffer<ClothAlphaTestCBuffer>> clothAlphaTestCBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clothOpacityTextureSRV;
    bool clothOpacityTextureLoaded = false;
    bool enableAlphaTest = true;
    float alphaThreshold = 0.5f;
public:
    struct ClothPlane
    {
        DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
        float d = 0.0f;
    };
    struct ClothPlaneCBuffer
    {
        ClothPlane planes[4];
    };
private:
    std::unique_ptr<ConstantBuffer<ClothPlaneCBuffer>> planeCBuffer;

    float windPhaseOffset = 0.0f;
    float windBase = 0.0f;
    DirectX::XMFLOAT3 windEmitPosition = {};
    bool enableDriveForce = true;
    float driveForceAmplitude = 1.2f;
    float driveForceFrequency = 5.0f;
    float driveForcePhaseGradient = 5.0f;
    float gravityScale = 0.0f;
    float bottomWindExponent = 1.0f;

    bool enableClothRendering = true;
    bool enableClothSimulation = true;

public:
    bool IsClothRenderingEnabled() const { return enableClothRendering; }

private:
    float fixedDeltaTime = 1.0f / 120.0f;
    int maxSubsteps = 8;
    float simulationAccumulator = 0.0f;
    float simulationElapsedTime = 0.0f;
    UINT lastExecutedSubsteps = 0;
    SimulationStage simulationStage = SimulationStage::UpdateAndConstraint;
    bool disableCulling = false;

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
        static_assert(sizeof(Vertex) == 104, "Cloth Mesh::Vertex must match HLSL ClothVertex stride");
        static_assert(offsetof(Vertex, position) == 0 && offsetof(Vertex, normal) == 12 && offsetof(Vertex, tangent) == 24 && offsetof(Vertex, texcoord) == 40, "ClothVertex prefix must match HLSL");
        static_assert(offsetof(Vertex, velocity) == 48 && offsetof(Vertex, oldPosition) == 60 && offsetof(Vertex, oldVelocity) == 72 && offsetof(Vertex, rotation) == 84 && offsetof(Vertex, isPinned) == 100, "ClothVertex simulation fields must match HLSL");

        std::string name;

        struct Primitive
        {
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
            DirectX::XMFLOAT4X4 previousGraphicsWorld{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
            bool hasPreviousGraphicsWorld = false;


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

                std::vector<float> initialBottomWeights(cachedVertices.size());
                if (!cachedVertices.empty())
                {
                    float minInitialY = cachedVertices.front().position.y;
                    float maxInitialY = minInitialY;
                    for (const Vertex& vertex : cachedVertices)
                    {
                        minInitialY = (std::min)(minInitialY, vertex.position.y);
                        maxInitialY = (std::max)(maxInitialY, vertex.position.y);
                    }
                    const float inverseInitialHeight = 1.0f / (std::max)(maxInitialY - minInitialY, 1.0e-6f);
                    for (size_t vertexIndex = 0; vertexIndex < cachedVertices.size(); ++vertexIndex)
                    {
                        initialBottomWeights[vertexIndex] = (std::clamp)((maxInitialY - cachedVertices[vertexIndex].position.y) * inverseInitialHeight, 0.0f, 1.0f);
                    }

                    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(float) * initialBottomWeights.size());
                    bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
                    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    bufferDesc.CPUAccessFlags = 0;
                    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                    bufferDesc.StructureByteStride = sizeof(float);
                    subresourceData = {};
                    subresourceData.pSysMem = initialBottomWeights.data();
                    hr = device->CreateBuffer(&bufferDesc, &subresourceData, initialBottomWeightBuffer.GetAddressOf());
                    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

                    D3D11_SHADER_RESOURCE_VIEW_DESC bottomWeightSrvDesc = {};
                    bottomWeightSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
                    bottomWeightSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
                    bottomWeightSrvDesc.Buffer.FirstElement = 0;
                    bottomWeightSrvDesc.Buffer.NumElements = static_cast<UINT>(initialBottomWeights.size());
                    hr = device->CreateShaderResourceView(initialBottomWeightBuffer.Get(), &bottomWeightSrvDesc, initialBottomWeightSRV.GetAddressOf());
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
                hr = device->CreateBuffer(&bufferDesc, &subresourceData, clothVB.GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            }

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clothSRV[2];
            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clothUAV[2];
            Microsoft::WRL::ComPtr<ID3D11Buffer> clothBuffer[2];
            Microsoft::WRL::ComPtr<ID3D11Buffer> initialBottomWeightBuffer;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> initialBottomWeightSRV;
            // 描画時に使用する
            Microsoft::WRL::ComPtr<ID3D11Buffer> clothVB;   // 頂点バッファ
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
private:
    std::vector<Mesh> meshes;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> buffers;

public:
    void Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4& world);

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
private:
    std::vector<Material> materials;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> materialResourceView;

public:
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
private:
    std::vector<Texture> textures;
public:
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
private:
    std::vector<Image> images;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textureResourceViews;

private:
    struct VertexDiagnostic
    {
        const char* label = "";
        UINT index = 0;
        DirectX::XMFLOAT3 position = {};
        DirectX::XMFLOAT3 velocity = {};
        float displacement = 0.0f;
        bool finite = true;
    };

    struct ClothStretchDiagnostics
    {
        UINT vertexCount = 0;
        UINT directedSpringCount = 0;
        UINT uniqueSpringCount = 0;
        UINT minConnections = 0;
        UINT maxConnections = 0;
        float averageConnections = 0.0f;
        float restLengthMin = 0.0f;
        float restLengthMax = 0.0f;
        float restLengthAverage = 0.0f;
        float initialHeight = 0.0f;
        float currentHeight = 0.0f;
        float maxStretchRatio = 0.0f;
        float averageStretchRatio = 0.0f;
        float maxVertexSpeed = 0.0f;
        bool finite = true;
        bool valid = false;
    };
    void CaptureVertexDiagnostics(ID3D11DeviceContext* immediateContext);
    void UpdateVertexDiagnosticsCapture(ID3D11DeviceContext* immediateContext);
    const char* GetSimulationStageName() const;
    int lastUpdateInputBuffer = -1;
    int lastUpdateOutputBuffer = -1;
    int lastConstraintInputBuffer = -1;
    int lastConstraintOutputBuffer = -1;
    int lastCopySourceBuffer = -1;
    UINT lastDispatchGroupCountX = 0;
    UINT lastVertexCount = 0;
    std::vector<VertexDiagnostic> lastVertexDiagnostics;
    ClothStretchDiagnostics lastClothStretchDiagnostics;
    std::string lastVertexDiagnosticStatus = "Not captured";
    Microsoft::WRL::ComPtr<ID3D11Buffer> pendingReadbackBuffer;
    Microsoft::WRL::ComPtr<ID3D11Query> pendingReadbackQuery;
    const Mesh::Primitive* pendingReadbackPrimitive = nullptr;
    UINT pendingReadbackSourceBuffer = 0;
    UINT pendingReadbackPollCount = 0;
    const Mesh::Primitive* lastSimulatedPrimitive = nullptr;

    void FetchNodes(const tinygltf::Model& gltf_model);
    void CumulateTransforms(std::vector<Node>& nodes);
    void FetchMeshes(ID3D11Device* device, const tinygltf::Model& gltf_model);
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    // 描画時に使用する
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> clothUpdateCS;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> clothConstraintCS;
    std::vector<Mesh::Vertex> allVertices;


public:
    struct ComputePrimitiveConstants
    {
        DirectX::XMFLOAT4X4 world;
        int material{ -1 };
        int hasTangent{ 0 };
        int skin{ -1 };
        int pad{ 0 };
        DirectX::XMFLOAT4X4 invWorld;
    };
    static_assert(sizeof(ComputePrimitiveConstants) == 144, "Cloth compute b0 size mismatch");
    static_assert(offsetof(ComputePrimitiveConstants, world) == 0, "Cloth compute b0 world offset mismatch");
    static_assert(offsetof(ComputePrimitiveConstants, material) == 64, "Cloth compute b0 material offset mismatch");
    static_assert(offsetof(ComputePrimitiveConstants, hasTangent) == 68, "Cloth compute b0 hasTangent offset mismatch");
    static_assert(offsetof(ComputePrimitiveConstants, skin) == 72, "Cloth compute b0 skin offset mismatch");
    static_assert(offsetof(ComputePrimitiveConstants, pad) == 76, "Cloth compute b0 padding offset mismatch");
    static_assert(offsetof(ComputePrimitiveConstants, invWorld) == 80, "Cloth compute b0 invWorld offset mismatch");

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
    static_assert(sizeof(GraphicsPrimitiveConstants) == 208, "Cloth graphics b0 size mismatch");
    static_assert(offsetof(GraphicsPrimitiveConstants, world) == 0, "Cloth graphics b0 world offset mismatch");
    static_assert(offsetof(GraphicsPrimitiveConstants, previousWorld) == 64, "Cloth graphics b0 previousWorld offset mismatch");
    static_assert(offsetof(GraphicsPrimitiveConstants, material) == 128, "Cloth graphics b0 material offset mismatch");
    static_assert(offsetof(GraphicsPrimitiveConstants, hasTangent) == 132, "Cloth graphics b0 hasTangent offset mismatch");
    static_assert(offsetof(GraphicsPrimitiveConstants, skin) == 136, "Cloth graphics b0 skin offset mismatch");
    static_assert(offsetof(GraphicsPrimitiveConstants, materialType) == 140, "Cloth graphics b0 materialType offset mismatch");
    static_assert(offsetof(GraphicsPrimitiveConstants, inverseTransposeWorld) == 144, "Cloth graphics b0 inverseTransposeWorld offset mismatch");

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> computePrimitiveCbuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> graphicsPrimitiveCbuffer;
    void FetchMaterials(ID3D11Device* device, const tinygltf::Model& gltf_model);
    void FetchTextures(ID3D11Device* device, const tinygltf::Model& gltf_model);

public:
    static const size_t PRIMITIVE_MAX_JOINTS = 512;
    struct PrimitiveJointConstants
    {
        DirectX::XMFLOAT4X4 matrices[PRIMITIVE_MAX_JOINTS];
    };

private:
    void CreateAndUploadResources(ID3D11Device* device);

    // 指定されたアライメントに合わせて数値を調整する関数
    UINT Align(UINT num, UINT alignment)
    {
        return (num + (alignment - 1)) & ~(alignment - 1);
    }

};
