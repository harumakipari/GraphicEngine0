#include "pch.h"
#include "MeshComponent.h"
#include "Core/Actor.h"

Transform SkeletalMeshComponent::GetSocketTransform(int socketNode) const
{

#if 1
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

#endif


    if (socketNode < 0)
    {
        return componentToWorld_;
    }

    using namespace DirectX;

    // ボーンのグローバル行列
    //XMFLOAT4X4 boneMatrix = model->GetJointMatrix(socketNode, modelNodes,GetComponentWorldTransform().ToWorldTransform());
    XMFLOAT4X4 boneMatrix = model->GetJointLocalMatrix(socketNode, modelNodes);

    XMMATRIX bone = XMLoadFloat4x4(&boneMatrix);

    // キャラのワールド
    XMMATRIX world = componentToWorld_.ToMatrix();

    // ボーンワールド
    XMMATRIX boneWorld = XMMatrixMultiply(bone, world);
    // デバッグ用ソケット補正
    XMMATRIX offset =
        XMMatrixRotationZ(XMConvertToRadians(90.0f)) *
        XMMatrixTranslation(0.0f, 0.0f, 5.0f);

    boneWorld = offset * boneWorld;
    Transform t(boneWorld);
    return t;
}

