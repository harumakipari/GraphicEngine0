#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <DirectXMath.h>

class Player;
class GruxEnemy;
class GameDefeatUIActor;
class GameBgmDirector;
class CinemaCamera;
class DarkCameraActor;

class GamePlayerDeathController
{
public:
    enum class Cue : uint8_t
    {
        GameplayHudFade,
        OverlayFade,
        DefeatedFade,
        BattleTimeFade,
        ButtonsFade
    };

    void SetCueCallback(std::function<void(Cue)> callback);
    void StartPresentation(float battleTime, GameDefeatUIActor& defeatUIActor);
    void ResetDeathCameraStart();
    void BeginHudFade(Player* player, GruxEnemy* boss);
    void PrepareDeathActor(Player* player, DarkCameraActor* camera, std::function<void()> cameraStartCallback);
    void StartDeathCamera(DarkCameraActor* camera, const std::shared_ptr<GruxEnemy>& boss);
    void ResetPresentationElapsed();
    bool AcceptSelection(GameDefeatUIActor& defeatUIActor, float confirmVolume);
    void UpdateFrameData(GameDefeatUIActor& defeatUIActor, float numberTexWidth, float numberTexHeight, float moveVolume) const;
    bool Update(float deltaTime, bool allowInput, Player* player, GruxEnemy* boss,
        GameDefeatUIActor& defeatUIActor, GameBgmDirector& bgmDirector, CinemaCamera* camera,
        float numberTexWidth, float numberTexHeight, float moveVolume);
    void StageDeathActors(Player* player, GruxEnemy* gruxEnemyActor);
#ifdef USE_IMGUI
    void DrawDebugGui(float& moveVolume, float& confirmVolume, float& texWidth, float& texHeight, float battleElapsedTime);
#endif

private:
#ifdef USE_IMGUI
    void DrawStagingDebugGui();
    void DrawPresentationDebugGui();
    void DrawAttemptTimeDebugGui() const;
#endif
    void NotifyGameplayHudFade();
    void ResetDeathPresentationCues();
    void FireDeathPresentationCue(Cue cue, bool& fired);
    void UpdateDeathResultPresentation(Player* player, GruxEnemy* boss, GameDefeatUIActor* defeatUIActor,
        GameBgmDirector& bgmDirector, CinemaCamera* camera, float numberTexWidth, float numberTexHeight, float moveVolume);

    enum class DeathStagingArea : uint8_t
    {
        Center,
        Front,
        Back,
        Left,
        Right,
        FrontLeft,
        FrontRight,
        BackLeft,
        BackRight
    };

    struct DeathStagingAreaSettings
    {
        float bossDistance = 4.5f;
    };

    DeathStagingArea DetermineDeathStagingArea(const DirectX::XMFLOAT3& originalPlayerPosition) const;

    bool deathCameraStartRequested = false;
    float deathAttemptTime = 0.0f;
    float deathPresentationElapsed = 0.0f;
    float deathHudFadeDuration = 0.60f;
    float deathOverlayStartTime = 4.6f;
    float deathDefeatedStartTime = 5.0f;
    float deathBattleTimeStartTime = 5.1f;
    float deathButtonsStartTime = 5.2f;
    float deathSelectLineStartTime = 4.40f;
    float deathResultInputEnableTime = 5.2f;
    bool deathHudFadeCueFired = false;
    bool deathOverlayCueFired = false;
    bool deathDefeatedCueFired = false;
    bool deathBattleTimeCueFired = false;
    bool deathButtonsCueFired = false;
    std::function<void(Cue)> deathPresentationCueCallback;

    float deathStagingMinPlayerX = 0.0f;
    float deathStagingMaxPlayerX = 17.425f;
    float deathStagingMinPlayerZ = 1.0f;
    float deathStagingMaxPlayerZ = 20.45f;
    float deathStagingWallInsetX = 0.8f;
    float deathStagingWallInsetZ = 0.8f;
    std::array<DeathStagingAreaSettings, 9> deathStagingAreaSettings{};
};
