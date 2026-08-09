#include "pch.h"
#include "DarkGameCamera.h"

#include "Game/Actors/Player/Player.h"
#include "Physics/CollisionFunction.h"

namespace
{
    float SmoothStep01(float value)
    {
        value = std::clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - 2.0f * value);
    }
}

void DarkCameraActor::Initialize(const Transform& transform)
{
    std::string parentName = "DarkCameraActor";
    mainCameraComponent = AddComponent<CameraComponent>(parentName);
    inputComponent = AddComponent<InputComponent>("inputComponent", parentName);

    // カメラを敵方向から何度横へ振るか
    lockOnYawOffsetDegree = -0.0f;
    // targetをどこに置くか
    // 0 = Player
    // 1 = Enemy
    lockOnTargetWeight = 0.72f;
    // 基本距離
    lockOnCameraDistance = 5.0f;
    // 敵との距離による増加量
    lockOnDistanceScale = 0.75f;
    // 最大距離
    lockOnMaxDistance = 8.0f;
    // Pitch
    lockOnPitchDegree = -10.0f;

    isExternalBlending = false;
    ResetLockOnAdaptiveState();
}

void DarkCameraActor::Update(float deltaTime)
{

    // プレイヤーの位置を取得
    auto playerHeadShared = playerHead.lock();
    if (!playerHeadShared)
    {
        return;
    }
    DirectX::XMFLOAT3 playerPos = playerHeadShared->GetComponentLocation();

    if (!isExternalBlending && !isBlending && currentMode == CameraMode::LockOn)
    {
        UpdateLockOnComposition(deltaTime);
    }


    if (isExternalBlending)
    {
        UpdateExternalBlend(deltaTime);
    }
    else if (isBlending)
    {
        UpdateBlend(deltaTime);
    }
    else
    {
        if (requestMode != currentMode)
        {
            StartBlend(currentMode, requestMode);
        }
        else
        {
            UpdateDesireRotation(deltaTime);
            UpdateRotation(deltaTime);
            currentPose = CalculatePose(currentMode, playerPos, currentYaw, currentPitch);
        }
    }

    if (!isExternalBlending)
    {
        const CameraCompositionSettings* fromSettings = &tpsSettings;
        if (currentMode == CameraMode::Focus) fromSettings = &focusSettings;
        if (currentMode == CameraMode::LockOn) fromSettings = &lockOnSettings;

        const float fovDegree = isBlending
            ? std::lerp(blendStartFovDegree, blendTargetFovDegree,
                std::clamp(blendTime / blendDuration, 0.0f, 1.0f))
            : fromSettings->fovDegree;
        mainCameraComponent->SetFov(DirectX::XMConvertToRadians(fovDegree));
    }

#if 0
    if (currentMode == CameraMode::LockOn)
    {
        mainCameraComponent->SetFov(DirectX::XMConvertToRadians(CalculateLockOnFov()));
    }
    else
    {
        mainCameraComponent->SetFov(DirectX::XMConvertToRadians(cameraFov));
    }
#endif // 0

    // 当たり判定前後を保存し、実距離を診断できるようにする。
    desiredEyePosition = currentPose.eye;
    collisionPreEyePosition = currentPose.eye;
    desiredCameraDistance = MathHelper::Distance(currentPose.target, collisionPreEyePosition);

    // 当たり判定でカメラの位置を修正する
    currentPose.eye = ResolveCameraCollision(currentPose.target, currentPose.eye);
    collisionPostEyePosition = currentPose.eye;
    actualCameraDistance = MathHelper::Distance(currentPose.target, collisionPostEyePosition);
    cameraCollisionRatio = desiredCameraDistance > FLT_EPSILON
        ? actualCameraDistance / desiredCameraDistance
        : 1.0f;

    float targetBlend = cameraHitWall ? 1.0f : 0.0f;

    wallBlend = std::lerp(wallBlend, targetBlend, deltaTime * 8.0f);

    SetPosition(currentPose.eye);
    mainCameraComponent->lookTarget = currentPose.target;
    mainCameraComponent->useLookTarget = true;
}

// ブレンドを開始する
void DarkCameraActor::StartBlend(CameraMode from, CameraMode to)
{
    (void)from;
    blendTime = 0.0f;
    isBlending = true;

    // プレイヤーの位置を取得
    auto playerHeadShared = playerHead.lock();
    if (!playerHeadShared)
    {
        return;
    }
    DirectX::XMFLOAT3 playerPos = playerHeadShared->GetComponentLocation();

    // Collision補正を含め、直前まで実際に表示していたPoseから開始する。
    blendStartPose = currentPose;
    blendStartPose.yaw = currentYaw;
    blendStartPose.pitch = currentPitch;
    const DirectX::XMFLOAT3 startEyeDirection = MathHelper::Normalize(
        MathHelper::Subtract(blendStartPose.eye, blendStartPose.target));
    blendStartEyeYaw = atan2f(startEyeDirection.x, startEyeDirection.z);
    blendStartEyePitch = asinf(std::clamp(startEyeDirection.y, -1.0f, 1.0f));
    blendStartEyeDistance = MathHelper::Distance(blendStartPose.eye, blendStartPose.target);
    float targetYaw = currentYaw;
    float targetPitch = currentPitch;
    switch (to)
    {
    case CameraMode::TPS:
        // Blend途中でTPSへ戻しても、古いTPS角度へ巻き戻さない。
        targetYaw = currentYaw;
        targetPitch = currentPitch;
        break;

    case CameraMode::Focus:
    {
        CameraDirectionInfo info = CreateFocusInfo();

        targetYaw = info.yaw;
        targetPitch = info.pitch;
        break;
    }

    case CameraMode::LockOn:
        CameraDirectionInfo info = CreateLockOnInfo();
        targetYaw = info.yaw;
        targetPitch = info.pitch;
        targetDistance = CalculateLockOnDistance();
        break;
    }
    blendTargetPose = CalculatePose(to, playerPos, targetYaw, targetPitch);
    blendStartFovDegree = DirectX::XMConvertToDegrees(mainCameraComponent->GetFov());
    blendTargetFovDegree = tpsSettings.fovDegree;
    if (to == CameraMode::Focus) blendTargetFovDegree = focusSettings.fovDegree;
    if (to == CameraMode::LockOn) blendTargetFovDegree = lockOnSettings.fovDegree;
}

// カメラをplayerのforward方向に向ける
void DarkCameraActor::RotateToPlayerForward()
{
    CameraDirectionInfo info = CreateFocusInfo();

    desiredYaw = info.yaw;
    desiredPitch = info.pitch;

    //blendStartPose = currentPose;
    //blendTime = 0.0f;
    //isBlending = true;
}

// 外部のカメラアクターとのブレンド用の関数
void DarkCameraActor::StartExternalBlend(const CameraPose& start, const CameraPose& target, float duration, std::function<void()> finishExternalBlend)
{
    externalBlendDuration = duration;
    externalStartPose = start;
    externalTargetPose = target;
    isExternalBlending = true;
    externalBlendTime = 0.0f;
    desiredPitch = externalTargetPose.pitch;
    desiredYaw = externalTargetPose.yaw;
    this->finishedExternalBlend = finishExternalBlend;
}

// ムービーカメラコンポーネントからカメラポーズを作成する
DarkCameraActor::CameraPose DarkCameraActor::CreatePoseFromMovie(const std::shared_ptr<MovieCameraComponent>& movieCamera)
{
    using namespace DirectX;
    CameraPose pose{};
    pose.eye = movieCamera->GetOwner()->GetPosition();
    pose.target = movieCamera->GetVirtualTarget(5.0f);
    auto dir = MathHelper::Normalize(MathHelper::Subtract(pose.target, pose.eye));
    pose.yaw = atan2f(dir.x, dir.z);
    pose.pitch = asinf(dir.y);
    return pose;
}

// Focus状態のポーズを作成する
DarkCameraActor::CameraPose DarkCameraActor::CreateFocusPose()
{
    CameraPose pose{};
    auto playerHeadShared = playerHead.lock();
    if (!playerHeadShared)
    {
        return pose;
    }

    auto playerActor = playerHeadShared->GetOwner();
    if (!playerActor)
    {
        return pose;
    }

    // プレイヤー位置
    XMFLOAT3 playerPos = playerHeadShared->GetComponentLocation();
    // プレイヤーの向き
    XMFLOAT3 forward = MathHelper::Normalize(playerActor->GetForward());

    // Focus時のYaw/Pitch
    pose.yaw = atan2f(forward.x, forward.z);
    pose.pitch = 0.0f;

    // TPSカメラ位置を計算
    pose = CalculatePose(CameraMode::TPS, playerPos, pose.yaw, pose.pitch);

    return pose;
}

// 外部のカメラアクターとのブレンド状態を更新する
void DarkCameraActor::UpdateExternalBlend(float deltaTime)
{
    externalBlendTime += deltaTime;

    float t = std::clamp(externalBlendTime / externalBlendDuration, 0.0f, 1.0f);
    currentPose.eye = MathHelper::Lerp(externalStartPose.eye, externalTargetPose.eye, t);
    currentPose.target = MathHelper::Lerp(externalStartPose.target, externalTargetPose.target, t);
    currentYaw = MathHelper::LerpAngle(externalStartPose.yaw, externalTargetPose.yaw, t);
    currentPitch = std::lerp(externalStartPose.pitch, externalTargetPose.pitch, t);

    if (t >= 1.0f)
    {
        if (finishedExternalBlend)
        {
            finishedExternalBlend();
            finishedExternalBlend = nullptr;
        }
        isExternalBlending = false;
        currentMode = CameraMode::TPS;
    }
}

// ブレンド状態を更新する
void DarkCameraActor::UpdateBlend(float deltaTime)
{
    if (!isBlending)
        return;

    blendTime += deltaTime;

    float t = std::clamp(blendTime / blendDuration, 0.0f, 1.0f);

    // 移動中のPlayer/Enemyを反映し、Blend完了次フレームとの差を残さない。
    CameraPose targetPose = blendTargetPose;
    if (auto head = playerHead.lock())
    {
        const DirectX::XMFLOAT3 currentPlayerPos = head->GetComponentLocation();
        targetPose = CalculatePose(
            requestMode,
            currentPlayerPos,
            blendTargetPose.yaw,
            blendTargetPose.pitch);
    }

    // Target、視線方向、Distanceを表示中Poseから同じBlend率で補間する。
    currentPose.target = MathHelper::Lerp(blendStartPose.target, targetPose.target, t);
    const DirectX::XMFLOAT3 targetEyeDirection = MathHelper::Normalize(
        MathHelper::Subtract(targetPose.eye, targetPose.target));
    const float targetEyeYaw = atan2f(targetEyeDirection.x, targetEyeDirection.z);
    const float targetEyePitch = asinf(std::clamp(targetEyeDirection.y, -1.0f, 1.0f));
    const float eyeYaw = MathHelper::LerpAngle(blendStartEyeYaw, targetEyeYaw, t);
    const float eyePitch = std::lerp(blendStartEyePitch, targetEyePitch, t);
    const float eyeDistance = std::lerp(
        blendStartEyeDistance,
        MathHelper::Distance(targetPose.eye, targetPose.target),
        t);
    const DirectX::XMFLOAT3 eyeDirection =
    {
        sinf(eyeYaw) * cosf(eyePitch),
        sinf(eyePitch),
        cosf(eyeYaw) * cosf(eyePitch)
    };
    currentPose.eye = MathHelper::Add(
        currentPose.target,
        MathHelper::Multiply(eyeDirection, eyeDistance));
    currentYaw = MathHelper::LerpAngle(blendStartPose.yaw, blendTargetPose.yaw, t);
    currentPitch = std::lerp(blendStartPose.pitch, blendTargetPose.pitch, t);
    currentPose.yaw = currentYaw;
    currentPose.pitch = currentPitch;

    if (t >= 1.0f)
    {
        currentPose = targetPose;
        currentYaw = blendTargetPose.yaw;
        currentPitch = blendTargetPose.pitch;
        isBlending = false;
        currentMode = requestMode;
        desiredYaw = currentYaw;
        desiredPitch = currentPitch;
        mainCameraComponent->SetYawAndPitch(currentYaw, currentPitch);
    }
}

// フォーカスカメラの情報を作成する
DarkCameraActor::CameraDirectionInfo DarkCameraActor::CreateFocusInfo()
{
    CameraDirectionInfo info = {};
    auto playerHeadShared = playerHead.lock();

    if (!playerHeadShared)
    {
        Logger::Warning("playerHeadShared is nullptr");
        return{};
    }

    auto playerActor = playerHeadShared->GetOwner();

    if (!playerActor)
    {
        Logger::Warning("playerActor is nullptr");
        return{};
    }

    info.direction = MathHelper::Normalize(playerActor->GetForward());
    if (auto player = dynamic_cast<Player*>(playerActor))
    {
        player->SetFocusDirection(info.direction);
    }

    //info.yaw = 0.0f;
    info.yaw = atan2f(info.direction.x, info.direction.z);
    info.pitch = 0.0f;
    return info;
}

// ロックオンカメラの情報を作成する
DarkCameraActor::CameraDirectionInfo DarkCameraActor::CreateLockOnInfo()
{
    CameraDirectionInfo info{};

    auto playerHeadShared = playerHead.lock();
    auto enemyHeadShared = enemyHead.lock();

    if (!playerHeadShared || !enemyHeadShared)
    {
        Logger::Warning("LockOn target missing");
        return {};
    }
    auto player = playerHeadShared->GetOwner();
    auto enemy = enemyHeadShared->GetOwner();

    if (!player || !enemy)
    {
        return {};
    }

    DirectX::XMFLOAT3 playerPos = player->GetPosition();

    DirectX::XMFLOAT3 enemyPos = enemy->GetPosition();

    info.direction = MathHelper::Normalize(MathHelper::Subtract(enemyPos, playerPos));
    info.yaw = atan2f(info.direction.x, info.direction.z);
    // 調整値
    info.yaw += XMConvertToRadians(lockOnYawOffsetDegree);

    //float horizontalLength = sqrtf(info.direction.x * info.direction.x + info.direction.z * info.direction.z);
    //info.pitch = atan2f(info.direction.y, horizontalLength);
    info.pitch = XMConvertToRadians(lockOnPitchDegree);

    return info;
}

// 目標の方向を更新する関数
void DarkCameraActor::UpdateDesireRotation(float deltaTime)
{
    auto intent = inputComponent->GetIntent();
    // 右スティックの入力値
    DirectX::XMFLOAT2 rightStick = intent.rightMove;
    switch (currentMode)
    {
    case CameraMode::TPS:
        // 右スティックがYawになる
        desiredYaw += rightStick.x * rotateSpeed * deltaTime;
        desiredPitch += rightStick.y * rotateSpeed * deltaTime;
        break;
    case CameraMode::Focus:
        if (std::abs(inputComponent->GetIntent().leftMove.x) >= FLT_EPSILON &&
            std::abs(inputComponent->GetIntent().leftMove.z) >= FLT_EPSILON)
        {

        }
        else
        {// player の左スティックが入力されていなかったら、
            // 右スティックを動かす
            desiredYaw += rightStick.x * rotateSpeed * deltaTime;
            desiredPitch += rightStick.y * rotateSpeed * deltaTime;
        }
        break;
    case CameraMode::LockOn:
        // 敵とプレイヤーが向いている方向
        if (auto playerHeadShared = playerHead.lock())
        {
            if (auto player = playerHeadShared->GetOwner())
            {
                if (auto enemy = enemyHead.lock()->GetOwner())
                {
                    DirectX::XMFLOAT3 playerPos = player->GetPosition();
                    DirectX::XMFLOAT3 enemyPos = enemy->GetPosition();

                    DirectX::XMFLOAT3 toEnemy = MathHelper::Subtract(enemyPos, playerPos);
                    desiredYaw = atan2f(toEnemy.x, toEnemy.z);
                    desiredPitch = XMConvertToRadians(lockOnPitchDegree);
                    // 調整値
                    desiredYaw += XMConvertToRadians(lockOnYawOffsetDegree);



                    // 右スティックを動かす
                    desiredYaw += rightStick.x * rotateSpeed * deltaTime;
                    desiredPitch += rightStick.y * rotateSpeed * deltaTime;


                }
            }
        }
        break;
    }

    // 
    desiredPitch = std::clamp(desiredPitch, DirectX::XMConvertToRadians(minPitchDegree), DirectX::XMConvertToRadians(maxPitchDegree));
    desiredYaw = MathHelper::ClampAngle(desiredYaw);
}

// 実際の方向を更新する関数
void DarkCameraActor::UpdateRotation(float deltaTime)
{
    //currentYaw = desiredYaw;
    //currentPitch = desiredPitch;
    float rotateRate = 20.0f;
    currentYaw = MathHelper::LerpAngle(currentYaw, desiredYaw, rotateRate * deltaTime);
    currentPitch = std::lerp(currentPitch, desiredPitch, rotateRate * deltaTime);
    mainCameraComponent->SetYawAndPitch(currentYaw, currentPitch);
}

void DarkCameraActor::UpdateLockOnComposition(float deltaTime)
{
    auto playerHeadShared = playerHead.lock();
    auto enemyHeadShared = enemyHead.lock();
    if (!playerHeadShared || !enemyHeadShared)
    {
        return;
    }

    lockOnEnemyDistance = MathHelper::Distance(
        playerHeadShared->GetComponentLocation(),
        enemyHeadShared->GetComponentLocation());

    if (lockOnDistanceForZoom <= FLT_EPSILON)
    {
        lockOnDistanceForZoom = lockOnEnemyDistance;
    }
    else if (lockOnEnemyDistance > lockOnDistanceForZoom + lockOnDistanceDeadZone)
    {
        lockOnDistanceForZoom = lockOnEnemyDistance - lockOnDistanceDeadZone;
    }
    else if (lockOnEnemyDistance < lockOnDistanceForZoom - lockOnDistanceDeadZone)
    {
        lockOnDistanceForZoom = lockOnEnemyDistance + lockOnDistanceDeadZone;
    }

    const float collisionRange = std::max<float>(
        lockOnCollisionStartRatio - lockOnCollisionFullRatio,
        FLT_EPSILON);
    const float collisionInput =
        (lockOnCollisionStartRatio - cameraCollisionRatio) / collisionRange;
    const float targetCollisionStrength = SmoothStep01(collisionInput);

    const float enemyRange = std::max<float>(
        lockOnDistanceFull - lockOnDistanceStart,
        FLT_EPSILON);
    const float enemyDistanceInput =
        (lockOnDistanceForZoom - lockOnDistanceStart) / enemyRange;
    const float targetDistanceStrength = SmoothStep01(enemyDistanceInput);

    const float lerpRate = std::clamp(
        deltaTime * lockOnCompositionLerpSpeed,
        0.0f,
        1.0f);
    lockOnCollisionStrength = std::lerp(
        lockOnCollisionStrength,
        targetCollisionStrength,
        lerpRate);
    lockOnDistanceStrength = targetDistanceStrength;

    adaptiveLockOnTargetWeight = std::lerp(
        lockOnTargetWeight,
        lockOnWallTargetWeight,
        lockOnCollisionStrength);
    adaptiveLockOnHorizontalOffset = lockOnSettings.horizontalOffset * std::lerp(
        1.0f,
        lockOnWallHorizontalScale,
        lockOnCollisionStrength);

    desiredLockOnCameraDistance =
        lockOnSettings.distance + lockOnMaxDistanceAdd * lockOnDistanceStrength;
    currentLockOnZoomSpeed = desiredLockOnCameraDistance > currentLockOnCameraDistance
        ? lockOnZoomOutSpeed
        : lockOnZoomInSpeed;
    const float zoomLerpRate = std::clamp(
        deltaTime * currentLockOnZoomSpeed,
        0.0f,
        1.0f);
    currentLockOnCameraDistance = std::lerp(
        currentLockOnCameraDistance,
        desiredLockOnCameraDistance,
        zoomLerpRate);

    adaptiveLockOnCameraDistance = currentLockOnCameraDistance * std::lerp(
        1.0f,
        lockOnWallDistanceScale,
        lockOnCollisionStrength);
}

void DarkCameraActor::ResetLockOnAdaptiveState()
{
    lockOnCollisionStrength = 0.0f;
    lockOnDistanceStrength = 0.0f;
    lockOnDistanceForZoom = 0.0f;
    lockOnEnemyDistance = 0.0f;
    desiredLockOnCameraDistance = lockOnSettings.distance;
    currentLockOnCameraDistance = lockOnSettings.distance;
    adaptiveLockOnCameraDistance = lockOnSettings.distance;
    currentLockOnZoomSpeed = 0.0f;
    adaptiveLockOnTargetWeight = lockOnTargetWeight;
    adaptiveLockOnHorizontalOffset = lockOnSettings.horizontalOffset;
}

DarkCameraActor::CameraPose DarkCameraActor::CalculatePose(CameraMode cameraMode, const DirectX::XMFLOAT3& playerPos, float yaw, float pitch) const
{
    CameraPose pose{};

    const CameraCompositionSettings* settings = &tpsSettings;
    if (cameraMode == CameraMode::Focus) settings = &focusSettings;
    if (cameraMode == CameraMode::LockOn) settings = &lockOnSettings;
    float distance = settings->distance;
    float horizontalOffset = settings->horizontalOffset;
    if (cameraMode == CameraMode::LockOn)
    {
        distance = adaptiveLockOnCameraDistance;
        horizontalOffset = adaptiveLockOnHorizontalOffset;
    }

    switch (cameraMode)
    {
    case CameraMode::TPS:
    {
        pose.target = playerPos;
        pose.target.y += settings->lookTargetHeight;
        break;
    }
    case CameraMode::Focus:
    {
        DirectX::XMFLOAT3 forward = { sinf(yaw),0.0f,cosf(yaw) };
        pose.target = MathHelper::Add(playerPos, MathHelper::Multiply(forward, focusDistance));
        pose.target.y += settings->lookTargetHeight;
        break;
    }
    case CameraMode::LockOn:
    {
        auto enemy = enemyHead.lock();
        if (enemy)
        {
            XMFLOAT3 playerLookPosition = playerPos;
            XMFLOAT3 enemyLookPosition = enemy->GetComponentLocation();
            playerLookPosition.y += lockOnPlayerLookHeight;
            enemyLookPosition.y += lockOnEnemyLookHeight;

            pose.target = MathHelper::Lerp(
                playerLookPosition,
                enemyLookPosition,
                adaptiveLockOnTargetWeight);
        }
        break;
    }
    }

    // Eye の処理は共通
    using namespace DirectX;

    XMVECTOR forward = XMVectorSet(
        sinf(yaw) * cosf(pitch),
        sinf(pitch),
        cosf(yaw) * cosf(pitch),
        0.0f);
    XMVECTOR target = XMLoadFloat3(&pose.target);
    XMVECTOR eye = target - forward * distance;
    XMVECTOR right = XMVectorSet(cosf(yaw), 0.0f, -sinf(yaw), 0.0f);
    eye += right * horizontalOffset;
    eye += XMVectorSet(0, settings->height, 0, 0);

    XMStoreFloat3(&pose.eye, eye);
    //  yaw と pitch を保存
    pose.yaw = yaw;
    pose.pitch = pitch;
    return pose;
}

// ロックオンのカメラ距離を計算する関数
float DarkCameraActor::CalculateLockOnDistance() const
{
    auto playerHeadShared = playerHead.lock();
    auto enemyHeadShared = enemyHead.lock();

    if (!playerHeadShared || !enemyHeadShared)
    {
        return lockOnCameraDistance;
    }

    auto player = playerHeadShared->GetOwner();
    auto enemy = enemyHeadShared->GetOwner();

    if (!player || !enemy)
    {
        return lockOnCameraDistance;
    }

    DirectX::XMFLOAT3 playerPos = player->GetPosition();
    DirectX::XMFLOAT3 enemyPos = enemy->GetPosition();

    float enemyDistance = MathHelper::Distance(playerPos, enemyPos);

    float distance =
        lockOnCameraDistance +
        enemyDistance * lockOnDistanceScale;

    distance = std::min<float>(distance, lockOnMaxDistance);

    return distance;
}


// 当たり判定を考慮する関数
DirectX::XMFLOAT3 DarkCameraActor::ResolveCameraCollision(DirectX::XMFLOAT3 target, DirectX::XMFLOAT3 eye)
{
    using namespace DirectX;
    DirectX::XMFLOAT3 idealEye = eye;
    HitResultWithActor hit;
    uint32_t mask =
        CollisionHelper::ToBit(CollisionLayer::WorldStatic) |
        CollisionHelper::ToBit(CollisionLayer::Floor) |
        CollisionHelper::ToBit(CollisionLayer::WorldProps);

    cameraHitWall = CollisionFunction::SphereRayCast(target, eye, hit, sphereCastRadius, mask);
    if (cameraHitWall)
    {
        float collisionOffset = sphereCastRadius + 0.05f;
        // 少し手前に出す
        idealEye = MathHelper::Add(hit.hitPoint,
            MathHelper::Multiply(hit.normal, collisionOffset));

        cameraHitDistance = hit.distance;

    }

    return idealEye;
}


void DarkCameraActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    if (ImGui::Button(U8("TPS")))
    {
        SetRequestMode(CameraMode::TPS);
    }
    if (ImGui::Button(U8("Focus")))
    {
        SetRequestMode(CameraMode::Focus);
    }
    if (ImGui::Button(U8("LockOn")))
    {
        SetRequestMode(CameraMode::LockOn);
    }
    ImGui::DragFloat(U8("右スティックの回転のスピード"), &rotateSpeed, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("フォーカス距離"), &focusDistance, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("最小ピッチ度数"), &minPitchDegree, 0.1f, -90.0f, 90.0f);
    ImGui::DragFloat(U8("最大ピッチ度数"), &maxPitchDegree, 0.1f, -90.0f, 90.0f);

    ImGui::DragFloat(U8("sphereCastRadius"), &sphereCastRadius, 0.01f, 0.01f, 1.0f);
    ImGui::DragFloat(U8("blendDuration"), &blendDuration, 0.01f, 0.01f, 1.0f);
    if (ImGui::TreeNode("TPS Composition"))
    {
        ImGui::DragFloat("Distance##TPS", &tpsSettings.distance, 0.05f, 0.1f, 30.0f);
        ImGui::DragFloat("Height##TPS", &tpsSettings.height, 0.05f, -10.0f, 10.0f);
        ImGui::DragFloat("LookTarget Height##TPS", &tpsSettings.lookTargetHeight, 0.05f, -10.0f, 10.0f);
        ImGui::DragFloat("FOV##TPS", &tpsSettings.fovDegree, 0.1f, 10.0f, 120.0f);
        ImGui::DragFloat("Shoulder Offset##TPS", &tpsSettings.horizontalOffset, 0.05f, -10.0f, 10.0f);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Focus Composition"))
    {
        ImGui::DragFloat("Distance##Focus", &focusSettings.distance, 0.05f, 0.1f, 30.0f);
        ImGui::DragFloat("Height##Focus", &focusSettings.height, 0.05f, -10.0f, 10.0f);
        ImGui::DragFloat("LookTarget Height##Focus", &focusSettings.lookTargetHeight, 0.05f, -10.0f, 10.0f);
        ImGui::DragFloat("FOV##Focus", &focusSettings.fovDegree, 0.1f, 10.0f, 120.0f);
        ImGui::DragFloat("Horizontal Offset##Focus", &focusSettings.horizontalOffset, 0.05f, -10.0f, 10.0f);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("LockOn Composition"))
    {
        ImGui::DragFloat("Distance##LockOn", &lockOnSettings.distance, 0.05f, 0.1f, 30.0f);
        ImGui::DragFloat("Height##LockOn", &lockOnSettings.height, 0.05f, -10.0f, 10.0f);
        ImGui::DragFloat("FOV##LockOn", &lockOnSettings.fovDegree, 0.1f, 10.0f, 120.0f);
        ImGui::DragFloat("Player Look Height", &lockOnPlayerLookHeight, 0.05f, -10.0f, 10.0f);
        ImGui::DragFloat("Enemy Look Height", &lockOnEnemyLookHeight, 0.05f, -10.0f, 10.0f);
        ImGui::SliderFloat("Player/Enemy Target Weight", &lockOnTargetWeight, 0.0f, 1.0f);
        ImGui::DragFloat("Horizontal Offset##LockOn", &lockOnSettings.horizontalOffset, 0.05f, -10.0f, 10.0f);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("LockOn Adaptive Composition"))
    {
        ImGui::DragFloat("Collision Start Ratio", &lockOnCollisionStartRatio, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Collision Full Ratio", &lockOnCollisionFullRatio, 0.01f, 0.0f, 1.0f);
        ImGui::SliderFloat("Wall Target Weight", &lockOnWallTargetWeight, 0.0f, 1.0f);
        ImGui::SliderFloat("Wall Horizontal Scale", &lockOnWallHorizontalScale, 0.0f, 1.0f);
        ImGui::SliderFloat("Wall Distance Scale", &lockOnWallDistanceScale, 0.1f, 1.0f);
        ImGui::DragFloat("Distance Start", &lockOnDistanceStart, 0.05f, 0.0f, 30.0f);
        ImGui::DragFloat("Distance Full", &lockOnDistanceFull, 0.05f, 0.0f, 30.0f);
        ImGui::DragFloat("Max Distance Add", &lockOnMaxDistanceAdd, 0.05f, 0.0f, 10.0f);
        ImGui::DragFloat("Zoom Out Speed", &lockOnZoomOutSpeed, 0.1f, 0.01f, 30.0f);
        ImGui::DragFloat("Zoom In Speed", &lockOnZoomInSpeed, 0.1f, 0.01f, 30.0f);
        ImGui::DragFloat("Distance Dead Zone", &lockOnDistanceDeadZone, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Composition Lerp Speed", &lockOnCompositionLerpSpeed, 0.1f, 0.1f, 30.0f);
        ImGui::Separator();
        ImGui::Text("Desired Eye: %.3f, %.3f, %.3f",
            desiredEyePosition.x, desiredEyePosition.y, desiredEyePosition.z);
        ImGui::Text("Collision Pre Eye: %.3f, %.3f, %.3f",
            collisionPreEyePosition.x, collisionPreEyePosition.y, collisionPreEyePosition.z);
        ImGui::Text("Collision Post Eye: %.3f, %.3f, %.3f",
            collisionPostEyePosition.x, collisionPostEyePosition.y, collisionPostEyePosition.z);
        ImGui::Text("Desired Camera Distance: %.3f", desiredCameraDistance);
        ImGui::Text("Actual Camera Distance: %.3f", actualCameraDistance);
        ImGui::Text("Collision Ratio: %.3f", cameraCollisionRatio);
        ImGui::Text("Player-Boss Distance: %.3f", lockOnEnemyDistance);
        ImGui::Text("Base Distance: %.3f", lockOnSettings.distance);
        ImGui::Text("Desired Distance: %.3f", desiredLockOnCameraDistance);
        ImGui::Text("Current Adaptive Distance: %.3f", adaptiveLockOnCameraDistance);
        ImGui::Text("Distance Strength: %.3f", lockOnDistanceStrength);
        ImGui::Text("Zoom Speed: %.3f", currentLockOnZoomSpeed);
        ImGui::Text("Target Weight: %.3f", lockOnTargetWeight);
        ImGui::Text("Adaptive Target Weight: %.3f", adaptiveLockOnTargetWeight);
        ImGui::Text("Adaptive Horizontal Offset: %.3f", adaptiveLockOnHorizontalOffset);
        ImGui::Text("Yaw Offset: %.3f deg", lockOnYawOffsetDegree);
        ImGui::Text("Pitch: %.3f deg", lockOnPitchDegree);
        ImGui::TreePop();
    }
    ImGui::DragFloat(U8("ロックオンカメラの基本距離"), &lockOnCameraDistance, 0.1f, 0.01f, 10.0f);
    ImGui::DragFloat(U8("敵との距離による増加量"), &lockOnDistanceScale, 0.05f, 0.01f, 0.999f);
    ImGui::DragFloat(U8("最大距離"), &lockOnMaxDistance, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat(U8("ロックオンピッチ度数"), &lockOnPitchDegree, 0.1f, -90.0f, 90.0f);
    ImGui::DragFloat(U8("ロックオンyaw度数"), &lockOnYawOffsetDegree, 0.1f, -90.0f, 90.0f);

    ImGui::DragFloat(U8("lockOnTargetNearWall"), &lockOnTargetNearWall, 0.1f, 0.01f, 10.0f);
    ImGui::DragFloat(U8("lockOnTargetNormal"), &lockOnTargetNormal, 0.1f, 0.01f, 10.0f);
    ImGui::DragFloat(U8("wallDistance"), &wallDistance, 0.1f, 0.01f, 10.0f);
    ImGui::Text("cameraHitDistance: %.4f", cameraHitDistance);
    ImGui::Text("cameraCollisionRatio: %.4f", cameraCollisionRatio);
    ImGui::Text("cameraHitWall: %s", cameraHitWall ? "true" : "false");

    ImGui::Text("Current : %d", (int)currentMode);
    ImGui::Text("Request : %d", (int)requestMode);
    ImGui::Text("Blend : %d", isBlending);

#endif
}
