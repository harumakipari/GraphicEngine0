#include "pch.h"
#include "TPSCameraController.h"
#include "Game/Actors/Camera/Camera.h"

void TPSCameraController::StartBlend(const Camera* currentCamera, float duration)
{
    if (currentCamera)
    {
        blendStartPos = currentCamera->GetPosition();
        blendStartPitch = currentCamera->GetCameraComponent()->GetPitch();
        blendStartYaw = currentCamera->GetCameraComponent()->GetYaw();
    }

    // TPSカメラの現在の目標
    blendTargetYaw = camera->GetYaw();
    blendTargetPitch = camera->GetPitch();

    blendTime = 0.0f;
    blendDuration = duration;
    startBlend = true;
}

void TPSCameraController::Update(float deltaTime)
{
    DirectX::XMFLOAT3 targetCameraPos = CalculateTargetCameraPosition(deltaTime);

    if (startBlend)
    {
        UpdateBlend(targetCameraPos, deltaTime);
    }
    else
    {
        UpdateFollow(targetCameraPos, deltaTime);
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
                enemyPos.x - targetCameraPos.x,
                0.0f,
                enemyPos.z - targetCameraPos.z
            };

            float yaw = atan2f(dir.x, dir.z);

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

// カメラのターゲット位置を計算する
DirectX::XMFLOAT3 TPSCameraController::CalculateTargetCameraPosition(float deltaTime)
{
    auto t = targetComponent.lock();
    if (!t) return{ 0.0f,0.0f,0.0f };

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
            std::clamp(deltaTime * pivotFollowSpeed, 0.0f, 1.0f);

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

    return targetCameraPos;
}

// TPSカメラの追従処理
void TPSCameraController::UpdateFollow(const DirectX::XMFLOAT3& targetCameraPos, float deltaTime)
{
    if (!initialized)
    {
        smoothedPosition = targetCameraPos;
        initialized = true;
    }
    else
    {
        float t = std::clamp(deltaTime * followSpeed, 0.0f, 1.0f);

        smoothedPosition.x =
            std::lerp(smoothedPosition.x, targetCameraPos.x, t);

        smoothedPosition.y =
            std::lerp(smoothedPosition.y, targetCameraPos.y, t);

        smoothedPosition.z =
            std::lerp(smoothedPosition.z, targetCameraPos.z, t);
    }

}

void TPSCameraController::UpdateBlend(const DirectX::XMFLOAT3& targetCameraPos, float deltaTime)
{
    blendTime += deltaTime;

    float t = std::min<float>(blendTime / blendDuration, 1.0f);

    smoothedPosition = MathHelper::Lerp(blendStartPos, targetCameraPos, t);

    float yaw = camera->GetYaw();
    float pitch = camera->GetPitch();

    yaw = std::lerp(blendStartYaw, blendTargetYaw, t);
    pitch = std::lerp(blendStartPitch, blendTargetPitch, t);

    camera->SetPitch(pitch);
    camera->SetYaw(yaw);

    if (t >= 1)
    {
        startBlend = false;
        initialized = true;
    }
}

// ロックオン処理
void TPSCameraController::UpdateLookTarget()
{

}