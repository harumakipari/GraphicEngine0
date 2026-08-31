#pragma once
#include "Components/Camera/CameraComponent.h"
#include "Core/Actor.h"

class MovieCameraManagerActor :public Actor
{
private:
    enum class DoorMovieState :uint8_t
    {
        None,
        DoorPreMovie,
        UpPlayerMovie,
        DoorOpening,
        EnemyEyeFlash,
        PreBossRoomLerp, // ボスの部屋が明るくなる
        BossRoomLerp,   // ボスの部屋が明るくなる
        UpPlayerCombat, // プレイヤーが剣を構える
        PreUpPlayerCombatMovie,
        UpPlayerCombatMovie,
        EnemyMovie,
        EnemyName,
        PreFinished,
        Finished
    };
public:
    explicit MovieCameraManagerActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform) override {}

    void Update(float dt) override;

    void DrawImGuiDetails() override;

    void SetMovieCameraComponent(std::shared_ptr<MovieCameraComponent> movieCameraComponent)
    {
        movieCameraWeakPtr = movieCameraComponent;
    }

    // ドアを開くムービーを再生する
    void PlayDoorMovie();

    void PlayMovie(const std::string& file);

    void PlayReverse(const std::string& file)
    {
        auto movieCamera = this->movieCameraWeakPtr.lock();
        movieCamera->LoadFromJson("./Data/Saves/MovieCameras/" + file);
        movieCamera->Start(true);
    }

private:
    DoorMovieState doorMovieState = DoorMovieState::None;
    std::weak_ptr<MovieCameraComponent> movieCameraWeakPtr;
    float elapsedTimer = 0.0f;
    float bossRoomZoomDuration = 3.0f;
    float bossRoomZoomTargetFovDegree = 20.0f;
    float bossRoomCameraMoveDistance =5.0f;
    float bossRoomZoomStartFov = DirectX::XMConvertToRadians(35.0f);
    float bossRoomZoomElapsed = 0.0f;
    DirectX::XMFLOAT3 bossRoomZoomStartPosition{};
    DirectX::XMFLOAT3 bossRoomZoomTargetPosition{};
    DirectX::XMFLOAT4 bossRoomZoomStartRotation{};
    DirectX::XMFLOAT4 bossRoomZoomTargetRotation{};
};
