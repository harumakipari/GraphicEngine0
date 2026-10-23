#pragma once

#include <array>
#include <memory>
#include <DirectXMath.h>
#include "Components/Camera/CameraComponent.h"

struct SceneShaderConstants;
class Scene;
class Player;
class GruxEnemy;
class CinemaCamera;
class CameraManager;
class GameBossDeathUIActor;
class UIImageComponent;
class GameBgmDirector;
class CoreStandaloneAudioSource;


class GameBossDeathController
{
public:
    struct Context
    {
        const Scene* scene;
        Player* player;
        GruxEnemy* boss;
        CinemaCamera* cameraActor;
        CameraManager* cameraManager;
        GameBossDeathUIActor* finishUI;
        UIImageComponent* fadeOverlay;
        GameBgmDirector* bgm;
        SceneShaderConstants* shader;
    };
    void Start(GameBossDeathUIActor& ui, UIImageComponent& fade);
    bool Update(const Context& context, float deltaTime, bool huskComplete);
    bool SkipToFinishPrompt(const Context& context);
    void StopCinematicAudio();
    void Reset(SceneShaderConstants& shader, GameBossDeathUIActor& ui, UIImageComponent& fade);
    bool IsFinishPromptActive() const;
    bool IsCinematicAudioActive() const;
    bool CanSkip() const;
#ifdef USE_IMGUI
    void DrawDebugGui(Player* player, GruxEnemy* boss);
#endif

    bool LoadBossDeathShots();
    bool AreShotsLoaded() const { return bossDeathShotsLoaded; }
    void CaptureGameplayDof(const SceneShaderConstants& shader);
    void RestoreGameplayDof(SceneShaderConstants& shader);
    void BlendToResult(CinematicCameraComponent& camera, SceneShaderConstants& shader, float duration) const;

private:
    enum class Shot : size_t { Scream, Fall, Landing, Finish, Result, Count };
    struct ActorPose
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    void CutToShot(Shot shot, CinematicCameraComponent* camera, SceneShaderConstants& shader) const;
    void BlendToLanding(CinematicCameraComponent* camera, SceneShaderConstants& shader, float duration) const;
    ActorPose GetScreamBossPose() const { return bossDeathShots[0].boss; }
    ActorPose GetFallPlayerPose() const { return bossDeathShots[1].player; }
    ActorPose GetLandingBossPose() const { return bossDeathShots[2].boss; }
    ActorPose GetFinishPlayerPose() const { return bossDeathShots[3].player; }

    enum class BossDeathPhase : uint8_t
    {
        FadeOut,
        SetupCinematic,
        FadeInScream,
        DeathScream,
        DeathFall,
        DeathLanding,
        PlayerApproach,
        PlayerWalkStop,
        RecallLeadIn,
        RecallPingPong,
        FinishTriggered,
        HuskDelay,
        HuskPreview,
    };

    BossDeathPhase bossDeathPhase = BossDeathPhase::FadeOut;
    float bossDeathPhaseElapsed = 0.0f;
    float bossDeathFadeOutDuration = 1.6f;
    float bossDeathFadeInDuration = 1.6f;
    float bossDeathRoarStartTime = 0.17f;
    float bossDeathRoarEndTime = 1.176f;
    float bossDeathRoarPlaybackRate = 0.71f;
    float bossDeathStunPlaybackRate = 0.5f;
    float bossDeathStunToDeathBlend = 0.25f;
    float bossDeathFallToLandingBlendDuration = 1.0f;
    float bossDeathFwdStartTime = 0.498f;
    float bossDeathFwdEndTime = 2.233f;
    float bossDeathFwdPlaybackRate = 0.44f;
    float bossDeathFwdPromptMinTime = 1.38f;
    float bossDeathFwdPromptMaxTime = 1.625f;
    float bossDeathFwdPromptTime = 1.55f;
    float bossDeathFwdPromptDirection = 1.0f;
    float bossDeathFwdPromptPlaybackRate = 0.10f;
    float bossDeathScreamHoldMin = 1.474f;
    float bossDeathScreamHoldMax = 1.588f;
    float bossDeathScreamHoldRate = 0.30f;
    float bossDeathScreamHoldTime = 1.50f;
    float bossDeathScreamHoldDirection = 1.0f;
    DirectX::XMFLOAT3 bossDeathFwdPositionOffset{ 0.6f,0.0f,0.0f };
    float bossDeathPlayerApproachDuration = 2.7f;
    float bossDeathWalkStopStartDistance = 0.35f;
    DirectX::XMFLOAT3 bossDeathWalkStopStartPosition{};
    DirectX::XMFLOAT3 bossDeathWalkStopFinishPosition{};
    DirectX::XMFLOAT4 bossDeathWalkStopStartRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT4 bossDeathWalkStopFinishRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 bossDeathApproachStartPosition{ 5.025f,-0.1f,11.723f };
    DirectX::XMFLOAT4 bossDeathApproachStartRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 bossDeathFinishPlayerPositionOffset{ -0.9f,0.0f,0.0f };
    float bossDeathRecallPromptMinTime = 1.155f;
    float bossDeathRecallPromptMaxTime = 1.225f;
    float bossDeathRecallPromptTime = 1.163f;
    float bossDeathRecallPromptDirection = 1.0f;
    float bossDeathRecallPromptPlaybackRate = 0.07f;
    float bossDeathFinishHoldMinTime = 1.72f;
    float bossDeathFinishHoldMaxTime = 1.875f;
    float bossDeathFinishHoldPlaybackRate = 0.07f;
    float bossDeathFinishHoldTime = 1.58f;
    float bossDeathFinishHoldDirection = 1.0f;
    float bossDeathRecallFinishHitTime = 1.875f;
    float bossDeathHuskDelay = 1.4f;
    bool bossDeathFinishInputEnabled = false;
    std::shared_ptr<CoreStandaloneAudioSource> bossDeathGroanAudio;
    std::shared_ptr<CoreStandaloneAudioSource> bossDeathVoiceAudio;
    void StartBossDeathGroanLoop();
    void StopBossDeathGroanLoop();
    void UpdateBossDeathPromptLoop(GruxEnemy* gruxEnemyActor, float deltaTime);
    void UpdateBossDeathScreamHold(GruxEnemy* gruxEnemyActor, float deltaTime);
    bool SetupBossDeathCinematic(const Context& context);
    void ClampBossDeathPreviewTuning(Player* player, GruxEnemy* gruxEnemyActor);
    static void SetFadeAlpha(UIImageComponent& fade, float alpha);
    struct BossDeathDofState
    {
        float focusDistance = 0.0f;
        float nearRange = 0.0f;
        float range = 0.0f;
        float blurStrength = 0.0f;
        int enableBlur = 0;
        int enableDof = 0;
    };
    struct BossDeathShotState
    {
        CinematicCameraComponent::CameraPose camera{};
        ActorPose player{};
        ActorPose boss{};
        BossDeathDofState dof{};
    };
    static void ApplyBossDeathDof(SceneShaderConstants& shader, const BossDeathDofState& dof);
    std::array<BossDeathShotState, static_cast<size_t>(Shot::Count)> bossDeathShots{};
    bool bossDeathShotsLoaded = false;
    BossDeathDofState bossDeathGameplayDof{};
    bool bossDeathGameplayDofCaptured = false;
};
