#include "pch.h"
#include "MeshComponent.h"
#include "Core/Actor.h"

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

void SkeletalMeshComponent::SetRuntimeMaterialEmissive(const std::string& materialName,
    bool enabled, const DirectX::XMFLOAT3& color, float intensity)
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

void SkeletalMeshComponent::ClearRuntimeMaterialEmissive(const std::string& materialName)
{
    SetRuntimeMaterialEmissive(materialName, false, {}, 0.0f);
}

void SkeletalMeshComponent::SetRuntimeMaterialRimLight(const std::string& materialName,
    const DirectX::XMFLOAT3& color, float power)
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
const std::vector<InterleavedGltfModel::Node>& SkeletalMeshComponent::GetRenderPoseNodes() const
{
    if (!model || !renderLocalYOffsetProvider) return modelNodes;
    const float offset = renderLocalYOffsetProvider();
    if (!std::isfinite(offset) || offset == 0.0f) return modelNodes;
    const int index = model->FindNodeIndexByName(renderOffsetNodeName);
    if (index < 0 || static_cast<size_t>(index) >= modelNodes.size()) return modelNodes;
    // Always rebuild from the unmodified shared pose, even between render passes.
    renderPoseNodes = modelNodes;
    renderPoseNodes[index].translation.y += offset;
    model->CumulateTransforms(renderPoseNodes);
    return renderPoseNodes;
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

