#pragma once
#include <memory>
#include <ranges>

#include "Core/Actor.h"
#include "Graphics/Resource/GeometricPrimitive.h"

#include "Components/Render/MeshComponent.h"
#include "Components/CollisionShape/CollisionComponent.h"
#include "Animation/AnimationController.h"
#include "Game/State/StateMachine.h"

class Character :public Actor
{
public:
    Character() = default;
    virtual ~Character() = default;

    Character(const std::string& modelName) :Actor(modelName)
    {
        animationBodyControllerName = "Body";
    }


    virtual void Update(float deltaTime)override
    {
        for (auto& controller : animationControllers | std::views::values)
        {
            controller->OnUpdate(deltaTime);
        }

        if (stateMachine_)
        {
            stateMachine_->Update(deltaTime);
        }
    };


    // ステートマシンをセットする
    void SetStateMachine(const std::shared_ptr<StateMachine>& stateMachine)
    {
        stateMachine_ = stateMachine;
    }


    // ステートマシンを取得する
    std::shared_ptr<StateMachine> GetStateMachine()
    {
        return stateMachine_;
    }

    // 使用
    void PlayBodyAnimation(const std::string& name, bool loop = true, bool blend = true, float blendTime = 0.3f) const
    {
        if (auto controller = GetAnimationController(animationBodyControllerName))
        {
            controller->ResetRootMotion(name, loop, blend, blendTime);
        }
    }

    void PlayAnimation(const std::string& controllerName, const std::string& name, bool loop = true, bool blend = true, float blendTime = 0.3f) const
    {
        if (auto controller = GetAnimationController(controllerName))
        {
            controller->ResetRootMotion(name, loop, blend, blendTime);
        }
    }

    virtual void LateUpdate(float elapsedTime) {};

    virtual void Move(float elapsedTime) {}

    // アニメーションの再生倍率を変更する関数
    void SetAnimationRate(const std::string& name, const float  animationRate) const
    {
        if (auto controller = GetAnimationController(name))
        {
            controller->SetAnimationRate(animationRate);
        }
    }

    void AddBodyAnimationController(const std::shared_ptr<AnimationController>& controller)
    {
        animationControllers[animationBodyControllerName] = controller;
    }

    // アニメーションコントローラーを追加する
    void AddAnimationController(const std::string& name, const std::shared_ptr<AnimationController>& controller)
    {
        animationControllers[name] = controller;
    }

    // アニメーションコントローラーを取得する
    std::shared_ptr<AnimationController> GetAnimationController(const std::string& name) const
    {
        auto it = animationControllers.find(name);

        if (it == animationControllers.end())
        {
            return nullptr;
        }
        return  it->second;
    }

    // アニメーションコントローラーを取得する
    std::shared_ptr<AnimationController> GetBodyAnimationController() const
    {
        auto it = animationControllers.find(animationBodyControllerName);

        if (it == animationControllers.end())
        {
            return nullptr;
        }
        return  it->second;
    }


    int GetHp() const { return hp; }

    //進行方向の単位ベクトルを取得する
    const DirectX::XMFLOAT3& GetForward()
    {
        // Z軸方向の単位方向ベクトル　デフォルト
        DirectX::XMVECTOR DefaultForward = DirectX::XMVectorSet(0, 0, 1, 0);
        //playerの回転値によって作られる回転行列
        DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&GetQuaternionRotation()));
        //DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(GetQuaternionRotation().x, GetQuaternionRotation().y, GetQuaternionRotation().z);
        // デフォルトのベクトルに回転行列を適応する
        DirectX::XMVECTOR TransformedForward = DirectX::XMVector3TransformNormal(DefaultForward, RotationMatrix);
        //正規化
        TransformedForward = DirectX::XMVector3Normalize(TransformedForward);

        DirectX::XMStoreFloat3(&front, TransformedForward);
        return front;
    }

    float GetYawFromForward(const DirectX::XMFLOAT3& forward)
    {
        // Z+ が前の座標系想定（LH）
        return std::atan2f(forward.x, forward.z);
    }


    // 入力をオンにするか
    virtual bool CanMove() { return canMove; }


    void DrawImGuiDetails()override
    {
#ifdef USE_IMGUI
        for (auto& controller : animationControllers | std::views::values)
        {
            controller->DrawImGui();
        }
        if (stateMachine_)
        {
            stateMachine_->DrawImGui();
        }
#endif
    }


public:
    //高さ
    float height = 0.0f;    //m単位

    //重さ
    float mass = 0.0f;      //kg単位

    //半径
    float radius = 0.0f;    //m単位

    //速度
    DirectX::XMFLOAT3 velocity = { 0.0f,0.0f,0.0f };

    //地面の上にいるか
    bool isGround = false;

    //重力
    float gravity = -9.8f;
protected:
    size_t animationIndex = 0;

    int hp = 0;

    //前方向ベクトル
    DirectX::XMFLOAT3 front{ 0.0f,0.0f,1.0f/*,0.0f*/ };

    //最大回転値
    float maxTurningSpeed = 360.0f;

    //回転速度　degree基準
    float turningSpeed = 0.0f;

    //レイを飛ばしたときにステージと当たる座標
    DirectX::XMFLOAT3  intersectStagePosition{ 0.0f,0.0f,0.0f };


    // ステートマシン
    std::shared_ptr<StateMachine> stateMachine_;

    bool canMove = true;

private:
    std::unordered_map<std::string, std::shared_ptr<AnimationController>> animationControllers;

    std::string animationBodyControllerName = "Body";

};