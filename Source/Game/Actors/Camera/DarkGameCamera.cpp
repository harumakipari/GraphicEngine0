#include "pch.h"
#include "DarkGameCamera.h"

#include "Game/Actors/Player/Player.h"

void DarkCameraActor::Initialize(const Transform& transform)
{
    std::string parentName = "DarkCameraActor";
    mainCameraComponent = AddComponent<CameraComponent>(parentName);

    inputComponent = AddComponent<InputComponent>("inputComponent", parentName);
}

void DarkCameraActor::Update(float deltaTime)
{
    if (currentMode != requestMode)
    {
        StartBlend(currentMode, requestMode);
    }

    if (!isBlending)
    {
        UpdateDesireRotation(deltaTime);
        UpdateRotation(deltaTime);
        CalculatePose();
    }

    UpdateBlend(deltaTime);

    SetPosition(currentPose.eye);

    mainCameraComponent->lookTarget = currentPose.target;
    mainCameraComponent->useLookTarget = true;
}

// ブレンドを開始する
void DarkCameraActor::StartBlend(CameraMode from, CameraMode to)
{
    // 現在のカメラ状態を保存
    CalculatePose();
    blendStartPose = currentPose;

    // 一時的に新しいモードへ
    CameraMode oldMode = currentMode;
    currentMode = to;
    if (currentMode == CameraMode::Focus)
    {
        SyncFocusCamera();
    }

    CalculatePose();

    blendTargetPose = currentPose;

    blendTime = 0.0f;
    isBlending = true;
}

// ブレンド状態を更新する
void DarkCameraActor::UpdateBlend(float deltaTime)
{
    if (!isBlending)
        return;

    blendTime += deltaTime;

    float t =
        std::clamp(
            blendTime / blendDuration,
            0.0f,
            1.0f);

    // 補間
    currentPose.eye =
        MathHelper::Lerp(
            blendStartPose.eye,
            blendTargetPose.eye,
            t);

    currentPose.target =
        MathHelper::Lerp(
            blendStartPose.target,
            blendTargetPose.target,
            t);



    currentYaw =
        std::lerp(
            blendStartPose.yaw,
            blendTargetPose.yaw,
            t);


    currentPitch =
        std::lerp(
            blendStartPose.pitch,
            blendTargetPose.pitch,
            t);



    if (t >= 1.0f)
    {
        isBlending = false;
        currentMode = requestMode;
        desiredYaw = currentYaw;
        desiredPitch = currentPitch;
    }
}

// フォーカスカメラと同期する
void DarkCameraActor::SyncFocusCamera()
{
    auto playerHeadShared = playerHead.lock();

    if (!playerHeadShared)
        return;


    auto playerActor = playerHeadShared->GetOwner();

    if (!playerActor)
        return;

    XMFLOAT3 forward = playerActor->GetForward();

    if (auto player = dynamic_cast<Player*>(playerActor))
    {
        player->SetFocusDirection(forward);
    }

    currentYaw = atan2f(forward.x, forward.z);
    currentPitch = 0.0f;
    //desiredYaw = atan2f(forward.x, forward.z);
    //desiredPitch = 0.0f;
}

// 目標の方向を更新する関数
void DarkCameraActor::UpdateDesireRotation(float deltaTime)
{
    auto intent = inputComponent->GetIntent();
    // 右スティックの入力値
    DirectX::XMFLOAT2 rightStick = intent.rightMove;
    Logger::Log("rightStick" + std::to_string(rightStick.x) + ":" + std::to_string(rightStick.y));
    switch (currentMode)
    {
    case CameraMode::TPS:
        // 右スティックがYawになる
        desiredYaw += rightStick.x * rotateSpeed * deltaTime;
        desiredPitch += rightStick.y * rotateSpeed * deltaTime;
        break;
    case CameraMode::Focus:
        desiredYaw += rightStick.x * rotateSpeed * deltaTime;
        desiredPitch += rightStick.y * rotateSpeed * deltaTime;
        break;
    case CameraMode::LockOn:
        // 敵とプレイヤーが向いている方向
        if (auto playerHeadShared = playerHead.lock())
        {
            if (auto player = playerHeadShared->GetOwner())
            {
                if (auto enemy = enemyHead.lock()->GetOwner())
                {
                    DirectX::XMFLOAT3 playerPos = player->GetPosition();
                    DirectX::XMFLOAT3 enemyPos = enemy->GetPosition();

                    DirectX::XMFLOAT3 toEnemy = MathHelper::Subtract(enemyPos, playerPos);
                    desiredYaw = atan2f(toEnemy.x, toEnemy.z);
                }
            }
        }
        break;
    }

    desiredPitch = std::clamp(desiredPitch,
        DirectX::XMConvertToRadians(minPitchDegree),
        DirectX::XMConvertToRadians(maxPitchDegree));
}

// 実際の方向を更新する関数
void DarkCameraActor::UpdateRotation(float deltaTime)
{
    currentYaw = desiredYaw;
    currentPitch = desiredPitch;


    mainCameraComponent->SetYawAndPitch(currentYaw, currentPitch);
}

void DarkCameraActor::CalculatePose()
{
    auto playerHeadShared = playerHead.lock();
    if (!playerHeadShared)
    {
        return;
    }

    DirectX::XMFLOAT3 playerPos = playerHeadShared->GetComponentLocation();


    switch (currentMode)
    {
    case CameraMode::TPS:
    {
        currentPose.target = playerPos;
        break;
    }


    case CameraMode::Focus:
    {

        currentPose.target =
            MathHelper::Add(
                playerPos,
                MathHelper::Multiply(CameraForwardXZ(), focusDistance)
            );

        break;
    }


    case CameraMode::LockOn:
    {
        // 後で作る
        break;
    }
    }


    // Eye の処理は共通

    using namespace DirectX;

    XMVECTOR forward = XMVectorSet(
        sinf(currentYaw) * cosf(currentPitch),
        sinf(currentPitch),
        cosf(currentYaw) * cosf(currentPitch),
        0.0f);
    XMVECTOR target = XMLoadFloat3(&currentPose.target);

    XMVECTOR eye = target - forward * cameraDistance;

    eye += XMVectorSet(0, cameraHeight, 0, 0);

    XMStoreFloat3(&currentPose.eye, eye);
    // 現在の yaw と pitch を保存
    currentPose.yaw = currentYaw;
    currentPose.pitch = currentPitch;
}


void DarkCameraActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    if (ImGui::Button(U8("TPS")))
    {
        SetCameraMode(CameraMode::TPS);

    }
    if (ImGui::Button(U8("Focus")))
    {
        SetCameraMode(CameraMode::Focus);

    }
    ImGui::DragFloat(U8("右スティックの回転のスピード"), &rotateSpeed, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("カメラの距離"), &cameraDistance, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("カメラの高さ"), &cameraHeight, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("フォーカス距離"), &focusDistance, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("最小ピッチ度数"), &minPitchDegree, 0.1f, -90.0f, 90.0f);
    ImGui::DragFloat(U8("最大ピッチ度数"), &maxPitchDegree, 0.1f, -90.0f, 90.0f);
#endif
}