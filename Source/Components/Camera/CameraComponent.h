#pragma once

#include <string>
#include <fstream>
#include <DirectXMath.h>

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "imgui.h"
#endif

#include "Components/Base/SceneComponent.h"
#include "Components/Easing/EasingComponent.h"
#include "Core/Actor.h"
#include "Engine/Camera/CameraConstants.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Scene/SceneSetting.h"

class TitleCamera;
class Camera;

class CameraComponent :public SceneComponent
{
public:
    CameraComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}

    // パースペクティブ設定
    void SetPerspective(const float fovY, const float aspect, const float nearZ, const float farZ)
    {
        this->fovY = fovY;
        this->aspect = aspect;
        this->nearZ = nearZ;
        this->farZ = farZ;
    }

    void SaveBookmark()
    {
        bookmark.position = GetComponentLocation();
        bookmark.rotation = GetComponentRotation();
        bookmark.yaw = yaw;
        bookmark.pitch = pitch;
        bookmark.fov = fovY;
        hasBookmark = true;
    }

    void LoadBookmark()
    {
        if (!hasBookmark) return;
        GetOwner()->SetPosition(bookmark.position);
        GetOwner()->SetQuaternionRotation(bookmark.rotation);
        yaw = bookmark.yaw;
        pitch = bookmark.pitch;
        fovY = bookmark.fov;
        UpdateRotationFromYawPitch();
    }

    const DirectX::XMFLOAT4X4& GetView();

    const DirectX::XMFLOAT4X4& GetProjection()
    {
        using namespace DirectX;
        XMStoreFloat4x4(&projection, XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ));
        return projection;
    }

    DirectX::XMFLOAT3 GetForward() const;

    DirectX::XMFLOAT3 GetRight() const;

    ViewConstants GetViewConstants();

    // CameraActorのrotationを更新する
    void UpdateRotationFromYawPitch();

    void AddYaw(const float v);

    void AddPitch(const float v);

    void SetYawAndPitch(const float yaw, const float pitch)
    {
        this->yaw = yaw;
        this->pitch = pitch;
        UpdateRotationFromYawPitch();
    }

    float GetYaw() const { return yaw; }

    float GetPitch() const { return pitch; }

    float GetFov()const { return fovY; }

    void SetFov(const float fov)
    {
        SetPerspective(fov, Graphics::GetScreenWidth() / Graphics::GetScreenHeight(), 0.1f, 1000.0f);
    }
    void SetYaw(const float yaw)
    {
        this->yaw = yaw;
    }
    void SetPitch(const float pitch)
    {
        this->pitch = pitch;
    }
public:
    CameraBookmark bookmark;
    bool hasBookmark = false;
    bool useLookTarget = false;
    DirectX::XMFLOAT3 lookTarget{};
protected:
    float fovY = DirectX::XMConvertToRadians(35.0f);
    float aspect = 1280.f / 720.f;
    float nearZ = 0.1f;
    float farZ = 1000.f;
    float yaw = 0.0f;
    float pitch = DirectX::XMConvertToRadians(-12.0f);

    DirectX::XMFLOAT4X4 view{};
    DirectX::XMFLOAT4X4 projection{};
    DirectX::XMFLOAT4X4 previousViewProjection{};

    float minPitchDegree = -26.5f; // pitchの最小角度
    float maxPitchDegree = 22.5f;  // pitchの最大角度

    bool firstFrame = true;
};


// 三人称視点のカメラ
class TPSCameraComponent : public CameraComponent
{
public:
    TPSCameraComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :CameraComponent(name, owner) {}

    void Tick(const float deltaTime) override
    {
        easingComponent.Tick(deltaTime);
    }

    void DrawImGuiInspector() override;

    DirectX::XMVECTOR ResolveCameraCollision(DirectX::FXMVECTOR focus, DirectX::FXMVECTOR idealEye);

public:
    float distance = 4.5f;
    DirectX::XMFLOAT3 targetOffset = { 0.0f, 1.5f, 0.0f }; // 注視点のオフセット。キャラクターの頭あたりを注視するために Y を 1.5f くらいにしている
    DirectX::XMFLOAT3 cameraOffset = { 0.0f, 0.0f, 0.0f }; // カメラの位置を微調整するためのオフセット
private:
    float distanceEasingValue = 0.0f;
    float distanceFrom = 0.0f;
    float distanceTo = 0.0f;

    EasingRunner easingComponent;
};


class DebugCameraComponent :public CameraComponent
{
public:
    DebugCameraComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :CameraComponent(name, owner) {}

    void Tick(const float deltaTime)override;

    void SetIsUseDebug(const bool useDebug) { this->useDebug = useDebug; }
private:
    void HandleKeyboardInput(float deltaTime);
    void HandleMouseInput(float deltaTime);
    void DrawImGuiInspector() override;

private:
    bool useDebug = false;
    float moveSpeed = 5.0f;
    float rotateSpeed = 0.001f;
};

class CinematicCameraComponent :public CameraComponent
{
public:
    struct CameraPose
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
        float fov = DirectX::XMConvertToRadians(60.0f);
    };

    CinematicCameraComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :CameraComponent(name, owner)
    {
        LoadBookmarksFromFile();
    }

    void Tick(const float deltaTime)override;

    void SetIsUseCinematic(const bool useCinematic) { this->useCinematic = useCinematic; }
    void SetDebugInputEnabled(const bool enabled) { cinematicCameraDebugInputEnabled = enabled; }

    void CutToPose(const CameraPose& pose);
    void BlendToPose(const CameraPose& pose, float duration);
    bool IsBlending() const { return poseBlending; }

public:
    std::vector<CameraBookmark> bookmarks;

private:
    // 保存関数
    void SaveBookmarksToFile();

    // 読み込み関数
    void LoadBookmarksFromFile();

    void HandleKeyboardInput(float deltaTime);
    void ApplyPose(const CameraPose& pose);
    void UpdatePoseBlend(float deltaTime);
    void SyncYawPitchFromRotation(const DirectX::XMFLOAT4& rotation);
    void HandleMouseInput(float deltaTime);
    void DrawImGuiInspector() override;

    void SaveBookmark(const int i);

    void LoadBookmark(const int i);

    void StartCameraPath();

    void UpdateCameraPath(const float deltaTime);

private:
    float moveSpeed = 2.0f;
    float rotateSpeed = 0.001f;
    bool useCinematic = false;
    bool cinematicCameraDebugInputEnabled = false;
    static const int MAX_BOOKMARKS = 10;

    bool playingPath = false;
    float pathTime = 0.f;
    int currentTarget = 0;
    float pathDuration = 2.0f; // 秒単位

    bool poseBlending = false;
    CameraPose poseBlendStart{};
    CameraPose poseBlendTarget{};
    float poseBlendElapsed = 0.0f;
    float poseBlendDuration = 0.0f;
};


class MovieCameraComponent : public CameraComponent
{
public:
    enum class EaseType:uint8_t
    {
        Linear,
        EaseIn,
        EaseOut,
        EaseInOut
    };

    struct CameraKeyframe
    {
        std::string name;

        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT4 rotation{};
        float fov = DirectX::XMConvertToRadians(60.f);

        float duration = 2.0f; // 次のキーまでの時間
        EaseType ease = EaseType::EaseInOut;
    };


    MovieCameraComponent(const std::string& name, const std::shared_ptr<Actor>& owner) : CameraComponent(name, owner) {}

    void Tick(const float deltaTime) override;

    void SetIsUseMovie(const bool useMovie){this->useMovieCamera = useMovie;}

    void CutToWorldPose(const DirectX::XMFLOAT3& position,const DirectX::XMFLOAT4& rotation, float fov);
    void ApplyWorldPose(const DirectX::XMFLOAT3& position,const DirectX::XMFLOAT4& rotation, float fov);
    float GetFirstKeyframeFov() const{return keys.empty() ? GetFov() : keys.front().fov;}
    // ムービー再生中かどうかを取得する
    bool IsMovieFinish() const { return finished; }
    bool IsPlaying() const { return playing; }

    float GetPlaybackElapsedTime() const;

    bool TryGetKeyframeStartTime(const std::string& keyName, float& outStartTime) const;

    bool TryGetKeyframeStartTime(const size_t keyIndex, float& outStartTime) const;

    size_t GetKeyframeCount() const { return keys.size(); }

    void Stop()
    {
        playing = false;
        finished = false;
        time = 0.0f;
    }

    void ResetToFirstKey()
    {
        Stop();
        reversePlay = false;
        currentIndex = 0;
        if (!keys.empty())
            ApplyWorldPose(keys.front().position, keys.front().rotation, keys.front().fov);
    }

    void JumpToKey(const size_t keyIndex);

    void SaveToJson(const std::string& path);

    void LoadFromJson(const std::string& path);
    void RefreshMovieFiles();

    void DrawImGuiInspector()override;

    void Start(bool reverse = false);
    bool ApplyFirstFrameToOwner();

    void SetOnMovieStart(const std::function<void()>& func) { onMovieStart = func; }

    // 現在のカメラ姿勢から仮想targetを計算する
    DirectX::XMFLOAT3 GetVirtualTarget(float distance = 5.0f);

    float ApplyEase(const float t, const EaseType type);

    std::string EaseToString(const EaseType e);

    EaseType StringToEase(const std::string& s);

private:
    void SyncYawPitchFromRotation(const DirectX::XMFLOAT4& rotation);
    void HandleKeyboardInput(float deltaTime);
    void HandleMouseInput(float deltaTime);
    void UpdatePath(float dt);
private:
    std::vector<CameraKeyframe> keys;
    int currentIndex = 0;
    int selectedKeyIndex = 0;
    float time = 0.f;
    bool playing = false;
    bool useMovieCamera = false;
    bool debugCameraMovementEnabled = false;
    float moveSpeed = 5.0f;
    float rotateSpeed = 0.001f;
    bool finished = false;  // 終わったかどうか
    bool reversePlay = false;   // 逆再生するかどうか

    std::string currentFile = "intro.json";
    std::string basePath = "./Data/Saves/MovieCameras/";
    std::vector<std::string> movieFiles;

    std::weak_ptr<TitleCamera> targetCamera;
    std::function<void()> onMovieStart;
};