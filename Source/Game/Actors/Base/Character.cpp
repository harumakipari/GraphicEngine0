#include "pch.h"
#include "Character.h"

void Character::UpdateDirectionVectors()
{
    using namespace DirectX;

    XMVECTOR q = XMLoadFloat4(&GetQuaternionRotation());

    XMStoreFloat3(
        &front,
        XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q));

    XMStoreFloat3(
        &right,
        XMVector3Rotate(XMVectorSet(1, 0, 0, 0), q));

    XMStoreFloat3(
        &up,
        XMVector3Rotate(XMVectorSet(0, 1, 0, 0), q));
}
