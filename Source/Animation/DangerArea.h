#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

// Actor-local convention: X=Right/Width, Y=Up/Height, Z=Forward/Depth.
// size is full size; halfExtent is used for point tests.
struct DangerArea
{
    DirectX::XMFLOAT3 origin{}, center{};
    DirectX::XMFLOAT3 right{ 1,0,0 }, up{ 0,1,0 }, forward{ 0,0,1 };
    DirectX::XMFLOAT3 offset{}, size{}, halfExtent{};

    struct PointResult
    {
        float localRight = 0, localUp = 0, localForward = 0;
        bool rightInside = false, upInside = false, forwardInside = false;
        bool inside = false;
    };

    struct CapsuleResult
    {
        float localRight = 0, localUp = 0, localForward = 0;
        DirectX::XMFLOAT3 expandedHalfExtent{};
        bool rightInside = false, upInside = false, forwardInside = false;
        bool overlap = false;
    };

    PointResult ContainsPoint(const DirectX::XMFLOAT3& point) const
    {
        using namespace DirectX;
        const XMVECTOR delta = XMVectorSubtract(XMLoadFloat3(&point), XMLoadFloat3(&center));
        PointResult result;
        result.localRight = XMVectorGetX(XMVector3Dot(delta, XMLoadFloat3(&right)));
        result.localUp = XMVectorGetX(XMVector3Dot(delta, XMLoadFloat3(&up)));
        result.localForward = XMVectorGetX(XMVector3Dot(delta, XMLoadFloat3(&forward)));
        result.rightInside = std::abs(result.localRight) <= halfExtent.x;
        result.upInside = std::abs(result.localUp) <= halfExtent.y;
        result.forwardInside = std::abs(result.localForward) <= halfExtent.z;
        result.inside = result.rightInside && result.upInside && result.forwardInside;
        return result;
    }

    // Conservative overlap for the player's upright capsule. Height is the
    // capsule's full tip-to-tip height, so its vertical half extent is height/2.
    CapsuleResult IntersectsPlayerCapsule(const DirectX::XMFLOAT3& capsuleCenter,
        float capsuleRadius, float capsuleHeight) const
    {
        using namespace DirectX;
        const float radius = (std::max)(0.0f, capsuleRadius);
        const float halfHeight = (std::max)(radius, capsuleHeight * 0.5f);
        const XMVECTOR delta = XMVectorSubtract(XMLoadFloat3(&capsuleCenter), XMLoadFloat3(&center));

        CapsuleResult result;
        result.localRight = XMVectorGetX(XMVector3Dot(delta, XMLoadFloat3(&right)));
        result.localUp = XMVectorGetX(XMVector3Dot(delta, XMLoadFloat3(&up)));
        result.localForward = XMVectorGetX(XMVector3Dot(delta, XMLoadFloat3(&forward)));
        result.expandedHalfExtent = {
            halfExtent.x + radius,
            halfExtent.y + halfHeight,
            halfExtent.z + radius
        };
        result.rightInside = std::abs(result.localRight) <= result.expandedHalfExtent.x;
        result.upInside = std::abs(result.localUp) <= result.expandedHalfExtent.y;
        result.forwardInside = std::abs(result.localForward) <= result.expandedHalfExtent.z;
        result.overlap = result.rightInside && result.upInside && result.forwardInside;
        return result;
    }

    DirectX::XMFLOAT4X4 WorldTransform() const
    {
        return { right.x,right.y,right.z,0, up.x,up.y,up.z,0,
            forward.x,forward.y,forward.z,0, center.x,center.y,center.z,1 };
    }
};

inline DangerArea BuildDangerArea(const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& right, const DirectX::XMFLOAT3& up,
    const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& offset,
    const DirectX::XMFLOAT3& size)
{
    using namespace DirectX;
    DangerArea area;
    area.origin=origin; area.right=right; area.up=up; area.forward=forward; area.offset=offset;
    area.size={std::abs(size.x),std::abs(size.y),std::abs(size.z)};
    area.halfExtent={area.size.x*.5f,area.size.y*.5f,area.size.z*.5f};
    XMVECTOR center=XMLoadFloat3(&origin);
    center=XMVectorMultiplyAdd(XMLoadFloat3(&right),XMVectorReplicate(offset.x),center);
    center=XMVectorMultiplyAdd(XMLoadFloat3(&up),XMVectorReplicate(offset.y),center);
    center=XMVectorMultiplyAdd(XMLoadFloat3(&forward),XMVectorReplicate(offset.z),center);
    XMStoreFloat3(&area.center,center);
    return area;
}
