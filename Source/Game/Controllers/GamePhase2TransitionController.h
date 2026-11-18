#pragma once

#include <array>
#include <cstddef>
#include <DirectXMath.h>
#include "Components/Camera/CameraComponent.h"

class Player;
class GruxEnemy;
class CinemaCamera;
class GameBgmDirector;

class GamePhase2TransitionController
{
public:
    void LoadShots();
    void ApplyRecallActorPose(const Player* player, const GruxEnemy* gruxEnemyActor);
    void CutToShot(size_t shotIndex, const CinemaCamera* cinemaCameraActor) const;
    bool UpdateRecall(GruxEnemy* gruxEnemyActor, GameBgmDirector& bgmDirector, float bossBgmVolume);
    void ResetTransform(bool phase2);
    void ApplyTransform(GruxEnemy* gruxEnemyActor) const;
    void ClearPlayerEmote();
    void ResetPlayerEmote(const Player* player);
    void BeginPlayerEmote(const Player* player);
    bool UpdatePlayerEmote(const Player* player);
    void FinishPlayerEmote(const Player* player);
#ifdef USE_IMGUI
    void DrawRecallDebugGui(GruxEnemy* gruxEnemyActor);
    void DrawPlayerEmoteDebugGui();
#endif

private:
    struct CinematicActorPose
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct Phase2CinematicShot
    {
        CinematicCameraComponent::CameraPose camera{};
        CinematicActorPose player{};
        CinematicActorPose boss{};
    };

    float phase2TransformStartTime = 5.3f;
    float phase2TransformEndTime = 8.0f;
    float phase2TransformProgress = 0.0f;
    bool phase2TransformCompleted = false;

    float phase2PlayerEmoteEndTime = 0.515f;
    bool phase2PlayerEmoteActive = false;
    bool phase2PlayerEmoteEndPoseHeld = false;

    bool LoadShotsInternal();
    std::array<Phase2CinematicShot, 2> phase2CinematicShots{};
    bool phase2CinematicShotsLoaded = false;


    const std::array<const char*, 2> phase2CinematicPresetPaths = {
    "Data/Saves/ScenePresets/boss_recall.json",
    "Data/Saves/ScenePresets/boss_recall_player.json",
    };

};
