#include "pch.h"
#include "MovieCameraManagerActor.h"
#include <fstream>
#include <json.hpp>

#include "Engine/Scene/Scene.h"
#include "Engine/Camera/CameraManager.h"
#include "Game/Actors/Player/Player.h"
#include "Game/DarkGame/DarkActors/DarkStageCandelabraActor.h"
#include "Game/DarkGame/DarkActors/DarkStageChandelierActor.h"
#include "Game/DarkGame/DarkActors/DoorActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Scenes/GameScene.h"

void MovieCameraManagerActor::Initialize(const Transform& transform)
{
    LoadPrisonOpeningEyeSettings();
}

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
    if (prisonOpeningMoviePlaying)
    {
        const auto cameraManager = scene->GetCameraManager();
        if (!movieCamera || !gameScene || !cameraManager)
        {
            FinishPrisonOpeningMovie();
        }
        else
        {
            if (cameraManager->IsUseCinematic())
                cameraManager->ToggleCinematicCamera(GetOwnerConstScene());

            if (!cameraManager->IsUseMovie())
            {
                FinishPrisonOpeningMovie();
            }
            else if (prisonOpeningState == PrisonOpeningState::CameraPathPlaying)
            {
                UpdatePrisonOpeningEyeOpening();
                if (movieCamera->IsMovieFinish())
                    HandlePrisonOpeningCameraPathCompleted();
                else if (!movieCamera->IsPlaying())
                    FinishPrisonOpeningMovie();
            }
            else
            {
                UpdatePrisonOpeningPresentation(deltaTime);
            }
        }
    }

    switch (doorMovieState)
    {
    case DoorMovieState::None:
        break;
    case DoorMovieState::DoorPreMovie:
        if (movieCamera->IsMovieFinish())
        {
            PlayMovie(playerMovieFileName);
            doorMovieState = DoorMovieState::UpPlayerMovie;
            if (player)
            {
                player->StartEvent();
                player->PlayBodyAnimation("Recall_0", false);
            }
            for (auto chandelier : chandelierActors)
            {
                chandelier->SetFireScaleToZero();
            }
            for (auto candleStand : candleStandActors)
            {
                candleStand->SetFireLightScale({ 0.0f,0.0f,0.0f });
            }
        }
        break;
    case DoorMovieState::UpPlayerMovie:
    {
        if (movieCamera->IsMovieFinish())
        {
            if (player)
            {
                player->PlayBodyAnimation("Idle", true);
            }
            if (gameScene)
            {
                gameScene->SetBossRoomLerpFactor(0.0f);
            }
            PlayMovie(doorOpenMovieFileName);
            if (doorActor)
            {
                doorActor->Open();
            }
            if (gruxEnemy)
            {
                gruxEnemy->PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.3f, false);
                gruxEnemy->SetBodyAnimationRate(0.0f);
            }
            doorMovieState = DoorMovieState::DoorOpening;
        }
    }
    break;
    case DoorMovieState::DoorOpening:
        if (doorActor->IsOpenDoor())
        {
            CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_groan.wav", 0.2f);
            doorMovieState = DoorMovieState::PreBossRoomLerp;
            if (player)
            {
                player->SetEulerRotation({ 0.0f,108.3f,0.0f });
            }
        }
        break;
    case DoorMovieState::PreBossRoomLerp:
    {
        const float duration = (std::max)(bossRoomZoomDuration, 0.01f);
        bossRoomZoomStartFov = movieCamera->GetFov();
        bossRoomZoomElapsed = 0.0f;
        bossRoomZoomStartPosition = movieCamera->GetOwner()->GetPosition();
        bossRoomZoomStartRotation = movieCamera->GetOwner()->GetQuaternionRotation();
        const DirectX::XMFLOAT3 forward = movieCamera->GetForward();
        bossRoomZoomTargetPosition =
        {
            bossRoomZoomStartPosition.x + forward.x * bossRoomCameraMoveDistance,
            bossRoomZoomStartPosition.y + forward.y * bossRoomCameraMoveDistance,
            bossRoomZoomStartPosition.z + forward.z * bossRoomCameraMoveDistance
        };
        bossRoomZoomTargetRotation = bossRoomZoomStartRotation;
        if (gruxEnemy)
        {
            if (const auto& cameraTarget = gruxEnemy->GetCameraTargetComponent())
            {
                const DirectX::XMFLOAT3 targetDirection = MathHelper::Normalize(
                    MathHelper::Subtract(
                        cameraTarget->GetComponentLocation(),
                        bossRoomZoomTargetPosition));
                bossRoomZoomTargetRotation = MathHelper::LookRotation(
                    targetDirection,
                    { 0.0f, 1.0f, 0.0f });
            }
        }
        if (gameScene)
        {
            const std::weak_ptr<MovieCameraManagerActor> managerWeak =
                std::dynamic_pointer_cast<MovieCameraManagerActor>(shared_from_this());
            const std::weak_ptr<MovieCameraComponent> movieCameraWeak =
                movieCameraWeakPtr;
            gameScene->StartBossRoomLerp(
                0.0f, 1.0f, duration,
                [managerWeak, movieCameraWeak, gruxEnemy]()
                {
                    const auto manager = managerWeak.lock();
                    if (!manager)
                        return;

                    if (const auto camera = movieCameraWeak.lock())
                    {
                        camera->GetOwner()->SetPosition(manager->bossRoomZoomTargetPosition);
                        camera->GetOwner()->SetQuaternionRotation(manager->bossRoomZoomTargetRotation);
                        camera->SetFov(DirectX::XMConvertToRadians(manager->bossRoomZoomTargetFovDegree));
                    }
                    if (gruxEnemy)
                    {
                        if (const auto controller = gruxEnemy->GetBodyAnimationController())
                            controller->ResetAnimationRate();
                    }
                    manager->doorMovieState = DoorMovieState::UpPlayerCombat;
                });
        }
        for (auto chandelier : chandelierActors)
        {
            chandelier->ResetFireLightScale(duration);
        }
        doorMovieState = DoorMovieState::BossRoomLerp;
    }
    break;
    case DoorMovieState::BossRoomLerp:
    {
        bossRoomZoomElapsed = (std::min)(
            bossRoomZoomElapsed + deltaTime,
            (std::max)(bossRoomZoomDuration, 0.01f));

        const float normalizedTime = std::clamp(
            bossRoomZoomElapsed / (std::max)(bossRoomZoomDuration, 0.01f),
            0.0f,
            1.0f);
        const float eased = normalizedTime * normalizedTime * (3.0f - 2.0f * normalizedTime);
        DirectX::XMFLOAT3 position{};
        position.x = std::lerp(bossRoomZoomStartPosition.x, bossRoomZoomTargetPosition.x, eased);
        position.y = std::lerp(bossRoomZoomStartPosition.y, bossRoomZoomTargetPosition.y, eased);
        position.z = std::lerp(bossRoomZoomStartPosition.z, bossRoomZoomTargetPosition.z, eased);
        movieCamera->GetOwner()->SetPosition(position);

        DirectX::XMVECTOR startRotation = DirectX::XMLoadFloat4(&bossRoomZoomStartRotation);
        DirectX::XMVECTOR targetRotation = DirectX::XMLoadFloat4(&bossRoomZoomTargetRotation);
        startRotation = DirectX::XMQuaternionNormalize(startRotation);
        targetRotation = DirectX::XMQuaternionNormalize(targetRotation);
        if (DirectX::XMVectorGetX(DirectX::XMVector4Dot(startRotation, targetRotation)) < 0.0f)
        {
            targetRotation = DirectX::XMVectorNegate(targetRotation);
        }
        const DirectX::XMVECTOR rotation = DirectX::XMQuaternionSlerp(
            startRotation,
            targetRotation,
            eased);
        DirectX::XMFLOAT4 quaternion{};
        DirectX::XMStoreFloat4(&quaternion, rotation);
        movieCamera->GetOwner()->SetQuaternionRotation(quaternion);

        const float targetFov = DirectX::XMConvertToRadians(bossRoomZoomTargetFovDegree);
        movieCamera->SetFov(std::lerp(bossRoomZoomStartFov, targetFov, eased));
    }
    break;
    case DoorMovieState::UpPlayerCombat:
        movieCamera->SetOnMovieStart([&, doorActor, player, gruxEnemy]()
            {
                if (doorActor)
                {
                    doorActor->Closed();
                }
                if (player)
                {
                    player->SetPosition({ -4.827f,-0.098f,11.724f });
                    player->PlayBodyAnimation("Level_Start_Cut", false);
                }
                if (gruxEnemy)
                {
                    gruxEnemy->SetEulerRotation({ 0.0f,-90.0f,0.0f });
                }
                doorMovieState = DoorMovieState::UpPlayerCombatMovie;
            });

        PlayMovie(playerCombatMovieFileName);
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
        {
            doorMovieState = DoorMovieState::EnemyMovie;
        }
        break;
    case DoorMovieState::EnemyMovie:
        PlayMovie(bossRoarMovieFileName);
        if (gruxEnemy)
        {
            gruxEnemy->PlayBodyAnimation("Ultimate_Roar_0", false);
        }
        doorMovieState = DoorMovieState::EnemyName;
        break;
    case DoorMovieState::EnemyName:
        if (movieCamera->IsMovieFinish())
        {
            if (gameScene)
                gameScene->PlayBossBattleBgm();
            PlayMovie(bossNameMovieFileName);
            if (gruxEnemy)
            {
                gruxEnemy->PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.3f, false);
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
                player->SetIsBossBattle(true);
            }
            if (scene->GetCameraManager()->IsUseMovie())
            {

                if (auto darkCameraActor = actorManager->GetActorOfType<DarkCameraActor>())
                {
                    DarkCameraActor::CameraPose start = darkCameraActor->CreatePoseFromMovie(movieCamera);
                    DarkCameraActor::CameraPose target = darkCameraActor->CreateFocusPose();
                    bossEntryExternalBlendActive = true;
                    darkCameraActor->StartExternalBlend(start, target, 2.0f, [this, player, gruxEnemy, gameScene]()
                        {
                            bossEntryExternalBlendActive = false;
                            FinishBossEntryToBattle();
                        }
                    );
                    scene->GetCameraManager()->ToggleMovieCamera(GetOwnerConstScene());
                }

            }
            if (gruxEnemy)
            {
                gruxEnemy->StartGruxNamePerform(1.0f, 1.0f, 0.0f);
            }
            doorMovieState = DoorMovieState::Finished;
        }
    case DoorMovieState::Finished:


        break;
    }
}

void MovieCameraManagerActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::SeparatorText("Prison Opening (Runtime)");
    ImGui::Text("Playing: %s", prisonOpeningMoviePlaying ? "true" : "false");
    if (ImGui::Button("Play Prison Opening Movie (Runtime)"))
        PlayPrisonOpeningMovie();
    const bool canReplayPrisonOpening = CanReplayPrisonOpeningMovie();
    if (!canReplayPrisonOpening)
        ImGui::BeginDisabled();
    if (ImGui::Button("Replay Prison Opening"))
        ReplayPrisonOpeningMovie();
    if (!canReplayPrisonOpening)
        ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("Replay: %s", canReplayPrisonOpening ? "Ready" : "Unavailable");
    ImGui::SeparatorText("Prison Opening Sitting Motion");
    int sittingMotionMode = static_cast<int>(prisonOpeningSittingMotionMode);
    if (ImGui::Combo("Sitting Motion Mode", &sittingMotionMode,
        "Original Sitting\0Standing Ping-Pong\0\0"))
    {
        prisonOpeningSittingMotionMode =
            static_cast<PrisonOpeningSittingMotionMode>(sittingMotionMode);
    }
    ImGui::DragFloat("Ping-Pong Start Time", &prisonOpeningSittingPingPongStartTime,
        0.01f, 0.0f, 10.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Ping-Pong End Time", &prisonOpeningSittingPingPongEndTime,
        0.01f, 0.001f, 10.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
    prisonOpeningSittingPingPongEndTime = (std::max)(
        prisonOpeningSittingPingPongStartTime + 0.001f, prisonOpeningSittingPingPongEndTime);
    ImGui::DragFloat("Ping-Pong Speed", &prisonOpeningSittingPingPongSpeed,
        0.01f, 0.1f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Sitting to Standing Blend Time", &prisonOpeningSittingToStandingBlendTime,
        0.01f, 0.0f, 0.5f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    const auto prisonPlayer = GetOwnerScene()
        ? GetOwnerScene()->GetActorManager()->GetActorOfType<Player>() : nullptr;
    if (prisonPlayer)
    {
        prisonPlayer->ConfigurePrisonOpeningSittingMotion(
            prisonOpeningSittingMotionMode == PrisonOpeningSittingMotionMode::StandingPingPong,
            prisonOpeningSittingPingPongStartTime, prisonOpeningSittingPingPongEndTime,
            prisonOpeningSittingPingPongSpeed, prisonOpeningSittingToStandingBlendTime);
    }
    ImGui::DragFloat("Prison Opening TPS Blend Time", &prisonOpeningBlendDuration,
        0.01f, 0.1f, 3.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("Eye Opening Enabled", &prisonOpeningEyeOpeningEnabled);
    ImGui::DragFloat("Eye Open Delay", &prisonOpeningEyeOpenDelay,
        0.01f, 0.0f, 30.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Eye Open Duration", &prisonOpeningEyeOpenDuration,
        0.01f, 0.01f, 10.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("Eye Opening Preview", &prisonOpeningEyeOpeningPreview);
    if (prisonOpeningEyeOpeningPreview)
        ImGui::DragFloat("Eye Opening Preview Time", &prisonOpeningEyeOpeningPreviewTime,
            0.01f, 0.0f, 30.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::Button("Save Prison Opening Settings"))
        SavePrisonOpeningEyeSettings();
    ImGui::SameLine();
    if (ImGui::Button("Load Prison Opening Settings"))
        LoadPrisonOpeningEyeSettings();

    ImGui::DragFloat("Boss Room Zoom Duration", &bossRoomZoomDuration,
        0.05f, 0.01f, 10.0f, "%.2f sec");
    ImGui::DragFloat("Boss Room Zoom Target FOV", &bossRoomZoomTargetFovDegree,
        0.1f, 10.0f, 60.0f, "%.1f deg");
    ImGui::DragFloat("Boss Room Camera Move Distance", &bossRoomCameraMoveDistance,
        0.05f, 0.0f, 3.0f, "%.2f m");
#endif
}

void MovieCameraManagerActor::PlayDoorMovie()
{
    bossEntryBattleStarted = false;
    bossEntryExternalBlendActive = false;
    if (auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene()))
    {
        gameScene->PrepareTutorialForBossEntry();
        gameScene->EnterBossRoomLockOnScope();
    }

    if (auto player = GetOwnerScene()->GetActorManager()->GetActorOfType < Player>())
    {
        player->StartEvent();


        DirectX::XMFLOAT3 fixedPosition = { -7.6f,-0.073f,10.16f };
        player->SetPosition(fixedPosition);
        player->rotationComponent->SetDirection({ 1.0f,0.0f,0.0f });
    }

    if (auto cameraManager = GetOwnerScene()->GetCameraManager())
    {
        if (!cameraManager->IsUseMovie())
        {
            cameraManager->ToggleMovieCamera(GetOwnerConstScene());
        }
    }
    std::string fileName = "door_pre_open.json";
    auto movieCamera = this->movieCameraWeakPtr.lock();
    movieCamera->LoadFromJson("./Data/Saves/MovieCameras/" + fileName);
    movieCamera->Start();
    doorMovieState = DoorMovieState::DoorPreMovie;
}

bool MovieCameraManagerActor::PreparePrisonOpeningMovieFirstFrame()
{
    auto scene = GetOwnerScene();
    auto movieCamera = movieCameraWeakPtr.lock();
    auto cameraManager = scene ? scene->GetCameraManager() : nullptr;
    if (!scene || !movieCamera || !cameraManager || prisonOpeningMoviePlaying)
        return false;

    if (prisonOpeningMoviePrepared)
        return cameraManager->IsUseMovie();

    movieCamera->LoadFromJson(
        "./Data/Saves/MovieCameras/prison_opening.json");
    UpdatePrisonOpeningFaceCloseUpKeyTime();
    if (!movieCamera->ApplyFirstFrameToOwner())
        return false;

    if (!cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(GetOwnerConstScene());
    movieCamera->ApplyFirstFrameToOwner();

    prisonOpeningMoviePrepared = true;
    return true;
}

void MovieCameraManagerActor::CancelPreparedPrisonOpeningMovie()
{
    if (!prisonOpeningMoviePrepared || prisonOpeningMoviePlaying)
        return;

    auto scene = GetOwnerScene();
    auto cameraManager = scene ? scene->GetCameraManager() : nullptr;
    if (cameraManager && cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(GetOwnerConstScene());

    prisonOpeningMoviePrepared = false;
}

bool MovieCameraManagerActor::PlayPrisonOpeningMovie()
{
    auto scene = GetOwnerScene();
    auto gameScene = dynamic_cast<GameScene*>(scene);
    auto movieCamera = movieCameraWeakPtr.lock();
    auto cameraManager = scene ? scene->GetCameraManager() : nullptr;
    if (!gameScene || !movieCamera || !cameraManager || prisonOpeningMoviePlaying)
        return false;

    if (cameraManager->IsUseCinematic())
        cameraManager->ToggleCinematicCamera(GetOwnerConstScene());
    if (const auto player = scene->GetActorManager()->GetActorOfType<Player>())
    {
        player->ConfigurePrisonOpeningSittingMotion(
            prisonOpeningSittingMotionMode == PrisonOpeningSittingMotionMode::StandingPingPong,
            prisonOpeningSittingPingPongStartTime, prisonOpeningSittingPingPongEndTime,
            prisonOpeningSittingPingPongSpeed, prisonOpeningSittingToStandingBlendTime);
    }


    if (!gameScene->BeginPrisonOpeningMoviePlayerSitting())
    {
        CancelPreparedPrisonOpeningMovie();
        return false;
    }

    if (!prisonOpeningMoviePrepared)
    {
        if (!cameraManager->IsUseMovie())
            cameraManager->ToggleMovieCamera(GetOwnerConstScene());

        movieCamera->LoadFromJson(
            "./Data/Saves/MovieCameras/prison_opening.json");
        UpdatePrisonOpeningFaceCloseUpKeyTime();
    }
    else if (!cameraManager->IsUseMovie())
    {
        gameScene->EndPrisonOpeningMoviePlayerSitting();
        CancelPreparedPrisonOpeningMovie();
        return false;
    }
    movieCamera->Start();
    if (!movieCamera->IsPlaying())
    {
        gameScene->EndPrisonOpeningMoviePlayerSitting();
        if (prisonOpeningMoviePrepared)
            CancelPreparedPrisonOpeningMovie();
        else if (cameraManager->IsUseMovie())
            cameraManager->ToggleMovieCamera(GetOwnerConstScene());
        return false;
    }
    UpdatePrisonOpeningFaceCloseUpKeyTime();

    const auto player = scene->GetActorManager()->GetActorOfType<Player>();
    prisonOpeningEyeOverrideActive = player && player->BeginPrisonOpeningEyeClosed();
    if (!prisonOpeningEyeOverrideActive)
        Logger::Warning("[PrisonOpeningMovie] Failed to begin Player eye-close override.");

    prisonOpeningMoviePrepared = false;
    prisonOpeningMoviePlaying = true;
    prisonOpeningJailDoorInteractionRequested = false;
    prisonOpeningState = PrisonOpeningState::CameraPathPlaying;
    return true;
}

void MovieCameraManagerActor::SkipPrisonOpeningToGameplay()
{
    if (!prisonOpeningMoviePlaying)
        return;

    const auto scene = GetOwnerScene();
    if (scene)
    {
        if (const auto jailDoor = scene->GetActorManager()->GetActorOfType<DoorJailActor>())
            jailDoor->SetOpenCompletedSilently();

        if (const auto darkCamera = scene->GetActorManager()->GetActorOfType<DarkCameraActor>())
        {
            DarkCameraActor::PrisonOpeningTpsPose target{};
            if (darkCamera->GetPrisonOpeningTpsPose(target))
                darkCamera->PreparePrisonOpeningTpsReturn(target);
        }
    }

    FinishPrisonOpeningMovie();
}

void MovieCameraManagerActor::SkipBossEntryToBattle()
{
    if (!IsBossEntryMovieActive() || bossEntryBattleStarted)
        return;

    const auto scene = GetOwnerScene();
    if (!scene)
        return;

    const auto actorManager = scene->GetActorManager();
    const auto gameScene = dynamic_cast<GameScene*>(scene);
    const auto player = actorManager->GetActorOfType<Player>();
    const auto gruxEnemy = actorManager->GetActorOfType<GruxEnemy>();
    const auto darkCamera = actorManager->GetActorOfType<DarkCameraActor>();

    if (const auto doorActor = actorManager->GetActorOfType<DoorLargeActor>())
    {
        doorActor->Closed();
        doorActor->CompleteInteraction();
    }

    if (gameScene)
    {
        gameScene->CancelBossRoomLerp();
        gameScene->SetBossRoomLerpFactor(1.0f);
    }

    if (const auto movieCamera = movieCameraWeakPtr.lock())
    {
        movieCamera->SetOnMovieStart(nullptr);
        movieCamera->Stop();
    }
    if (const auto cameraManager = scene->GetCameraManager(); cameraManager && cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(GetOwnerConstScene());

    if (player)
    {
        player->SetPosition({ -1.3f, -0.1f, 11.24f });
        player->SetEulerRotation({ 0.0f, 90.0f, 0.0f });
        player->PlayBodyAnimation("Idle", true);
    }
    if (gruxEnemy)
    {
        gruxEnemy->SetEulerRotation({ 0.0f, -90.0f, 0.0f });
        gruxEnemy->PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.3f, false);
        if (const auto controller = gruxEnemy->GetBodyAnimationController())
            controller->ResetAnimationRate();
        gruxEnemy->StartGruxNamePerform(1.0f, 1.0f, 0.0f);
    }

    if (darkCamera)
        darkCamera->SnapToExternalPose(darkCamera->CreateFocusPose());

    FinishBossEntryToBattle();
}

void MovieCameraManagerActor::PlayMovie(const std::string& file)
{
    if (auto cameraManager = GetOwnerScene()->GetCameraManager())
    {
        if (!cameraManager->IsUseMovie())
        {
            cameraManager->ToggleMovieCamera(GetOwnerConstScene());
        }
    }
    auto movieCamera = this->movieCameraWeakPtr.lock();
    movieCamera->LoadFromJson("./Data/Saves/MovieCameras/" + file);
    movieCamera->Start();
}

void MovieCameraManagerActor::HandlePrisonOpeningCameraPathCompleted()
{
    if (!prisonOpeningMoviePlaying ||
        prisonOpeningState != PrisonOpeningState::CameraPathPlaying)
        return;

    const auto scene = GetOwnerScene();
    const auto jailDoor = scene
        ? scene->GetActorManager()->GetActorOfType<DoorJailActor>()
        : nullptr;
    if (!jailDoor)
    {
        Logger::Warning("[PrisonOpeningMovie] DoorJailActor was not found.");
        FinishPrisonOpeningMovie();
        return;
    }

    prisonOpeningState = PrisonOpeningState::WaitingForJailDoorOpen;
    if (!prisonOpeningJailDoorInteractionRequested &&
        !jailDoor->IsOpenDoor() && !jailDoor->IsOpeningDoor())
    {
        jailDoor->Interact();
        prisonOpeningJailDoorInteractionRequested = true;
    }
}

void MovieCameraManagerActor::UpdatePrisonOpeningPresentation(float deltaTime)
{
    const auto scene = GetOwnerScene();
    if (!scene)
    {
        FinishPrisonOpeningMovie();
        return;
    }

    switch (prisonOpeningState)
    {
    case PrisonOpeningState::WaitingForJailDoorOpen:
    {
        const auto jailDoor = scene->GetActorManager()->GetActorOfType<DoorJailActor>();
        if (!jailDoor)
        {
            Logger::Warning("[PrisonOpeningMovie] DoorJailActor was not found while waiting to open.");
            FinishPrisonOpeningMovie();
            return;
        }

        if (jailDoor->IsOpenDoor())
            BeginPrisonOpeningPlayerStanding();
        break;
    }

    case PrisonOpeningState::PlayerStanding:
    {
        const auto player = scene->GetActorManager()->GetActorOfType<Player>();
        if (!player)
        {
            Logger::Warning("[PrisonOpeningMovie] Player was not found while waiting for Standing.");
            FinishPrisonOpeningMovie();
            return;
        }

        if (player->IsPrisonOpeningStandingFinished())
            BeginPrisonOpeningBlendToTPS();
        break;
    }

    case PrisonOpeningState::BlendingToTPS:
    {
        UpdatePrisonOpeningBlendToTPS(deltaTime);
        break;
    }

    default:
        break;
    }
}

void MovieCameraManagerActor::UpdatePrisonOpeningEyeOpening()
{
    const auto movieCamera = movieCameraWeakPtr.lock();
    const auto scene = GetOwnerScene();
    const auto player = scene ? scene->GetActorManager()->GetActorOfType<Player>() : nullptr;
    float prisonOpeningCurrentEyeWeight = 0.0f;

    if (!movieCamera || !player || !prisonOpeningEyeOverrideActive ||
        !player->IsPrisonOpeningEyeOverrideActive())
    {
        prisonOpeningEyeOverrideActive = false;
        prisonOpeningCurrentEyeWeight = 0.0f;
        return;
    }

    float prisonOpeningCurrentMovieTime = movieCamera->GetPlaybackElapsedTime();
    if (!prisonOpeningEyeOpeningEnabled)
    {
        player->SetPrisonOpeningEyeClosedWeight(0.0f);
        EndPrisonOpeningEyeOverride();
        return;
    }

    if (!prisonOpeningFaceCloseUpKeyTimeValid)
    {
        prisonOpeningCurrentEyeWeight = 1.0f;
        player->SetPrisonOpeningEyeClosedWeight(prisonOpeningCurrentEyeWeight);
        return;
    }

    const float evaluationTime = prisonOpeningEyeOpeningPreview ? prisonOpeningEyeOpeningPreviewTime : prisonOpeningCurrentMovieTime;
    const float duration = (std::max)(prisonOpeningEyeOpenDuration, FLT_EPSILON);
    const float normalizedTime = std::clamp((evaluationTime - (prisonOpeningFaceCloseUpKeyTime + prisonOpeningEyeOpenDelay)) / duration, 0.0f, 1.0f);
    const float smoothStep = normalizedTime * normalizedTime * (3.0f - 2.0f * normalizedTime);
    prisonOpeningCurrentEyeWeight = 1.0f - smoothStep;
    player->SetPrisonOpeningEyeClosedWeight(prisonOpeningCurrentEyeWeight);

    if (!prisonOpeningEyeOpeningPreview && normalizedTime >= 1.0f)
        EndPrisonOpeningEyeOverride();
}

void MovieCameraManagerActor::UpdatePrisonOpeningFaceCloseUpKeyTime()
{
    prisonOpeningFaceCloseUpKeyTime = 0.0f;
    prisonOpeningFaceCloseUpKeyTimeValid = false;
    const auto movieCamera = movieCameraWeakPtr.lock();
    if (!movieCamera)
        return;

    prisonOpeningFaceCloseUpKeyTimeValid =
        movieCamera->TryGetKeyframeStartTime(
            PrisonOpeningFaceCloseUpKeyName, prisonOpeningFaceCloseUpKeyTime);
    if (!prisonOpeningFaceCloseUpKeyTimeValid)
    {
        prisonOpeningFaceCloseUpKeyTimeValid =
            movieCamera->TryGetKeyframeStartTime(
                PrisonOpeningFaceCloseUpKeyIndex, prisonOpeningFaceCloseUpKeyTime);
    }
}

bool MovieCameraManagerActor::ReplayPrisonOpeningMovie()
{
    if (!CanReplayPrisonOpeningMovie())
        return false;

    const auto scene = GetOwnerScene();
    const auto movieCamera = movieCameraWeakPtr.lock();
    const auto jailDoor = scene->GetActorManager()->GetActorOfType<DoorJailActor>();
    if (!movieCamera || !jailDoor)
        return false;

    movieCamera->LoadFromJson(PrisonOpeningMoviePath);
    UpdatePrisonOpeningFaceCloseUpKeyTime();
    if (movieCamera->GetKeyframeCount() < 2)
        return false;

    EndPrisonOpeningEyeOverride();
    jailDoor->ResetToClosedForPrisonOpening();
    doorMovieState = DoorMovieState::None;
    prisonOpeningState = PrisonOpeningState::None;
    prisonOpeningJailDoorInteractionRequested = false;
    prisonOpeningMoviePrepared = false;
    return PlayPrisonOpeningMovie();
}

bool MovieCameraManagerActor::CanReplayPrisonOpeningMovie() const
{
    const auto scene = GetOwnerScene();
    const auto gameScene = dynamic_cast<GameScene*>(scene);
    const auto cameraManager = scene ? scene->GetCameraManager() : nullptr;
    if (!scene || !gameScene || !cameraManager || scene->IsPaused() ||
        prisonOpeningMoviePlaying || prisonOpeningMoviePrepared ||
        cameraManager->IsUseMovie() || cameraManager->IsUseCinematic())
    {
        return false;
    }

    const auto player = scene->GetActorManager()->GetActorOfType<Player>();
    const auto jailDoor = scene->GetActorManager()->GetActorOfType<DoorJailActor>();
    return player && jailDoor && !player->IsBossBattle() && player->GetHp() > 0;
}

void MovieCameraManagerActor::LoadPrisonOpeningEyeSettings()
{
    std::ifstream file(PrisonOpeningEyeSettingsPath);
    if (!file.is_open())
        return;

    try
    {
        nlohmann::json settings;
        file >> settings;
        prisonOpeningEyeOpeningEnabled = settings.value("eyeOpeningEnabled", prisonOpeningEyeOpeningEnabled);
        prisonOpeningEyeOpenDelay = (std::max)(0.0f,
            settings.value("eyeOpenDelay", prisonOpeningEyeOpenDelay));
        prisonOpeningEyeOpenDuration = (std::max)(0.01f,
            settings.value("eyeOpenDuration", prisonOpeningEyeOpenDuration));
        const int sittingMode = settings.value("sittingMotionMode",
            static_cast<int>(prisonOpeningSittingMotionMode));
        prisonOpeningSittingMotionMode = sittingMode == static_cast<int>(PrisonOpeningSittingMotionMode::OriginalSitting)
            ? PrisonOpeningSittingMotionMode::OriginalSitting : PrisonOpeningSittingMotionMode::StandingPingPong;
        prisonOpeningSittingPingPongStartTime = (std::max)(0.0f, settings.value("pingPongStartTime", prisonOpeningSittingPingPongStartTime));
        prisonOpeningSittingPingPongEndTime = (std::max)(prisonOpeningSittingPingPongStartTime + 0.001f, settings.value("pingPongEndTime", prisonOpeningSittingPingPongEndTime));
        prisonOpeningSittingPingPongSpeed = std::clamp(settings.value("pingPongSpeed", prisonOpeningSittingPingPongSpeed), 0.1f, 2.0f);
        prisonOpeningSittingToStandingBlendTime = std::clamp(settings.value("sittingToStandingBlendTime", prisonOpeningSittingToStandingBlendTime), 0.0f, 0.5f);
        prisonOpeningBlendDuration = std::clamp(settings.value("tpsBlendTime", prisonOpeningBlendDuration), 0.1f, 3.0f);
    }
    catch (const std::exception& exception)
    {
        Logger::Warning(std::string("[PrisonOpeningMovie] Failed to load eye settings: ") + exception.what());
    }
}

void MovieCameraManagerActor::SavePrisonOpeningEyeSettings() const
{
    std::ofstream file(PrisonOpeningEyeSettingsPath);
    if (!file.is_open())
    {
        Logger::Warning("[PrisonOpeningMovie] Failed to save eye settings.");
        return;
    }

    nlohmann::json settings;
    settings["eyeOpeningEnabled"] = prisonOpeningEyeOpeningEnabled;
    settings["sittingMotionMode"] = static_cast<int>(prisonOpeningSittingMotionMode);
    settings["pingPongStartTime"] = prisonOpeningSittingPingPongStartTime;
    settings["pingPongEndTime"] = prisonOpeningSittingPingPongEndTime;
    settings["pingPongSpeed"] = prisonOpeningSittingPingPongSpeed;
    settings["sittingToStandingBlendTime"] = prisonOpeningSittingToStandingBlendTime;
    settings["tpsBlendTime"] = prisonOpeningBlendDuration;
    settings["eyeOpenDelay"] = prisonOpeningEyeOpenDelay;
    settings["eyeOpenDuration"] = prisonOpeningEyeOpenDuration;
    file << settings.dump(4);
}

void MovieCameraManagerActor::EndPrisonOpeningEyeOverride()
{
    if (!prisonOpeningEyeOverrideActive)
        return;

    const auto scene = GetOwnerScene();
    if (const auto player = scene
        ? scene->GetActorManager()->GetActorOfType<Player>()
        : nullptr)
    {
        player->EndPrisonOpeningEyeOverride();
    }
    prisonOpeningEyeOverrideActive = false;
}

void MovieCameraManagerActor::BeginPrisonOpeningPlayerStanding()
{
    if (!prisonOpeningMoviePlaying ||
        prisonOpeningState != PrisonOpeningState::WaitingForJailDoorOpen)
    {
        return;
    }

    const auto scene = GetOwnerScene();
    const auto player = scene
        ? scene->GetActorManager()->GetActorOfType<Player>()
        : nullptr;
    if (!player || !player->BeginPrisonOpeningStanding())
    {
        Logger::Warning("[PrisonOpeningMovie] Failed to start Player Standing animation.");
        FinishPrisonOpeningMovie();
        return;
    }

    prisonOpeningState = PrisonOpeningState::PlayerStanding;
}

void MovieCameraManagerActor::BeginPrisonOpeningBlendToTPS()
{
    if (!prisonOpeningMoviePlaying ||
        prisonOpeningState != PrisonOpeningState::PlayerStanding)
    {
        return;
    }

    const auto movieCamera = movieCameraWeakPtr.lock();
    const auto scene = GetOwnerScene();
    const auto darkCamera = scene
        ? scene->GetActorManager()->GetActorOfType<DarkCameraActor>()
        : nullptr;
    if (!movieCamera || !movieCamera->GetOwner() || !darkCamera)
    {
        Logger::Warning("[PrisonOpeningMovie] Failed to begin TPS camera blend.");
        FinishPrisonOpeningMovie();
        return;
    }

    DarkCameraActor::PrisonOpeningTpsPose target{};
    if (!darkCamera->GetPrisonOpeningTpsPose(target))
    {
        Logger::Warning("[PrisonOpeningMovie] Failed to evaluate TPS camera blend target.");
        FinishPrisonOpeningMovie();
        return;
    }

    prisonOpeningBlendStartPosition = movieCamera->GetOwner()->GetPosition();
    prisonOpeningBlendStartRotation = movieCamera->GetOwner()->GetQuaternionRotation();
    prisonOpeningBlendStartFov = movieCamera->GetFov();
    prisonOpeningBlendElapsed = 0.0f;

    if (prisonOpeningBlendDuration <= 0.0f)
    {
        darkCamera->PreparePrisonOpeningTpsReturn(target);
        FinishPrisonOpeningMovie();
        return;
    }

    prisonOpeningState = PrisonOpeningState::BlendingToTPS;
}

void MovieCameraManagerActor::UpdatePrisonOpeningBlendToTPS(float deltaTime)
{
    const auto movieCamera = movieCameraWeakPtr.lock();
    const auto scene = GetOwnerScene();
    const auto darkCamera = scene
        ? scene->GetActorManager()->GetActorOfType<DarkCameraActor>()
        : nullptr;
    if (!movieCamera || !darkCamera)
    {
        FinishPrisonOpeningMovie();
        return;
    }

    DarkCameraActor::PrisonOpeningTpsPose target{};
    if (!darkCamera->GetPrisonOpeningTpsPose(target))
    {
        FinishPrisonOpeningMovie();
        return;
    }

    if (prisonOpeningBlendDuration <= 0.0f)
    {
        darkCamera->PreparePrisonOpeningTpsReturn(target);
        FinishPrisonOpeningMovie();
        return;
    }

    const float duration = prisonOpeningBlendDuration;
    prisonOpeningBlendElapsed = (std::min)(
        prisonOpeningBlendElapsed + (std::max)(deltaTime, 0.0f), duration);
    const float linearT = prisonOpeningBlendElapsed / duration;
    const float t = linearT * linearT * (3.0f - 2.0f * linearT);

    using namespace DirectX;
    const XMFLOAT4 targetRotation = CreateLookRotation(target.eye, target.target);
    const XMVECTOR rotation = XMQuaternionSlerp(
        XMLoadFloat4(&prisonOpeningBlendStartRotation),
        XMLoadFloat4(&targetRotation),
        t);
    XMFLOAT4 blendedRotation{};
    XMStoreFloat4(&blendedRotation, XMQuaternionNormalize(rotation));
    movieCamera->ApplyWorldPose(
        MathHelper::Lerp(prisonOpeningBlendStartPosition, target.eye, t),
        blendedRotation,
        std::lerp(prisonOpeningBlendStartFov, target.fov, t));

    if (linearT >= 1.0f)
    {
        darkCamera->PreparePrisonOpeningTpsReturn(target);
        FinishPrisonOpeningMovie();
    }
}
void MovieCameraManagerActor::FinishPrisonOpeningMovie()
{
    if (!prisonOpeningMoviePlaying)
        return;


    EndPrisonOpeningEyeOverride();
    auto scene = GetOwnerScene();
    auto cameraManager = scene ? scene->GetCameraManager() : nullptr;
    if (const auto gameScene = dynamic_cast<GameScene*>(scene))
        gameScene->EndPrisonOpeningMoviePlayerSitting();

    if (cameraManager)
    {
        if (cameraManager->IsUseMovie())
            cameraManager->ToggleMovieCamera(GetOwnerConstScene());
        if (cameraManager->IsUseCinematic())
            cameraManager->ToggleCinematicCamera(GetOwnerConstScene());
    }
    if (scene)
    {
        if (const auto darkCamera = scene->GetActorManager()->GetActorOfType<DarkCameraActor>())
            darkCamera->SetRequestMode(DarkCameraActor::CameraMode::TPS);
    }

    prisonOpeningMoviePlaying = false;
    prisonOpeningJailDoorInteractionRequested = false;
    prisonOpeningState = PrisonOpeningState::None;
    prisonOpeningBlendElapsed = 0.0f;
}

void MovieCameraManagerActor::FinishBossEntryToBattle()
{
    if (bossEntryBattleStarted)
        return;

    bossEntryBattleStarted = true;
    bossEntryExternalBlendActive = false;
    doorMovieState = DoorMovieState::Finished;

    const auto scene = GetOwnerScene();
    const auto gameScene = dynamic_cast<GameScene*>(scene);
    const auto player = scene ? scene->GetActorManager()->GetActorOfType<Player>() : nullptr;
    const auto gruxEnemy = scene ? scene->GetActorManager()->GetActorOfType<GruxEnemy>() : nullptr;
    if (player)
        player->EndEvent();
    if (gruxEnemy)
        gruxEnemy->EnterBattleIdlePresentation();
    if (gameScene)
        gameScene->StartBossBattle();
}

DirectX::XMFLOAT4  MovieCameraManagerActor::CreateLookRotation(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& target)
{
    using namespace DirectX;
    XMFLOAT3 direction = MathHelper::Subtract(target, eye);
    if (MathHelper::Length(direction) <= FLT_EPSILON)
        direction = { 0.0f, 0.0f, 1.0f };
    else
        direction = MathHelper::Normalize(direction);

    const float yaw = atan2f(direction.x, direction.z);
    const float pitch = -asinf(std::clamp(direction.y, -1.0f, 1.0f));
    const XMVECTOR qYaw = XMQuaternionRotationAxis(
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), yaw);
    const XMVECTOR right = XMVector3Rotate(
        XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), qYaw);
    const XMVECTOR qPitch = XMQuaternionRotationAxis(right, pitch);

    XMFLOAT4 result{};
    XMStoreFloat4(&result,
        XMQuaternionNormalize(XMQuaternionMultiply(qPitch, qYaw)));
    return result;
}
