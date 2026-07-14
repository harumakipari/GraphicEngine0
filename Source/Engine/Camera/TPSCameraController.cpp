#include "pch.h"
#include "TPSCameraController.h"
#include "Game/Actors/Camera/Camera.h"

void TPSCameraController::StartBlend(const Camera* currentCamera, float duration, std::function<void()> finished)
{
    onFinished = finished;
    startBlend = true;
    blendTime = 0.0f;
    blendDuration = duration;

    blendTargetEndPos = targetComponent.lock()->GetComponentLocation();
    blendTargetStartPos = targetComponent.lock()->GetComponentLocation();
    blendEyeStartPos = currentCamera->GetPosition();
    blendEyeEndPos = eyeComponent.lock()->GetComponentLocation();



    return;

    camera->lookTarget = targetComponent.lock()->GetComponentLocation();
    camera->GetOwner()->SetPosition(eyeComponent.lock()->GetComponentLocation());

    if (currentCamera)
    {
        blendEyeStartPos = currentCamera->GetPosition();
        blendStartPitch = currentCamera->GetCameraComponent()->GetPitch();
        blendStartYaw = currentCamera->GetCameraComponent()->GetYaw();
    }

    // TPSカメラの現在の目標
    blendTargetYaw = camera->GetYaw();
    blendTargetPitch = camera->GetPitch();

    using namespace DirectX;

    XMFLOAT4 rot = currentCamera->GetQuaternionRotation();

    XMVECTOR q = XMLoadFloat4(&rot);

    XMVECTOR forward =
        XMVector3Rotate(
            XMVectorSet(0, 0, 1, 0),
            q);

    XMVECTOR eye =
        XMLoadFloat3(&blendEyeStartPos);

    XMVECTOR target = eye + forward * 5.0f; // 適当な距離

    auto targetComp = targetComponent.lock();

    blendTargetEndPos = targetComp->GetComponentLocation();

    blendTargetEndPos.y += 1.5f;


    XMStoreFloat3(&blendTargetStartPos, target);


    blendTime = 0.0f;
    blendDuration = duration;
    startBlend = true;

    blendTargetEndPos = smoothedPivot;
    blendEyeEndPos = CalculateTargetCameraPosition(0.0f);
}

void TPSCameraController::Update(float deltaTime)
{
    DirectX::XMFLOAT3 targetCameraPos = CalculateTargetCameraPosition(deltaTime);

    if (startBlend)
    {
        //camera->lookTarget = targetComponent.lock()->GetComponentLocation();
        //camera->GetOwner()->SetPosition(eyeComponent.lock()->GetComponentLocation());


        UpdateBlend(eyeComponent.lock()->GetComponentLocation(), deltaTime);
        camera->GetOwner()->SetPosition(smoothedPosition);
        return;
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

    {
        camera->lookTarget = lookTargetPos;
        camera->useLookTarget = true;
    }

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

    // TPSモードの時の理想のカメラの位置
    XMVECTOR forward = XMVectorSet(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch), 0);
    XMVECTOR idealEye = pivot - forward * camera->distance;
    XMVECTOR resolvedEye = idealEye;

    if (cameraMode == CameraMode::BossBattle)
    {// ボスにターゲットを合わせる時の理想のカメラの位置
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

    // Eyeが理想位置からどれだけ押されたかに応じてLookTargetをplayer寄りへ補間する
    float idealDistance = XMVectorGetX(XMVector3Length(idealEye - pivot));
    float resolvedDistance = XMVectorGetX(XMVector3Length(resolvedEye - pivot));
    //Logger::Log("idealDistance:" + std::to_string(idealDistance));
    //Logger::Log("resolvedDistance:" + std::to_string(resolvedDistance));

    // 1.0 -> 壁に当たっていない　0.0 -> player と壁が近い
    float pushDistance = XMVectorGetX(XMVector3Length(idealEye - resolvedEye));
    blendLookTarget = std::clamp(pushDistance / 3.0f, 0.0f, 1.0f);

    //float playerToWallRatio = resolvedDistance / idealDistance;
    //blendLookTarget = 1.0f - playerToWallRatio;
    //blendLookTarget = std::clamp(blendLookTarget, 0.0f, 1.0f);
    //blendLookTarget *= blendLookTarget;

    XMVECTOR currentEye = resolvedEye;

    XMFLOAT3 pos;
    DirectX::XMStoreFloat3(&pos, currentEye);

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

    smoothedPosition = MathHelper::Lerp(blendEyeStartPos, targetCameraPos, t);

    float yaw = camera->GetYaw();
    float pitch = camera->GetPitch();

    //yaw = std::lerp(blendStartYaw, blendTargetYaw, t);
    //pitch = std::lerp(blendStartPitch, blendTargetPitch, t);

    //camera->SetPitch(pitch);
    //camera->SetYaw(yaw);

    camera->lookTarget = MathHelper::Lerp(blendTargetStartPos, blendTargetEndPos, t);
    camera->useLookTarget = true;
    if (t >= 1)
    {
        startBlend = false;
        initialized = true;
        if (onFinished)
        {
            onFinished();
            onFinished = nullptr;
        }

    }
}

// ロックオン処理
void TPSCameraController::UpdateLookTarget()
{

}