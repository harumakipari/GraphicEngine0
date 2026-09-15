#include "pch.h"
#include "Character.h"

#include "Engine/Input/InputSystem.h"

void Character::HandleCommonAnimationNotifyEvent(const AnimationNotifyEvent& event)
{
    if (event.type != AnimationNotifyEvent::Type::ControllerRumble)
        return;

    InputSystem::RequestRumble(
        event.leftMotorStrength,
        event.rightMotorStrength,
        event.duration);
}

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
