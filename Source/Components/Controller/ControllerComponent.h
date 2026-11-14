#pragma once

#include <memory>
#include <string>
#include <DirectXMath.h>

#include "Components/Base/Component.h"
#include "Components/Base/SceneComponent.h"

class Actor;

struct MoveIntent
{
    DirectX::XMFLOAT3 leftMove;         // 左スティックの入力値
    DirectX::XMFLOAT2 rightMove;    // 右スティックの入力値
};

class InputComponent :public Component
{
public:
    InputComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :Component(name, owner) {}

    const MoveIntent& GetIntent() const { return intent; }
    void ClearIntent() { intent = {}; }

    void Tick(float) override;

    const DirectX::XMFLOAT3& GetMoveInput() const { return intent.leftMove; }

private:
    MoveIntent intent;
};

class CharacterMovementComponent : public SceneComponent
{
public:
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

public:
    CharacterMovementComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

    void Tick(float deltaTime) override;

    void TickDeferredMovement(float deltaTime);

    void DrawImGuiInspector() override;

    void SetMoveDirection(const DirectX::XMFLOAT3& dir) { inputDir = dir; }

    // Actor::Updateで入力方向を確定してから、同じフレームに一度だけ移動する場合に使用する。
    void SetDeferredMovementTick(const bool deferred) { deferredMovementTick = deferred; }

    // このフレームだけ通常移動へ加算する速度（Animation Motion Warpなど）
    void SetFrameAdditionalVelocity(const DirectX::XMFLOAT3& velocity) { frameAdditionalVelocity = velocity; }

    void SetUseGravity(const bool useGravity) { this->useGravity = useGravity; }
    // スティックの入力の強さを設定する
    void SetInputMagnitude(const float power) { this->inputMagnitude = power; }
    float GetInputMagnitude() const { return this->inputMagnitude; }

    void SetMoveSpeedScale(const float scale) { moveSpeedScale = std::clamp(scale, 0.25f, 1.5f); }

    void SetBossRoomProbeChargeWallHitArmed(const bool armed)
    {
        bossRoomProbeChargeWallHitArmed = armed;
        if (!armed) ClearBossRoomProbeChargeWallHitEvent();
    }
    void ClearBossRoomProbeChargeWallHitEvent() { bossRoomProbeChargeWallHitEvent = {}; }
    bool ConsumeBossRoomProbeChargeWallHitEvent(BossRoomProbeMoveBlockEvent& outEvent);

    void SetBossRoomWallProbeEnabled(const bool enabled) { bossRoomWallProbeEnabled = enabled; }
    void SetBossRoomWallProbeRadius(const float radius) { bossRoomWallProbeRadius = (std::max)(radius, 0.01f); }

    float GetActiveHorizontalWallProbeRadius() const { return bossRoomWallProbeEnabled ? bossRoomWallProbeRadius : radius; }

    // 入力によって変わる現在の速さを取得
    float GetCurrentInputNormalizeSpeed()const;

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

    // 一定時間だけ強制移動する速度を設定するための関数  ルートモーションではないアニメーションなどに使用する
    void AddForcedMove(const DirectX::XMFLOAT3& direction, const float speed, const float duration);

    // targetまで進む時に使用する
    void  MoveToActor(const std::shared_ptr<Actor>& target, const float moveTime, float stopDistance = 1.5f/*targetのどれくらい手前で止まるか*/);

    // targetまで進んだかどうか
    bool IsMoveToActorFinished() const { return !moveToTarget; }

    // 動きの速度を設定する
    void SetMoveSpeedSetting(const float walkSpeed, const float runSpeed)
    {
        this->walkSpeed = walkSpeed;
        this->runSpeed = runSpeed;
    }


private:
    void TickMovement(float deltaTime);

private:
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
    float moveToTargetTimer = 0.0f;
    float stopDistance = 1.5f;
    float inputMagnitude = 0.0f;// スティックの強さ　
    float walkSpeed = 2.0f;
    float runSpeed = 5.0f;
    // 入力によって変わるスピード結果
    float targetSpeed = 0.0f;
    float moveSpeedScale = 1.0f;
    float finalMoveSpeed = 0.0f;
    // 固定のスピードを使用するかどうか
    bool useFixedSpeed = false;
    bool deferredMovementTick = false;
};



class RotationComponent :public SceneComponent
{
public:
    RotationComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

    void Tick(float deltaTime)override;

    void DrawImGuiInspector() override;

    void SetRotateTime(const float t) { this->rotateTime = t; }
    void SetDirection(const DirectX::XMFLOAT3& dir);
    void SetDirectionImmediate(const DirectX::XMFLOAT3& dir);
    void ForceDirectionImmediate(const DirectX::XMFLOAT3& dir);
    void SyncCurrentRotation();
    bool RotateTowardsDirection(const DirectX::XMFLOAT3& direction, float maxDegreesPerSecond, float deltaTime);
private:
    DirectX::XMFLOAT4 SlerpQuaternion(const DirectX::XMFLOAT4& current, const DirectX::XMFLOAT4& target, const float t);

    bool IsSameDirection(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);

    DirectX::XMFLOAT3 previousDirection = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 startAngle = { 0.0f,0.0f,0.0f };  // degree
    DirectX::XMFLOAT4 targetRotation = { 0.0f,0.0f,0.0f,1.0f };
    DirectX::XMFLOAT4 startRotation = { 0.0f,0.0f,0.0f,1.0f };
    float lerpTime = 0.0f;
    float rotateTime = 0.3f;    // 0.3秒で rotation する
};
