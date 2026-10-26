#include "pch.h"
#include "GameVictoryController.h"
#include "Game/Actors/UI/GameVictoryUIActor.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Actors/Camera/Camera.h"
#include "Engine/Utility/Time.h"
#include "Engine/Audio/Audio.h"
#include <json.hpp>
#include <fstream>
#include <filesystem>

void GameVictoryController::ResetPresentation()
{
    victoryResultPhase = VictoryResultPhase::None;
    victoryResultDelayElapsed = 0.0f;
}

void GameVictoryController::PreparePlayer(Player* player)
{
    if (const auto controller = player ? player->GetBodyAnimationController() : nullptr)
    {
        controller->ReleaseHeldAnimationPose(true);
        controller->ResetAnimationRate();
        const float idleBlend = std::isfinite(resultRecallToIdleBlendDuration)
            ? std::clamp(resultRecallToIdleBlendDuration, 0.0f, 0.5f) : 0.15f;
        player->PlayBodyAnimation("Idle", true, idleBlend > 0.0f, idleBlend, true);
    }
}

void GameVictoryController::BeginPresentation()
{
    victoryResultDelayElapsed = 0.0f;
    victoryResultPhase = VictoryResultPhase::CameraBlend;
}

bool GameVictoryController::CanAcceptSelection(const GameVictoryUIActor& victoryUIActor) const
{
    return victoryUIActor.IsInputEnabled() && victoryResultPhase == VictoryResultPhase::Interactive;
}

bool GameVictoryController::AcceptSelection(int selection, bool restartAllowed, GameVictoryUIActor& victoryUIActor, float confirmVolume)
{
    if (selection == 0 && !restartAllowed) return false;
    victoryUIActor.DisableInput();
    CoreAudio::PlayOneShot("./Data/Sound/SE/button_push.wav", confirmVolume);
    return true;
}

int GameVictoryController::UpdateUntilSelection(Player* player, CinemaCamera* cinemaCameraActor,
    GameVictoryUIActor& ui, float deathResultMoveSeVolume)
{
    auto* victoryUIActor = &ui;
    const auto controller = player ? player->GetBodyAnimationController() : nullptr;
    auto* camera = cinemaCameraActor
        ? dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()) : nullptr;
    switch (victoryResultPhase)
    {
    case VictoryResultPhase::CameraBlend:
        if (camera && !camera->IsBlending() && controller)
        {
            controller->ResetAnimationRate();
            player->PlayBodyAnimation("Emote_Win", false, true, 0.2f, true);
            controller->SetPlaybackRange(0.0f, victoryEmoteEndTime);
            victoryResultPhase = VictoryResultPhase::WinEmote;
        }
        break;
    case VictoryResultPhase::WinEmote:
        if (!victoryUIActor->IsBackgroundStarted() && controller &&
            controller->GetCurrentAnimationName() == "Emote_Win")
        {
            const float triggerTime = std::isfinite(resultUITriggerAnimationTime)
                ? std::clamp(resultUITriggerAnimationTime, 0.0f, victoryEmoteEndTime) : 1.17f;
            if (controller->GetCurrentAnimationTime() >= triggerTime)
            {
                victoryUIActor->StartBackgroundFade();
            }
        }
        if (controller && controller->GetCurrentAnimationName() == "Emote_Win" &&
            controller->GetCurrentAnimationTime() >= victoryEmoteEndTime)
        {
            player->PlayBodyAnimation("Idle", true, true, 0.2f, true);
            victoryResultDelayElapsed = 0.0f;
            victoryResultPhase = VictoryResultPhase::ResultDelay;
        }
        break;
    case VictoryResultPhase::ResultDelay:
    {
        const float duration = std::isfinite(victoryResultDelayDuration)
            ? std::clamp(victoryResultDelayDuration, 0.0f, 2.0f) : 0.70f;
        victoryResultDelayElapsed = (std::min)(duration,
            victoryResultDelayElapsed + (std::max)(0.0f, Time::UnscaledDeltaTime()));
        if (victoryResultDelayElapsed >= duration)
        {
            victoryUIActor->ShowButtons();
            victoryResultPhase = VictoryResultPhase::ShowButtons;
        }
        break;
    }
    case VictoryResultPhase::ShowButtons:
    {
        if (victoryUIActor->AdvanceButtonFade())
        {
            victoryResultPhase = VictoryResultPhase::Interactive;
            victoryUIActor->EnableInput();
        }
        break;
    }
    case VictoryResultPhase::Interactive:
        return victoryUIActor->UpdateInput(deathResultMoveSeVolume);
    default:
        break;
    }
    return -1;
}

void GameVictoryController::FinishUpdate(GameVictoryUIActor& victoryUIActor, float numberTexWidth, float numberTexHeight)
{
    victoryUIActor.SetNumberTextureSize(numberTexWidth, numberTexHeight);
    victoryUIActor.UpdateBackground();
    victoryUIActor.UpdateButtonLayout();
}

void GameVictoryController::LoadBestTime()
{
    const char* path = "Data/Saves/BossBestTime.json";
    try
    {
        if (!std::filesystem::exists(path)) return;
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Cannot open boss best time");
        nlohmann::json data;
        input >> data;
        const double value = data.at("bossBestTimeSeconds").get<double>();
        if (!std::isfinite(value) || value < 0.0)
            throw std::runtime_error("Invalid boss best time");
        victoryBestTime = value;
        victoryBestTimeValid = true;
    }
    catch (const std::exception& error)
    {
        Logger::Warning(Logger::LogCategory::System,
            (std::string("Boss best time load failed: ") + error.what()).c_str());
    }
}

void GameVictoryController::CaptureResult(float finalBattleTime, bool finalBattleTimeSaved, GameVictoryUIActor& victoryUIActor)
{
    const bool validClear = finalBattleTimeSaved && std::isfinite(finalBattleTime) && finalBattleTime >= 0.0f;
    const double victoryClearTime = validClear ? static_cast<double>(finalBattleTime) : 0.0;
    const bool victoryIsNewRecord = validClear && (!victoryBestTimeValid || victoryClearTime < victoryBestTime);
    if (victoryIsNewRecord)
    {
        victoryBestTime = victoryClearTime;
        victoryBestTimeValid = true;
        try
        {
            std::filesystem::create_directories("Data/Saves");
            const char* temporaryPath = "Data/Saves/BossBestTime.json.tmp";
            {
                std::ofstream output(temporaryPath, std::ios::trunc);
                output.exceptions(std::ios::badbit | std::ios::failbit);
                output << nlohmann::json{ { "bossBestTimeSeconds", victoryBestTime } }.dump(4);
                output.close();
            }
            if (!MoveFileExA(temporaryPath, "Data/Saves/BossBestTime.json",
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Cannot replace boss best time save");
        }
        catch (const std::exception& error)
        {
            Logger::Warning(Logger::LogCategory::System,
                (std::string("Boss best time save failed: ") + error.what()).c_str());
        }
    }
    const double victoryDisplayedBestTime = victoryBestTimeValid ? victoryBestTime : 0.0;
    const float s = std::isfinite(victoryRankSLimit) ? (std::max)(0.0f, victoryRankSLimit) : 60.0f;
    const float a = std::isfinite(victoryRankALimit) ? (std::max)(s, victoryRankALimit) : (std::max)(s, 90.0f);
    const float b = std::isfinite(victoryRankBLimit) ? (std::max)(a, victoryRankBLimit) : (std::max)(a, 120.0f);
    const GameVictoryUIActor::VictoryRank victoryRank = victoryClearTime <= s ? GameVictoryUIActor::VictoryRank::S : victoryClearTime <= a ? GameVictoryUIActor::VictoryRank::A : victoryClearTime <= b ? GameVictoryUIActor::VictoryRank::B : GameVictoryUIActor::VictoryRank::C;
    victoryUIActor.SetResultData(victoryClearTime, victoryDisplayedBestTime, victoryIsNewRecord, victoryRank);
}

#ifdef USE_IMGUI
void GameVictoryController::DrawCameraIntroDebugGui()
{
    ImGui::DragFloat(U8("リザルト表示までのカメラブレンド"), &victoryCameraBlendDuration,0.01f, 0.0f, 2.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Result Recall To Idle Blend Duration", &resultRecallToIdleBlendDuration,
        0.01f, 0.0f, 0.5f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
}

void GameVictoryController::DrawAnimationDebugGui()
{
    ImGui::DragFloat("Result UI Trigger Animation Time", &resultUITriggerAnimationTime,
        0.01f, 0.0f, victoryEmoteEndTime, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
}

void GameVictoryController::DrawDelayDebugGui()
{
    ImGui::DragFloat("Result Delay", &victoryResultDelayDuration,
        0.01f, 0.0f, 2.0f, "%.3f s", ImGuiSliderFlags_AlwaysClamp);
}

void GameVictoryController::DrawRankDebugGui()
{
    ImGui::DragFloat("Victory S Limit", &victoryRankSLimit, 1.0f, 0.0f, 5999.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Victory A Limit", &victoryRankALimit, 1.0f, 0.0f, 5999.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Victory B Limit", &victoryRankBLimit, 1.0f, 0.0f, 5999.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    victoryRankALimit = (std::max)(victoryRankSLimit, victoryRankALimit);
    victoryRankBLimit = (std::max)(victoryRankALimit, victoryRankBLimit);
}
#endif
