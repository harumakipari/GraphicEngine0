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
        {// ���悪�I��������player��A�b�v���铮��Đ�
            PlayMovie(playerMovieFileName);
            doorMovieState = DoorMovieState::UpPlayerMovie;
            if (player)
            {
                // �C�x���g�V�[�����J�n�������Ƃ�ʒm����
                player->StartEvent();
                player->PlayBodyAnimation("Recall_0", false);
            }
            // �����̃V�����f���A�̉��̌������
            for (auto chandelier : chandelierActors)
            {
                chandelier->SetFireScaleToZero();
            }
            // �����̘X�C�X�^���h�̉��̌������
            for (auto candleStand : candleStandActors)
            {
                candleStand->SetFireLightScale({ 0.0f,0.0f,0.0f });
            }
        }
        break;
    case DoorMovieState::UpPlayerMovie:
    {
        if (movieCamera->IsMovieFinish())
        {// �v���C���[��A�b�v���铮�悪�I�������A
            if (player)
            {// �v���C���[�̃A�j���[�V������ҋ@�ɕύX
                player->PlayBodyAnimation("Idle", true);
            }
            if (gameScene)
            {
                // �{�X�̕�����Â�����
                gameScene->SetBossRoomLerpFactor(0.0f);
                // �ڂ�Bloom�݂̂�I���ɂ���
            }
            // �h�A���J���J�������[�N
            PlayMovie(doorOpenMovieFileName);
            // �h�A���J���A�j���[�V����
            if (doorActor)
            {
                doorActor->Open();
            }
            // �G�̃A�j���[�V������~�߂�
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
        {// �h�A���J������A
            CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_groan.wav", 0.2f);
            // �G�̖ڋʂ�����
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
        // �{�X�̖ڋʂ�Ȃ���
        // ��������X�ɖ��邭����
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
        // �����̃V�����f���A�̉��̌�����X�ɖ߂�
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
                // �h�A��߂���Ԃɂ���
                if (doorActor)
                {
                    doorActor->Closed();
                }
                // �v���C���[�̈ʒu��h�A�O�ɂ���
                if (player)
                {
                    player->SetPosition({ -4.827f,-0.098f,11.724f });
                    // �v���C���[�̃A�j���[�V������Đ�
                    player->PlayBodyAnimation("Level_Start_Cut", false);
                }
                // �G�̈ʒu�𕔉��̉��ɂ���
                if (gruxEnemy)
                {
                    gruxEnemy->SetEulerRotation({ 0.0f,-90.0f,0.0f });
                }
                doorMovieState = DoorMovieState::UpPlayerCombatMovie;
            });

        // �v���C���[�A�b�v�J�������[�N
        PlayMovie(playerCombatMovieFileName);
        // �����̘X�C�X�^���h�̉��̌���߂�
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
        {// �J�������[�N���I�������A
            doorMovieState = DoorMovieState::EnemyMovie;
        }
        break;
    case DoorMovieState::EnemyMovie:
        // �{�X�A�b�v�J�������[�N
        PlayMovie(bossRoarMovieFileName);
        // �G���i����
        if (gruxEnemy)
        {
            gruxEnemy->PlayBodyAnimation("Ultimate_Roar_0", false);
        }
        doorMovieState = DoorMovieState::EnemyName;
        break;
    case DoorMovieState::EnemyName:
        if (movieCamera->IsMovieFinish())
        {
            // BGM��Đ�����
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
                // �J������{�X�펞�̏�ԂɕύX����
                player->SetIsBossBattle(true);
            }
            //�J������O�l�̂ɖ߂�
            if (scene->GetCameraManager()->IsUseMovie())
            {// ���[�r�[�J�������g�p���̏ꍇ�̂ݐ؂�ւ�
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
                                // ���o�����I��������Ƃ�ʒm����
                                player->EndEvent();
                            }
                            if (gruxEnemy)
                            {// �����Ń{�X�̖��O��UI�����
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
            {// �����Ń{�X�̖��O��UI�����
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

void MovieCameraManagerActor::PlayMovie(const std::string& file)
{
    if (auto cameraManager = GetOwnerScene()->GetCameraManager())
    {
        if (!cameraManager->IsUseMovie())
        {// ���łɃ��[�r�[�J�������g�p���łȂ��ꍇ�̂ݐ؂�ւ�
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

// �h�A��J�����[�r�[��Đ�����
void MovieCameraManagerActor::PlayDoorMovie()
{
    if (auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene()))
    {
        gameScene->PrepareTutorialForBossEntry();
        gameScene->EnterBossRoomLockOnScope();
    }

    if (auto player = GetOwnerScene()->GetActorManager()->GetActorOfType < Player>())
    {
        // ���o���n�܂������Ƃ���Ƃ�ʒm����
        player->StartEvent();


        // �v���C���[�̈ʒu��Œ肷��
        DirectX::XMFLOAT3 fixedPosition = { -7.6f,-0.073f,10.16f };
        player->SetPosition(fixedPosition); // �v���C���[�̈ʒu��Œ肷����W�ɐݒ�
        player->rotationComponent->SetDirection({ 1.0f,0.0f,0.0f });
    }

    if (auto cameraManager = GetOwnerScene()->GetCameraManager())
    {
        if (!cameraManager->IsUseMovie())
        {// ���łɃ��[�r�[�J�������g�p���łȂ��ꍇ�̂ݐ؂�ւ�
            cameraManager->ToggleMovieCamera(GetOwnerConstScene());
        }
    }
    std::string fileName = "door_pre_open.json";
    auto movieCamera = this->movieCameraWeakPtr.lock();
    movieCamera->LoadFromJson("./Data/Saves/MovieCameras/" + fileName);
    movieCamera->Start();
    doorMovieState = DoorMovieState::DoorPreMovie;
}
