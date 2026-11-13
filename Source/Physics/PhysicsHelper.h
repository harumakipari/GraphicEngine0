#pragma once

#include <DirectXMath.h>
#include <PxPhysicsAPI.h>

#include "Components/Transform/Transform.h"

namespace PhysicsHelper
{
    // 変換: Transform -> PxTransform
    inline physx::PxTransform ToPxTransform(const Transform& t)
    {
        using namespace physx;
        DirectX::XMFLOAT3 pos;
        DirectX::XMStoreFloat3(&pos, t.translation_);

        DirectX::XMFLOAT4 rot;
        DirectX::XMStoreFloat4(&rot, t.rotation_);

        return PxTransform(PxVec3(pos.x, pos.y, pos.z), PxQuat(rot.x, rot.y, rot.z, rot.w));
    }

    // 変換: PxTransform -> Transform
    inline Transform FromPxTransform(const physx::PxTransform& p)
    {
        // 位置と回転をセット、スケールは物理エンジンでは扱わないため (1,1,1) を使用
        return Transform(
            DirectX::XMVectorSet(p.p.x, p.p.y, p.p.z, 0.0f),
            DirectX::XMVectorSet(p.q.x, p.q.y, p.q.z, 0.0f),
            DirectX::XMVectorSet(1.0f, 1.0f, 1.0, 0.0f)
        );
    }

    // 変換: DirectX::XMFLOAT4X4 -> PxTransform
    inline physx::PxTransform ToPxTransform(const DirectX::XMFLOAT4X4& mat)
    {
        using namespace DirectX;

        XMMATRIX Mat = XMLoadFloat4x4(&mat);

        XMVECTOR scale;
        XMVECTOR rotationQuat;
        XMVECTOR translation;

        // Matrix から XmVector を作成
        XMMatrixDecompose(&scale, &rotationQuat, &translation, Mat);

        XMFLOAT3 pos;
        XMFLOAT4 rot;
        XMStoreFloat3(&pos, translation);
        XMStoreFloat4(&rot, rotationQuat);

        return physx::PxTransform(
            physx::PxVec3(pos.x, pos.y, pos.z),
            physx::PxQuat(rot.x, rot.y, rot.z, rot.w)
        );
    }
}
