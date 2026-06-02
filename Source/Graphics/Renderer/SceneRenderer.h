#pragma once
#include<d3d11.h>
#include <vector>
#include <memory>

#include "Core/Actor.h"
#include "Components/Render/MeshComponent.h"
#include "Components/CollisionShape/ShapeComponent.h"

#include "Graphics/Core/ConstantBuffer.h"
#include "Graphics/Core/PipelineLibrary.h"
#include "Engine/Camera/CameraConstants.h"

struct InstanceBatch
{
    InterleavedGltfModel* model;
    std::vector<InstanceMeshComponent*> instances;
};


struct RenderQueues
{
    std::vector<MeshComponent*> meshes;

    std::vector<MeshComponent*> shadowCasters;

    std::vector<InstanceBatch> instanceBatches;
};

class SceneRenderer
{
public:
    SceneRenderer()
    {
        ID3D11Device* device = Graphics::GetDevice();
        // 定数バッファ
        viewBuffer = std::make_unique<ConstantBuffer<ViewConstants>>(Graphics::GetDevice());
        primitiveJointCBuffer = std::make_unique<ConstantBuffer<PrimitiveJointConstants>>(Graphics::GetDevice());
        primitiveCBuffer = std::make_unique<ConstantBuffer<PrimitiveConstants>>(Graphics::GetDevice());

        // パイプラインステート
        pipeLineStateSet = std::make_unique<PipeLineStateSet>();
        pipeLineStateSet->InitStaticMesh(Graphics::GetDevice());
        pipeLineStateSet->InitSkeletalMesh(Graphics::GetDevice());
        
        HRESULT hr = CreatePsFromCSO(Graphics::GetDevice(), "./Data/Shaders/InstancePointLightModelPS.cso", pipeLineState.pixelShader.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    //コピーコンストラクタとコピー代入演算子を禁止にする
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    SceneRenderer(SceneRenderer&&) noexcept = delete;
    SceneRenderer& operator=(SceneRenderer&&) noexcept = delete;

    virtual ~SceneRenderer() = default;

    // View関連の定数バッファを更新する
    void UpdateViewConstants(ID3D11DeviceContext* immediateContext, const ViewConstants& data) const
    {
        viewBuffer->data = data;
        viewBuffer->Activate(immediateContext, 4);
    }

    void RenderOpaque(ID3D11DeviceContext* immediateContext, const std::vector<MeshComponent*>& items) const;

    void RenderMask(ID3D11DeviceContext* immediateContext, const std::vector<MeshComponent*>& items) const;

    void RenderBlend(ID3D11DeviceContext* immediateContext, const std::vector<MeshComponent*>& items) const;

    void RenderInstanced(ID3D11DeviceContext* immediateContext, const std::vector<InstanceBatch>& batches);

    void CastShadowRender(ID3D11DeviceContext* immediateContext, const std::vector<MeshComponent*>& items);

    void CastShadow(ID3D11DeviceContext* immediateContext, const MeshComponent* meshComponent, const DirectX::XMFLOAT4X4& world, const std::vector<InterleavedGltfModel::Node>& animatedNodes, InterleavedGltfModel::RenderPass pass);

    void CastShadowWithStaticBatching(ID3D11DeviceContext* immediateContext, const MeshComponent* meshComponent, const DirectX::XMFLOAT4X4& world, const std::vector<InterleavedGltfModel::Node>& animatedNodes) const;

    // メッシュを収集して振り分ける
    RenderQueues BuildRenderQueues();
private:
    void Draw(ID3D11DeviceContext* immediateContext, const MeshComponent* meshComponent, const DirectX::XMFLOAT4X4& world, const std::vector<InterleavedGltfModel::Node>& animatedNodes, InterleavedGltfModel::RenderPass pass) const;

    void DrawWithStaticBatching(ID3D11DeviceContext* immediateContext, const MeshComponent* meshComponent, const DirectX::XMFLOAT4X4& world, const std::vector<InterleavedGltfModel::Node>& animatedNodes, InterleavedGltfModel::RenderPass pass) const;

    void DrawCloth(ID3D11DeviceContext* immediateContext, const MeshComponent* meshComponent, const DirectX::XMFLOAT4X4& world, const std::vector<InterleavedGltfModel::Node>& animatedNodes, InterleavedGltfModel::RenderPass pass);
private:
    // カメラの定数バッファ
    std::unique_ptr<ConstantBuffer<ViewConstants>> viewBuffer;

    // パイプラインステート
    std::unique_ptr<PipeLineStateSet> pipeLineStateSet;

    // 
    static constexpr int PRIMITIVE_MAX_JOINTS = 512;
    struct PrimitiveJointConstants
    {
        DirectX::XMFLOAT4X4 matrices[PRIMITIVE_MAX_JOINTS];
    };
    std::unique_ptr<ConstantBuffer<PrimitiveJointConstants>> primitiveJointCBuffer;

    struct PrimitiveConstants
    {
        DirectX::XMFLOAT4X4 world;

        int material{ -1 };
        int hasTangent{ 0 };
        int skin{ -1 };
        int materialType = 0; // 0:通常 

        DirectX::XMFLOAT4X4 inverseTransposeWorld;  // 法線変換行列
    };
    std::unique_ptr<ConstantBuffer<PrimitiveConstants>> primitiveCBuffer;

public:
    // 今のRenderPath
    RenderPath currentRenderPath = RenderPath::Deferred;

private:

    std::vector<InterleavedGltfModel::InstanceData> instanceData;
    PipeLineStateDesc pipeLineState;
};

