#pragma once
#include "Components/Render/MeshComponent.h"

struct ElasticForce
{
    DirectX::XMFLOAT3 point;   // 力が加わる位置（例：上端）
    DirectX::XMFLOAT3 dir;     // 方向
    float strength;            // 強さ
};



class ElasticMeshComponent :public MeshComponent
{
public:
    ElasticMeshComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :MeshComponent(name, owner)
    {
        overrideDeferredPipelineName = "elasticBuildingDeferred";
        overrideForwardPipelineName = "elasticBuildingForward";
        overrideCascadeShadowPipelineName = "CascadeShadowMapElasticBuilding";
    }

    ~ElasticMeshComponent() override = default;

    void SetModel(const std::string& filename, bool isSaveVerticesData = false, bool convertToLHS = false) override
    {
        ID3D11Device* device = Graphics::GetDevice();
        model = AssetManager::Get().LoadModel(device, filename, ModelTypes::ModelMode::SkeletalMesh, false, convertToLHS);
        modelNodes = model->GetNodes();
        int a = 0;
    }


    void Initialize() override;

    void Tick(float deltaTime)override;

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

    void UpdateConstantBuffer(ID3D11DeviceContext* immediateContext) const override;

    // サクランボのためにプリンの表面の位置を取得する関数
    void GetSurfacePositionTangent(DirectX::XMFLOAT3& surfacePosition, DirectX::XMFLOAT3& tangent);

    // 力が加わった時
    void AddImpulse(DirectX::XMFLOAT3 impulse);

    // 力をリセットする
    void ClearForce()
    {
        mouseForce = { 0.0f,0.0f,0.0f };
        cherryForce = { 0.0f,0.0f,0.0f };
    }

    void SetUseMouseInput(const bool useMouseInput) { this->useMouseInput = useMouseInput; }

    // --- このあたりの関数を使っていないから後程削除する ---
    void Render(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world, InterleavedGltfModel::RenderPass pass)const override
    {
        //model->Render(immediateContext, world, modelNodes, InterleavedGltfModel::RenderPass::Opaque, pipeLineState_);
    }
    void CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world) const override
    {
        //model->CastShadow(immediateContext, world, modelNodes);
    }

    DirectX::XMFLOAT3 GetJointWorldPosition(const std::string& name)
    {
        if (auto parent = attachParent_.lock())
        {
            DirectX::XMFLOAT4X4 parentWorld = parent->GetComponentWorldTransform().ToWorldTransform();
            return model->GetJointWorldPosition(name, modelNodes, parentWorld);
        }
        else
        {
            DirectX::XMFLOAT4X4 world = GetComponentWorldTransform().ToWorldTransform();
            return model->GetJointWorldPosition(name, modelNodes, world);
        }


        return { 0.0f,0.0f,0.0f };
    }

    virtual void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        SceneComponent::DrawImGuiInspector();
        if (ImGui::TreeNode((name_ + "  model").c_str()))
        {
            ImGui::Checkbox("isVisible", &isVisible_);
            ImGui::TreePop();
        }
        ImGui::DragFloat3("p2", &elasticConstants.p2.x, 0.1f);
        ImGui::DragFloat3("p3", &elasticConstants.p3.x, 0.1f);
        ImGui::SliderFloat("Stiffness", &elasticParameters.stiffness, 0.1f, 10.0f);
        ImGui::SliderFloat("Damping", &elasticParameters.damping, 0.1f, 0.99f);
        ImGui::SliderFloat("Mass", &elasticParameters.mass, 0.1f, 5.0f);
        ImGui::SliderFloat("momentumX", &elasticParameters.momentumX, -10.0f, 10.0f);
        ImGui::SliderFloat("momentumY", &elasticParameters.momentumY, -10.0f, 10.0f);
        ImGui::SliderFloat("momentumZ", &elasticParameters.momentumZ, -10.0f, 10.0f);
        ImGui::SliderFloat(U8("サクランボの伸び"), &elasticParameters.maxDist, -5.0f, 10.0f);
        ImGui::SliderFloat(" maxAngleDegrees", &elasticParameters.maxAngleDegrees, 0.0f, 360.0f);
        ImGui::DragFloat(U8("サクランボのオフセット"), &cherryOffset, 0.01f);
#endif
    }

    struct ElasticConstants
    {
        DirectX::XMFLOAT4 p1; // 始点
        DirectX::XMFLOAT4 p2; // 制御点
        DirectX::XMFLOAT4 p3; // 終点
        float maxAngleDegree; // 度以上曲がらない
        float modelHeight; // モデルの高さ
    };

    struct ElasticParameters
    {
        float stiffness = 10.0f;     // 硬さ（戻る力）
        float damping = 0.95f;    // 減衰
        float mass = 1.0f;     // 重さ（外力用）
        float maxAngleDegrees = 100.0f;     // 最大変形量
        float momentumX = 0.0f;
        float momentumY = 0.0f;
        float momentumZ = 0.0f;
        float maxDist = 3.45f;
    };

    struct PullInfo
    {
        bool active = false;
        float amount = 0.0f;   // 0~1
        float radianAngle = 0.0f;    // rad
    };

    const PullInfo& GetPullInfo() const { return pullInfo; }

    float GetModelHeight() const { return modelHeight; }


    void SetElasticEnabled(bool enabled);

    bool IsElasticEnabled() const { return elasticEnabled; }
private:
    void UpdatePushElastic(float deltaTime);

    bool UpdateFromMouse(float deltaTime);

    std::unique_ptr<ConstantBuffer<ElasticConstants>> elasticBuildingCBuffer;
    ElasticConstants elasticConstants{};
    ElasticParameters elasticParameters;
    float modelHeight = 0.0f;

    DirectX::XMFLOAT2 dragStartMousePos = { 0.0f,0.0f };
    float baseStretchRate = 1.0f;

    const float maxPullLength = 5.0f; // 最大引っ張り長さ
    DirectX::XMFLOAT3 grabPointWorld = { 0.0f,0.0f,0.0f };
    bool hasGrabPoint = false;

    float cherryOffset = -0.5f;// サクランボのためのオフセット

    PullInfo pullInfo;

    DirectX::XMFLOAT3 p3Target;   // 入力・外力で決まる目標位置
    DirectX::XMFLOAT3 p3Current;  // 実際に描画やシェーダに渡す p3（結果）
    DirectX::XMFLOAT3 p3Base;

    DirectX::XMFLOAT3 velocity = { 0.0f,0.0f,0.0f};

    DirectX::XMFLOAT3 mouseForce = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 cherryForce = { 0.0f,0.0f,0.0f };

    bool useMouseInput = true;

    bool elasticEnabled = true;
};

class ElasticPullController : public Component
{
public:
    ElasticPullController(const std::string& name, const std::shared_ptr<Actor>& owner) :Component(name, owner) {}

    ~ElasticPullController() override = default;


    void SetElasticMesh(ElasticMeshComponent* mesh)
    {
        elasticMesh = mesh;
    }

    void Tick(float dt) override;

private:
    ElasticMeshComponent* elasticMesh = nullptr;

    DirectX::XMFLOAT3 puddingTop{};
    DirectX::XMFLOAT3 pullDir{};
    float pullAmount = 0.0f;
};