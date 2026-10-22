#include "pch.h"
#include "GameBgmDirector.h"
#include "Game/Actors/BgmActor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void GameBgmDirector::SetActors(const std::shared_ptr<BgmActor>& game, const std::shared_ptr<BgmActor>& boss,
    const std::shared_ptr<BgmActor>& phase2, const std::shared_ptr<BgmActor>& bossDeath,
    const std::shared_ptr<BgmActor>& playerDeath)
{
    gameBgmActor = game;
    bossBgmActor = boss;
    phase2BgmActor = phase2;
    bossDeathSecondBgmActor = bossDeath;
    playerDeathBgmActor = playerDeath;
}

void GameBgmDirector::SetGameActor(const std::shared_ptr<BgmActor>& game)
{
    gameBgmActor = game;
}

void GameBgmDirector::ClearActors()
{
    gameBgmActor.reset();
    bossBgmActor.reset();
    phase2BgmActor.reset();
    bossDeathSecondBgmActor.reset();
    playerDeathBgmActor.reset();
}

void GameBgmDirector::BeginGameFade()
{
    const auto game = gameBgmActor.lock();
    if (gameBgmFading || !game) return;
    gameBgmFadeStartVolume = game->GetVolume();
    gameBgmFadeElapsed = 0.0f;
    gameBgmFading = true;
}

void GameBgmDirector::BeginBossDeathFade()
{
    const auto activeBgm = GetActiveBossBgmActor();
    if (bossBgmFading || !activeBgm) return;
    bossBgmFadeStartVolume = activeBgm->GetVolume();
    bossBgmFadeElapsed = 0.0f;
    bossBgmFading = true;
}

void GameBgmDirector::BeginPlayerDeathFade()
{
    const auto activeBgm = GetActiveBossBgmActor();
    if (playerBgmFading || !activeBgm) return;
    phase2BgmCrossFadeActive = false;
    bossBgmFadeStartVolume = activeBgm->GetVolume();
    bossBgmFadeElapsed = 0.0f;
    playerBgmFading = true;
}

void GameBgmDirector::BeginPhase2CrossFade(float, float bossBgmVolume)
{
    if (phase2BgmStarted) return;
    phase2BgmStarted = true;
    const auto boss = bossBgmActor.lock();
    const auto phase2 = phase2BgmActor.lock();
    phase2BgmFadeElapsed = 0.0f;
    phase2BgmPhase1StartVolume = boss ? boss->GetVolume() : 0.0f;
    phase2BgmCrossFadeActive = boss && phase2;
    if (!phase2) return;
    phase2->Stop(false);
    phase2->SetVolume(0.0f);
    phase2->Play();
    if (!phase2BgmCrossFadeActive) phase2->SetVolume(bossBgmVolume);
}

void GameBgmDirector::ResetPhase2CrossFade()
{
    phase2BgmCrossFadeActive = false;
    phase2BgmFadeElapsed = 0.0f;
}

void GameBgmDirector::ResetDeathFade()
{
    bossBgmFading = false;
    playerBgmFading = false;
    bossBgmFadeElapsed = 0.0f;
}

void GameBgmDirector::UpdateGameBgmFadeOut(const float deltaTime)
{
    const auto game = gameBgmActor.lock();
    if (!gameBgmFading || !game) return;
    gameBgmFadeElapsed += (std::max)(0.0f, deltaTime);
    const float duration = (std::max)(gameBgmFadeDuration, 0.001f);
    const float t = std::clamp(gameBgmFadeElapsed / duration, 0.0f, 1.0f);
    game->SetVolume(std::lerp(gameBgmFadeStartVolume, 0.0f, t));
    if (t >= 1.0f)
    {
        game->Stop(false);
        gameBgmFading = false;
    }
}

void GameBgmDirector::UpdateDeathBgmFade(const float deltaTime)
{
    const auto activeBgm = GetActiveBossBgmActor();
    if ((!bossBgmFading && !playerBgmFading) || !activeBgm) return;
    const float duration = playerBgmFading ? playerDeathBgmFadeTime : bossDeathBgmFadeTime;
    bossBgmFadeElapsed += deltaTime;
    const float t = duration > 0.0f ? std::clamp(bossBgmFadeElapsed / duration, 0.0f, 1.0f) : 1.0f;
    activeBgm->SetVolume(std::lerp(bossBgmFadeStartVolume, 0.0f, t));
    if (t >= 1.0f)
    {
        activeBgm->Stop(false);
        bossBgmFading = false;
        playerBgmFading = false;
    }
}

void GameBgmDirector::UpdatePhase2BgmCrossFade(const float deltaTime, const float bossBgmVolume)
{
    const auto boss = bossBgmActor.lock();
    const auto phase2 = phase2BgmActor.lock();
    if (!phase2BgmCrossFadeActive || !boss || !phase2) return;
    phase2BgmFadeElapsed += (std::max)(0.0f, deltaTime);
    const float fadeOut = (std::max)(phase2BgmFadeOutDuration, 0.001f);
    const float fadeIn = (std::max)(phase2BgmFadeInDuration, 0.001f);
    const float outT = std::clamp(phase2BgmFadeElapsed / fadeOut, 0.0f, 1.0f);
    const float inT = std::clamp(phase2BgmFadeElapsed / fadeIn, 0.0f, 1.0f);
    boss->SetVolume(std::lerp(phase2BgmPhase1StartVolume, 0.0f, outT));
    phase2->SetVolume(std::lerp(0.0f, bossBgmVolume, inT));
    if (outT >= 1.0f)
        boss->Stop(false);
    if (outT >= 1.0f && inT >= 1.0f)
        phase2BgmCrossFadeActive = false;
}

std::shared_ptr<BgmActor> GameBgmDirector::GetActiveBossBgmActor() const
{
    const auto phase2 = phase2BgmActor.lock();
    return phase2BgmStarted && phase2 ? phase2 : bossBgmActor.lock();
}

void GameBgmDirector::PlayGameBgm()
{
    if (const auto game = gameBgmActor.lock()) game->Play();
}

void GameBgmDirector::PlayBossBattleBgm()
{
    if (const auto boss = bossBgmActor.lock()) boss->Play();
}

void GameBgmDirector::StopGameBgmIfNotFading()
{
    const auto game = gameBgmActor.lock();
    if (!gameBgmFading && game) game->Stop(false);
}

void GameBgmDirector::PlayBossDeathSecondBgm()
{
    const auto bossDeath = bossDeathSecondBgmActor.lock();
    if (!bossDeathSecondBgmPlayed && bossDeath) { bossDeathSecondBgmPlayed = true; bossDeath->Play(); }
}

void GameBgmDirector::PlayPlayerDeathBgm()
{
    const auto playerDeath = playerDeathBgmActor.lock();
    if (!playerDeathBgmPlayed && playerDeath) { playerDeathBgmPlayed = true; playerDeath->Play(); }
}

void GameBgmDirector::StopDeathBgm()
{
    if (const auto bossDeath = bossDeathSecondBgmActor.lock()) bossDeath->Stop(false);
    if (const auto playerDeath = playerDeathBgmActor.lock()) playerDeath->Stop(false);
}

void GameBgmDirector::ResetPhase2Runtime()
{
    phase2BgmStarted = false;
    ResetPhase2CrossFade();
}

void GameBgmDirector::ResetBossBattleBgm(BossBgmPhase phase, bool play, float bossBgmVolume)
{
    ResetPhase2CrossFade();
    phase2BgmStarted = phase == BossBgmPhase::Phase2;
    const auto activeBgm = GetActiveBossBgmActor();
    const auto stopInactiveBgm = [activeBgm, bossBgmVolume](const std::shared_ptr<BgmActor>& bgm)
        {
            if (bgm && bgm != activeBgm)
            {
                bgm->Stop(false);
                bgm->SetVolume(bossBgmVolume);
            }
        };
    stopInactiveBgm(bossBgmActor.lock());
    stopInactiveBgm(phase2BgmActor.lock());
    if (activeBgm)
    {
        activeBgm->SetVolume(bossBgmVolume);
        if (play && phase == BossBgmPhase::Phase1)
        {
            Logger::Log(Logger::LogCategory::Gameplay,
                "[BossBGM][RestartFromBeginning] caller=GameScene::ResetBossBattleBgm");
            activeBgm->RestartFromBeginning();
        }
        else if (play)
            activeBgm->Play();
        else
            activeBgm->Stop(false);
    }
    if (const auto game = gameBgmActor.lock()) game->Stop();
}

void GameBgmDirector::ResetDeathBgmState(BossBgmPhase phase, float bossBgmVolume)
{
    StopDeathBgm();
    bossDeathSecondBgmPlayed = false;
    playerDeathBgmPlayed = false;
    ResetDeathFade();
    ResetBossBattleBgm(phase, true, bossBgmVolume);
}

#ifdef USE_IMGUI
void GameBgmDirector::DrawPhase2DebugGui()
{
    ImGui::DragFloat("Phase2 BGM Fade Out Duration", &phase2BgmFadeOutDuration,
        0.01f, 0.0f, 5.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Phase2 BGM Fade In Duration", &phase2BgmFadeInDuration,
        0.01f, 0.0f, 5.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
}

void GameBgmDirector::DrawGameBgmDebugGui()
{
    ImGui::SeparatorText("BGM Debug");
    ImGui::DragFloat("Game BGM Fade Duration", &gameBgmFadeDuration,
        0.01f, 0.0f, 5.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
}
#endif
