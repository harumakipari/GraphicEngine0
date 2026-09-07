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

DirectX::XMFLOAT3 CameraComponent::GetForward()const
{
    using namespace DirectX;

    XMFLOAT3 pos = GetComponentLocation();

    if (useLookTarget)
    {
        return MathHelper::Normalize(
            MathHelper::Subtract(lookTarget, pos));
    }

    XMFLOAT4 rot = GetComponentRotation();

    XMVECTOR q = XMLoadFloat4(&rot);

    XMVECTOR forward = XMVector3Rotate(
        XMVectorSet(0, 0, 1, 0),
        q);

    XMFLOAT3 result;
    XMStoreFloat3(&result, forward);

    return MathHelper::Normalize(result);
}

DirectX::XMFLOAT3 CameraComponent::GetRight() const
{
    DirectX::XMFLOAT3 up = { 0,1,0 };

    return MathHelper::Normalize(
        MathHelper::Cross(up, GetForward()));
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



DirectX::XMVECTOR TPSCameraComponent::ResolveCameraCollision(
    DirectX::FXMVECTOR focus, DirectX::FXMVECTOR idealEye)
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
    if (actorRelativeEditMode && !actorRelativeBasis.valid)
    {
        Logger::Warning("Actor Relative Movie save skipped: basis is not set.");
        return;
    }

    using json = nlohmann::json;
    json j;

    if (actorRelativeEditMode)
        j["coordinateSpace"] = "ActorRelative";

    for (auto& k : keys)
    {
        json item;

        DirectX::XMFLOAT3 position = k.position;
        DirectX::XMFLOAT4 rotation = k.rotation;
        if (actorRelativeEditMode)
        {
            using namespace DirectX;
            const XMVECTOR delta = XMLoadFloat3(&k.position) -
                XMLoadFloat3(&actorRelativeBasis.origin);
            const XMVECTOR right = XMLoadFloat3(&actorRelativeBasis.right);
            const XMVECTOR up = XMLoadFloat3(&actorRelativeBasis.up);
            const XMVECTOR forward = XMLoadFloat3(&actorRelativeBasis.forward);
            position =
            {
                XMVectorGetX(XMVector3Dot(delta, right)),
                XMVectorGetX(XMVector3Dot(delta, up)),
                XMVectorGetX(XMVector3Dot(delta, forward))
            };

            const XMVECTOR basis = XMQuaternionNormalize(
                XMLoadFloat4(&actorRelativeBasis.basisRotation));
            const XMVECTOR world = XMQuaternionNormalize(XMLoadFloat4(&k.rotation));
            XMStoreFloat4(&rotation,
                XMQuaternionNormalize(XMQuaternionMultiply(
                    XMQuaternionInverse(basis), world)));
        }

        item["name"] = k.name;

        item["pos"] = { position.x, position.y, position.z };
        item["rot"] = { rotation.x, rotation.y, rotation.z, rotation.w };

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

    const bool isActorRelative =
        j.value("coordinateSpace", "World") == "ActorRelative";
    actorRelativeEditMode = isActorRelative;
    relativeKeysPendingConversion = isActorRelative;
    if (isActorRelative && !suppressRelativeLoadConversion)
    {
        ConvertRelativeKeysToWorld();
    }
}

MovieCameraComponent::ActorRelativeBasis
MovieCameraComponent::CreateActorRelativeBasis(
    const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& forward)
{
    using namespace DirectX;

    ActorRelativeBasis result{};
    result.origin = origin;
    result.forward = forward;
    result.forward.y = 0.0f;
    if (MathHelper::Length(result.forward) <= FLT_EPSILON)
        result.forward = { 0.0f, 0.0f, 1.0f };
    result.forward = MathHelper::Normalize(result.forward);

    result.up = { 0.0f, 1.0f, 0.0f };
    result.right = MathHelper::Cross(result.up, result.forward);
    if (MathHelper::Length(result.right) <= FLT_EPSILON)
        result.right = { 1.0f, 0.0f, 0.0f };
    result.right = MathHelper::Normalize(result.right);

    const float yaw = atan2f(result.forward.x, result.forward.z);
    XMStoreFloat4(&result.basisRotation,
        XMQuaternionNormalize(XMQuaternionRotationAxis(
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), yaw)));
    result.valid = true;
    return result;
}

void MovieCameraComponent::SetActorRelativeBasis(const ActorRelativeBasis& basis)
{
    if (!basis.valid)
    {
        actorRelativeBasis.valid = false;
        return;
    }

    actorRelativeBasis = CreateActorRelativeBasis(
        basis.origin, basis.forward);
    if (actorRelativeBasis.valid)
        ConvertRelativeKeysToWorld();
}

void MovieCameraComponent::ConvertRelativeKeysToWorld()
{
    if (!actorRelativeBasis.valid || !relativeKeysPendingConversion)
        return;

    using namespace DirectX;
    const XMVECTOR origin = XMLoadFloat3(&actorRelativeBasis.origin);
    const XMVECTOR right = XMLoadFloat3(&actorRelativeBasis.right);
    const XMVECTOR up = XMLoadFloat3(&actorRelativeBasis.up);
    const XMVECTOR forward = XMLoadFloat3(&actorRelativeBasis.forward);
    const XMVECTOR basis = XMQuaternionNormalize(
        XMLoadFloat4(&actorRelativeBasis.basisRotation));

    for (auto& key : keys)
    {
        const XMVECTOR local = XMLoadFloat3(&key.position);
        const XMVECTOR world = origin
            + right * XMVectorGetX(local)
            + up * XMVectorGetY(local)
            + forward * XMVectorGetZ(local);
        XMStoreFloat3(&key.position, world);

        const XMVECTOR localRotation = XMQuaternionNormalize(
            XMLoadFloat4(&key.rotation));
        XMStoreFloat4(&key.rotation,
            XMQuaternionNormalize(XMQuaternionMultiply(basis, localRotation)));
    }
    relativeKeysPendingConversion = false;
}

void MovieCameraComponent::LoadFromJsonRelative(
    const std::string& path,
    const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& forward)
{
    suppressRelativeLoadConversion = true;
    LoadFromJson(path);
    suppressRelativeLoadConversion = false;
    actorRelativeEditMode = false;
    relativeKeysPendingConversion = false;

    using namespace DirectX;
    const ActorRelativeBasis basis = CreateActorRelativeBasis(origin, forward);
    const XMVECTOR basisQuaternion = XMQuaternionNormalize(
        XMLoadFloat4(&basis.basisRotation));
    const XMVECTOR rightVector = XMLoadFloat3(&basis.right);
    const XMVECTOR upVector = XMLoadFloat3(&basis.up);
    const XMVECTOR forwardBasisVector = XMLoadFloat3(&basis.forward);
    const XMVECTOR originVector = XMLoadFloat3(&basis.origin);

    for (auto& key : keys)
    {
        const XMVECTOR localPosition = XMLoadFloat3(&key.position);
        const XMVECTOR worldPosition = originVector
            + rightVector * XMVectorGetX(localPosition)
            + upVector * XMVectorGetY(localPosition)
            + forwardBasisVector * XMVectorGetZ(localPosition);
        XMStoreFloat3(&key.position, worldPosition);

        XMVECTOR localQuaternion = XMLoadFloat4(&key.rotation);
        if (XMVectorGetX(XMVector4LengthSq(localQuaternion)) <= FLT_EPSILON)
            localQuaternion = XMQuaternionIdentity();
        localQuaternion = XMQuaternionNormalize(localQuaternion);
        // Apply the authored local rotation after the actor basis.
        XMStoreFloat4(&key.rotation,
            XMQuaternionNormalize(XMQuaternionMultiply(basisQuaternion, localQuaternion)));
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

    owner->SetPosition(position);
    owner->SetQuaternionRotation(rotation);
    // SetQuaternionRotationDirect updates the actor root only.  Refresh the
    // component hierarchy before GetComponentRotation()/GetForward()/GetView()
    // are queried in the same frame.
    owner->UpdateAllComponentTransforms();
    SetFov(fov);
    useLookTarget = false;
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

    //const auto& first = reversePlay ? keys.back() : keys.front();
    //GetOwner()->SetPosition(first.position);
    //GetOwner()->SetQuaternionRotation(first.rotation);
    //SetFov(first.fov);

    finished = false;

    // 再生中は手動禁止
    manualControl = false;
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

        // ムービー終了後は手動操作を禁止する
        // ここをtrueにすると、ムービー終了後に手動操作が可能になる
        // 画角が変わるのを防ぐために、falseにしておく
        manualControl = false;

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
