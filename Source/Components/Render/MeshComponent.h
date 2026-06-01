#pragma once

// C++ 標準ライブラリ
#include <memory>
#include <string>

// 他ライブラリ
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "../External/imgui/imgui.h"
#endif

// プロジェクトの他のヘッダ
#include "Graphics/Core/Graphics.h"
#include "Graphics/Core/Shader.h"
#include "Graphics/Core/PipelineState.h"
#include "Components/Base/SceneComponent.h"
#include "Core/Vector.h"
#include "Engine/Debug/DebugRender.h"
#include "Graphics/Resource/InterleavedGltfModel.h"
#include "Engine/Utility/Win32Utils.h"
#include "Game/Actors/WaterSphere/MorphModel.h"
#include "Graphics/Resource/AssetManager.h"
#include "Graphics/Resource/Texture.h"

class Actor;

//--　描画
class MeshComponent :public SceneComponent
{
public:
    PipeLineStateDesc pipeLineState_;   // これ使ってないから後で消す
    std::optional<std::string> overrideDeferredPipelineName;
    std::optional<std::string> overrideForwardPipelineName;
    std::optional<std::string> overrideCascadeShadowPipelineName;

    // このメッシュのレンダーパス
    enum class MeshRenderPass :uint8_t
    {
        Deferred,
        Forward
    };
    MeshRenderPass renderPass = MeshRenderPass::Deferred;
public:
    MeshComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner)
    {
        plusAlphaCBuffer = std::make_unique<ConstantBuffer<PlusAlphaConstants>>(Graphics::GetDevice());
    };
    std::shared_ptr<InterleavedGltfModel> model;

    void Tick(float deltaTime)override {}

    DirectX::XMFLOAT3 GetModelSize() const
    {
        AABB aabb = model->GetAABB();
        return{ aabb.max.x - aabb.min.x,aabb.max.y - aabb.min.y,aabb.max.z - aabb.min.z };
    }

    virtual void SetModel(const std::string& fileName, bool isSaveVerticesData = false, bool convertToLHS = false) = 0;

    virtual void Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world, InterleavedGltfModel::RenderPass pass) const = 0;

    virtual void CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world) const = 0;

    virtual void SetIsVisible(bool isVisible) { this->isVisible_ = isVisible; }

    virtual bool IsVisible() const { return isVisible_; }

    // 他のメッシュコンポーネントに必要な外部からの定数バッファ更新するためのフック関数
    virtual void UpdateConstantBuffer(ID3D11DeviceContext* immediateContext) const {}

    void UpdatePlusAlphaConstants(ID3D11DeviceContext* immediateContext) const
    {
        plusAlphaCBuffer->data.dissolve = dissolve;
        plusAlphaCBuffer->Activate(immediateContext, 5);
    }

    const std::vector<InterleavedGltfModel::Node>& GetNodes() const { return modelNodes; }

    void SetModelNodes(const std::vector<InterleavedGltfModel::Node>& nodes) { modelNodes = nodes; }

    virtual void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        SceneComponent::DrawImGuiInspector();
        if (ImGui::TreeNode((name_ + "  model").c_str()))
        {
            ImGui::Checkbox("isVisible", &isVisible_);
            ImGui::SliderFloat(U8("色相調整"), &plusAlphaCBuffer->data.hueShift, -1.0f, +1.0);
            ImGui::SliderFloat(U8("彩度調整"), &plusAlphaCBuffer->data.saturation, -1.0f, +1.0);
            ImGui::SliderFloat(U8("明度調整"), &plusAlphaCBuffer->data.brightness, -1.0f, +1.0);
            ImGui::SliderFloat(U8("コントラスト調整"), &plusAlphaCBuffer->data.contrast, -1.0f, +1.0);
            ImGui::SliderFloat("dissolve", &dissolve, 0.0f, 1.0f);
            ImGui::ColorEdit4("cpuColor", &plusAlphaCBuffer->data.cpuColor.x);
            ImGui::SliderFloat("emissionPower", &plusAlphaCBuffer->data.emissionPower, 0.0f, 50.0f);
            ImGui::SliderFloat4("morphWeight", &plusAlphaCBuffer->data.morphWeights.x, 0.0f, 1.0f);
            const char* objectTypes[] =
            {
                "Default",
                "Player",
                "Enemy",
                "Stage",
            };

            int type = static_cast<int>(plusAlphaCBuffer->data.objectType);

            if (ImGui::Combo("Render Step", &type, objectTypes, IM_ARRAYSIZE(objectTypes)))
            {
                plusAlphaCBuffer->data.objectType = static_cast<ObjectType>(type);
            }

            ImGui::TreePop();
        }
#endif
    }

    void SetPipeLineState(const PipeLineStateDesc& pipelinesState) { this->pipeLineState_ = pipelinesState; }

    PipeLineStateDesc GetPipeLineState()const { return pipeLineState_; }

    void SetIsCastShadow(bool isCastShadow) { this->isCastShadow_ = isCastShadow; }

    virtual bool IsCastShadow() const { return isCastShadow_; }

    virtual void OnRegister() override {}

    // 数値が大きいほうが後に描画される
    void SetPriority(int priority) { this->priority = priority; }
    int GetPriority() const { return priority; }

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
    };
    std::unique_ptr<ConstantBuffer<PlusAlphaConstants>> plusAlphaCBuffer;

    float   dissolve = 0.0f;   // ディゾルブ用
    float morphWeight = 0.0f;   // モーフモデルに使用する weight  0.0f ~ 1.0f 

protected:
    //描画するかどうか
    bool isVisible_ = true;
    // 影をつけるかどうか
    bool isCastShadow_ = true;
    // 描画優先度
    int priority = 0;
    // モデルのノード情報
    std::vector<InterleavedGltfModel::Node> modelNodes = {};


};

class SkeletalMeshComponent :public MeshComponent
{
public:
    SkeletalMeshComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :MeshComponent(name, owner)
    {
    }

    void SetModel(const std::string& filename, bool isSaveVerticesData = false, bool convertToLHS = false)override
    {
        ID3D11Device* device = Graphics::GetDevice();
        //model = std::make_shared<InterleavedGltfModel>(device, filename, ModelTypes::ModelMode::SkeletalMesh, isSaveVerticesData);
        model = AssetManager::Get().LoadModel(device, filename, ModelTypes::ModelMode::SkeletalMesh, isSaveVerticesData, convertToLHS);
        modelNodes = model->GetNodes();
    }


    Transform GetSocketTransform(int socketNode) const override;

    void AppendAnimations(const std::vector<std::string>& filenames) const
    {
        //model->AddAnimations(filenames);
        model->AppendAnimations(filenames);
    }

    void Tick(float deltaTime)override
    {
#ifdef _DEBUG
#endif
    }

    int FindIndexByName(const std::string& name) const
    {
        return model->FindNodeIndexByName(name);
    }

    void SetMaterialPS(const std::string& psFilename, const std::string& materialName) const
    {
        ID3D11Device* device = Graphics::GetDevice();
        for (InterleavedGltfModel::Material& material : model->materials)
        {
            if (material.name == materialName)
            {
                HRESULT hr = CreatePsFromCSO(device, psFilename.c_str(), material.replacedPixelShader.GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            }
        }
    }

    void Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world, InterleavedGltfModel::RenderPass pass) const override
    {
        model->Render(immediateContext, world, modelNodes, pass, pipeLineState_);
    }

    void CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world) const override
    {
        model->CastShadow(immediateContext, world, modelNodes);
    }

    DirectX::XMFLOAT3 GetJointWorldPosition(const std::string& name)
    {
        if (auto parent = attachParent_.lock())
        {
            DirectX::XMFLOAT4X4 parentWorld = parent->GetComponentWorldTransform().ToWorldTransform();
            return model->GetJointWorldPosition(name, modelNodes, parentWorld);
        }
        DirectX::XMFLOAT4X4 world = GetComponentWorldTransform().ToWorldTransform();
        return model->GetJointWorldPosition(name, modelNodes, world);
    }

private:

};

class StaticMeshComponent :public MeshComponent
{
public:
    StaticMeshComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :MeshComponent(name, owner)
    {
    }

    void SetModel(const std::string& filename, bool isSaveVerticesData = false, bool convertToLHS = false)override
    {
        ID3D11Device* device = Graphics::GetDevice();
        model = AssetManager::Get().LoadModel(device, filename, ModelTypes::ModelMode::StaticMesh, isSaveVerticesData, convertToLHS);
        modelNodes = model->GetNodes();
    }


    void Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world, InterleavedGltfModel::RenderPass pass) const override
    {
        model->Render(immediateContext, world, modelNodes, pass, pipeLineState_);
    }

    void CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world) const override
    {
        //const DirectX::XMFLOAT4X4 world = CreateWorldMatrix();
        model->CastShadow(immediateContext, world, modelNodes);
    }
};

class InstanceMeshComponent : public MeshComponent
{
public:
    InstanceMeshComponent(const std::string & name, const std::shared_ptr<Actor>&owner) :MeshComponent(name, owner)
    {
    }

    void SetModel(const std::string & filename, bool isSaveVerticesData = false, bool convertToLHS = false)override
    {
        ID3D11Device* device = Graphics::GetDevice();
        model = AssetManager::Get().LoadModel(device, filename, ModelTypes::ModelMode::InstancedStaticMesh, isSaveVerticesData, convertToLHS);
        modelNodes = model->GetNodes();
    }

    void Render(ID3D11DeviceContext * immediateContext, const DirectX::XMFLOAT4X4 world, InterleavedGltfModel::RenderPass pass) const override
    {
        model->Render(immediateContext, world, modelNodes, pass, pipeLineState_);
    }

    void CastShadow(ID3D11DeviceContext * immediateContext, const DirectX::XMFLOAT4X4 world) const override
    {
        model->CastShadow(immediateContext, world, modelNodes);
    }
};