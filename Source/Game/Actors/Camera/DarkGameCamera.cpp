#include "pch.h"
#include "DarkGameCamera.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Physics/CollisionFunction.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Utility/Time.h"


bool DarkCameraActor::HasValidLockOnTarget() const
{
	const auto target = enemyHead.lock();
	const auto* enemy = target ? dynamic_cast<const Enemy*>(target->GetOwner()) : nullptr;
	return enemy && !enemy->IsDefeated() && !enemy->IsPendingKill();
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
	// Pitch
	lockOnPitchDegree = -10.0f;

	isExternalBlending = false;
	ResetLockOnAdaptiveState();

	initialCorridorTpsSettings = corridorTpsSettings;
	initialStandardTpsSettings = standardTpsSettings;
	initialBossTpsSettings = bossTpsSettings;
	initialLockOnSettings = lockOnSettings;
	initialCompactEnemyLockOnProfile = compactEnemyLockOnProfile;
	initialStandardEnemyLockOnProfile = standardEnemyLockOnProfile;
	initialLockOnEnemyLookHeight = lockOnEnemyLookHeight;
	initialLockOnTargetWeight = lockOnTargetWeight;
	initialLockOnZoomInSpeed = lockOnZoomInSpeed;
	initialLockOnZoomOutSpeed = lockOnZoomOutSpeed;
}

void DarkCameraActor::Update(float deltaTime)
{
	auto playerHeadShared = playerHead.lock();
	if (!playerHeadShared)
	{
		CancelOffscreenAttackAssist();
		return;
	}

	const DirectX::XMFLOAT3 playerPos =
		playerHeadShared->GetComponentLocation();
	const bool preservePrisonOpeningReturnPose =
		holdPrisonOpeningTpsReturnPoseForOneUpdate;
	holdPrisonOpeningTpsReturnPoseForOneUpdate = false;

	UpdateCameraPose(deltaTime, playerPos, preservePrisonOpeningReturnPose);
	ApplyCameraFov(preservePrisonOpeningReturnPose);
	ResolvePostPoseCollision(deltaTime, preservePrisonOpeningReturnPose);
	ApplyPostCollisionFov(deltaTime, preservePrisonOpeningReturnPose);
	CommitCameraPose();
}

void DarkCameraActor::UpdateCameraPose(float deltaTime, const DirectX::XMFLOAT3& playerPos, const bool preservePrisonOpeningReturnPose)
{
	if (!preservePrisonOpeningReturnPose && !isExternalBlending && !isBlending && currentMode == CameraMode::LockOn)
	{
		UpdateLockOnComposition(deltaTime);
	}

	if (!preservePrisonOpeningReturnPose)
	{
		if (isExternalBlending)
		{
			UpdateExternalBlend(deltaTime);
		}
		else if (isBlending)
		{
			UpdateBlend(deltaTime);
		}
		else if (requestMode != currentMode)
		{
			StartBlend(currentMode, requestMode);
		}
		else
		{
			UpdateDesiredRotation(deltaTime);
			if (requestMode != currentMode)
			{
				StartBlend(currentMode, requestMode);
			}
			else
			{
				UpdateRotation(deltaTime);
				currentPose = CalculatePose(currentMode, playerPos, currentYaw, currentPitch);
			}
		}
	}

	if (!preservePrisonOpeningReturnPose)
	{
		UpdateCombatPresentation(deltaTime);
		compositionLookTarget = currentPose.target;
	}
}

void DarkCameraActor::ApplyCameraFov(const bool preservePrisonOpeningReturnPose)
{
	if (preservePrisonOpeningReturnPose)
	{
		mainCameraComponent->SetFov(prisonOpeningReturnFov);
	}
	else if (!isExternalBlending)
	{
		float fovDegree = 0.0f;
		if (isBlending)
		{
			const float effectiveBlendDuration = requestMode == CameraMode::Death ? (std::max)(deathCameraSettings.deathBlendTime, 0.01f) : (blendDurationOverride > 0.0f ? blendDurationOverride : blendDuration);
			const float blendT = std::clamp(blendTime / effectiveBlendDuration, 0.0f, 1.0f); fovDegree = std::lerp(blendStartFovDegree, blendTargetFovDegree, blendT);
		}
		else
		{
			fovDegree = GetFovDegreeForMode(currentMode);
		}
		mainCameraComponent->SetFov(DirectX::XMConvertToRadians(fovDegree));
	}
}

void DarkCameraActor::ResolvePostPoseCollision(const float deltaTime, const bool preservePrisonOpeningReturnPose)
{
	if (!preservePrisonOpeningReturnPose)
	{
		collisionPreEyePosition = currentPose.eye;
		desiredCameraDistance = MathHelper::Distance(
			currentPose.target, collisionPreEyePosition);

		currentPose.eye = ResolveCameraCollision(currentPose.target, currentPose.eye);
		collisionPostEyePosition = currentPose.eye;
		UpdateLockOnWallLateralEscape(deltaTime);
		collisionPostEyePosition = currentPose.eye;
		UpdateLockOnCompositionLookCorrection(deltaTime);
		actualCameraDistance = MathHelper::Distance(currentPose.target, collisionPostEyePosition);
		cameraCollisionRatio = desiredCameraDistance > FLT_EPSILON ? actualCameraDistance / desiredCameraDistance : 1.0f;
		lockOnFramingDeficit = currentMode == CameraMode::LockOn ? lockOnRequiredFramingDistance - actualCameraDistance : 0.0f;

		if (currentMode == CameraMode::LockOn && requestMode == CameraMode::LockOn && !isBlending && !isExternalBlending)
		{
			currentPose.target = compositionLookTarget;
		}
	}
}

void DarkCameraActor::ApplyPostCollisionFov(const float deltaTime, const bool preservePrisonOpeningReturnPose)
{
	if (!preservePrisonOpeningReturnPose && !isExternalBlending && !isBlending && currentMode == CameraMode::LockOn)
	{
		UpdateLockOnFovFallback(deltaTime);
	}
	if (!preservePrisonOpeningReturnPose)
		ApplyCombatPresentationFov();
}

void DarkCameraActor::CommitCameraPose()
{
	const CameraPose renderPose = currentPose;
	SetPosition(renderPose.eye);
	mainCameraComponent->lookTarget = renderPose.target;
	mainCameraComponent->useLookTarget = true;
}
void DarkCameraActor::PlayCameraShakePreset(const std::string& presetName)
{
	if (auto* scene = GetOwnerScene())
		if (auto* manager = scene->GetCameraManager())
			manager->PlayCameraShakePreset(presetName);
}

void DarkCameraActor::RequestOffscreenAttackAssist(const DirectX::XMFLOAT3& worldPosition, const float strength, const float duration)
{
	const ScreenProjectionResult projection = ProjectWorldPositionForOffscreenAssist(worldPosition);

	const auto playerHeadShared = playerHead.lock();
	const auto* player = playerHeadShared ? dynamic_cast<const Player*>(playerHeadShared->GetOwner()) : nullptr;
	if (currentMode != CameraMode::TPS || requestMode != CameraMode::TPS ||
		isBlending || isExternalBlending || combatPresentationMode == CombatPresentationMode::Rush ||
		(player && player->IsRushActive()) || strength <= 0.0f || duration <= 0.0f || !projection.valid || projection.insideSafeFrame)
	{
		return;
	}

	DirectX::XMFLOAT3 toBoss = MathHelper::Subtract(worldPosition, currentPose.target);
	toBoss.y = 0.0f;
	if (MathHelper::Length(toBoss) <= FLT_EPSILON)
		return;

	offscreenAttackAssist.active = true;
	offscreenAttackAssist.worldPosition = worldPosition;
	offscreenAttackAssist.strength = strength;
	offscreenAttackAssist.duration = duration;
	offscreenAttackAssist.elapsed = 0.0f;
	offscreenAssistPitchStart = currentPitch;

	const float targetYaw = atan2f(toBoss.x, toBoss.z);
	float yawDelta = MathHelper::ClampAngle(targetYaw - desiredYaw);
	const float bossOnCameraRight = toBoss.x * cosf(currentYaw) - toBoss.z * sinf(currentYaw);
	if (std::abs(bossOnCameraRight) > FLT_EPSILON)
		offscreenAttackAssist.turnSign = bossOnCameraRight > 0.0f ? 1.0f : -1.0f;
	else
		offscreenAttackAssist.turnSign = yawDelta < 0.0f ? -1.0f : 1.0f;
}

void DarkCameraActor::CancelOffscreenAttackAssist()
{
	offscreenAttackAssist.active = false;
}

DarkCameraActor::ScreenProjectionResult DarkCameraActor::ProjectWorldPositionForOffscreenAssist(const DirectX::XMFLOAT3& worldPosition) const
{
	ScreenProjectionResult result{};
	if (!mainCameraComponent)
		return result;

	using namespace DirectX;
	const XMMATRIX view = XMLoadFloat4x4(&mainCameraComponent->GetView());
	const XMMATRIX projection = XMLoadFloat4x4(&mainCameraComponent->GetProjection());
	const XMVECTOR world = XMVectorSet(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f);
	const XMVECTOR clip = XMVector4Transform(world, view * projection);

	XMFLOAT4 clipPosition{};
	XMStoreFloat4(&clipPosition, clip);
	result.clipW = clipPosition.w;
	constexpr float clipWEpsilon = 0.0001f;
	if (!std::isfinite(clipPosition.x) || !std::isfinite(clipPosition.y) ||
		!std::isfinite(clipPosition.z) || !std::isfinite(clipPosition.w) ||
		std::abs(clipPosition.w) <= clipWEpsilon)
	{
		return result;
	}

	result.valid = true;
	result.inFront = clipPosition.w > clipWEpsilon;
	result.ndc = {
		clipPosition.x / clipPosition.w,
		clipPosition.y / clipPosition.w,
		clipPosition.z / clipPosition.w
	};

	float viewportX = 0.0f;
	float viewportY = 0.0f;
	float viewportWidth = 0.0f;
	float viewportHeight = 0.0f;
	Graphics::GetViewport(viewportX, viewportY, viewportWidth, viewportHeight);
	if (viewportWidth <= 0.0f || viewportHeight <= 0.0f)
	{
		result.valid = false;
		return result;
	}

	result.screenPosition =
	{
		viewportX + (result.ndc.x + 1.0f) * 0.5f * viewportWidth,
		viewportY + (1.0f - result.ndc.y) * 0.5f * viewportHeight
	};
	result.insideViewport = result.inFront && std::abs(result.ndc.x) <= 1.0f && std::abs(result.ndc.y) <= 1.0f && result.ndc.z >= 0.0f && result.ndc.z <= 1.0f;

	const float horizontalSafeExtent = 1.0f - 2.0f * std::clamp(offscreenAssistHorizontalSafeMargin, 0.0f, 0.49f);
	const float verticalSafeExtent = 1.0f - 2.0f * std::clamp(offscreenAssistVerticalSafeMargin, 0.0f, 0.49f);
	result.insideSafeFrame = result.insideViewport && std::abs(result.ndc.x) <= horizontalSafeExtent && std::abs(result.ndc.y) <= verticalSafeExtent;
	return result;
}

DarkCameraActor::WorldScreenProjection DarkCameraActor::ProjectWorldPositionForUI(const DirectX::XMFLOAT3& worldPosition) const
{
	const auto projection = ProjectWorldPositionForOffscreenAssist(worldPosition);
	WorldScreenProjection result{};
	result.valid = projection.valid;
	result.inFront = projection.inFront;
	result.insideViewport = projection.insideViewport;
	result.screenPosition = projection.screenPosition;
	result.ndc = projection.ndc;
	result.clipW = projection.clipW;
	return result;
}

bool DarkCameraActor::IsNormalEnemyLockOnProjectionWithinScreenRadius(const WorldScreenProjection& projection) const
{
	const float screenHeight = Graphics::GetScreenHeight();
	const float aspect = screenHeight > FLT_EPSILON ? Graphics::GetScreenWidth() / screenHeight : 1.0f;
	const float normalizedDistanceSq = (projection.ndc.x * aspect) * (projection.ndc.x * aspect) + projection.ndc.y * projection.ndc.y;
	const float radius = (std::max)(normalEnemyLockOnScreenRadius, 0.0f);
	return normalizedDistanceSq <= radius * radius;
}
void DarkCameraActor::UpdateOffscreenAttackAssist(const float deltaTime)
{
	if (!offscreenAttackAssist.active)
		return;

	const auto playerHeadShared = playerHead.lock();
	const auto* player = playerHeadShared ? dynamic_cast<const Player*>(playerHeadShared->GetOwner()) : nullptr;
	if (currentMode != CameraMode::TPS || requestMode != CameraMode::TPS ||
		isBlending || isExternalBlending || !playerHeadShared ||
		combatPresentationMode == CombatPresentationMode::Rush ||
		(player && player->IsRushActive()))
	{
		CancelOffscreenAttackAssist();
		return;
	}

	const ScreenProjectionResult projection = ProjectWorldPositionForOffscreenAssist(offscreenAttackAssist.worldPosition);
	if (!projection.valid ||
		projection.insideSafeFrame ||
		offscreenAttackAssist.elapsed >= offscreenAttackAssist.duration)
	{
		CancelOffscreenAttackAssist();
		return;
	}

	DirectX::XMFLOAT3 toBoss = MathHelper::Subtract(offscreenAttackAssist.worldPosition, currentPose.target);
	toBoss.y = 0.0f;
	if (MathHelper::Length(toBoss) <= FLT_EPSILON)
	{
		CancelOffscreenAttackAssist();
		return;
	}

	const float targetYaw = atan2f(toBoss.x, toBoss.z);
	float yawDelta = MathHelper::ClampAngle(targetYaw - desiredYaw);
	constexpr float behindStabilizationRange = DirectX::XM_PI / 180.0f * 2.0f;
	if (std::abs(std::abs(yawDelta) - DirectX::XM_PI) <= behindStabilizationRange)
	{
		yawDelta = std::copysign(std::abs(yawDelta), offscreenAttackAssist.turnSign);
	}

	const float maxStep = DirectX::XMConvertToRadians(offscreenAssistMaxAngularSpeedDegree) * offscreenAttackAssist.strength * deltaTime;
	const float appliedYawStep = std::clamp(yawDelta, -maxStep, maxStep);
	desiredYaw = MathHelper::ClampAngle(desiredYaw + appliedYawStep);

	const float pitchBlendTime = (std::max)(offscreenAssistPitchBlendTime, 0.0f);
	const float pitchProgress = pitchBlendTime > FLT_EPSILON ? (offscreenAttackAssist.elapsed + deltaTime) / pitchBlendTime : 1.0f;
	const float offscreenAssistPitchBlend = MathHelper::SmoothStep01(pitchProgress);
	const float targetPitch = DirectX::XMConvertToRadians(offscreenAssistTargetPitchDegree);
	currentPitch = desiredPitch = std::lerp(offscreenAssistPitchStart, targetPitch, offscreenAssistPitchBlend);

	offscreenAttackAssist.elapsed += deltaTime;
	if (offscreenAttackAssist.elapsed >= offscreenAttackAssist.duration)
		CancelOffscreenAttackAssist();
}

// ブレンドを開始する
void DarkCameraActor::StartDeathMode(const std::shared_ptr<GruxEnemy>& boss, std::function<void()> onBlendFinished)
{
	deathBoss = boss;
	deathBlendFinished = std::move(onBlendFinished);
	SetRequestMode(CameraMode::Death);
}

void DarkCameraActor::ClearDeathTarget()
{
	deathBoss.reset();
	deathBlendFinished = nullptr;
}

void DarkCameraActor::SetTpsCameraProfile(const TpsCameraProfile profile)
{
	if (tpsCameraProfile == profile)
		return;

	tpsCameraProfile = profile;

	if (!isExternalBlending && currentMode == CameraMode::TPS && requestMode == CameraMode::TPS)
	{
		StartBlend(CameraMode::TPS, CameraMode::TPS);
		blendDurationOverride = 0.30f;
	}
}

void DarkCameraActor::StartBlend(CameraMode from, CameraMode to)
{
	CancelOffscreenAttackAssist();

	// プレイヤーの位置を取得
	auto playerHeadShared = playerHead.lock();
	if (!playerHeadShared)
	{
		return;
	}
	DirectX::XMFLOAT3 playerPos = playerHeadShared->GetComponentLocation();
	blendTime = 0.0f;
	blendDurationOverride = (to == CameraMode::Focus) ? focusBlendDuration : 0.0f;
	isBlending = true;

	if (MathHelper::Distance(currentPose.eye, currentPose.target) <= FLT_EPSILON)
	{
		currentPose = CalculatePose(currentMode, playerPos, currentYaw, currentPitch);
	}

	// Collision補正を含め、直前まで実際に表示していたPoseから開始する。
	blendStartPose = currentPose;
	blendStartPose.yaw = currentYaw;
	blendStartPose.pitch = currentPitch;
	const DirectX::XMFLOAT3 startEyeDirection = MathHelper::Normalize(MathHelper::Subtract(blendStartPose.eye, blendStartPose.target));
	blendStartEyeYaw = atan2f(startEyeDirection.x, startEyeDirection.z);
	blendStartEyePitch = asinf(std::clamp(startEyeDirection.y, -1.0f, 1.0f));
	blendStartEyeDistance = MathHelper::Distance(blendStartPose.eye, blendStartPose.target);
	float targetYaw = currentYaw;
	float targetPitch = currentPitch;
	CameraPose selectedDeathPose{};
	bool hasSelectedDeathPose = false;
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
		InitializeLockOnTransitionDistanceSnapshot();
		CameraDirectionInfo info = CreateLockOnInfo();
		targetYaw = info.yaw;
		targetPitch = info.pitch;
		break;

	case CameraMode::Death:
	{
		selectedDeathPose = CalculateDeathPoseWithSideOffset(
			playerPos, currentYaw, deathCameraSettings.sideOffset);
		deathBlendSideOffset = deathCameraSettings.sideOffset;
		hasSelectedDeathPose = true;
		const CameraPose& deathPose = selectedDeathPose;
		targetYaw = deathPose.yaw;
		targetPitch = deathPose.pitch;
		break;
	}
	}
	blendTargetPose = hasSelectedDeathPose ? selectedDeathPose : CalculatePose(to, playerPos, targetYaw, targetPitch);
	blendStartFovDegree = DirectX::XMConvertToDegrees(mainCameraComponent->GetFov());
	blendTargetFovDegree = GetFovDegreeForMode(to);
}

bool DarkCameraActor::IsBossBattle() const
{
	const auto head = playerHead.lock();
	const auto player = head ? dynamic_cast<Player*>(head->GetOwner()) : nullptr;
	return player && player->IsBossBattle();
}

bool DarkCameraActor::IsGruxLockOnTarget() const
{
	const auto target = enemyHead.lock();
	return target && dynamic_cast<const GruxEnemy*>(target->GetOwner());
}

const DarkCameraActor::CameraCompositionSettings& DarkCameraActor::GetActiveTpsSettings() const
{
	if (IsBossBattle())
		return bossTpsSettings;
	return tpsCameraProfile == TpsCameraProfile::Corridor ? corridorTpsSettings : standardTpsSettings;
}

DarkCameraActor::LockOnProfile DarkCameraActor::GetActiveLockOnProfile() const
{
	if (IsGruxLockOnTarget())
	{
		return
		{
			lockOnSettings.fovDegree,
			lockOnMaxFallbackFovDegree,
			lockOnSettings.distance,
			lockOnMaxDistanceAdd
		};
	}

	const auto target = enemyHead.lock();
	const auto enemy = target ? dynamic_cast<const Enemy*>(target->GetOwner()) : nullptr;
	return enemy && enemy->GetLockOnCameraProfile() == EnemyLockOnCameraProfile::Standard ? standardEnemyLockOnProfile : compactEnemyLockOnProfile;
}

float DarkCameraActor::GetFovDegreeForMode(CameraMode mode) const
{
	if (mode == CameraMode::LockOn) return GetActiveLockOnProfile().fovDegree;
	if (mode == CameraMode::Focus) return focusSettings.fovDegree;
	if (mode == CameraMode::Death) return deathCameraSettings.fovDegree;
	return GetActiveTpsSettings().fovDegree;
}

void DarkCameraActor::ResetCameraTuning()
{
	corridorTpsSettings = initialCorridorTpsSettings;
	standardTpsSettings = initialStandardTpsSettings;
	bossTpsSettings = initialBossTpsSettings;
	lockOnSettings.fovDegree = initialLockOnSettings.fovDegree;
	lockOnSettings.distance = initialLockOnSettings.distance;
	compactEnemyLockOnProfile = initialCompactEnemyLockOnProfile;
	standardEnemyLockOnProfile = initialStandardEnemyLockOnProfile;
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
}

// 外部のカメラアクターとのブレンド用の関数
void DarkCameraActor::SnapToTpsDirection(const DirectX::XMFLOAT3& direction)
{
	if (fabs(direction.x) < 0.001f && fabs(direction.z) < 0.001f)
		return;

	CancelOffscreenAttackAssist();
	isBlending = false;
	blendTime = 0.0f;
	deathBlendFinished = nullptr;
	isExternalBlending = false;
	externalBlendTime = 0.0f;
	externalBlendDuration = 0.0f;
	finishedExternalBlend = nullptr;

	currentMode = CameraMode::TPS;
	requestMode = CameraMode::TPS;
	currentYaw = atan2f(direction.x, direction.z);
	desiredYaw = currentYaw;
	desiredPitch = currentPitch;

	if (const auto playerHeadShared = playerHead.lock())
	{
		currentPose = CalculatePose(
			CameraMode::TPS,
			playerHeadShared->GetComponentLocation(),
			currentYaw,
			currentPitch);
		compositionLookTarget = currentPose.target;
	}

	mainCameraComponent->SetYawAndPitch(currentYaw, currentPitch);
}

void DarkCameraActor::StartExternalBlend(const CameraPose& start, const CameraPose& target, float duration, std::function<void()> finishExternalBlend)
{
	CancelOffscreenAttackAssist();
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
void DarkCameraActor::CancelExternalBlend()
{
	isExternalBlending = false;
	externalBlendTime = 0.0f;
	externalBlendDuration = 0.0f;
	finishedExternalBlend = nullptr;
}

void DarkCameraActor::SnapToExternalPose(const CameraPose& pose)
{
	CancelExternalBlend();
	currentPose = pose;
	currentYaw = pose.yaw;
	currentPitch = pose.pitch;
	desiredYaw = pose.yaw;
	desiredPitch = pose.pitch;
	currentMode = CameraMode::TPS;
	requestMode = CameraMode::TPS;
	compositionLookTarget = pose.target;
	mainCameraComponent->SetYawAndPitch(currentYaw, currentPitch);
	mainCameraComponent->SetFov(DirectX::XMConvertToRadians(GetFovDegreeForMode(CameraMode::TPS)));
	mainCameraComponent->lookTarget = pose.target;
	mainCameraComponent->useLookTarget = true;
	SetPosition(pose.eye);
}

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

bool DarkCameraActor::GetPrisonOpeningTpsPose(PrisonOpeningTpsPose& outPose)
{
	const auto playerHeadShared = playerHead.lock();
	const auto playerActor = playerHeadShared ? playerHeadShared->GetOwner() : nullptr;
	if (!playerHeadShared || !playerActor)
		return false;

	DirectX::XMFLOAT3 forward = playerActor->GetForward();
	forward.y = 0.0f;
	if (MathHelper::Length(forward) <= FLT_EPSILON)
		forward = { sinf(currentYaw), 0.0f, cosf(currentYaw) };
	else
		forward = MathHelper::Normalize(forward);

	const float yaw = atan2f(forward.x, forward.z);
	CameraPose pose = CalculatePose(
		CameraMode::TPS,
		playerHeadShared->GetComponentLocation(),
		yaw,
		0.0f);
	outPose.desiredEye = pose.eye;
	outPose.yaw = pose.yaw;
	outPose.pitch = pose.pitch;
	outPose.desiredDistance = MathHelper::Distance(pose.target, pose.eye);
	pose.eye = ResolveCameraCollision(pose.target, pose.eye);

	outPose.eye = pose.eye;
	outPose.target = pose.target;
	outPose.fov = DirectX::XMConvertToRadians(GetFovDegreeForMode(CameraMode::TPS));
	outPose.actualDistance = MathHelper::Distance(pose.target, pose.eye);
	outPose.collisionRatio = outPose.desiredDistance > FLT_EPSILON ? outPose.actualDistance / outPose.desiredDistance : 1.0f;
	outPose.hitWall = cameraHitWall;
	return true;
}

void DarkCameraActor::PreparePrisonOpeningTpsReturn(const PrisonOpeningTpsPose& pose)
{
	const auto playerHeadShared = playerHead.lock();
	const auto playerActor = playerHeadShared ? playerHeadShared->GetOwner() : nullptr;
	if (!playerActor)
		return;

	SnapToTpsDirection(playerActor->GetForward());
	currentYaw = pose.yaw;
	desiredYaw = pose.yaw;
	currentPitch = pose.pitch;
	desiredPitch = pose.pitch;
	currentPose = { pose.eye, pose.target, currentYaw, currentPitch };
	compositionLookTarget = currentPose.target;
	collisionPreEyePosition = pose.desiredEye;
	collisionPostEyePosition = pose.eye;
	desiredCameraDistance = pose.desiredDistance;
	actualCameraDistance = pose.actualDistance;
	cameraCollisionRatio = pose.collisionRatio;
	cameraHitWall = pose.hitWall;
	prisonOpeningReturnFov = pose.fov;
	mainCameraComponent->SetYawAndPitch(currentYaw, currentPitch);
	mainCameraComponent->SetFov(pose.fov);
	mainCameraComponent->lookTarget = pose.target;
	mainCameraComponent->useLookTarget = true;
	SetPosition(pose.eye);
	holdPrisonOpeningTpsReturnPoseForOneUpdate = true;
}


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
		currentPose = externalTargetPose;
		currentYaw = externalTargetPose.yaw;
		currentPitch = externalTargetPose.pitch;
		desiredYaw = currentYaw;
		desiredPitch = currentPitch;
		currentMode = CameraMode::TPS;
		requestMode = CameraMode::TPS;
		mainCameraComponent->SetYawAndPitch(currentYaw, currentPitch);

		isBlending = false;
		blendDurationOverride = 0.0f;
		blendTime = 0.0f;
		deathBlendFinished = nullptr;
		isExternalBlending = false;
		externalBlendTime = externalBlendDuration;

		auto callback = std::move(finishedExternalBlend);
		finishedExternalBlend = nullptr;
		if (callback)
			callback();
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
		: (blendDurationOverride > 0.0f ? blendDurationOverride : blendDuration);
	float t = std::clamp(blendTime / currentBlendDuration, 0.0f, 1.0f);

	// 移動中のPlayer/Enemyを反映し、Blend完了次フレームとの差を残さない。
	CameraPose targetPose = blendTargetPose;
	if (auto head = playerHead.lock())
	{
		const DirectX::XMFLOAT3 currentPlayerPos = head->GetComponentLocation();
		targetPose = requestMode == CameraMode::Death
			? CalculateDeathPoseWithSideOffset(
				currentPlayerPos,
				blendTargetPose.yaw,
				deathBlendSideOffset)
			: CalculatePose(
				requestMode,
				currentPlayerPos,
				blendTargetPose.yaw,
				blendTargetPose.pitch);
	}

	// Target、視線方向、Distanceを表示中Poseから同じBlend率で補間する。
	currentPose.target = MathHelper::Lerp(blendStartPose.target, targetPose.target, t);
	const DirectX::XMFLOAT3 targetEyeDirection = MathHelper::Normalize(MathHelper::Subtract(targetPose.eye, targetPose.target));
	const float targetEyeYaw = atan2f(targetEyeDirection.x, targetEyeDirection.z);
	const float targetEyePitch = asinf(std::clamp(targetEyeDirection.y, -1.0f, 1.0f));
	const float eyeYaw = MathHelper::LerpAngle(blendStartEyeYaw, targetEyeYaw, t);
	const float eyePitch = std::lerp(blendStartEyePitch, targetEyePitch, t);
	const float eyeDistance = std::lerp(blendStartEyeDistance, MathHelper::Distance(targetPose.eye, targetPose.target), t);
	const DirectX::XMFLOAT3 eyeDirection =
	{
		sinf(eyeYaw) * cosf(eyePitch),
		sinf(eyePitch),
		cosf(eyeYaw) * cosf(eyePitch)
	};
	currentPose.eye = MathHelper::Add(currentPose.target, MathHelper::Multiply(eyeDirection, eyeDistance));
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

	info.pitch = XMConvertToRadians(lockOnPitchDegree);

	return info;
}

// 目標の方向を更新する関数
void DarkCameraActor::UpdateDesiredRotation(float deltaTime)
{
	auto intent = inputComponent->GetIntent();
	// 右スティックの入力値
	DirectX::XMFLOAT2 rightStick = intent.rightMove;
	const float rightStickMagnitude = std::sqrt(
		rightStick.x * rightStick.x + rightStick.y * rightStick.y);
	switch (currentMode)
	{
	case CameraMode::TPS:
		if (rightStickMagnitude >
			offscreenAssistRightStickCancelThreshold)
		{
			CancelOffscreenAttackAssist();
		}
		// 右スティックがYawになる
		desiredYaw += rightStick.x * rotateSpeed * deltaTime;
		desiredPitch += rightStick.y * rotateSpeed * deltaTime;
		UpdateOffscreenAttackAssist(deltaTime);
		break;
	case CameraMode::Focus:
		if (!(std::abs(inputComponent->GetIntent().leftMove.x) >= FLT_EPSILON && std::abs(inputComponent->GetIntent().leftMove.z) >= FLT_EPSILON))
		{// player の左スティックが入力されていなかったら、
			// 右スティックを動かす
			desiredYaw += rightStick.x * rotateSpeed * deltaTime;
			desiredPitch += rightStick.y * rotateSpeed * deltaTime;
		}
		break;
	case CameraMode::LockOn:
	{
		const auto enemyHeadShared = enemyHead.lock();
		if (!enemyHeadShared)
		{
			SetRequestMode(CameraMode::TPS);
			break;
		}
		// 敵とプレイヤーが向いている方向
		if (auto playerHeadShared = playerHead.lock())
		{
			if (auto player = playerHeadShared->GetOwner())
			{
				if (auto enemy = enemyHeadShared->GetOwner())
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
	case CameraMode::Death:
		break;
	}

	// 
	desiredPitch = std::clamp(desiredPitch, DirectX::XMConvertToRadians(minPitchDegree), DirectX::XMConvertToRadians(maxPitchDegree));
	desiredYaw = MathHelper::ClampAngle(desiredYaw);
}

// 実際の方向を更新する関数
void DarkCameraActor::UpdateRotation(float deltaTime)
{
	float rotateRate = 20.0f;
	currentYaw = MathHelper::LerpAngle(currentYaw, desiredYaw, rotateRate * deltaTime);
	currentPitch = std::lerp(currentPitch, desiredPitch, rotateRate * deltaTime);
	mainCameraComponent->SetYawAndPitch(currentYaw, currentPitch);
}

void DarkCameraActor::UpdateLockOnComposition(float deltaTime)
{
	const LockOnProfile activeProfile = GetActiveLockOnProfile();
	auto playerHeadShared = playerHead.lock();
	auto enemyHeadShared = enemyHead.lock();
	if (!playerHeadShared || !enemyHeadShared)
	{
		return;
	}

	lockOnEnemyDistance = MathHelper::Distance(playerHeadShared->GetComponentLocation(), enemyHeadShared->GetComponentLocation());

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

	const float collisionRange = std::max<float>(lockOnCollisionStartRatio - lockOnCollisionFullRatio, FLT_EPSILON);
	const float collisionInput = (lockOnCollisionStartRatio - lockOnCollisionRatioForAdaptive) / collisionRange;
	const float targetCollisionStrength = MathHelper::SmoothStep01(collisionInput);

	const float enemyRange = std::max<float>(lockOnDistanceFull - lockOnDistanceStart, FLT_EPSILON);
	const float enemyDistanceInput = (lockOnDistanceForZoom - lockOnDistanceStart) / enemyRange;
	const float targetDistanceStrength = MathHelper::SmoothStep01(enemyDistanceInput);

	const float lerpRate = std::clamp(deltaTime * lockOnCompositionLerpSpeed, 0.0f, 1.0f);
	lockOnCollisionStrength = std::lerp(lockOnCollisionStrength, targetCollisionStrength, lerpRate);
	lockOnDistanceStrength = targetDistanceStrength;

	// 壁際の初期比較はDistance Adaptiveのみ。Weight/OffsetはBase値を維持する。
	adaptiveLockOnTargetWeight = lockOnTargetWeight;
	adaptiveLockOnHorizontalOffset = lockOnSettings.horizontalOffset;

	lockOnExistingAdaptiveDistance = activeProfile.distance + activeProfile.maxDistanceAdd * lockOnDistanceStrength;
	lockOnRequiredFramingDistance = CalculateRequiredLockOnFramingDistance();
	desiredLockOnCameraDistance = ClampEnemyLockOnDistanceToProfile(
		(std::max)(lockOnExistingAdaptiveDistance, lockOnRequiredFramingDistance), activeProfile);
	const float currentLockOnZoomSpeed = desiredLockOnCameraDistance > currentLockOnCameraDistance ? lockOnZoomOutSpeed : lockOnZoomInSpeed;
	const float zoomLerpRate = std::clamp(deltaTime * currentLockOnZoomSpeed, 0.0f, 1.0f);
	currentLockOnCameraDistance = std::lerp(currentLockOnCameraDistance, desiredLockOnCameraDistance, zoomLerpRate);
	adaptiveLockOnCameraDistance = currentLockOnCameraDistance * std::lerp(1.0f, lockOnWallDistanceScale, lockOnCollisionStrength);
}

float DarkCameraActor::CalculateRequiredLockOnFramingDistance()
{
	const LockOnProfile activeProfile = GetActiveLockOnProfile();
	lockOnRequiredXDistance = 0.0f;
	lockOnRequiredYDistance = 0.0f;

	const float minimumLockOnDistance = (std::max)(activeProfile.distance, 0.1f);
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

	const float verticalFov = DirectX::XMConvertToRadians(activeProfile.fovDegree);
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

float DarkCameraActor::EvaluateRequiredLockOnFovFromEye(const DirectX::XMFLOAT3& eye, LockOnFovDiagnostics& diagnostics, bool& outValid) const
{
	return EvaluateRequiredLockOnFovFromEye(eye, currentPose.target, diagnostics, outValid);
}

float DarkCameraActor::EvaluateRequiredLockOnFovFromEye(
	const DirectX::XMFLOAT3& eye,
	const DirectX::XMFLOAT3& lookTarget,
	LockOnFovDiagnostics& diagnostics,
	bool& outValid) const
{
	outValid = false;
	diagnostics = {};
	const auto playerHeadShared = playerHead.lock();
	const auto enemyHeadShared = enemyHead.lock();
	if (!playerHeadShared || !enemyHeadShared)
		return GetActiveLockOnProfile().fovDegree;

	const DirectX::XMFLOAT3 forwardVector = MathHelper::Subtract(
		lookTarget, eye);
	if (MathHelper::Length(forwardVector) <= 0.1f)
		return GetActiveLockOnProfile().fovDegree;

	const DirectX::XMFLOAT3 cameraForward = MathHelper::Normalize(forwardVector);
	const DirectX::XMFLOAT3 worldUp{ 0.0f, 1.0f, 0.0f };
	const DirectX::XMFLOAT3 rightVector = MathHelper::Cross(worldUp, cameraForward);
	if (MathHelper::Length(rightVector) <= FLT_EPSILON)
		return GetActiveLockOnProfile().fovDegree;

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
		return GetActiveLockOnProfile().fovDegree;
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
		return GetActiveLockOnProfile().fovDegree;
	}

	const float requiredFovDegree = DirectX::XMConvertToDegrees(
		2.0f * std::atan(requiredTanVertical));
	if (!std::isfinite(requiredFovDegree))
		return GetActiveLockOnProfile().fovDegree;

	outValid = true;
	return requiredFovDegree;
}

bool DarkCameraActor::ResolveWallLateralCandidateCollision(
	const DirectX::XMFLOAT3& target,
	const DirectX::XMFLOAT3& candidateEye,
	DirectX::XMFLOAT3& outResolvedEye,
	bool& outCollisionHit) const
{
	HitResultWithActor hit{};
	const uint32_t mask =
		CollisionHelper::ToBit(CollisionLayer::WorldStatic) |
		CollisionHelper::ToBit(CollisionLayer::Floor) |
		CollisionHelper::ToBit(CollisionLayer::WorldProps);

	outResolvedEye = candidateEye;
	outCollisionHit = CollisionFunction::SphereRayCast(
		target, candidateEye, hit, sphereCastRadius, mask);
	if (!outCollisionHit)
		return true;

	if (hit.initialOverlap || !hit.hasPosition || !hit.hasNormal)
		return false;

	const float collisionOffset = sphereCastRadius + 0.05f;
	outResolvedEye = MathHelper::Add(hit.hitPoint,
		MathHelper::Multiply(hit.normal, collisionOffset));
	return true;
}

void DarkCameraActor::UpdateLockOnWallLateralEscape(float deltaTime)
{
	struct Candidate
	{
		const char* name = "None";
		int side = 0;
		float distance = 0.0f;
		float requiredFov = 45.0f;
		DirectX::XMFLOAT3 resolvedEye{};
		LockOnFovDiagnostics diagnostics{};
		bool collisionHit = false;
		bool valid = false;
	};

	lockOnWallEscapeSevereComposition = false;
	const float baseFovDegree = GetActiveLockOnProfile().fovDegree;
	lockOnWallEscapeCurrentRequiredFov = baseFovDegree;
	targetLateralEscapeOffset = 0.0f;

	if (isExternalBlending || isBlending || currentMode != CameraMode::LockOn)
	{
		currentEscapeSide = 0;
		currentLateralEscapeOffset = 0.0f;
		lockOnWallEscapeActive = false;
		return;
	}

	const DirectX::XMFLOAT3 baseEye = collisionPostEyePosition;
	const DirectX::XMFLOAT3 forward = MathHelper::Subtract(
		currentPose.target, baseEye);
	if (MathHelper::Length(forward) <= 0.1f)
		return;

	const DirectX::XMFLOAT3 worldUp{ 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 cameraRight = MathHelper::Cross(
		worldUp, MathHelper::Normalize(forward));
	if (MathHelper::Length(cameraRight) <= FLT_EPSILON)
		return;
	cameraRight = MathHelper::Normalize(cameraRight);

	LockOnFovDiagnostics currentDiagnostics{};
	bool currentValid = false;
	lockOnWallEscapeCurrentRequiredFov = EvaluateRequiredLockOnFovFromEye(
		baseEye, currentDiagnostics, currentValid);
	lockOnWallEscapeSevereComposition = cameraHitWall &&
		(!currentValid || !currentDiagnostics.player.inFront ||
			!currentDiagnostics.boss.inFront ||
			lockOnWallEscapeCurrentRequiredFov >=
			lockOnWallEscapeSevereFovThreshold);

	Candidate leftNear{ "Left Near", -1,
		(std::max)(lockOnWallEscapeNearOffset, 0.0f) };
	Candidate leftFar{ "Left Far", -1,
		(std::max)(lockOnWallEscapeFarOffset, 0.0f) };
	Candidate rightNear{ "Right Near", 1,
		(std::max)(lockOnWallEscapeNearOffset, 0.0f) };
	Candidate rightFar{ "Right Far", 1,
		(std::max)(lockOnWallEscapeFarOffset, 0.0f) };

	const auto evaluateCandidate = [&](Candidate& candidate)
		{
			const DirectX::XMFLOAT3 rawEye = MathHelper::Add(baseEye,
				MathHelper::Multiply(cameraRight,
					candidate.distance * static_cast<float>(candidate.side)));
			const bool collisionValid = ResolveWallLateralCandidateCollision(
				currentPose.target, rawEye, candidate.resolvedEye,
				candidate.collisionHit);
			if (!collisionValid)
				return;

			bool compositionValid = false;
			candidate.requiredFov = EvaluateRequiredLockOnFovFromEye(
				candidate.resolvedEye, candidate.diagnostics, compositionValid);
			candidate.valid = compositionValid &&
				candidate.diagnostics.player.inFront &&
				candidate.diagnostics.boss.inFront;
		};

	Candidate* selected = nullptr;
	if (lockOnWallEscapeSevereComposition)
	{
		evaluateCandidate(leftNear);
		evaluateCandidate(leftFar);
		evaluateCandidate(rightNear);
		evaluateCandidate(rightFar);

		const auto chooseDistance = [&](Candidate& nearCandidate,
			Candidate& farCandidate) -> Candidate*
			{
				if (!nearCandidate.valid)
					return farCandidate.valid ? &farCandidate : nullptr;
				if (!farCandidate.valid)
					return &nearCandidate;
				return nearCandidate.requiredFov - farCandidate.requiredFov >=
					lockOnWallEscapeDistanceImprovementThreshold
					? &farCandidate
					: &nearCandidate;
			};

		Candidate* bestLeft = chooseDistance(leftNear, leftFar);
		Candidate* bestRight = chooseDistance(rightNear, rightFar);
		Candidate* currentSideCandidate = currentEscapeSide < 0
			? bestLeft
			: currentEscapeSide > 0 ? bestRight : nullptr;
		Candidate* oppositeCandidate = currentEscapeSide < 0
			? bestRight
			: currentEscapeSide > 0 ? bestLeft : nullptr;

		if (currentSideCandidate)
		{
			selected = currentSideCandidate;
			if (oppositeCandidate &&
				currentSideCandidate->requiredFov -
				oppositeCandidate->requiredFov >=
				lockOnWallEscapeSideSwitchImprovementThreshold)
			{
				selected = oppositeCandidate;
			}
		}
		else if (oppositeCandidate)
		{
			selected = oppositeCandidate;
		}
		else if (bestLeft && bestRight)
		{
			selected = bestLeft->requiredFov <= bestRight->requiredFov
				? bestLeft
				: bestRight;
		}
		else
		{
			selected = bestLeft ? bestLeft : bestRight;
		}

		if (selected)
		{
			const bool clearlyImproved = !currentValid ||
				lockOnWallEscapeCurrentRequiredFov - selected->requiredFov >=
				lockOnWallEscapeImprovementThreshold;
			if (!clearlyImproved)
				selected = nullptr;
		}
	}

	if (selected)
	{
		targetLateralEscapeOffset = selected->distance *
			static_cast<float>(selected->side);
		currentEscapeSide = selected->side;
	}

	const float speed = std::abs(targetLateralEscapeOffset) > FLT_EPSILON
		? lockOnWallEscapeMoveSpeed
		: lockOnWallEscapeReturnSpeed;
	const float maxOffsetDelta = (std::max)(speed, 0.0f) *
		(std::max)(deltaTime, 0.0f);
	currentLateralEscapeOffset += std::clamp(
		targetLateralEscapeOffset - currentLateralEscapeOffset,
		-maxOffsetDelta, maxOffsetDelta);
	if (std::abs(currentLateralEscapeOffset) <= FLT_EPSILON &&
		std::abs(targetLateralEscapeOffset) <= FLT_EPSILON)
	{
		currentLateralEscapeOffset = 0.0f;
		currentEscapeSide = 0;
	}

	lockOnWallEscapeActive = std::abs(currentLateralEscapeOffset) > FLT_EPSILON;
	if (!lockOnWallEscapeActive)
		return;

	const DirectX::XMFLOAT3 smoothedEye = MathHelper::Add(baseEye,
		MathHelper::Multiply(cameraRight, currentLateralEscapeOffset));
	DirectX::XMFLOAT3 resolvedSmoothedEye{};
	bool finalCollisionHit = false;
	if (ResolveWallLateralCandidateCollision(currentPose.target, smoothedEye,
		resolvedSmoothedEye, finalCollisionHit))
	{
		currentPose.eye = resolvedSmoothedEye;
		collisionPostEyePosition = resolvedSmoothedEye;
	}
}

void DarkCameraActor::UpdateCombatPresentation(float deltaTime)
{
	const auto head = playerHead.lock();
	const auto* player = head ? dynamic_cast<const Player*>(head->GetOwner()) : nullptr;
	const bool allowed = player && !isExternalBlending && currentMode != CameraMode::Death && requestMode != CameraMode::Death;
	const bool rush = allowed && player->IsRushActive();
	const bool just = allowed && player->IsJustDodgePresentationActive();
	const float dt = (std::max)(Time::UnscaledDeltaTime(), 0.0f);
	if (!allowed)
	{
		combatPresentationMode = CombatPresentationMode::None;
		combatPresentationPhase = CombatPresentationPhase::Inactive;
		combatPresentationBlend = combatPresentationRushBlend = 0.0f;
		combatPresentationRushRotationBlend = combatPresentationRushFovBlend = 0.0f;
		combatPresentationRushStartPitch = 0.0f;
		combatPresentationRushIntroElapsed = combatPresentationRushTravelBlend = combatPresentationRushAttackBlend = combatPresentationRushWindupPushBlend = 0.0f;
		combatPresentationRushStartedFromJustDodge = false;
		combatPresentationRushTarget.reset();
		combatPresentationJustDodgeSignalLastFrame = false;
		return;
	}
	if (rush)
	{
		if (const auto target = player->GetRushTarget()) combatPresentationRushTarget = target;
		if (combatPresentationMode != CombatPresentationMode::Rush)
		{
			combatPresentationRushStartedFromJustDodge = combatPresentationMode == CombatPresentationMode::JustDodge && combatPresentationBlend > FLT_EPSILON;
			combatPresentationRushBlend = 0.0f;
			combatPresentationRushRotationBlend = 0.0f;
			combatPresentationRushFovBlend = 0.0f;
			combatPresentationRushStartPitch = currentPitch;
			combatPresentationRushIntroElapsed = 0.0f;
			combatPresentationRushTravelBlend = 0.0f;
			combatPresentationRushAttackBlend = 0.0f;
			combatPresentationRushWindupPushBlend = 0.0f;
			combatPresentationRushAttackLastFrame = player->IsRushAttackActive();
			combatPresentationMode = CombatPresentationMode::Rush;
			combatPresentationPhase = CombatPresentationPhase::Enter;
			combatPresentationPhaseElapsed = 0.0f;
		}
	}
	if (combatPresentationMode == CombatPresentationMode::Rush)
	{
		const bool windupActive = rush && player->IsRushWindupActive();
		const float pushTime = windupActive ? rushPresentationWindupPushDuration : rushPresentationWindupPushReleaseDuration;
		const float pushStep = pushTime > FLT_EPSILON ? dt / pushTime : 1.0f;
		combatPresentationRushWindupPushBlend = std::clamp(combatPresentationRushWindupPushBlend +
			(windupActive ? pushStep : -pushStep), 0.0f, 1.0f);
	}
	if (rush && combatPresentationMode == CombatPresentationMode::Rush)
	{
		const bool rushAttack = player->IsRushAttackActive();
		if (rushAttack && !combatPresentationRushAttackLastFrame)
		{
			combatPresentationRushAttackBlend = 0.0f;
			combatPresentationRushTravelBlend = 0.0f;
			combatPresentationRushIntroElapsed = 0.0f;
		}
		combatPresentationRushAttackLastFrame = rushAttack;
	}
	else if (just && !combatPresentationJustDodgeSignalLastFrame && combatPresentationMode != CombatPresentationMode::Rush)
	{
		combatPresentationMode = CombatPresentationMode::JustDodge;
		combatPresentationPhase = CombatPresentationPhase::Enter;
		combatPresentationPhaseElapsed = 0.0f;
	}
	else if (combatPresentationMode == CombatPresentationMode::Rush && combatPresentationPhase != CombatPresentationPhase::Exit)
	{
		const float t = std::clamp(combatPresentationRushBlend, 0.0f, 1.0f);
		const float easedT = t * t * (3.0f - 2.0f * t);
		const float rushTargetPitch = DirectX::XMConvertToRadians(rushPresentationTargetPitchDegree);
		currentPitch = desiredPitch = std::lerp(combatPresentationRushStartPitch, rushTargetPitch, easedT);
		currentPose = CalculatePose(currentMode, head->GetComponentLocation(), currentYaw, currentPitch);
		combatPresentationPhase = CombatPresentationPhase::Exit;
		combatPresentationPhaseElapsed = 0.0f;
	}
	combatPresentationJustDodgeSignalLastFrame = just;
	if (combatPresentationPhase == CombatPresentationPhase::Inactive) return;
	combatPresentationPhaseElapsed += dt;
	const auto blend = [this, dt](const float time, const bool enter)
		{
			combatPresentationBlend = std::clamp(combatPresentationBlend + (enter ? 1.0f : -1.0f) * (time > FLT_EPSILON ? dt / time : 1.0f), 0.0f, 1.0f);
		};
	if (combatPresentationPhase == CombatPresentationPhase::Enter)
	{
		const float time = combatPresentationMode == CombatPresentationMode::Rush ? rushPresentationApproachEnterTime : justDodgePresentationEnterTime;
		blend(time, true);
		if (combatPresentationMode == CombatPresentationMode::Rush)
		{
			combatPresentationRushBlend = std::clamp(combatPresentationRushBlend + (time > FLT_EPSILON ? dt / time : 1.0f), 0.0f, 1.0f);

		}
		if (combatPresentationBlend >= 1.0f && (combatPresentationMode != CombatPresentationMode::Rush || combatPresentationRushBlend >= 1.0f))
		{
			combatPresentationPhase = CombatPresentationPhase::Hold;
			combatPresentationPhaseElapsed = 0.0f;
		}
	}
	else if (combatPresentationPhase == CombatPresentationPhase::Hold)
	{
		if (combatPresentationMode == CombatPresentationMode::Rush)
		{
			if (player->IsRushAttackActive())
			{
				const float attackEnterTime = (std::max)(rushPresentationEnterTime, 0.0f);
				combatPresentationRushAttackBlend = std::clamp(combatPresentationRushAttackBlend +
					(attackEnterTime > FLT_EPSILON ? dt / attackEnterTime : 1.0f), 0.0f, 1.0f);
				combatPresentationRushRotationBlend = combatPresentationRushAttackBlend;
				combatPresentationRushFovBlend = combatPresentationRushAttackBlend;
				if (combatPresentationRushAttackBlend >= 1.0f)
				{
					combatPresentationRushIntroElapsed += dt;
					const float travelTime = (std::max)(rushPresentationIntroToTravelBlendTime, 0.0f);
					const float holdTime = (std::max)(rushPresentationIntroHoldTime, 0.0f);
					combatPresentationRushTravelBlend = std::clamp(
						travelTime > FLT_EPSILON
						? (combatPresentationRushIntroElapsed - holdTime) / travelTime
						: (combatPresentationRushIntroElapsed >= holdTime ? 1.0f : 0.0f),
						0.0f, 1.0f);
				}
			}
		}
		else if (combatPresentationPhaseElapsed >= (std::max)(justDodgePresentationHoldTime, 0.0f))
		{
			combatPresentationPhase = CombatPresentationPhase::Exit;
			combatPresentationPhaseElapsed = 0.0f;
		}
	}
	else if (combatPresentationPhase == CombatPresentationPhase::Exit)
	{
		if (combatPresentationMode == CombatPresentationMode::Rush)
		{
			const auto fadeOut = [dt](float& value, const float time)
				{
					value = std::clamp(value - (time > FLT_EPSILON ? dt / time : 1.0f), 0.0f, 1.0f);
				};
			fadeOut(combatPresentationBlend, rushPresentationPositionExitTime);
			fadeOut(combatPresentationRushRotationBlend, rushPresentationRotationExitTime);
			fadeOut(combatPresentationRushFovBlend, rushPresentationFovExitTime);
		}
		else
		{
			blend(justDodgePresentationExitTime, false);
		}
		if (combatPresentationBlend <= 0.0f &&
			(combatPresentationMode != CombatPresentationMode::Rush ||
				(combatPresentationRushRotationBlend <= 0.0f && combatPresentationRushFovBlend <= 0.0f)))
		{
			combatPresentationMode = CombatPresentationMode::None;
			combatPresentationPhase = CombatPresentationPhase::Inactive;
			combatPresentationRushBlend = 0.0f;
			combatPresentationRushRotationBlend = 0.0f;
			combatPresentationRushFovBlend = 0.0f;
			combatPresentationRushStartPitch = 0.0f;
			combatPresentationRushIntroElapsed = combatPresentationRushTravelBlend = combatPresentationRushAttackBlend = combatPresentationRushWindupPushBlend = 0.0f;
			combatPresentationRushStartedFromJustDodge = false;
			combatPresentationRushTarget.reset();
		}
	}
	if (combatPresentationBlend > FLT_EPSILON && head)
		ApplyCombatPresentationPose(head->GetComponentLocation(), combatPresentationRushTarget.lock());
}

void DarkCameraActor::ApplyCombatPresentationPose(const DirectX::XMFLOAT3& playerLookPosition, const std::shared_ptr<Enemy>& rushTarget)
{
	const CameraPose base = currentPose;
	const auto playerHeadComponent = playerHead.lock();
	const auto* player = playerHeadComponent ? dynamic_cast<const Player*>(playerHeadComponent->GetOwner()) : nullptr;
	const auto smoothStep = [](float value)
		{
			value = std::clamp(value, 0.0f, 1.0f);
			return value * value * (3.0f - 2.0f * value);
		};
	const auto justEye = [&]()
		{
			const auto direction = MathHelper::Normalize(MathHelper::Subtract(base.eye, base.target));
			return MathHelper::Add(base.target, MathHelper::Multiply(direction, (std::max)(0.1f, MathHelper::Distance(base.eye, base.target) + justDodgePresentationDistanceOffset)));
		};
	if (combatPresentationMode == CombatPresentationMode::JustDodge)
	{
		currentPose.eye = MathHelper::Lerp(base.eye, justEye(), smoothStep(combatPresentationBlend));
		return;
	}
	if (combatPresentationMode != CombatPresentationMode::Rush || !rushTarget) return;

	// Prefer the rush target's actual look component when it is available.
	DirectX::XMFLOAT3 enemyLookPosition = rushTarget->GetPosition();
	if (const auto targetHead = enemyHead.lock(); targetHead)
	{
		const auto targetOwner = targetHead->GetOwner();
		if (targetOwner && targetOwner == rushTarget.get())
			enemyLookPosition = targetHead->GetComponentLocation();
	}
	enemyLookPosition.y += lockOnEnemyLookHeight;

	DirectX::XMFLOAT3 travelDirection = MathHelper::Subtract(rushTarget->GetPosition(), playerLookPosition);
	travelDirection.y = 0.0f;
	if (MathHelper::Length(travelDirection) <= FLT_EPSILON) { travelDirection = GetForward(); travelDirection.y = 0.0f; }
	travelDirection = MathHelper::Normalize(travelDirection);

	// Windup and Dash use a target-facing shot from directly behind the Player.
	const float approachYaw = atan2f(travelDirection.x, travelDirection.z);
	const float approachPitch = base.pitch;
	const DirectX::XMFLOAT3 approachForward{ sinf(approachYaw) * cosf(approachPitch), sinf(approachPitch), cosf(approachYaw) * cosf(approachPitch) };
	const DirectX::XMFLOAT3 approachRight{ cosf(approachYaw), 0.0f, -sinf(approachYaw) };
	const DirectX::XMFLOAT3 approachTarget = MathHelper::Lerp(playerLookPosition, enemyLookPosition, rushPresentationIntroEnemyTargetWeight);
	DirectX::XMFLOAT3 approachEye = MathHelper::Subtract(approachTarget, MathHelper::Multiply(approachForward,
		(std::max)(0.1f, MathHelper::Distance(base.eye, base.target) + rushPresentationApproachDistanceOffset - rushPresentationWindupPushIn * smoothStep(combatPresentationRushWindupPushBlend))));
	approachEye = MathHelper::Add(approachEye, MathHelper::Multiply(approachRight, rushPresentationApproachSideOffset));
	approachEye.y += rushPresentationApproachHeightOffset;
	const float approachBlend = smoothStep(combatPresentationRushBlend);
	if (!player || !player->IsRushAttackActive())
	{
		const DirectX::XMFLOAT3 startEye = combatPresentationRushStartedFromJustDodge ? justEye() : base.eye;
		currentPose.target = MathHelper::Lerp(base.target, approachTarget, approachBlend);
		currentPose.eye = MathHelper::Lerp(startEye, approachEye, approachBlend);
		return;
	}

	const float travelBlend = smoothStep(combatPresentationRushTravelBlend);
	const float yawOffset = std::lerp(rushPresentationIntroYawOffsetDegree, rushPresentationTravelYawOffsetDegree, travelBlend);
	const float sideOffset = std::lerp(rushPresentationIntroSideOffset, rushPresentationTravelSideOffset, travelBlend);
	const float heightOffset = std::lerp(rushPresentationIntroHeightOffset, rushPresentationTravelHeightOffset, travelBlend);
	const float distanceOffset = std::lerp(rushPresentationIntroDistanceOffset, rushPresentationTravelDistanceOffset, travelBlend);
	const float targetWeight = std::clamp(std::lerp(rushPresentationIntroEnemyTargetWeight, rushPresentationTravelEnemyTargetWeight, travelBlend), 0.0f, 1.0f);

	const float rotationBlend = smoothStep(combatPresentationRushRotationBlend);
	const float yaw = atan2f(travelDirection.x, travelDirection.z) +
		DirectX::XMConvertToRadians(yawOffset * rotationBlend);
	const float introEnterBlend = smoothStep(combatPresentationRushBlend);
	const float presentationBlend = smoothStep(combatPresentationBlend);
	const float rushTargetPitch = DirectX::XMConvertToRadians(rushPresentationTargetPitchDegree);
	const float rushPitch = std::lerp(combatPresentationRushStartPitch, rushTargetPitch, introEnterBlend);
	// Blend back to the normal TPS/LockOn pitch during Rush exit.
	const float appliedPitch = std::lerp(base.pitch, rushPitch, presentationBlend);
	const DirectX::XMFLOAT3 forward{ sinf(yaw) * cosf(rushPitch), sinf(rushPitch), cosf(yaw) * cosf(rushPitch) };
	const DirectX::XMFLOAT3 right{ cosf(yaw), 0.0f, -sinf(yaw) };
	const DirectX::XMFLOAT3 target = MathHelper::Lerp(playerLookPosition, enemyLookPosition, targetWeight);
	DirectX::XMFLOAT3 eye = MathHelper::Subtract(target, MathHelper::Multiply(forward, (std::max)(0.1f, MathHelper::Distance(base.eye, base.target) + distanceOffset)));
	eye = MathHelper::Add(eye, MathHelper::Multiply(right, sideOffset));
	eye.y += heightOffset;

	// SmoothStep prevents a linear yaw/pitch sweep both into Intro and from Intro to Travel.
	const DirectX::XMFLOAT3 startEye = combatPresentationRushStartedFromJustDodge ? justEye() : base.eye;
	const DirectX::XMFLOAT3 transitionedTarget = MathHelper::Lerp(base.target, target, introEnterBlend);
	const DirectX::XMFLOAT3 transitionedEye = MathHelper::Lerp(startEye, eye, introEnterBlend);
	const float attackBlend = smoothStep(combatPresentationRushAttackBlend);
	const DirectX::XMFLOAT3 specialTarget = MathHelper::Lerp(base.target, transitionedTarget, presentationBlend);
	const DirectX::XMFLOAT3 specialEye = MathHelper::Lerp(base.eye, transitionedEye, presentationBlend);
	currentPose.target = MathHelper::Lerp(approachTarget, specialTarget, attackBlend);
	currentPose.eye = MathHelper::Lerp(approachEye, specialEye, attackBlend);
	currentPose.pitch = std::lerp(approachPitch, appliedPitch, attackBlend);
}

void DarkCameraActor::ApplyCombatPresentationFov()
{
	const auto head = playerHead.lock();
	const auto* player = head ? dynamic_cast<const Player*>(head->GetOwner()) : nullptr;
	const bool rushFovStillBlending = combatPresentationMode == CombatPresentationMode::Rush && combatPresentationRushFovBlend > FLT_EPSILON;
	if ((combatPresentationBlend <= FLT_EPSILON && !rushFovStillBlending) || isExternalBlending || currentMode == CameraMode::Death || requestMode == CameraMode::Death) return;
	const auto smoothStep = [](float value)
		{
			value = std::clamp(value, 0.0f, 1.0f);
			return value * value * (3.0f - 2.0f * value);
		};
	const float base = DirectX::XMConvertToDegrees(mainCameraComponent->GetFov());
	float fov = base;
	if (combatPresentationMode == CombatPresentationMode::JustDodge)
		fov = base + justDodgePresentationFovOffsetDegree * smoothStep(combatPresentationBlend);
	else if (combatPresentationMode == CombatPresentationMode::Rush)
	{
		if (player && player->IsRushAttackActive())
		{
			const float rushFov = std::lerp(rushPresentationIntroFovDegree, rushPresentationTravelFovDegree, smoothStep(combatPresentationRushTravelBlend));
			fov = std::lerp(base, rushFov, smoothStep(combatPresentationRushAttackBlend));
		}
		else if (combatPresentationPhase == CombatPresentationPhase::Exit &&
			combatPresentationRushFovBlend > FLT_EPSILON)
		{
			const float rushFov = std::lerp(rushPresentationIntroFovDegree, rushPresentationTravelFovDegree, smoothStep(combatPresentationRushTravelBlend));
			fov = std::lerp(base, rushFov, smoothStep(combatPresentationRushFovBlend));
		}
	}
	if (currentMode == CameraMode::LockOn && lockOnFovFallbackActive) fov = (std::max)(fov, base);
	mainCameraComponent->SetFov(DirectX::XMConvertToRadians(std::clamp(fov, 10.0f, 120.0f)));
}

void DarkCameraActor::UpdateLockOnFovFallback(float deltaTime)
{
	const LockOnProfile activeProfile = GetActiveLockOnProfile();
	const float baseFovDegree = activeProfile.fovDegree;
	const float maxFallbackFovDegree = (std::max)(
		baseFovDegree, activeProfile.maxFallbackFovDegree);
	LockOnFovDiagnostics requiredFovDiagnostics{};
	bool requiredFovValid = false;
	lockOnRequiredFovDegree = EvaluateRequiredLockOnFovFromEye(
		collisionPostEyePosition, compositionLookTarget,
		requiredFovDiagnostics, requiredFovValid);

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

void DarkCameraActor::UpdateLockOnCompositionLookCorrection(
	const float deltaTime)
{
	const LockOnProfile activeProfile = GetActiveLockOnProfile();
	compositionPlayerInsideSafeFrame = false;
	compositionBossInsideSafeFrame = false;
	compositionPlayerInFront = false;
	compositionBossInFront = false;
	targetCompositionYawCorrection = 0.0f;
	compositionRequiredFovAfter = activeProfile.fovDegree;

	const auto moveCorrection = [&]()
		{
			const float maxDelta = DirectX::XMConvertToRadians(
				(std::max)(compositionAngularSpeedDegree, 0.0f)) *
				(std::max)(deltaTime, 0.0f);
			currentCompositionYawCorrection += std::clamp(
				targetCompositionYawCorrection - currentCompositionYawCorrection,
				-maxDelta, maxDelta);
			if (std::abs(currentCompositionYawCorrection) <
				DirectX::XMConvertToRadians(0.01f))
			{
				currentCompositionYawCorrection = 0.0f;
			}
		};

	bool beforeValid = false;
	LockOnFovDiagnostics beforeDiagnostics{};
	EvaluateRequiredLockOnFovFromEye(
		currentPose.eye, currentPose.target, beforeDiagnostics, beforeValid);
	if (beforeValid)
	{
		const float currentFovDegree = DirectX::XMConvertToDegrees(
			mainCameraComponent->GetFov());
		compositionPlayerInFront = beforeDiagnostics.player.inFront;
		compositionBossInFront = beforeDiagnostics.boss.inFront;
		compositionPlayerInsideSafeFrame = compositionPlayerInFront &&
			beforeDiagnostics.player.requiredFovFromX <= currentFovDegree &&
			beforeDiagnostics.player.requiredFovFromY <= currentFovDegree;
		compositionBossInsideSafeFrame = compositionBossInFront &&
			beforeDiagnostics.boss.requiredFovFromX <= currentFovDegree &&
			beforeDiagnostics.boss.requiredFovFromY <= currentFovDegree;
	}

	const bool eligible = currentMode == CameraMode::LockOn &&
		!isBlending && !isExternalBlending && cameraHitWall &&
		lockOnWallEscapeActive && beforeValid &&
		compositionPlayerInFront && compositionBossInFront &&
		(!compositionPlayerInsideSafeFrame ||
			!compositionBossInsideSafeFrame);
	if (eligible)
	{
		const auto player = playerHead.lock();
		const auto boss = enemyHead.lock();
		if (player && boss)
		{
			DirectX::XMFLOAT3 playerLookPosition =
				player->GetComponentLocation();
			DirectX::XMFLOAT3 bossLookPosition =
				boss->GetComponentLocation();
			playerLookPosition.y += lockOnPlayerLookHeight;
			bossLookPosition.y += lockOnEnemyLookHeight;

			DirectX::XMFLOAT3 toPlayer = MathHelper::Subtract(
				playerLookPosition, currentPose.eye);
			DirectX::XMFLOAT3 toBoss = MathHelper::Subtract(
				bossLookPosition, currentPose.eye);
			toPlayer.y = 0.0f;
			toBoss.y = 0.0f;
			const float playerLength = MathHelper::Length(toPlayer);
			const float bossLength = MathHelper::Length(toBoss);
			if (playerLength > FLT_EPSILON && bossLength > FLT_EPSILON)
			{
				toPlayer = MathHelper::Multiply(toPlayer, 1.0f / playerLength);
				toBoss = MathHelper::Multiply(toBoss, 1.0f / bossLength);
				const DirectX::XMFLOAT3 compositionDirection =
					MathHelper::Add(toPlayer, toBoss);
				if (MathHelper::Length(compositionDirection) > FLT_EPSILON)
				{
					const float compositionTargetYaw = atan2f(
						compositionDirection.x, compositionDirection.z);
					const DirectX::XMFLOAT3 baseDirection =
						MathHelper::Subtract(currentPose.target, currentPose.eye);
					const float compositionBaseYaw = atan2f(baseDirection.x, baseDirection.z);
					targetCompositionYawCorrection = MathHelper::ClampAngle(
						compositionTargetYaw - compositionBaseYaw);
					const float maxCorrection = DirectX::XMConvertToRadians(
						(std::max)(compositionMaxCorrectionDegree, 0.0f));
					targetCompositionYawCorrection = std::clamp(
						targetCompositionYawCorrection,
						-maxCorrection, maxCorrection);
					if (std::abs(targetCompositionYawCorrection) <
						DirectX::XMConvertToRadians(
							(std::max)(compositionDeadZoneDegree, 0.0f)))
					{
						targetCompositionYawCorrection = 0.0f;
					}
				}
			}
		}
	}

	moveCorrection();
	const DirectX::XMFLOAT3 lookVector = MathHelper::Subtract(
		currentPose.target, currentPose.eye);
	const float cosine = std::cos(currentCompositionYawCorrection);
	const float sine = std::sin(currentCompositionYawCorrection);
	const DirectX::XMFLOAT3 rotatedLookVector{
		cosine * lookVector.x + sine * lookVector.z,
		lookVector.y,
		-sine * lookVector.x + cosine * lookVector.z
	};
	compositionLookTarget = MathHelper::Add(
		currentPose.eye, rotatedLookVector);

	LockOnFovDiagnostics afterDiagnostics{};
	bool afterValid = false;
	compositionRequiredFovAfter = EvaluateRequiredLockOnFovFromEye(
		currentPose.eye, compositionLookTarget, afterDiagnostics, afterValid);

	recoveryCandidateValid = false;
	recoveryBaseRequiredFov = compositionRequiredFovAfter;
	recoveryFinalRequiredFov = compositionRequiredFovAfter;

	const bool recoveryEligible = currentMode == CameraMode::LockOn &&
		!isBlending && !isExternalBlending && cameraHitWall &&
		lockOnWallEscapeActive && beforeValid && compositionPlayerInFront &&
		compositionBossInFront && !compositionPlayerInsideSafeFrame &&
		compositionRequiredFovAfter > activeProfile.maxFallbackFovDegree;

	const auto calculateCompositionTarget =
		[&](const DirectX::XMFLOAT3& eye,
			DirectX::XMFLOAT3& outTarget,
			LockOnFovDiagnostics& outDiagnostics,
			float& outRequiredFov) -> bool
		{
			const auto player = playerHead.lock();
			const auto boss = enemyHead.lock();
			if (!player || !boss)
				return false;

			DirectX::XMFLOAT3 playerLook = player->GetComponentLocation();
			DirectX::XMFLOAT3 bossLook = boss->GetComponentLocation();
			playerLook.y += lockOnPlayerLookHeight;
			bossLook.y += lockOnEnemyLookHeight;
			DirectX::XMFLOAT3 toPlayer = MathHelper::Subtract(playerLook, eye);
			DirectX::XMFLOAT3 toBoss = MathHelper::Subtract(bossLook, eye);
			toPlayer.y = 0.0f;
			toBoss.y = 0.0f;
			const float playerLength = MathHelper::Length(toPlayer);
			const float bossLength = MathHelper::Length(toBoss);
			if (playerLength <= FLT_EPSILON || bossLength <= FLT_EPSILON)
				return false;
			toPlayer = MathHelper::Multiply(toPlayer, 1.0f / playerLength);
			toBoss = MathHelper::Multiply(toBoss, 1.0f / bossLength);
			const DirectX::XMFLOAT3 direction = MathHelper::Add(toPlayer, toBoss);
			if (MathHelper::Length(direction) <= FLT_EPSILON)
				return false;

			const float targetYaw = atan2f(direction.x, direction.z);
			const DirectX::XMFLOAT3 baseDirection = MathHelper::Subtract(currentPose.target, eye);
			const float baseYaw = atan2f(baseDirection.x, baseDirection.z);
			float correction = MathHelper::ClampAngle(targetYaw - baseYaw);
			const float maxCorrection = DirectX::XMConvertToRadians((std::max)(compositionMaxCorrectionDegree, 0.0f));
			correction = std::clamp(correction, -maxCorrection, maxCorrection);
			const float c = std::cos(correction);
			const float s = std::sin(correction);
			const DirectX::XMFLOAT3 lookVector = MathHelper::Subtract(currentPose.target, eye);
			const DirectX::XMFLOAT3 rotatedLookVector =
			{
				c * lookVector.x + s * lookVector.z,
				lookVector.y,
				-s * lookVector.x + c * lookVector.z
			};
			outTarget = MathHelper::Add(eye, rotatedLookVector);
			bool valid = false;
			outRequiredFov = EvaluateRequiredLockOnFovFromEye(eye, outTarget, outDiagnostics, valid);
			return valid && outDiagnostics.player.inFront && outDiagnostics.boss.inFront;
		};

	if (recoveryEligible)
	{
		const DirectX::XMFLOAT3 baseEye = currentPose.eye;
		const DirectX::XMFLOAT3 toDesired = MathHelper::Subtract(collisionPreEyePosition, baseEye);
		const float distanceToDesired = MathHelper::Length(toDesired);
		const float maxRecovery = (std::max)(recoveryMaxDistance, 0.0f);
		const float candidateDistance = (std::min)(distanceToDesired, maxRecovery);
		if (candidateDistance > FLT_EPSILON && distanceToDesired > FLT_EPSILON)
		{
			const DirectX::XMFLOAT3 candidateEye = MathHelper::Add(baseEye,MathHelper::Multiply(toDesired, candidateDistance / distanceToDesired));
			DirectX::XMFLOAT3 resolvedCandidateEye{};
			bool candidateHit = false;
			const bool collisionValid = ResolveWallLateralCandidateCollision(currentPose.target, candidateEye, resolvedCandidateEye, candidateHit);
			DirectX::XMFLOAT3 candidateLookTarget{};
			LockOnFovDiagnostics candidateDiagnostics{};
			float candidateFov = compositionRequiredFovAfter;
			recoveryCandidateValid = collisionValid &&calculateCompositionTarget(resolvedCandidateEye,candidateLookTarget, candidateDiagnostics, candidateFov) &&recoveryBaseRequiredFov - candidateFov >=recoveryFovImprovementThreshold;
			if (recoveryCandidateValid)
				targetRecoveryDistance = candidateDistance;
			else
				targetRecoveryDistance = 0.0f;
		}
		else
		{
			targetRecoveryDistance = 0.0f;
		}
	}
	else
	{
		targetRecoveryDistance = 0.0f;
	}

	const float recoverySpeed = targetRecoveryDistance > currentRecoveryDistance ? recoveryMoveSpeed : recoveryReturnSpeed;
	const float recoveryStep = (std::max)(recoverySpeed, 0.0f) * (std::max)(deltaTime, 0.0f);
	currentRecoveryDistance += std::clamp(targetRecoveryDistance - currentRecoveryDistance, -recoveryStep, recoveryStep);

	if (currentRecoveryDistance > FLT_EPSILON)
	{
		const DirectX::XMFLOAT3 baseEye = currentPose.eye;
		const DirectX::XMFLOAT3 toDesired = MathHelper::Subtract(collisionPreEyePosition, baseEye);
		const float distanceToDesired = MathHelper::Length(toDesired);
		if (distanceToDesired > FLT_EPSILON)
		{
			const float distance = (std::min)(currentRecoveryDistance, distanceToDesired);
			const DirectX::XMFLOAT3 recoveryEye = MathHelper::Add(baseEye, MathHelper::Multiply(toDesired, distance / distanceToDesired));
			DirectX::XMFLOAT3 resolvedEye{};
			bool collisionHit = false;
			if (ResolveWallLateralCandidateCollision(currentPose.target, recoveryEye, resolvedEye, collisionHit))
			{
				currentPose.eye = resolvedEye;
				collisionPostEyePosition = resolvedEye;
				DirectX::XMFLOAT3 finalLookTarget{};
				LockOnFovDiagnostics finalDiagnostics{};
				if (calculateCompositionTarget(resolvedEye, finalLookTarget, finalDiagnostics, recoveryFinalRequiredFov))
				{
					compositionLookTarget = finalLookTarget;
					compositionRequiredFovAfter = recoveryFinalRequiredFov;
				}
			}
		}
	}
}

void DarkCameraActor::ResetLockOnAdaptiveState()
{
	const LockOnProfile activeProfile = GetActiveLockOnProfile();
	lockOnCollisionStrength = 0.0f;
	lockOnCollisionRatioForAdaptive = 1.0f;
	lockOnDistanceStrength = 0.0f;
	lockOnDistanceForZoom = 0.0f;
	lockOnEnemyDistance = 0.0f;
	desiredLockOnCameraDistance = activeProfile.distance;
	currentLockOnCameraDistance = activeProfile.distance;
	adaptiveLockOnCameraDistance = activeProfile.distance;
	adaptiveLockOnTargetWeight = lockOnTargetWeight;
	adaptiveLockOnHorizontalOffset = lockOnSettings.horizontalOffset;
	lockOnRequiredXDistance = 0.0f;
	lockOnRequiredYDistance = 0.0f;
	lockOnRequiredFramingDistance = activeProfile.distance;
	lockOnExistingAdaptiveDistance = activeProfile.distance;
	lockOnFramingDeficit = 0.0f;
	lockOnRequiredFovDegree = activeProfile.fovDegree;
	lockOnTargetFovDegree = activeProfile.fovDegree;
	lockOnCurrentFovDegree = activeProfile.fovDegree;
	lockOnFovReturnDelayElapsed = 0.0f;
	lockOnFovFallbackActive = false;
	currentEscapeSide = 0;
	currentLateralEscapeOffset = 0.0f;
	targetLateralEscapeOffset = 0.0f;
	lockOnWallEscapeActive = false;
	currentCompositionYawCorrection = 0.0f;
	targetCompositionYawCorrection = 0.0f;
	currentRecoveryDistance = 0.0f;
	targetRecoveryDistance = 0.0f;
	compositionLookTarget = currentPose.target;
}

float DarkCameraActor::ClampEnemyLockOnDistanceToProfile(float distance, const LockOnProfile& profile) const
{
	if (IsGruxLockOnTarget())
		return distance;

	const float minimum = (std::max)(profile.distance, 0.1f);
	const float maximum = minimum + (std::max)(profile.maxDistanceAdd, 0.0f);
	return std::clamp(distance, minimum, maximum);
}

void DarkCameraActor::InitializeLockOnTransitionDistanceSnapshot()
{
	const LockOnProfile activeProfile = GetActiveLockOnProfile();

	if (IsGruxLockOnTarget())
		return;

	const auto playerHeadShared = playerHead.lock();
	const auto enemyHeadShared = enemyHead.lock();
	if (!playerHeadShared || !enemyHeadShared)
		return;

	lockOnEnemyDistance = MathHelper::Distance(playerHeadShared->GetComponentLocation(), enemyHeadShared->GetComponentLocation()); lockOnDistanceForZoom = lockOnEnemyDistance;
	const float enemyRange = (std::max)(lockOnDistanceFull - lockOnDistanceStart, FLT_EPSILON);
	const float enemyDistanceInput = (lockOnDistanceForZoom - lockOnDistanceStart) / enemyRange;
	lockOnDistanceStrength = MathHelper::SmoothStep01(enemyDistanceInput);
	lockOnExistingAdaptiveDistance = activeProfile.distance + activeProfile.maxDistanceAdd * lockOnDistanceStrength;
	lockOnRequiredFramingDistance = CalculateRequiredLockOnFramingDistance();

	const float lockOnTransitionSnapshotDistance = ClampEnemyLockOnDistanceToProfile((std::max)(lockOnExistingAdaptiveDistance, lockOnRequiredFramingDistance), activeProfile);
	currentLockOnCameraDistance = lockOnTransitionSnapshotDistance;
	desiredLockOnCameraDistance = lockOnTransitionSnapshotDistance;
	adaptiveLockOnCameraDistance = lockOnTransitionSnapshotDistance;
}

DarkCameraActor::CameraPose DarkCameraActor::CalculatePose(CameraMode cameraMode, const DirectX::XMFLOAT3& playerPos, float yaw, float pitch) const
{
	CameraPose pose{};

	if (cameraMode == CameraMode::Death)
	{
		return CalculateDeathPoseWithSideOffset(playerPos, yaw, deathBlendSideOffset);
	}

	const CameraCompositionSettings* settings = &GetActiveTpsSettings();
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

			pose.target = MathHelper::Lerp(playerLookPosition,enemyLookPosition,adaptiveLockOnTargetWeight);
		}
		break;
	}
	}

	// Eye の処理は共通
	using namespace DirectX;

	XMVECTOR forward = XMVectorSet(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch), 0.0f);
	XMVECTOR target = XMLoadFloat3(&pose.target);
	XMVECTOR eye = target - forward * distance;
	XMVECTOR right = XMVectorSet(cosf(yaw), 0.0f, -sinf(yaw), 0.0f);
	eye += right * horizontalOffset;
	eye += XMVectorSet(0, settings->height, 0, 0);

	XMStoreFloat3(&pose.eye, eye);
	pose.yaw = yaw;
	pose.pitch = pitch;
	return pose;
}

DarkCameraActor::CameraPose DarkCameraActor::CalculateDeathPoseWithSideOffset(const DirectX::XMFLOAT3& playerPos, float yaw, float sideOffset) const
{
	CameraPose pose{};
	DirectX::XMFLOAT3 toBoss{};
	if (const auto boss = deathBoss.lock(); boss && !boss->IsPendingKill())
	{
		if (const auto target = boss->GetCameraTargetComponent())
		{
			toBoss = MathHelper::Subtract(target->GetComponentLocation(), playerPos);
			toBoss.y = 0.0f;
		}
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
			MathHelper::Multiply(right, sideOffset)),
		MathHelper::Multiply(worldUp, deathCameraSettings.cameraHeight));

	const DirectX::XMFLOAT3 viewDirection = MathHelper::Normalize(
		MathHelper::Subtract(pose.target, pose.eye));
	pose.yaw = atan2f(viewDirection.x, viewDirection.z);
	pose.pitch = asinf(std::clamp(viewDirection.y, -1.0f, 1.0f));
	return pose;
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
		if (hit.initialOverlap || !hit.hasPosition || !hit.hasNormal)
			return eye;

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
	if (ImGui::CollapsingHeader(U8("実行時カメラ調整"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat(U8("通常TPS FOV"), &standardTpsSettings.fovDegree, 0.1f, 10.0f, 120.0f);
		ImGui::DragFloat(U8("通常TPS 距離"), &standardTpsSettings.distance, 0.05f, 0.1f, 30.0f);
		ImGui::DragFloat(U8("通常敵 ロックオン画面内判定半径"), &normalEnemyLockOnScreenRadius, 0.01f, 0.0f, 1.5f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat(U8("通常敵 ロックオン最大選択距離"), &lockOnTargetSelectionMaxDistance, 0.1f, 3.0f, 20.0f, "%.1f m", ImGuiSliderFlags_AlwaysClamp);

		ImGui::Separator();

		ImGui::DragFloat(U8("Grux ロックオンFOV"), &lockOnSettings.fovDegree, 0.1f, 10.0f, 120.0f);
		ImGui::DragFloat(U8("Grux ロックオン基本距離"), &lockOnSettings.distance, 0.05f, 0.1f, 30.0f);

		ImGui::SeparatorText(U8("小型敵 ロックオン設定"));
		ImGui::DragFloat(U8("小型敵 ロックオンFOV"), &compactEnemyLockOnProfile.fovDegree, 0.1f, 10.0f, 120.0f);
		ImGui::DragFloat(U8("小型敵 FOV補正の最大値"), &compactEnemyLockOnProfile.maxFallbackFovDegree, 0.1f, compactEnemyLockOnProfile.fovDegree, 90.0f);
		ImGui::DragFloat(U8("小型敵 ロックオン基本距離"), &compactEnemyLockOnProfile.distance, 0.05f, 0.1f, 30.0f);
		ImGui::DragFloat(U8("小型敵 ロックオン最大追加距離"), &compactEnemyLockOnProfile.maxDistanceAdd, 0.05f, 0.0f, 10.0f);

		ImGui::SeparatorText(U8("通常敵 ロックオン設定"));
		ImGui::DragFloat(U8("通常敵 ロックオンFOV"), &standardEnemyLockOnProfile.fovDegree, 0.1f, 10.0f, 120.0f);
		ImGui::DragFloat(U8("通常敵 FOV補正の最大値"), &standardEnemyLockOnProfile.maxFallbackFovDegree, 0.1f, standardEnemyLockOnProfile.fovDegree, 90.0f);
		ImGui::DragFloat(U8("通常敵 ロックオン基本距離"), &standardEnemyLockOnProfile.distance, 0.05f, 0.1f, 30.0f);
		ImGui::DragFloat(U8("通常敵 ロックオン最大追加距離"), &standardEnemyLockOnProfile.maxDistanceAdd, 0.05f, 0.0f, 10.0f);

		ImGui::DragFloat(U8("ロックオン敵注視高さ"), &lockOnEnemyLookHeight, 0.05f, -10.0f, 10.0f);
		ImGui::SliderFloat(U8("ロックオン注視比率（0=プレイヤー、1=ボス）"), &lockOnTargetWeight, 0.0f, 1.0f);
		ImGui::DragFloat(U8("ロックオン ズームイン速度"), &lockOnZoomInSpeed, 0.01f, 0.0f, 30.0f);
		ImGui::DragFloat(U8("ロックオン ズームアウト速度"), &lockOnZoomOutSpeed, 0.01f, 0.0f, 30.0f);

		ImGui::SeparatorText(U8("戦闘カメラ演出"));

		if (ImGui::TreeNode(U8("ジャスト回避カメラ演出")))
		{
			ImGui::DragFloat(U8("FOVオフセット"), &justDodgePresentationFovOffsetDegree, 0.1f, -45.0f, 45.0f, "%.1f deg");
			ImGui::DragFloat(U8("距離オフセット"), &justDodgePresentationDistanceOffset, 0.01f, -5.0f, 5.0f);
			ImGui::DragFloat(U8("演出開始時間"), &justDodgePresentationEnterTime, 0.01f, 0.0f, 2.0f, "%.2f sec");
			ImGui::DragFloat(U8("演出維持時間"), &justDodgePresentationHoldTime, 0.01f, 0.0f, 5.0f, "%.2f sec");
			ImGui::DragFloat(U8("演出終了時間"), &justDodgePresentationExitTime, 0.01f, 0.0f, 2.0f, "%.2f sec");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("Rushカメラ演出")))
		{
			if (ImGui::TreeNode(U8("背後カメラ（予備動作・ダッシュ）")))
			{
				ImGui::DragFloat(U8("横方向オフセット##RushBehind"), &rushPresentationApproachSideOffset, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat(U8("高さオフセット##RushBehind"), &rushPresentationApproachHeightOffset, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat(U8("距離オフセット##RushBehind"), &rushPresentationApproachDistanceOffset, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat(U8("背後カメラへの切替時間"), &rushPresentationApproachEnterTime, 0.01f, 0.0f, 0.5f, "%.2f sec", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat(U8("予備動作中のカメラ押し込み距離"), &rushPresentationWindupPushIn, 0.01f, 0.0f, 1.0f, "%.2f m", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat(U8("カメラ押し込み時間"), &rushPresentationWindupPushDuration, 0.01f, 0.0f, 0.4f, "%.2f sec", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat(U8("カメラ押し込み解除時間"), &rushPresentationWindupPushReleaseDuration, 0.01f, 0.0f, 0.4f, "%.2f sec", ImGuiSliderFlags_AlwaysClamp);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode(U8("Rush開始演出")))
			{
				ImGui::DragFloat(U8("Yawオフセット##RushIntro"), &rushPresentationIntroYawOffsetDegree, 0.5f, -180.0f, 180.0f, "%.1f deg");
				ImGui::DragFloat(U8("横方向オフセット##RushIntro"), &rushPresentationIntroSideOffset, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat(U8("高さオフセット##RushIntro"), &rushPresentationIntroHeightOffset, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat(U8("距離オフセット##RushIntro"), &rushPresentationIntroDistanceOffset, 0.01f, -10.0f, 10.0f);
				ImGui::SliderFloat(U8("敵への注視比率##RushIntro"), &rushPresentationIntroEnemyTargetWeight, 0.0f, 1.0f);
				ImGui::DragFloat(U8("FOV##RushIntro"), &rushPresentationIntroFovDegree, 0.1f, 10.0f, 120.0f, "%.1f deg");
				ImGui::DragFloat(U8("Rush開始演出への切替時間"), &rushPresentationEnterTime, 0.01f, 0.0f, 0.5f, "%.2f sec", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat(U8("Rush開始演出の維持時間"), &rushPresentationIntroHoldTime, 0.01f, 0.0f, 0.3f, "%.2f sec", ImGuiSliderFlags_AlwaysClamp);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode(U8("Rush移動中の演出")))
			{
				ImGui::DragFloat(U8("Yawオフセット##RushTravel"), &rushPresentationTravelYawOffsetDegree, 0.5f, -180.0f, 180.0f, "%.1f deg");
				ImGui::DragFloat(U8("横方向オフセット##RushTravel"), &rushPresentationTravelSideOffset, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat(U8("高さオフセット##RushTravel"), &rushPresentationTravelHeightOffset, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat(U8("距離オフセット##RushTravel"), &rushPresentationTravelDistanceOffset, 0.01f, -10.0f, 10.0f);
				ImGui::SliderFloat(U8("敵への注視比率##RushTravel"), &rushPresentationTravelEnemyTargetWeight, 0.0f, 1.0f);
				ImGui::DragFloat(U8("FOV##RushTravel"), &rushPresentationTravelFovDegree, 0.1f, 10.0f, 120.0f, "%.1f deg");
				ImGui::TreePop();
			}

			ImGui::DragFloat(U8("開始演出から移動演出への切替時間"), &rushPresentationIntroToTravelBlendTime, 0.01f, 0.0f, 2.0f, "%.2f sec");
			ImGui::DragFloat(U8("Rush注視対象のピッチ角度"), &rushPresentationTargetPitchDegree, 0.5f, -89.0f, 89.0f, "%.1f deg");
			ImGui::DragFloat(U8("回転の復帰時間##Rush"), &rushPresentationRotationExitTime, 0.01f, 0.0f, 3.0f, "%.2f sec");
			ImGui::DragFloat(U8("位置・距離の復帰時間##Rush"), &rushPresentationPositionExitTime, 0.01f, 0.0f, 3.0f, "%.2f sec");
			ImGui::DragFloat(U8("FOVの復帰時間##Rush"), &rushPresentationFovExitTime, 0.01f, 0.0f, 3.0f, "%.2f sec");
			ImGui::TreePop();
		}

		ImGui::SeparatorText(U8("死亡時カメラ"));
		ImGui::DragFloat(U8("前景までの距離"), &deathCameraSettings.foregroundDistance, 0.05f, 0.1f, 15.0f);
		ImGui::DragFloat(U8("横方向オフセット"), &deathCameraSettings.sideOffset, 0.05f, -10.0f, 10.0f);
		ImGui::DragFloat(U8("カメラの高さ"), &deathCameraSettings.cameraHeight, 0.05f, -10.0f, 10.0f);
		ImGui::DragFloat(U8("注視点の高さ"), &deathCameraSettings.lookHeight, 0.05f, -10.0f, 10.0f);
		ImGui::DragFloat(U8("ボスへの注視比率"), &deathCameraSettings.bossLookWeight, 0.05f, -5.0f, 10.0f);
		ImGui::SliderFloat(U8("死亡時FOV"), &deathCameraSettings.fovDegree, 10.0f, 120.0f, "%.1f deg");
		ImGui::DragFloat(U8("死亡時カメラの切替時間"), &deathCameraSettings.deathBlendTime, 0.01f, 0.01f, 5.0f, "%.2f sec");

		if (ImGui::Button(U8("カメラ設定をリセット"))) ResetCameraTuning();
	}

	if (ImGui::CollapsingHeader(U8("TPS画面外攻撃アシスト"),
		ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SeparatorText(U8("アシスト調整"));
		ImGui::DragFloat(U8("水平方向の安全余白"), &offscreenAssistHorizontalSafeMargin, 0.005f, 0.0f, 0.49f);
		ImGui::DragFloat(U8("垂直方向の安全余白"), &offscreenAssistVerticalSafeMargin, 0.005f, 0.0f, 0.49f);
		ImGui::DragFloat(U8("アシスト最大回転速度"), &offscreenAssistMaxAngularSpeedDegree, 1.0f, 0.0f, 720.0f, "%.1f deg/sec");
		ImGui::DragFloat(U8("画面外攻撃時の目標ピッチ角度"), &offscreenAssistTargetPitchDegree, 0.5f, -89.0f, 89.0f, "%.1f deg");
		ImGui::DragFloat(U8("ピッチ角度の補間時間"), &offscreenAssistPitchBlendTime, 0.01f, 0.0f, 3.0f, "%.2f sec");
		ImGui::DragFloat(U8("右スティック操作による解除閾値"), &offscreenAssistRightStickCancelThreshold, 0.005f, 0.0f, 1.0f);
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

	ImGui::DragFloat(U8("右スティックの回転速度"), &rotateSpeed, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat(U8("フォーカス距離"), &focusDistance, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat(U8("最小ピッチ角度"), &minPitchDegree, 0.1f, -90.0f, 90.0f);
	ImGui::DragFloat(U8("最大ピッチ角度"), &maxPitchDegree, 0.1f, -90.0f, 90.0f);
	ImGui::DragFloat(U8("カメラ衝突判定の半径"), &sphereCastRadius, 0.01f, 0.01f, 1.0f);
	ImGui::DragFloat(U8("カメラ切替の補間時間"), &blendDuration, 0.01f, 0.01f, 1.0f);

	if (ImGui::TreeNode(U8("TPSカメラ構図")))
	{
		ImGui::DragFloat(U8("距離##TPS"), &standardTpsSettings.distance, 0.05f, 0.1f, 30.0f);
		ImGui::DragFloat(U8("高さ##TPS"), &standardTpsSettings.height, 0.05f, -10.0f, 10.0f);
		ImGui::DragFloat(U8("注視点の高さ##TPS"), &standardTpsSettings.lookTargetHeight, 0.05f, -10.0f, 10.0f);
		ImGui::DragFloat(U8("FOV##TPS"), &standardTpsSettings.fovDegree, 0.1f, 10.0f, 120.0f);
		ImGui::DragFloat(U8("肩越しオフセット##TPS"), &standardTpsSettings.horizontalOffset, 0.05f, -10.0f, 10.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode(U8("フォーカスカメラ構図")))
	{
		ImGui::DragFloat(U8("距離##Focus"), &focusSettings.distance, 0.05f, 0.1f, 30.0f);
		ImGui::DragFloat(U8("高さ##Focus"), &focusSettings.height, 0.05f, -10.0f, 10.0f);
		ImGui::DragFloat(U8("注視点の高さ##Focus"), &focusSettings.lookTargetHeight, 0.05f, -10.0f, 10.0f);
		ImGui::DragFloat(U8("FOV##Focus"), &focusSettings.fovDegree, 0.1f, 10.0f, 120.0f);
		ImGui::DragFloat(U8("水平方向オフセット##Focus"), &focusSettings.horizontalOffset, 0.05f, -10.0f, 10.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode(U8("ロックオンカメラ構図")))
	{
		ImGui::DragFloat(U8("距離##LockOn"), &lockOnSettings.distance, 0.05f, 0.1f, 30.0f);
		ImGui::DragFloat(U8("高さ##LockOn"), &lockOnSettings.height, 0.05f, -10.0f, 10.0f);
		ImGui::DragFloat(U8("FOV##LockOn"), &lockOnSettings.fovDegree, 0.1f, 10.0f, 120.0f);
		ImGui::DragFloat(U8("プレイヤー注視高さ"), &lockOnPlayerLookHeight, 0.05f, -10.0f, 10.0f);
		ImGui::DragFloat(U8("敵の注視高さ"), &lockOnEnemyLookHeight, 0.05f, -10.0f, 10.0f);
		ImGui::SliderFloat(U8("プレイヤー・敵の注視比率"), &lockOnTargetWeight, 0.0f, 1.0f);
		ImGui::DragFloat(U8("水平方向オフセット##LockOn"), &lockOnSettings.horizontalOffset, 0.05f, -10.0f, 10.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode(U8("ロックオン構図の自動補正")))
	{
		ImGui::DragFloat(U8("衝突補正開始比率"), &lockOnCollisionStartRatio, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat(U8("衝突補正最大比率"), &lockOnCollisionFullRatio, 0.01f, 0.0f, 1.0f);
		ImGui::SliderFloat(U8("壁までの距離倍率"), &lockOnWallDistanceScale, 0.1f, 1.0f);
		ImGui::DragFloat(U8("衝突比率のヒステリシス"), &lockOnCollisionRatioHysteresis, 0.005f, 0.0f, 0.2f);
		ImGui::DragFloat(U8("距離補正開始位置"), &lockOnDistanceStart, 0.05f, 0.0f, 30.0f);
		ImGui::DragFloat(U8("距離補正最大位置"), &lockOnDistanceFull, 0.05f, 0.0f, 30.0f);
		ImGui::DragFloat(U8("Grux ロックオン最大追加距離"), &lockOnMaxDistanceAdd, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat(U8("ズームアウト速度"), &lockOnZoomOutSpeed, 0.1f, 0.01f, 30.0f);
		ImGui::DragFloat(U8("ズームイン速度"), &lockOnZoomInSpeed, 0.1f, 0.01f, 30.0f);
		ImGui::DragFloat(U8("距離補正のデッドゾーン"), &lockOnDistanceDeadZone, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat(U8("構図の補間速度"), &lockOnCompositionLerpSpeed, 0.1f, 0.1f, 30.0f);
		ImGui::DragFloat(U8("安全フレーム 水平余白"), &lockOnSafeFrameHorizontalMargin, 0.005f, 0.0f, 0.49f, "%.3f");
		ImGui::DragFloat(U8("安全フレーム 垂直余白"), &lockOnSafeFrameVerticalMargin, 0.005f, 0.0f, 0.49f, "%.3f");

		ImGui::SeparatorText(U8("壁際の横方向回避"));
		ImGui::DragFloat(U8("近距離時の横方向オフセット"), &lockOnWallEscapeNearOffset, 0.05f, 0.0f, 5.0f, "%.2f m");
		ImGui::DragFloat(U8("遠距離時の横方向オフセット"), &lockOnWallEscapeFarOffset, 0.05f, 0.0f, 5.0f, "%.2f m");
		ImGui::DragFloat(U8("構図悪化判定のFOV閾値"), &lockOnWallEscapeSevereFovThreshold, 0.5f, lockOnSettings.fovDegree, 120.0f, "%.1f deg");
		ImGui::DragFloat(U8("必要なFOV改善量"), &lockOnWallEscapeImprovementThreshold, 0.25f, 0.0f, 30.0f, "%.1f deg");
		ImGui::DragFloat(U8("回避方向切替に必要な改善量"), &lockOnWallEscapeSideSwitchImprovementThreshold, 0.25f, 0.0f, 30.0f, "%.1f deg");
		ImGui::DragFloat(U8("距離補正に必要な改善量"), &lockOnWallEscapeDistanceImprovementThreshold, 0.25f, 0.0f, 30.0f, "%.1f deg");
		ImGui::DragFloat(U8("壁回避の移動速度"), &lockOnWallEscapeMoveSpeed, 0.25f, 0.0f, 20.0f, "%.2f m/s");
		ImGui::DragFloat(U8("壁回避の復帰速度"), &lockOnWallEscapeReturnSpeed, 0.25f, 0.0f, 20.0f, "%.2f m/s");

		ImGui::SeparatorText(U8("補正失敗時の距離復帰"));
		ImGui::DragFloat(U8("最大復帰距離"), &recoveryMaxDistance, 0.05f, 0.0f, 5.0f, "%.2f m");
		ImGui::DragFloat(U8("復帰時の移動速度"), &recoveryMoveSpeed, 0.05f, 0.0f, 10.0f, "%.2f m/s");
		ImGui::DragFloat(U8("復帰後の戻り速度"), &recoveryReturnSpeed, 0.05f, 0.0f, 10.0f, "%.2f m/s");
		ImGui::DragFloat(U8("復帰に必要なFOV改善量"), &recoveryFovImprovementThreshold, 0.25f, 0.0f, 30.0f, "%.1f deg");
		ImGui::DragFloat(U8("構図補正の最大角度"), &compositionMaxCorrectionDegree, 0.25f, 0.0f, 15.0f, "%.1f deg");
		ImGui::DragFloat(U8("構図補正の回転速度"), &compositionAngularSpeedDegree, 1.0f, 0.0f, 180.0f, "%.1f deg/sec");
		ImGui::DragFloat(U8("構図補正のデッドゾーン"), &compositionDeadZoneDegree, 0.1f, 0.0f, 10.0f, "%.1f deg");

		ImGui::SeparatorText(U8("衝突時のFOV補正"));
		ImGui::DragFloat(U8("Grux FOV補正の最大値"), &lockOnMaxFallbackFovDegree, 0.1f, lockOnSettings.fovDegree, 90.0f, "%.1f deg");
		ImGui::DragFloat(U8("FOV拡大速度"), &lockOnFovExpandSpeed, 0.5f, 0.0f, 180.0f, "%.1f deg/s");
		ImGui::DragFloat(U8("FOV復帰速度"), &lockOnFovReturnSpeed, 0.5f, 0.0f, 180.0f, "%.1f deg/s");
		ImGui::DragFloat(U8("FOV補正開始時の不足距離"), &lockOnFovFallbackEnterDeficit, 0.01f, 0.0f, 10.0f, "%.2f m");
		ImGui::DragFloat(U8("FOV補正終了時の不足距離"), &lockOnFovFallbackExitDeficit, 0.01f, 0.0f, 10.0f, "%.2f m");
		ImGui::DragFloat(U8("FOV復帰までの遅延時間"), &lockOnFovFallbackReturnDelay, 0.01f, 0.0f, 2.0f, "%.2f sec");
		ImGui::TreePop();
	}

	ImGui::DragFloat(U8("ロックオン ピッチ角度"), &lockOnPitchDegree, 0.1f, -90.0f, 90.0f);
	ImGui::DragFloat(U8("ロックオン Yaw角度"), &lockOnYawOffsetDegree, 0.1f, -90.0f, 90.0f);
#endif
}
