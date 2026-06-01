#pragma once
#include "Components/Camera/CameraComponent.h"

class MovieManager
{
public:
    void Play(const std::string& file)
    {
        camera->LoadFromJson("./Data/Saves/MovieCameras/" + file);
        camera->Start();
    }

    void PlayReverse(const std::string& file)
    {
        camera->LoadFromJson("./Data/Saves/MovieCameras/" + file);
        camera->Start(true);
    }

private:
    std::shared_ptr<MovieCameraComponent> camera;
};
