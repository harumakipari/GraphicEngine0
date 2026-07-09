#pragma once
#include "Components/Camera/CameraComponent.h"

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
    void Update(float dt)
    {
        auto t = targetComponent.lock();
        if (!t) return;

        using namespace DirectX;

        XMFLOAT3 targetPos = t->GetComponentLocation();

        XMVECTOR pivot = XMLoadFloat3(&targetPos) + XMVectorSet(0, 1.5f, 0, 0);

        XMFLOAT3 targetPivot;
        XMStoreFloat3(&targetPivot, pivot);

        if (!initialized)
        {
            smoothedPivot = targetPivot;
        }
        else
        {
            float pivotT =
                std::clamp(dt * pivotFollowSpeed, 0.0f, 1.0f);

            smoothedPivot.x =
                std::lerp(smoothedPivot.x,
                    targetPivot.x,
                    pivotT);

            smoothedPivot.y =
                std::lerp(smoothedPivot.y,
                    targetPivot.y,
                    pivotT);

            smoothedPivot.z =
                std::lerp(smoothedPivot.z,
                    targetPivot.z,
                    pivotT);
        }

        pivot = XMLoadFloat3(&smoothedPivot);

        static DirectX::XMFLOAT3 lastPos{};

        lastPos = targetPos;

        float yaw = camera->GetYaw();
        float pitch = camera->GetPitch();

        XMVECTOR forward =
            XMVectorSet(
                sinf(yaw) * cosf(pitch),
                sinf(pitch),
                cosf(yaw) * cosf(pitch),
                0);

        XMVECTOR idealEye = pivot - forward * camera->distance;
        XMVECTOR resolvedEye = idealEye;

        if (cameraMode == CameraMode::BossBattle)
        {
            if (auto eyeComp = eyeComponent.lock())
            {
                DirectX::XMFLOAT3 eyePosition = eyeComp->GetComponentLocation();
                idealEye = DirectX::XMLoadFloat3(&eyePosition);
            }
        }

        if (useRaycast)
        {
            resolvedEye = camera->ResolveCameraCollision(pivot, idealEye);
        }

        XMVECTOR currentEye = resolvedEye;

        XMFLOAT3 pos;
        XMStoreFloat3(&pos, currentEye);

        pos.x += shakeOffset.x;
        pos.y += shakeOffset.y;
        pos.z += shakeOffset.z;

        XMFLOAT3 targetCameraPos;
        XMStoreFloat3(&targetCameraPos, resolvedEye);

        if (!initialized)
        {
            smoothedPosition = targetCameraPos;
            initialized = true;
        }
        else
        {
            float t = std::clamp(dt * followSpeed, 0.0f, 1.0f);

            smoothedPosition.x =
                std::lerp(smoothedPosition.x, targetCameraPos.x, t);

            smoothedPosition.y =
                std::lerp(smoothedPosition.y, targetCameraPos.y, t);

            smoothedPosition.z =
                std::lerp(smoothedPosition.z, targetCameraPos.z, t);
        }

        camera->GetOwner()->SetPosition(smoothedPosition);
        XMFLOAT3 lookTargetPos = smoothedPivot;

        if (isLockOn)
        {
            auto enemy = lockTarget.lock();

            if (enemy)
            {
                XMFLOAT3 enemyPos = enemy->GetComponentLocation();

                XMFLOAT3 dir =
                {
                    enemyPos.x - targetPos.x,
                    0.0f,
                    enemyPos.z - targetPos.z
                };

                yaw = atan2f(dir.x, dir.z);

                camera->SetYaw(yaw);

                lookTargetPos.x = (smoothedPivot.x + enemyPos.x) * 0.5f;
                lookTargetPos.y = (smoothedPivot.y + enemyPos.y) * 0.5f;
                lookTargetPos.z = (smoothedPivot.z + enemyPos.z) * 0.5f;
            }
            else
            {
                isLockOn = false;
            }
        }

        camera->lookTarget = lookTargetPos;
        camera->useLookTarget = true;

    }

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
private:
    DirectX::XMFLOAT3 smoothedPosition{};
    DirectX::XMFLOAT3 smoothedPivot{};

    bool initialized = false;

    float followSpeed = 10.0f;
    float pivotFollowSpeed = 5.0f;
};

