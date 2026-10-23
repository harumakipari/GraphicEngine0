#include "pch.h"
#include "GamePhase2TransitionController.h"
#include "GameBgmDirector.h"
#include "Engine/Utility/Time.h"
#include "Engine/Debug/SceneEditor.h"
#include "Game/Actors/Camera/Camera.h"
#include "Game/Actors/Player/Player.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"


void GamePhase2TransitionController::LoadShots()
{
    phase2CinematicShotsLoaded = LoadShotsInternal();
}

bool GamePhase2TransitionController::LoadShotsInternal()
{
    try
    {
        for (size_t shotIndex = 0; shotIndex < phase2CinematicPresetPaths.size(); ++shotIndex)
        {
            SceneState sceneState{};
            SceneEditor::LoadSceneState(phase2CinematicPresetPaths[shotIndex], sceneState);
            const auto findActor = [&sceneState](const char* actorName)
                {
                    return std::find_if(sceneState.actorStates.begin(), sceneState.actorStates.end(),
                        [actorName](const ActorTransformState& actor) { return actor.name == actorName; });
                };
            const auto playerState = findActor("Player");
            const auto bossState = findActor("GruxEnemy");
            if (playerState == sceneState.actorStates.end() || bossState == sceneState.actorStates.end())
                return false;

            auto& shot = phase2CinematicShots[shotIndex];
            shot.camera = { sceneState.camera.position, sceneState.camera.rotation, sceneState.camera.fov };
            shot.player = { playerState->position, playerState->rotation };
            shot.boss = { bossState->position, bossState->rotation };
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        Logger::Error(Logger::LogCategory::System,
            std::format("Failed to load Phase2 cinematic presets: {}", exception.what()).c_str());
        return false;
    }
}

void GamePhase2TransitionController::ApplyRecallActorPose(const Player* player, const GruxEnemy* gruxEnemyActor)
{
    if (!player || !gruxEnemyActor || !phase2CinematicShotsLoaded)
        return;

    const auto& shot = phase2CinematicShots[0];
    gruxEnemyActor->SetPosition(shot.boss.position);
    gruxEnemyActor->SetQuaternionRotation(shot.boss.rotation);
    player->SetPosition(shot.player.position);
    player->SetQuaternionRotation(shot.player.rotation);
    gruxEnemyActor->UpdateAllComponentTransforms();
    player->UpdateAllComponentTransforms();
}

void GamePhase2TransitionController::CutToShot(const size_t shotIndex, const CinemaCamera* cinemaCameraActor) const
{
    if (shotIndex >= phase2CinematicShots.size() || !cinemaCameraActor)
        return;
    if (auto* camera = dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()))
        camera->CutToPose(phase2CinematicShots[shotIndex].camera);
}


bool GamePhase2TransitionController::UpdateRecall(
    GruxEnemy* gruxEnemyActor, GameBgmDirector& bgmDirector, const float bossBgmVolume)
{
        const auto controller = gruxEnemyActor ? gruxEnemyActor->GetBodyAnimationController() : nullptr;
        if (controller)
            controller->OnUpdate(Time::UnscaledDeltaTime());

        const bool isRecallAnimation = controller &&
            controller->GetCurrentAnimationName() == "PrimaryAttack_Recall";
        if (isRecallAnimation && !phase2TransformCompleted)
        {
            const float recallAnimationTime = controller->GetCurrentAnimationTime();
            const float transformDuration = phase2TransformEndTime - phase2TransformStartTime;
            if (transformDuration <= 0.001f)
            {
                if (recallAnimationTime >= phase2TransformStartTime)
                {
                    phase2TransformProgress = 1.0f;
                    phase2TransformCompleted = true;
                }
                else
                {
                    phase2TransformProgress = 0.0f;
                }
            }
            else
            {
                phase2TransformProgress = std::clamp(
                    (recallAnimationTime - phase2TransformStartTime) / transformDuration,
                    0.0f, 1.0f);
                if (recallAnimationTime >= phase2TransformEndTime)
                {
                    phase2TransformProgress = 1.0f;
                    phase2TransformCompleted = true;
                }
            }
        }
        else if (isRecallAnimation)
        {
            phase2TransformProgress = 1.0f;
        }

        if (isRecallAnimation && gruxEnemyActor)
        {
            gruxEnemyActor->SetPhaseTransformProgress(phase2TransformProgress);
        }
        if (isRecallAnimation && phase2TransformCompleted && !bgmDirector.IsPhase2Started())
        {
            bgmDirector.BeginPhase2CrossFade(controller->GetCurrentAnimationTime(), bossBgmVolume);
        }

        const bool recallFinished = !controller ||
            (isRecallAnimation && !controller->IsPlayAnimation());
        if (!recallFinished)
            return false;

        phase2TransformProgress = 1.0f;
        phase2TransformCompleted = true;
        if (!bgmDirector.IsPhase2Started())
            bgmDirector.BeginPhase2CrossFade(controller ? controller->GetCurrentAnimationTime() : phase2TransformEndTime, bossBgmVolume);
        if (gruxEnemyActor)
            gruxEnemyActor->SetPhaseTransformProgress(phase2TransformProgress);
        return true;
}

void GamePhase2TransitionController::ResetTransform(const bool phase2)
{
    phase2TransformProgress = phase2 ? 1.0f : 0.0f;
    phase2TransformCompleted = phase2;
}

void GamePhase2TransitionController::ApplyTransform(GruxEnemy* gruxEnemyActor) const
{
    if (gruxEnemyActor)
        gruxEnemyActor->SetPhaseTransformProgress(phase2TransformProgress);
}

#ifdef USE_IMGUI
void GamePhase2TransitionController::DrawRecallDebugGui(GruxEnemy* gruxEnemyActor)
{
    ImGui::DragFloat(U8("Phase2 Transform Start Time"), &phase2TransformStartTime,
        0.01f, 0.0f, 15.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(U8("Phase2 Transform End Time"), &phase2TransformEndTime,
        0.01f, 0.0f, 15.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::DragFloat(U8("Phase2 Transform Progress"), &phase2TransformProgress,
        0.01f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp) && gruxEnemyActor)
    {
        gruxEnemyActor->SetPhaseTransformProgress(phase2TransformProgress);
    }
}
#endif

void GamePhase2TransitionController::ClearPlayerEmote()
{
    phase2PlayerEmoteActive = false;
    phase2PlayerEmoteEndPoseHeld = false;
}

void GamePhase2TransitionController::ResetPlayerEmote(const Player* player)
{
    if ((phase2PlayerEmoteActive || phase2PlayerEmoteEndPoseHeld) && player)
        player->PlayBodyAnimation("Idle", true, true, 0.0f, true);
    ClearPlayerEmote();
}

void GamePhase2TransitionController::BeginPlayerEmote(const Player* player)
{
    phase2PlayerEmoteActive = false;
    phase2PlayerEmoteEndPoseHeld = false;
    if (const auto controller = player ? player->GetBodyAnimationController() : nullptr)
    {
        player->PlayBodyAnimation("Emote_Phase2", false, true, 0.2f, true);
        phase2PlayerEmoteActive = controller->SetPlaybackRange(
            0.0f, std::clamp(phase2PlayerEmoteEndTime, 0.0f,
                controller->GetAnimationLength("Emote_Phase2")));
    }
}

bool GamePhase2TransitionController::UpdatePlayerEmote(const Player* player)
{
    const auto controller = player ? player->GetBodyAnimationController() : nullptr;
    if (controller)
        controller->OnUpdate(Time::UnscaledDeltaTime());

    const bool emoteFinished = !controller || !phase2PlayerEmoteActive ||
        (controller->GetCurrentAnimationName() == "Emote_Phase2" && !controller->IsPlayAnimation());
    if (!emoteFinished)
        return false;

    phase2PlayerEmoteActive = false;
    phase2PlayerEmoteEndPoseHeld = controller &&
        controller->GetCurrentAnimationName() == "Emote_Phase2";
    return true;
}

void GamePhase2TransitionController::FinishPlayerEmote(const Player* player)
{
    if (player)
        player->PlayBodyAnimation("Idle", true, true, 0.2f, true);
    phase2PlayerEmoteEndPoseHeld = false;
}

#ifdef USE_IMGUI
void GamePhase2TransitionController::DrawPlayerEmoteDebugGui()
{
    ImGui::DragFloat(U8("Player Emote EndTime"), &phase2PlayerEmoteEndTime,
        0.001f, 0.001f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
}
#endif
