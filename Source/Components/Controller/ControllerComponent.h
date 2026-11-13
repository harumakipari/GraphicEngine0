#pragma once

#include <memory>
#include <string>

#include <DirectXMath.h>

#include "Components/Base/Component.h"
#include "Components/Base/SceneComponent.h"
#include "Engine/Input/GamePad.h"
#include "Engine/Input/InputSystem.h"

class Actor;

struct MoveIntent
{
    DirectX::XMFLOAT3 leftMove;         // 左スティックの入力値
    DirectX::XMFLOAT2 rightMove;    // 右スティックの入力値
    bool jump = false;
};

class InputComponent :public Component
{
public:
    InputComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :Component(name, owner) {}

    const MoveIntent& GetIntent() const { return intent_; }
    void ClearIntent() { intent_ = {}; }

    void Tick(float) override;

    const DirectX::XMFLOAT3& GetMoveInput() const { return intent_.leftMove; }

    float GetTumbStateLx()
    {
        return pad.ThumbStateLx();
    }
    float GetTumbStateLy()
    {
        return pad.ThumbStateLy();
    }
    // [a]:-1   [d]:+1
    float GetThumbStateRx()
    {
        return pad.ThumbStateRx();
    }
    // [w]:+1  [s]:-1
    float GetThumbStateRy()
    {
        return pad.ThumbStateRy();
    }

private:
    MoveIntent intent_;
    GamePad pad;
};

class CharacterMovementComponent : public SceneComponent
{
public:
    CharacterMovementComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

    void Tick(float deltaTime) override;

    // Actor::Updateで入力方向を確定してから、同じフレームに一度だけ移動する場合に使用する。
    void SetDeferredMovementTick(const bool deferred) { deferredMovementTick = deferred; }
    void TickDeferredMovement(float deltaTime);

    void SetMoveDirection(const DirectX::XMFLOAT3& dir)
    {
        inputDir = dir;
    }

    // このフレームだけ通常移動へ加算する速度（Animation Motion Warpなど）。
    void SetFrameAdditionalVelocity(const DirectX::XMFLOAT3& velocity)
    {
        frameAdditionalVelocity = velocity;
    }

    void ApplyIntent(const MoveIntent& intent)
    {
        inputDir = intent.leftMove;
    }

    void SetUseGravity(const bool useGravity) { this->useGravity = useGravity; }

    // スティックの入力の強さを設定する
    void SetInputMagnitude(const float power) { this->inputMagnitude = power; }
    float GetInputMagnitude() const { return this->inputMagnitude; }

    void SetMoveSpeedScale(const float scale)
    {
        moveSpeedScale = std::clamp(scale, 0.25f, 1.5f);
    }

    float GetActualHorizontalSpeed() const { return actualHorizontalSpeed; }
    float GetFinalMoveSpeed() const { return finalMoveSpeed; }
    bool GetLastWallRayCastHitForDebug() const { return lastWallRayCastHitForDebug; }
    bool GetLastWallProbeUsedSphereCastForDebug() const { return lastWallProbeUsedSphereCastForDebug; }
    bool GetLastStageWallRayCastHitForDebug() const { return lastStageWallRayCastHitForDebug; }
    bool GetLastBossRoomWallSphereCastHitForDebug() const { return lastBossRoomWallSphereCastHitForDebug; }
    bool GetLastWallProbeInitialOverlapForDebug() const { return lastWallProbeInitialOverlapForDebug; }
    uint32_t GetLastWallProbeHitLayerForDebug() const { return lastWallProbeHitLayerForDebug; }
    const DirectX::XMFLOAT3& GetLastWallProbeOriginForDebug() const { return lastWallProbeOriginForDebug; }
    const DirectX::XMFLOAT3& GetLastWallProbeDirectionForDebug() const { return lastWallProbeDirectionForDebug; }
    float GetLastWallProbeDistanceForDebug() const { return lastWallProbeDistanceForDebug; }
    const DirectX::XMFLOAT3& GetLastRequestedHorizontalMoveForDebug() const { return lastRequestedHorizontalMoveForDebug; }
    const DirectX::XMFLOAT3& GetLastResolvedHorizontalMoveForDebug() const { return lastResolvedHorizontalMoveForDebug; }

    struct BossRoomProbeMoveBlockEvent
    {
        bool pending = false;
        DirectX::XMFLOAT3 hitPosition{};
        DirectX::XMFLOAT3 hitNormal{};
        uint32_t hitLayer = 0;
        bool initialOverlap = false;
        DirectX::XMFLOAT3 requestedMove{};
        DirectX::XMFLOAT3 resolvedMove{};
        float forwardMoveLost = 0.0f;
    };

    void SetBossRoomProbeChargeWallHitArmed(const bool armed)
    {
        bossRoomProbeChargeWallHitArmed = armed;
        if (!armed) ClearBossRoomProbeChargeWallHitEvent();
    }
    void ClearBossRoomProbeChargeWallHitEvent() { bossRoomProbeChargeWallHitEvent = {}; }
    bool ConsumeBossRoomProbeChargeWallHitEvent(BossRoomProbeMoveBlockEvent& outEvent)
    {
        if (!bossRoomProbeChargeWallHitEvent.pending) return false;
        outEvent = bossRoomProbeChargeWallHitEvent;
        bossRoomProbeChargeWallHitEvent = {};
        return true;
    }

    void SetBossRoomWallProbeEnabled(const bool enabled) { bossRoomWallProbeEnabled = enabled; }
    bool IsBossRoomWallProbeEnabled() const { return bossRoomWallProbeEnabled; }
    void SetBossRoomWallProbeRadius(const float radius) { bossRoomWallProbeRadius = (std::max)(radius, 0.01f); }
    float GetBossRoomWallProbeRadius() const { return bossRoomWallProbeRadius; }
    float GetActiveHorizontalWallProbeRadius() const
    {
        return bossRoomWallProbeEnabled ? bossRoomWallProbeRadius : radius;
    }

    const DirectX::XMFLOAT3& GetLastWallCollisionPositionForDebug() const
    {
        return lastWallCollisionPositionForDebug;
    }

    // 速度を取得する
    DirectX::XMFLOAT3 GetVelocity() const
    {
        return velocity;
    }

    // 初速を設定する
    void SetInitialSpeed(const float s)
    {
        this->initialSpeed = s;
        this->speed = s;
    }

    // 入力によって変わる現在の速さを取得
    float GetCurrentInputSpeed() const { return targetSpeed; }

    // 入力によって変わる現在の速さを取得
    float GetCurrentInputNormalizeSpeed()const
    {
        // 0除算を防ぐため
        float speed = targetSpeed / std::max<float>(0.00001f, runSpeed);
        return speed;
    }

    // 速さを設定する
    void SetFixedSpeed(const float speed)
    {
        this->speed = speed;
        useFixedSpeed = true;
    }

    // 速さをリセットする
    void ResetFixedSpeed()
    {
        useFixedSpeed = false;
        this->speed = initialSpeed;
    }

    // 吹き飛ばしなどで外部から速度を加算するための関数
    void AddImpulse(const DirectX::XMFLOAT3& impulse)
    {
        externalVelocity.x += impulse.x;
        externalVelocity.y += impulse.y;
        externalVelocity.z += impulse.z;
    }

    // 一定時間だけ強制移動する速度を設定するための関数  ルートモーションではないアニメーションなどに使用する
    void AddForcedMove(const DirectX::XMFLOAT3& direction, const float speed, const float duration)
    {
        forcedVelocity =
        {
            direction.x * speed,
            direction.y * speed,
            direction.z * speed
        };

        forcedMoveTime = duration;
    }

    void Jump(const float power)
    {
        velocity.y += power;
    }

    bool IsGround() const { return isGrounded; }

    // targetまで進む時に使用する
    void  MoveToActor(const std::shared_ptr<Actor>& target, const float moveTime, float stopDistance = 1.5f/*targetのどれくらい手前で止まるか*/)
    {
        this->target = target;
        moveToTargetTime = moveTime;
        moveToTargetTimer = moveTime;
        this->stopDistance = stopDistance;
        moveToTarget = true;
    }

    // targetまで進んだかどうか
    bool IsMoveToActorFinished() const { return !moveToTarget; }

    // 動きの速度を設定する
    void SetMoveSpeedSetting(const float walkSpeed, const float runSpeed)
    {
        this->walkSpeed = walkSpeed;
        this->runSpeed = runSpeed;
    }

    void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        SceneComponent::DrawImGuiInspector();
        ImGui::DragFloat(U8("歩く速度"), &walkSpeed, 0.05f, 0.0f, 5.0f);
        ImGui::DragFloat(U8("走り速度"), &runSpeed, 0.05f, 0.0f, 10.0f);

#endif
    }

private:
    void TickMovement(float deltaTime);

    // 状態
    DirectX::XMFLOAT3 velocity = { 0,0,0 };
    bool isGrounded = false;

    // 設定値
    float speed = 1.0f;    // 強制的に速さを変更する場合に使用
    float gravity = -4.9f;
    float groundOffset = 1.0f;
    float radius = 0.4f;
    bool bossRoomWallProbeEnabled = false;
    float bossRoomWallProbeRadius = 0.4f;
    bool lastWallProbeUsedSphereCastForDebug = false;
    bool lastStageWallRayCastHitForDebug = false;
    bool lastBossRoomWallSphereCastHitForDebug = false;
    bool lastWallProbeInitialOverlapForDebug = false;
    uint32_t lastWallProbeHitLayerForDebug = 0;
    DirectX::XMFLOAT3 lastWallProbeOriginForDebug{};
    DirectX::XMFLOAT3 lastWallProbeDirectionForDebug{};
    float lastWallProbeDistanceForDebug = 0.0f;
    DirectX::XMFLOAT3 lastRequestedHorizontalMoveForDebug{};
    DirectX::XMFLOAT3 lastResolvedHorizontalMoveForDebug{};
    bool bossRoomProbeChargeWallHitArmed = false;
    BossRoomProbeMoveBlockEvent bossRoomProbeChargeWallHitEvent{};
    bool useGravity = true;

    float initialSpeed = 1.0f; // 初速

    DirectX::XMFLOAT3 inputDir{ 0,0,0 };
    DirectX::XMFLOAT3 frameAdditionalVelocity{ 0,0,0 };
    DirectX::XMFLOAT3 externalVelocity = { 0.0f,0.0f,0.0f }; // 外部から加算される速度（吹き飛ばしなど）
    float damping = 3.5f; // 外部速度の減衰率（1秒あたりどれだけ外部速度が減るか）

    // 一定時間だけ強制移動する時に使用する　RootMotionではないアニメーションに使用
    DirectX::XMFLOAT3 forcedVelocity{ 0,0,0 };
    float forcedMoveTime = 0.0f;

    // targetまで進む時に使用する
    bool moveToTarget = false;
    std::weak_ptr<Actor> target;
    float moveToTargetTime = 0.0f;
    float moveToTargetTimer = 0.0f;
    float stopDistance = 1.5f;

    // スティックの強さ　
    float inputMagnitude = 0.0f;

    float walkSpeed = 2.0f;
    float runSpeed = 5.0f;
    // 入力によって変わるスピード結果
    float targetSpeed = 0.0f;
    float moveSpeedScale = 1.0f;
    float finalMoveSpeed = 0.0f;
    float actualHorizontalSpeed = 0.0f;
    bool lastWallRayCastHitForDebug = false;
    DirectX::XMFLOAT3 lastWallCollisionPositionForDebug{};
    // 固定のスピードを使用するかどうか
    bool useFixedSpeed = false;
    bool deferredMovementTick = false;
};



class RotationComponent :public SceneComponent
{
public:
    RotationComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

    void SetDirection(const DirectX::XMFLOAT3& dir);
    void SetDirectionImmediate(const DirectX::XMFLOAT3& dir);
    void ForceDirectionImmediate(const DirectX::XMFLOAT3& dir);
    void SyncCurrentRotation();
    bool RotateTowardsDirection(const DirectX::XMFLOAT3& direction, float maxDegreesPerSecond, float deltaTime);

    void Tick(float deltaTime)override;

    void SetRotateTime(const float t) { this->rotateTime = t; }

    void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        SceneComponent::DrawImGuiInspector();
        ImGui::DragFloat(U8("回転時間"), &rotateTime, 0.05f, 0.0f, 10.0f);

#endif
    }
private:
    // t: 補間率（0.0?1.0）
    DirectX::XMFLOAT4 SlerpQuaternion(const DirectX::XMFLOAT4& current, const DirectX::XMFLOAT4& target, const float t)
    {
        using namespace DirectX;

        XMVECTOR q1 = XMLoadFloat4(&current);
        XMVECTOR q2 = XMLoadFloat4(&target);
        XMVECTOR result = XMQuaternionSlerp(q1, q2, t);

        XMFLOAT4 out;
        XMStoreFloat4(&out, result);
        return out;
    }

    bool IsSameDirection(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        constexpr float epsilon = 0.001f;
        return std::abs(a.x - b.x) < epsilon &&
            std::abs(a.y - b.y) < epsilon &&
            std::abs(a.z - b.z) < epsilon;
    }

    DirectX::XMFLOAT3 direction = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 previousDirection = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 startAngle = { 0.0f,0.0f,0.0f };  // degree
    DirectX::XMFLOAT4 targetRotation = { 0.0f,0.0f,0.0f,1.0f };
    DirectX::XMFLOAT4 startRotation = { 0.0f,0.0f,0.0f,1.0f };
    float lerpTime = 0.0f;
    float rotateTime = 0.3f;    // 3秒で rotation する
};
