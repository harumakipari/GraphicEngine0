#include "pch.h"
#include "DarkGameCamera.h"

void DarkCameraActor::Initialize(const Transform& transform)
{
    std::string parentName = "DarkCameraActor";
    mainCameraComponent = AddComponent<CameraComponent>(parentName);

    inputComponent = AddComponent<InputComponent>("inputComponent", parentName);
}

// 目標の方向を更新する関数
void DarkCameraActor::UpdateDesireRotation()
{
    auto intent = inputComponent->GetIntent();
    // 右スティックの入力値
    DirectX::XMFLOAT2 rightStick = intent.rightMove;

    switch (cameraMode)
    {
    case CameraMode::TPS:
        // 右スティックがYawになる
        desiredYaw += rightStick.x * rotateSpeed;
        desiredPitch += rightStick.y * rotateSpeed;
        break;
    case CameraMode::Focus:
        // プレイヤーが向いている方向を目標Yawにする
        if (auto playerHeadShared = playerHead.lock())
        {
            if (auto player = playerHeadShared->GetOwner())
            {
                DirectX::XMFLOAT3 forward = player->GetForward();

                desiredYaw = atan2f(forward.x, forward.z);

                // 少しだけ右スティックで補正
                desiredYaw += rightStick.x * rotateSpeed * 0.3f;
            }
        }
        // 少しだけ右スティックで補正
        desiredPitch += rightStick.y * rotateSpeed * 0.3f;
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
}

// 実際の方向を更新する関数
void DarkCameraActor::UpdateRotation()
{
    currentYaw = desiredYaw;
    currentPitch = desiredPitch;

    currentPitch = std::clamp(currentPitch, DirectX::XMConvertToRadians(minPitchDegree), DirectX::XMConvertToRadians(maxPitchDegree));

    mainCameraComponent->SetYaw(currentYaw);
    mainCameraComponent->SetPitch(currentPitch);
    mainCameraComponent->UpdateRotationFromYawPitch();
}

void DarkCameraActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::DragFloat(U8("右スティックの回転のスピード"), &rotateSpeed, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("最小ピッチ度数"), &minPitchDegree, 0.1f, -90.0f, 90.0f);
    ImGui::DragFloat(U8("最大ピッチ度数"), &maxPitchDegree, 0.1f, -90.0f, 90.0f);
#endif
}