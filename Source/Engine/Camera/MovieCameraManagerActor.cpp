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
    std::string bossNameMovieFileName = "grux.json";

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
                // イベントシーンが開始したことを通知する
                player->StartEvent();
                player->PlayBodyAnimation("Recall_0", false);
            }
            // 部屋のシャンデリアの炎の光を消す
            for (auto chandelier : chandelierActors)
            {
                chandelier->SetFireScaleToZero();
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
            if (gameScene)
            {
                // ボスの部屋を暗くする
                gameScene->SetBossRoomLerpFactor(0.0f);
                // 目のBloomのみをオンにする
                gameScene->SetEyeBloom(true);
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
            CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_groan.wav", 0.2f);
            // 敵の目玉が光る
            if (gruxEnemyEye)
            {
                gruxEnemyEye->StartEyeFlash([&]()
                    {
                        //doorMovieState = DoorMovieState::PreBossRoomLerp;
                    });
                doorMovieState = DoorMovieState::EnemyEyeFlash;
                doorMovieState = DoorMovieState::PreBossRoomLerp;
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
    {
        constexpr float duration = 3.0f;
        // ボスの目玉をなくす
        if (gruxEnemyEye)
        {
            gruxEnemyEye->ToSmallEyeModel(duration, [&, gameScene, gruxEnemy]()
                {
                    // 目のBloomのみをオフにして、Bloomをオンにする
                    gameScene->SetEyeBloom(false);
                    gruxEnemy->GetBodyAnimationController()->ResetAnimationRate();
                });
        }
        // 部屋を徐々に明るくする
        if (gameScene)
        {
            gameScene->StartBossRoomLerp(0.0f, 1.0f, duration, [&]()
                {
                    doorMovieState = DoorMovieState::UpPlayerCombat;
                });
        }
        // 部屋のシャンデリアの炎の光を徐々に戻す
        for (auto chandelier : chandelierActors)
        {
            chandelier->ResetFireLightScale(duration);
        }
        doorMovieState = DoorMovieState::BossRoomLerp;
    }
    break;
    case DoorMovieState::BossRoomLerp:
        break;
    case DoorMovieState::UpPlayerCombat:
        movieCamera->SetOnMovieStart([&, doorActor, player, gruxEnemy]()
            {
                // ドアを閉めた状態にする
                if (doorActor)
                {
                    doorActor->Closed();
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
                doorMovieState = DoorMovieState::UpPlayerCombatMovie;
            });

        // プレイヤーアップカメラワーク
        PlayMovie(playerCombatMovieFileName);
        // 部屋の蝋燭スタンドの炎の光を戻す
        for (auto candleStand : candleStandActors)
        {
            candleStand->ResetFireLightScale();
        }
        doorMovieState = DoorMovieState::PreUpPlayerCombatMovie;
        break;
    case DoorMovieState::PreUpPlayerCombatMovie:
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
        }
        doorMovieState = DoorMovieState::EnemyName;
        break;
    case DoorMovieState::EnemyName:
        if (movieCamera->IsMovieFinish())
        {
            // BGMを再生する
            auto bgmActors = GetOwnerScene()->GetActorManager()->GetActorsOfType <BgmActor>();
            for (auto bgmActor : bgmActors)
            {
                if (bgmActor->GetName() == "BossBgmActor")
                {
                    bgmActor->Play();
                }
            }
            PlayMovie(bossNameMovieFileName);
            if (gruxEnemy)
            {
                gruxEnemy->PlayBodyAnimation("TravelMode_Idle_0");
                gruxEnemy->StartGruxNamePerform(1.5f);
            }
            if (player)
            {
                player->SetPosition({ -1.3f,-0.1f,11.24f });
                player->SetEulerRotation({ 0.0f,90.0f,0.0f });
            }
            doorMovieState = DoorMovieState::PreFinished;
        }
        break;
    case DoorMovieState::PreFinished:
        if (movieCamera->IsMovieFinish())
        {
            if (player)
            {
                // カメラをボス戦時の状態に変更する
                player->SetIsBossBattle(true);
            }
            //カメラを三人称に戻す
            if (scene->GetCameraManager()->IsUseMovie())
            {// ムービーカメラが使用中の場合のみ切り替え
#if 0
                if (auto mainCamera = actorManager->GetActorOfType<MainCamera>())
                {
                    mainCamera->SetEye(player->GetCameraEyeComponent());
                    mainCamera->SetLookTarget(gruxEnemy->GetCameraTargetComponent());
                    //mainCamera->SetCameraMode(TPSCameraController::CameraMode::TPS);
                    mainCamera->StartBlend(dynamic_cast<Camera*>(movieCamera->GetOwner()), 2.0f, [&, mainCamera]()
                        {
                            mainCamera->SetCameraMode(TPSCameraController::CameraMode::BossBattle);
                        });
                    scene->GetCameraManager()->ToggleMovieCamera(GetOwnerConstScene());
                }
#else
                if (auto darkCameraActor = actorManager->GetActorOfType<DarkCameraActor>())
                {
                    DarkCameraActor::CameraPose start = darkCameraActor->CreatePoseFromMovie(movieCamera);
                    DarkCameraActor::CameraPose target = darkCameraActor->CreateFocusPose();
                    darkCameraActor->StartExternalBlend(start, target, 2.0f, [player]()
                        {
                            if (player)
                            {
                                // 演出がが終わったことを通知する
                                player->EndEvent();
                            }
                        }
                    );
                    scene->GetCameraManager()->ToggleMovieCamera(GetOwnerConstScene());
                }
#endif // 0

            }
            if (gruxEnemy)
            {// ここでボスの名前のUIを消す
                gruxEnemy->StartGruxNamePerform(1.0f, 1.0f, 0.0f);
            }
            doorMovieState = DoorMovieState::Finished;
        }
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
    // BGMを止める
    auto bgmActors = GetOwnerScene()->GetActorManager()->GetActorsOfType <BgmActor>();
    for (auto bgmActor : bgmActors)
    {
        if (bgmActor->GetName() == "GameBgmActor")
        {
            bgmActor->Stop();
        }
    }

    if (auto player = GetOwnerScene()->GetActorManager()->GetActorOfType < Player>())
    {
        // 演出が始まったことをことを通知する
        player->StartEvent();


        // プレイヤーの位置を固定する
        DirectX::XMFLOAT3 fixedPosition = { -7.6f,-0.073f,10.16f };
        player->SetPosition(fixedPosition); // プレイヤーの位置を固定する座標に設定
        player->rotationComponent->SetDirection({ 1.0f,0.0f,0.0f });
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