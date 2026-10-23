#include "pch.h"
#include "GameBossDeathController.h"
#include "Engine/Debug/SceneEditor.h"
#include "Engine/Scene/SceneSetting.h"
#include "Engine/Camera/CameraManager.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Input/InputSystem.h"
#include "Game/Actors/Camera/Camera.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Actors/UI/GameBossDeathUIActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/DarkGame/GameBgmDirector.h"
#include "UI/Widgets/Widget.h"

namespace
{
    void AdvanceBossDeathHold(float& time, float& direction, float minTime,
        float maxTime, float rate, float entryRate, float deltaTime)
    {
        float remaining = (std::max)(deltaTime, 0.0f);
        if (time < minTime || time > maxTime)
        {
            const float target = time < minTime ? minTime : maxTime;
            direction = time < minTime ? 1.0f : -1.0f;
            if (entryRate <= 0.0f)
                return;
            const float entryDuration = std::abs(target - time) / entryRate;
            if (remaining < entryDuration)
            {
                time += direction * entryRate * remaining;
                return;
            }
            time = target;
            remaining -= entryDuration;
        }

        const float span = maxTime - minTime;
        if (span <= 0.0f || rate <= 0.0f)
            return;
        time += direction * std::fmod(rate * remaining, 2.0f * span);
        while (time > maxTime || time < minTime)
        {
            if (time > maxTime)
            {
                time = maxTime - (time - maxTime);
                direction = -1.0f;
            }
            else
            {
                time = minTime + (minTime - time);
                direction = 1.0f;
            }
        }
    }

    constexpr float RecallFirstSeTime = 1.4f;
    constexpr float RecallTimeSafetyMargin = 0.001f;
    constexpr std::array<const char*, 5> BossDeathPresetPaths = {
        "Data/Saves/ScenePresets/BossDeath_Scream.json",
        "Data/Saves/ScenePresets/BossDeath_Fall.json",
        "Data/Saves/ScenePresets/BossDeath_Landing.json",
        "Data/Saves/ScenePresets/BossDeath_Finish.json",
        "Data/Saves/ScenePresets/BossDeath_Result.json",
    };
}

bool GameBossDeathController::LoadBossDeathShots()
{
    bossDeathShotsLoaded = false;
    try
    {
        for (size_t shotIndex = 0; shotIndex < BossDeathPresetPaths.size(); ++shotIndex)
        {
            SceneState sceneState{};
            SceneEditor::LoadSceneState(BossDeathPresetPaths[shotIndex], sceneState);

            const auto findActor = [&sceneState](const char* actorName)
                {
                    return std::find_if(sceneState.actorStates.begin(), sceneState.actorStates.end(),
                        [actorName](const ActorTransformState& actor)
                        {
                            return actor.name == actorName;
                        });
                };

            const auto playerState = findActor("Player");
            const auto bossState = findActor("GruxEnemy");
            if (playerState == sceneState.actorStates.end() ||
                bossState == sceneState.actorStates.end())
            {
                const std::string message = std::format(
                    "Boss death preset is missing Player or GruxEnemy: {}",
                    BossDeathPresetPaths[shotIndex]);
                Logger::Error(Logger::LogCategory::System, message.c_str());
                return false;
            }

            auto& shot = bossDeathShots[shotIndex];
            shot.camera.position = sceneState.camera.position;
            shot.camera.rotation = sceneState.camera.rotation;
            shot.camera.fov = sceneState.camera.fov;
            shot.player = { playerState->position, playerState->rotation };
            shot.boss = { bossState->position, bossState->rotation };

            const auto& shader = sceneState.shader;
            shot.dof = {
                shader.focusDistance,
                shader.dofNearRange,
                shader.dofRange,
                shader.dofBlurStrength,
                shader.enableBlur,
                shader.enableDof,
            };
        }

    }
    catch (const std::exception& exception)
    {
        const std::string message = std::format(
            "Failed to load boss death presets: {}", exception.what());
        Logger::Error(Logger::LogCategory::System, message.c_str());
        return false;
    }

    bossDeathShotsLoaded = true;
    return true;
}


void GameBossDeathController::CaptureGameplayDof(const SceneShaderConstants& shader)
{
    bossDeathGameplayDof = { shader.focusDistance, shader.dofNearRange, shader.dofRange,
        shader.dofBlurStrength, shader.enableBlur, shader.enableDof };
    bossDeathGameplayDofCaptured = true;
}

void GameBossDeathController::RestoreGameplayDof(SceneShaderConstants& shader)
{
    if (bossDeathGameplayDofCaptured)
        ApplyBossDeathDof(shader, bossDeathGameplayDof);
    bossDeathGameplayDofCaptured = false;
}

void GameBossDeathController::ApplyBossDeathDof(SceneShaderConstants& shader, const BossDeathDofState& dof)
{
    shader.focusDistance = dof.focusDistance;
    shader.dofNearRange = dof.nearRange;
    shader.dofRange = dof.range;
    shader.dofBlurStrength = dof.blurStrength;
    shader.enableBlur = dof.enableBlur;
    shader.enableDof = dof.enableDof;
}

void GameBossDeathController::CutToShot(Shot shot, CinematicCameraComponent* camera, SceneShaderConstants& shader) const
{
    const auto index = static_cast<size_t>(shot);
    if (index >= bossDeathShots.size() || !camera)
        return;
    camera->CutToPose(bossDeathShots[index].camera);
    ApplyBossDeathDof(shader, bossDeathShots[index].dof);
}

void GameBossDeathController::BlendToLanding(CinematicCameraComponent* camera, SceneShaderConstants& shader, float duration) const
{
    const auto& shot = bossDeathShots[static_cast<size_t>(Shot::Landing)];
    if (camera)
        camera->BlendToPose(shot.camera, duration);
    ApplyBossDeathDof(shader, shot.dof);
}

void GameBossDeathController::BlendToResult(CinematicCameraComponent& camera, SceneShaderConstants& shader, float duration) const
{
    const auto& shot = bossDeathShots[static_cast<size_t>(Shot::Result)];
    camera.BlendToPose(shot.camera, duration);
    ApplyBossDeathDof(shader, shot.dof);
}

void GameBossDeathController::SetFadeAlpha(UIImageComponent* fade, const float alpha)
{
    if (!fade)
        return;

    const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
    fade->SetColor(CoreColor{ 0.0f, 0.0f, 0.0f, clampedAlpha });
    fade->SetVisible(clampedAlpha > 0.0f);
}

void GameBossDeathController::StopCinematicAudio()
{
    StopBossDeathGroanLoop();
    if (bossDeathVoiceAudio)
    {
        bossDeathVoiceAudio->Stop(false);
        bossDeathVoiceAudio.reset();
    }
}

void GameBossDeathController::Start(GameBossDeathUIActor& ui, UIImageComponent* fade)
{
    bossDeathWalkStopStartPosition = {};
    bossDeathWalkStopFinishPosition = {};
    bossDeathWalkStopStartRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bossDeathWalkStopFinishRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bossDeathPhase = BossDeathPhase::FadeOut;
    bossDeathPhaseElapsed = 0.0f;
    bossDeathRecallPromptTime = bossDeathRecallPromptMinTime;
    bossDeathRecallPromptDirection = 1.0f;
    bossDeathFinishHoldTime = bossDeathFinishHoldMinTime;
    bossDeathFinishHoldDirection = 1.0f;
    bossDeathFwdPromptTime = bossDeathFwdPromptMinTime;
    bossDeathFwdPromptDirection = 1.0f;
    bossDeathScreamHoldTime = bossDeathScreamHoldMin;
    bossDeathScreamHoldDirection = 1.0f;
    bossDeathFinishInputEnabled = false;
    StopCinematicAudio();
    ui.SetFinishGuideVisible(false);
    SetFadeAlpha(fade, 0.0f);

}

void GameBossDeathController::Reset(SceneShaderConstants& shader, GameBossDeathUIActor& ui, UIImageComponent* fade)
{
    RestoreGameplayDof(shader);

    bossDeathWalkStopStartPosition = {};
    bossDeathWalkStopFinishPosition = {};
    bossDeathWalkStopStartRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bossDeathWalkStopFinishRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bossDeathPhase = BossDeathPhase::FadeOut;
    bossDeathPhaseElapsed = 0.0f;
    bossDeathRecallPromptTime = bossDeathRecallPromptMinTime;
    bossDeathRecallPromptDirection = 1.0f;
    bossDeathFinishHoldTime = bossDeathFinishHoldMinTime;
    bossDeathFinishHoldDirection = 1.0f;
    bossDeathFwdPromptTime = bossDeathFwdPromptMinTime;
    bossDeathFwdPromptDirection = 1.0f;
    bossDeathScreamHoldTime = bossDeathScreamHoldMin;
    bossDeathScreamHoldDirection = 1.0f;
    bossDeathApproachStartRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bossDeathFinishInputEnabled = false;
    ui.SetFinishGuideVisible(false);
    SetFadeAlpha(fade, 0.0f);

}

bool GameBossDeathController::IsFinishPromptActive() const
{
    return bossDeathPhase >= BossDeathPhase::RecallPingPong;
}

bool GameBossDeathController::IsCinematicAudioActive() const
{
    return bossDeathPhase < BossDeathPhase::RecallPingPong;
}

bool GameBossDeathController::CanSkip() const
{
    return bossDeathPhase < BossDeathPhase::RecallPingPong;
}

void GameBossDeathController::StartBossDeathGroanLoop()
{
    StopBossDeathGroanLoop();
    bossDeathGroanAudio = std::make_shared<CoreStandaloneAudioSource>(L"./Data/Sound/SE/enemy_groan.wav");
    bossDeathGroanAudio->SetVolume(0.05f);
    bossDeathGroanAudio->Play(true);
}

void GameBossDeathController::StopBossDeathGroanLoop()
{
    if (!bossDeathGroanAudio)
        return;

    bossDeathGroanAudio->Stop(false);
    bossDeathGroanAudio.reset();
}

void GameBossDeathController::UpdateBossDeathPromptLoop(GruxEnemy* gruxEnemyActor, const float deltaTime)
{
    AdvanceBossDeathHold(bossDeathFwdPromptTime, bossDeathFwdPromptDirection,
        bossDeathFwdPromptMinTime, bossDeathFwdPromptMaxTime,
        bossDeathFwdPromptPlaybackRate, bossDeathFwdPlaybackRate, deltaTime);

    if (gruxEnemyActor)
    {
        if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
        {
            controller->HoldAnimationPose("Death_B_0", bossDeathFwdPromptTime);
        }
    }
}

void GameBossDeathController::UpdateBossDeathScreamHold(GruxEnemy* gruxEnemyActor, const float deltaTime)
{
    if (!gruxEnemyActor)
        return;
    const auto mesh = gruxEnemyActor->GetSkeletalMeshComponent();
    if (!mesh || !mesh->IsVisible())
        return;

    AdvanceBossDeathHold(bossDeathScreamHoldTime, bossDeathScreamHoldDirection,
        bossDeathScreamHoldMin, bossDeathScreamHoldMax,
        bossDeathScreamHoldRate, bossDeathScreamHoldRate, deltaTime);
    if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
        controller->HoldAnimationPose("Death_B_0", bossDeathScreamHoldTime);
}

bool GameBossDeathController::SkipToFinishPrompt(const Context& context)
{
    auto* player = context.player;
    auto* gruxEnemyActor = context.boss;
    auto* cinemaCameraActor = context.cameraActor;
    auto* cameraManager = context.cameraManager;
    auto* bossDeathUIActor = context.finishUI;
    auto& bgmDirector = *context.bgm;

    if (!CanSkip())
        return false;

    if (!AreShotsLoaded() || !player || !gruxEnemyActor || !cinemaCameraActor)
    {
        Logger::Warning(Logger::LogCategory::System,
            "Boss death skip ignored because finish-prompt staging is unavailable");
        return false;
    }

    // Finish staging can bypass the remaining damage-flash decay.
    gruxEnemyActor->ClearDamageFlash();
    ClampBossDeathPreviewTuning(player, gruxEnemyActor);
    if (cinemaCameraActor)
        if (auto* camera = dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()))
            camera->SetDebugInputEnabled(false);
    gruxEnemyActor->StopBossDeathCinematicOneShots();
    if (bossDeathVoiceAudio)
    {
        bossDeathVoiceAudio->Stop(false);
        bossDeathVoiceAudio.reset();
    }
    bgmDirector.BeginBossDeathFade();
    bgmDirector.PlayBossDeathSecondBgm();
    if (!bossDeathGroanAudio)
        StartBossDeathGroanLoop();
    SetFadeAlpha(context.fadeOverlay, 0.0f);

    bossDeathPhaseElapsed = 0.0f;
    bossDeathRecallPromptTime = bossDeathRecallPromptMinTime;
    bossDeathRecallPromptDirection = 1.0f;
    bossDeathFwdPromptTime = bossDeathFwdPromptMinTime;
    bossDeathFwdPromptDirection = 1.0f;

    if (const auto playerCapsule = std::dynamic_pointer_cast<ShapeComponent>(
        player->FindComponentByName("capsuleComponent")))
    {
        playerCapsule->DisableCollision();
    }
    if (const auto bossCapsule = std::dynamic_pointer_cast<ShapeComponent>(
        gruxEnemyActor->FindComponentByName("enemyCapsuleComponent")))
    {
        bossCapsule->DisableCollision();
    }

    const auto& finishPlayerPose = GetFinishPlayerPose();
    DirectX::XMFLOAT3 finishPlayerPosition = finishPlayerPose.position;
    finishPlayerPosition.x += bossDeathFinishPlayerPositionOffset.x;
    finishPlayerPosition.y += bossDeathFinishPlayerPositionOffset.y;
    finishPlayerPosition.z += bossDeathFinishPlayerPositionOffset.z;
    player->ClearTransientBattleActions();
    player->SetPosition(finishPlayerPosition);
    player->SetQuaternionRotation(finishPlayerPose.rotation);
    player->UpdateAllComponentTransforms();
    if (const auto controller = player->GetBodyAnimationController())
    {
        controller->ReleaseLatchedVisualPose();
        controller->ReleaseHeldAnimationPose();
        controller->ResetAnimationRate();
        player->PlayBodyAnimation("Result_Recall", false, true, 0.0f, true);
        if (!controller->SetPlaybackRange(0.0f, bossDeathRecallPromptMinTime) ||
            !controller->HoldAnimationPose("Result_Recall", bossDeathRecallPromptTime))
        {
            Logger::Error(Logger::LogCategory::System,
                "Failed to hold Result_Recall for boss death skip");
        }
    }

    const auto& landingBossPose = GetLandingBossPose();
    DirectX::XMFLOAT3 finishBossPosition = landingBossPose.position;
    finishBossPosition.x += bossDeathFwdPositionOffset.x;
    finishBossPosition.y += bossDeathFwdPositionOffset.y;
    finishBossPosition.z += bossDeathFwdPositionOffset.z;
    gruxEnemyActor->StopBattleActions();
    gruxEnemyActor->AbortBehaviorTreeForDeath();
    gruxEnemyActor->EndFinalHitReaction();
    gruxEnemyActor->SetPosition(finishBossPosition);
    gruxEnemyActor->SetQuaternionRotation(landingBossPose.rotation);
    gruxEnemyActor->UpdateAllComponentTransforms();
    if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
    {
        controller->ReleaseHeldAnimationPose();
        controller->ResetAnimationRate();
        gruxEnemyActor->PlayBodyAnimation("Death_B_0", false, true, 0.0f, true,
            "GameScene::SkipBossDeathToFinishPrompt");
        if (!controller->SetPlaybackRange(bossDeathFwdStartTime, bossDeathFwdEndTime) ||
            !controller->HoldAnimationPose("Death_B_0", bossDeathFwdPromptTime))
        {
            Logger::Error(Logger::LogCategory::System,
                "Failed to hold Death_B_0 for boss death skip");
        }
    }

    if (cameraManager->IsUseDebug())
        cameraManager->ToggleCamera(context.scene);
    if (cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(context.scene);
    if (!cameraManager->IsUseCinematic())
        cameraManager->ToggleCinematicCamera(context.scene);
    CutToShot(Shot::Finish,
                cinemaCameraActor ? dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()) : nullptr,
                *context.shader);
    gruxEnemyActor->StopBossDeathCinematicOneShots();

    bossDeathPhase = BossDeathPhase::RecallPingPong;
    bossDeathFinishInputEnabled = true;
    bossDeathUIActor->SetFinishGuideVisible(true);
    return true;
}
bool GameBossDeathController::SetupBossDeathCinematic(const Context& context)
{
    auto* player = context.player;
    auto* gruxEnemyActor = context.boss;
    auto* cinemaCameraActor = context.cameraActor;
    auto* cameraManager = context.cameraManager;
    auto* bossDeathUIActor = context.finishUI;
    auto& bgmDirector = *context.bgm;

    if (!AreShotsLoaded() || !player || !gruxEnemyActor || !cinemaCameraActor)
        return false;

    // SetupCinematic is entered only after FadeOut has set the overlay to full
    // black. Release the combat pose latch before staging the Player or cameras.
    if (const auto controller = player->GetBodyAnimationController())
        controller->ReleaseLatchedVisualPose();

    if (const auto playerCapsule = std::dynamic_pointer_cast<ShapeComponent>(
        player->FindComponentByName("capsuleComponent")))
    {
        playerCapsule->DisableCollision();
    }
    if (const auto bossCapsule = std::dynamic_pointer_cast<ShapeComponent>(
        gruxEnemyActor->FindComponentByName("enemyCapsuleComponent")))
    {
        bossCapsule->DisableCollision();
    }

    // Boss uses the Scream staging transform. Player starts from the shared
    // Fall/Landing transform and approaches the Finish transform later.
    const auto fallPlayerPose = GetFallPlayerPose();
    const auto screamBossPose = GetScreamBossPose();
    player->SetPosition(fallPlayerPose.position);
    player->SetQuaternionRotation(fallPlayerPose.rotation);
    player->UpdateAllComponentTransforms();
    gruxEnemyActor->SetPosition(screamBossPose.position);
    gruxEnemyActor->SetQuaternionRotation(screamBossPose.rotation);
    gruxEnemyActor->UpdateAllComponentTransforms();

    if (cameraManager->IsUseDebug())
        cameraManager->ToggleCamera(context.scene);
    if (cameraManager->IsUseMovie())
        cameraManager->ToggleMovieCamera(context.scene);
    if (!cameraManager->IsUseCinematic())
        cameraManager->ToggleCinematicCamera(context.scene);

    // ToggleCinematicCamera copies the gameplay pose first. Overwrite every pose
    // field and DOF in this same update so that copied pose is never rendered.
    CutToShot(Shot::Scream,
                cinemaCameraActor ? dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()) : nullptr,
                *context.shader);
    const auto controller = gruxEnemyActor->GetBodyAnimationController();
    if (controller)
        controller->SetAnimationRate(bossDeathRoarPlaybackRate);
    gruxEnemyActor->PlayBodyAnimation("Ultimate_Roar_0", false, true, 0.1f, true,
        "GameScene::SetupBossDeathCinematic");
    if (!controller || !controller->SetPlaybackRange(
        bossDeathRoarStartTime, bossDeathRoarEndTime))
    {
        Logger::Error(Logger::LogCategory::System,
            "Failed to set Ultimate_Roar_0 playback range for boss death preview");
    }
    return true;
}

void GameBossDeathController::ClampBossDeathPreviewTuning(Player* player, GruxEnemy* gruxEnemyActor)
{
    bossDeathFallToLandingBlendDuration =
        (std::max)(bossDeathFallToLandingBlendDuration, 0.0f);
    bossDeathRoarPlaybackRate = std::clamp(
        bossDeathRoarPlaybackRate, 0.1f, 2.0f);
    bossDeathStunPlaybackRate = std::clamp(
        bossDeathStunPlaybackRate, 0.1f, 2.0f);
    bossDeathFwdPlaybackRate = std::clamp(
        bossDeathFwdPlaybackRate, 0.1f, 2.0f);
    bossDeathPlayerApproachDuration =
        (std::max)(bossDeathPlayerApproachDuration, 0.0f);
    bossDeathWalkStopStartDistance = std::isfinite(bossDeathWalkStopStartDistance)
        ? std::clamp(bossDeathWalkStopStartDistance, 0.0f, 2.0f) : 0.35f;
    bossDeathRecallPromptPlaybackRate =
        (std::max)(bossDeathRecallPromptPlaybackRate, 0.0f);
    bossDeathFwdPromptPlaybackRate =
        (std::max)(bossDeathFwdPromptPlaybackRate, 0.0f);
    bossDeathStunToDeathBlend = std::isfinite(bossDeathStunToDeathBlend)
        ? std::clamp(bossDeathStunToDeathBlend, 0.0f, 1.0f) : 0.25f;
    bossDeathScreamHoldRate = std::isfinite(bossDeathScreamHoldRate)
        ? std::clamp(bossDeathScreamHoldRate, 0.0f, 2.0f) : 0.30f;
    bossDeathHuskDelay = (std::max)(bossDeathHuskDelay, 0.0f);

    bossDeathFinishHoldPlaybackRate = std::isfinite(bossDeathFinishHoldPlaybackRate)
        ? std::clamp(bossDeathFinishHoldPlaybackRate, 0.0f, 3.0f) : 0.07f;
    if (player)
    {
        if (const auto controller = player->GetBodyAnimationController())
        {
            const float duration = controller->GetAnimationLength("Result_Recall");
            if (std::isfinite(duration) && duration >= 0.0f)
            {
                const float margin = (std::min)(0.001f, duration);
                bossDeathFinishHoldMinTime = std::clamp(
                    std::isfinite(bossDeathFinishHoldMinTime) ? bossDeathFinishHoldMinTime : 1.58f,
                    0.0f, duration - margin);
                bossDeathFinishHoldMaxTime = std::clamp(
                    std::isfinite(bossDeathFinishHoldMaxTime) ? bossDeathFinishHoldMaxTime : 1.70f,
                    bossDeathFinishHoldMinTime + margin, duration);
                bossDeathFinishHoldTime = std::clamp(bossDeathFinishHoldTime,
                    bossDeathFinishHoldMinTime, bossDeathFinishHoldMaxTime);
            }
        }
    }

    const float recallMaxLimit = RecallFirstSeTime - RecallTimeSafetyMargin;
    bossDeathRecallPromptMinTime = std::clamp(
        bossDeathRecallPromptMinTime,
        0.0f,
        recallMaxLimit - RecallTimeSafetyMargin);
    bossDeathRecallPromptMaxTime = std::clamp(
        bossDeathRecallPromptMaxTime,
        bossDeathRecallPromptMinTime + RecallTimeSafetyMargin,
        recallMaxLimit);

    if (gruxEnemyActor)
    {
        if (const auto controller = gruxEnemyActor->GetBodyAnimationController())
        {
            const auto clampPlaybackRange = [](float& start, float& end,
                const float duration)
                {
                    constexpr float Margin = 0.001f;
                    if (!std::isfinite(duration) || duration <= Margin)
                    {
                        start = 0.0f;
                        end = (std::max)(duration, 0.0f);
                        return;
                    }

                    start = std::clamp(start, 0.0f, duration - Margin);
                    end = std::clamp(end, start + Margin, duration);
                };

            const float roarDuration =
                controller->GetAnimationLength("Ultimate_Roar_0");
            clampPlaybackRange(
                bossDeathRoarStartTime, bossDeathRoarEndTime, roarDuration);

            const float deathFwdDuration = controller->GetAnimationLength("Death_B_0");
            clampPlaybackRange(
                bossDeathFwdStartTime, bossDeathFwdEndTime, deathFwdDuration);
            clampPlaybackRange(
                bossDeathFwdPromptMinTime,
                bossDeathFwdPromptMaxTime,
                deathFwdDuration);
            if (!std::isfinite(bossDeathScreamHoldMin))
                bossDeathScreamHoldMin = 1.50f;
            if (!std::isfinite(bossDeathScreamHoldMax))
                bossDeathScreamHoldMax = 1.58f;
            clampPlaybackRange(bossDeathScreamHoldMin, bossDeathScreamHoldMax, deathFwdDuration);
        }
    }
}

bool GameBossDeathController::Update(const Context& context, const float deltaTime, const bool huskComplete)
{
    auto* player = context.player;
    auto* gruxEnemyActor = context.boss;
    auto* cinemaCameraActor = context.cameraActor;
    auto* cameraManager = context.cameraManager;
    auto* bossDeathUIActor = context.finishUI;
    auto& bgmDirector = *context.bgm;

    ClampBossDeathPreviewTuning(player, gruxEnemyActor);
    bossDeathPhaseElapsed += deltaTime;

    // Keep the boss held until capture hides its mesh, including a pending capture.
    if (bossDeathPhase == BossDeathPhase::HuskDelay ||
        bossDeathPhase == BossDeathPhase::HuskPreview)
        UpdateBossDeathScreamHold(gruxEnemyActor, deltaTime);

    if (bossDeathPhase == BossDeathPhase::HuskDelay ||
        bossDeathPhase == BossDeathPhase::HuskPreview)
    {
        const float span = bossDeathFinishHoldMaxTime - bossDeathFinishHoldMinTime;
        if (span > 0.0f)
        {
            // Remove whole round trips, then reflect overshoot as in Recall Prompt.
            const float advance = std::fmod(
                bossDeathFinishHoldPlaybackRate * (std::max)(deltaTime, 0.0f), 2.0f * span);
            bossDeathFinishHoldTime += bossDeathFinishHoldDirection * advance;
            while (bossDeathFinishHoldTime > bossDeathFinishHoldMaxTime ||
                bossDeathFinishHoldTime < bossDeathFinishHoldMinTime)
            {
                if (bossDeathFinishHoldTime > bossDeathFinishHoldMaxTime)
                {
                    bossDeathFinishHoldTime = bossDeathFinishHoldMaxTime -
                        (bossDeathFinishHoldTime - bossDeathFinishHoldMaxTime);
                    bossDeathFinishHoldDirection = -1.0f;
                }
                else
                {
                    bossDeathFinishHoldTime = bossDeathFinishHoldMinTime +
                        (bossDeathFinishHoldMinTime - bossDeathFinishHoldTime);
                    bossDeathFinishHoldDirection = 1.0f;
                }
            }
        }
        else
        {
            bossDeathFinishHoldTime = bossDeathFinishHoldMinTime;
        }
        if (player)
        {
            if (const auto controller = player->GetBodyAnimationController())
                controller->HoldAnimationPose("Result_Recall", bossDeathFinishHoldTime);
        }
    }

    switch (bossDeathPhase)
    {
    case BossDeathPhase::FadeOut:
    {
        const float duration = (std::max)(bossDeathFadeOutDuration, FLT_EPSILON);
        SetFadeAlpha(context.fadeOverlay, bossDeathPhaseElapsed / duration);
        if (bossDeathPhaseElapsed >= duration)
        {
            SetFadeAlpha(context.fadeOverlay, 1.0f);
            bossDeathPhase = BossDeathPhase::SetupCinematic;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::SetupCinematic:
        SetFadeAlpha(context.fadeOverlay, 1.0f);
        if (SetupBossDeathCinematic(context))
        {
            bossDeathPhase = BossDeathPhase::FadeInScream;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;

    case BossDeathPhase::FadeInScream:
    {
        const float duration = (std::max)(bossDeathFadeInDuration, FLT_EPSILON);
        SetFadeAlpha(context.fadeOverlay, 1.0f - bossDeathPhaseElapsed / duration);
        if (bossDeathPhaseElapsed >= duration)
        {
            SetFadeAlpha(context.fadeOverlay, 0.0f);
            bossDeathPhase = BossDeathPhase::DeathScream;
            bgmDirector.BeginBossDeathFade();
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::DeathScream:
    {
        const auto controller = gruxEnemyActor ? gruxEnemyActor->GetBodyAnimationController()
            : nullptr;
        if (controller &&
            controller->GetCurrentAnimationName() == "Ultimate_Roar_0" &&
            !controller->IsPlayAnimation())
        {
            CutToShot(Shot::Fall,
                cinemaCameraActor ? dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()) : nullptr,
                *context.shader);
            controller->ResetAnimationRate();
            controller->SetAnimationRate(bossDeathStunPlaybackRate);
            gruxEnemyActor->PlayBodyAnimation(
                "Result_Down_Start", false, true, 0.1f, true);
            bossDeathPhase = BossDeathPhase::DeathFall;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::DeathFall:
    {
        const auto controller = gruxEnemyActor
            ? gruxEnemyActor->GetBodyAnimationController()
            : nullptr;
        if (controller &&
            controller->GetCurrentAnimationName() == "Result_Down_Start" &&
            !controller->IsPlayAnimation())
        {
            controller->ResetAnimationRate();
            BlendToLanding(
                dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()),
                *context.shader, bossDeathFallToLandingBlendDuration);

            const auto& landingBossPose = GetLandingBossPose();
            DirectX::XMFLOAT3 deathFwdPosition = landingBossPose.position;
            deathFwdPosition.x += bossDeathFwdPositionOffset.x;
            deathFwdPosition.y += bossDeathFwdPositionOffset.y;
            deathFwdPosition.z += bossDeathFwdPositionOffset.z;
            gruxEnemyActor->SetPosition(deathFwdPosition);
            gruxEnemyActor->SetQuaternionRotation(landingBossPose.rotation);
            gruxEnemyActor->UpdateAllComponentTransforms();

            controller->SetAnimationRate(bossDeathFwdPlaybackRate);
            gruxEnemyActor->PlayBodyAnimation(
                "Death_B_0", false, true, bossDeathStunToDeathBlend, true);
            if (!controller->SetPlaybackRange(
                bossDeathFwdStartTime, bossDeathFwdEndTime))
            {
                Logger::Error(Logger::LogCategory::System,
                    "Failed to set Death_Fwd playback range for boss death preview");
            }
            bossDeathPhase = BossDeathPhase::DeathLanding;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::DeathLanding:
    {
        const auto controller = gruxEnemyActor
            ? gruxEnemyActor->GetBodyAnimationController()
            : nullptr;
        if (controller &&
            controller->GetCurrentAnimationName() == "Death_B_0" &&
            controller->GetCurrentAnimationTime() >= bossDeathFwdEndTime)
        {
            controller->ResetAnimationRate();
            // Own the endpoint pose first; walking never restarts the boss clip.
            bossDeathFwdPromptTime = controller->GetCurrentAnimationTime();
            bossDeathFwdPromptDirection =
                bossDeathFwdPromptTime >= bossDeathFwdPromptMaxTime ? -1.0f : 1.0f;
            if (!controller->HoldAnimationPose("Death_B_0", bossDeathFwdPromptTime))
            {
                break;
            }
            bgmDirector.PlayBossDeathSecondBgm();
            StartBossDeathGroanLoop();
            player->SetPosition(bossDeathApproachStartPosition);
            player->UpdateAllComponentTransforms();
            bossDeathApproachStartRotation = player->GetQuaternionRotation();
            player->PlayBodyAnimation("Walk_Fwd", true, true, 0.2f, true);
            bossDeathPhase = BossDeathPhase::PlayerApproach;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::PlayerApproach:
    {
        UpdateBossDeathPromptLoop(gruxEnemyActor, deltaTime);
        const float duration = (std::max)(
            bossDeathPlayerApproachDuration, FLT_EPSILON);
        const float t = std::clamp(bossDeathPhaseElapsed / duration, 0.0f, 1.0f);
        float smoothT = t * t * (3.0f - 2.0f * t);
        const auto& finishPose = GetFinishPlayerPose();
        DirectX::XMFLOAT3 adjustedFinishPosition = finishPose.position;
        adjustedFinishPosition.x += bossDeathFinishPlayerPositionOffset.x;
        adjustedFinishPosition.y += bossDeathFinishPlayerPositionOffset.y;
        adjustedFinishPosition.z += bossDeathFinishPlayerPositionOffset.z;

        const float approachDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(
            DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&adjustedFinishPosition),
                DirectX::XMLoadFloat3(&bossDeathApproachStartPosition))));
        const float stopStartT = approachDistance > FLT_EPSILON
            ? 1.0f - (std::min)(bossDeathWalkStopStartDistance / approachDistance, 1.0f)
            : 0.0f;
        // Clamp at the threshold even when a long frame crosses it.
        const bool startStop = smoothT >= stopStartT;
        smoothT = (std::min)(smoothT, stopStartT);
        DirectX::XMFLOAT3 position{};
        position.x = std::lerp(
            bossDeathApproachStartPosition.x, adjustedFinishPosition.x, smoothT);
        position.y = std::lerp(
            bossDeathApproachStartPosition.y, adjustedFinishPosition.y, smoothT);
        position.z = std::lerp(
            bossDeathApproachStartPosition.z, adjustedFinishPosition.z, smoothT);
        DirectX::XMFLOAT4 rotation{};
        DirectX::XMStoreFloat4(&rotation, DirectX::XMQuaternionNormalize(
            DirectX::XMQuaternionSlerp(
                DirectX::XMLoadFloat4(&bossDeathApproachStartRotation),
                DirectX::XMLoadFloat4(&finishPose.rotation), smoothT)));

        player->SetPosition(position);
        player->SetQuaternionRotation(rotation);
        player->UpdateAllComponentTransforms();

        if (startStop)
        {
            bossDeathWalkStopStartPosition = player->GetPosition();
            bossDeathWalkStopFinishPosition = adjustedFinishPosition;
            bossDeathWalkStopStartRotation = player->GetQuaternionRotation();
            bossDeathWalkStopFinishRotation = finishPose.rotation;
            if (const auto controller = player->GetBodyAnimationController())
                controller->ResetAnimationRate();
            // Player root motion is disabled; only this scene moves the actor.
            player->PlayBodyAnimation("Walk_Fwd_Stop_Fast", false, true, 0.1f, true);
            bossDeathPhase = BossDeathPhase::PlayerWalkStop;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::PlayerWalkStop:
    {
        UpdateBossDeathPromptLoop(gruxEnemyActor, deltaTime);
        const auto controller = player->GetBodyAnimationController();
        if (!controller || controller->GetCurrentAnimationName() != "Walk_Fwd_Stop_Fast")
            break;

        const float duration = (std::max)(controller->GetAnimationLength("Walk_Fwd_Stop_Fast"), FLT_EPSILON);
        const float t = std::clamp(controller->GetCurrentAnimationTime() / duration, 0.0f, 1.0f);
        const float easeOut = 1.0f - (1.0f - t) * (1.0f - t);
        DirectX::XMFLOAT3 position{};
        DirectX::XMStoreFloat3(&position, DirectX::XMVectorLerp(
            DirectX::XMLoadFloat3(&bossDeathWalkStopStartPosition),
            DirectX::XMLoadFloat3(&bossDeathWalkStopFinishPosition), easeOut));
        DirectX::XMFLOAT4 rotation{};
        DirectX::XMStoreFloat4(&rotation, DirectX::XMQuaternionNormalize(DirectX::XMQuaternionSlerp(
            DirectX::XMLoadFloat4(&bossDeathWalkStopStartRotation),
            DirectX::XMLoadFloat4(&bossDeathWalkStopFinishRotation), easeOut)));
        player->SetPosition(position);
        player->SetQuaternionRotation(rotation);
        player->UpdateAllComponentTransforms();

        if (!controller->IsPlayAnimation())
        {
            player->SetPosition(bossDeathWalkStopFinishPosition);
            player->SetQuaternionRotation(bossDeathWalkStopFinishRotation);
            player->UpdateAllComponentTransforms();
            CutToShot(Shot::Finish,
                cinemaCameraActor ? dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()) : nullptr,
                *context.shader);
            player->PlayBodyAnimation("Result_Recall", false, true, 0.15f, true);
            if (const auto controller = player->GetBodyAnimationController();
                !controller || !controller->SetPlaybackRange(
                    0.0f, bossDeathRecallPromptMinTime))
            {
                Logger::Error(Logger::LogCategory::System,
                    "Failed to set Result_Recall playback range for boss death preview");
            }
            bossDeathPhase = BossDeathPhase::RecallLeadIn;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::RecallLeadIn:
    {
        UpdateBossDeathPromptLoop(gruxEnemyActor, deltaTime);
        const auto controller = player
            ? player->GetBodyAnimationController()
            : nullptr;
        if (controller &&
            controller->GetCurrentAnimationName() == "Result_Recall" &&
            !controller->IsPlayAnimation())
        {
            // Recall's runtime ShowTrail state starts at 0.24 sec but its end at
            // 1.4 sec is intentionally never reached. Clear only transient
            // battle presentation before pose-only sampling begins.
            player->ClearTransientBattleActions();
            bossDeathRecallPromptTime = bossDeathRecallPromptMinTime;
            bossDeathRecallPromptDirection = 1.0f;
            if (controller->HoldAnimationPose(
                "Result_Recall", bossDeathRecallPromptTime))
            {
                bossDeathPhase = BossDeathPhase::RecallPingPong;
                bossDeathPhaseElapsed = 0.0f;
                bossDeathFinishInputEnabled = true;
                bossDeathUIActor->SetFinishGuideVisible(true);
            }
        }
        break;
    }

    case BossDeathPhase::RecallPingPong:
    {
        UpdateBossDeathPromptLoop(gruxEnemyActor, deltaTime);
        bossDeathUIActor->UpdatePresentation();
        bossDeathRecallPromptTime +=
            bossDeathRecallPromptDirection *
            bossDeathRecallPromptPlaybackRate * deltaTime;

        // Reflect overshoot at both endpoints. The loop also handles a large
        // debug-frame delta crossing the very short range more than once.
        while (bossDeathRecallPromptTime > bossDeathRecallPromptMaxTime ||
            bossDeathRecallPromptTime < bossDeathRecallPromptMinTime)
        {
            if (bossDeathRecallPromptTime > bossDeathRecallPromptMaxTime)
            {
                bossDeathRecallPromptTime =
                    bossDeathRecallPromptMaxTime -
                    (bossDeathRecallPromptTime - bossDeathRecallPromptMaxTime);
                bossDeathRecallPromptDirection = -1.0f;
            }
            else
            {
                bossDeathRecallPromptTime =
                    bossDeathRecallPromptMinTime +
                    (bossDeathRecallPromptMinTime - bossDeathRecallPromptTime);
                bossDeathRecallPromptDirection = 1.0f;
            }
        }

        if (const auto controller = player->GetBodyAnimationController())
        {
            controller->HoldAnimationPose(
                "Result_Recall", bossDeathRecallPromptTime);
        }

        const bool finishTriggered = bossDeathFinishInputEnabled &&
            InputSystem::GetInputState(
                "Attack", InputStateMask::Trigger, DeviceFlags::GamePadOnly);
        if (finishTriggered)
        {
            bossDeathFinishInputEnabled = false;
            bossDeathUIActor->SetFinishGuideVisible(false);
            if (const auto controller = player->GetBodyAnimationController())
            {
                const float resumeTime = bossDeathRecallPromptTime;
                controller->ReleaseHeldAnimationPose();
                player->PlayBodyAnimation(
                    "Result_Recall", false, false, 0.0f, true);
                if (!controller->SetPlaybackRange(
                    resumeTime, bossDeathRecallFinishHitTime))
                {
                    Logger::Error(Logger::LogCategory::System,
                        "Failed to resume Result_Recall for boss finish preview");
                }
            }
            player->ClearTransientBattleActions();
            bossDeathPhase = BossDeathPhase::FinishTriggered;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::FinishTriggered:
    {
        UpdateBossDeathPromptLoop(gruxEnemyActor, deltaTime);
        const auto controller = player
            ? player->GetBodyAnimationController()
            : nullptr;
        if (controller &&
            controller->GetCurrentAnimationName() == "Result_Recall" &&
            controller->GetCurrentAnimationTime() >= bossDeathRecallFinishHitTime)
        {
            StopBossDeathGroanLoop();
            // Keep pose ownership and enter the narrower interval continuously.
            bossDeathScreamHoldTime = bossDeathFwdPromptTime;
            bossDeathScreamHoldDirection = bossDeathFwdPromptDirection;
            // É{ÉXéÄñSífñññÇ
            if (bossDeathVoiceAudio)
                bossDeathVoiceAudio->Stop(false);
            bossDeathVoiceAudio = CoreAudio::PlayOneShot("./Data/Sound/SE/boss_death_voice1.wav", 3.0f);
            // ÉRÉìÉgÉçÅ[ÉâÅ[êUìÆ
            InputSystem::SetVibration(0.9f, 0.2f);

            bossDeathFinishHoldTime = bossDeathFinishHoldMinTime;
            bossDeathFinishHoldDirection = 1.0f;
            controller->ResetAnimationRate();
            controller->HoldAnimationPose("Result_Recall", bossDeathFinishHoldTime);
            bossDeathPhase = BossDeathPhase::HuskDelay;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;
    }

    case BossDeathPhase::HuskDelay:
        if (bossDeathPhaseElapsed >= bossDeathHuskDelay)
        {
            if (gruxEnemyActor)
                gruxEnemyActor->RequestBeginHuskParticle();
            bossDeathPhase = BossDeathPhase::HuskPreview;
            bossDeathPhaseElapsed = 0.0f;
        }
        break;

    case BossDeathPhase::HuskPreview:
        if (huskComplete)
            return true;
        break;
    }
    return false;
}

#ifdef USE_IMGUI
void GameBossDeathController::DrawDebugGui(Player* player, GruxEnemy* gruxEnemyActor)
{
    ImGui::DragFloat(U8("à√ì]éûä‘"), &bossDeathFadeOutDuration, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat(U8("à√ì]âèúéûä‘"), &bossDeathFadeInDuration, 0.01f, 0.01f, 5.0f);

    ImGui::SeparatorText("Scream");
    ImGui::DragFloat("Ultimate Roar Start Time",
        &bossDeathRoarStartTime, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Ultimate Roar End Time",
        &bossDeathRoarEndTime, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Ultimate Roar Playback Rate",
        &bossDeathRoarPlaybackRate, 0.01f, 0.1f, 2.0f);

    ImGui::SeparatorText("Fall / Landing");
    ImGui::DragFloat("Stun Playback Rate",
        &bossDeathStunPlaybackRate, 0.01f, 0.1f, 2.0f);
    ImGui::DragFloat("Boss Death Stun To Death Blend",
        &bossDeathStunToDeathBlend, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Boss Death Scream Hold Min",
        &bossDeathScreamHoldMin, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Boss Death Scream Hold Max",
        &bossDeathScreamHoldMax, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Boss Death Scream Hold Rate",
        &bossDeathScreamHoldRate, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Fall To Landing Blend Duration",
        &bossDeathFallToLandingBlendDuration, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Death Fwd Start Time",
        &bossDeathFwdStartTime, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Death Fwd End Time",
        &bossDeathFwdEndTime, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Death Fwd Playback Rate",
        &bossDeathFwdPlaybackRate, 0.01f, 0.1f, 2.0f);
    ImGui::DragFloat("Death Fwd Prompt Min Time",
        &bossDeathFwdPromptMinTime, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Death Fwd Prompt Max Time",
        &bossDeathFwdPromptMaxTime, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Death Fwd Prompt Playback Rate",
        &bossDeathFwdPromptPlaybackRate, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat3("Death Fwd Boss Position Offset",
        &bossDeathFwdPositionOffset.x, 0.01f);

    ImGui::SeparatorText("Player Approach");
    ImGui::DragFloat3("Player Approach Start Position",
        &bossDeathApproachStartPosition.x, 0.01f);
    ImGui::DragFloat("Player Approach Duration",
        &bossDeathPlayerApproachDuration, 0.01f, 0.0f, 10.0f);

    ImGui::DragFloat("Boss Death Walk Stop Start Distance",
        &bossDeathWalkStopStartDistance, 0.01f, 0.0f, 2.0f, "%.2f m");

    ImGui::SeparatorText("Finish");
    ImGui::DragFloat3("Finish Player Position Offset",
        &bossDeathFinishPlayerPositionOffset.x, 0.01f);

    ImGui::DragFloat("Husk Start Delay",
        &bossDeathHuskDelay, 0.01f, 0.0f, 5.0f);

    ImGui::SeparatorText("Recall");
    ImGui::DragFloat("Recall Prompt Min Time",
        &bossDeathRecallPromptMinTime, 0.001f, 0.0f, RecallFirstSeTime);
    ImGui::DragFloat("Recall Prompt Max Time",
        &bossDeathRecallPromptMaxTime, 0.001f, 0.0f, RecallFirstSeTime);
    ImGui::DragFloat("Recall Prompt Playback Rate",
        &bossDeathRecallPromptPlaybackRate, 0.01f, 0.0f, 3.0f);
    ClampBossDeathPreviewTuning(player, gruxEnemyActor);
    ImGui::DragFloat("Finish Hold Min Time",
        &bossDeathFinishHoldMinTime, 0.001f);
    ImGui::DragFloat("Finish Hold Max Time",
        &bossDeathFinishHoldMaxTime, 0.001f);
    ImGui::DragFloat("Finish Hold Playback Rate",
        &bossDeathFinishHoldPlaybackRate, 0.01f, 0.0f, 3.0f);
    ClampBossDeathPreviewTuning(player, gruxEnemyActor);
    ImGui::Text("Finish Hold Time: %.3f", bossDeathFinishHoldTime);
    ImGui::Text("Recall Prompt Time: %.3f", bossDeathRecallPromptTime);
}
#endif
