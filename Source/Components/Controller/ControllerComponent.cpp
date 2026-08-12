#include "pch.h"
#include "ControllerComponent.h"

#include <algorithm>

#include "Core/Actor.h"
#include "Core/ActorManager.h"
#include "Engine/Scene/Scene.h"
#include "Game/Actors/Base/Character.h"
#include "Engine/Utility/Win32Utils.h"
#include "Physics/CollisionSystem.h"

void CharacterMovementComponent::Tick(float deltaTime)
{
    if (deferredMovementTick_)
        return;

    TickMovement(deltaTime);
}

void CharacterMovementComponent::TickDeferredMovement(float deltaTime)
{
    if (!deferredMovementTick_)
        return;

    TickMovement(deltaTime);
}

void CharacterMovementComponent::TickMovement(float deltaTime)
{
    if (useGravity)
    {
        gravity_ = -9.8f;
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

    if (useFixedSpeed)
    {
        targetSpeed = speed_;
    }
    else
    {
        targetSpeed = std::lerp(walkSpeed, runSpeed, inputMagnitude);
    }

    finalMoveSpeed_ = len > 0.001f
        ? targetSpeed * moveSpeedScale_
        : 0.0f;
    velocity_.x = wishDir.x * finalMoveSpeed_;
    velocity_.z = wishDir.z * finalMoveSpeed_;

    // 外力を加算
    velocity_.x += externalVelocity_.x;
    velocity_.y += externalVelocity_.y;
    velocity_.z += externalVelocity_.z;

    // 減衰
    externalVelocity_.x -= externalVelocity_.x * damping_ * deltaTime;
    externalVelocity_.y -= externalVelocity_.y * damping_ * deltaTime;
    externalVelocity_.z -= externalVelocity_.z * damping_ * deltaTime;

    // 一定時間だけ強制移動する速度の処理
    if (forcedMoveTime_ > 0.0f)
    {
        velocity_.x += forcedVelocity_.x;
        velocity_.y += forcedVelocity_.y;
        velocity_.z += forcedVelocity_.z;

        forcedMoveTime_ -= deltaTime;

        if (forcedMoveTime_ <= 0.0f)
        {
            forcedVelocity_ = { 0,0,0 };
        }
    }

    if (moveToTarget_)
    {
        auto targetShared = target.lock();

        if (!targetShared)
        {
            moveToTarget_ = false;
        }
        else
        {
            DirectX::XMFLOAT3 pos = owner_.lock()->GetPosition();
            DirectX::XMFLOAT3 targetPos = targetShared->GetPosition();

            DirectX::XMFLOAT3 dir =
            {
                targetPos.x - pos.x,
                0.0f,
                targetPos.z - pos.z
            };

            float dist =
                sqrtf(dir.x * dir.x +
                    dir.z * dir.z);

            if (dist <= stopDistance_)
            {
                moveToTarget_ = false;
            }
            else
            {
                dir.x /= dist;
                dir.z /= dist;

                float speed = (dist - stopDistance_) / std::max<float>(moveToTargetTimer_, 0.01f);

                velocity_.x = dir.x * speed;
                velocity_.z = dir.z * speed;
            }

            moveToTargetTimer_ -= deltaTime;
        }
    }

    // 重力加速度を適用
    if (!isGrounded_)
    {
        velocity_.y += gravity_ * deltaTime;
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

    nextPos.x += velocity_.x * deltaTime;
    nextPos.y += velocity_.y * deltaTime;
    nextPos.z += velocity_.z * deltaTime;
    nextPos.x += frameAdditionalVelocity_.x * deltaTime;
    nextPos.y += frameAdditionalVelocity_.y * deltaTime;
    nextPos.z += frameAdditionalVelocity_.z * deltaTime;
    frameAdditionalVelocity_ = { 0.0f, 0.0f, 0.0f };

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
        CollisionHelper::ToBit(CollisionLayer::WorldStatic)))
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
    DirectX::XMFLOAT3 horizontalMove =
    {
        nextPos.x - pos.x,
        0,
        nextPos.z - pos.z
    };

    float dist = sqrt(horizontalMove.x * horizontalMove.x + horizontalMove.z * horizontalMove.z);

    if (dist > 0.001f)
    {
        horizontalMove.x /= dist;
        horizontalMove.z /= dist;

        HitResult wallHit;
        uint32_t mask = CollisionHelper::ToBit(CollisionLayer::WorldStatic) | CollisionHelper::ToBit(CollisionLayer::WorldPropsNoRaycast)
            | CollisionHelper::ToBit(CollisionLayer::WorldProps);

        if (Physics::Instance().RayCast(
            { pos.x, pos.y + 1.0f, pos.z },
            horizontalMove,
            dist + radius_,
            wallHit,
            mask))
        {
            nextPos.x = pos.x;
            nextPos.z = pos.z;
        }
    }

    if (deltaTime > FLT_EPSILON)
    {
        const float actualDeltaX = nextPos.x - pos.x;
        const float actualDeltaZ = nextPos.z - pos.z;
        actualHorizontalSpeed_ =
            sqrtf(actualDeltaX * actualDeltaX + actualDeltaZ * actualDeltaZ) /
            deltaTime;
    }
    else
    {
        actualHorizontalSpeed_ = 0.0f;
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

void RotationComponent::SetDirectionImmediate(const DirectX::XMFLOAT3& dir)
{
    // Keep the current rotation for a zero-length XZ direction.
    if (fabs(dir.x) < 0.001f && fabs(dir.z) < 0.001f)
        return;

    SetDirection(dir);
    if (auto owner = owner_.lock())
        owner->SetQuaternionRotation(targetRotation_);
    lerpTime_ = rotateTime_;
}
bool RotationComponent::RotateTowardsDirection(
    const DirectX::XMFLOAT3& direction,
    const float maxDegreesPerSecond,
    const float deltaTime)
{
    if (auto owner = owner_.lock())
    {
        const float directionLengthSq =
            direction.x * direction.x + direction.z * direction.z;
        if (directionLengthSq <= FLT_EPSILON)
            return true;

        const float targetYaw = std::atan2f(direction.x, direction.z);
        DirectX::XMVECTOR currentForward = DirectX::XMVector3Rotate(
            DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
            DirectX::XMLoadFloat4(&owner->GetQuaternionRotation()));
        DirectX::XMFLOAT3 forward{};
        DirectX::XMStoreFloat3(&forward, currentForward);
        const float currentYaw = std::atan2f(forward.x, forward.z);
        const float yawDelta = std::atan2f(
            std::sinf(targetYaw - currentYaw),
            std::cosf(targetYaw - currentYaw));
        const float maxStep = DirectX::XMConvertToRadians(
            (std::max)(0.0f, maxDegreesPerSecond)) * (std::max)(0.0f, deltaTime);
        const float appliedDelta = std::clamp(yawDelta, -maxStep, maxStep);
        const float newYaw = currentYaw + appliedDelta;

        DirectX::XMFLOAT4 rotation{};
        DirectX::XMStoreFloat4(&rotation,
            DirectX::XMQuaternionRotationRollPitchYaw(0.0f, newYaw, 0.0f));
        owner->SetQuaternionRotation(rotation);
        // Do not let a pending SetDirection interpolation overwrite this rotation.
        lerpTime_ = rotateTime_;
        return std::abs(yawDelta) <= maxStep;
    }
    return false;
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
    }
}
