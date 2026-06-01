#include "pch.h"
#include "CameraComponent.h"

#include "Core/Actor.h"
#include "Physics/CollisionFunction.h"
#include "Game/Actors/Camera/Camera.h"
#include <json.hpp>


const DirectX::XMFLOAT4X4& CameraComponent::GetView()
{
    using namespace DirectX;

    XMFLOAT3 pos = GetComponentLocation();

#if 0
    XMVECTOR eye = XMLoadFloat3(&pos);

    XMVECTOR forward =
        XMVector3Normalize(
            XMVectorSet(
                sinf(yaw) * cosf(pitch),
                sinf(pitch),
                cosf(yaw) * cosf(pitch),
                0));
#else

    XMFLOAT4 rot = GetComponentRotation();

    XMVECTOR eye = XMLoadFloat3(&pos);
    XMVECTOR q = XMLoadFloat4(&rot);

    XMVECTOR forward = XMVector3Rotate(
        XMVectorSet(0, 0, 1, 0),
        q);

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);


#endif // 0
    XMVECTOR focus;
    if (useLookTarget)
    {
        focus = XMLoadFloat3(&lookTarget);
    }
    else
    {
        focus = eye + forward;
    }

    XMStoreFloat4x4(
        &view,
        XMMatrixLookAtLH(
            eye,
            focus,
            up
        ));

    return view;
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



DirectX::XMVECTOR TPSCameraComponent::ResolveCameraCollision(
    DirectX::FXMVECTOR focus,
    DirectX::FXMVECTOR idealEye
)
{
    using namespace DirectX;

    XMFLOAT3 f, e;
    XMStoreFloat3(&f, focus);
    XMStoreFloat3(&e, idealEye);

    HitResultWithActor hit;
    uint32_t mask =
        CollisionHelper::ToBit(CollisionLayer::WorldStatic) |
        CollisionHelper::ToBit(CollisionLayer::Floor);
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

    //SetWorldLocationDirect(positionLocal);

    GetOwner()->SetPosition(positionLocal);

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
        fovY = std::clamp(
            fovY,
            XMConvertToRadians(10.f),
            XMConvertToRadians(90.f));
        //move += forward * wheelDelta * 30.0f;
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

    //SetWorldLocationDirect(positionLocal);

    GetOwner()->SetPosition(positionLocal);

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
        //move += forward * wheelDelta * 30.0f;
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

    //SetWorldLocationDirect(positionLocal);

    GetOwner()->SetPosition(positionLocal);

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

// 最初のフレームを適応する
void MovieCameraComponent::ApplyFirstFrame()
{
    if (keys.empty())
        return;

    auto target = targetCamera.lock();

    if (!target)
        return;

    auto& first = keys.front();

    target->SetPosition(first.position);
    target->SetQuaternionRotation(first.rotation);

    SetFov(first.fov);
}


// 最初のフレームを適応する
void MovieCameraComponent::ApplyLastFrame()
{
    if (keys.empty())
        return;

    auto target = targetCamera.lock();

    if (!target)
        return;

    auto& last = keys.back();

    target->SetPosition(last.position);
    target->SetQuaternionRotation(last.rotation);

    SetFov(last.fov);
}

void MovieCameraComponent::LoadFromJson(const std::string& path)
{
    using json = nlohmann::json;

    std::ifstream file(path);
    if (!file.is_open()) return;

    json j;
    file >> j;

    keys.clear();

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

    // 再生中は手動禁止
    manualControl = false;
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

        // ムービー終了後は手動操作OK
        manualControl = true;

        auto& last = reversePlay
            ? keys.front()
            : keys.back();

        GetOwner()->SetPosition(last.position);
        GetOwner()->SetQuaternionRotation(last.rotation);


        fovY = last.fov;

        return;
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