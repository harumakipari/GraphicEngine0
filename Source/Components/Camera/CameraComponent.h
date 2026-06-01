#ifndef CAMERA_COMPONENT_H
#define CAMERA_COMPONENT_H

// C++ 標準ライブラリ
#include <string>
#include <fstream>

// 他ライブラリ
#include <DirectXMath.h>


#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "imgui.h"
#endif

// プロジェクトの他のヘッダ
#include "Components/Base/SceneComponent.h"
#include "Components/Easing/CoreEasingComponent.h"
#include "Core/Actor.h"
#include "Engine/Camera/CameraConstants.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Scene/SceneSetting.h"

class TitleCamera;
class Camera;

class CameraComponent :public SceneComponent
{
public:
    CameraBookmark bookmark;
    bool hasBookmark = false;

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


    bool useLookTarget = false;
    DirectX::XMFLOAT3 lookTarget{};

    ViewConstants GetViewConstants();

    // CameraActorのrotationを更新する
    void UpdateRotationFromYawPitch()
    {
        using namespace DirectX;

#if 0
        XMVECTOR qYaw = XMQuaternionRotationAxis(
            XMVectorSet(0, 1, 0, 0),
            yaw);

        XMVECTOR qPitch = XMQuaternionRotationAxis(
            XMVectorSet(1, 0, 0, 0),
            pitch);

        XMVECTOR q = XMQuaternionNormalize(
            XMQuaternionMultiply(qPitch, qYaw));
#else
        XMVECTOR qYaw = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), yaw);
        XMVECTOR right = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), qYaw);
        XMVECTOR qPitch = XMQuaternionRotationAxis(right, pitch);
        XMVECTOR q = XMQuaternionNormalize(XMQuaternionMultiply(qPitch, qYaw));
#endif // 0

        XMFLOAT4 rot;
        XMStoreFloat4(&rot, q);

        GetOwner()->SetQuaternionRotation(rot);
    }

    void AddYaw(const float v)
    {
        yaw += v;
        UpdateRotationFromYawPitch();
    }

    void AddPitch(const float v)
    {
        pitch += v;

        pitch = std::clamp(
            pitch,
            DirectX::XMConvertToRadians(-60.0f),
            DirectX::XMConvertToRadians(50.0f)
        );

        UpdateRotationFromYawPitch();
    }

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
protected:
    float fovY = DirectX::XMConvertToRadians(35.0f);
    float aspect = 1280.f / 720.f;
    float nearZ = 0.1f;
    float farZ = 1000.f;
    float yaw = 0.0f;
    float roll = 0.0f;
    float pitch = DirectX::XMConvertToRadians(-12.0f);

    DirectX::XMFLOAT4X4 view{};
    DirectX::XMFLOAT4X4 projection{};
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

    virtual void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        SceneComponent::DrawImGuiInspector();
        if (ImGui::TreeNode((name_ + "  camera").c_str()))
        {
            ImGui::DragFloat3(U8("注視点のオフセット"), &targetOffset.x, 0.1f);
            ImGui::DragFloat3(U8("カメラの位置のオフセット"), &cameraOffset.x, 0.1f);
            // ===== yaw / pitch / fov を degree 表示 =====
            float yawDeg = DirectX::XMConvertToDegrees(yaw);
            float pitchDeg = DirectX::XMConvertToDegrees(pitch);
            float fovDeg = DirectX::XMConvertToDegrees(fovY);
            if (ImGui::DragFloat("FOV (deg)", &fovDeg, 0.5f, 10.0f, 120.0f))
            {
                fovY = DirectX::XMConvertToRadians(fovDeg);
            }
            if (ImGui::DragFloat("yaw (deg)", &yawDeg, 0.5f))
            {
                yaw = DirectX::XMConvertToRadians(yawDeg);
            }
            if (ImGui::DragFloat("pitch (deg)", &pitchDeg, 0.5f))
            {
                pitch = DirectX::XMConvertToRadians(pitchDeg);
            }
            ImGui::SliderFloat("nearZ", &nearZ, 0.01f, 100.0f);
            ImGui::DragFloat("farZ", &farZ, 0.1f);
            ImGui::SliderFloat("distance", &distance, 0.01f, 100.0f);
            ImGui::Checkbox("Camera Lock", &cameraLock);
            ImGui::SliderFloat(U8("追従するスピード"), &autoFollowStrength, 0.05f, 10.0f);

            ImGui::TreePop();
        }
#endif
    }

    // カメラの距離をイージングで変化させる
    void PlayDistance(const float from, const float to, const float time)
    {
        distanceFrom = from;
        distanceTo = to;

        TestEasingHandler handler;
        handler.AddEasing(TestEaseType::OutExp, 0.0f, 1.0f, time);

        PropertyAccessor<float> accessor;
        accessor.getter = [this]() { return distanceEasingValue; };
        accessor.setter = [this](const float t)
            {
                distanceEasingValue = t;
                distance = std::lerp(distanceFrom, distanceTo, t);
            };

        easingComponent.StartHandler(handler, accessor);
    }

    float distance = 4.5f;
    DirectX::XMFLOAT3 targetOffset = { 0.0f, 1.5f, 0.0f }; // 注視点のオフセット。キャラクターの頭あたりを注視するために Y を 1.5f くらいにしている
    DirectX::XMFLOAT3 cameraOffset = { 0.0f, 0.0f, 0.0f }; // カメラの位置を微調整するためのオフセット

    DirectX::XMVECTOR ResolveCameraCollision(
        DirectX::FXMVECTOR focus,
        DirectX::FXMVECTOR idealEye
    );


private:


    static float WrapAngle(float a)
    {
        using namespace DirectX;

        while (a > XM_PI)  a -= XM_2PI;
        while (a < -XM_PI) a += XM_2PI;
        return a;
    }
private:
    // 自動追従のためのパラメータ
    float autoFollowStrength = 2.0f; // 追従の強さ。大きいほど素早く追従する
    float autoFollowDeadZone = 0.15f;
    float autoFollowDelayTimer = 0.0f;
    float autoFollowDelay = 1.2f; // カメラの方向を変えたら、一定時間追従しないようにするための遅延時間
    float autoFollowDegree = 25.0f; // 追従する角度の閾値（degree）

    float distanceEasingValue = 0.0f;
    float distanceFrom = 0.0f;
    float distanceTo = 0.0f;

    EasingRunner easingComponent;
    bool cameraLock = false;
};


class DebugCameraComponent :public CameraComponent
{
public:
    DebugCameraComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :CameraComponent(name, owner) {}

    void Tick(const float deltaTime)override
    {
        if (!useDebug) return;
        HandleKeyboardInput(deltaTime);
        HandleMouseInput(deltaTime);
        if (InputSystem::GetInputState("F5", InputStateMask::Release))
        {
            SaveBookmark();
        }

        if (InputSystem::GetInputState("F6", InputStateMask::Release))
        {
            LoadBookmark();
        }
    }

    void SetIsUseDebug(const bool useDebug) { this->useDebug = useDebug; }


private:

    void HandleKeyboardInput(float deltaTime);
    void HandleMouseInput(float deltaTime)
    {
#if 1
        if (InputSystem::GetInputState("MouseRight"))
        {
            int dx, dy;
            InputSystem::GetMouseDelta(dx, dy);

            AddYaw(dx * rotateSpeed);
            AddPitch(dy * rotateSpeed);

            //yaw += dx * rotateSpeed;
            //pitch += dy * rotateSpeed;

            // 上下向きすぎ防止（重要）
            pitch = std::clamp(pitch, -DirectX::XM_PIDIV2 + 0.01f, DirectX::XM_PIDIV2 - 0.01f);
        }
        using namespace DirectX;

        // ワールドY軸Yaw
        XMVECTOR qYaw = XMQuaternionRotationAxis(
            XMVectorSet(0, 1, 0, 0),
            yaw
        );

        // ローカルX軸Pitch
        XMVECTOR qPitch = XMQuaternionRotationAxis(
            XMVectorSet(1, 0, 0, 0),
            pitch
        );

        // 合成順序：Yaw → Pitch
        XMVECTOR q = XMQuaternionNormalize(
            XMQuaternionMultiply(qPitch, qYaw)
        );

        XMFLOAT4 rot;
        XMStoreFloat4(&rot, q);
        GetOwner()->SetQuaternionRotation(rot);
        return;
#endif // 0


        if (InputSystem::GetInputState("MouseRight"))
        {
            int dx, dy;
            InputSystem::GetMouseDelta(dx, dy);


            float yawDelta = dx * rotateSpeed;
            float pitchDelta = dy * rotateSpeed;

            using namespace DirectX;

            //XMFLOAT4 rot = GetComponentRotation();
            XMFLOAT4 rot = GetOwner()->GetQuaternionRotation();
            XMVECTOR q = XMLoadFloat4(&rot);

            XMVECTOR qYaw = XMQuaternionRotationAxis(
                XMVectorSet(0, 1, 0, 0),
                yawDelta
            );

            XMVECTOR right = XMVector3Rotate(
                XMVectorSet(1, 0, 0, 0),
                q
            );

            XMVECTOR qPitch = XMQuaternionRotationAxis(
                right,
                pitchDelta
            );

            q = XMQuaternionMultiply(q, qYaw);
            q = XMQuaternionMultiply(q, qPitch);

            q = XMQuaternionNormalize(q);

            XMStoreFloat4(&rot, q);
            GetOwner()->SetQuaternionRotation(rot);
        }
    }

    virtual void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI

        SceneComponent::DrawImGuiInspector();
        if (ImGui::TreeNode((name_ + "  camera").c_str()))
        {
            ImGui::DragFloat("moveSpeed", &moveSpeed, 0.1f);
            ImGui::DragFloat("rotateSpeed", &rotateSpeed, 0.1f);

            // ===== yaw / pitch / fov を degree 表示 =====
            float yawDeg = DirectX::XMConvertToDegrees(yaw);
            float pitchDeg = DirectX::XMConvertToDegrees(pitch);
            float fovDeg = DirectX::XMConvertToDegrees(fovY);
            if (ImGui::DragFloat("FOV (deg)", &fovDeg, 0.5f, 10.0f, 120.0f))
            {
                fovY = DirectX::XMConvertToRadians(fovDeg);
            }
            if (ImGui::DragFloat("yaw (deg)", &yawDeg, 0.5f))
            {
                yaw = DirectX::XMConvertToRadians(yawDeg);
            }
            if (ImGui::DragFloat("pitch (deg)", &pitchDeg, 0.5f))
            {
                pitch = DirectX::XMConvertToRadians(pitchDeg);
            }
            ImGui::SliderFloat("nearZ", &nearZ, 0.01f, 100.0f);

            ImGui::TreePop();
        }
#endif
    }


private:
    bool useDebug = false;
    float moveSpeed = 5.0f;
    float rotateSpeed = 0.001f;

};

class CinematicCameraComponent :public CameraComponent
{
public:
    CinematicCameraComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :CameraComponent(name, owner)
    {
        LoadBookmarksFromFile();
    }

    void Tick(const float deltaTime)override
    {
        if (!useCinematic) return;
        HandleKeyboardInput(deltaTime);
        HandleMouseInput(deltaTime);
        if (playingPath) {
            UpdateCameraPath(deltaTime);
        }
    }

    void SetIsUseCinematic(const bool useCinematic) { this->useCinematic = useCinematic; }

    std::vector<CameraBookmark> bookmarks;

private:
    float moveSpeed = 2.0f;
    float rotateSpeed = 0.001f;
    bool useCinematic = false;

    // 保存関数
    void SaveBookmarksToFile();

    // 読み込み関数
    void LoadBookmarksFromFile();

    void HandleKeyboardInput(float deltaTime);
    void HandleMouseInput(float deltaTime)
    {
#if 1
        if (InputSystem::GetInputState("MouseRight"))
        {
            int dx, dy;
            InputSystem::GetMouseDelta(dx, dy);

            AddYaw(dx * rotateSpeed);
            AddPitch(dy * rotateSpeed);

            //yaw += dx * rotateSpeed;
            //pitch += dy * rotateSpeed;

            // 上下向きすぎ防止（重要）
            pitch = std::clamp(pitch, -DirectX::XM_PIDIV2 + 0.01f, DirectX::XM_PIDIV2 - 0.01f);
        }
        using namespace DirectX;

        // ワールドY軸Yaw
        XMVECTOR qYaw = XMQuaternionRotationAxis(
            XMVectorSet(0, 1, 0, 0),
            yaw
        );

        // ローカルX軸Pitch
        XMVECTOR qPitch = XMQuaternionRotationAxis(
            XMVectorSet(1, 0, 0, 0),
            pitch
        );

        // 合成順序：Yaw → Pitch
        XMVECTOR q = XMQuaternionNormalize(
            XMQuaternionMultiply(qPitch, qYaw)
        );

        XMFLOAT4 rot;
        XMStoreFloat4(&rot, q);
        GetOwner()->SetQuaternionRotation(rot);
        return;
#endif // 0


        if (InputSystem::GetInputState("MouseRight"))
        {
            int dx, dy;
            InputSystem::GetMouseDelta(dx, dy);


            float yawDelta = dx * rotateSpeed;
            float pitchDelta = dy * rotateSpeed;

            using namespace DirectX;

            //XMFLOAT4 rot = GetComponentRotation();
            XMFLOAT4 rot = GetOwner()->GetQuaternionRotation();
            XMVECTOR q = XMLoadFloat4(&rot);

            XMVECTOR qYaw = XMQuaternionRotationAxis(
                XMVectorSet(0, 1, 0, 0),
                yawDelta
            );

            XMVECTOR right = XMVector3Rotate(
                XMVectorSet(1, 0, 0, 0),
                q
            );

            XMVECTOR qPitch = XMQuaternionRotationAxis(
                right,
                pitchDelta
            );

            q = XMQuaternionMultiply(q, qYaw);
            q = XMQuaternionMultiply(q, qPitch);

            q = XMQuaternionNormalize(q);

            XMStoreFloat4(&rot, q);
            GetOwner()->SetQuaternionRotation(rot);
        }
    }

    virtual void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI

        SceneComponent::DrawImGuiInspector();
        if (ImGui::TreeNode((name_ + "  camera").c_str()))
        {
            ImGui::DragFloat("moveSpeed", &moveSpeed, 0.1f);
            ImGui::DragFloat("rotateSpeed", &rotateSpeed, 0.1f);

            // ===== yaw / pitch / fov を degree 表示 =====
            float yawDeg = DirectX::XMConvertToDegrees(yaw);
            float pitchDeg = DirectX::XMConvertToDegrees(pitch);
            float fovDeg = DirectX::XMConvertToDegrees(fovY);
            if (ImGui::DragFloat("FOV (deg)", &fovDeg, 0.5f, 10.0f, 120.0f))
            {
                fovY = DirectX::XMConvertToRadians(fovDeg);
            }
            if (ImGui::DragFloat("yaw (deg)", &yawDeg, 0.5f))
            {
                yaw = DirectX::XMConvertToRadians(yawDeg);
            }
            if (ImGui::DragFloat("pitch (deg)", &pitchDeg, 0.5f))
            {
                pitch = DirectX::XMConvertToRadians(pitchDeg);
            }
            ImGui::SliderFloat("nearZ", &nearZ, 0.01f, 100.0f);

            for (int i = 0; i < MAX_BOOKMARKS; i++)
            {
                ImGui::PushID(i);

                // 名前を編集できる
                char nameBuf[64];
                strncpy_s(nameBuf, bookmarks[i].name.c_str(), sizeof(nameBuf));
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                {
                    bookmarks[i].name = nameBuf; // 入力変更を反映
                    SaveBookmarksToFile();       // 編集したら即保存もOK
                }

                ImGui::SameLine();
                if (ImGui::Button("Go"))
                {
                    LoadBookmark(i);
                }

                ImGui::SameLine();

                if (ImGui::Button("Save"))
                {
                    SaveBookmark(i);
                }


                ImGui::PopID();
            }

            if (ImGui::Button("Play Path")) {
                StartCameraPath();
            }

            ImGui::TreePop();
        }
#endif
    }

    void SaveBookmark(int i)
    {
        if (i >= MAX_BOOKMARKS)
            return;

        if (bookmarks.size() < MAX_BOOKMARKS)
            bookmarks.resize(MAX_BOOKMARKS);

        bookmarks[i].position = GetComponentLocation();
        bookmarks[i].rotation = GetComponentRotation();
        bookmarks[i].yaw = yaw;
        bookmarks[i].pitch = pitch;
        bookmarks[i].fov = fovY;

        SaveBookmarksToFile();
    }
    void LoadBookmark(int i)
    {
        if (i >= bookmarks.size()) return;

        auto& b = bookmarks[i];

        GetOwner()->SetPosition(b.position);

        yaw = b.yaw;
        pitch = b.pitch;
        fovY = b.fov;

        UpdateRotationFromYawPitch();
    }

    void StartCameraPath() {
        if (bookmarks.empty()) return;
        playingPath = true;
        pathTime = 0.f;
        currentTarget = 1; // 1つ目のブックマークに向かう
    }

    void UpdateCameraPath(float deltaTime)
    {
        if (currentTarget >= bookmarks.size()) {
            playingPath = false;
            return;
        }

        auto& src = bookmarks[currentTarget - 1];
        auto& dst = bookmarks[currentTarget];

        pathTime += deltaTime;
        float t = std::clamp(pathTime / pathDuration, 0.f, 1.f);

        // Lerp position
        DirectX::XMFLOAT3 pos;
        pos.x = src.position.x + (dst.position.x - src.position.x) * t;
        pos.y = src.position.y + (dst.position.y - src.position.y) * t;
        pos.z = src.position.z + (dst.position.z - src.position.z) * t;
        GetOwner()->SetPosition(pos);

        // SLerp rotation
        DirectX::XMVECTOR q1 = DirectX::XMLoadFloat4(&src.rotation);
        DirectX::XMVECTOR q2 = DirectX::XMLoadFloat4(&dst.rotation);

        q1 = DirectX::XMQuaternionNormalize(q1);
        q2 = DirectX::XMQuaternionNormalize(q2);

        float dot = DirectX::XMVectorGetX(DirectX::XMVector4Dot(q1, q2));
        if (dot < 0.f) q2 = DirectX::XMVectorNegate(q2); // 最短角度補正

        DirectX::XMVECTOR q;
        if (DirectX::XMQuaternionEqual(q1, q2)) {
            q = q1;
        }
        else {
            q = DirectX::XMQuaternionSlerp(q1, q2, t);
        }

        DirectX::XMFLOAT4 rot;
        DirectX::XMStoreFloat4(&rot, q);
        GetOwner()->SetQuaternionRotation(rot);
        // Lerp FOV
        fovY = src.fov + (dst.fov - src.fov) * t;

        if (t >= 1.f) {
            // 次のターゲットに移行
            pathTime = 0.f;
            currentTarget++;
        }
    }

    static const int MAX_BOOKMARKS = 10;

    bool playingPath = false;
    float pathTime = 0.f;
    int currentTarget = 0;
    float pathDuration = 2.0f; // 秒単位
};


class MovieCameraComponent : public CameraComponent
{
public:
    enum class EaseType
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

    void SetTargetCamera(const std::shared_ptr<TitleCamera>& camera)
    {
        targetCamera = camera;
    }


    float ApplyEase(float t, EaseType type)
    {
        switch (type)
        {
        case EaseType::Linear: return t;
        case EaseType::EaseIn: return t * t;
        case EaseType::EaseOut: return 1 - (1 - t) * (1 - t);
        case EaseType::EaseInOut:
            return t < 0.5f
                ? 2 * t * t
                : 1 - powf(-2 * t + 2, 2) / 2;
        }
        return t;
    }

    std::string EaseToString(EaseType e)
    {
        switch (e)
        {
        case EaseType::Linear: return "Linear";
        case EaseType::EaseIn: return "EaseIn";
        case EaseType::EaseOut: return "EaseOut";
        case EaseType::EaseInOut: return "EaseInOut";
        }
        return "Linear";
    }

    EaseType StringToEase(const std::string& s)
    {
        if (s == "EaseIn") return EaseType::EaseIn;
        if (s == "EaseOut") return EaseType::EaseOut;
        if (s == "EaseInOut") return EaseType::EaseInOut;
        return EaseType::Linear;
    }

    MovieCameraComponent(const std::string& name, const std::shared_ptr<Actor>& owner)
        : CameraComponent(name, owner)
    {
    }

    void Tick(float deltaTime) override
    {
        if (!useMovieCamera)
            return;

        // 手動操作
        if (manualControl)
        {
            HandleKeyboardInput(deltaTime);
            HandleMouseInput(deltaTime);
        }

        // ムービー再生
        if (playing)
        {
            UpdatePath(deltaTime);
        }
    }

    void SetIsUseMovie(const bool useMovie) { this->useMovieCamera = useMovie; }

    // 最初のフレームを適応する
    void ApplyFirstFrame();
    // 最後のフレームを適応する
    void ApplyLastFrame();

    void SaveToJson(const std::string& path);

    void LoadFromJson(const std::string& path);
    void RefreshMovieFiles();

    void DrawImGuiInspector()override
    {
#ifdef USE_IMGUI

        SceneComponent::DrawImGuiInspector();

        if (ImGui::Button("Add Key"))
        {
            CameraKeyframe k;
            k.position = GetComponentLocation();
            k.rotation = GetComponentRotation();
            k.fov = fovY;
            keys.push_back(k);
        }

        char fileBuf[128];
        strncpy_s(fileBuf, currentFile.c_str(), sizeof(fileBuf));

        if (ImGui::InputText("File", fileBuf, sizeof(fileBuf)))
        {
            currentFile = fileBuf;
        }

        ImGui::SameLine();
        if (ImGui::Button("Play"))
        {
            Start();
        }
        if (ImGui::Button("ReversePlay"))
        {
            Start(true);
        }
        if (ImGui::Button("Save JSON"))
        {
            SaveToJson(basePath + currentFile);
        }
        if (ImGui::Button("Load JSON"))
        {
            LoadFromJson(basePath + currentFile);
        }

        if (ImGui::Button("Refresh Files"))
        {
            RefreshMovieFiles();

        }
        ImGui::SameLine();

        if (ImGui::Button("New"))
        {
            keys.clear();
            currentFile = "new_movie.json";
        }

        for (auto& file : movieFiles)
        {
            if (ImGui::Selectable(file.c_str()))
            {
                currentFile = file;
                LoadFromJson(basePath + file);
            }
        }

        for (int i = 0; i < keys.size(); i++)
        {
            ImGui::PushID(i);

            auto& k = keys[i];
            // 名前を編集できる
            char nameBuf[64];
            strncpy_s(nameBuf, k.name.c_str(), sizeof(nameBuf));
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
            {
                k.name = nameBuf; // 入力変更を反映
            }
            ImGui::DragFloat("Duration", &k.duration, 0.1f, 0.1f, 10.0f);

            const char* easeItems[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut" };
            int easeIndex = (int)k.ease;
            if (ImGui::Combo("Ease", &easeIndex, easeItems, IM_ARRAYSIZE(easeItems)))
            {
                k.ease = static_cast<EaseType>(easeIndex);
            }

            if (ImGui::Button("Set From Current"))
            {
                k.position = GetComponentLocation();
                k.rotation = GetComponentRotation();
                k.fov = fovY;
            }

            ImGui::Separator();
            ImGui::PopID();
        }
#endif
    }

    // =========================
    void Start(bool reverse = false);

private:
    void HandleKeyboardInput(float deltaTime);
    void HandleMouseInput(float deltaTime)
    {
#if 1
        if (InputSystem::GetInputState("MouseRight"))
        {
            int dx, dy;
            InputSystem::GetMouseDelta(dx, dy);

            AddYaw(dx * rotateSpeed);
            AddPitch(dy * rotateSpeed);

            //yaw += dx * rotateSpeed;
            //pitch += dy * rotateSpeed;

            // 上下向きすぎ防止（重要）
            pitch = std::clamp(pitch, -DirectX::XM_PIDIV2 + 0.01f, DirectX::XM_PIDIV2 - 0.01f);
        }
        using namespace DirectX;

        // ワールドY軸Yaw
        XMVECTOR qYaw = XMQuaternionRotationAxis(
            XMVectorSet(0, 1, 0, 0),
            yaw
        );

        // ローカルX軸Pitch
        XMVECTOR qPitch = XMQuaternionRotationAxis(
            XMVectorSet(1, 0, 0, 0),
            pitch
        );

        // 合成順序：Yaw → Pitch
        XMVECTOR q = XMQuaternionNormalize(
            XMQuaternionMultiply(qPitch, qYaw)
        );

        XMFLOAT4 rot;
        XMStoreFloat4(&rot, q);
        GetOwner()->SetQuaternionRotation(rot);
        return;
#endif // 0


        if (InputSystem::GetInputState("MouseRight"))
        {
            int dx, dy;
            InputSystem::GetMouseDelta(dx, dy);


            float yawDelta = dx * rotateSpeed;
            float pitchDelta = dy * rotateSpeed;

            using namespace DirectX;

            //XMFLOAT4 rot = GetComponentRotation();
            XMFLOAT4 rot = GetOwner()->GetQuaternionRotation();
            XMVECTOR q = XMLoadFloat4(&rot);

            XMVECTOR qYaw = XMQuaternionRotationAxis(
                XMVectorSet(0, 1, 0, 0),
                yawDelta
            );

            XMVECTOR right = XMVector3Rotate(
                XMVectorSet(1, 0, 0, 0),
                q
            );

            XMVECTOR qPitch = XMQuaternionRotationAxis(
                right,
                pitchDelta
            );

            q = XMQuaternionMultiply(q, qYaw);
            q = XMQuaternionMultiply(q, qPitch);

            q = XMQuaternionNormalize(q);

            XMStoreFloat4(&rot, q);
            GetOwner()->SetQuaternionRotation(rot);
        }
    }

    std::vector<CameraKeyframe> keys;

    int currentIndex = 0;
    float time = 0.f;
    bool playing = false;


    // =========================
    void UpdatePath(float dt);

    bool useMovieCamera = false;
    float moveSpeed = 5.0f;
    float rotateSpeed = 0.001f;
    bool holdLastFrame = true;  // 最後のフレームを保持する
    bool finished = false;  // 終わったかどうか
    bool reversePlay = false;   // 逆再生するかどうか
    bool manualControl = true;

    std::string currentFile = "intro.json";
    std::string basePath = "./Data/Saves/MovieCameras/";
    std::vector<std::string> movieFiles;

    std::weak_ptr<TitleCamera> targetCamera;
};

#endif //CAMERA_COMPONENT_H