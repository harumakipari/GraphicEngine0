#ifndef CONTROLLER_COMPONENT_H
#define CONTROLLER_COMPONENT_H

// C++ 標準ライブラリ
#include <memory>
#include <unordered_map>
#include <string>

// 他ライブラリ
#include <DirectXMath.h>

// プロジェクトの他のヘッダ
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

    void SetMoveDirection(const DirectX::XMFLOAT3& dir)
    {
        inputDir_ = dir;
    }

    void ApplyIntent(const MoveIntent& intent)
    {
        inputDir_ = intent.leftMove;
    }

    void SetUseGravity(const bool useGravity) { this->useGravity = useGravity; }

    // スティックの入力の強さを設定する
    void SetInputMagnitude(const float power) { this->inputMagnitude = power; }

    // 速度を取得する
    DirectX::XMFLOAT3 GetVelocity() const
    {
        return velocity_;
    }

    // 初速を設定する
    void SetInitialSpeed(const float s)
    {
        this->initialSpeed = s;
        this->speed_ = s;
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
    void SetSpeed(const float speed) { this->speed_ = speed; }

    // 速さをリセットする
    void ResetSpeed() { this->speed_ = initialSpeed; }

    // 吹き飛ばしなどで外部から速度を加算するための関数
    void AddImpulse(const DirectX::XMFLOAT3& impulse)
    {
        externalVelocity_.x += impulse.x;
        externalVelocity_.y += impulse.y;
        externalVelocity_.z += impulse.z;
    }

    // 一定時間だけ強制移動する速度を設定するための関数  ルートモーションではないアニメーションなどに使用する
    void AddForcedMove(const DirectX::XMFLOAT3& direction, float speed, float duration)
    {
        forcedVelocity_ =
        {
            direction.x * speed,
            direction.y * speed,
            direction.z * speed
        };

        forcedMoveTime_ = duration;
    }

    void Jump(const float power)
    {
        velocity_.y += power;
    }

    bool IsGround() const { return isGrounded_; }

    // targetまで進む時に使用する
    void  MoveToActor(
        const std::shared_ptr<Actor>& target,
        float moveTime, float stopDistance = 1.5f/*targetのどれくらい手前で止まるか*/)
    {
        this->target = target;
        moveToTargetTime_ = moveTime;
        moveToTargetTimer_ = moveTime;
        stopDistance_ = stopDistance;
        moveToTarget_ = true;
    }

    // targetまで進んだかどうか
    bool IsMoveToActorFinished() const { return !moveToTarget_; }

    void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        SceneComponent::DrawImGuiInspector();
        ImGui::DragFloat(U8("歩く速度"), &walkSpeed, 0.05f, 0.0f, 5.0f);
        ImGui::DragFloat(U8("走り速度"), &runSpeed, 0.05f, 0.0f, 10.0f);

#endif
    }

private:
    // 状態
    DirectX::XMFLOAT3 velocity_{ 0,0,0 };
    bool isGrounded_ = false;

    // 設定値
    float speed_ = 1.0f;    // 強制的に速さを変更する場合に使用
    float gravity_ = -4.9f;
    float groundOffset_ = 1.0f;
    float radius_ = 0.4f;
    bool useGravity = true;

    float initialSpeed = 1.0f; // 初速

    DirectX::XMFLOAT3 inputDir_{ 0,0,0 };
    DirectX::XMFLOAT3 externalVelocity_ = { 0.0f,0.0f,0.0f }; // 外部から加算される速度（吹き飛ばしなど）
    float damping_ = 3.5f; // 外部速度の減衰率（1秒あたりどれだけ外部速度が減るか）

    // 一定時間だけ強制移動する時に使用する　RootMotionではないアニメーションに使用
    DirectX::XMFLOAT3 forcedVelocity_{ 0,0,0 };
    float forcedMoveTime_ = 0.0f;

    // targetまで進む時に使用する
    bool moveToTarget_ = false;
    std::weak_ptr<Actor> target;
    float moveToTargetTime_ = 0.0f;
    float moveToTargetTimer_ = 0.0f;
    float stopDistance_ = 1.5f;

    // スティックの強さ　
    float inputMagnitude = 0.0f;

    float walkSpeed = 2.0f;
    float runSpeed = 5.0f;
    // 入力によって変わるスピード結果
    float targetSpeed = 0.0f;
};



class RotationComponent :public SceneComponent
{
public:
    RotationComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

    void SetDirection(const DirectX::XMFLOAT3& dir);

    void Tick(float deltaTime)override;

    void SetRotateTime(float t) { this->rotateTime_ = t; }

    void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        SceneComponent::DrawImGuiInspector();
        ImGui::DragFloat(U8("回転時間"), &rotateTime_, 0.05f, 0.0f, 10.0f);

#endif
    }
private:
    // t: 補間率（0.0?1.0）
    DirectX::XMFLOAT4 SlerpQuaternion(const DirectX::XMFLOAT4& current, const DirectX::XMFLOAT4& target, float t)
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

    DirectX::XMFLOAT3 direction_ = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 previousDirection_ = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 startAngle_ = { 0.0f,0.0f,0.0f };  // degree
    DirectX::XMFLOAT4 targetRotation_ = { 0.0f,0.0f,0.0f,1.0f };
    DirectX::XMFLOAT4 startRotation_ = { 0.0f,0.0f,0.0f,1.0f };
    float lerpTime_ = 0.0f;
    float rotateTime_ = 0.3f;    // 3秒で rotation する
};


#endif //CONTROLLER_COMPONENT_H