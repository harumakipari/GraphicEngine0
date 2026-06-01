#include "pch.h"
#include "ControllerComponent.h"

#include <algorithm>

#include "Core/Actor.h"
#include "Core/ActorManager.h"
#include "Engine/Scene/Scene.h"
#include "Game/Actors/Base/Character.h"
#include "Engine/Utility/Win32Utils.h"
#include "Physics/CollisionSystem.h"

void CharacterMovementComponent::Tick(float dt)
{
    if (useGravity)
    {
        gravity_ = -4.9f;
    }
    else
    {
        gravity_ = 0.0f;
    }

    // 入力方向を正規化して速度に反映
    DirectX::XMFLOAT3 wishDir = inputDir_;
    wishDir.y = 0.0f;

    float len = sqrt(wishDir.x * wishDir.x + wishDir.z * wishDir.z);
    if (len > 0.001f)
    {
        wishDir.x /= len;
        wishDir.z /= len;
    }

    velocity_.x = wishDir.x * speed_;
    velocity_.z = wishDir.z * speed_;

    // 外力を加算
    velocity_.x += externalVelocity_.x;
    velocity_.y += externalVelocity_.y;
    velocity_.z += externalVelocity_.z;

    // 減衰
    externalVelocity_.x -= externalVelocity_.x * damping_ * dt;
    externalVelocity_.y -= externalVelocity_.y * damping_ * dt;
    externalVelocity_.z -= externalVelocity_.z * damping_ * dt;

    // 重力加速度を適用
    if (!isGrounded_)
    {
        velocity_.y += gravity_ * dt;
    }

    if (owner_.expired())
    {
        Logger::Warning("owner is not existed so movement is nullptr!");
        _ASSERT("owner is not existed so movement is nullptr!");
    }
    auto owner = owner_.lock();

    // 位置を予測
    DirectX::XMFLOAT3 pos = owner->GetPosition();
    DirectX::XMFLOAT3 nextPos = pos;

    nextPos.x += velocity_.x * dt;
    nextPos.y += velocity_.y * dt;
    nextPos.z += velocity_.z * dt;

    // 床との衝突判定
    isGrounded_ = false;

    float fallDistance = pos.y - nextPos.y;   // ← 重要：pos - nextPos

    float rayStartY = pos.y + groundOffset_;
    float rayLength = groundOffset_ + std::max<float>(fallDistance, 0.0f) + 0.1f;

    HitResult hit;
    if (Physics::Instance().RayCast(
        { pos.x, rayStartY, pos.z },
        { 0,-1,0 },
        rayLength,
        hit,
        CollisionHelper::ToBit(CollisionLayer::Floor)))
    {
        float groundY = hit.position.y;

        if (nextPos.y <= groundY)
        {
            nextPos.y = groundY;
            velocity_.y = 0;
            isGrounded_ = true;
        }
        Graphics::GetShapeRenderer()->DrawSphere(
            hit.position, 0.1f, { 1,0,0,1 });
    }

    // 壁とのレイキャスト
    DirectX::XMFLOAT3 horizMove =
    {
        nextPos.x - pos.x,
        0,
        nextPos.z - pos.z
    };

    float dist = sqrt(horizMove.x * horizMove.x + horizMove.z * horizMove.z);

    if (dist > 0.001f)
    {
        horizMove.x /= dist;
        horizMove.z /= dist;

        HitResult wallHit;
        if (Physics::Instance().RayCast(
            { pos.x, pos.y + 1.0f, pos.z },
            horizMove,
            dist + radius_,
            wallHit,
            CollisionHelper::ToBit(CollisionLayer::WorldStatic)))
        {
            nextPos.x = pos.x;
            nextPos.z = pos.z;
        }
    }

    // 位置を更新
    owner->SetPosition(nextPos);
}



void RotationComponent::SetDirection(const DirectX::XMFLOAT3& dir)
{
    // 入力がゼロなら何もしない（向きを維持）
    if (fabs(dir.x) < 0.001f && fabs(dir.z) < 0.001f)
        return;

    // 方向に変化がなければ何もしない
    if (IsSameDirection(dir, previousDirection_))
        return;

    previousDirection_ = dir;
    direction_ = dir;

    // 補間の初期化
    lerpTime_ = 0.0f;
    startAngle_ = owner_.lock()->GetEulerRotation();
    startRotation_ = owner_.lock()->GetQuaternionRotation();
    float targetYaw = std::atan2f(dir.x, dir.z);
    startAngle_.x = DirectX::XMConvertToRadians(startAngle_.x);
    startAngle_.z = DirectX::XMConvertToRadians(startAngle_.z);

    DirectX::XMStoreFloat4(&targetRotation_, DirectX::XMQuaternionRotationRollPitchYaw(startAngle_.x, targetYaw, startAngle_.z));
}

void RotationComponent::Tick(float deltaTime)
{
    if (lerpTime_ >= rotateTime_)
        return; // 補間完了

    lerpTime_ += deltaTime;
    float t = lerpTime_ / rotateTime_;
    t = std::min<float>(t, 1.0f);

    auto q = SlerpQuaternion(startRotation_, targetRotation_, t);
    owner_.lock()->SetQuaternionRotation(q);
}

void InputComponent::Tick(float)
{
    intent_.leftMove = { 0,0,0 };
    intent_.rightMove = { 0.0f,0.0f };
    auto scene = Scene::GetCurrentScene();
    if (!scene)
    {
        Logger::Warning(Logger::LogCategory::System, U8("InputComponent で　scene が null です！"));
    }

    if (!scene->GetCameraManager()->IsUseDebug() &&
        !scene->GetCameraManager()->IsUseCinematic() && !scene->GetCameraManager()->IsUseMovie())
    {
        // 左スティック
        float lx = InputSystem::GetLeftStick().x;
        float ly = InputSystem::GetLeftStick().y;

        intent_.leftMove.x = lx;
        intent_.leftMove.z = ly;

        // 右スティック
        float rx = InputSystem::GetRightStick().x;
        float ry = InputSystem::GetRightStick().y;

        intent_.rightMove.x = rx;
        intent_.rightMove.y = ry;

#if 0
        if (InputSystem::GetInputState("W"))
        {
            intent_.move.z += 1.0f;
        }
        if (InputSystem::GetInputState("S"))
        {
            intent_.move.z -= 1.0f;
        }
        if (InputSystem::GetInputState("D"))
        {
            intent_.move.x += 1.0f;
        }
        if (InputSystem::GetInputState("A"))
        {
            intent_.move.x -= 1.0f;
        }

#endif // 0
    }
}