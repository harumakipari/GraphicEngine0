#include "pch.h"
#include "ControllerComponent.h"

#include <algorithm>

#include "Core/Actor.h"
#include "Core/ActorManager.h"
#include "Engine/Scene/Scene.h"
#include "Game/Actors/Base/Character.h"
#include "Engine/Utility/Win32Utils.h"
#include "Physics/CollisionSystem.h"

void InputComponent::Tick(float)
{
    intent.leftMove = { 0,0,0 };
    intent.rightMove = { 0.0f,0.0f };

    if (!InputSystem::IsInputEnabled())
        return;

    auto scene = Scene::GetCurrentScene();
    if (!scene)
    {
        Logger::Warning(Logger::LogCategory::System, U8("InputComponent で　scene が null です！"));
        return;
    }

    if (!scene->GetCameraManager()->IsUseDebug() &&
        !scene->GetCameraManager()->IsUseCinematic() && !scene->GetCameraManager()->IsUseMovie())
    {
        // 左スティック
        float lx = InputSystem::GetLeftStick().x;
        float ly = InputSystem::GetLeftStick().y;

        intent.leftMove.x = lx;
        intent.leftMove.z = ly;

        // 右スティック
        float rx = InputSystem::GetRightStick().x;
        float ry = InputSystem::GetRightStick().y;

        intent.rightMove.x = rx;
        intent.rightMove.y = ry;
    }
}

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



void CharacterMovementComponent::DrawImGuiInspector()
{
#ifdef USE_IMGUI
    SceneComponent::DrawImGuiInspector();
    ImGui::DragFloat(U8("歩く速度"), &walkSpeed, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat(U8("走り速度"), &runSpeed, 0.05f, 0.0f, 10.0f);

#endif
}

bool CharacterMovementComponent::ConsumeBossRoomProbeChargeWallHitEvent(BossRoomProbeMoveBlockEvent& outEvent)
{
    if (!bossRoomProbeChargeWallHitEvent.pending) return false;
    outEvent = bossRoomProbeChargeWallHitEvent;
    bossRoomProbeChargeWallHitEvent = {};
    return true;
}
// 入力によって変わる現在の速さを取得
float CharacterMovementComponent::GetCurrentInputNormalizeSpeed()const
{
    // 0除算を防ぐため
    float speed = targetSpeed / std::max<float>(0.00001f, runSpeed);
    return speed;
}

// 一定時間だけ強制移動する速度を設定するための関数  ルートモーションではないアニメーションなどに使用する
void CharacterMovementComponent::AddForcedMove(const DirectX::XMFLOAT3& direction, const float speed, const float duration)
{
    forcedVelocity =
    {
        direction.x * speed,
        direction.y * speed,
        direction.z * speed
    };

    forcedMoveTime = duration;
}

// targetまで進む時に使用する
void  CharacterMovementComponent::MoveToActor(const std::shared_ptr<Actor>& target, const float moveTime, float stopDistance)
{
    this->target = target;
    moveToTargetTimer = moveTime;
    this->stopDistance = stopDistance;
    moveToTarget = true;
}

void CharacterMovementComponent::TickMovement(float deltaTime)
{
    auto owner = this->owner.lock();
    if (!owner)
    {
        Logger::Warning("owner is not existed so movement is nullptr!");
        _ASSERT("owner is not existed so movement is nullptr!");
        return;
    }

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

    finalMoveSpeed = len > 0.001f ? targetSpeed * moveSpeedScale : 0.0f;
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
            DirectX::XMFLOAT3 pos = owner->GetPosition();
            DirectX::XMFLOAT3 targetPos = targetShared->GetPosition();

            DirectX::XMFLOAT3 dir =
            {
                targetPos.x - pos.x,
                0.0f,
                targetPos.z - pos.z
            };

            float dist = sqrtf(dir.x * dir.x + dir.z * dir.z);

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
    if (Physics::Instance().RayCast({ pos.x, rayStartY, pos.z }, { 0,-1,0 }, rayLength, hit, CollisionHelper::ToBit(CollisionLayer::WorldStatic)))
    {
        float groundY = hit.position.y;

        if (nextPos.y <= groundY)
        {
            nextPos.y = groundY;
            velocity.y = 0;
            isGrounded = true;
        }
        Graphics::GetShapeRenderer()->DrawSphere(hit.position, 0.1f, { 1,0,0,1 });
    }

    // 壁とのレイキャスト
    DirectX::XMFLOAT3 horizontalMove = { nextPos.x - pos.x, 0.0f, nextPos.z - pos.z };
    lastResolvedHorizontalMoveForDebug = {};
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
            nextPos.x = pos.x;
            nextPos.z = pos.z;
        }
        else if (bossRoomWallProbeEnabled)
        {
            const float probeRadius = GetActiveHorizontalWallProbeRadius();
            const DirectX::XMFLOAT3 probeOrigin = { pos.x, pos.y + (std::max)(1.0f, probeRadius + 0.05f), pos.z };
            HitResult wallHit;
            if (Physics::Instance().SphereCast(probeOrigin, moveDirection, dist, probeRadius, wallHit, bossMask))
            {
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
        const float resolvedForward = lastResolvedHorizontalMoveForDebug.x * moveDirection.x + lastResolvedHorizontalMoveForDebug.z * moveDirection.z;
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

    // 位置を更新
    owner->SetPosition(nextPos);
}
void RotationComponent::Tick(float deltaTime)
{
    if (lerpTime >= rotateTime)
        return; // 補間完了

    auto owner = this->owner.lock();
    if (!owner)
        return;

    lerpTime += deltaTime;
    float t = lerpTime / rotateTime;
    t = std::min<float>(t, 1.0f);

    auto q = SlerpQuaternion(startRotation, targetRotation, t);
    owner->SetQuaternionRotation(q);
}


void RotationComponent::DrawImGuiInspector()
{
#ifdef USE_IMGUI
    SceneComponent::DrawImGuiInspector();
    ImGui::DragFloat(U8("回転時間"), &rotateTime, 0.05f, 0.0f, 10.0f);

#endif
}

void RotationComponent::SetDirection(const DirectX::XMFLOAT3& dir)
{
    // 入力がゼロなら何もしない（向きを維持）
    if (fabs(dir.x) < 0.001f && fabs(dir.z) < 0.001f)
        return;

    // 方向に変化がなければ何もしない
    if (IsSameDirection(dir, previousDirection))
        return;

    auto owner = this->owner.lock();
    if (!owner)
        return;

    previousDirection = dir;

    // 補間の初期化
    lerpTime = 0.0f;
    startAngle = owner->GetEulerRotation();
    startRotation = owner->GetQuaternionRotation();
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

bool RotationComponent::RotateTowardsDirection(const DirectX::XMFLOAT3& direction, const float maxDegreesPerSecond, const float deltaTime)
{
    if (auto owner = this->owner.lock())
    {
        const float directionLengthSq = direction.x * direction.x + direction.z * direction.z;
        if (directionLengthSq <= FLT_EPSILON)
            return true;

        const float targetYaw = std::atan2f(direction.x, direction.z);
        DirectX::XMVECTOR currentForward = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), DirectX::XMLoadFloat4(&owner->GetQuaternionRotation()));
        DirectX::XMFLOAT3 forward{};
        DirectX::XMStoreFloat3(&forward, currentForward);
        const float currentYaw = std::atan2f(forward.x, forward.z);
        const float yawDelta = std::atan2f(std::sinf(targetYaw - currentYaw), std::cosf(targetYaw - currentYaw));
        const float maxStep = DirectX::XMConvertToRadians((std::max)(0.0f, maxDegreesPerSecond)) * (std::max)(0.0f, deltaTime);
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




DirectX::XMFLOAT4 RotationComponent::SlerpQuaternion(const DirectX::XMFLOAT4& current, const DirectX::XMFLOAT4& target, const float t)
{
    using namespace DirectX;

    XMVECTOR q1 = XMLoadFloat4(&current);
    XMVECTOR q2 = XMLoadFloat4(&target);
    XMVECTOR result = XMQuaternionSlerp(q1, q2, t);

    XMFLOAT4 out;
    XMStoreFloat4(&out, result);
    return out;
}

bool RotationComponent::IsSameDirection(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
    constexpr float epsilon = 0.001f;
    return std::abs(a.x - b.x) < epsilon &&
        std::abs(a.y - b.y) < epsilon &&
        std::abs(a.z - b.z) < epsilon;
}
