#pragma once
#include <memory>
#include <string>
#include "Engine/Camera/CameraConstants.h"

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
    void Update(float deltaTime);
    void PlayCameraShake(float intensity, float duration, float frequency, float positionAmount, float targetAmount, const std::string& presetName = {});
    void PlayCameraShakePreset(const std::string& presetName);
    void ClearCameraShake();
    ViewConstants GetRenderViewConstants(const Scene* scene);
#ifdef USE_IMGUI
    void DrawImGuiDetails(const Scene* scene) const;
#endif

    void Clear()
    {
        //gameCamera.reset();
        debugCamera.reset();
        useDebugCamera = false;
        cinematicCamera.reset();
        useCinematicCamera = false;
        movieCamera.reset();
        useMovieCamera = false;
        ClearCameraShake();
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
    bool shakeActive = false;
    float shakeElapsed = 0.0f, shakeDuration = 0.0f, shakeIntensity = 0.0f, shakeFrequency = 0.0f;
    float shakePositionAmount = 0.0f, shakeTargetAmount = 0.0f;
    float shakePositionSide = 0.0f, shakePositionVertical = 0.0f;
    float shakeTargetYaw = 0.0f, shakeTargetPitch = 0.0f;
    std::string shakePresetName;
};
