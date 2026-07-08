#include "pch.h"
#include "MovieCameraManagerActor.h"

#include "Engine/Scene/Scene.h"
#include "Engine/Camera/CameraManager.h"
#include "Game/Actors/Player/Player.h"
#include "Game/DarkGame/DarkActors/DarkStageCandelabraActor.h"
#include "Game/DarkGame/DarkActors/DarkStageChandelierActor.h"
#include "Game/DarkGame/DarkActors/DoorActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemyEyeActor.h"
#include "Game/Scenes/GameScene.h"

void MovieCameraManagerActor::Update(float deltaTime)
{
    std::string playerMovieFileName = "player_up.json";
    std::string doorOpenMovieFileName = "door_open.json";
    std::string playerCombatMovieFileName = "door_player_prepare.json";
    std::string bossRoarMovieFileName = "boss_roar.json";

    auto movieCamera = this->movieCameraWeakPtr.lock();
    if (!movieCamera)
    {
        Logger::Warning(U8("movieCamera is nullptr!"));
    }

    auto scene = GetOwnerScene();
    if (!scene)
    {
        Logger::Warning(U8("scene is nullptr!"));
    }

    auto actorManager = scene->GetActorManager();
    if (!actorManager)
    {
        Logger::Warning(U8("actorManager is nullptr!"));
    }

    auto doorActor = actorManager->GetActorOfType<DoorLargeActor>();
    auto player = actorManager->GetActorOfType<Player>();
    auto chandelierActors = actorManager->GetActorsOfType<DarkStageChandelierActor>();
    auto candleStandActors = actorManager->GetActorsOfType<DarkStageCandleStandActor>();
    auto gameScene = dynamic_cast<GameScene*>(scene);

    auto gruxEnemy = actorManager->GetActorOfType<GruxEnemy>();
    auto gruxEnemyEye = actorManager->GetActorOfType<GruxEnemyEyeActor>();

    switch (doorMovieState)
    {
    case DoorMovieState::None:
        break;
    case DoorMovieState::DoorPreMovie:
        if (movieCamera->IsMovieFinish())
        {// 動画が終了したらplayerをアップする動画再生
            PlayMovie(playerMovieFileName);
            doorMovieState = DoorMovieState::UpPlayerMovie;
            if (player)
            {
                player->SetInputEnabled(false); // 入力を無効化する
                player->PlayBodyAnimation("Recall_0", false);
            }
            // 部屋のシャンデリアの炎の光を消す
            for (auto chandelier : chandelierActors)
            {
                chandelier->SetFireLightScale({ 0.0f,0.0f,0.0f });
            }
            // 部屋の蝋燭スタンドの炎の光を消す
            for (auto candleStand : candleStandActors)
            {
                candleStand->SetFireLightScale({ 0.0f,0.0f,0.0f });
            }
        }
        break;
    case DoorMovieState::UpPlayerMovie:
    {
        if (movieCamera->IsMovieFinish())
        {// プレイヤーをアップする動画が終わったら、
            if (player)
            {// プレイヤーのアニメーションを待機に変更
                player->PlayBodyAnimation("Idle", true);
            }
            // Bloomの値を落ち着ける
            if (scene)
            {
                auto& shader = scene->GetSceneSettings();
                shader.bloomConstantBuffer.bloomExtractionThreshold = 7.83f;
                shader.bloomConstantBuffer.bloomIntensity = 0.058f;
            }

            // ボスの部屋を暗くする
            if (gameScene)
            {
                gameScene->SetBossRoomLerpFactor(0.0f);
            }
            // ドアが開くカメラワーク
            PlayMovie(doorOpenMovieFileName);
            // ドアが開くアニメーション
            if (doorActor)
            {
                doorActor->Open();
            }
            // 敵のアニメーションを止める
            if (gruxEnemy)
            {
                gruxEnemy->PlayBodyAnimation("TravelMode_Idle_0");
                gruxEnemy->SetBodyAnimationRate(0.0f);
            }
            doorMovieState = DoorMovieState::DoorOpening;
        }
    }
    break;
    case DoorMovieState::DoorOpening:
        if (doorActor->IsOpenDoor())
        {// ドアが開いたら、
            // 敵の目玉が光る
            if (gruxEnemyEye)
            {
                gruxEnemyEye->StartEyeFlash([&]()
                    {
                        doorMovieState = DoorMovieState::PreBossRoomLerp;
                    });
                doorMovieState = DoorMovieState::EnemyEyeFlash;
            }

            if (player)
            {
                player->SetEulerRotation({ 0.0f,108.3f,0.0f });
            }
        }
        break;
    case DoorMovieState::EnemyEyeFlash:
        break;
    case DoorMovieState::PreBossRoomLerp:
        // Bloomの値を落ち着ける
        //if (scene)
        //{
        //    auto& shader = scene->GetSceneSettings();
        //    shader.bloomConstantBuffer.bloomExtractionThreshold = 9.0f;
        //    shader.bloomConstantBuffer.bloomIntensity = 0.415f;
        //}
        // 部屋を徐々に明るくする
        if (gameScene)
        {
            gameScene->StartBossRoomLerp(0.0f, 1.0f, 5.0f, [&]()
                {
                    doorMovieState = DoorMovieState::UpPlayerCombat;
                });
        }
        doorMovieState = DoorMovieState::BossRoomLerp;
        break;
    case DoorMovieState::BossRoomLerp:
        // ボスの目玉をなくす
        if (gruxEnemyEye)
        {
            gruxEnemyEye->ToSmallEyeModel();
        }
        break;
    case DoorMovieState::UpPlayerCombat:
        // 部屋のシャンデリアの炎の光を戻す
        for (auto chandelier : chandelierActors)
        {
            chandelier->ResetFireLightScale();
        }
        // 部屋の蝋燭スタンドの炎の光を戻す
        for (auto candleStand : candleStandActors)
        {
            candleStand->ResetFireLightScale();
        }

        // ドアを閉めた状態にする
        if (doorActor)
        {
            doorActor->SetState(DoorLargeActor::DoorState::Closed);
        }

        // プレイヤーの位置をドア前にする
        if (player)
        {
            player->SetPosition({ -4.827f,-0.098f,11.724f });
            // プレイヤーのアニメーションを再生
            player->PlayBodyAnimation("Level_Start_Cut", false);
        }
        // 敵の位置を部屋の奥にする
        if (gruxEnemy)
        {
            gruxEnemy->SetPosition({ 12.795f,-0.12f,10.774f });
            gruxEnemy->SetEulerRotation({ 0.0f,-90.0f,0.0f });
        }
        // プレイヤーアップカメラワーク
        PlayMovie(playerCombatMovieFileName);
        doorMovieState = DoorMovieState::UpPlayerCombatMovie;
        break;
    case DoorMovieState::UpPlayerCombatMovie:
        if (movieCamera->IsMovieFinish())
        {// カメラワークが終わったら、
            doorMovieState = DoorMovieState::EnemyMovie;
        }
        break;
    case DoorMovieState::EnemyMovie:
        // ボスアップカメラワーク
        PlayMovie(bossRoarMovieFileName);
        // 敵が吠える
        if (gruxEnemy)
        {
            gruxEnemy->PlayBodyAnimation("Ultimate_Roar_0", false);
            gruxEnemy->GetBodyAnimationController()->ResetAnimationRate();
        }
        doorMovieState = DoorMovieState::Finished;
        break;
    case DoorMovieState::Finished:
        if (player)
        {
            player->SetInputEnabled(true); // 入力を有効化する
        }

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
        player->SetInputEnabled(false);
        DirectX::XMFLOAT3 fixedPosition = { -7.6f,-0.073f,10.16f };
        player->SetPosition(fixedPosition); // プレイヤーの位置を固定する座標に設定
        player->SetEulerRotation({ 0.0f,83.3f,0.0f });
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