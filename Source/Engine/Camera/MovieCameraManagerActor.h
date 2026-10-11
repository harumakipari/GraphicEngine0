#pragma once
#include <vector>
#include "Components/Camera/CameraComponent.h"
#include "Core/Actor.h"

class MovieCameraManagerActor :public Actor
{
public:
    struct DeathWideAnchorValidationDebug
    {
        std::string name = "None";
        std::string reason = "Valid";
        DirectX::XMFLOAT3 anchorPosition{};
        DirectX::XMFLOAT3 targetPosition{};
        float distance = 0.0f;
        bool targetPathHit = false;
        bool targetPathInitialOverlap = false;
        float targetPathHitDistance = 0.0f;
        DirectX::XMFLOAT3 targetPathHitPosition{};
        DirectX::XMFLOAT3 targetPathHitNormal{};
        std::string targetPathLayer = "None";
        bool probeHit = false;
        bool probeInitialOverlap = false;
        DirectX::XMFLOAT3 probeStart{};
        DirectX::XMFLOAT3 probeEnd{};
        std::string probeLayer = "None";
        std::string spaceProbeLayer = "None";
        bool valid = false;
    };

private:
    enum class DoorMovieState :uint8_t
    {
        None,
        DoorPreMovie,
        UpPlayerMovie,
        DoorOpening,
        PreBossRoomLerp, // ボスの部屋が明るくなる
        BossRoomLerp,   // ボスの部屋が明るくなる
        UpPlayerCombat, // プレイヤーが剣を構える
        PreUpPlayerCombatMovie,
        UpPlayerCombatMovie,
        EnemyMovie,
        EnemyName,
        PreFinished,
        Finished
    };

    enum class PrisonOpeningState : uint8_t
    {
        None,
        CameraPathPlaying,
        WaitingForJailDoorOpen,
        PlayerStanding,
        BlendingToTPS,
    };
public:
    enum class PrisonOpeningSittingMotionMode : uint8_t
    {
        OriginalSitting,
        StandingPingPong,
    };

    explicit MovieCameraManagerActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform) override;

    void Update(float dt) override;

    void DrawImGuiDetails() override;

    void SetMovieCameraComponent(std::shared_ptr<MovieCameraComponent> movieCameraComponent)
    {
        movieCameraWeakPtr = movieCameraComponent;
    }

    // ドアを開くムービーを再生する
    void PlayDoorMovie();
    // Runtime-only Veil of Aurora prison-opening entry. MovieCamera debug Play is separate.
    bool PreparePrisonOpeningMovieFirstFrame();
    void CancelPreparedPrisonOpeningMovie();
    bool PlayPrisonOpeningMovie();

    void PlayMovie(const std::string& file);
    void PlayDeathWideRelative(const DirectX::XMFLOAT3& playerPosition,
        const DirectX::XMFLOAT3& bossPosition);
    bool PlayDeathWideFromAnchors(
        const std::vector<std::shared_ptr<SceneComponent>>& anchors,
        const std::shared_ptr<SceneComponent>& target,
        float minimumDistance = 2.0f);

    void PlayReverse(const std::string& file)
    {
        auto movieCamera = this->movieCameraWeakPtr.lock();
        movieCamera->LoadFromJson("./Data/Saves/MovieCameras/" + file);
        movieCamera->Start(true);
    }

private:
    void UpdateDeathWideAnchorPreview();
    void HandlePrisonOpeningCameraPathCompleted();
    void UpdatePrisonOpeningPresentation(float deltaTime);
    void UpdatePrisonOpeningEyeOpening();
    void UpdatePrisonOpeningFaceCloseUpKeyTime();
    bool ReplayPrisonOpeningMovie();
    bool CanReplayPrisonOpeningMovie() const;
    void LoadPrisonOpeningEyeSettings();
    void SavePrisonOpeningEyeSettings() const;
    void EndPrisonOpeningEyeOverride();
    void BeginPrisonOpeningPlayerStanding();
    void BeginPrisonOpeningBlendToTPS();
    void UpdatePrisonOpeningBlendToTPS(float deltaTime);
    void FinishPrisonOpeningMovie();
    void LogPrisonOpeningTransitionDebug(const char* phase);

    DoorMovieState doorMovieState = DoorMovieState::None;
    bool prisonOpeningMoviePrepared = false;
    bool prisonOpeningMoviePlaying = false;
    bool prisonOpeningJailDoorInteractionRequested = false;
    PrisonOpeningState prisonOpeningState = PrisonOpeningState::None;
    DirectX::XMFLOAT3 prisonOpeningBlendStartPosition{};
    DirectX::XMFLOAT4 prisonOpeningBlendStartRotation{};
    float prisonOpeningBlendStartFov = 0.0f;
    float prisonOpeningBlendElapsed = 0.0f;
    float prisonOpeningBlendDuration = 1.4f;
    PrisonOpeningSittingMotionMode prisonOpeningSittingMotionMode =
        PrisonOpeningSittingMotionMode::StandingPingPong;
    float prisonOpeningSittingPingPongStartTime = 0.0f;
    float prisonOpeningSittingPingPongEndTime = 0.4f;
    float prisonOpeningSittingPingPongSpeed = 0.15f;
    float prisonOpeningSittingToStandingBlendTime = 0.15f;
    bool prisonOpeningEyeOpeningEnabled = true;
    float prisonOpeningEyeOpenDelay = 0.0f;
    float prisonOpeningEyeOpenDuration = 1.f;
    float prisonOpeningFaceCloseUpKeyTime = 0.0f;
    bool prisonOpeningFaceCloseUpKeyTimeValid = false;
    float prisonOpeningCurrentMovieTime = 0.0f;
    float prisonOpeningCurrentEyeWeight = 0.0f;
    bool prisonOpeningEyeOverrideActive = false;
    bool prisonOpeningEyeOpeningPreview = false;
    float prisonOpeningEyeOpeningPreviewTime = 0.0f;
    static constexpr const char* PrisonOpeningEyeSettingsPath = "./Data/Saves/MovieCameras/prison_opening_eye_settings.json";
    static constexpr const char* PrisonOpeningFaceCloseUpKeyName = "4";
    static constexpr size_t PrisonOpeningFaceCloseUpKeyIndex = 3;
    static constexpr const char* PrisonOpeningMoviePath = "./Data/Saves/MovieCameras/prison_opening.json";

    bool prisonOpeningTransitionDebugActive = false;
    int prisonOpeningTransitionDebugFrame = 0;
    int prisonOpeningTransitionDebugPostFinishFramesRemaining = 0;
    std::weak_ptr<MovieCameraComponent> movieCameraWeakPtr;
    std::weak_ptr<SceneComponent> deathWidePreviewAnchor;
    std::weak_ptr<SceneComponent> deathWidePreviewTarget;
    std::string deathWidePreviewAnchorName = "None";
    float deathWidePreviewFov = 0.0f;
    bool deathWideAnchorPreviewEnabled = false;
    DirectX::XMFLOAT3 deathWideDebugEyeToTargetDirection{};
    DirectX::XMFLOAT4 deathWideDebugGeneratedRotation{};
    DirectX::XMFLOAT4 deathWideDebugRotationBefore{};
    DirectX::XMFLOAT4 deathWideDebugRotationAfter{};
    DirectX::XMFLOAT4 deathWideDebugComponentRotationAfter{};
    DirectX::XMFLOAT3 deathWideDebugMovieForward{};
    DirectX::XMFLOAT3 deathWideDebugDirectForward{};
    float deathWideDebugForwardDot = 0.0f;
    float deathWideDebugDirectForwardDot = 0.0f;
    bool deathWideDebugRotationValid = false;
    std::vector<DeathWideAnchorValidationDebug> deathWideValidationDebug;
    float elapsedTimer = 0.0f;
    float bossRoomZoomDuration = 3.0f;
    float bossRoomZoomTargetFovDegree = 20.0f;
    float bossRoomCameraMoveDistance =5.0f;
    float bossRoomZoomStartFov = DirectX::XMConvertToRadians(35.0f);
    float bossRoomZoomElapsed = 0.0f;
    DirectX::XMFLOAT3 bossRoomZoomStartPosition{};
    DirectX::XMFLOAT3 bossRoomZoomTargetPosition{};
    DirectX::XMFLOAT4 bossRoomZoomStartRotation{};
    DirectX::XMFLOAT4 bossRoomZoomTargetRotation{};
};
