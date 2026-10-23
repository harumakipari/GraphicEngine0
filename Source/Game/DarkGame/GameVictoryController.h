#pragma once

class GameVictoryUIActor;
class Player;
class CinemaCamera;

class GameVictoryController
{
public:
    void ResetPresentation();
    float PrepareCameraBlend() const;
    void PreparePlayer(Player* player);
    void BeginPresentation();
    bool CanAcceptSelection(const GameVictoryUIActor& victoryUIActor) const;
    bool AcceptSelection(int selection, bool restartAllowed, GameVictoryUIActor& victoryUIActor, float confirmVolume);
    int UpdateUntilSelection(Player* player, CinemaCamera* cinemaCameraActor,
        GameVictoryUIActor& victoryUIActor, float deathResultMoveSeVolume);
    void FinishUpdate(GameVictoryUIActor& victoryUIActor, float numberTexWidth, float numberTexHeight);
    void LoadBestTime();
    void CaptureResult(float finalBattleTime, bool finalBattleTimeSaved, GameVictoryUIActor& victoryUIActor);
#ifdef USE_IMGUI
    void DrawCameraIntroDebugGui();
    void DrawRankDebugGui();
    void DrawAnimationDebugGui();
    void DrawDelayDebugGui();
#endif

private:
    enum class VictoryResultPhase { None, CameraBlend, WinEmote, ResultDelay, ShowButtons, Interactive };
    VictoryResultPhase victoryResultPhase = VictoryResultPhase::None;
    float victoryResultDelayElapsed = 0.0f;
    float victoryResultDelayDuration = 0.70f;
    float victoryCameraBlendDuration = 0.60f;
    float resultRecallToIdleBlendDuration = 0.26f;
    // Weapon bone drops rapidly between 1.13 and 1.20 seconds in Emote_Slice_0.
    float resultUITriggerAnimationTime = 1.17f;
    static constexpr float victoryEmoteEndTime = 1.48f;
    bool victoryBestTimeValid = false;
    double victoryBestTime = 0.0;
    float victoryRankSLimit = 90.0f;
    float victoryRankALimit = 120.0f;
    float victoryRankBLimit = 180.0f;
};
