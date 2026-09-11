#include "pch.h"
#include "MeshComponent.h"
#include "Core/Actor.h"

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

