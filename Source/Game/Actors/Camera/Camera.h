#pragma once
#include <DirectXMath.h>

#include "Core/Actor.h"

#include "Components/Camera/CameraComponent.h"
#include "Components/CollisionShape/ShapeComponent.h"

#include "Engine/Camera/CameraConstants.h"
#include "Engine/Camera/TPSCameraController.h"

class Camera :public Actor
{
public:
    //引数付きコンストラクタ
    explicit Camera(const std::string& actorName) :Actor(actorName)
    {
    }

    virtual ~Camera() = default;

    virtual void Initialize(const Transform& transform)override;

    CameraComponent* GetCameraComponent() const { return mainCameraComponent.get(); }

    ViewConstants GetViewConstants() const
    {
        return mainCameraComponent->GetViewConstants();
    }
protected:
    std::shared_ptr<CameraComponent> mainCameraComponent;
};

class DebugCamera :public Camera
{
public:
    //引数付きコンストラクタ
    DebugCamera(const std::string& actorName) :Camera(actorName) {}

    virtual ~DebugCamera() = default;
    void Initialize(const Transform& transform)override
    {
        mainCameraComponent = this->AddComponent<class DebugCameraComponent>("debugCamera");
    }

};

class CinemaCamera :public Camera
{
public:
    //引数付きコンストラクタ
    CinemaCamera(const std::string& actorName) :Camera(actorName) {}

    virtual ~CinemaCamera() = default;
    void Initialize(const Transform& transform)override
    {
        mainCameraComponent = this->AddComponent<CinematicCameraComponent>("cinemaCamera");
    }
    // カメラを移動できるようにするかどうか
    void SetUseDebugMode(const bool useDebug) const
    {
        std::dynamic_pointer_cast<CinematicCameraComponent>(mainCameraComponent)->SetIsUseCinematic(useDebug);
    }

};

class MovieCamera :public Camera
{
public:
    //引数付きコンストラクタ
    MovieCamera(const std::string& actorName) :Camera(actorName) {}

    virtual ~MovieCamera() = default;
    void Initialize(const Transform& transform)override
    {
        mainCameraComponent = this->AddComponent<MovieCameraComponent>("movieCamera");
    }

    std::shared_ptr<MovieCameraComponent> GetMovieCameraComponent() const
    {
        return std::dynamic_pointer_cast<MovieCameraComponent>(mainCameraComponent);
    }
};


class MainCamera :public Camera
{
public:
    //引数付きコンストラクタ
    explicit MainCamera(const std::string& actorName) :Camera(actorName) {}
    virtual ~MainCamera() = default;

    void Initialize(const Transform& transform)override
    {
        Camera::Initialize(transform);
        tpsController.camera =
            static_cast<TPSCameraComponent*>(mainCameraComponent.get());
    };

    void SetLookTarget(const std::shared_ptr<SceneComponent>& target)
    {
        tpsController.targetComponent = target;
    }

    void SetEye(const std::shared_ptr<SceneComponent>& eye)
    {
        tpsController.eyeComponent = eye;
    }

    void SetCameraMode(TPSCameraController::CameraMode cameraMode)
    {
        tpsController.cameraMode = cameraMode;
    }

    void StartBlend(const Camera* currentCamera, float duration, std::function<void()> finished)
    {
        tpsController.StartBlend(currentCamera, duration, finished);
    }

    //更新処理
    void Update(float deltaTime)override;

    void DrawImGuiDetails()override;

    DirectX::XMFLOAT3 CameraForwardXZ() const
    {
        auto forward = mainCameraComponent->GetForward();
        forward.y = 0.0f;
        return MathHelper::Normalize(forward);
    }

    DirectX::XMFLOAT3 CameraRightXZ() const
    {
        auto right = mainCameraComponent->GetRight();
        right.y = 0.0f;
        return MathHelper::Normalize(right);
    }

    void SetLockTarget(const std::shared_ptr<SceneComponent>& target)
    {
        tpsController.SetLockTarget(target);
    }


    void ClearLockTarget()
    {
        tpsController.ClearLockTarget();
    }

    // カメラのモードを取得する
    TPSCameraController::CameraMode GetCameraMode() const { return tpsController.cameraMode; }
    TPSCameraController tpsController;

private:
    bool didShake = false;
};



class TitleCamera :public Camera
{
public:
    //引数付きコンストラクタ
    explicit TitleCamera(const std::string& actorName) :Camera(actorName) {}
    virtual ~TitleCamera() = default;

    void Initialize(const Transform& transform)override
    {
        Camera::Initialize(transform);
        tpsController.camera = static_cast<TPSCameraComponent*>(mainCameraComponent.get());
        // FixedCamera ではレイキャストを使わない
        tpsController.useRaycast = false;

        easingPitchRunner = std::make_unique<EasingRunner>();
        easingYawRunner = std::make_unique<EasingRunner>();
        easingFovRunner = std::make_unique<EasingRunner>();

        startYaw = 231.5f;
        endYaw = 180.0f;

        startPitch = -7.5f;
        endPitch = -60.0f;

        currentPitch = startPitch;
        currentYaw = startYaw;
    };

    void SetTarget(const std::shared_ptr<SceneComponent>& target)
    {
        tpsController.targetComponent = target;
    }

    //更新処理
    void Update(float deltaTime)override;

    void SetUseMovie(bool use)
    {
        useMovie = use;
    }


    DirectX::XMFLOAT3 CameraForwardXZ() const
    {
        float yaw = mainCameraComponent->GetYaw();

        return {
            sinf(yaw),
            0.0f,
            cosf(yaw)
        };
    }

    DirectX::XMFLOAT3 CameraRightXZ() const
    {
        float yaw = mainCameraComponent->GetYaw();

        return {
            cosf(yaw),
            0.0f,
            -sinf(yaw)
        };
    }

    void Play(float interval);
    void PlayReverse(float interval);
private:
    TPSCameraController tpsController;
    bool useMovie = false;
    bool didShake = false;

    float startYaw = 231.5f;
    float endYaw = 180.0f;
    float currentYaw = 0.0f;

    float startPitch = -7.5f;
    float endPitch = -60.0f;
    float currentPitch = 0.0f;

    float startFov = 24.0f;
    float endFov = 24.0f;
    float currentFov = 24.0f;


    std::unique_ptr<EasingRunner> easingPitchRunner;
    std::unique_ptr<EasingRunner> easingYawRunner;
    std::unique_ptr<EasingRunner> easingFovRunner;
};


