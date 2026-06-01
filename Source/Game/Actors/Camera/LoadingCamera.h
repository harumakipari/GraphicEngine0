#ifndef TITLE_CAMERA_H
#define TITLE_CAMERA_H

#include <DirectXMath.h>

#include "Core/Actor.h"
#include "Core/ActorManager.h"

#include "Components/Camera/CameraComponent.h"
#include "Game/Actors/Camera/Camera.h"

class LoadingCamera :public Camera
{
public:
    //引数付きコンストラクタ
    explicit LoadingCamera(const std::string& actorName) :Camera(actorName) {}

    virtual ~LoadingCamera() = default;


    void Initialize(const Transform& transform)override
    {
        mainCameraComponent = this->AddComponent<CameraComponent>("mainCamera");
        mainCameraComponent->SetPerspective(DirectX::XMConvertToRadians(35), Graphics::GetScreenWidth() / Graphics::GetScreenHeight(), 0.1f, 1000.0f);
    };

    //更新処理
    void Update(float deltaTime)override
    {
    }

    void DrawImGuiDetails()override
    {
    }

};

#endif // !TITLE_CAMERA_H
