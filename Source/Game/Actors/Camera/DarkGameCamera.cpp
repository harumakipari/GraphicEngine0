#include "pch.h"
#include "DarkGameCamera.h"

#include "Game/Actors/Player/Player.h"
#include "Engine/Debug/DebugRender.h"
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
    lockOnTargetWeight = 0.5f;
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

    initialTpsSettings = tpsSettings;
    initialLockOnSettings = lockOnSettings;
    initialBossTpsFovDegree = bossTpsFovDegree;
    initialLockOnEnemyLookHeight = lockOnEnemyLookHeight;
    initialLockOnTargetWeight = lockOnTargetWeight;
    initialLockOnZoomInSpeed = lockOnZoomInSpeed;
    initialLockOnZoomOutSpeed = lockOnZoomOutSpeed;
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
        const float fovDegree = isBlending
            ? std::lerp(blendStartFovDegree, blendTargetFovDegree,
                std::clamp(blendTime / (requestMode == CameraMode::Death
                    ? (std::max)(deathCameraSettings.deathBlendTime, 0.01f)
                    : blendDuration), 0.0f, 1.0f))
            : GetFovDegreeForMode(currentMode);
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
    lockOnFramingDeficit = currentMode == CameraMode::LockOn
        ? lockOnRequiredFramingDistance - actualCameraDistance
        : 0.0f;

    // Blend 中の FOV は既存処理を優先し、安定した LockOn 時だけ Collision Fallback を適用する。
    if (!isExternalBlending && !isBlending && currentMode == CameraMode::LockOn)
    {
        UpdateLockOnFovFallback(deltaTime);
    }

    if (showCameraCollisionDebug)
    {
        DebugRender::DrawLine(currentPose.target, collisionPreEyePosition,
            { 0.2f, 0.8f, 1.0f, 1.0f }, 0.0f, true);
        DebugRender::DrawSphere(collisionPostEyePosition, 0.12f,
            cameraHitWall ? DirectX::XMFLOAT4{ 1.0f, 0.2f, 0.2f, 1.0f }
                          : DirectX::XMFLOAT4{ 0.2f, 1.0f, 0.3f, 1.0f },
            0.0f, true);
    }

    UpdateLockOnTransitionDiagnostics();

    float targetBlend = cameraHitWall ? 1.0f : 0.0f;

    wallBlend = std::lerp(wallBlend, targetBlend, deltaTime * 8.0f);

    UpdateCameraShake(deltaTime);

    CameraPose renderPose = currentPose;
    renderPose.eye = MathHelper::Add(renderPose.eye, shakePositionOffset);
    renderPose.target = MathHelper::Add(renderPose.target, shakeTargetOffset);

    SetPosition(renderPose.eye);
    mainCameraComponent->lookTarget = renderPose.target;
    mainCameraComponent->useLookTarget = true;
}

void DarkCameraActor::PlayCameraShake(const float intensity, const float duration,const float frequency, const float positionAmount, const float targetAmount)
{
    shakeIntensity = (std::max)(intensity, 0.0f);
    shakeDuration = (std::max)(duration, 0.0f);
    shakeFrequency = (std::max)(frequency, 0.0f);
    shakePositionAmount = (std::max)(positionAmount, 0.0f);
    shakeTargetAmount = (std::max)(targetAmount, 0.0f);
    shakeElapsedTime = 0.0f;
    currentShakeEnvelope = 0.0f;
    shakePositionOffset = {};
    shakeTargetOffset = {};
    shakeActive = shakeIntensity > 0.0f && shakeDuration > FLT_EPSILON;
}

void DarkCameraActor::PlayCameraShakePreset(const std::string& presetName)
{
    const CameraShakePreset* preset = FindCameraShakePreset(presetName);
    if (!preset)
    {
        const std::string message = "[CameraShake] Unknown preset: " + presetName;
        Logger::Warning(Logger::LogCategory::Gameplay, message.c_str());
        return;
    }

    PlayCameraShake(preset->intensity, preset->duration, preset->frequency,
        preset->positionAmount, preset->targetAmount);
}

const DarkCameraActor::CameraShakePreset* DarkCameraActor::FindCameraShakePreset(
    const std::string& presetName) const
{
    if (presetName == BossHeavyLandingPresetName) return &bossHeavyLandingShake;
    if (presetName == BossWallImpactPresetName) return &bossWallImpactShake;
    if (presetName == RushFinalPresetName) return &rushFinalShake;
    return nullptr;
}

void DarkCameraActor::UpdateCameraShake(const float deltaTime)
{
    shakePositionOffset = {};
    shakeTargetOffset = {};
    currentShakeEnvelope = 0.0f;

    if (!shakeActive)
        return;

    if (shakeElapsedTime >= shakeDuration)
    {
        shakeActive = false;
        shakeElapsedTime = shakeDuration;
        return;
    }

    const float normalizedTime = std::clamp(shakeElapsedTime / shakeDuration, 0.0f, 1.0f);
    const float remaining = 1.0f - normalizedTime;
    currentShakeEnvelope = remaining * remaining;

    const float phase = shakeElapsedTime * shakeFrequency * DirectX::XM_2PI;
    const float verticalWave =
        (sinf(phase) + 0.35f * sinf(phase * 2.17f + 1.1f)) / 1.35f;
    const float sideWave =
        (cosf(phase * 1.37f + 0.4f) + 0.25f * sinf(phase * 0.73f + 2.0f)) / 1.25f;
    const float pitchWave =
        (sinf(phase * 0.73f + 1.7f) + 0.4f * cosf(phase * 1.91f)) / 1.4f;
    const float yawWave =
        (cosf(phase * 1.13f + 0.8f) + 0.3f * sinf(phase * 2.31f)) / 1.3f;

    const DirectX::XMFLOAT3 forward = MathHelper::Normalize(
        MathHelper::Subtract(currentPose.target, currentPose.eye));
    const DirectX::XMFLOAT3 up{ 0.0f, 1.0f, 0.0f };
    const DirectX::XMFLOAT3 right = MathHelper::Normalize(MathHelper::Cross(up, forward));
    const float strength = shakeIntensity * currentShakeEnvelope;

    shakePositionOffset = MathHelper::Add(
        MathHelper::Multiply(up, verticalWave * shakePositionAmount * strength),
        MathHelper::Multiply(right, sideWave * shakePositionAmount * strength * 0.35f));
    shakeTargetOffset = MathHelper::Add(
        MathHelper::Multiply(up, pitchWave * shakeTargetAmount * strength),
        MathHelper::Multiply(right, yawWave * shakeTargetAmount * strength * 0.45f));

    shakeElapsedTime = (std::min)(shakeElapsedTime + (std::max)(deltaTime, 0.0f), shakeDuration);
}

// ブレンドを開始する
void DarkCameraActor::StartDeathMode(std::function<void()> onBlendFinished)
{
    deathBlendFinished = std::move(onBlendFinished);
    SetRequestMode(CameraMode::Death);
}

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

    case CameraMode::Death:
    {
        const CameraPose deathPose = CalculatePose(to, playerPos, currentYaw, currentPitch);
        targetYaw = deathPose.yaw;
        targetPitch = deathPose.pitch;
        break;
    }
    }
    blendTargetPose = CalculatePose(to, playerPos, targetYaw, targetPitch);
    blendStartFovDegree = DirectX::XMConvertToDegrees(mainCameraComponent->GetFov());
    blendTargetFovDegree = GetFovDegreeForMode(to);
}

bool DarkCameraActor::IsBossBattle() const
{
    const auto head = playerHead.lock();
    const auto player = head ? dynamic_cast<Player*>(head->GetOwner()) : nullptr;
    return player && player->IsBossBattle();
}

float DarkCameraActor::GetFovDegreeForMode(CameraMode mode) const
{
    if (mode == CameraMode::LockOn) return lockOnSettings.fovDegree;
    if (mode == CameraMode::Focus) return focusSettings.fovDegree;
    return IsBossBattle() ? bossTpsFovDegree : tpsSettings.fovDegree;
}

void DarkCameraActor::ResetCameraTuning()
{
    tpsSettings.fovDegree = initialTpsSettings.fovDegree;
    tpsSettings.distance = initialTpsSettings.distance;
    bossTpsFovDegree = initialBossTpsFovDegree;
    lockOnSettings.fovDegree = initialLockOnSettings.fovDegree;
    lockOnSettings.distance = initialLockOnSettings.distance;
    lockOnEnemyLookHeight = initialLockOnEnemyLookHeight;
    lockOnTargetWeight = initialLockOnTargetWeight;
    lockOnZoomInSpeed = initialLockOnZoomInSpeed;
    lockOnZoomOutSpeed = initialLockOnZoomOutSpeed;
    ResetLockOnAdaptiveState();
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

    const float currentBlendDuration = requestMode == CameraMode::Death
        ? (std::max)(deathCameraSettings.deathBlendTime, 0.01f)
        : blendDuration;
    float t = std::clamp(blendTime / currentBlendDuration, 0.0f, 1.0f);

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
        if (currentMode == CameraMode::Death && deathBlendFinished)
        {
            auto callback = std::move(deathBlendFinished);
            deathBlendFinished = nullptr;
            callback();
        }
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
    case CameraMode::Death:
        // The death composition is derived from the actors, not camera input.
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

    // Collision Ratioが接触境界で微振動してもAdaptive値を往復させない。
    if (cameraCollisionRatio < lockOnCollisionRatioForAdaptive - lockOnCollisionRatioHysteresis)
    {
        lockOnCollisionRatioForAdaptive = cameraCollisionRatio + lockOnCollisionRatioHysteresis;
    }
    else if (cameraCollisionRatio > lockOnCollisionRatioForAdaptive + lockOnCollisionRatioHysteresis)
    {
        lockOnCollisionRatioForAdaptive = cameraCollisionRatio - lockOnCollisionRatioHysteresis;
    }

    const float collisionRange = std::max<float>(
        lockOnCollisionStartRatio - lockOnCollisionFullRatio,
        FLT_EPSILON);
    const float collisionInput =
        (lockOnCollisionStartRatio - lockOnCollisionRatioForAdaptive) / collisionRange;
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

    // 壁際の初期比較はDistance Adaptiveのみ。Weight/OffsetはBase値を維持する。
    lockOnWallTargetWeight = lockOnTargetWeight;
    lockOnWallHorizontalScale = 1.0f;
    adaptiveLockOnTargetWeight = lockOnTargetWeight;
    adaptiveLockOnHorizontalOffset = lockOnSettings.horizontalOffset;

    lockOnExistingAdaptiveDistance =
        lockOnSettings.distance + lockOnMaxDistanceAdd * lockOnDistanceStrength;
    lockOnRequiredFramingDistance = CalculateRequiredLockOnFramingDistance();
    desiredLockOnCameraDistance = (std::max)(
        lockOnExistingAdaptiveDistance, lockOnRequiredFramingDistance);
    lockOnFramingActive =
        lockOnRequiredFramingDistance > lockOnExistingAdaptiveDistance;
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

float DarkCameraActor::CalculateRequiredLockOnFramingDistance()
{
    lockOnHorizontalExtent = 0.0f;
    lockOnVerticalExtent = 0.0f;
    lockOnRequiredXDistance = 0.0f;
    lockOnRequiredYDistance = 0.0f;

    const float minimumLockOnDistance = (std::max)(lockOnSettings.distance, 0.1f);
    const auto playerHeadShared = playerHead.lock();
    const auto enemyHeadShared = enemyHead.lock();
    if (!playerHeadShared || !enemyHeadShared)
        return minimumLockOnDistance;

    DirectX::XMFLOAT3 playerLookPosition = playerHeadShared->GetComponentLocation();
    DirectX::XMFLOAT3 enemyLookPosition = enemyHeadShared->GetComponentLocation();
    playerLookPosition.y += lockOnPlayerLookHeight;
    enemyLookPosition.y += lockOnEnemyLookHeight;

    const float targetWeight = std::clamp(lockOnTargetWeight, 0.0f, 1.0f);
    const DirectX::XMFLOAT3 framingTarget = MathHelper::Lerp(
        playerLookPosition, enemyLookPosition, targetWeight);

    DirectX::XMFLOAT3 playerPosition = playerLookPosition;
    DirectX::XMFLOAT3 enemyPosition = enemyLookPosition;
    if (const auto player = playerHeadShared->GetOwner())
        playerPosition = player->GetPosition();
    if (const auto enemy = enemyHeadShared->GetOwner())
        enemyPosition = enemy->GetPosition();

    const DirectX::XMFLOAT3 playerToEnemy = MathHelper::Subtract(
        enemyPosition, playerPosition);
    float lockOnYaw = currentYaw;
    if (std::abs(playerToEnemy.x) > FLT_EPSILON ||
        std::abs(playerToEnemy.z) > FLT_EPSILON)
    {
        lockOnYaw = std::atan2(playerToEnemy.x, playerToEnemy.z) +
            DirectX::XMConvertToRadians(lockOnYawOffsetDegree);
    }
    const float lockOnPitch = DirectX::XMConvertToRadians(lockOnPitchDegree);
    const float cosPitch = std::cos(lockOnPitch);
    const DirectX::XMFLOAT3 cameraForward{
        std::sin(lockOnYaw) * cosPitch,
        std::sin(lockOnPitch),
        std::cos(lockOnYaw) * cosPitch };
    const DirectX::XMFLOAT3 cameraRight{
        std::cos(lockOnYaw), 0.0f, -std::sin(lockOnYaw) };
    const DirectX::XMFLOAT3 cameraUp = MathHelper::Normalize(
        MathHelper::Cross(cameraForward, cameraRight));

    const float verticalFov = DirectX::XMConvertToRadians(lockOnSettings.fovDegree);
    const float screenWidth = Graphics::GetScreenWidth();
    const float screenHeight = Graphics::GetScreenHeight();
    const float aspect = screenHeight > FLT_EPSILON
        ? screenWidth / screenHeight
        : 0.0f;
    const float tanHalfVertical = std::tan(verticalFov * 0.5f);
    const float tanHalfHorizontal = tanHalfVertical * aspect;
    const float safeScaleX = 1.0f - 2.0f * std::clamp(
        lockOnSafeFrameHorizontalMargin, 0.0f, 0.49f);
    const float safeScaleY = 1.0f - 2.0f * std::clamp(
        lockOnSafeFrameVerticalMargin, 0.0f, 0.49f);
    const float horizontalDenominator = tanHalfHorizontal * safeScaleX;
    const float verticalDenominator = tanHalfVertical * safeScaleY;
    if (!std::isfinite(verticalFov) || verticalFov <= 0.0f ||
        verticalFov >= DirectX::XM_PI || !std::isfinite(aspect) ||
        aspect <= FLT_EPSILON || !std::isfinite(horizontalDenominator) ||
        !std::isfinite(verticalDenominator) ||
        horizontalDenominator <= FLT_EPSILON ||
        verticalDenominator <= FLT_EPSILON)
    {
        return minimumLockOnDistance;
    }

    const auto evaluateSample = [&](const DirectX::XMFLOAT3& sample)
    {
        const DirectX::XMFLOAT3 relative = MathHelper::Subtract(
            sample, framingTarget);
        const float x = MathHelper::Dot(relative, cameraRight);
        const float y = MathHelper::Dot(relative, cameraUp);
        const float z = MathHelper::Dot(relative, cameraForward);
        lockOnHorizontalExtent = (std::max)(lockOnHorizontalExtent, std::abs(x));
        lockOnVerticalExtent = (std::max)(lockOnVerticalExtent, std::abs(y));
        lockOnRequiredXDistance = (std::max)(lockOnRequiredXDistance,
            std::abs(x) / horizontalDenominator - z);
        lockOnRequiredYDistance = (std::max)(lockOnRequiredYDistance,
            std::abs(y) / verticalDenominator - z);
    };

    evaluateSample(playerLookPosition);
    evaluateSample(enemyLookPosition);
    lockOnRequiredXDistance = (std::max)(lockOnRequiredXDistance, 0.0f);
    lockOnRequiredYDistance = (std::max)(lockOnRequiredYDistance, 0.0f);
    return (std::max)({
        minimumLockOnDistance,
        lockOnRequiredXDistance,
        lockOnRequiredYDistance });
}

float DarkCameraActor::CalculateRequiredLockOnFov(bool& outValid)
{
    LockOnFovDiagnostics collisionPreDiagnostics{};
    bool collisionPreValid = false;
    lockOnCollisionPreRequiredFovDegree = EvaluateRequiredLockOnFovFromEye(
        collisionPreEyePosition, collisionPreDiagnostics, collisionPreValid);

    return EvaluateRequiredLockOnFovFromEye(
        collisionPostEyePosition, lockOnFovDiagnostics, outValid);
}

float DarkCameraActor::EvaluateRequiredLockOnFovFromEye(
    const DirectX::XMFLOAT3& eye,
    LockOnFovDiagnostics& diagnostics,
    bool& outValid) const
{
    outValid = false;
    diagnostics = {};
    const auto playerHeadShared = playerHead.lock();
    const auto enemyHeadShared = enemyHead.lock();
    if (!playerHeadShared || !enemyHeadShared)
        return lockOnSettings.fovDegree;

    const DirectX::XMFLOAT3 forwardVector = MathHelper::Subtract(
        currentPose.target, eye);
    if (MathHelper::Length(forwardVector) <= 0.1f)
        return lockOnSettings.fovDegree;

    const DirectX::XMFLOAT3 cameraForward = MathHelper::Normalize(forwardVector);
    const DirectX::XMFLOAT3 worldUp{ 0.0f, 1.0f, 0.0f };
    const DirectX::XMFLOAT3 rightVector = MathHelper::Cross(worldUp, cameraForward);
    if (MathHelper::Length(rightVector) <= FLT_EPSILON)
        return lockOnSettings.fovDegree;

    // CameraComponent::GetRight / LookAtLH と同じ cross 順序。
    const DirectX::XMFLOAT3 cameraRight = MathHelper::Normalize(rightVector);
    const DirectX::XMFLOAT3 cameraUp = MathHelper::Normalize(
        MathHelper::Cross(cameraForward, cameraRight));

    const float screenWidth = Graphics::GetScreenWidth();
    const float screenHeight = Graphics::GetScreenHeight();
    const float aspect = screenHeight > FLT_EPSILON
        ? screenWidth / screenHeight
        : 0.0f;
    const float safeScaleX = 1.0f - 2.0f * std::clamp(
        lockOnSafeFrameHorizontalMargin, 0.0f, 0.49f);
    const float safeScaleY = 1.0f - 2.0f * std::clamp(
        lockOnSafeFrameVerticalMargin, 0.0f, 0.49f);
    if (!std::isfinite(aspect) || aspect <= FLT_EPSILON ||
        safeScaleX <= FLT_EPSILON || safeScaleY <= FLT_EPSILON)
    {
        return lockOnSettings.fovDegree;
    }

    DirectX::XMFLOAT3 playerLookPosition = playerHeadShared->GetComponentLocation();
    DirectX::XMFLOAT3 enemyLookPosition = enemyHeadShared->GetComponentLocation();
    playerLookPosition.y += lockOnPlayerLookHeight;
    enemyLookPosition.y += lockOnEnemyLookHeight;

    float requiredTanVertical = 0.0f;
    const auto evaluateSample = [&](const DirectX::XMFLOAT3& sample,
        LockOnFovSampleDiagnostics& sampleDiagnostics,
        const char* sampleName)
    {
        const DirectX::XMFLOAT3 relative = MathHelper::Subtract(
            sample, eye);
        const float signedX = MathHelper::Dot(relative, cameraRight);
        const float signedY = MathHelper::Dot(relative, cameraUp);
        const float z = MathHelper::Dot(relative, cameraForward);
        sampleDiagnostics.cameraSpace = { signedX, signedY, z };
        constexpr float nearZ = 0.1f;
        sampleDiagnostics.inFront = std::isfinite(z) && z > nearZ;
        if (!std::isfinite(signedX) || !std::isfinite(signedY) ||
            !std::isfinite(z) || !sampleDiagnostics.inFront)
        {
            return false;
        }

        const float x = std::abs(signedX);
        const float y = std::abs(signedY);
        const float requiredTanVerticalFromY = y / (z * safeScaleY);
        const float requiredTanHorizontal = x / (z * safeScaleX);
        const float requiredTanVerticalFromX = requiredTanHorizontal / aspect;
        sampleDiagnostics.requiredFovFromX = DirectX::XMConvertToDegrees(
            2.0f * std::atan(requiredTanVerticalFromX));
        sampleDiagnostics.requiredFovFromY = DirectX::XMConvertToDegrees(
            2.0f * std::atan(requiredTanVerticalFromY));

        if (requiredTanVerticalFromX > requiredTanVertical)
        {
            requiredTanVertical = requiredTanVerticalFromX;
            diagnostics.limiter = std::string(sampleName) + "-X";
        }
        if (requiredTanVerticalFromY > requiredTanVertical)
        {
            requiredTanVertical = requiredTanVerticalFromY;
            diagnostics.limiter = std::string(sampleName) + "-Y";
        }
        return std::isfinite(requiredTanVertical);
    };

    const bool playerValid = evaluateSample(
        playerLookPosition, diagnostics.player, "Player");
    const bool bossValid = evaluateSample(
        enemyLookPosition, diagnostics.boss, "Boss");
    if (!playerValid || !bossValid)
    {
        diagnostics.limiter = !playerValid
            ? "Invalid / Player Behind Camera"
            : "Invalid / Boss Behind Camera";
        return lockOnSettings.fovDegree;
    }

    const float requiredFovDegree = DirectX::XMConvertToDegrees(
        2.0f * std::atan(requiredTanVertical));
    if (!std::isfinite(requiredFovDegree))
        return lockOnSettings.fovDegree;

    outValid = true;
    return requiredFovDegree;
}

void DarkCameraActor::UpdateLockOnFovFallback(float deltaTime)
{
    const float baseFovDegree = lockOnSettings.fovDegree;
    const float maxFallbackFovDegree = (std::max)(
        baseFovDegree, lockOnMaxFallbackFovDegree);
    bool requiredFovValid = false;
    lockOnRequiredFovDegree = CalculateRequiredLockOnFov(requiredFovValid);

    const bool canEnterFallback = requiredFovValid && cameraHitWall &&
        lockOnFramingDeficit > lockOnFovFallbackEnterDeficit &&
        lockOnRequiredFovDegree > baseFovDegree;
    const bool canRemainInFallback = requiredFovValid && cameraHitWall &&
        lockOnFramingDeficit > lockOnFovFallbackExitDeficit &&
        lockOnRequiredFovDegree > baseFovDegree;

    if (!lockOnFovFallbackActive)
    {
        if (canEnterFallback)
        {
            lockOnFovFallbackActive = true;
            lockOnFovReturnDelayElapsed = 0.0f;
        }
    }
    else if (canRemainInFallback)
    {
        lockOnFovReturnDelayElapsed = 0.0f;
    }
    else
    {
        lockOnFovReturnDelayElapsed += (std::max)(deltaTime, 0.0f);
        if (lockOnFovReturnDelayElapsed >= lockOnFovFallbackReturnDelay)
        {
            lockOnFovFallbackActive = false;
            lockOnFovReturnDelayElapsed = 0.0f;
        }
    }

    if (lockOnFovFallbackActive && (canEnterFallback || canRemainInFallback))
    {
        lockOnTargetFovDegree = std::clamp(
            lockOnRequiredFovDegree, baseFovDegree, maxFallbackFovDegree);
    }
    else if (!lockOnFovFallbackActive)
    {
        lockOnTargetFovDegree = baseFovDegree;
    }
    // Return Delay 中は直前の Target FOV を保持する。

    const float interpolationSpeed = lockOnTargetFovDegree > lockOnCurrentFovDegree
        ? lockOnFovExpandSpeed
        : lockOnFovReturnSpeed;
    const float maxFovDelta = (std::max)(interpolationSpeed, 0.0f) *
        (std::max)(deltaTime, 0.0f);
    lockOnCurrentFovDegree += std::clamp(
        lockOnTargetFovDegree - lockOnCurrentFovDegree,
        -maxFovDelta,
        maxFovDelta);
    lockOnCurrentFovDegree = std::clamp(
        lockOnCurrentFovDegree, baseFovDegree, maxFallbackFovDegree);
    mainCameraComponent->SetFov(DirectX::XMConvertToRadians(lockOnCurrentFovDegree));
}

void DarkCameraActor::ResetLockOnAdaptiveState()
{
    lockOnCollisionStrength = 0.0f;
    lockOnCollisionRatioForAdaptive = 1.0f;
    lockOnDistanceStrength = 0.0f;
    lockOnDistanceForZoom = 0.0f;
    lockOnEnemyDistance = 0.0f;
    desiredLockOnCameraDistance = lockOnSettings.distance;
    currentLockOnCameraDistance = lockOnSettings.distance;
    adaptiveLockOnCameraDistance = lockOnSettings.distance;
    currentLockOnZoomSpeed = 0.0f;
    adaptiveLockOnTargetWeight = lockOnTargetWeight;
    adaptiveLockOnHorizontalOffset = lockOnSettings.horizontalOffset;
    lockOnHorizontalExtent = 0.0f;
    lockOnVerticalExtent = 0.0f;
    lockOnRequiredXDistance = 0.0f;
    lockOnRequiredYDistance = 0.0f;
    lockOnRequiredFramingDistance = lockOnSettings.distance;
    lockOnExistingAdaptiveDistance = lockOnSettings.distance;
    lockOnFramingDeficit = 0.0f;
    lockOnFramingActive = false;
    lockOnRequiredFovDegree = lockOnSettings.fovDegree;
    lockOnTargetFovDegree = lockOnSettings.fovDegree;
    lockOnCurrentFovDegree = lockOnSettings.fovDegree;
    lockOnFovReturnDelayElapsed = 0.0f;
    lockOnFovFallbackActive = false;
}

void DarkCameraActor::BeginLockOnTransitionDiagnostics(CameraMode from, CameraMode to)
{
    (void)from;
    lockOnTransitionDiagnosticsActive = true;
    lockOnTransitionDiagnosticsFrame = 0;
    lockOnTransitionTo = to;
    transitionStartDesiredEye = desiredEyePosition;
    transitionStartCollisionPostEye = collisionPostEyePosition;
    transitionStartCollisionRatio = cameraCollisionRatio;
    transitionStartAdaptiveDistance = adaptiveLockOnCameraDistance;
    transitionStartAdaptiveTargetWeight = adaptiveLockOnTargetWeight;
    transitionMaxDesiredEyeDelta = 0.0f;
    transitionMaxCollisionPostEyeDelta = 0.0f;
    transitionMaxCollisionRatioDelta = 0.0f;
    transitionMaxAdaptiveDistanceDelta = 0.0f;
    transitionMaxAdaptiveTargetWeightDelta = 0.0f;
}

void DarkCameraActor::UpdateLockOnTransitionDiagnostics()
{
    if (!lockOnTransitionDiagnosticsActive)
    {
        return;
    }

    const float deltas[] =
    {
        MathHelper::Distance(desiredEyePosition, transitionStartDesiredEye),
        MathHelper::Distance(collisionPostEyePosition, transitionStartCollisionPostEye),
        std::abs(cameraCollisionRatio - transitionStartCollisionRatio),
        std::abs(adaptiveLockOnCameraDistance - transitionStartAdaptiveDistance),
        std::abs(adaptiveLockOnTargetWeight - transitionStartAdaptiveTargetWeight)
    };
    transitionMaxDesiredEyeDelta = std::max<float>(transitionMaxDesiredEyeDelta, deltas[0]);
    transitionMaxCollisionPostEyeDelta = std::max<float>(transitionMaxCollisionPostEyeDelta, deltas[1]);
    transitionMaxCollisionRatioDelta = std::max<float>(transitionMaxCollisionRatioDelta, deltas[2]);
    transitionMaxAdaptiveDistanceDelta = std::max<float>(transitionMaxAdaptiveDistanceDelta, deltas[3]);
    transitionMaxAdaptiveTargetWeightDelta = std::max<float>(transitionMaxAdaptiveTargetWeightDelta, deltas[4]);

    const char* direction = lockOnTransitionTo == CameraMode::LockOn ? "TPS->LockOn" : "LockOn->TPS";
    Logger::Log(std::string("[LockOnTransition][") + direction + "][frame=" +
        std::to_string(lockOnTransitionDiagnosticsFrame) + "] DesiredEye=(" +
        std::to_string(desiredEyePosition.x) + "," + std::to_string(desiredEyePosition.y) + "," +
        std::to_string(desiredEyePosition.z) + ") CollisionPostEye=(" +
        std::to_string(collisionPostEyePosition.x) + "," + std::to_string(collisionPostEyePosition.y) + "," +
        std::to_string(collisionPostEyePosition.z) + ") CollisionRatio=" + std::to_string(cameraCollisionRatio) +
        " AdaptiveDistance=" + std::to_string(adaptiveLockOnCameraDistance) +
        " AdaptiveTargetWeight=" + std::to_string(adaptiveLockOnTargetWeight));

    ++lockOnTransitionDiagnosticsFrame;
    if (lockOnTransitionDiagnosticsFrame < 30)
    {
        return;
    }

    const float maxDeltas[] = { transitionMaxDesiredEyeDelta, transitionMaxCollisionPostEyeDelta,
        transitionMaxCollisionRatioDelta, transitionMaxAdaptiveDistanceDelta,
        transitionMaxAdaptiveTargetWeightDelta };
    const char* names[] = { "Desired Eye", "Collision Post Eye", "Collision Ratio",
        "Adaptive Distance", "Adaptive Target Weight" };
    int largestIndex = 0;
    for (int i = 1; i < 5; ++i)
    {
        if (maxDeltas[i] > maxDeltas[largestIndex]) largestIndex = i;
    }
    Logger::Log(std::string("[LockOnTransition][") + direction + "][summary] maxDelta: DesiredEye=" +
        std::to_string(maxDeltas[0]) + " CollisionPostEye=" + std::to_string(maxDeltas[1]) +
        " CollisionRatio=" + std::to_string(maxDeltas[2]) + " AdaptiveDistance=" +
        std::to_string(maxDeltas[3]) + " AdaptiveTargetWeight=" + std::to_string(maxDeltas[4]) +
        " Largest=" + names[largestIndex]);
    lockOnTransitionDiagnosticsActive = false;
}

DarkCameraActor::CameraPose DarkCameraActor::CalculatePose(CameraMode cameraMode, const DirectX::XMFLOAT3& playerPos, float yaw, float pitch) const
{
    CameraPose pose{};

    if (cameraMode == CameraMode::Death)
    {
        DirectX::XMFLOAT3 toBoss{};
        if (auto enemy = enemyHead.lock())
        {
            toBoss = MathHelper::Subtract(enemy->GetComponentLocation(), playerPos);
            toBoss.y = 0.0f;
        }
        if (MathHelper::Length(toBoss) <= FLT_EPSILON)
            toBoss = { sinf(yaw), 0.0f, cosf(yaw) };
        toBoss = MathHelper::Normalize(toBoss);

        const DirectX::XMFLOAT3 worldUp{ 0.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT3 right = MathHelper::Cross(worldUp, toBoss);
        if (MathHelper::Length(right) <= FLT_EPSILON)
            right = { 1.0f, 0.0f, 0.0f };
        right = MathHelper::Normalize(right);

        pose.target = MathHelper::Add(
            MathHelper::Add(playerPos, MathHelper::Multiply(toBoss, deathCameraSettings.bossLookWeight)),
            MathHelper::Multiply(worldUp, deathCameraSettings.lookHeight));
        pose.eye = MathHelper::Add(
            MathHelper::Add(
                MathHelper::Subtract(playerPos, MathHelper::Multiply(toBoss, deathCameraSettings.foregroundDistance)),
                MathHelper::Multiply(right, deathCameraSettings.sideOffset)),
            MathHelper::Multiply(worldUp, deathCameraSettings.cameraHeight));

        const DirectX::XMFLOAT3 viewDirection = MathHelper::Normalize(
            MathHelper::Subtract(pose.target, pose.eye));
        pose.yaw = atan2f(viewDirection.x, viewDirection.z);
        pose.pitch = asinf(std::clamp(viewDirection.y, -1.0f, 1.0f));
        return pose;
    }

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
    cameraCollisionHitName = "None";
    cameraCollisionInitialOverlap = false;
    cameraCollisionHitPoint = {};
    cameraCollisionHitNormal = {};
    cameraCollisionHitDistance = 0.0f;
    if (cameraHitWall)
    {
        cameraCollisionInitialOverlap = hit.initialOverlap;
        cameraCollisionHitPoint = hit.hitPoint;
        cameraCollisionHitNormal = hit.normal;
        cameraCollisionHitDistance = hit.distance;
        float collisionOffset = sphereCastRadius + 0.05f;
        // 少し手前に出す
        idealEye = MathHelper::Add(hit.hitPoint,
            MathHelper::Multiply(hit.normal, collisionOffset));

        cameraHitDistance = hit.distance;
        if (hit.actor)
        {
            cameraCollisionHitName = hit.actor->GetName();
        }
        else if (hit.component)
        {
            cameraCollisionHitName = hit.component->GetName();
        }

    }

    return idealEye;
}


void DarkCameraActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Runtime Camera Tuning", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("TPS FOV", &tpsSettings.fovDegree, 0.1f, 10.0f, 120.0f);
        ImGui::DragFloat("TPS Distance", &tpsSettings.distance, 0.05f, 0.1f, 30.0f);
        ImGui::DragFloat("Boss TPS FOV", &bossTpsFovDegree, 0.1f, 10.0f, 120.0f);
        ImGui::Separator();
        ImGui::DragFloat("LockOn FOV", &lockOnSettings.fovDegree, 0.1f, 10.0f, 120.0f);
        ImGui::DragFloat("LockOn Base Distance", &lockOnSettings.distance, 0.05f, 0.1f, 30.0f);
        ImGui::DragFloat("LockOn Enemy Look Height", &lockOnEnemyLookHeight, 0.05f, -10.0f, 10.0f);
        ImGui::SliderFloat("LockOn LookAt Bias (0=Player, 1=Boss)", &lockOnTargetWeight, 0.0f, 1.0f);
        lockOnTargetWeight = std::clamp(lockOnTargetWeight, 0.0f, 1.0f);
        ImGui::DragFloat("LockOn Zoom In Speed", &lockOnZoomInSpeed, 0.01f, 0.0f, 30.0f);
        ImGui::DragFloat("LockOn Zoom Out Speed", &lockOnZoomOutSpeed, 0.01f, 0.0f, 30.0f);

        ImGui::SeparatorText("Death Camera");
        ImGui::DragFloat("Foreground Distance", &deathCameraSettings.foregroundDistance,
            0.05f, 0.1f, 15.0f);
        ImGui::DragFloat("Side Offset", &deathCameraSettings.sideOffset,
            0.05f, -10.0f, 10.0f);
        ImGui::DragFloat("Camera Height", &deathCameraSettings.cameraHeight,
            0.05f, -10.0f, 10.0f);
        ImGui::DragFloat("Look Height", &deathCameraSettings.lookHeight,
            0.05f, -10.0f, 10.0f);
        ImGui::DragFloat("Boss Look Weight", &deathCameraSettings.bossLookWeight,
            0.05f, -5.0f, 10.0f);
        ImGui::DragFloat("Death Blend Time", &deathCameraSettings.deathBlendTime,
            0.01f, 0.01f, 5.0f, "%.2f sec");

        ImGui::SeparatorText("Camera Shake Presets");
        const auto drawShakePreset = [this](const char* displayName, const char* presetName,
            CameraShakePreset& preset)
        {
            if (!ImGui::TreeNode(displayName)) return;

            ImGui::PushID(presetName);
            ImGui::DragFloat("Intensity", &preset.intensity, 0.05f, 0.0f, 5.0f, "%.2f");
            ImGui::DragFloat("Duration", &preset.duration, 0.01f, 0.01f, 2.0f, "%.2f sec");
            ImGui::DragFloat("Frequency", &preset.frequency, 0.1f, 0.1f, 30.0f, "%.1f Hz");
            ImGui::DragFloat("Position Amount", &preset.positionAmount, 0.005f, 0.0f, 0.5f, "%.3f");
            ImGui::DragFloat("Target Amount", &preset.targetAmount, 0.01f, 0.0f, 1.0f, "%.3f");
            const std::string testLabel = "Test " + std::string(displayName);
            if (ImGui::Button(testLabel.c_str())) PlayCameraShakePreset(presetName);
            ImGui::PopID();
            ImGui::TreePop();
        };

        drawShakePreset("Boss Heavy Landing", BossHeavyLandingPresetName, bossHeavyLandingShake);
        drawShakePreset("Boss Wall Impact", BossWallImpactPresetName, bossWallImpactShake);
        drawShakePreset("Rush Final", RushFinalPresetName, rushFinalShake);

        ImGui::SeparatorText("Active Camera Shake");
        ImGui::Text("Active: %s", shakeActive ? "true" : "false");
        ImGui::Text("Elapsed / Duration: %.3f / %.3f", shakeElapsedTime, shakeDuration);
        ImGui::Text("Envelope: %.3f", currentShakeEnvelope);

        if (ImGui::Button("Reset Camera Tuning")) ResetCameraTuning();
    }

    if (ImGui::CollapsingHeader("FOV Switching Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* modeNames[] = { "TPS", "Focus", "LockOn", "Death" };
        const float currentFov = DirectX::XMConvertToDegrees(mainCameraComponent->GetFov());
        const float desiredFov = isBlending ? blendTargetFovDegree : GetFovDegreeForMode(currentMode);
        ImGui::Text("Boss Battle: %s", IsBossBattle() ? "true" : "false");
        ImGui::Text("Current Mode: %s", modeNames[static_cast<int>(currentMode)]);
        ImGui::Text("Requested Mode: %s", modeNames[static_cast<int>(requestMode)]);
        ImGui::Text("Current FOV: %.2f deg", currentFov);
        ImGui::Text("Desired FOV: %.2f deg", desiredFov);
        ImGui::Text("TPS / Boss TPS / LockOn: %.2f / %.2f / %.2f deg",
            tpsSettings.fovDegree, bossTpsFovDegree, lockOnSettings.fovDegree);
        if (isBlending)
            ImGui::Text("FOV Blend: %.2f -> %.2f deg", blendStartFovDegree, blendTargetFovDegree);
    }

    if (ImGui::CollapsingHeader("Camera Collision Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("World Collision Debug", &showCameraCollisionDebug);
        ImGui::Text("Ideal / Desired Distance: %.3f", desiredCameraDistance);
        ImGui::Text("Actual Distance: %.3f", actualCameraDistance);
        const ImVec4 ratioColor = cameraCollisionRatio < 0.3f
            ? ImVec4(1.0f, 0.15f, 0.15f, 1.0f)
            : cameraCollisionRatio < 0.7f
                ? ImVec4(1.0f, 0.75f, 0.15f, 1.0f)
                : ImVec4(0.75f, 1.0f, 0.75f, 1.0f);
        ImGui::TextColored(ratioColor, "Collision Ratio: %.3f", cameraCollisionRatio);
        ImGui::Text("SphereCast Hit: %s", cameraHitWall ? "true" : "false");
        ImGui::Text("Hit Object: %s", cameraCollisionHitName.c_str());
        ImGui::Text("Player transparency threshold: ratio < 0.300");
    }

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
        ImGui::Text("Wall Target Weight: %.3f (Base)", lockOnWallTargetWeight);
        ImGui::Text("Wall Horizontal Scale: %.3f (fixed)", lockOnWallHorizontalScale);
        ImGui::SliderFloat("Wall Distance Scale", &lockOnWallDistanceScale, 0.1f, 1.0f);
        ImGui::DragFloat("Collision Ratio Hysteresis", &lockOnCollisionRatioHysteresis, 0.005f, 0.0f, 0.2f);
        ImGui::DragFloat("Distance Start", &lockOnDistanceStart, 0.05f, 0.0f, 30.0f);
        ImGui::DragFloat("Distance Full", &lockOnDistanceFull, 0.05f, 0.0f, 30.0f);
        ImGui::DragFloat("Max Distance Add", &lockOnMaxDistanceAdd, 0.05f, 0.0f, 10.0f);
        ImGui::DragFloat("Zoom Out Speed", &lockOnZoomOutSpeed, 0.1f, 0.01f, 30.0f);
        ImGui::DragFloat("Zoom In Speed", &lockOnZoomInSpeed, 0.1f, 0.01f, 30.0f);
        ImGui::DragFloat("Distance Dead Zone", &lockOnDistanceDeadZone, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Composition Lerp Speed", &lockOnCompositionLerpSpeed, 0.1f, 0.1f, 30.0f);
        ImGui::DragFloat("Safe Frame Horizontal Margin",
            &lockOnSafeFrameHorizontalMargin, 0.005f, 0.0f, 0.49f, "%.3f");
        ImGui::DragFloat("Safe Frame Vertical Margin",
            &lockOnSafeFrameVerticalMargin, 0.005f, 0.0f, 0.49f, "%.3f");
        lockOnSafeFrameHorizontalMargin = std::clamp(
            lockOnSafeFrameHorizontalMargin, 0.0f, 0.49f);
        lockOnSafeFrameVerticalMargin = std::clamp(
            lockOnSafeFrameVerticalMargin, 0.0f, 0.49f);
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
        ImGui::Text("Adaptive Collision Ratio: %.3f", lockOnCollisionRatioForAdaptive);
        ImGui::SeparatorText("Collision Diagnostics");
        ImGui::Text("Collision Hit: %s", cameraHitWall ? "true" : "false");
        ImGui::Text("Initial Overlap: %s",
            cameraCollisionInitialOverlap ? "true" : "false");
        ImGui::Text("SphereCast Hit Point: %.3f, %.3f, %.3f",
            cameraCollisionHitPoint.x, cameraCollisionHitPoint.y,
            cameraCollisionHitPoint.z);
        ImGui::Text("SphereCast Hit Normal: %.3f, %.3f, %.3f",
            cameraCollisionHitNormal.x, cameraCollisionHitNormal.y,
            cameraCollisionHitNormal.z);
        ImGui::Text("SphereCast Hit Distance: %.3f",
            cameraCollisionHitDistance);
        ImGui::Text("Player-Boss Distance: %.3f", lockOnEnemyDistance);
        ImGui::Text("Base Distance: %.3f", lockOnSettings.distance);
        ImGui::Text("Horizontal Extent: %.3f", lockOnHorizontalExtent);
        ImGui::Text("Vertical Extent: %.3f", lockOnVerticalExtent);
        ImGui::Text("Required X Distance: %.3f", lockOnRequiredXDistance);
        ImGui::Text("Required Y Distance: %.3f", lockOnRequiredYDistance);
        ImGui::Text("Required Framing Distance: %.3f",
            lockOnRequiredFramingDistance);
        ImGui::Text("Existing Adaptive Distance: %.3f",
            lockOnExistingAdaptiveDistance);
        ImGui::Text("Desired Distance: %.3f", desiredLockOnCameraDistance);
        ImGui::Text("Current Adaptive Distance: %.3f", adaptiveLockOnCameraDistance);
        ImGui::Text("Framing Deficit: %.3f", lockOnFramingDeficit);
        ImGui::Text("Framing Active: %s", lockOnFramingActive ? "YES" : "NO");
        ImGui::SeparatorText("Collision FOV Fallback");
        ImGui::DragFloat("Max Fallback FOV", &lockOnMaxFallbackFovDegree,
            0.1f, lockOnSettings.fovDegree, 90.0f, "%.1f deg");
        ImGui::DragFloat("FOV Expand Speed", &lockOnFovExpandSpeed,
            0.5f, 0.0f, 180.0f, "%.1f deg/s");
        ImGui::DragFloat("FOV Return Speed", &lockOnFovReturnSpeed,
            0.5f, 0.0f, 180.0f, "%.1f deg/s");
        ImGui::DragFloat("Fallback Enter Deficit", &lockOnFovFallbackEnterDeficit,
            0.01f, 0.0f, 10.0f, "%.2f m");
        ImGui::DragFloat("Fallback Exit Deficit", &lockOnFovFallbackExitDeficit,
            0.01f, 0.0f, 10.0f, "%.2f m");
        ImGui::DragFloat("FOV Return Delay", &lockOnFovFallbackReturnDelay,
            0.01f, 0.0f, 2.0f, "%.2f sec");
        lockOnMaxFallbackFovDegree = (std::max)(
            lockOnMaxFallbackFovDegree, lockOnSettings.fovDegree);
        lockOnFovExpandSpeed = (std::max)(lockOnFovExpandSpeed, 0.0f);
        lockOnFovReturnSpeed = (std::max)(lockOnFovReturnSpeed, 0.0f);
        lockOnFovFallbackExitDeficit = (std::max)(
            lockOnFovFallbackExitDeficit, 0.0f);
        lockOnFovFallbackEnterDeficit = (std::max)(
            lockOnFovFallbackEnterDeficit, lockOnFovFallbackExitDeficit);
        lockOnFovFallbackReturnDelay = (std::max)(
            lockOnFovFallbackReturnDelay, 0.0f);
        ImGui::Text("Base LockOn FOV: %.2f", lockOnSettings.fovDegree);
        ImGui::Text("Required FOV: %.2f", lockOnRequiredFovDegree);
        ImGui::Text("Collision Pre Required FOV: %.2f",
            lockOnCollisionPreRequiredFovDegree);
        ImGui::Text("Collision Post Required FOV: %.2f",
            lockOnRequiredFovDegree);
        ImGui::SeparatorText("Player Camera Space");
        ImGui::Text("Player X / Y / Z: %.3f / %.3f / %.3f",
            lockOnFovDiagnostics.player.cameraSpace.x,
            lockOnFovDiagnostics.player.cameraSpace.y,
            lockOnFovDiagnostics.player.cameraSpace.z);
        ImGui::Text("Player In Front: %s",
            lockOnFovDiagnostics.player.inFront ? "true" : "false");
        ImGui::Text("Player Required FOV From X / Y: %.2f / %.2f",
            lockOnFovDiagnostics.player.requiredFovFromX,
            lockOnFovDiagnostics.player.requiredFovFromY);
        ImGui::SeparatorText("Boss Camera Space");
        ImGui::Text("Boss X / Y / Z: %.3f / %.3f / %.3f",
            lockOnFovDiagnostics.boss.cameraSpace.x,
            lockOnFovDiagnostics.boss.cameraSpace.y,
            lockOnFovDiagnostics.boss.cameraSpace.z);
        ImGui::Text("Boss In Front: %s",
            lockOnFovDiagnostics.boss.inFront ? "true" : "false");
        ImGui::Text("Boss Required FOV From X / Y: %.2f / %.2f",
            lockOnFovDiagnostics.boss.requiredFovFromX,
            lockOnFovDiagnostics.boss.requiredFovFromY);
        ImGui::Text("Framing Limiter: %s", lockOnFovDiagnostics.limiter.c_str());
        ImGui::Text("Target FOV: %.2f", lockOnTargetFovDegree);
        ImGui::Text("Current FOV: %.2f", lockOnCurrentFovDegree);
        ImGui::Text("Max Fallback FOV: %.2f", lockOnMaxFallbackFovDegree);
        ImGui::Text("FOV Fallback Active: %s",
            lockOnFovFallbackActive ? "YES" : "NO");
        ImGui::Text("Framing Deficit: %.3f", lockOnFramingDeficit);
        ImGui::Text("Camera Collision Ratio: %.3f", cameraCollisionRatio);
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
