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
    if (deferredMovementTick)
        return;

    TickMovement(deltaTime);
}

void CharacterMovementComponent::TickDeferredMovement(float deltaTime)
{
    if (!deferredMovementTick)
        return;

    TickMovement(deltaTime);
}

void CharacterMovementComponent::TickMovement(float deltaTime)
{
    if (useGravity)
    {
        gravity = -9.8f;
    }
    else
    {
        gravity = 0.0f;
    }

    // 入力方向を正規化して速度に反映
    DirectX::XMFLOAT3 wishDir = inputDir;
    wishDir.y = 0.0f;

    float len = sqrt(wishDir.x * wishDir.x + wishDir.z * wishDir.z);
    if (len > 0.001f)
    {
        wishDir.x /= len;
        wishDir.z /= len;
    }

    if (useFixedSpeed)
    {
        targetSpeed = speed;
    }
    else
    {
        targetSpeed = std::lerp(walkSpeed, runSpeed, inputMagnitude);
    }

    finalMoveSpeed = len > 0.001f
        ? targetSpeed * moveSpeedScale
        : 0.0f;
    velocity.x = wishDir.x * finalMoveSpeed;
    velocity.z = wishDir.z * finalMoveSpeed;

    // 外力を加算
    velocity.x += externalVelocity.x;
    velocity.y += externalVelocity.y;
    velocity.z += externalVelocity.z;

    // 減衰
    externalVelocity.x -= externalVelocity.x * damping * deltaTime;
    externalVelocity.y -= externalVelocity.y * damping * deltaTime;
    externalVelocity.z -= externalVelocity.z * damping * deltaTime;

    // 一定時間だけ強制移動する速度の処理
    if (forcedMoveTime > 0.0f)
    {
        velocity.x += forcedVelocity.x;
        velocity.y += forcedVelocity.y;
        velocity.z += forcedVelocity.z;

        forcedMoveTime -= deltaTime;

        if (forcedMoveTime <= 0.0f)
        {
            forcedVelocity = { 0,0,0 };
        }
    }

    if (moveToTarget)
    {
        auto targetShared = target.lock();

        if (!targetShared)
        {
            moveToTarget = false;
        }
        else
        {
            DirectX::XMFLOAT3 pos = owner.lock()->GetPosition();
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

            if (dist <= stopDistance)
            {
                moveToTarget = false;
            }
            else
            {
                dir.x /= dist;
                dir.z /= dist;

                float speed = (dist - stopDistance) / std::max<float>(moveToTargetTimer, 0.01f);

                velocity.x = dir.x * speed;
                velocity.z = dir.z * speed;
            }

            moveToTargetTimer -= deltaTime;
        }
    }

    // 重力加速度を適用
    if (!isGrounded)
    {
        velocity.y += gravity * deltaTime;
    }

    if (owner.expired())
    {
        Logger::Warning("owner is not existed so movement is nullptr!");
        _ASSERT("owner is not existed so movement is nullptr!");
    }
    auto owner = this->owner.lock();

    // 位置を予測
    DirectX::XMFLOAT3 pos = owner->GetPosition();
    DirectX::XMFLOAT3 nextPos = pos;

    nextPos.x += velocity.x * deltaTime;
    nextPos.y += velocity.y * deltaTime;
    nextPos.z += velocity.z * deltaTime;
    nextPos.x += frameAdditionalVelocity.x * deltaTime;
    nextPos.y += frameAdditionalVelocity.y * deltaTime;
    nextPos.z += frameAdditionalVelocity.z * deltaTime;
    frameAdditionalVelocity = { 0.0f, 0.0f, 0.0f };

    // 床との衝突判定
    isGrounded = false;

    float fallDistance = pos.y - nextPos.y;   // ← 重要：pos - nextPos

    float rayStartY = pos.y + groundOffset;
    float rayLength = groundOffset + std::max<float>(fallDistance, 0.0f) + 0.1f;

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
            velocity.y = 0;
            isGrounded = true;
        }
        Graphics::GetShapeRenderer()->DrawSphere(
            hit.position, 0.1f, { 1,0,0,1 });
    }

    // 壁とのレイキャスト
    DirectX::XMFLOAT3 horizontalMove = { nextPos.x - pos.x, 0.0f, nextPos.z - pos.z };
    lastRequestedHorizontalMoveForDebug = horizontalMove;
    lastResolvedHorizontalMoveForDebug = {};
    lastWallRayCastHitForDebug = false;
    lastStageWallRayCastHitForDebug = false;
    lastBossRoomWallSphereCastHitForDebug = false;
    lastWallProbeUsedSphereCastForDebug = false;
    lastWallProbeInitialOverlapForDebug = false;
    lastWallProbeHitLayerForDebug = 0;
    lastWallProbeOriginForDebug = {};
    lastWallProbeDirectionForDebug = {};
    lastWallProbeDistanceForDebug = 0.0f;
    HitResult bossRoomProbeHitForCharge{};
    bool bossRoomProbeInwardContactForCharge = false;
    if (bossRoomProbeChargeWallHitArmed)
        bossRoomProbeChargeWallHitEvent = {};
    const float dist = sqrtf(horizontalMove.x * horizontalMove.x + horizontalMove.z * horizontalMove.z);
    if (dist > 0.001f)
    {
        const DirectX::XMFLOAT3 moveDirection = { horizontalMove.x / dist, 0.0f, horizontalMove.z / dist };
        const uint32_t stageMask = CollisionHelper::ToBit(CollisionLayer::WorldStatic) | CollisionHelper::ToBit(CollisionLayer::WorldProps);
        const uint32_t bossMask = CollisionHelper::ToBit(CollisionLayer::WorldPropsNoRaycast);
        const uint32_t legacyMask = bossRoomWallProbeEnabled ? stageMask : stageMask | bossMask;
        HitResult stageHit;
        if (Physics::Instance().RayCast({ pos.x, pos.y + 1.0f, pos.z }, moveDirection, dist + radius, stageHit, legacyMask))
        {
            lastWallRayCastHitForDebug = true;
            lastStageWallRayCastHitForDebug = true;
            lastWallCollisionPositionForDebug = stageHit.position;
            nextPos.x = pos.x;
            nextPos.z = pos.z;
        }
        else if (bossRoomWallProbeEnabled)
        {
            const float probeRadius = GetActiveHorizontalWallProbeRadius();
            const DirectX::XMFLOAT3 probeOrigin = { pos.x, pos.y + (std::max)(1.0f, probeRadius + 0.05f), pos.z };
            HitResult wallHit;
            lastWallProbeUsedSphereCastForDebug = true;
            lastWallProbeOriginForDebug = probeOrigin;
            lastWallProbeDirectionForDebug = moveDirection;
            lastWallProbeDistanceForDebug = dist;
            if (Physics::Instance().SphereCast(probeOrigin, moveDirection, dist, probeRadius, wallHit, bossMask))
            {
                lastWallRayCastHitForDebug = true;
                lastBossRoomWallSphereCastHitForDebug = true;
                lastWallProbeInitialOverlapForDebug = wallHit.initialOverlap;
                lastWallProbeHitLayerForDebug = wallHit.layerMask;
                lastWallCollisionPositionForDebug = wallHit.position;
                if (!wallHit.initialOverlap)
                {
                    const float intoWall = horizontalMove.x * wallHit.normal.x + horizontalMove.z * wallHit.normal.z;
                    if (intoWall < -0.0001f)
                    {
                        bossRoomProbeInwardContactForCharge = true;
                        bossRoomProbeHitForCharge = wallHit;
                        DirectX::XMFLOAT3 slide = { horizontalMove.x - wallHit.normal.x * intoWall, 0.0f, horizontalMove.z - wallHit.normal.z * intoWall };
                        const float slideDistance = sqrtf(slide.x * slide.x + slide.z * slide.z);
                        if (slideDistance <= 0.001f)
                        {
                            nextPos.x = pos.x;
                            nextPos.z = pos.z;
                        }
                        else
                        {
                            slide.x /= slideDistance;
                            slide.z /= slideDistance;
                            HitResult slideHit;
                            float allowed = slideDistance;
                            if (Physics::Instance().SphereCast(probeOrigin, slide, slideDistance, probeRadius, slideHit, bossMask) && !slideHit.initialOverlap)
                                allowed = (std::max)(0.0f, slideHit.distance - 0.01f);
                            nextPos.x = pos.x + slide.x * allowed;
                            nextPos.z = pos.z + slide.z * allowed;
                        }
                    }
                }
            }
        }
    }
    lastResolvedHorizontalMoveForDebug = { nextPos.x - pos.x, 0.0f, nextPos.z - pos.z };
    if (bossRoomProbeChargeWallHitArmed && bossRoomProbeInwardContactForCharge &&
        !bossRoomProbeHitForCharge.initialOverlap && dist > 0.001f)
    {
        const DirectX::XMFLOAT3 moveDirection = { horizontalMove.x / dist, 0.0f, horizontalMove.z / dist };
        const float resolvedForward =
            lastResolvedHorizontalMoveForDebug.x * moveDirection.x +
            lastResolvedHorizontalMoveForDebug.z * moveDirection.z;
        const float forwardMoveLost = (std::max)(0.0f, dist - resolvedForward);
        if (forwardMoveLost > 0.01f)
        {
            bossRoomProbeChargeWallHitEvent.pending = true;
            bossRoomProbeChargeWallHitEvent.hitPosition = bossRoomProbeHitForCharge.position;
            bossRoomProbeChargeWallHitEvent.hitNormal = bossRoomProbeHitForCharge.normal;
            bossRoomProbeChargeWallHitEvent.hitLayer = bossRoomProbeHitForCharge.layerMask;
            bossRoomProbeChargeWallHitEvent.initialOverlap = false;
            bossRoomProbeChargeWallHitEvent.requestedMove = horizontalMove;
            bossRoomProbeChargeWallHitEvent.resolvedMove = lastResolvedHorizontalMoveForDebug;
            bossRoomProbeChargeWallHitEvent.forwardMoveLost = forwardMoveLost;
        }
    }
    if (deltaTime > FLT_EPSILON)
    {
        const float actualDeltaX = nextPos.x - pos.x;
        const float actualDeltaZ = nextPos.z - pos.z;
        actualHorizontalSpeed =
            sqrtf(actualDeltaX * actualDeltaX + actualDeltaZ * actualDeltaZ) /
            deltaTime;
    }
    else
    {
        actualHorizontalSpeed = 0.0f;
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
    if (IsSameDirection(dir, previousDirection))
        return;

    previousDirection = dir;
    direction = dir;

    // 補間の初期化
    lerpTime = 0.0f;
    startAngle = owner.lock()->GetEulerRotation();
    startRotation = owner.lock()->GetQuaternionRotation();
    float targetYaw = std::atan2f(dir.x, dir.z);
    startAngle.x = DirectX::XMConvertToRadians(startAngle.x);
    startAngle.z = DirectX::XMConvertToRadians(startAngle.z);

    DirectX::XMStoreFloat4(&targetRotation, DirectX::XMQuaternionRotationRollPitchYaw(startAngle.x, targetYaw, startAngle.z));
}

void RotationComponent::SetDirectionImmediate(const DirectX::XMFLOAT3& dir)
{
    if (fabs(dir.x) < 0.001f && fabs(dir.z) < 0.001f)
        return;

    SetDirection(dir);
    if (auto owner = this->owner.lock())
        owner->SetQuaternionRotation(targetRotation);
    lerpTime = rotateTime;
}
void RotationComponent::ForceDirectionImmediate(const DirectX::XMFLOAT3& dir)
{
    if (fabs(dir.x) < 0.001f && fabs(dir.z) < 0.001f)
        return;

    if (auto owner = this->owner.lock())
    {
        direction = dir;
        previousDirection = dir;
        const DirectX::XMFLOAT3 rotation = owner->GetEulerRotation();
        const float pitch = DirectX::XMConvertToRadians(rotation.x);
        const float roll = DirectX::XMConvertToRadians(rotation.z);
        const float yaw = std::atan2f(dir.x, dir.z);
        DirectX::XMStoreFloat4(
            &targetRotation,
            DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
        startRotation = targetRotation;
        startAngle = { rotation.x, DirectX::XMConvertToDegrees(yaw), rotation.z };
        lerpTime = rotateTime;
        owner->SetQuaternionRotation(targetRotation);
    }
}

void RotationComponent::SyncCurrentRotation()
{
    if (const auto owner = this->owner.lock())
    {
        const DirectX::XMFLOAT4 currentRotation = owner->GetQuaternionRotation();
        startRotation = currentRotation;
        targetRotation = currentRotation;
        lerpTime = rotateTime;
    }
}

bool RotationComponent::RotateTowardsDirection(
    const DirectX::XMFLOAT3& direction,
    const float maxDegreesPerSecond,
    const float deltaTime)
{
    if (auto owner = this->owner.lock())
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
        
        lerpTime = rotateTime;
        return std::abs(yawDelta) <= maxStep;
    }
    return false;
}

void RotationComponent::Tick(float deltaTime)
{
    if (lerpTime >= rotateTime)
        return; // 補間完了

    lerpTime += deltaTime;
    float t = lerpTime / rotateTime;
    t = std::min<float>(t, 1.0f);

    auto q = SlerpQuaternion(startRotation, targetRotation, t);
    owner.lock()->SetQuaternionRotation(q);
}

void InputComponent::Tick(float)
{
    intent_.leftMove = { 0,0,0 };
    intent_.rightMove = { 0.0f,0.0f };

    if (!InputSystem::IsInputEnabled())
        return;

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
