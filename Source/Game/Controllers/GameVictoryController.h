#pragma once

class GameVictoryUIActor;
class Player;
class CinemaCamera;

class GameVictoryController
{
private:
    enum class VictoryResultPhase :uint8_t
    {
        None,
        CameraBlend,
        WinEmote,
        ResultDelay,
        ShowButtons,
        Interactive
    };

public:
    // リセット処理
    void ResetPresentation();
    // リザルト表示までのカメラブレンドを取得
    float GetVictoryCameraBlend() const { return victoryCameraBlendDuration; }

    void PreparePlayer(const Player* player) const;
    void BeginPresentation();
    bool CanAcceptSelection(const GameVictoryUIActor& victoryUIActor) const;
    bool AcceptSelection(int selection, bool restartAllowed, GameVictoryUIActor& victoryUIActor, float confirmVolume);
    int UpdateUntilSelection(const Player* player, const CinemaCamera* cinemaCameraActor,
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
    VictoryResultPhase victoryResultPhase = VictoryResultPhase::None;
    float victoryResultDelayElapsed = 0.0f;
    float victoryResultDelayDuration = 0.70f;
    float victoryCameraBlendDuration = 0.60f;   // リザルト表示までのカメラブレンド
    float resultRecallToIdleBlendDuration = 0.26f;  // Recall->Idleのブレンド
    float resultUITriggerAnimationTime = 1.17f;
    static constexpr float victoryEmoteEndTime = 1.48f;
    bool victoryBestTimeValid = false;
    double victoryBestTime = 0.0;
    float victoryRankSLimit = 90.0f;
    float victoryRankALimit = 120.0f;
    float victoryRankBLimit = 180.0f;
};
