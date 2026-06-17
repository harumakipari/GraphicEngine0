#include "pch.h"
#include "MeshComponent.h"
#include "Core/Actor.h"

Transform SkeletalMeshComponent::GetSocketTransform(int socketNode) const
{
    Transform new_transform = SceneComponent::GetSocketTransform(socketNode);
    if (socketNode > -1)
    {
        const InterleavedGltfModel::Node& node = modelNodes.at(socketNode);

        using namespace DirectX;

        XMMATRIX C = XMMatrixSet
        (
            -1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        );

        XMMATRIX socket_transform =
            XMLoadFloat4x4(&node.globalTransform) *
            //C *
            new_transform.ToMatrix();

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

