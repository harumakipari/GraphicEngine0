#pragma once
#include "Components/Camera/CameraComponent.h"

class TPSCameraController
{
public:
    TPSCameraComponent* camera = nullptr;
    std::weak_ptr<SceneComponent> target;
    bool useRaycast = true; // 障害物の回避にレイキャストを使うかどうか
    void Update(float dt)
    {
        auto t = target.lock();
        if (!t) return;

        using namespace DirectX;

        XMFLOAT3 targetPos = t->GetComponentLocation();

        XMVECTOR pivot =
            XMLoadFloat3(&targetPos) +
            XMVectorSet(0, 1.5f, 0, 0);

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
        if (useRaycast)
        {
            resolvedEye =
                camera->ResolveCameraCollision(pivot, idealEye);
        }


        XMVECTOR currentEye = resolvedEye;
        //XMVECTOR currentEye = idealEye;

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
        //camera->GetOwner()->SetPosition(pos);
        XMFLOAT3 pivot3;
        XMStoreFloat3(&pivot3, pivot);

        camera->lookTarget = pivot3;
        camera->useLookTarget = true;
    }

    DirectX::XMFLOAT3 shakeOffset = {};
private:
    DirectX::XMFLOAT3 smoothedPosition{};
    bool initialized = false;
    float followSpeed = 15.0f;
};

