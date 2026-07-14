#pragma once
#include "Components/Camera/CameraComponent.h"

class Camera;

class TPSCameraController
{
public:
    enum class CameraMode :uint8_t
    {
        TPS,
        BossBattle,
    };

public:
    TPSCameraComponent* camera = nullptr;
    std::weak_ptr<SceneComponent> eyeComponent; // カメラ位置
    std::weak_ptr<SceneComponent> targetComponent;  // 注視点
    std::weak_ptr<SceneComponent> lockTarget;
    CameraMode cameraMode = CameraMode::TPS;

    bool isLockOn = false;
    bool useRaycast = true; // 障害物の回避にレイキャストを使うかどうか

    void StartBlend(const Camera* currentCamera,float duration,std::function<void()> finished);

    void Update(float deltaTime);

    void SetLockTarget(const std::shared_ptr<SceneComponent>& target)
    {
        lockTarget = target;
        isLockOn = true;
    }


    void ClearLockTarget()
    {
        lockTarget.reset();
        isLockOn = false;
    }

    DirectX::XMFLOAT3 shakeOffset = {};

    bool startBlend = false;

private:
    // カメラのターゲット位置を計算する
    DirectX::XMFLOAT3 CalculateTargetCameraPosition(float deltaTime);

    // TPSカメラの追従処理
    void UpdateFollow(const DirectX::XMFLOAT3& targetCameraPos, float deltaTime);

    // カメラのブレンド処理
    void UpdateBlend(const DirectX::XMFLOAT3& targetCameraPos, float deltaTime);

    // ロックオン処理
    void UpdateLookTarget();

private:
    DirectX::XMFLOAT3 smoothedPosition{};
    DirectX::XMFLOAT3 smoothedPivot{};

    bool initialized = false;

    float followSpeed = 10.0f;
    float pivotFollowSpeed = 5.0f;


    // ブレンド
    float blendTime = 0.0f;
    float blendDuration = 0.5f;

    DirectX::XMFLOAT3 blendEyeStartPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 blendTargetStartPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 blendEyeEndPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 blendTargetEndPos = { 0.0f,0.0f,0.0f };
    float blendStartYaw = 0.0f;
    float blendStartPitch = 0.0f;
    float blendTargetYaw = 0.0f;
    float blendTargetPitch = 0.0f;

    std::function<void()> onFinished;

public:
    // 壁とプレイヤーとのブレンド
    float blendLookTarget = 0.0f;
};

