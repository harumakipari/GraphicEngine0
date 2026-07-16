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
    // プレイヤーの位置を取得
    auto playerHeadShared = playerHead.lock();
    if (!playerHeadShared)
    {
        return;
    }
    DirectX::XMFLOAT3 playerPos = playerHeadShared->GetComponentLocation();


    if (!isBlending)
    {
        if (requestMode != currentMode)
        {
            StartBlend(currentMode, requestMode);
        }
        else
        {
            UpdateDesireRotation(deltaTime);
            UpdateRotation(deltaTime);
            currentPose = CalculatePose(currentMode, playerPos, currentYaw, currentPitch);
        }
    }
    else
    {
        UpdateBlend(deltaTime);
    }

    SetPosition(currentPose.eye);
    mainCameraComponent->lookTarget = currentPose.target;
    mainCameraComponent->useLookTarget = true;
}

// ブレンドを開始する
void DarkCameraActor::StartBlend(CameraMode from, CameraMode to)
{
    blendTime = 0.0f;
    isBlending = true;

    // プレイヤーの位置を取得
    auto playerHeadShared = playerHead.lock();
    if (!playerHeadShared)
    {
        return;
    }
    DirectX::XMFLOAT3 playerPos = playerHeadShared->GetComponentLocation();

    // 現在のカメラ状態を保存
    blendStartPose = CalculatePose(from, playerPos, currentYaw, currentPitch);
    CameraPose oldPose = currentPose;
    float targetYaw = currentYaw;
    float targetPitch = currentPitch;
    switch (to)
    {
    case CameraMode::TPS:
        targetYaw = desiredYaw;
        targetPitch = desiredPitch;
        break;

    case CameraMode::Focus:
    {
        FocusInfo info = CreateFocusInfo();

        targetYaw = info.yaw;
        targetPitch = info.pitch;
        break;
    }

    case CameraMode::LockOn:

        break;
    }
    blendTargetPose = CalculatePose(to, playerPos, targetYaw, targetPitch);
    currentPose = oldPose;
}

// ブレンド状態を更新する
void DarkCameraActor::UpdateBlend(float deltaTime)
{
    if (!isBlending)
        return;

    blendTime += deltaTime;

    float t = std::clamp(blendTime / blendDuration, 0.0f, 1.0f);

    // 補間
    currentPose.target = MathHelper::Lerp(blendStartPose.target, blendTargetPose.target, t);
    currentYaw = MathHelper::LerpAngle(blendStartPose.yaw, blendTargetPose.yaw, t);
    currentPitch = std::lerp(blendStartPose.pitch, blendTargetPose.pitch, t);

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

    if (t >= 1.0f)
    {
        isBlending = false;
        currentMode = requestMode;
        desiredYaw = currentYaw;
        desiredPitch = currentPitch;
    }
}

// フォーカスカメラの情報を作成する
DarkCameraActor::FocusInfo DarkCameraActor::CreateFocusInfo()
{
    FocusInfo info = {};
    auto playerHeadShared = playerHead.lock();

    if (!playerHeadShared)
    {
        Logger::Warning("playerHeadShared is nullptr");
        return{};
    }

    auto playerActor = playerHeadShared->GetOwner();

    if (!playerActor)
    {
        Logger::Warning("playerActor is nullptr");
        return{};
    }

    info.direction = playerActor->GetForward();

    if (auto player = dynamic_cast<Player*>(playerActor))
    {
        player->SetFocusDirection(info.direction);
    }

    info.yaw = atan2f(info.direction.x, info.direction.z);
    info.pitch = 0.0f;
    return info;
}

#if 0
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
#endif // 0


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
        if (std::abs(inputComponent->GetIntent().leftMove.x) >= FLT_EPSILON &&
            std::abs(inputComponent->GetIntent().leftMove.z) >= FLT_EPSILON)
        {

        }
        else
        {// player の左スティックが入力されていなかったら、
            // 右スティックを動かす
            desiredYaw += rightStick.x * rotateSpeed * deltaTime;
            desiredPitch += rightStick.y * rotateSpeed * deltaTime;
        }
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

    // 
    desiredPitch = std::clamp(desiredPitch, DirectX::XMConvertToRadians(minPitchDegree), DirectX::XMConvertToRadians(maxPitchDegree));
    desiredYaw = MathHelper::ClampAngle(desiredYaw);
}

// 実際の方向を更新する関数
void DarkCameraActor::UpdateRotation(float deltaTime)
{
    currentYaw = desiredYaw;
    currentPitch = desiredPitch;
    mainCameraComponent->SetYawAndPitch(currentYaw, currentPitch);
}

DarkCameraActor::CameraPose DarkCameraActor::CalculatePose(CameraMode cameraMode, const DirectX::XMFLOAT3& playerPos, float yaw, float pitch) const
{
    CameraPose pose{};
    switch (cameraMode)
    {
    case CameraMode::TPS:
    {
        pose.target = playerPos;
        break;
    }
    case CameraMode::Focus:
    {
        DirectX::XMFLOAT3 forward = { sinf(yaw),0.0f,cosf(yaw) };
        pose.target = MathHelper::Add(playerPos, MathHelper::Multiply(forward, focusDistance));        break;
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
        sinf(yaw) * cosf(pitch),
        sinf(pitch),
        cosf(yaw) * cosf(pitch),
        0.0f);
    XMVECTOR target = XMLoadFloat3(&pose.target);
    XMVECTOR eye = target - forward * cameraDistance;
    eye += XMVectorSet(0, cameraHeight, 0, 0);

    XMStoreFloat3(&pose.eye, eye);
    //  yaw と pitch を保存
    pose.yaw = yaw;
    pose.pitch = pitch;
    return pose;
}

#if 0
void DarkCameraActor::CalculatePose(CameraMode cameraMode)
{
    auto playerHeadShared = playerHead.lock();
    if (!playerHeadShared)
    {
        return;
    }
    DirectX::XMFLOAT3 playerPos = playerHeadShared->GetComponentLocation();
    switch (cameraMode)
    {
    case CameraMode::TPS:
    {
        currentPose.target = playerPos;
        break;
    }
    case CameraMode::Focus:
    {
        currentPose.target = MathHelper::Add(playerPos, MathHelper::Multiply(CameraForwardXZ(), focusDistance));
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
#endif // 0



void DarkCameraActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    if (ImGui::Button(U8("TPS")))
    {
        SetRequestMode(CameraMode::TPS);

    }
    if (ImGui::Button(U8("Focus")))
    {
        SetRequestMode(CameraMode::Focus);

    }
    ImGui::DragFloat(U8("右スティックの回転のスピード"), &rotateSpeed, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("カメラの距離"), &cameraDistance, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("カメラの高さ"), &cameraHeight, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("フォーカス距離"), &focusDistance, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("最小ピッチ度数"), &minPitchDegree, 0.1f, -90.0f, 90.0f);
    ImGui::DragFloat(U8("最大ピッチ度数"), &maxPitchDegree, 0.1f, -90.0f, 90.0f);
#endif
}