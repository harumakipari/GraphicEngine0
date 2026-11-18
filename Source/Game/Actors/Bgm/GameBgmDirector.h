#pragma once

#include <memory>

class BgmActor;

class GameBgmDirector
{
public:
    enum class BossBgmPhase { Phase1, Phase2, Transition };
    void SetActors(const std::shared_ptr<BgmActor>& game, const std::shared_ptr<BgmActor>& boss,
        const std::shared_ptr<BgmActor>& phase2, const std::shared_ptr<BgmActor>& bossDeath,
        const std::shared_ptr<BgmActor>& playerDeath);
    void SetGameActor(const std::shared_ptr<BgmActor>& game);
    void ClearActors();
    void StopDeathBgm();
    void PlayGameBgm();
    void PlayBossBattleBgm();
    void StopGameBgmIfNotFading();
    void PlayBossDeathSecondBgm();
    void PlayPlayerDeathBgm();
    void ResetBossBattleBgm(BossBgmPhase phase, bool play, float bossBgmVolume);
    void ResetDeathBgmState(BossBgmPhase phase, float bossBgmVolume);
    void ResetPhase2Runtime();

    void BeginGameFade();
    void BeginBossDeathFade();
    void BeginPlayerDeathFade();
    void BeginPhase2CrossFade(float recallAnimationTime, float bossBgmVolume);
    bool IsPhase2Started() const { return phase2BgmStarted; }
    bool HasPhase2BgmRuntime() const { return phase2BgmStarted || phase2BgmCrossFadeActive; }

    void UpdateGameBgmFadeOut(float deltaTime);
    void UpdateDeathBgmFade(float deltaTime);
    void UpdatePhase2BgmCrossFade(float deltaTime, float bossBgmVolume);

#ifdef USE_IMGUI
    void DrawPhase2DebugGui();
    void DrawGameBgmDebugGui();
#endif

private:
    std::shared_ptr<BgmActor> GetActiveBossBgmActor() const;
    void ResetPhase2CrossFade();
    void ResetDeathFade();
    std::weak_ptr<BgmActor> gameBgmActor;
    std::weak_ptr<BgmActor> bossBgmActor;
    std::weak_ptr<BgmActor> phase2BgmActor;
    std::weak_ptr<BgmActor> bossDeathSecondBgmActor;
    std::weak_ptr<BgmActor> playerDeathBgmActor;

    float gameBgmFadeDuration = 0.75f;
    float gameBgmFadeStartVolume = 0.0f;
    float gameBgmFadeElapsed = 0.0f;
    bool gameBgmFading = false;
    float bossDeathBgmFadeTime = 1.0f;
    float playerDeathBgmFadeTime = 1.0f;
    float bossBgmFadeStartVolume = 0.0f;
    float bossBgmFadeElapsed = 0.0f;
    bool bossBgmFading = false;
    bool playerBgmFading = false;
    bool phase2BgmStarted = false;
    bool bossDeathSecondBgmPlayed = false;
    bool playerDeathBgmPlayed = false;
    bool phase2BgmCrossFadeActive = false;
    float phase2BgmFadeElapsed = 0.0f;
    float phase2BgmFadeOutDuration = 0.5f;
    float phase2BgmFadeInDuration = 0.5f;
    float phase2BgmPhase1StartVolume = 0.0f;
};
