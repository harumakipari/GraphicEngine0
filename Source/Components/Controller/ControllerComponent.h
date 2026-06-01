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

    void Tick(float dt) override;

    void SetMoveDirection(const DirectX::XMFLOAT3& dir)
    {
        inputDir_ = dir;
    }

    void ApplyIntent(const MoveIntent& intent)
    {
        inputDir_ = intent.leftMove;
    }

    void SetUseGravity(const bool useGravity) { this->useGravity = useGravity; }

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

    // 速さを設定する
    void SetSpeed(const float speed) { this->speed_ = speed; }

    // 速さをリセットする
    void ResetSpeed() { this->speed_ = initialSpeed; }

    // ジャンプや吹き飛ばしなどで外部から速度を加算するための関数
    void AddImpulse(const DirectX::XMFLOAT3& impulse)
    {
        externalVelocity_.x += impulse.x;
        externalVelocity_.y += impulse.y;
        externalVelocity_.z += impulse.z;
    }
private:
    // 状態
    DirectX::XMFLOAT3 velocity_{ 0,0,0 };
    bool isGrounded_ = false;

    // 設定値
    float speed_ = 5.0f;
    float gravity_ = -4.9f;
    float groundOffset_ = 1.0f;
    float radius_ = 0.4f;
    bool useGravity = true;

    float initialSpeed = 5.0f; // 初速


    DirectX::XMFLOAT3 inputDir_{ 0,0,0 };
    DirectX::XMFLOAT3 externalVelocity_ = { 0.0f,0.0f,0.0f }; // 外部から加算される速度（ジャンプや吹き飛ばしなど）
    float damping_ = 3.5f; // 外部速度の減衰率（1秒あたりどれだけ外部速度が減るか）
};



class RotationComponent :public SceneComponent
{
public:
    RotationComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

    void SetDirection(const DirectX::XMFLOAT3& dir);

    void Tick(float deltaTime)override;

    void SetRotateTime(float t) { this->rotateTime_ = t; }
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
    float rotateSpeed_ = 10.0f; // ( degree / second )
};


#endif //CONTROLLER_COMPONENT_H