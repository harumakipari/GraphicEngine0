#include "pch.h"
#include "GruxDashAttackBT.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Scenes/GameScene.h"

// Dash telegraph visuals
void GruxEnemy::BeginDashTelegraphVisual()
{
	HideDashTelegraphVisual();
	dashTelegraphLineStart = dashAttackStartPosition;
	dashTelegraphLineEnd = dashBTPredictedKnockupPosition;
	const DirectX::XMFLOAT3 fanPosition = { dashBTPredictedKnockupPosition.x,
		dashTelegraphFanYOffset, dashBTPredictedKnockupPosition.z };
	const DirectX::XMFLOAT3 fanForward = dashAttackDirection;
	const float dx = dashTelegraphLineEnd.x - dashTelegraphLineStart.x;
	const float dz = dashTelegraphLineEnd.z - dashTelegraphLineStart.z;
	dashTelegraphLineLength = std::sqrt(dx * dx + dz * dz);
	const bool dashTelegraphSnapshotValid = dashTelegraphLineLength > FLT_EPSILON;
	if (!dashTelegraphSnapshotValid || !showDashTelegraph)
		return;


	if (!dashTelegraphLineMeshComponent || !dashTelegraphFanMeshComponent)
		return;

	const float yaw = std::atan2(-fanForward.x, -fanForward.z);
	DirectX::XMFLOAT4 rotation{};
	DirectX::XMStoreFloat4(&rotation, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f));
	dashTelegraphLineMeshComponent->SetRelativeLocationDirect({ dashTelegraphLineStart.x, dashTelegraphLineYOffset, dashTelegraphLineStart.z });
	dashTelegraphLineMeshComponent->SetRelativeRotationDirect(rotation);
	dashTelegraphLineMeshComponent->SetRelativeScaleDirect({dashTelegraphLineWidth, 1.0f, dashTelegraphLineLength });
	dashTelegraphFanMeshComponent->SetRelativeLocationDirect(fanPosition);
	dashTelegraphFanMeshComponent->SetRelativeRotationDirect(rotation);
	dashTelegraphFanMeshComponent->SetRelativeScaleDirect({dashTelegraphFanRadius, 1.0f, dashTelegraphFanRadius });
	dashTelegraphLineMeshComponent->SetIsVisible(true);
	dashTelegraphFanMeshComponent->SetIsVisible(true);
	dashTelegraphVisualState = DashTelegraphVisualState::LineExpanding;
	dashTelegraphVisualElapsed = 0.0f;
	dashTelegraphLineDynamicShrinkActive = false;
	dashTelegraphLineAlpha = 1.0f;
	dashTelegraphFanAlpha = 0.0f;
	dashTelegraphLineMeshComponent->SetRelativeScaleDirect({ dashTelegraphLineWidth, 1.0f, 0.0f });
	dashTelegraphLineMeshComponent->plusAlphaCBuffer->data.brightness = 0.0f;
	dashTelegraphLineMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
	dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.brightness = -1.0f;
	dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
	dashTelegraphFanMeshComponent->SetIsVisible(false);
	PlayBossTelegraphSE("boss_dash_telegraph.wav");
}

void GruxEnemy::UpdateDashTelegraphVisual(float deltaTime)
{
	if (dashTelegraphVisualState == DashTelegraphVisualState::Hidden)
		return;
	if (!dashTelegraphLineMeshComponent || !dashTelegraphFanMeshComponent)
		return;
	const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
	const DirectX::XMFLOAT3& phaseColor = scene && scene->IsBossInFinalPhase()
		? dashTelegraphPhase2EmissiveColor : dashTelegraphPhase1EmissiveColor;
	const DirectX::XMFLOAT4 emissiveColor{ phaseColor.x, phaseColor.y, phaseColor.z, 1.0f };
	const float emissiveIntensity = dashTelegraphEmissiveEnabled
		? (std::max)(0.0f, dashTelegraphEmissiveIntensity) : 0.0f;
	for (const auto& mesh : { dashTelegraphLineMeshComponent, dashTelegraphFanMeshComponent })
	{
		mesh->plusAlphaCBuffer->data.cpuColor = emissiveColor;
		mesh->plusAlphaCBuffer->data.emissionPower = emissiveIntensity;
	}
	const auto setLineAlpha = [&](float alpha)
		{
			dashTelegraphLineAlpha = std::clamp(alpha, 0.0f, 1.0f);
			dashTelegraphLineMeshComponent->plusAlphaCBuffer->data.brightness = dashTelegraphLineAlpha - 1.0f;
		};
	const auto setFanAlpha = [&](float alpha)
		{
			dashTelegraphFanAlpha = std::clamp(alpha, 0.0f, 1.0f);
			dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.brightness = dashTelegraphFanAlpha - 1.0f;
		};

	if (dashTelegraphLineDynamicShrinkActive)
		UpdateDashTelegraphLineShrink();

	dashTelegraphVisualElapsed += deltaTime;
	switch (dashTelegraphVisualState)
	{
	case DashTelegraphVisualState::LineExpanding:
	{
		const float duration = (std::max)(0.001f, dashTelegraphLineExpandDuration);
		const float linear = std::clamp(dashTelegraphVisualElapsed / duration, 0.0f, 1.0f);
		const float lineExpandProgress = 1.0f - (1.0f - linear) * (1.0f - linear);
		dashTelegraphLineMeshComponent->SetRelativeScaleDirect({
			dashTelegraphLineWidth, 1.0f, dashTelegraphLineLength * lineExpandProgress });
		if (linear >= 1.0f)
		{
			dashTelegraphVisualState = DashTelegraphVisualState::FanFadingIn;
			dashTelegraphVisualElapsed = 0.0f;
			dashTelegraphFanMeshComponent->SetIsVisible(true);
		}
		break;
	}
	case DashTelegraphVisualState::FanFadingIn:
	{
		const float duration = (std::max)(0.001f, dashTelegraphFanFadeInDuration);
		const float fanFadeInProgress = std::clamp(dashTelegraphVisualElapsed / duration, 0.0f, 1.0f);
		setFanAlpha(fanFadeInProgress);
		if (fanFadeInProgress >= 1.0f)
			dashTelegraphVisualState = DashTelegraphVisualState::Holding;
		break;
	}
	case DashTelegraphVisualState::DashActive:
		break;
	case DashTelegraphVisualState::FanFlashing:
	{
		const float duration = (std::max)(0.001f, dashTelegraphFanFlashDuration);
		const float progress = std::clamp(dashTelegraphVisualElapsed / duration, 0.0f, 1.0f);
		dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.flashValue =
			dashTelegraphFanFlashIntensity * (1.0f - progress);
		if (progress >= 1.0f)
		{
			dashTelegraphVisualState = DashTelegraphVisualState::DashActive;
			dashTelegraphVisualElapsed = 0.0f;
			dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
		}
		break;
	}
	case DashTelegraphVisualState::FanFadingOut:
	{
		const float lineDuration = (std::max)(0.001f, dashTelegraphLineFadeOutDuration);
		const float fanDuration = (std::max)(0.001f, dashTelegraphFanFadeOutDuration);
		const float lineProgress = std::clamp(dashTelegraphVisualElapsed / lineDuration, 0.0f, 1.0f);
		const float fanProgress = std::clamp(dashTelegraphVisualElapsed / fanDuration, 0.0f, 1.0f);
		setLineAlpha(dashTelegraphLineFadeOutStartAlpha * (1.0f - lineProgress));
		setFanAlpha(dashTelegraphFanFadeOutStartAlpha * (1.0f - fanProgress));
		if (dashTelegraphLineAlpha <= 0.0f)
			dashTelegraphLineMeshComponent->SetIsVisible(false);
		if (dashTelegraphFanAlpha <= 0.0f)
			dashTelegraphFanMeshComponent->SetIsVisible(false);
		if (dashTelegraphLineAlpha <= 0.0f && dashTelegraphFanAlpha <= 0.0f)
			HideDashTelegraphVisual();
		break;
	}
	default: break;
	}
}

void GruxEnemy::BeginDashTelegraphFanFlash()
{
	if (dashTelegraphVisualState == DashTelegraphVisualState::Hidden ||
		!dashTelegraphFanMeshComponent || !dashTelegraphFanMeshComponent->IsVisible())
		return;
	dashTelegraphVisualState = DashTelegraphVisualState::FanFlashing;
	dashTelegraphVisualElapsed = 0.0f;
}

void GruxEnemy::BeginDashTelegraphLineShrink()
{
	if (dashTelegraphVisualState == DashTelegraphVisualState::Hidden ||
		!dashTelegraphLineMeshComponent || !dashTelegraphLineMeshComponent->IsVisible() ||
		dashTelegraphLineLength <= dashTelegraphLineMinimumVisibleLength)
		return;

	dashTelegraphLineDynamicShrinkActive = true;
}

void GruxEnemy::UpdateDashTelegraphLineShrink()
{
	if (!dashTelegraphLineDynamicShrinkActive || !dashTelegraphLineMeshComponent)
		return;

	const float originalDx = dashTelegraphLineEnd.x - dashTelegraphLineStart.x;
	const float originalDz = dashTelegraphLineEnd.z - dashTelegraphLineStart.z;
	const float originalLength = std::sqrt(originalDx * originalDx + originalDz * originalDz);
	if (originalLength <= dashTelegraphLineMinimumVisibleLength)
	{
		dashTelegraphLineMeshComponent->SetIsVisible(false);
		dashTelegraphLineDynamicShrinkActive = false;
		return;
	}

	const DirectX::XMFLOAT3 gruxPosition = GetPosition();
	const float forwardX = originalDx / originalLength;
	const float forwardZ = originalDz / originalLength;
	const float projectedDistance = std::clamp(
		(gruxPosition.x - dashTelegraphLineStart.x) * forwardX +
		(gruxPosition.z - dashTelegraphLineStart.z) * forwardZ,
		0.0f, originalLength);
	const DirectX::XMFLOAT3 dynamicLineStart = {
		dashTelegraphLineStart.x + forwardX * projectedDistance,
		dashTelegraphLineStart.y,
		dashTelegraphLineStart.z + forwardZ * projectedDistance };
	const float remainingDx = dashTelegraphLineEnd.x - dynamicLineStart.x;
	const float remainingDz = dashTelegraphLineEnd.z - dynamicLineStart.z;
	const float currentLineLength = std::sqrt(remainingDx * remainingDx + remainingDz * remainingDz);

	if (currentLineLength <= dashTelegraphLineMinimumVisibleLength)
	{
		dashTelegraphLineMeshComponent->SetIsVisible(false);
		dashTelegraphLineDynamicShrinkActive = false;
		return;
	}

	const float yaw = std::atan2(-remainingDx, -remainingDz);
	DirectX::XMFLOAT4 rotation{};
	DirectX::XMStoreFloat4(&rotation,
		DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f));
	dashTelegraphLineMeshComponent->SetRelativeLocationDirect({
		dynamicLineStart.x, dashTelegraphLineYOffset, dynamicLineStart.z });
	dashTelegraphLineMeshComponent->SetRelativeRotationDirect(rotation);
	dashTelegraphLineMeshComponent->SetRelativeScaleDirect({
		dashTelegraphLineWidth, 1.0f, currentLineLength });
}

void GruxEnemy::BeginDashTelegraphFadeOut()
{
	if (dashTelegraphVisualState == DashTelegraphVisualState::Hidden || !dashTelegraphFanMeshComponent ||
		!dashTelegraphFanMeshComponent->IsVisible())
		return;

	dashTelegraphVisualState = DashTelegraphVisualState::FanFadingOut;
	dashTelegraphVisualElapsed = 0.0f;
	dashTelegraphLineDynamicShrinkActive = false;
	dashTelegraphLineFadeOutStartAlpha = dashTelegraphLineMeshComponent &&
		dashTelegraphLineMeshComponent->IsVisible() ? dashTelegraphLineAlpha : 0.0f;
	dashTelegraphFanFadeOutStartAlpha = dashTelegraphFanAlpha;
	dashTelegraphLineMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
	dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
}

void GruxEnemy::HideDashTelegraphVisual()
{
	if (dashTelegraphLineMeshComponent)
		dashTelegraphLineMeshComponent->SetIsVisible(false);
	if (dashTelegraphFanMeshComponent)
		dashTelegraphFanMeshComponent->SetIsVisible(false);
	dashTelegraphVisualState = DashTelegraphVisualState::Hidden;
	dashTelegraphVisualElapsed = 0.0f;
	dashTelegraphLineDynamicShrinkActive = false;
	dashTelegraphLineAlpha = 0.0f;
	dashTelegraphFanAlpha = 0.0f;
	dashTelegraphLineFadeOutStartAlpha = 0.0f;
	dashTelegraphFanFadeOutStartAlpha = 0.0f;
	if (dashTelegraphLineMeshComponent)
	{
		dashTelegraphLineMeshComponent->plusAlphaCBuffer->data.brightness = 0.0f;
		dashTelegraphLineMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
	}
	if (dashTelegraphFanMeshComponent)
	{
		dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.brightness = 0.0f;
		dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
	}
}

// Dash BT nodes
bool DashPlanAvailable::Judgment()
{
	return owner->IsBattleAIActive() &&
		!owner->IsStunned() &&
		owner->CanPlanDashAttack();
}


ActionBase::State PrepareDashSetupTarget::Run(float)
{
	return owner->PrepareDashAttackSetupTarget() ? State::Complete : State::Failed;
}

ActionBase::State FaceDashPlayerIfNeeded::Run(float deltaTime)
{
	const auto context = owner->BuildTargetContext();
	if (!context.valid || owner->IsDead())
		return State::Failed;
	owner->SetDashBTFacing();
	if (context.absoluteAngleDegrees <= owner->GetTurnCompleteAngle())
		return State::Complete;
	owner->RotateTowardsPlayer(context.directionToPlayer, owner->GetTurnSpeed(), deltaTime);
	return State::Run;
}

ActionBase::State CanExecuteDashAttack::Run(float)
{
	const bool ready = owner->CanExecuteDashAttack();
	return ready ? State::Complete : State::Failed;
}

ActionBase::State StartDashAttack::Run(float)
{
	return owner->StartDashAttackTelegraph() ? State::Complete : State::Failed;
}

ActionBase::State ExecuteDashAttack::Run(float deltaTime)
{
	switch (owner->UpdateDashAttackBT(deltaTime))
	{
	case GruxEnemy::DashBTResult::Running: return State::Run;
	case GruxEnemy::DashBTResult::Complete: return State::Complete;
	default: return State::Failed;
	}
}

// Dash planning and preparation
bool GruxEnemy::CanPlanDashAttack() const
{
	if (IsDead() || !BuildTargetContext().valid || IsDeathPresentationActive())
		return false;
	for (size_t i = 0; i < combatActionData.size(); ++i)
	{
		if (combatActionData[i].type == BossActionType::DashAttack)
			return combatActionCooldownRemaining[i] <= 0.0f;
	}
	return false;
}

bool GruxEnemy::PrepareDashAttackSetupTarget()
{
	if (!CanPlanDashAttack())
		return false;
	dashBTPhase = DashBTPhase::Setup;
	dashBTPreviousAction = selectedActionType;
	selectedAttackType = BossAttackType::DashAttack;
	selectedActionType = BossActionType::DashAttack;
	StopAttackSetupMovement();
	ClearAttackSetupTarget();

	DirectX::XMFLOAT3 target{};
	if (!FindAttackSetupTarget(dashSetupDistanceMin, dashSetupDistanceMax,
		attackSetupCandidateAngleStep, attackSetupClampTolerance, dashSetupMinimumMoveDistance,
		target))
	{
		CleanupDashAttackBT();
		return false;
	}
	attackSetupTarget = {};
	attackSetupTarget.valid = true;
	attackSetupTarget.targetPosition = target;
	attackSetupTarget.stuckMovementThreshold = 0.01f;
	return true;
}

bool GruxEnemy::CanExecuteDashAttack() const
{
	return !IsDead() && BuildTargetContext().valid &&
		dashBTPhase == DashBTPhase::Facing && !dashBTAttackStarted &&
		selectedAttackType == BossAttackType::DashAttack && attackSetupTarget.valid &&
		!attackSetupRuntime.movementActive && !dashAttackMovementActive &&
		characterMovementComponent && rotationComponent && GetBodyAnimationController();
}

// Dash execution and cleanup
bool GruxEnemy::StartDashAttackTelegraph()
{
	if (!CanExecuteDashAttack())
		return false;
	const auto controller = GetBodyAnimationController();
	for (const char* animation : { "Pre_Stampede_0", "Stampede_0", "Stampede_Knockup_0" })
	{
		if (!controller->GetAnimationAsset(animation))
			return false;
	}
	StopAttackSetupMovement();
	ClearAttackSetupTarget();
	StartAttack();
	dashBTAttackStarted = true;
	const bool isTripleDash = IsBossInFinalPhase();
	dashBTDashIndex = 0;
	dashBTDashMaxCount = isTripleDash ? 3 : 1;
	dashBTCurrentDashHitStartCount = GetCurrentAttackHitCount();
	dashBTAbortRemainingDashes = false;
	dashBTTransitionElapsed = 0.0f;
	dashBTDirectionLocked = false;
	dashBTMovementSnapshotPrepared = false;
	dashBTDirectionLockRequested = false;
	dashBTDashStartRequested = false;
	dashBTPhase = DashBTPhase::Telegraph;
	if (!PlayAttackStage(BossAttackType::DashAttack, 0))
		return false;
	OnSelectedActionStartedSuccessfully();
	return true;
}

GruxEnemy::DashBTResult GruxEnemy::UpdateDashAttackBT(float deltaTime)
{
	const auto controller = GetBodyAnimationController();
	if (IsDead() || !controller || !dashBTAttackStarted)
		return DashBTResult::Failed;
	const float dt = (std::max)(0.0f, deltaTime);
	switch (dashBTPhase)
	{
	case DashBTPhase::Telegraph:
	{
		const auto context = BuildTargetContext();
		if (!context.valid)
			return DashBTResult::Failed;
		StopAIMovement();

		if (!dashBTDirectionLocked)
			RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt);

		if (!dashBTDirectionLocked && dashBTDirectionLockRequested)
		{
			dashBTDirectionLockRequested = false;
			if (!PrepareDashAttackMovementSnapshot())
				return DashBTResult::Failed;
			dashBTDirectionLocked = true;
			BeginDashTelegraphVisual();
		}

		if (dashBTDirectionLocked && dashBTDashStartRequested)
		{
			dashBTDashStartRequested = false;
			BeginAdditionalAttackStage();
			if (!PlayAttackStage(BossAttackType::DashAttack, 1))
				return DashBTResult::Failed;
			BeginDashTelegraphLineShrink();
			dashBTPhase = DashBTPhase::Movement;
			return DashBTResult::Running;
		}
		if (controller->GetCurrentAnimationName() != "Pre_Stampede_0" ||
			!controller->IsPlayAnimation())
			return DashBTResult::Failed;
		return DashBTResult::Running;
	}
	case DashBTPhase::Movement:
	{
		if (GetCurrentAttackHitCount() > dashBTCurrentDashHitStartCount ||
			WasCurrentAttackSequenceJustDodged())
		{
			dashBTAbortRemainingDashes = true;
		}
		const bool finished = UpdateDashAttackMovement(dt, true);
		if (!finished && controller->IsPlayAnimation())
			return DashBTResult::Running;
		if (GetCurrentAttackHitCount() > dashBTCurrentDashHitStartCount ||
			WasCurrentAttackSequenceJustDodged())
		{
			dashBTAbortRemainingDashes = true;
		}
		StopDashAttackMovement();
		BeginAdditionalAttackStage();
		dashBTCurrentDashHitStartCount = GetCurrentAttackHitCount();
		if (!PlayAttackStage(BossAttackType::DashAttack, 2))
			return DashBTResult::Failed;
		dashBTPhase = DashBTPhase::Knockup;
		return DashBTResult::Running;
	}
	case DashBTPhase::Knockup:
		if (GetCurrentAttackHitCount() > dashBTCurrentDashHitStartCount ||
			WasCurrentAttackSequenceJustDodged())
		{
			dashBTAbortRemainingDashes = true;
		}
		if (controller->IsPlayAnimation())
			return DashBTResult::Running;
		if (!dashBTAbortRemainingDashes && dashBTDashIndex + 1 < dashBTDashMaxCount)
		{
			StopDashAttackMovement();
			DisableAttackHitBoxes();
			dashBTPhase = DashBTPhase::InterDashTransition;
			dashBTTransitionElapsed = 0.0f;
			dashBTDirectionLocked = false;
			dashBTMovementSnapshotPrepared = false;
			dashBTDirectionLockRequested = false;
			dashBTDashStartRequested = false;
			return DashBTResult::Running;
		}
		FinishDashAttackBT();
		return DashBTResult::Complete;
	case DashBTPhase::InterDashTransition:
	{
		StopDashAttackMovement();
		DisableAttackHitBoxes();
		const auto context = BuildTargetContext();
		if (!context.valid)
			return DashBTResult::Failed;
		const float transitionDuration = (std::max)(0.0f, dashBTTransitionDuration);
		const float trackingDuration = std::clamp(dashBTInterDashTrackingDuration,
			0.0f, transitionDuration);
		if (!dashBTDirectionLocked && dashBTTransitionElapsed < trackingDuration)
			RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt);
		dashBTTransitionElapsed += dt;
		if (!dashBTDirectionLocked && dashBTTransitionElapsed >= trackingDuration)
		{
			if (!PrepareDashAttackMovementSnapshot())
				return DashBTResult::Failed;
			dashBTDirectionLocked = true;
			BeginDashTelegraphVisual();
		}
		if (dashBTTransitionElapsed < transitionDuration)
			return DashBTResult::Running;

		++dashBTDashIndex;
		BeginAdditionalAttackStage();
		DisableAttackHitBoxes();
		if (!PlayAttackStage(BossAttackType::DashAttack, 1))
			return DashBTResult::Failed;
		BeginDashTelegraphLineShrink();
		dashBTCurrentDashHitStartCount = GetCurrentAttackHitCount();
		dashBTPhase = DashBTPhase::Movement;
		return DashBTResult::Running;
	}
	default:
		return DashBTResult::Failed;
	}
}

void GruxEnemy::FinishDashAttackBT()
{
	CleanupDashAttackBT();
	dashBTPhase = DashBTPhase::Recovery;
}

void GruxEnemy::CleanupDashAttackBT()
{
	// Dash固有のRuntime、演出、当たり判定を後始末する。
	HideDashTelegraphVisual();
	if (!IsDashAttackBTActive())
		return;
	if (dashBTAttackStarted)
	{
		StartSelectedActionCooldown();
		dashBTAttackStarted = false;
	}
	StopDashAttackMovement();
	ClearAttackSetupTarget();
	const bool externallyInterrupted = IsDead() || IsDeathPresentationActive() || IsStunned();
	if (!externallyInterrupted)
		EndPositioningAnimation();
	DisableAttackHitBoxes();
	dashBTPhase = DashBTPhase::None;
	dashBTDashIndex = 0;
	dashBTDashMaxCount = 1;
	dashBTCurrentDashHitStartCount = 0;
	dashBTAbortRemainingDashes = false;
	dashBTTransitionElapsed = 0.0f;
	dashBTDirectionLocked = false;
	dashBTMovementSnapshotPrepared = false;
	dashBTDirectionLockRequested = false;
	dashBTDashStartRequested = false;
	dashBTPredictedKnockupPosition = {};
	dashAttackDirection = {};
	dashAttackStartPosition = {};
	dashTargetPosition = {};
	calculatedDashAttackDistance = 0.0f;
	selectedActionType = dashBTPreviousAction;
}
