#include "pch.h"
#include "Camera.h"

#include "Components/Controller/ControllerComponent.h"
#include "Engine/Scene/SceneBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Physics/CollisionFunction.h"

void Camera::Initialize(const Transform& transform)
{
    mainCameraComponent = this->AddComponent<class TPSCameraComponent>("mainCamera");
    mainCameraComponent->SetPerspective(DirectX::XMConvertToRadians(35), Graphics::GetScreenWidth() / Graphics::GetScreenHeight(), 0.1f, 1000.0f);
}

void MainCamera::Update(float deltaTime)
{
#if 0
    // プレイヤー移動方向
    XMFLOAT3 moveDir = {};

    if (auto target = tpsController.targetComponent.lock())
    {
        auto actor = target->GetOwner();

        if (auto movement = actor->GetComponent<CharacterMovementComponent>())
        {
            moveDir = movement->GetVelocity();
        }
    }

#endif // 0

    // 右スティック
    XMFLOAT2 rightStick = InputSystem::GetRightStick();

    // カメラ回転
    mainCameraComponent->AddYaw(-rightStick.x * deltaTime * 2.0f);
    mainCameraComponent->AddPitch(rightStick.y * deltaTime * 2.0f);

    //const float limit = DirectX::XMConvertToRadians(80.0f);

    //mainCameraComponent->pitch =
    //    std::clamp(
    //        mainCameraComponent->pitch,
    //        -limit,
    //        limit
    //    );

    // Controller更新
    tpsController.Update(deltaTime);
}

void MainCamera::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    if (ImGui::Button("SetLockTarget"))
    {
        auto enemy = GetOwnerScene()->GetActorManager()->GetActorOfType<GruxEnemy>();
        SetLockTarget(enemy->GetRootComponent());
    }
    if (ImGui::Button("ClearLockTarget"))
    {
        ClearLockTarget();
    }

#endif
}

void TitleCamera::Update(float deltaTime)
{
    easingYawRunner->Tick(deltaTime);
    easingPitchRunner->Tick(deltaTime);
    easingFovRunner->Tick(deltaTime);
    // Controller更新
    tpsController.Update(deltaTime);

    mainCameraComponent->SetPitch(DirectX::XMConvertToRadians(currentPitch));
    mainCameraComponent->SetYaw(DirectX::XMConvertToRadians(currentYaw));
    mainCameraComponent->SetFov(DirectX::XMConvertToRadians(currentFov));
}

void TitleCamera::Play(float interval)
{
    // position の easing
    {
        TestEasingHandler handler;

        handler.AddWait(0.0f);

        handler.AddEasing(
            TestEaseType::OutExp,
            startPitch,
            endPitch,
            interval
        );

        handler.SetCompletedFunction([this]()
            {
                currentPitch = endPitch;
            });
        PropertyAccessor<float> accessor;

        accessor.getter = [this]() { return currentPitch; };
        accessor.setter = [this](float t)
            {
                currentPitch = t;
            };

        easingPitchRunner->StartHandler(handler, accessor);
    }


    // position の easing
    {
        TestEasingHandler handler;

        handler.AddWait(0.0f);

        handler.AddEasing(
            TestEaseType::OutExp,
            startYaw,
            endYaw,
            interval
        );

        handler.SetCompletedFunction([this]()
            {
                currentYaw = endYaw;
            });
        PropertyAccessor<float> accessor;

        accessor.getter = [this]() { return currentYaw; };
        accessor.setter = [this](float t)
            {
                currentYaw = t;
            };

        easingYawRunner->StartHandler(handler, accessor);
    }

    // fov の easing
    {
        TestEasingHandler handler;

        handler.AddWait(0.0f);

        handler.AddEasing(
            TestEaseType::OutExp,
            startFov,
            endFov,
            interval
        );

        handler.SetCompletedFunction([this]()
            {
                currentFov = endFov;
            });
        PropertyAccessor<float> accessor;

        accessor.getter = [this]() { return currentFov; };
        accessor.setter = [this](float t)
            {
                currentFov = t;
            };

        easingFovRunner->StartHandler(handler, accessor);
    }
}

void TitleCamera::PlayReverse(float interval)
{
    // position の easing
    {
        TestEasingHandler handler;

        handler.AddWait(0.0f);

        handler.AddEasing(
            TestEaseType::OutExp,
            endPitch,
            startPitch,
            interval
        );

        handler.SetCompletedFunction([this]()
            {
                currentPitch = startPitch;
            });
        PropertyAccessor<float> accessor;

        accessor.getter = [this]() { return currentPitch; };
        accessor.setter = [this](float t)
            {
                currentPitch = t;
            };

        easingPitchRunner->StartHandler(handler, accessor);
    }


    // position の easing
    {
        TestEasingHandler handler;

        handler.AddWait(0.0f);

        handler.AddEasing(
            TestEaseType::OutExp,
            endYaw,
            startYaw,
            interval
        );

        handler.SetCompletedFunction([this]()
            {
                currentYaw = startYaw;
            });
        PropertyAccessor<float> accessor;

        accessor.getter = [this]() { return currentYaw; };
        accessor.setter = [this](float t)
            {
                currentYaw = t;
            };

        easingYawRunner->StartHandler(handler, accessor);
    }
}