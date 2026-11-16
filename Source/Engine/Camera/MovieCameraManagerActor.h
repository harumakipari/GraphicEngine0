#pragma once
#include "Components/Camera/CameraComponent.h"
#include "Core/Actor.h"

class MovieCameraManagerActor :public Actor
{
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

    void SetMovieCameraComponent(const std::shared_ptr<MovieCameraComponent>& movieCameraComponent)
    {
        movieCameraWeakPtr = movieCameraComponent;
    }

    // ドアを開くムービーを再生する
    void PlayDoorMovie();
    bool PreparePrisonOpeningMovieFirstFrame();
    void CancelPreparedPrisonOpeningMovie();
    bool PlayPrisonOpeningMovie();
    void SkipPrisonOpeningToGameplay();
    void SkipBossEntryToBattle();

    bool IsPrisonOpeningMovieActive() const { return prisonOpeningMoviePlaying; }

    bool IsBossEntryMovieActive() const
    {
        return (doorMovieState != DoorMovieState::None &&
            doorMovieState != DoorMovieState::Finished) || bossEntryExternalBlendActive;
    }

    void PlayMovie(const std::string& file);
    void PlayReverse(const std::string& file)
    {
        auto movieCamera = this->movieCameraWeakPtr.lock();
        movieCamera->LoadFromJson("./Data/Saves/MovieCameras/" + file);
        movieCamera->Start(true);
    }

private:
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
    void FinishBossEntryToBattle();
    DirectX::XMFLOAT4 CreateLookRotation(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& target);

    DoorMovieState doorMovieState = DoorMovieState::None;
    bool bossEntryExternalBlendActive = false;
    bool bossEntryBattleStarted = false;
    bool prisonOpeningMoviePrepared = false;
    bool prisonOpeningMoviePlaying = false;
    bool prisonOpeningJailDoorInteractionRequested = false;
    PrisonOpeningState prisonOpeningState = PrisonOpeningState::None;
    DirectX::XMFLOAT3 prisonOpeningBlendStartPosition{};
    DirectX::XMFLOAT4 prisonOpeningBlendStartRotation{};
    float prisonOpeningBlendStartFov = 0.0f;
    float prisonOpeningBlendElapsed = 0.0f;
    float prisonOpeningBlendDuration = 1.4f;
    PrisonOpeningSittingMotionMode prisonOpeningSittingMotionMode = PrisonOpeningSittingMotionMode::StandingPingPong;
    float prisonOpeningSittingPingPongStartTime = 0.0f;
    float prisonOpeningSittingPingPongEndTime = 0.4f;
    float prisonOpeningSittingPingPongSpeed = 0.15f;
    float prisonOpeningSittingToStandingBlendTime = 0.15f;
    bool prisonOpeningEyeOpeningEnabled = true;
    float prisonOpeningEyeOpenDelay = 0.0f;
    float prisonOpeningEyeOpenDuration = 1.f;
    float prisonOpeningFaceCloseUpKeyTime = 0.0f;
    bool prisonOpeningFaceCloseUpKeyTimeValid = false;
    bool prisonOpeningEyeOverrideActive = false;
    bool prisonOpeningEyeOpeningPreview = false;
    float prisonOpeningEyeOpeningPreviewTime = 0.0f;
    static constexpr const char* PrisonOpeningEyeSettingsPath = "./Data/Saves/MovieCameras/prison_opening_eye_settings.json";
    static constexpr const char* PrisonOpeningFaceCloseUpKeyName = "4";
    static constexpr size_t PrisonOpeningFaceCloseUpKeyIndex = 3;
    static constexpr const char* PrisonOpeningMoviePath = "./Data/Saves/MovieCameras/prison_opening.json";

    std::weak_ptr<MovieCameraComponent> movieCameraWeakPtr;
    float bossRoomZoomDuration = 3.0f;
    float bossRoomZoomTargetFovDegree = 20.0f;
    float bossRoomCameraMoveDistance = 5.0f;
    float bossRoomZoomStartFov = DirectX::XMConvertToRadians(35.0f);
    float bossRoomZoomElapsed = 0.0f;
    DirectX::XMFLOAT3 bossRoomZoomStartPosition{};
    DirectX::XMFLOAT3 bossRoomZoomTargetPosition{};
    DirectX::XMFLOAT4 bossRoomZoomStartRotation{};
    DirectX::XMFLOAT4 bossRoomZoomTargetRotation{};
};
