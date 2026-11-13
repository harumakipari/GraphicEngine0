#include "pch.h"
#include "CameraComponent.h"

#include "Core/Actor.h"
#include "Physics/CollisionFunction.h"
#include "Game/Actors/Camera/Camera.h"
#include <json.hpp>
#include <fstream>


const DirectX::XMFLOAT4X4& CameraComponent::GetView()
{
    using namespace DirectX;

    XMFLOAT3 pos = GetComponentLocation();

    XMFLOAT4 rot = GetComponentRotation();

    XMVECTOR eye = XMLoadFloat3(&pos);
    XMVECTOR q = XMLoadFloat4(&rot);

    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0),q);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMVECTOR focus;
    if (useLookTarget)
    {
        focus = XMLoadFloat3(&lookTarget);
    }
    else
    {
        focus = eye + forward;
    }

    XMStoreFloat4x4(&view,XMMatrixLookAtLH(eye,focus,up));
    return view;
}

DirectX::XMFLOAT3 CameraComponent::GetForward()const
{
    using namespace DirectX;

    XMFLOAT3 pos = GetComponentLocation();

    if (useLookTarget)
    {
        return MathHelper::Normalize(MathHelper::Subtract(lookTarget, pos));
    }

    XMFLOAT4 rot = GetComponentRotation();

    XMVECTOR q = XMLoadFloat4(&rot);

    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0),q);

    XMFLOAT3 result;
    XMStoreFloat3(&result, forward);

    return MathHelper::Normalize(result);
}

DirectX::XMFLOAT3 CameraComponent::GetRight() const
{
    DirectX::XMFLOAT3 up = { 0,1,0 };

    return MathHelper::Normalize(MathHelper::Cross(up, GetForward()));
}


ViewConstants CameraComponent::GetViewConstants()
{
    ViewConstants vc;

    vc.view = GetView();
    vc.projection = GetProjection();


    using namespace DirectX;

    XMMATRIX V = XMLoadFloat4x4(&vc.view);
    XMMATRIX P = XMLoadFloat4x4(&vc.projection);

    XMStoreFloat4x4(&vc.viewProjection, V * P);
    XMStoreFloat4x4(&vc.invView, XMMatrixInverse(nullptr, V));
    XMStoreFloat4x4(&vc.invProjection, XMMatrixInverse(nullptr, P));
    XMStoreFloat4x4(&vc.invViewProjection, XMMatrixInverse(nullptr, V * P));

    // 前フレームのviewProjectionを入れる
    vc.previousViewProjection = previousViewProjection;
    previousViewProjection = vc.viewProjection;

    if (firstFrame)
    {
        previousViewProjection = vc.viewProjection;
        firstFrame = false;
    }

    vc.cameraPosition =
    {
        vc.invView._41,
        vc.invView._42,
        vc.invView._43,
        1.0f
    };
    XMFLOAT3 pos = GetComponentLocation();

    vc.cameraPosition =
    {
        pos.x,
        pos.y,
        pos.z,
        1.0f
    };
    vc.cameraClipDistance =
    {
        nearZ,
        farZ,
        nearZ * farZ,
        farZ - nearZ
    };

    return vc;
}

// CameraActorのrotationを更新する
void CameraComponent::UpdateRotationFromYawPitch()
{
    using namespace DirectX;

    XMVECTOR qYaw = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), yaw);
    XMVECTOR right = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), qYaw);
    XMVECTOR qPitch = XMQuaternionRotationAxis(right, pitch);
    XMVECTOR q = XMQuaternionNormalize(XMQuaternionMultiply(qPitch, qYaw));
    XMFLOAT4 rot;
    XMStoreFloat4(&rot, q);

    GetOwner()->SetQuaternionRotation(rot);
}

void CameraComponent::AddYaw(const float v)
{
    yaw += v;
    UpdateRotationFromYawPitch();
}

void CameraComponent::AddPitch(const float v)
{
    pitch += v;

    pitch = std::clamp(pitch,DirectX::XMConvertToRadians(minPitchDegree),DirectX::XMConvertToRadians(maxPitchDegree));
    UpdateRotationFromYawPitch();
}


void TPSCameraComponent::DrawImGuiInspector() 
{
#ifdef USE_IMGUI
    SceneComponent::DrawImGuiInspector();
    if (ImGui::TreeNode((name + "  camera").c_str()))
    {
        ImGui::DragFloat3(U8("注視点のオフセット"), &targetOffset.x, 0.1f);
        ImGui::DragFloat3(U8("カメラの位置のオフセット"), &cameraOffset.x, 0.1f);
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
        ImGui::TreePop();
    }
#endif
}


DirectX::XMVECTOR TPSCameraComponent::ResolveCameraCollision(DirectX::FXMVECTOR focus, DirectX::FXMVECTOR idealEye)
{
    using namespace DirectX;

    XMFLOAT3 f, e;
    XMStoreFloat3(&f, focus);
    XMStoreFloat3(&e, idealEye);

    HitResultWithActor hit;
    uint32_t mask =
        CollisionHelper::ToBit(CollisionLayer::WorldStatic) |
        CollisionHelper::ToBit(CollisionLayer::Floor) |
        CollisionHelper::ToBit(CollisionLayer::WorldProps);
    if (CollisionFunction::SphereRayCast(
        f,
        e,
        hit,
        0.35f, //
        mask))
    {
        XMVECTOR h = XMLoadFloat3(&hit.hitPoint);
        XMVECTOR n = XMLoadFloat3(&hit.normal);

        // 少し手前に出す
        return h + XMVectorScale(n, 0.05f);
    }

    return idealEye;
}

void DebugCameraComponent::Tick(const float deltaTime)
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


void DebugCameraComponent::HandleKeyboardInput(float deltaTime)
{
    using namespace DirectX;
    XMFLOAT4 rotation = GetComponentRotation();
    XMVECTOR q = XMLoadFloat4(&rotation);

    XMVECTOR forward = XMVector3Rotate(
        XMVectorSet(0, 0, 1, 0), q);

    XMVECTOR right = XMVector3Rotate(
        XMVectorSet(1, 0, 0, 0), q);

    XMVECTOR up = XMVector3Rotate(
        XMVectorSet(0, 1, 0, 0), q);
    DirectX::XMVECTOR move = DirectX::XMVectorZero();
#ifdef USE_IMGUI

    if (float wheelDelta = ImGui::GetIO().MouseWheel)
    {
        move += forward * wheelDelta * 30.0f;
    }
#endif
    if (InputSystem::GetInputState("W")) { move += forward; }
    if (InputSystem::GetInputState("S")) { move -= forward; }
    if (InputSystem::GetInputState("D")) { move += right; }
    if (InputSystem::GetInputState("A")) { move -= right; }
    //
    if (InputSystem::GetInputState("E")) { move += up; }
    if (InputSystem::GetInputState("Q")) { move -= up; }

    if (InputSystem::GetInputState("Shift")) { move = DirectX::XMVectorScale(move, 2.5f); }

    move = DirectX::XMVectorScale(move, moveSpeed * deltaTime);

    DirectX::XMFLOAT3 position = GetComponentLocation();
    DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&position);
    pos += move;
    DirectX::XMFLOAT3 positionLocal{};
    DirectX::XMStoreFloat3(&positionLocal, pos);
    GetOwner()->SetPosition(positionLocal);
}

void DebugCameraComponent::HandleMouseInput(float deltaTime)
{
    if (InputSystem::GetInputState("MouseRight"))
    {
        int dx, dy;
        InputSystem::GetMouseDelta(dx, dy);

        AddYaw(dx * rotateSpeed);
        AddPitch(dy * rotateSpeed);

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
}

void DebugCameraComponent::DrawImGuiInspector() 
{
#ifdef USE_IMGUI
    SceneComponent::DrawImGuiInspector();
    if (ImGui::TreeNode((name + "  camera").c_str()))
    {
        ImGui::Checkbox("useDebug", &useDebug);
        ImGui::DragFloat("moveSpeed", &moveSpeed, 0.1f);
        ImGui::DragFloat("rotateSpeed", &rotateSpeed, 0.1f);

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

        ImGui::TreePop();
    }
#endif
}

void CinematicCameraComponent::Tick(const float deltaTime)
{
    if (!useCinematic) return;
    if (poseBlending)
    {
        UpdatePoseBlend(deltaTime);
        return;
    }
    if (cinematicCameraDebugInputEnabled)
    {
        HandleKeyboardInput(deltaTime);
        HandleMouseInput(deltaTime);
    }
    if (playingPath) {
        UpdateCameraPath(deltaTime);
    }
}

void CinematicCameraComponent::CutToPose(const CameraPose& pose)
{
    poseBlending = false;
    playingPath = false;
    poseBlendElapsed = 0.0f;
    poseBlendDuration = 0.0f;
    ApplyPose(pose);
}

void CinematicCameraComponent::BlendToPose(const CameraPose& pose, float duration)
{
    if (duration <= 0.0f)
    {
        CutToPose(pose);
        return;
    }

    auto owner = GetOwner();
    if (!owner)
        return;

    poseBlendStart.position = owner->GetPosition();
    poseBlendStart.rotation = owner->GetQuaternionRotation();
    poseBlendStart.fov = GetFov();
    poseBlendTarget = pose;
    poseBlendElapsed = 0.0f;
    poseBlendDuration = duration;
    playingPath = false;
    poseBlending = true;
}

// 保存関数
void CinematicCameraComponent::SaveBookmarksToFile()
{
    using json = nlohmann::json;
    json j;

    for (auto& b : bookmarks)
    {
        json item;

        item["name"] = b.name;

        item["pos"] = { b.position.x, b.position.y, b.position.z };
        item["rotation"] = { b.rotation.x, b.rotation.y, b.rotation.z,b.rotation.w };
        item["yaw"] = b.yaw;
        item["pitch"] = b.pitch;
        item["fov"] = b.fov;

        j["bookmarks"].push_back(item);
    }

    std::ofstream file("./Data/Saves/CameraBookmarks/CinematicCamera.json");
    file << j.dump(4); // インデント付き
}
// 読み込み関数
void CinematicCameraComponent::LoadBookmarksFromFile()
{
    using json = nlohmann::json;

    std::ifstream file("./Data/Saves/CameraBookmarks/CinematicCamera.json");

    if (!file.is_open())
        return;

    json j;
    file >> j;

    bookmarks.clear();

    if (!j.contains("bookmarks")) return;

    for (auto& item : j["bookmarks"])
    {
        CameraBookmark b{};
        b.name = item.value("name", "Bookmark");
        b.position.x = item["pos"][0];
        b.position.y = item["pos"][1];
        b.position.z = item["pos"][2];
        b.rotation.x = item["rotation"][0];
        b.rotation.y = item["rotation"][1];
        b.rotation.z = item["rotation"][2];
        b.rotation.w = item["rotation"][3];
        b.yaw = item["yaw"];
        b.pitch = item["pitch"];
        b.fov = item["fov"];

        bookmarks.push_back(b);
    }
}

void CinematicCameraComponent::HandleKeyboardInput(float deltaTime)
{
    using namespace DirectX;
    XMFLOAT4 rotation = GetComponentRotation();
    XMVECTOR q = XMLoadFloat4(&rotation);

    XMVECTOR forward = XMVector3Rotate(
        XMVectorSet(0, 0, 1, 0), q);

    XMVECTOR right = XMVector3Rotate(
        XMVectorSet(1, 0, 0, 0), q);

    XMVECTOR up = XMVector3Rotate(
        XMVectorSet(0, 1, 0, 0), q);
    DirectX::XMVECTOR move = DirectX::XMVectorZero();
#ifdef USE_IMGUI

    if (float wheelDelta = ImGui::GetIO().MouseWheel)
    {
        fovY -= wheelDelta * 0.03f;
        fovY = std::clamp(fovY,XMConvertToRadians(10.f),XMConvertToRadians(90.f));
    }
#endif
    if (InputSystem::GetInputState("W")) { move += forward; }
    if (InputSystem::GetInputState("S")) { move -= forward; }
    if (InputSystem::GetInputState("D")) { move += right; }
    if (InputSystem::GetInputState("A")) { move -= right; }
    //
    if (InputSystem::GetInputState("E")) { move += up; }
    if (InputSystem::GetInputState("Q")) { move -= up; }

    if (InputSystem::GetInputState("Shift")) { move = DirectX::XMVectorScale(move, 2.5f); }

    move = DirectX::XMVectorScale(move, moveSpeed * deltaTime);

    DirectX::XMFLOAT3 position = GetComponentLocation();
    DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&position);
    pos += move;
    DirectX::XMFLOAT3 positionLocal{};
    DirectX::XMStoreFloat3(&positionLocal, pos);

    GetOwner()->SetPosition(positionLocal);
}

void CinematicCameraComponent::ApplyPose(const CameraPose& pose)
{
    auto owner = GetOwner();
    if (!owner)
        return;

    using namespace DirectX;
    XMVECTOR rotation = XMLoadFloat4(&pose.rotation);
    if (XMVector4Equal(rotation, XMVectorZero()))
        rotation = XMQuaternionIdentity();
    rotation = XMQuaternionNormalize(rotation);

    XMFLOAT4 normalizedRotation{};
    XMStoreFloat4(&normalizedRotation, rotation);
    owner->SetPosition(pose.position);
    owner->SetQuaternionRotation(normalizedRotation);
    owner->UpdateAllComponentTransforms();
    SetFov(pose.fov);
    useLookTarget = false;
    SyncYawPitchFromRotation(normalizedRotation);
}

void CinematicCameraComponent::UpdatePoseBlend(float deltaTime)
{
    poseBlendElapsed += (std::max)(deltaTime, 0.0f);
    const float t = std::clamp(
        poseBlendElapsed / (std::max)(poseBlendDuration, FLT_EPSILON),
        0.0f, 1.0f);

    CameraPose pose{};
    pose.position =
    {
        std::lerp(poseBlendStart.position.x, poseBlendTarget.position.x, t),
        std::lerp(poseBlendStart.position.y, poseBlendTarget.position.y, t),
        std::lerp(poseBlendStart.position.z, poseBlendTarget.position.z, t)
    };

    using namespace DirectX;
    XMVECTOR startRotation = XMLoadFloat4(&poseBlendStart.rotation);
    XMVECTOR targetRotation = XMLoadFloat4(&poseBlendTarget.rotation);
    if (XMVector4Equal(startRotation, XMVectorZero()))
        startRotation = XMQuaternionIdentity();
    if (XMVector4Equal(targetRotation, XMVectorZero()))
        targetRotation = XMQuaternionIdentity();
    startRotation = XMQuaternionNormalize(startRotation);
    targetRotation = XMQuaternionNormalize(targetRotation);
    if (XMVectorGetX(XMVector4Dot(startRotation, targetRotation)) < 0.0f)
        targetRotation = XMVectorNegate(targetRotation);
    XMStoreFloat4(&pose.rotation,
                  XMQuaternionSlerp(startRotation, targetRotation, t));
    pose.fov = std::lerp(poseBlendStart.fov, poseBlendTarget.fov, t);

    if (t >= 1.0f)
    {
        ApplyPose(poseBlendTarget);
        poseBlending = false;
        return;
    }

    ApplyPose(pose);
}

void CinematicCameraComponent::SyncYawPitchFromRotation(
    const DirectX::XMFLOAT4& rotation)
{
    using namespace DirectX;
    const XMVECTOR forward = XMVector3Rotate(
        XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
        XMQuaternionNormalize(XMLoadFloat4(&rotation)));
    const float forwardX = XMVectorGetX(forward);
    const float forwardY = XMVectorGetY(forward);
    const float forwardZ = XMVectorGetZ(forward);
    yaw = atan2f(forwardX, forwardZ);
    pitch = atan2f(-forwardY,
                   sqrtf(forwardX * forwardX + forwardZ * forwardZ));
}

void CinematicCameraComponent::HandleMouseInput(float deltaTime)
{
    if (!InputSystem::GetInputState("MouseRight"))
        return;

    int dx, dy;
    InputSystem::GetMouseDelta(dx, dy);

    AddYaw(dx * rotateSpeed);
    AddPitch(dy * rotateSpeed);

    // 上下向きすぎ防止（重要）
    pitch = std::clamp(pitch, -DirectX::XM_PIDIV2 + 0.01f, DirectX::XM_PIDIV2 - 0.01f);

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
}

void CinematicCameraComponent::DrawImGuiInspector() 
{
#ifdef USE_IMGUI

    SceneComponent::DrawImGuiInspector();
    if (ImGui::TreeNode((name + "  camera").c_str()))
    {
        ImGui::Checkbox("useCinematic", &useCinematic);
        ImGui::Checkbox("Cinematic Camera Debug Input", &cinematicCameraDebugInputEnabled);
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
            UpdateRotationFromYawPitch();
        }
        if (ImGui::DragFloat("pitch (deg)", &pitchDeg, 0.5f))
        {
            pitch = DirectX::XMConvertToRadians(pitchDeg);
            UpdateRotationFromYawPitch();
        }
        ImGui::SliderFloat("nearZ", &nearZ, 0.01f, 100.0f);
        ImGui::DragFloat("farZ", &farZ, 0.1f);

        if (bookmarks.empty())
        {
            ImGui::TextUnformatted("No bookmarks.");
        }
        else
        {
            const int bookmarkCount = static_cast<int>((std::min)(bookmarks.size(), static_cast<size_t>(MAX_BOOKMARKS)));
            for (int i = 0; i < bookmarkCount; ++i)
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
        }

        if (ImGui::Button("Play Path")) {
            StartCameraPath();
        }

        ImGui::TreePop();
    }
#endif
}

void CinematicCameraComponent::SaveBookmark(const int i)
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

void CinematicCameraComponent::LoadBookmark(const int i)
{
    if (i >= bookmarks.size()) return;

    auto& b = bookmarks[i];

    GetOwner()->SetPosition(b.position);

    yaw = b.yaw;
    pitch = b.pitch;
    fovY = b.fov;

    UpdateRotationFromYawPitch();
}

void CinematicCameraComponent::StartCameraPath()
{
    if (bookmarks.empty()) return;
    poseBlending = false;
    playingPath = true;
    pathTime = 0.f;
    currentTarget = 1; // 1つ目のブックマークに向かう
}

void CinematicCameraComponent::UpdateCameraPath(const float deltaTime)
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
    if (DirectX::XMQuaternionEqual(q1, q2)) 
    {
        q = q1;
    }
    else
    {
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

void MovieCameraComponent::Tick(const float deltaTime)
{
    if (!useMovieCamera)
        return;

    if (playing)
    {
        UpdatePath(deltaTime);
    }
    else if (debugCameraMovementEnabled)
    {
        HandleKeyboardInput(deltaTime);
        HandleMouseInput(deltaTime);
    }
}

void MovieCameraComponent::CutToWorldPose(
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT4& rotation,
    float fov)
{
    auto owner = GetOwner();
    if (!owner)
        return;

    ApplyWorldPose(position, rotation, fov);
    time = 0.0f;
    currentIndex = 0;
    playing = false;
    finished = false;
}


void MovieCameraComponent::ApplyWorldPose(
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT4& rotation,
    float fov)
{
    auto owner = GetOwner();
    if (!owner)
        return;

    using namespace DirectX;
    XMVECTOR quaternion = XMLoadFloat4(&rotation);
    if (XMVector4Equal(quaternion, XMVectorZero()))
        quaternion = XMQuaternionIdentity();
    quaternion = XMQuaternionNormalize(quaternion);

    XMFLOAT4 normalizedRotation{};
    XMStoreFloat4(&normalizedRotation, quaternion);

    owner->SetPosition(position);
    owner->SetQuaternionRotation(normalizedRotation);
    owner->UpdateAllComponentTransforms();
    SetFov(fov);
    useLookTarget = false;
    SyncYawPitchFromRotation(normalizedRotation);
}

float MovieCameraComponent::GetPlaybackElapsedTime() const
{
    if (reversePlay || keys.size() < 2)
        return 0.0f;

    size_t completedSegments = currentIndex > 0
                                   ? static_cast<size_t>(currentIndex) : 0;
    completedSegments = (std::min)(completedSegments, keys.size() - 1);

    float elapsed = 0.0f;
    for (size_t index = 0; index < completedSegments; ++index)
        elapsed += (std::max)(keys[index].duration, 0.01f);

    if (currentIndex >= 0 && static_cast<size_t>(currentIndex) < keys.size() - 1)
        elapsed += time;
    return elapsed;
}

bool MovieCameraComponent::TryGetKeyframeStartTime(const std::string& keyName, float& outStartTime) const
{
    float elapsed = 0.0f;
    for (const auto& key : keys)
    {
        if (key.name == keyName)
        {
            outStartTime = elapsed;
            return true;
        }
        elapsed += (std::max)(key.duration, 0.01f);
    }
    return false;
}

bool MovieCameraComponent::TryGetKeyframeStartTime(const size_t keyIndex, float& outStartTime) const
{
    if (keyIndex >= keys.size()) return false;
    outStartTime = 0.0f;
    for (size_t index = 0; index < keyIndex; ++index)
        outStartTime += (std::max)(keys[index].duration, 0.01f);
    return true;
}

void MovieCameraComponent::JumpToKey(const size_t keyIndex)
{
    if (keyIndex >= keys.size())
        return;

    Stop();
    reversePlay = false;
    currentIndex = static_cast<int>(keyIndex);
    ApplyWorldPose(keys[keyIndex].position, keys[keyIndex].rotation, keys[keyIndex].fov);
}

void MovieCameraComponent::SaveToJson(const std::string& path)
{
    using json = nlohmann::json;
    json j;

    for (auto& k : keys)
    {
        json item;

        item["name"] = k.name;

        item["pos"] = { k.position.x, k.position.y, k.position.z };
        item["rot"] = { k.rotation.x, k.rotation.y, k.rotation.z, k.rotation.w };

        item["fov"] = k.fov;
        item["duration"] = k.duration;
        item["ease"] = EaseToString(k.ease);

        j["keys"].push_back(item);
    }

    std::ofstream file(path);
    file << j.dump(4);
}

void MovieCameraComponent::LoadFromJson(const std::string& path)
{
    using json = nlohmann::json;

    std::ifstream file(path);
    if (!file.is_open()) return;

    json j;
    file >> j;

    keys.clear();
    selectedKeyIndex = 0;

    if (!j.contains("keys")) return;

    for (auto& item : j["keys"])
    {
        CameraKeyframe k{};

        k.name = item.value("name", "");

        auto pos = item["pos"];
        k.position = { pos[0], pos[1], pos[2] };

        auto rot = item["rot"];
        k.rotation = { rot[0], rot[1], rot[2], rot[3] };

        k.fov = item.value("fov", DirectX::XMConvertToRadians(60.f));
        k.duration = item.value("duration", 2.0f);
        k.ease = StringToEase(item.value("ease", "Linear"));

        keys.push_back(k);
    }
}

void MovieCameraComponent::RefreshMovieFiles()
{
    movieFiles.clear();

    for (auto& entry : std::filesystem::directory_iterator(basePath))
    {
        if (entry.path().extension() == ".json")
        {
            movieFiles.push_back(entry.path().filename().string());
        }
    }
}

void MovieCameraComponent::DrawImGuiInspector()
{
#ifdef USE_IMGUI

    SceneComponent::DrawImGuiInspector();

    ImGui::Checkbox("Enable Debug Camera Movement", &debugCameraMovementEnabled);
    ImGui::Text("Playback: %s", playing ? "Playing" : "Stopped");

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
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        Stop();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        ResetToFirstKey();
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
        selectedKeyIndex = 0;
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
        ImGui::RadioButton("Selected Key", &selectedKeyIndex, i);
        ImGui::SameLine();
        if (ImGui::Button("Jump To Key"))
        {
            selectedKeyIndex = i;
            JumpToKey(static_cast<size_t>(selectedKeyIndex));
        }
        // 名前を編集できる
        char nameBuf[64];
        strncpy_s(nameBuf, k.name.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
        {
            k.name = nameBuf; // 入力変更を反映
        }
        ImGui::DragFloat3("Position", &k.position.x, 0.01f);

        if (ImGui::DragFloat4("Rotation (Quaternion)", &k.rotation.x, 0.001f))
        {
            using namespace DirectX;
            XMVECTOR rotation = XMLoadFloat4(&k.rotation);
            if (XMVector4Equal(rotation, XMVectorZero()))
                rotation = XMQuaternionIdentity();
            XMStoreFloat4(&k.rotation, XMQuaternionNormalize(rotation));
        }

        float fovDeg = DirectX::XMConvertToDegrees(k.fov);
        if (ImGui::DragFloat("FOV (deg)", &fovDeg, 0.1f, 10.0f, 120.0f))
        {
            k.fov = DirectX::XMConvertToRadians(fovDeg);
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
            using namespace DirectX;
            const XMFLOAT4 currentRotation = GetComponentRotation();
            XMVECTOR rotation = XMLoadFloat4(&currentRotation);
            if (XMVector4Equal(rotation, XMVectorZero()))
                rotation = XMQuaternionIdentity();
            XMStoreFloat4(&k.rotation, XMQuaternionNormalize(rotation));
            k.fov = fovY;
        }

        ImGui::Separator();
        ImGui::PopID();
    }
#endif
}

void MovieCameraComponent::Start(bool reverse)
{
    if (keys.size() < 2) return;
    reversePlay = reverse;

    if (reversePlay)
        currentIndex = static_cast<int>(keys.size()) - 2;
    else
        currentIndex = 0;

    time = 0.f;
    playing = true;
    auto target = targetCamera.lock();

    if (target)
    {
        target->SetUseMovie(true);
    }

    finished = false;
}
bool MovieCameraComponent::ApplyFirstFrameToOwner()
{
    if (keys.empty())
        return false;

    const auto& first = keys.front();
    GetOwner()->SetPosition(first.position);
    GetOwner()->SetQuaternionRotation(first.rotation);
    fovY = first.fov;
    return true;
}

// 現在のカメラ姿勢から仮想targetを計算する
DirectX::XMFLOAT3 MovieCameraComponent::GetVirtualTarget(float distance)
{
    using namespace DirectX;
    // カメラ位置
    XMFLOAT3 position = GetOwner()->GetPosition();
    // カメラ回転
    XMFLOAT4 rotation = GetOwner()->GetQuaternionRotation();


    XMVECTOR q =
        XMLoadFloat4(&rotation);


    q = XMQuaternionNormalize(q);


    // カメラの前方向
    XMVECTOR forward =
        XMVector3Rotate(
            XMVectorSet(0, 0, 1, 0),
            q);


    forward =
        XMVector3Normalize(forward);


    XMFLOAT3 forward3;
    XMStoreFloat3(&forward3, forward);



    XMFLOAT3 target;

    target.x =
        position.x + forward3.x * distance;

    target.y =
        position.y + forward3.y * distance;

    target.z =
        position.z + forward3.z * distance;


    return target;
}

float MovieCameraComponent::ApplyEase(const float t, const EaseType type)
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

std::string MovieCameraComponent::EaseToString(const EaseType e)
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

MovieCameraComponent::EaseType MovieCameraComponent::StringToEase(const std::string& s)
{
    if (s == "EaseIn") return EaseType::EaseIn;
    if (s == "EaseOut") return EaseType::EaseOut;
    if (s == "EaseInOut") return EaseType::EaseInOut;
    return EaseType::Linear;
}

void MovieCameraComponent::SyncYawPitchFromRotation(
    const DirectX::XMFLOAT4& rotation)
{
    using namespace DirectX;
    const XMVECTOR forward = XMVector3Rotate(
        XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
        XMQuaternionNormalize(XMLoadFloat4(&rotation)));
    const float forwardX = XMVectorGetX(forward);
    const float forwardY = XMVectorGetY(forward);
    const float forwardZ = XMVectorGetZ(forward);
    yaw = atan2f(forwardX, forwardZ);
    pitch = atan2f(-forwardY,
                   sqrtf(forwardX * forwardX + forwardZ * forwardZ));
}

void MovieCameraComponent::HandleKeyboardInput(float deltaTime)
{
    using namespace DirectX;
    XMFLOAT4 rotation = GetComponentRotation();
    XMVECTOR q = XMLoadFloat4(&rotation);

    XMVECTOR forward = XMVector3Rotate(
        XMVectorSet(0, 0, 1, 0), q);

    XMVECTOR right = XMVector3Rotate(
        XMVectorSet(1, 0, 0, 0), q);

    XMVECTOR up = XMVector3Rotate(
        XMVectorSet(0, 1, 0, 0), q);
    DirectX::XMVECTOR move = DirectX::XMVectorZero();
#ifdef USE_IMGUI
    if (float wheelDelta = ImGui::GetIO().MouseWheel)
    {
        fovY -= wheelDelta * 0.03f;
        fovY = std::clamp(
            fovY,
            XMConvertToRadians(10.f),
            XMConvertToRadians(90.f));
    }
#endif
    if (InputSystem::GetInputState("W")) { move += forward; }
    if (InputSystem::GetInputState("S")) { move -= forward; }
    if (InputSystem::GetInputState("D")) { move += right; }
    if (InputSystem::GetInputState("A")) { move -= right; }
    if (InputSystem::GetInputState("E")) { move += up; }
    if (InputSystem::GetInputState("Q")) { move -= up; }
    if (InputSystem::GetInputState("Shift")) { move = DirectX::XMVectorScale(move, 2.5f); }

    move = DirectX::XMVectorScale(move, moveSpeed * deltaTime);
    DirectX::XMFLOAT3 position = GetComponentLocation();
    DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&position);
    pos += move;
    DirectX::XMFLOAT3 positionLocal{};
    DirectX::XMStoreFloat3(&positionLocal, pos);
    GetOwner()->SetPosition(positionLocal);
}

void MovieCameraComponent::HandleMouseInput(float deltaTime)
{
    if (InputSystem::GetInputState("MouseRight"))
    {
        int dx, dy;
        InputSystem::GetMouseDelta(dx, dy);

        AddYaw(dx * rotateSpeed);
        AddPitch(dy * rotateSpeed);

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
}

void MovieCameraComponent::UpdatePath(float dt)
{
    bool reachedEnd =
        (!reversePlay && currentIndex >= keys.size() - 1) ||
        (reversePlay && currentIndex < 0);

    if (reachedEnd)
    {
        playing = false;
        finished = true;

        auto target = targetCamera.lock();

        if (target)
        {
            target->SetUseMovie(false);
        }

        auto& last = reversePlay
                         ? keys.front()
                         : keys.back();

        GetOwner()->SetPosition(last.position);
        GetOwner()->SetQuaternionRotation(last.rotation);

        fovY = last.fov;

        return;
    }

    if (onMovieStart)
    {
        onMovieStart();
        onMovieStart = nullptr;
    }

    auto& a = reversePlay ? keys[currentIndex + 1] : keys[currentIndex];
    auto& b = reversePlay ? keys[currentIndex] : keys[currentIndex + 1];

    float duration = std::max<float>(a.duration, 0.01f);

    time += dt;
    float t = std::clamp(time / duration, 0.f, 1.f);
    float eased = ApplyEase(t, a.ease);

    // -------- Position --------
    DirectX::XMFLOAT3 pos;
    pos.x = a.position.x + (b.position.x - a.position.x) * eased;
    pos.y = a.position.y + (b.position.y - a.position.y) * eased;
    pos.z = a.position.z + (b.position.z - a.position.z) * eased;
    GetOwner()->SetPosition(pos);

    // -------- Rotation (安全版SLerp) --------
    using namespace DirectX;

    XMVECTOR q1 = XMLoadFloat4(&a.rotation);
    XMVECTOR q2 = XMLoadFloat4(&b.rotation);

    if (XMVector4Equal(q1, XMVectorZero())) q1 = XMQuaternionIdentity();
    if (XMVector4Equal(q2, XMVectorZero())) q2 = XMQuaternionIdentity();

    q1 = XMQuaternionNormalize(q1);
    q2 = XMQuaternionNormalize(q2);

    float dot = XMVectorGetX(XMVector4Dot(q1, q2));
    if (dot < 0.f) q2 = XMVectorNegate(q2);

    XMVECTOR q = XMQuaternionSlerp(q1, q2, eased);

    XMFLOAT4 rot;
    XMStoreFloat4(&rot, q);
    GetOwner()->SetQuaternionRotation(rot);

    // -------- FOV --------
    fovY = a.fov + (b.fov - a.fov) * eased;

    // -------- 次へ --------
    if (t >= 1.f)
    {
        time = 0.f;

        if (reversePlay)
        {
            currentIndex--;

        }
        else
        {
            currentIndex++;
        }
    }
}
