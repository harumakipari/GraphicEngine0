#include "pch.h"
#include "MovieCameraManagerActor.h"

#include "Engine/Scene/Scene.h"
#include "Engine/Camera/CameraManager.h"
#include "Game/Actors/Player/Player.h"
#include "Game/DarkGame/DarkActors/DoorActor.h"

void MovieCameraManagerActor::Update(float deltaTime)
{
    std::string playerMovieFileName = "player_up.json";
    std::string doorOpenMovieFileName = "door_open.json";

    auto movieCamera = this->movieCameraWeakPtr.lock();

    if (!movieCamera)
    {
        Logger::Warning(U8("movieCamera is nullptr!"));
    }

    switch (doorMovieState)
    {
    case DoorMovieState::None:
        break;
    case DoorMovieState::DoorPreMovie:
        if (movieCamera->IsMovieFinish())
        {// 動画が終了したらplayerをアップする動画再生
            PlayMovie(playerMovieFileName);
            doorMovieState = DoorMovieState::UpPlayerMovie;
        }
        break;
    case DoorMovieState::UpPlayerMovie:
    {
        if (movieCamera->IsMovieFinish())
        {
            PlayMovie(doorOpenMovieFileName);
            if (auto doorLargeActor = GetOwnerScene()->GetActorManager()->GetActorOfType<DoorLargeActor>())
            {
                doorLargeActor->Open();
            }
            doorMovieState = DoorMovieState::DoorOpening;
        }
    }
        break;
    case DoorMovieState::DoorOpening:
        break;
    case DoorMovieState::EnemyMovie:
        break;
    case DoorMovieState::Finished:
        break;
    }

}

void MovieCameraManagerActor::PlayMovie(const std::string& file)
{
    if (auto cameraManager = GetOwnerScene()->GetCameraManager())
    {
        if (!cameraManager->IsUseMovie())
        {// すでにムービーカメラが使用中でない場合のみ切り替え
            cameraManager->ToggleMovieCamera(GetOwnerConstScene());
        }
    }
    auto movieCamera = this->movieCameraWeakPtr.lock();
    movieCamera->LoadFromJson("./Data/Saves/MovieCameras/" + file);
    movieCamera->Start();
}

// ドアを開くムービーを再生する
void MovieCameraManagerActor::PlayDoorMovie()
{
    // プレイヤーの操作を無効化して位置を固定する
    if (auto player = GetOwnerScene()->GetActorManager()->GetActorOfType < Player>())
    {
        //player->SetInputEnabled(false);
        DirectX::XMFLOAT3 fixedPosition = { -7.6f,-0.073f,10.16f };
        player->SetPosition(fixedPosition); // プレイヤーの位置を固定する座標に設定
    }


    if (auto cameraManager = GetOwnerScene()->GetCameraManager())
    {
        if (!cameraManager->IsUseMovie())
        {// すでにムービーカメラが使用中でない場合のみ切り替え
            cameraManager->ToggleMovieCamera(GetOwnerConstScene());
        }
    }
    std::string fileName = "door_pre_open.json";
    auto movieCamera = this->movieCameraWeakPtr.lock();
    movieCamera->LoadFromJson("./Data/Saves/MovieCameras/" + fileName);
    movieCamera->Start();
    doorMovieState = DoorMovieState::DoorPreMovie;
}