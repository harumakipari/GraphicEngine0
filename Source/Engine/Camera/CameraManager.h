#pragma once
#include <memory>

//#include "Engine/Scene/SceneBase.h"

class Camera;
class Scene;

class CameraManager
{
public:
    void ToggleCamera(const Scene* scene);

    void ToggleCinematicCamera(const Scene* scene);

    void ToggleMovieCamera(const Scene* scene);

    Camera* GetRenderCamera(const Scene* scene) const;

    void Clear()
    {
        //gameCamera.reset();
        debugCamera.reset();
        useDebugCamera = false;
        cinematicCamera.reset();
        useCinematicCamera = false;
        movieCamera.reset();
        useMovieCamera = false;
    }

    bool IsUseDebug() const { return useDebugCamera; }
    bool IsUseCinematic() const { return useCinematicCamera; }
    bool IsUseMovie() const { return useMovieCamera; }

    //void SetGameCamera(const std::weak_ptr<Camera>& camera) { gameCamera = camera; }
    void SetDebugCamera(const std::shared_ptr<Camera>& camera) { debugCamera = camera; }

    void SetCinematicCamera(const std::shared_ptr<Camera>& camera) { cinematicCamera = camera; }

    void SetMovieCamera(const std::shared_ptr<Camera>& camera) { movieCamera = camera; }
private:
    std::weak_ptr<Camera> debugCamera;
    std::weak_ptr<Camera> cinematicCamera;
    std::weak_ptr<Camera> movieCamera;

    bool useDebugCamera = false;
    bool useCinematicCamera = false;
    bool useMovieCamera = false;
};
