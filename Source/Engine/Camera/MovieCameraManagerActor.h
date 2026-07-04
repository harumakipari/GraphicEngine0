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
        EnemyMovie,
        Finished
    };
public:
    explicit MovieCameraManagerActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform) override {}

    void Update(float dt) override;

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
};
