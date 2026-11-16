#pragma once

#include <memory>
#include <functional>
#include <string>
#include <DirectXMath.h>
#include <d3d11.h>

#include "Graphics/Core/Graphics.h"
#include "Graphics/Core/Shader.h"
#include "Graphics/Core/PipelineState.h"
#include "Components/Base/SceneComponent.h"
#include "Graphics/Resource/InterleavedGltfModel.h"
#include "Graphics/Resource/AssetManager.h"
#include "Graphics/Resource/Texture.h"

class Actor;

//--　描画
class MeshComponent :public SceneComponent
{
public:
    // エフェクト用の構造体 
    struct EffectParameters
    {
        DirectX::XMFLOAT4 edgeColor;
        DirectX::XMFLOAT4 innerColor;

        float edgeWidth;
        float edgePower;
        float fresnelPower;
        float trailBlend;

        float noiseScale;
        float noiseSpeed;
    };

    // モデルごとに更新したいPlusAlpha 用定数バッファ
    struct PlusAlphaConstants
    {
        DirectX::XMFLOAT4 cpuColor; // 色をCPU側で指定する用　（ダメージ当たったときとか）

        float hueShift = 0.0f;	// 色相調整 -1 から 1 （-1 は負方向の 180 度、0 は変更なし、1 は正方向の 180 度）
        float saturation = 0.0f;	// 彩度調整（-1は濃灰、0は変化なし、1は最大彩度）
        float brightness = 0.0f;	// 明度調整（-1 は完全な黒、0 は変化なし、1 は完全な白）
        float   dissolve;   // ディゾルブ用

        DirectX::XMFLOAT4 morphWeights = { 0.0f,0.0f,0.0f,0.0f };  // モーフモデルに使用する weight 0.0f ~ 1.0f

        float contrast = 0.0f;  // コントラスト調整（-1は完全な灰色、0は変化なし、1は最大コントラスト）
        float emissionPower;    // 自己発光の強さ
        float flashValue = 0.0f; //　白くフラッシュする値
        ObjectType objectType = ObjectType::Default; // オブジェクトの種類

        EffectParameters effectParameters = {};

        float chargePower = 0.0f;
        float deathVisualFade = 0.0f;
        DirectX::XMFLOAT3 deathDeadEmissiveColor = { 0.35f, 0.35f, 0.35f };   // 敵の角などを光らせるのに使用する処理
    };
public:
    MeshComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner)
    {
        plusAlphaCBuffer = std::make_unique<ConstantBuffer<PlusAlphaConstants>>(Graphics::GetDevice());
    };

    DirectX::XMFLOAT3 GetModelSize() const;

    virtual void SetModel(const std::string& fileName, bool isSaveVerticesData = false, bool convertToLHS = false) = 0;

    virtual void CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world) const = 0;

    virtual void SetIsVisible(const bool isVisible) { this->isVisible = isVisible; }

    // 影用のモデルにする
    virtual void SetIsOnlyShadow(const bool isOnlyShadow) { this->isOnlyShadow = isOnlyShadow; }

    virtual bool IsVisible() const { return isVisible; }

    // 他のメッシュコンポーネントに必要な外部からの定数バッファ更新するためのフック関数
    virtual void UpdateConstantBuffer(ID3D11DeviceContext* immediateContext) const {}

    void UpdatePlusAlphaConstants(ID3D11DeviceContext* immediateContext) const
    {
        plusAlphaCBuffer->Activate(immediateContext, 5);
    }

    const std::vector<InterleavedGltfModel::Node>& GetNodes() const { return modelNodes; }
    virtual const std::vector<InterleavedGltfModel::Node>& GetRenderPoseNodes() const { return modelNodes; }

    void SetModelNodes(const std::vector<InterleavedGltfModel::Node>& nodes) { modelNodes = nodes; }

    void DrawImGuiInspector() override;

    void SetPipeLineState(const PipeLineStateDesc& pipelinesState) { this->pipeLineState = pipelinesState; }

    PipeLineStateDesc GetPipeLineState()const { return pipeLineState; }

    void SetIsCastShadow(const bool isCastShadow) { this->isCastShadow = isCastShadow; }

    void SetIsShadowMap(const bool isShadowMap) { this->isShadowMap = isShadowMap; }

    bool IsShadowMap() const { return isShadowMap; }

    virtual bool IsCastShadow() const { return isCastShadow; }

    // 影用のモデルかどうか
    virtual bool IsOnlyShadow() const { return isOnlyShadow; }

    // 数値が大きいほうが後に描画される
    void SetPriority(const int priority) { this->priority = priority; }
    int GetPriority() const { return priority; }

public:
    std::shared_ptr<InterleavedGltfModel> model;
    std::optional<std::string> overrideDeferredPipelineName;
    std::optional<std::string> overrideForwardPipelineName;
    std::optional<std::string> overrideCascadeShadowPipelineName;
    std::unique_ptr<ConstantBuffer<PlusAlphaConstants>> plusAlphaCBuffer;
protected:
    //描画するかどうか
    bool isVisible = true;
    // 影をつけるかどうか
    bool isCastShadow = true;
    // シャドウマップで影をつけるかどうか
    bool isShadowMap = false;
    // 影用のモデルかどうか
    bool isOnlyShadow = false;
    // 描画優先度
    int priority = 0;
    // モデルのノード情報
    std::vector<InterleavedGltfModel::Node> modelNodes = {};

private:
    PipeLineStateDesc pipeLineState;
};

class SkeletalMeshComponent :public MeshComponent
{
public:
    struct Phase2TextureBlendConstants
    {
        float progress = 0.0f;
        float worldMinY = 0.0f;
        float worldMaxY = 1.0f;
        float maskSoftness = 0.05f;
        int padding = 0;
        DirectX::XMFLOAT3 hairTint = { 1.0f, 1.0f, 1.0f };
    };

public:
    SkeletalMeshComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :MeshComponent(name, owner)
    {
        phase2TextureBlendCBuffer = std::make_unique<ConstantBuffer<Phase2TextureBlendConstants>>(Graphics::GetDevice());
    }

    void SetRuntimeMaterialEmissive(const std::string& materialName, bool enabled, const DirectX::XMFLOAT3& color, float intensity);
    void SetRuntimeMaterialRimLight(const std::string& materialName, const DirectX::XMFLOAT3& color, float power);
    void ClearRuntimeMaterialRimLight(const std::string& materialName);
    ID3D11ShaderResourceView* GetRuntimeMaterialResourceView() const { return runtimeMaterialResourceView.Get(); }
    void UpdateRuntimeMaterialOverrides(ID3D11DeviceContext* immediateContext) const;

    bool LoadPhase2BaseColorTexture(const std::string& materialName, const std::wstring& filename);
    void SetPhase2TextureBlendProgress(const float progress);
    void SetPhase2TextureBlendWorldYRange(const float worldMinY, const float worldMaxY, const float maskSoftness);
    ID3D11ShaderResourceView* GetPhase2BaseColorTextureSRV(const std::string& materialName) const;

    void UpdateConstantBuffer(ID3D11DeviceContext* immediateContext) const override;

    void SetModel(const std::string& filename, const bool isSaveVerticesData = false, const bool convertToLHS = false)override;

    Transform GetSocketTransform(int socketNode) const override;

    void SetRenderLocalYOffsetProvider(const std::string& nodeName, std::function<float()> provider);

    int FindIndexByName(const std::string& name) const;

    void CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world) const override;
    
private:
    void CreateRuntimeMaterialOverrideBuffer(ID3D11Device* device);

private:
    std::vector<InterleavedGltfModel::Material::Cbuffer> runtimeMaterialData;
    std::vector<InterleavedGltfModel::Material::Cbuffer> sourceMaterialData;
    Microsoft::WRL::ComPtr<ID3D11Buffer> runtimeMaterialBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> runtimeMaterialResourceView;
    mutable bool runtimeMaterialOverridesDirty = false;

    Texture phase2BaseColorTexture;
    Texture phase2GearBaseColorTexture;
    Texture phase2HeadBaseColorTexture;
    Texture phase2LegsHandsBaseColorTexture;
    std::unique_ptr<ConstantBuffer<Phase2TextureBlendConstants>> phase2TextureBlendCBuffer;

    const std::vector<InterleavedGltfModel::Node>& GetRenderPoseNodes() const override;
    std::string renderOffsetNodeName;
    std::function<float()> renderLocalYOffsetProvider;
    mutable std::vector<InterleavedGltfModel::Node> renderPoseNodes;
};

class StaticMeshComponent :public MeshComponent
{
public:
    StaticMeshComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :MeshComponent(name, owner) {}

    void SetModel(const std::string& filename, const bool isSaveVerticesData = false, const bool convertToLHS = false)override
    {
        ID3D11Device* device = Graphics::GetDevice();
        model = AssetManager::Get().LoadModel(device, filename, ModelTypes::ModelMode::StaticMesh, isSaveVerticesData, convertToLHS);
        modelNodes = model->GetNodes();
    }

    void CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world) const override
    {
        model->CastShadow(immediateContext, world, modelNodes);
    }
};

class InstanceMeshComponent : public MeshComponent
{
public:
    InstanceMeshComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :MeshComponent(name, owner) {}

    void SetModel(const std::string& filename, const bool isSaveVerticesData = false, const bool convertToLHS = false)override
    {
        ID3D11Device* device = Graphics::GetDevice();
        model = AssetManager::Get().LoadModel(device, filename, ModelTypes::ModelMode::InstancedStaticMesh, isSaveVerticesData, convertToLHS);
        modelNodes = model->GetNodes();
    }

    void CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world) const override
    {
        model->CastShadow(immediateContext, world, modelNodes);
    }
};

class MainChargeWindMeshComponent final : public StaticMeshComponent
{
public:
    struct Constants
    {
        DirectX::XMFLOAT2 maskScrollSpeed{};
        float opacity = 0.5f;
        float radialSpeed = 1.0f;
        float radialTiling = 2.0f;
        int maskFlowMode = 0;
        DirectX::XMFLOAT2 padding{};
    };

    MainChargeWindMeshComponent(const std::string& name, const std::shared_ptr<Actor>& owner) : StaticMeshComponent(name, owner), constants(Graphics::GetDevice()) {}

    void SetWindConstants(const DirectX::XMFLOAT2& scrollSpeed, const float opacity, const float radialSpeed, const float radialTiling, const int maskFlowMode) const;

    void UpdateConstantBuffer(ID3D11DeviceContext* immediateContext) const override
    {
        constants.Activate(immediateContext, 6);
    }

private:
    mutable ConstantBuffer<Constants> constants;
};
