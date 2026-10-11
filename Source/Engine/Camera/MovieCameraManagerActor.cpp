#include "pch.h"
#include "MovieCameraManagerActor.h"

#include "Engine/Scene/Scene.h"
#include "Engine/Camera/CameraManager.h"
#include "Game/Actors/Player/Player.h"
#include "Game/DarkGame/DarkActors/DarkStageCandelabraActor.h"
#include "Game/DarkGame/DarkActors/DarkStageChandelierActor.h"
#include "Game/DarkGame/DarkActors/DoorActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Scenes/GameScene.h"
#include <fstream>
#include <json.hpp>
#include "Physics/CollisionFunction.h"
#include "Components/CollisionShape/ShapeComponent.h"

namespace
{
    DirectX::XMFLOAT4 CreateDeathWideLookRotation(
        const DirectX::XMFLOAT3& eye,
        const DirectX::XMFLOAT3& target)
    {
        using namespace DirectX;
        XMFLOAT3 direction = MathHelper::Subtract(target, eye);
        if (MathHelper::Length(direction) <= FLT_EPSILON)
            direction = { 0.0f, 0.0f, 1.0f };
        else
            direction = MathHelper::Normalize(direction);

        const float yaw = atan2f(direction.x, direction.z);
        // XMVector3Rotate(+Z, qPitch * qYaw) uses the opposite sign for
        // camera elevation, so invert the geometric pitch here.
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

    const char* CollisionLayerName(uint32_t layer)
    {
        if (layer == CollisionHelper::ToBit(CollisionLayer::WorldStatic)) return "WorldStatic";
        if (layer == CollisionHelper::ToBit(CollisionLayer::Floor)) return "Floor";
        if (layer == CollisionHelper::ToBit(CollisionLayer::WorldProps)) return "WorldProps";
        if (layer == CollisionHelper::ToBit(CollisionLayer::WorldPropsNoRaycast)) return "WorldPropsNoRaycast";
        if (layer == CollisionHelper::ToBit(CollisionLayer::Convex)) return "Convex";
        if (layer == CollisionHelper::ToBit(CollisionLayer::Wall)) return "Wall";
        return "Unknown";
    }

    bool IsDeathWideAnchorUsable(
        const std::shared_ptr<SceneComponent>& anchor,
        const DirectX::XMFLOAT3& lookPoint,
        float minimumDistance,
        MovieCameraManagerActor::DeathWideAnchorValidationDebug* debug)
    {
        if (debug)
        {
            debug->targetPosition = lookPoint;
            debug->name = anchor ? anchor->GetName() : "None";
        }
        if (!anchor)
        {
            if (debug) debug->reason = "InvalidAnchor";
            return false;
        }

        const DirectX::XMFLOAT3 anchorPosition =
            anchor->GetComponentLocation();
        const float distance = MathHelper::Distance(anchorPosition, lookPoint);
        if (debug)
        {
            debug->anchorPosition = anchorPosition;
            debug->distance = distance;
        }
        if (distance < minimumDistance)
        {
            if (debug) debug->reason = "TooClose";
            return false;
        }

        const uint32_t mask =
            CollisionHelper::ToBit(CollisionLayer::WorldStatic) |
            CollisionHelper::ToBit(CollisionLayer::Floor) |
            CollisionHelper::ToBit(CollisionLayer::WorldProps);
        HitResultWithActor hit{};
        const bool targetPathHit = CollisionFunction::SphereRayCast(
            lookPoint, anchorPosition, hit, 0.3f, mask);
        if (debug)
        {
            debug->targetPathHit = targetPathHit;
            debug->targetPathInitialOverlap = hit.initialOverlap;
            debug->targetPathHitDistance = hit.distance;
            debug->targetPathHitPosition = hit.hitPoint;
            debug->targetPathHitNormal = hit.normal;
            if (hit.component)
                debug->targetPathLayer = CollisionLayerName(
                    hit.component->GetCollisionLayer());
        }
        if (targetPathHit)
        {
            if (debug) debug->reason = "TargetPathBlocked";
            return false;
        }

        // Reject an anchor whose own sphere starts inside world geometry.
        const DirectX::XMFLOAT3 probeDirection = MathHelper::Normalize(
            MathHelper::Subtract(anchorPosition, lookPoint));
        const DirectX::XMFLOAT3 probeEnd = MathHelper::Add(
            anchorPosition, MathHelper::Multiply(probeDirection, 0.02f));
        if (debug)
        {
            debug->probeStart = anchorPosition;
            debug->probeEnd = probeEnd;
        }
        HitResultWithActor probeHit{};
        const bool probeCastHit = CollisionFunction::SphereRayCast(
            anchorPosition, probeEnd, probeHit, 0.3f, mask);
        const bool probeHitResult = probeCastHit && probeHit.initialOverlap;
        if (debug)
        {
            debug->probeHit = probeCastHit;
            debug->probeInitialOverlap = probeHit.initialOverlap;
            if (probeHit.component)
                debug->probeLayer = CollisionLayerName(
                    probeHit.component->GetCollisionLayer());
        }
        if (probeHitResult)
        {
            if (debug) debug->reason = "AnchorInitialOverlap";
            return false;
        }

        // There is no overlap-sphere query in the current Physics facade.
        // Approximate the camera safety volume with short sweeps in all axes
        // around the candidate anchor.  This does not move or resolve the
        // camera; it only rejects an unsafe candidate.
        const DirectX::XMFLOAT3 spaceProbeDirections[] =
        {
            { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }
        };
        constexpr float spaceProbeLength = 0.05f;
        for (const auto& direction : spaceProbeDirections)
        {
            const DirectX::XMFLOAT3 end = MathHelper::Add(
                anchorPosition,
                MathHelper::Multiply(direction, spaceProbeLength));
            HitResultWithActor spaceHit{};
            if (CollisionFunction::SphereRayCast(
                anchorPosition, end, spaceHit, 0.3f, mask))
            {
                if (debug)
                {
                    debug->reason = "AnchorSpaceBlocked";
                    if (spaceHit.component)
                        debug->spaceProbeLayer = CollisionLayerName(
                            spaceHit.component->GetCollisionLayer());
                }
                return false;
            }
        }
        if (debug)
        {
            debug->valid = true;
            debug->reason = "Valid";
        }
        return true;
    }
}

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

    UpdateDeathWideAnchorPreview();

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
    if (!prisonOpeningMoviePlaying && prisonOpeningTransitionDebugActive &&
        prisonOpeningTransitionDebugPostFinishFramesRemaining > 0)
    {
        LogPrisonOpeningTransitionDebug("TPSPostFinish");
        ++prisonOpeningTransitionDebugFrame;
        --prisonOpeningTransitionDebugPostFinishFramesRemaining;
        if (prisonOpeningTransitionDebugPostFinishFramesRemaining == 0)
            prisonOpeningTransitionDebugActive = false;
    }
    if (prisonOpeningMoviePlaying)
    {
        const auto cameraManager = scene->GetCameraManager();
        if (!movieCamera || !gameScene || !cameraManager)
        {
            FinishPrisonOpeningMovie();
        }
        else
        {
            // CameraManager renders Cinematic before Movie, so preserve the movie
            // presentation while the jail door opens and Aurora stands.
            if (cameraManager->IsUseCinematic())
                cameraManager->ToggleCinematicCamera(GetOwnerConstScene());

            if (!cameraManager->IsUseMovie())
            {
                // Stop/toggle-off is an interruption, not a path completion.
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

    if (movieCamera && player && gruxEnemy)
    {
        const auto playerPosition = player->GetPosition();
        const DirectX::XMFLOAT3 forward =
        {
            gruxEnemy->GetPosition().x - playerPosition.x,
            0.0f,
            gruxEnemy->GetPosition().z - playerPosition.z
        };
        const auto basis = MovieCameraComponent::CreateActorRelativeBasis(
            playerPosition, forward);
        movieCamera->SetActorRelativeBasis(basis);
    }

    switch (doorMovieState)
    {
    case DoorMovieState::None:
        break;
    case DoorMovieState::DoorPreMovie:
        if (movieCamera->IsMovieFinish())
        {// 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ隰費ｽｪ繝ｻ・ｽI繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽplayer繝ｻ・ｽ繝ｻ・ｽA繝ｻ・ｽb繝ｻ・ｽv繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ鬮費ｽｮ繝ｻ・ｽ繝ｻ・ｽ・・腸・ｿ・ｽ
            PlayMovie(playerMovieFileName);
            doorMovieState = DoorMovieState::UpPlayerMovie;
            if (player)
            {
                // 繝ｻ・ｽC繝ｻ・ｽx繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽg繝ｻ・ｽV繝ｻ・ｽ[繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽJ繝ｻ・ｽn繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・ｑ・ｿ・ｽ・顔衍繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
                player->StartEvent();
                player->PlayBodyAnimation("Recall_0", false);
            }
            // 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・後す繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽf繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽA繝ｻ・ｽ・御ｼ夲ｽｿ・ｽ繝ｻ・ｽ・碁ｯ会ｽｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
            for (auto chandelier : chandelierActors)
            {
                chandelier->SetFireScaleToZero();
            }
            // 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・瑚搴繝ｻ・ｽC繝ｻ・ｽX繝ｻ・ｽ^繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽh繝ｻ・ｽ・御ｼ夲ｽｿ・ｽ繝ｻ・ｽ・碁ｯ会ｽｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
            for (auto candleStand : candleStandActors)
            {
                candleStand->SetFireLightScale({ 0.0f,0.0f,0.0f });
            }
        }
        break;
    case DoorMovieState::UpPlayerMovie:
    {
        if (movieCamera->IsMovieFinish())
        {// 繝ｻ・ｽv繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽC繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽ繝ｻ・ｽA繝ｻ・ｽb繝ｻ・ｽv繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ鬮費ｽｮ繝ｻ・ｽ隰費ｽｪ繝ｻ・ｽI繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽA
            if (player)
            {// 繝ｻ・ｽv繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽC繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽ・後い繝ｻ・ｽj繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽV繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・呈ｩ溘・・ｽ・牙､画峩
                player->PlayBodyAnimation("Idle", true);
            }
            if (gameScene)
            {
                // 繝ｻ・ｽ{繝ｻ・ｽX繝ｻ・ｽ・悟桁・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・ｑ・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
                gameScene->SetBossRoomLerpFactor(0.0f);
                // 繝ｻ・ｽ・壹ｑ・ｿ・ｽBloom繝ｻ・ｽ・後∩繧托ｽｿ・ｽI繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・峨ｑ・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ
            }
            // 繝ｻ・ｽh繝ｻ・ｽA繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽN
            PlayMovie(doorOpenMovieFileName);
            // 繝ｻ・ｽh繝ｻ・ｽA繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽA繝ｻ・ｽj繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽV繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
            if (doorActor)
            {
                doorActor->Open();
            }
            // 繝ｻ・ｽG繝ｻ・ｽ・後い繝ｻ・ｽj繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽV繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ~繝ｻ・ｽ・溘ｑ・ｿ・ｽ
            if (gruxEnemy)
            {
                gruxEnemy->PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.3f, false,
                    "MovieCameraManager::UpPlayerMovie");
                gruxEnemy->SetBodyAnimationRate(0.0f);
            }
            doorMovieState = DoorMovieState::DoorOpening;
        }
    }
    break;
    case DoorMovieState::DoorOpening:
        if (doorActor->IsOpenDoor())
        {// 繝ｻ・ｽh繝ｻ・ｽA繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽA
            CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_groan.wav", 0.2f);
            // 繝ｻ・ｽG繝ｻ・ｽ・檎岼邇峨ｑ・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
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
        // 繝ｻ・ｽ{繝ｻ・ｽX繝ｻ・ｽ・檎岼邇峨ｑ・ｿ・ｽ・医ｑ・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ
        // 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽX繝ｻ・ｽ・牙私・ｿ・ｽ繝ｻ・ｽ鬩搾ｽｭ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
        if (gameScene)
        {
            gameScene->StartBossRoomLerp(0.0f, 1.0f, duration, [&,gruxEnemy]()
                {
                    if (auto camera = movieCameraWeakPtr.lock())
                    {
                        camera->GetOwner()->SetPosition(bossRoomZoomTargetPosition);
                        camera->GetOwner()->SetQuaternionRotation(bossRoomZoomTargetRotation);
                        camera->SetFov(DirectX::XMConvertToRadians(bossRoomZoomTargetFovDegree));
                    }
                    gruxEnemy->GetBodyAnimationController()->ResetAnimationRate();
                    doorMovieState = DoorMovieState::UpPlayerCombat;
                });
        }
        // 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・後す繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽf繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽA繝ｻ・ｽ・御ｼ夲ｽｿ・ｽ繝ｻ・ｽ・碁ｯ会ｽｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽX繝ｻ・ｽ・画綾繧托ｽｿ・ｽ
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
                // 繝ｻ・ｽh繝ｻ・ｽA繝ｻ・ｽ繝ｻ・ｽ・ゅａ繧托ｽｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ・斐↓繧托ｽｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ
                if (doorActor)
                {
                    doorActor->Closed();
                }
                // 繝ｻ・ｽv繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽC繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽ・御ｽ咲ｽｮ繝ｻ・ｽ繝ｻ・ｽh繝ｻ・ｽA繝ｻ・ｽO繝ｻ・ｽ・峨ｑ・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ
                if (player)
                {
                    player->SetPosition({ -4.827f,-0.098f,11.724f });
                    // 繝ｻ・ｽv繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽC繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽ・後い繝ｻ・ｽj繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽV繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・腸・ｿ・ｽ
                    player->PlayBodyAnimation("Level_Start_Cut", false);
                }
                // 繝ｻ・ｽG繝ｻ・ｽ・御ｽ咲ｽｮ繝ｻ・ｽ﨟皮ｧ､繝ｻ・ｽ繝ｻ・ｽ・御ｼ夲ｽｿ・ｽ繝ｻ・ｽ・峨ｑ・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ
                if (gruxEnemy)
                {
                    gruxEnemy->SetEulerRotation({ 0.0f,-90.0f,0.0f });
                }
                doorMovieState = DoorMovieState::UpPlayerCombatMovie;
            });

        // 繝ｻ・ｽv繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽC繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽA繝ｻ・ｽb繝ｻ・ｽv繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽN
        PlayMovie(playerCombatMovieFileName);
        // 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・瑚搴繝ｻ・ｽC繝ｻ・ｽX繝ｻ・ｽ^繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽh繝ｻ・ｽ・御ｼ夲ｽｿ・ｽ繝ｻ・ｽ・碁ｯ会ｽｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ・溘ｑ・ｿ・ｽ
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
        {// 繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽN繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽI繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽA
            doorMovieState = DoorMovieState::EnemyMovie;
        }
        break;
    case DoorMovieState::EnemyMovie:
        // 繝ｻ・ｽ{繝ｻ・ｽX繝ｻ・ｽA繝ｻ・ｽb繝ｻ・ｽv繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽN
        PlayMovie(bossRoarMovieFileName);
        // 繝ｻ・ｽG繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽi繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
        if (gruxEnemy)
        {
            gruxEnemy->PlayBodyAnimation("Ultimate_Roar_0", false);
        }
        doorMovieState = DoorMovieState::EnemyName;
        break;
    case DoorMovieState::EnemyName:
        if (movieCamera->IsMovieFinish())
        {
            // BGM繝ｻ・ｽ繝ｻ・ｽ・・腸・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
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
                gruxEnemy->PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.3f, false,
                    "MovieCameraManager::EnemyName");
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
                // 繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ{繝ｻ・ｽX繝ｻ・ｽ蟇ｬ讖ｸ・ｿ・ｽ・梧得・ｿ・ｽ・斐↓螟画峩繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
                player->SetIsBossBattle(true);
            }
            //繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽO繝ｻ・ｽl繝ｻ・ｽ・後↓謌ｻ繧托ｽｿ・ｽ
            if (scene->GetCameraManager()->IsUseMovie())
            {// 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽr繝ｻ・ｽ[繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽg繝ｻ・ｽp繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・悟ｴ蜷医・・ｽ・後∩蛻・ｑ・ｿ・ｽ・悶ｑ・ｿ・ｽ
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
                    darkCameraActor->StartExternalBlend(start, target, 2.0f, [player,gruxEnemy,gameScene]()
                        {
                            if (player)
                            {
                                // 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽo繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽI繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・ｑ・ｿ・ｽ・顔衍繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
                                player->EndEvent();
                            }
                            if (gruxEnemy)
                            {// 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・・繝ｻ・ｽX繝ｻ・ｽ・悟私・ｿ・ｽ繝ｻ・ｽO繝ｻ・ｽ繝ｻ・ｽUI繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
                                gruxEnemy->GetStateMachine()->ChangeState("EnemyIdleState");
                            }
                            if (gameScene)
                            {
                                gameScene->StartBossBattle();
                            }

                        }
                    );
                    scene->GetCameraManager()->ToggleMovieCamera(GetOwnerConstScene());
                }
#endif // 0

            }
            if (gruxEnemy)
            {// 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・・繝ｻ・ｽX繝ｻ・ｽ・悟私・ｿ・ｽ繝ｻ・ｽO繝ｻ・ｽ繝ｻ・ｽUI繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
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
    ImGui::DragFloat("TPS Blend Duration", &prisonOpeningBlendDuration,
        0.01f, 0.0f, 2.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("Eye Opening Enabled", &prisonOpeningEyeOpeningEnabled);
    ImGui::Text("Face CloseUp Key Time: %s%.3f s",
        prisonOpeningFaceCloseUpKeyTimeValid ? "" : "Unavailable ",
        prisonOpeningFaceCloseUpKeyTime);
    ImGui::DragFloat("Eye Open Delay", &prisonOpeningEyeOpenDelay,
        0.01f, 0.0f, 30.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Eye Open Duration", &prisonOpeningEyeOpenDuration,
        0.01f, 0.01f, 10.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    const float calculatedEyeOpenStart = prisonOpeningFaceCloseUpKeyTime + prisonOpeningEyeOpenDelay;
    ImGui::Text("Calculated Eye Open Start: %.3f s", calculatedEyeOpenStart);
    ImGui::Text("Calculated Eye Open End: %.3f s",
        calculatedEyeOpenStart + prisonOpeningEyeOpenDuration);
    ImGui::Text("Current Movie Time: %.3f s", prisonOpeningCurrentMovieTime);
    ImGui::Text("Current Eye Weight: %.3f", prisonOpeningCurrentEyeWeight);
    ImGui::Checkbox("Eye Opening Preview", &prisonOpeningEyeOpeningPreview);
    if (prisonOpeningEyeOpeningPreview)
        ImGui::DragFloat("Eye Opening Preview Time", &prisonOpeningEyeOpeningPreviewTime,
            0.01f, 0.0f, 30.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::Button("Save Prison Eye Settings"))
        SavePrisonOpeningEyeSettings();
    ImGui::SameLine();
    if (ImGui::Button("Load Prison Eye Settings"))
        LoadPrisonOpeningEyeSettings();

    ImGui::Checkbox("DeathWide Anchor Preview",
        &deathWideAnchorPreviewEnabled);
    ImGui::Text("Selected Anchor: %s",
        deathWidePreviewAnchorName.c_str());

    const auto previewAnchor = deathWidePreviewAnchor.lock();
    const auto previewTarget = deathWidePreviewTarget.lock();
    const auto movieCamera = movieCameraWeakPtr.lock();
    const bool movieActive = GetOwnerScene() &&
        GetOwnerScene()->GetCameraManager() &&
        GetOwnerScene()->GetCameraManager()->IsUseMovie();
    ImGui::Text("Preview Enabled: %s",
        deathWideAnchorPreviewEnabled ? "true" : "false");
    ImGui::Text("MovieCamera Active: %s",
        movieActive ? "true" : "false");
    ImGui::Text("Preview Anchor Valid: %s",
        previewAnchor ? "true" : "false");
    ImGui::Text("Preview Target Valid: %s",
        previewTarget ? "true" : "false");
    ImGui::Text("DeathWide Collision Mask: WorldStatic | Floor | WorldProps");

    if (!deathWideValidationDebug.empty())
    {
        ImGui::Separator();
        ImGui::Text("DeathWide Anchor Validation");
        for (const auto& validation : deathWideValidationDebug)
        {
            ImGui::Text("Anchor: %s  Final Result: %s  Invalid Reason: %s",
                validation.name.c_str(),
                validation.valid ? "Valid" : "Invalid",
                validation.reason.c_str());
            ImGui::Text("  Anchor World Position: %.3f, %.3f, %.3f",
                validation.anchorPosition.x, validation.anchorPosition.y,
                validation.anchorPosition.z);
            ImGui::Text("  Target World Position: %.3f, %.3f, %.3f",
                validation.targetPosition.x, validation.targetPosition.y,
                validation.targetPosition.z);
            ImGui::Text("  Distance: %.3f", validation.distance);
            ImGui::Text("  Target -> Anchor SphereCast: Hit=%s InitialOverlap=%s Distance=%.3f",
                validation.targetPathHit ? "true" : "false",
                validation.targetPathInitialOverlap ? "true" : "false",
                validation.targetPathHitDistance);
            ImGui::Text("    Hit Position: %.3f, %.3f, %.3f",
                validation.targetPathHitPosition.x,
                validation.targetPathHitPosition.y,
                validation.targetPathHitPosition.z);
            ImGui::Text("    Hit Normal: %.3f, %.3f, %.3f Layer=%s",
                validation.targetPathHitNormal.x,
                validation.targetPathHitNormal.y,
                validation.targetPathHitNormal.z,
                validation.targetPathLayer.c_str());
            ImGui::Text("  Anchor Probe: Hit=%s InitialOverlap=%s",
                validation.probeHit ? "true" : "false",
                validation.probeInitialOverlap ? "true" : "false");
            ImGui::Text("    Probe Start: %.3f, %.3f, %.3f",
                validation.probeStart.x, validation.probeStart.y,
                validation.probeStart.z);
            ImGui::Text("    Probe End: %.3f, %.3f, %.3f Layer=%s",
                validation.probeEnd.x, validation.probeEnd.y,
                validation.probeEnd.z, validation.probeLayer.c_str());
            ImGui::Text("    Anchor Space Probe Layer: %s",
                validation.spaceProbeLayer.c_str());
        }
    }

    if (previewAnchor && previewTarget && movieCamera && movieCamera->GetOwner())
    {
        const DirectX::XMFLOAT3 anchorPosition =
            previewAnchor->GetComponentLocation();
        const DirectX::XMFLOAT3 targetPosition =
            previewTarget->GetComponentLocation();
        const DirectX::XMFLOAT3 eyeToTargetVector =
            MathHelper::Subtract(targetPosition, anchorPosition);
        const float eyeToTargetLength = MathHelper::Length(eyeToTargetVector);
        const DirectX::XMFLOAT3 eyeToTargetDirection =
            eyeToTargetLength > FLT_EPSILON
            ? MathHelper::Normalize(eyeToTargetVector)
            : DirectX::XMFLOAT3{};
        const DirectX::XMFLOAT3 moviePosition =
            movieCamera->GetOwner()->GetPosition();
        const DirectX::XMFLOAT3 movieForward = movieCamera->GetForward();
        const float forwardDot = eyeToTargetLength > FLT_EPSILON
            ? movieForward.x * eyeToTargetDirection.x +
              movieForward.y * eyeToTargetDirection.y +
              movieForward.z * eyeToTargetDirection.z
            : 0.0f;

        const DirectX::XMFLOAT3 targetRelativePosition =
            previewTarget->GetRelativeLocation();
        ImGui::Text("Anchor World Position: %.3f, %.3f, %.3f",
            anchorPosition.x, anchorPosition.y, anchorPosition.z);
        ImGui::Text("Target Relative Position: %.3f, %.3f, %.3f",
            targetRelativePosition.x, targetRelativePosition.y,
            targetRelativePosition.z);
        ImGui::Text("Target World Position: %.3f, %.3f, %.3f",
            targetPosition.x, targetPosition.y, targetPosition.z);
        ImGui::Text("MovieCamera Position: %.3f, %.3f, %.3f",
            moviePosition.x, moviePosition.y, moviePosition.z);
        if (deathWideDebugRotationValid)
        {
            ImGui::Text("Generated Rotation: %.5f, %.5f, %.5f, %.5f",
                deathWideDebugGeneratedRotation.x,
                deathWideDebugGeneratedRotation.y,
                deathWideDebugGeneratedRotation.z,
                deathWideDebugGeneratedRotation.w);
            ImGui::Text("MovieCamera Rotation BEFORE ApplyWorldPose: %.5f, %.5f, %.5f, %.5f",
                deathWideDebugRotationBefore.x,
                deathWideDebugRotationBefore.y,
                deathWideDebugRotationBefore.z,
                deathWideDebugRotationBefore.w);
            ImGui::Text("MovieCamera Rotation AFTER ApplyWorldPose: %.5f, %.5f, %.5f, %.5f",
                deathWideDebugRotationAfter.x,
                deathWideDebugRotationAfter.y,
                deathWideDebugRotationAfter.z,
                deathWideDebugRotationAfter.w);
            ImGui::Text("CameraComponent Rotation AFTER ApplyWorldPose: %.5f, %.5f, %.5f, %.5f",
                deathWideDebugComponentRotationAfter.x,
                deathWideDebugComponentRotationAfter.y,
                deathWideDebugComponentRotationAfter.z,
                deathWideDebugComponentRotationAfter.w);
            ImGui::Text("Component Rotation Forward: %.3f, %.3f, %.3f",
                deathWideDebugDirectForward.x,
                deathWideDebugDirectForward.y,
                deathWideDebugDirectForward.z);
            ImGui::Text("Component Forward Dot TargetDirection: %.4f",
                deathWideDebugDirectForwardDot);
        }
        ImGui::Text("MovieCamera Forward: %.3f, %.3f, %.3f",
            movieForward.x, movieForward.y, movieForward.z);
        if (deathWideDebugRotationValid)
        {
            ImGui::Text("MovieCamera Forward AFTER ApplyWorldPose: %.3f, %.3f, %.3f",
                deathWideDebugMovieForward.x,
                deathWideDebugMovieForward.y,
                deathWideDebugMovieForward.z);
            ImGui::Text("Forward Dot TargetDirection AFTER ApplyWorldPose: %.4f",
                deathWideDebugForwardDot);
        }
        ImGui::Text("Eye -> Target Direction: %.3f, %.3f, %.3f",
            eyeToTargetDirection.x, eyeToTargetDirection.y,
            eyeToTargetDirection.z);
        ImGui::Text("Forward Dot TargetDirection: %.4f", forwardDot);
    }

    ImGui::DragFloat("Boss Room Zoom Duration", &bossRoomZoomDuration,
        0.05f, 0.01f, 10.0f, "%.2f sec");
    ImGui::DragFloat("Boss Room Zoom Target FOV", &bossRoomZoomTargetFovDegree,
        0.1f, 10.0f, 60.0f, "%.1f deg");
    ImGui::DragFloat("Boss Room Camera Move Distance", &bossRoomCameraMoveDistance,
        0.05f, 0.0f, 3.0f, "%.2f m");
#endif
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

    // Toggle copies the previous render-camera pose into MovieCamera. Reapply
    // the authored first key so the Opening fade reveals the movie frame.
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

    // CameraManager renders Cinematic before Movie.  A left-over cinematic mode
    // would otherwise hide this runtime movie and later continue to override TPS.
    if (cameraManager->IsUseCinematic())
        cameraManager->ToggleCinematicCamera(GetOwnerConstScene());

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
    prisonOpeningCurrentMovieTime = 0.0f;
    prisonOpeningCurrentEyeWeight = prisonOpeningEyeOverrideActive ? 1.0f : 0.0f;
    if (!prisonOpeningEyeOverrideActive)
        Logger::Warning("[PrisonOpeningMovie] Failed to begin Player eye-close override.");

    prisonOpeningMoviePrepared = false;
    prisonOpeningMoviePlaying = true;
    prisonOpeningJailDoorInteractionRequested = false;
    prisonOpeningState = PrisonOpeningState::CameraPathPlaying;
    return true;
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
    prisonOpeningTransitionDebugActive = true;
    prisonOpeningTransitionDebugFrame = 0;
    prisonOpeningTransitionDebugPostFinishFramesRemaining = 0;
    LogPrisonOpeningTransitionDebug("StandingFinished_BlendBegin");

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
    // SmoothStep: t * t * (3 - 2t).
    const float t = linearT * linearT * (3.0f - 2.0f * linearT);

    using namespace DirectX;
    const XMFLOAT4 targetRotation = CreateDeathWideLookRotation(target.eye, target.target);
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
    LogPrisonOpeningTransitionDebug("BlendingToTPS");
    ++prisonOpeningTransitionDebugFrame;

    if (linearT >= 1.0f)
    {
        // Use the same pose applied to MovieCamera on this frame so the
        // CameraManager hand-off cannot introduce a final-frame jump.
        darkCamera->PreparePrisonOpeningTpsReturn(target);
        FinishPrisonOpeningMovie();
    }
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
    prisonOpeningTransitionDebugActive = false;
    prisonOpeningTransitionDebugPostFinishFramesRemaining = 0;
    return PlayPrisonOpeningMovie();
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
    settings["eyeOpenDelay"] = prisonOpeningEyeOpenDelay;
    settings["eyeOpenDuration"] = prisonOpeningEyeOpenDuration;
    file << settings.dump(4);
}

void MovieCameraManagerActor::UpdatePrisonOpeningEyeOpening()
{
    const auto movieCamera = movieCameraWeakPtr.lock();
    const auto scene = GetOwnerScene();
    const auto player = scene
        ? scene->GetActorManager()->GetActorOfType<Player>()
        : nullptr;
    if (!movieCamera || !player || !prisonOpeningEyeOverrideActive ||
        !player->IsPrisonOpeningEyeOverrideActive())
    {
        prisonOpeningEyeOverrideActive = false;
        prisonOpeningCurrentEyeWeight = 0.0f;
        return;
    }

    prisonOpeningCurrentMovieTime = movieCamera->GetPlaybackElapsedTime();
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

    const float evaluationTime = prisonOpeningEyeOpeningPreview
        ? prisonOpeningEyeOpeningPreviewTime
        : prisonOpeningCurrentMovieTime;
    const float duration = (std::max)(prisonOpeningEyeOpenDuration, FLT_EPSILON);
    const float normalizedTime = std::clamp(
        (evaluationTime - (prisonOpeningFaceCloseUpKeyTime +
            prisonOpeningEyeOpenDelay)) / duration,
        0.0f, 1.0f);
    const float smoothStep = normalizedTime * normalizedTime *
        (3.0f - 2.0f * normalizedTime);
    prisonOpeningCurrentEyeWeight = 1.0f - smoothStep;
    player->SetPrisonOpeningEyeClosedWeight(prisonOpeningCurrentEyeWeight);

    // Preview is confined to an active prison movie and must not consume the
    // override.  Normal playback clears only after it has reached the base pose.
    if (!prisonOpeningEyeOpeningPreview && normalizedTime >= 1.0f)
        EndPrisonOpeningEyeOverride();
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
    prisonOpeningCurrentEyeWeight = 0.0f;
}

void MovieCameraManagerActor::FinishPrisonOpeningMovie()
{
    if (!prisonOpeningMoviePlaying)
        return;

    if (prisonOpeningTransitionDebugActive)
        LogPrisonOpeningTransitionDebug("Finish_Pre");

    EndPrisonOpeningEyeOverride();
    auto scene = GetOwnerScene();
    auto cameraManager = scene ? scene->GetCameraManager() : nullptr;
    if (const auto gameScene = dynamic_cast<GameScene*>(scene))
        gameScene->EndPrisonOpeningMoviePlayerSitting();

    if (cameraManager)
    {
        // Remove all non-gameplay render-camera overrides.  GetRenderCamera()
        // then falls back to GameScene's DarkCameraActor (TPS).
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
    if (prisonOpeningTransitionDebugActive)
    {
        LogPrisonOpeningTransitionDebug("Finish_Post");
        prisonOpeningTransitionDebugPostFinishFramesRemaining = 5;
    }
}

void MovieCameraManagerActor::LogPrisonOpeningTransitionDebug(const char* phase)
{
    if (!prisonOpeningTransitionDebugActive)
        return;

    const auto scene = GetOwnerScene();
    const auto cameraManager = scene ? scene->GetCameraManager() : nullptr;
    const auto movieCamera = movieCameraWeakPtr.lock();
    const auto darkCamera = scene
        ? scene->GetActorManager()->GetActorOfType<DarkCameraActor>()
        : nullptr;
    const auto player = scene
        ? scene->GetActorManager()->GetActorOfType<Player>()
        : nullptr;

    DarkCameraActor::PrisonOpeningTpsPose target{};
    const bool hasTarget = darkCamera && darkCamera->GetPrisonOpeningTpsPose(target);
    const auto movieOwner = movieCamera ? movieCamera->GetOwner() : nullptr;
    const DirectX::XMFLOAT3 moviePosition = movieOwner
        ? movieOwner->GetPosition() : DirectX::XMFLOAT3{};
    const DirectX::XMFLOAT4 movieRotation = movieOwner
        ? movieOwner->GetQuaternionRotation() : DirectX::XMFLOAT4{};
    const float movieFov = movieCamera ? movieCamera->GetFov() : 0.0f;
    const float duration = (std::max)(prisonOpeningBlendDuration, 0.0f);
    const float linearT = duration > FLT_EPSILON
        ? std::clamp(prisonOpeningBlendElapsed / duration, 0.0f, 1.0f) : 1.0f;
    const float smoothT = linearT * linearT * (3.0f - 2.0f * linearT);
    const auto renderCamera = cameraManager ? cameraManager->GetRenderCamera(scene) : nullptr;

    Logger::Log(Logger::LogCategory::Gameplay, std::format(
        "[PrisonOpeningTrace][MovieManager] phase={} frame={} render={} movieEnabled={} "
        "moviePos=({:.3f},{:.3f},{:.3f}) movieRot=({:.4f},{:.4f},{:.4f},{:.4f}) movieFov={:.4f} "
        "targetValid={} t={:.3f} smooth={:.3f} elapsed={:.3f}/{:.3f} "
        "tpsEye=({:.3f},{:.3f},{:.3f}) tpsTarget=({:.3f},{:.3f},{:.3f}) tpsFov={:.4f} tpsYaw={:.4f} tpsPitch={:.4f} tpsDesiredDistance={:.3f} tpsActualDistance={:.3f} tpsCollisionRatio={:.3f}",
        phase, prisonOpeningTransitionDebugFrame,
        renderCamera ? renderCamera->GetName() : "None",
        cameraManager && cameraManager->IsUseMovie(),
        moviePosition.x, moviePosition.y, moviePosition.z,
        movieRotation.x, movieRotation.y, movieRotation.z, movieRotation.w, movieFov,
        hasTarget, linearT, smoothT, prisonOpeningBlendElapsed, duration,
        target.eye.x, target.eye.y, target.eye.z,
        target.target.x, target.target.y, target.target.z, target.fov,
        target.yaw, target.pitch, target.desiredDistance, target.actualDistance,
        target.collisionRatio));

    if (player)
        player->LogPrisonOpeningTransitionDebug(phase, prisonOpeningTransitionDebugFrame);
    if (darkCamera)
        darkCamera->RequestPrisonOpeningTransitionDebug(phase, prisonOpeningTransitionDebugFrame);
}
void MovieCameraManagerActor::PlayMovie(const std::string& file)
{
    if (auto cameraManager = GetOwnerScene()->GetCameraManager())
    {
        if (!cameraManager->IsUseMovie())
        {// 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・↓繝ｻ・ｿ・ｽ繝ｻ・ｽ[繝ｻ・ｽr繝ｻ・ｽ[繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽg繝ｻ・ｽp繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・↑繧托ｽｿ・ｽ繝ｻ・ｽ鮠ｾ繝ｻ・ｿ・ｽ・後∩蛻・ｑ・ｿ・ｽ・悶ｑ・ｿ・ｽ
            cameraManager->ToggleMovieCamera(GetOwnerConstScene());
        }
    }
    auto movieCamera = this->movieCameraWeakPtr.lock();
    movieCamera->LoadFromJson("./Data/Saves/MovieCameras/" + file);
    movieCamera->Start();
}

void MovieCameraManagerActor::PlayDeathWideRelative(
    const DirectX::XMFLOAT3& playerPosition,
    const DirectX::XMFLOAT3& bossPosition)
{
    auto scene = GetOwnerScene();
    auto cameraManager = scene ? scene->GetCameraManager() : nullptr;
    auto movieCamera = movieCameraWeakPtr.lock();
    if (!scene || !cameraManager || !movieCamera)
        return;

    DirectX::XMFLOAT3 forward =
    {
        bossPosition.x - playerPosition.x,
        0.0f,
        bossPosition.z - playerPosition.z
    };
    if (MathHelper::Length(forward) <= FLT_EPSILON)
        forward = { 0.0f, 0.0f, 1.0f };

    movieCamera->LoadFromJsonRelative(
        "./Data/Saves/MovieCameras/DeathWide.json",
        playerPosition,
        forward);
    if (!movieCamera->ApplyFirstFrameToOwner())
        return;

    // Toggle is an intentional CUT. Reapply the first key after the toggle,
    // because ToggleMovieCamera copies the previous render camera pose.
    if (!cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(scene);
    movieCamera->ApplyFirstFrameToOwner();
    movieCamera->Start();
}

bool MovieCameraManagerActor::PlayDeathWideFromAnchors(
    const std::vector<std::shared_ptr<SceneComponent>>& anchors,
    const std::shared_ptr<SceneComponent>& target,
    float minimumDistance)
{
    auto scene = GetOwnerScene();
    auto cameraManager = scene ? scene->GetCameraManager() : nullptr;
    auto movieCamera = movieCameraWeakPtr.lock();
    if (!scene || !cameraManager || !movieCamera)
        return false;
    if (!target)
        return false;

    const DirectX::XMFLOAT3 targetPosition = target->GetComponentLocation();

    std::shared_ptr<SceneComponent> selectedAnchor;
    deathWideValidationDebug.clear();
    for (const auto& anchor : anchors)
    {
        deathWideValidationDebug.emplace_back();
        auto& validation = deathWideValidationDebug.back();
        const bool usable = IsDeathWideAnchorUsable(
            anchor, targetPosition, minimumDistance, &validation);
        if (usable && !selectedAnchor)
        {
            selectedAnchor = anchor;
        }
    }
    if (!selectedAnchor)
    {
        deathWidePreviewAnchor.reset();
        deathWidePreviewTarget.reset();
        deathWidePreviewAnchorName = "None";
        return false;
    }

    // Keep the authored DeathWide FOV, but replace its world pose with the anchor.
    movieCamera->LoadFromJson(
        "./Data/Saves/MovieCameras/DeathWide.json");
    const float fov = movieCamera->GetFirstKeyframeFov();
    deathWidePreviewAnchor = selectedAnchor;
    deathWidePreviewTarget = target;
    deathWidePreviewAnchorName = selectedAnchor->GetName();
    deathWidePreviewFov = fov;

    // ToggleMovieCamera copies the current render pose, so CUT afterwards.
    if (!cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(scene);
    const DirectX::XMFLOAT3 eye = selectedAnchor->GetComponentLocation();
    movieCamera->CutToWorldPose(
        eye,
        CreateDeathWideLookRotation(eye, targetPosition),
        fov);
    return true;
}

void MovieCameraManagerActor::UpdateDeathWideAnchorPreview()
{
    if (!deathWideAnchorPreviewEnabled)
        return;

    auto scene = GetOwnerScene();
    auto cameraManager = scene ? scene->GetCameraManager() : nullptr;
    auto movieCamera = movieCameraWeakPtr.lock();
    auto anchor = deathWidePreviewAnchor.lock();
    auto target = deathWidePreviewTarget.lock();
    if (!cameraManager || !movieCamera || !anchor || !target ||
        !cameraManager->IsUseMovie())
        return;
    const auto movieOwner = movieCamera->GetOwner();
    if (!movieOwner)
        return;

    const DirectX::XMFLOAT3 eye = anchor->GetComponentLocation();
    const DirectX::XMFLOAT3 targetPosition = target->GetComponentLocation();
    const DirectX::XMFLOAT3 eyeToTargetVector =
        MathHelper::Subtract(targetPosition, eye);
    const float eyeToTargetLength = MathHelper::Length(eyeToTargetVector);
    deathWideDebugEyeToTargetDirection = eyeToTargetLength > FLT_EPSILON
        ? MathHelper::Normalize(eyeToTargetVector)
        : DirectX::XMFLOAT3{};
    const DirectX::XMFLOAT4 generatedRotation =
        CreateDeathWideLookRotation(eye, targetPosition);
    deathWideDebugGeneratedRotation = generatedRotation;
    deathWideDebugRotationBefore = movieOwner->GetQuaternionRotation();
    movieCamera->ApplyWorldPose(
        eye,
        generatedRotation,
        deathWidePreviewFov);
    deathWideDebugRotationAfter = movieOwner->GetQuaternionRotation();
    deathWideDebugMovieForward = movieCamera->GetForward();
    const DirectX::XMFLOAT4 componentRotation =
        movieCamera->GetComponentRotation();
    deathWideDebugComponentRotationAfter = componentRotation;
    DirectX::XMVECTOR directForward = DirectX::XMVector3Rotate(
        DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
        DirectX::XMLoadFloat4(&componentRotation));
    directForward = DirectX::XMVector3Normalize(directForward);
    DirectX::XMStoreFloat3(&deathWideDebugDirectForward, directForward);
    deathWideDebugForwardDot = eyeToTargetLength > FLT_EPSILON
        ? deathWideDebugMovieForward.x * deathWideDebugEyeToTargetDirection.x +
          deathWideDebugMovieForward.y * deathWideDebugEyeToTargetDirection.y +
          deathWideDebugMovieForward.z * deathWideDebugEyeToTargetDirection.z
        : 0.0f;
    deathWideDebugDirectForwardDot = eyeToTargetLength > FLT_EPSILON
        ? deathWideDebugDirectForward.x * deathWideDebugEyeToTargetDirection.x +
          deathWideDebugDirectForward.y * deathWideDebugEyeToTargetDirection.y +
          deathWideDebugDirectForward.z * deathWideDebugEyeToTargetDirection.z
        : 0.0f;
    deathWideDebugRotationValid = true;
}

// 繝ｻ・ｽh繝ｻ・ｽA繝ｻ・ｽ繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽr繝ｻ・ｽ[繝ｻ・ｽ繝ｻ・ｽ・・腸・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
void MovieCameraManagerActor::PlayDoorMovie()
{
    if (auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene()))
    {
        gameScene->PrepareTutorialForBossEntry();
        gameScene->EnterBossRoomLockOnScope();
    }

    if (auto player = GetOwnerScene()->GetActorManager()->GetActorOfType < Player>())
    {
        // 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽo繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽn繝ｻ・ｽ・懊ｑ・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・ｑ・ｿ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・ｑ・ｿ・ｽ・顔衍繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ
        player->StartEvent();


        // 繝ｻ・ｽv繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽC繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽ・御ｽ咲ｽｮ繝ｻ・ｽ繝ｻ・ｽ・・ｮ壹☆繝ｻ・ｽ繝ｻ・ｽ
        DirectX::XMFLOAT3 fixedPosition = { -7.6f,-0.073f,10.16f };
        player->SetPosition(fixedPosition); // 繝ｻ・ｽv繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽC繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ[繝ｻ・ｽ・御ｽ咲ｽｮ繝ｻ・ｽ繝ｻ・ｽ・・ｮ壹☆繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽW繝ｻ・ｽ・芽ｨｭ謚ｵ・ｿ・ｽ
        player->rotationComponent->SetDirection({ 1.0f,0.0f,0.0f });
    }

    if (auto cameraManager = GetOwnerScene()->GetCameraManager())
    {
        if (!cameraManager->IsUseMovie())
        {// 繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・↓繝ｻ・ｿ・ｽ繝ｻ・ｽ[繝ｻ・ｽr繝ｻ・ｽ[繝ｻ・ｽJ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽg繝ｻ・ｽp繝ｻ・ｽ繝ｻ・ｽ繝ｻ・ｽ・・↑繧托ｽｿ・ｽ繝ｻ・ｽ鮠ｾ繝ｻ・ｿ・ｽ・後∩蛻・ｑ・ｿ・ｽ・悶ｑ・ｿ・ｽ
            cameraManager->ToggleMovieCamera(GetOwnerConstScene());
        }
    }
    std::string fileName = "door_pre_open.json";
    auto movieCamera = this->movieCameraWeakPtr.lock();
    movieCamera->LoadFromJson("./Data/Saves/MovieCameras/" + fileName);
    movieCamera->Start();
    doorMovieState = DoorMovieState::DoorPreMovie;
}
