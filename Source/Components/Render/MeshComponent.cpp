#include "pch.h"
#include "MeshComponent.h"
#include "Core/Actor.h"

DirectX::XMFLOAT3 MeshComponent::GetModelSize() const
{
    AABB aabb = model->GetAABB();
    return{ aabb.max.x - aabb.min.x,aabb.max.y - aabb.min.y,aabb.max.z - aabb.min.z };
}

void MeshComponent::DrawImGuiInspector() 
{
#ifdef USE_IMGUI
    SceneComponent::DrawImGuiInspector();
    if (ImGui::TreeNode((name + "  model").c_str()))
    {
        ImGui::Checkbox("isVisible", &isVisible);
        ImGui::SliderFloat(U8("色相調整"), &plusAlphaCBuffer->data.hueShift, -1.0f, +1.0);
        ImGui::SliderFloat(U8("彩度調整"), &plusAlphaCBuffer->data.saturation, -1.0f, +1.0);
        ImGui::SliderFloat(U8("明度調整"), &plusAlphaCBuffer->data.brightness, -1.0f, +1.0);
        ImGui::SliderFloat(U8("コントラスト調整"), &plusAlphaCBuffer->data.contrast, -1.0f, +1.0);
        ImGui::SliderFloat("dissolve", &plusAlphaCBuffer->data.dissolve, 0.0f, 1.0f);
        ImGui::ColorEdit4("cpuColor", &plusAlphaCBuffer->data.cpuColor.x);
        ImGui::SliderFloat("emissionPower", &plusAlphaCBuffer->data.emissionPower, 0.0f, 50.0f);
        ImGui::SliderFloat4("morphWeight", &plusAlphaCBuffer->data.morphWeights.x, 0.0f, 1.0f);
        ImGui::SliderFloat("value", &plusAlphaCBuffer->data.flashValue, 0.0f, 20.0f);
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


void SkeletalMeshComponent::SetRuntimeMaterialEmissive(const std::string& materialName,bool enabled, const DirectX::XMFLOAT3& color, float intensity)
{
    if (!model)
        return;

    if (!enabled && !runtimeMaterialResourceView)
        return;

    CreateRuntimeMaterialOverrideBuffer(Graphics::GetDevice());
    for (size_t materialIndex = 0; materialIndex < model->materials.size(); ++materialIndex)
    {
        if (model->materials[materialIndex].name != materialName)
            continue;

        auto& target = runtimeMaterialData[materialIndex];
        const auto& source = sourceMaterialData[materialIndex];
        if (enabled)
        {
            const float clampedIntensity = (std::max)(0.0f, intensity);
            target.emissiveFactor[0] = color.x * clampedIntensity;
            target.emissiveFactor[1] = color.y * clampedIntensity;
            target.emissiveFactor[2] = color.z * clampedIntensity;
        }
        else
        {
            target.emissiveFactor[0] = source.emissiveFactor[0];
            target.emissiveFactor[1] = source.emissiveFactor[1];
            target.emissiveFactor[2] = source.emissiveFactor[2];
        }
        runtimeMaterialOverridesDirty = true;
        return;
    }
}
void SkeletalMeshComponent::SetRuntimeMaterialRimLight(const std::string& materialName,const DirectX::XMFLOAT3& color, float power)
{
    if (!model || (power <= 0.0f && !runtimeMaterialResourceView))
        return;

    CreateRuntimeMaterialOverrideBuffer(Graphics::GetDevice());
    for (size_t materialIndex = 0; materialIndex < model->materials.size(); ++materialIndex)
    {
        if (model->materials[materialIndex].name != materialName)
            continue;

        auto& target = runtimeMaterialData[materialIndex];
        const auto& source = sourceMaterialData[materialIndex];
        const float clampedPower = (std::max)(0.0f, power);
        const DirectX::XMFLOAT3 targetColor = clampedPower > 0.0f ? color : source.rimColor;
        const float targetPower = clampedPower > 0.0f ? clampedPower : source.rimPower;
        if (target.rimColor.x == targetColor.x && target.rimColor.y == targetColor.y &&
            target.rimColor.z == targetColor.z && target.rimPower == targetPower)
        {
            return;
        }

        target.rimColor = targetColor;
        target.rimPower = targetPower;
        runtimeMaterialOverridesDirty = true;
        return;
    }
}
void SkeletalMeshComponent::ClearRuntimeMaterialRimLight(const std::string& materialName)
{
    SetRuntimeMaterialRimLight(materialName, {}, 0.0f);
}
void SkeletalMeshComponent::UpdateRuntimeMaterialOverrides(ID3D11DeviceContext* immediateContext) const
{
    if (!runtimeMaterialOverridesDirty || !runtimeMaterialBuffer)
        return;

    immediateContext->UpdateSubresource(runtimeMaterialBuffer.Get(), 0, nullptr,
                                        runtimeMaterialData.data(), 0, 0);
    runtimeMaterialOverridesDirty = false;
}
bool SkeletalMeshComponent::LoadPhase2BaseColorTexture(const std::string& materialName, const std::wstring& filename)
{
    if (materialName == "M_Grux_Qilin_Torso")
        return phase2BaseColorTexture.Load(Graphics::GetDevice(), filename);
    if (materialName == "M_Grux_Qilin_Gear")
        return phase2GearBaseColorTexture.Load(Graphics::GetDevice(), filename);
    if (materialName == "M_Grux_Qilin_Head")
        return phase2HeadBaseColorTexture.Load(Graphics::GetDevice(), filename);
    if (materialName == "M_Grux_Qilin_LegsHands")
        return phase2LegsHandsBaseColorTexture.Load(Graphics::GetDevice(), filename);
    return false;
}

void SkeletalMeshComponent::SetPhase2TextureBlendProgress(const float progress)
{
    phase2TextureBlendCBuffer->data.progress = std::clamp(progress, 0.0f, 1.0f);
}

void SkeletalMeshComponent::SetPhase2TextureBlendWorldYRange(const float worldMinY, const float worldMaxY,const float maskSoftness)
{
    phase2TextureBlendCBuffer->data.worldMinY = worldMinY;
    phase2TextureBlendCBuffer->data.worldMaxY = (std::max)(worldMaxY, worldMinY + 0.0001f);
    phase2TextureBlendCBuffer->data.maskSoftness = std::clamp(maskSoftness, 0.0f, 0.3f);
}

ID3D11ShaderResourceView* SkeletalMeshComponent::GetPhase2BaseColorTextureSRV(const std::string& materialName) const
{
    if (materialName == "M_Grux_Qilin_Torso") return phase2BaseColorTexture.GetSRV();
    if (materialName == "M_Grux_Qilin_Gear") return phase2GearBaseColorTexture.GetSRV();
    if (materialName == "M_Grux_Qilin_Head") return phase2HeadBaseColorTexture.GetSRV();
    if (materialName == "M_Grux_Qilin_LegsHands") return phase2LegsHandsBaseColorTexture.GetSRV();
    return nullptr;
}

void SkeletalMeshComponent::UpdateConstantBuffer(ID3D11DeviceContext* immediateContext) const
{
    phase2TextureBlendCBuffer->Activate(immediateContext, 6);
}


void SkeletalMeshComponent::SetModel(const std::string& filename, const bool isSaveVerticesData, const bool convertToLHS)
{
    ID3D11Device* device = Graphics::GetDevice();

    model = AssetManager::Get().LoadModel(device, filename, ModelTypes::ModelMode::SkeletalMesh, isSaveVerticesData, convertToLHS);
    modelNodes = model->GetNodes();
    runtimeMaterialData.clear();
    sourceMaterialData.clear();
    runtimeMaterialBuffer.Reset();
    runtimeMaterialResourceView.Reset();
    runtimeMaterialOverridesDirty = false;
}

Transform SkeletalMeshComponent::GetSocketTransform(int socketNode) const
{
    Transform new_transform = SceneComponent::GetSocketTransform(socketNode);
    if (socketNode > -1)
    {
        const InterleavedGltfModel::Node& node = modelNodes.at(socketNode);

        using namespace DirectX;

        XMMATRIX socket_transform = XMLoadFloat4x4(&node.globalTransform) * new_transform.ToMatrix();

        XMVECTOR scale;
        XMVECTOR rot;
        XMVECTOR trans;

        XMMatrixDecompose(&scale, &rot, &trans, socket_transform);

        new_transform.scale_ = XMVectorSet(1, 1, 1, 0);
        new_transform.rotation_ = rot;
        new_transform.translation_ = trans;
    }
    return new_transform;
}


void SkeletalMeshComponent::SetRenderLocalYOffsetProvider(const std::string& nodeName, std::function<float()> provider)
{
    renderOffsetNodeName = nodeName;
    renderLocalYOffsetProvider = std::move(provider);
}

int SkeletalMeshComponent::FindIndexByName(const std::string& name) const
{
    return model->FindNodeIndexByName(name);
}
void SkeletalMeshComponent::CastShadow(ID3D11DeviceContext* immediateContext, const DirectX::XMFLOAT4X4 world) const 
{
    model->CastShadow(immediateContext, world, GetRenderPoseNodes());
}
void SkeletalMeshComponent::CreateRuntimeMaterialOverrideBuffer(ID3D11Device* device)
{
    if (runtimeMaterialResourceView || !model || model->materials.empty())
        return;

    sourceMaterialData.clear();
    sourceMaterialData.reserve(model->materials.size());
    for (const InterleavedGltfModel::Material& material : model->materials)
        sourceMaterialData.emplace_back(material.data);
    runtimeMaterialData = sourceMaterialData;

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(InterleavedGltfModel::Material::Cbuffer) * runtimeMaterialData.size());
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(InterleavedGltfModel::Material::Cbuffer);

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = runtimeMaterialData.data();
    HRESULT hr = device->CreateBuffer(&bufferDesc, &initialData, runtimeMaterialBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
    viewDesc.Format = DXGI_FORMAT_UNKNOWN;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    viewDesc.Buffer.NumElements = static_cast<UINT>(runtimeMaterialData.size());
    hr = device->CreateShaderResourceView(runtimeMaterialBuffer.Get(), &viewDesc,
                                          runtimeMaterialResourceView.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

const std::vector<InterleavedGltfModel::Node>& SkeletalMeshComponent::GetRenderPoseNodes() const
{
    if (!model || !renderLocalYOffsetProvider) return modelNodes;
    const float offset = renderLocalYOffsetProvider();
    if (!std::isfinite(offset) || offset == 0.0f) return modelNodes;
    const int index = model->FindNodeIndexByName(renderOffsetNodeName);
    if (index < 0 || static_cast<size_t>(index) >= modelNodes.size()) return modelNodes;
    renderPoseNodes = modelNodes;
    renderPoseNodes[index].translation.y += offset;
    model->CumulateTransforms(renderPoseNodes);
    return renderPoseNodes;
}

void MainChargeWindMeshComponent::SetWindConstants(const DirectX::XMFLOAT2& scrollSpeed, const float opacity,const float radialSpeed, const float radialTiling, const int maskFlowMode) const
{
    constants.data.maskScrollSpeed = scrollSpeed;
    constants.data.opacity = opacity;
    constants.data.radialSpeed = radialSpeed;
    constants.data.radialTiling = radialTiling;
    constants.data.maskFlowMode = maskFlowMode;
}
