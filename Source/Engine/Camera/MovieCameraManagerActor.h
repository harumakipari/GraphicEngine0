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
        EnemyEyeFlash,
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
public:
    explicit MovieCameraManagerActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform) override {}

    void Update(float dt) override;

    void DrawImGuiDetails() override;

    void SetMovieCameraComponent(std::shared_ptr<MovieCameraComponent> movieCameraComponent)
    {
        movieCameraWeakPtr = movieCameraComponent;
    }

    // ドアを開くムービーを再生する
    void PlayDoorMovie();

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

    DoorMovieState doorMovieState = DoorMovieState::None;
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
