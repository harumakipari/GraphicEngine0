#include "pch.h"
#include "GruxDashAttackBT.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/State/StateMachine.h"

void GruxEnemy::EnsureDashTelegraphVisual()
{
    if (!dashTelegraphLineMeshComponent)
        dashTelegraphLineMeshComponent = AddComponent<StaticMeshComponent>("dashTelegraphLine", "GruxEnemy");
    if (!dashTelegraphFanMeshComponent)
        dashTelegraphFanMeshComponent = AddComponent<StaticMeshComponent>("dashTelegraphFan", "GruxEnemy");

    const auto configure = [](const std::shared_ptr<StaticMeshComponent>& mesh, const char* modelPath,
        const bool retainVerticesForDebug)
    {
        if (!mesh->model)
            mesh->SetModel(modelPath, retainVerticesForDebug);
        if (mesh->model)
            for (auto& material : mesh->model->materials)
                material.data.alphaMode = 2; // BLEND: asset mask supplies alpha.
        mesh->overrideDeferredPipelineName = "chargeTelegraphUnlitForward";
        mesh->overrideForwardPipelineName = "chargeTelegraphUnlitForward";
        mesh->SetIsCastShadow(false);
        mesh->SetIsVisible(false);
        mesh->SetUsingAbsoluteLocation(true);
        mesh->SetUsingAbsoluteRotation(true);
        mesh->SetUsingAbsoluteScale(true);
        mesh->plusAlphaCBuffer->data.cpuColor = { 1.0f, 0.16f, 0.03f, 1.0f };
        mesh->plusAlphaCBuffer->data.emissionPower = 0.0f;
        mesh->plusAlphaCBuffer->data.objectType = ObjectType::NoLighting;
    };
    // The on-disk asset name intentionally follows the supplied filename
    configure(dashTelegraphLineMeshComponent, "./Data/Models/EffectModel/DashTelegraphPlane.glb", true);
    configure(dashTelegraphFanMeshComponent, "./Data/Models/EffectModel/DashTelegraphFan.glb", false);
}

void GruxEnemy::BeginDashTelegraphVisual()
{
    HideDashTelegraphVisual();
    // Copy the locked gameplay snapshot once. It remains valid after BT cleanup
    // so Force Show can inspect this exact leg later.
    dashTelegraphLineStart = dashAttackStartPosition;
    dashTelegraphLineEnd = dashBTPredictedKnockupPosition;
    dashTelegraphFanPosition = { dashBTPredictedKnockupPosition.x,
        dashTelegraphFanYOffset, dashBTPredictedKnockupPosition.z };
    dashTelegraphFanForward = dashAttackDirection;
    const float dx = dashTelegraphLineEnd.x - dashTelegraphLineStart.x;
    const float dz = dashTelegraphLineEnd.z - dashTelegraphLineStart.z;
    dashTelegraphLineLength = std::sqrt(dx * dx + dz * dz);
    dashTelegraphSnapshotValid = dashTelegraphLineLength > FLT_EPSILON;
    if (!dashTelegraphSnapshotValid || (!showDashTelegraph && !forceShowDashTelegraph))
        return;

    EnsureDashTelegraphVisual();
    ApplyDashTelegraphVisualSnapshot();
    dashTelegraphVisualState = DashTelegraphVisualState::LineExpanding;
    dashTelegraphVisualElapsed = 0.0f;
    dashTelegraphLineExpandProgress = 0.0f;
    dashTelegraphLineDynamicShrinkActive = false;
    dashTelegraphDynamicLineStart = dashTelegraphLineStart;
    dashTelegraphCurrentLineLength = dashTelegraphLineLength;
    dashTelegraphLineShrinkProgress = 0.0f;
    // No FadeOut is armed until the authored Knockup event arrives.
    dashTelegraphLineFadeOutElapsed = -1.0f;
    dashTelegraphLineAlpha = 1.0f;
    dashTelegraphFanFadeInProgress = 0.0f;
    dashTelegraphFanAlpha = 0.0f;
    dashTelegraphFanFlashActive = false;
    dashTelegraphLineMeshComponent->SetRelativeScaleDirect({ dashTelegraphLineWidth, 1.0f, 0.0f });
    dashTelegraphLineMeshComponent->plusAlphaCBuffer->data.brightness = 0.0f;
    dashTelegraphLineMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
    dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.brightness = -1.0f;
    dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
    dashTelegraphFanMeshComponent->SetIsVisible(false);
}

void GruxEnemy::UpdateDashTelegraphVisual(float deltaTime)
{
    if (forceShowDashTelegraph || dashTelegraphVisualState == DashTelegraphVisualState::Hidden)
        return;
    if (!dashTelegraphLineMeshComponent || !dashTelegraphFanMeshComponent)
        return;
    const float dt = (std::max)(0.0f, deltaTime);
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

    dashTelegraphVisualElapsed += dt;
    switch (dashTelegraphVisualState)
    {
    case DashTelegraphVisualState::LineExpanding:
    {
        const float duration = (std::max)(0.001f, dashTelegraphLineExpandDuration);
        const float linear = std::clamp(dashTelegraphVisualElapsed / duration, 0.0f, 1.0f);
        dashTelegraphLineExpandProgress = 1.0f - (1.0f - linear) * (1.0f - linear);
        dashTelegraphLineMeshComponent->SetRelativeScaleDirect({
            dashTelegraphLineWidth, 1.0f, dashTelegraphLineLength * dashTelegraphLineExpandProgress });
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
        dashTelegraphFanFadeInProgress = std::clamp(dashTelegraphVisualElapsed / duration, 0.0f, 1.0f);
        setFanAlpha(dashTelegraphFanFadeInProgress);
        if (dashTelegraphFanFadeInProgress >= 1.0f)
            dashTelegraphVisualState = DashTelegraphVisualState::Holding;
        break;
    }
    case DashTelegraphVisualState::DashActive:
        // Dash start and Flash completion retain the locked Line/Fan snapshot.
        break;
    case DashTelegraphVisualState::FanFlashing:
    {
        const float duration = (std::max)(0.001f, dashTelegraphFanFlashDuration);
        const float progress = std::clamp(dashTelegraphVisualElapsed / duration, 0.0f, 1.0f);
        dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.flashValue =
            dashTelegraphFanFlashIntensity * (1.0f - progress);
        if (progress >= 1.0f)
        {
            // Flash is a Timeline event, but it must not implicitly hide the fan.
            // The authored FadeOut event owns that later transition.
            dashTelegraphVisualState = DashTelegraphVisualState::DashActive;
            dashTelegraphVisualElapsed = 0.0f;
            dashTelegraphFanFlashActive = false;
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
    dashTelegraphFanFlashActive = true;
}

void GruxEnemy::BeginDashTelegraphLineShrink()
{
    if (dashTelegraphVisualState == DashTelegraphVisualState::Hidden ||
        !dashTelegraphLineMeshComponent || !dashTelegraphLineMeshComponent->IsVisible() ||
        dashTelegraphLineLength <= dashTelegraphLineMinimumVisibleLength)
        return;

    dashTelegraphLineDynamicShrinkActive = true;
    dashTelegraphDynamicLineStart = dashTelegraphLineStart;
    dashTelegraphCurrentLineLength = dashTelegraphLineLength;
    dashTelegraphLineShrinkProgress = 0.0f;
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
        dashTelegraphCurrentLineLength = 0.0f;
        dashTelegraphLineShrinkProgress = 1.0f;
        return;
    }

    // Use Grux's locked-path progress, never the player's current position.
    const DirectX::XMFLOAT3 gruxPosition = GetPosition();
    const float forwardX = originalDx / originalLength;
    const float forwardZ = originalDz / originalLength;
    const float projectedDistance = std::clamp(
        (gruxPosition.x - dashTelegraphLineStart.x) * forwardX +
        (gruxPosition.z - dashTelegraphLineStart.z) * forwardZ,
        0.0f, originalLength);
    dashTelegraphDynamicLineStart = {
        dashTelegraphLineStart.x + forwardX * projectedDistance,
        dashTelegraphLineStart.y,
        dashTelegraphLineStart.z + forwardZ * projectedDistance };
    const float remainingDx = dashTelegraphLineEnd.x - dashTelegraphDynamicLineStart.x;
    const float remainingDz = dashTelegraphLineEnd.z - dashTelegraphDynamicLineStart.z;
    dashTelegraphCurrentLineLength = std::sqrt(remainingDx * remainingDx + remainingDz * remainingDz);
    dashTelegraphLineShrinkProgress = std::clamp(projectedDistance / originalLength, 0.0f, 1.0f);

    if (dashTelegraphCurrentLineLength <= dashTelegraphLineMinimumVisibleLength)
    {
        dashTelegraphLineMeshComponent->SetIsVisible(false);
        dashTelegraphLineDynamicShrinkActive = false;
        return;
    }

    // The mesh pivot is its local start edge; place that edge at the moving
    // start and scale only the recomputed remaining segment toward the fixed end.
    const float yaw = std::atan2(-remainingDx, -remainingDz);
    DirectX::XMFLOAT4 rotation{};
    DirectX::XMStoreFloat4(&rotation,
        DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f));
    dashTelegraphLineMeshComponent->SetRelativeLocationDirect({
        dashTelegraphDynamicLineStart.x, dashTelegraphLineYOffset, dashTelegraphDynamicLineStart.z });
    dashTelegraphLineMeshComponent->SetRelativeRotationDirect(rotation);
    dashTelegraphLineMeshComponent->SetRelativeScaleDirect({
        dashTelegraphLineWidth, 1.0f, dashTelegraphCurrentLineLength });
}

void GruxEnemy::BeginDashTelegraphFadeOut()
{
    if (dashTelegraphVisualState == DashTelegraphVisualState::Hidden ||
        forceShowDashTelegraph || !dashTelegraphFanMeshComponent ||
        !dashTelegraphFanMeshComponent->IsVisible())
        return;

    dashTelegraphFadeOutEventReceived = true;
    dashTelegraphFadeOutActive = true;
    dashTelegraphVisualState = DashTelegraphVisualState::FanFadingOut;
    dashTelegraphVisualElapsed = 0.0f;
    dashTelegraphLineDynamicShrinkActive = false;
    dashTelegraphLineFadeOutStartAlpha = dashTelegraphLineMeshComponent &&
        dashTelegraphLineMeshComponent->IsVisible() ? dashTelegraphLineAlpha : 0.0f;
    dashTelegraphFanFadeOutStartAlpha = dashTelegraphFanAlpha;
    dashTelegraphFanFlashActive = false;
    dashTelegraphLineMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
    dashTelegraphFanMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
}

void GruxEnemy::ApplyDashTelegraphVisualSnapshot()
{
    if (!dashTelegraphSnapshotValid ||
        (!showDashTelegraph && !forceShowDashTelegraph))
    {
        HideDashTelegraphVisual();
        return;
    }

    EnsureDashTelegraphVisual();
    if (!dashTelegraphLineMeshComponent || !dashTelegraphFanMeshComponent)
        return;

    // Both Dash assets are local -Z forward. The line model pivot is its start edge.
    const float yaw = std::atan2(-dashTelegraphFanForward.x, -dashTelegraphFanForward.z);
    DirectX::XMFLOAT4 rotation{};
    DirectX::XMStoreFloat4(&rotation,
        DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f));
    // These components use absolute transforms. As in Jump Telegraph, their
    // relative values are therefore the intended world values; SetWorld* would
    // first apply Grux's inverse parent transform and place the mesh incorrectly.
    dashTelegraphLineMeshComponent->SetRelativeLocationDirect({
        dashTelegraphLineStart.x, dashTelegraphLineYOffset, dashTelegraphLineStart.z });
    dashTelegraphLineMeshComponent->SetRelativeRotationDirect(rotation);
    dashTelegraphLineMeshComponent->SetRelativeScaleDirect({
        dashTelegraphLineWidth, 1.0f, dashTelegraphLineLength });
    dashTelegraphFanMeshComponent->SetRelativeLocationDirect(dashTelegraphFanPosition);
    dashTelegraphFanMeshComponent->SetRelativeRotationDirect(rotation);
    dashTelegraphFanMeshComponent->SetRelativeScaleDirect({
        dashTelegraphFanRadius, 1.0f, dashTelegraphFanRadius });
    dashTelegraphLineMeshComponent->SetIsVisible(true);
    dashTelegraphFanMeshComponent->SetIsVisible(true);
    dashTelegraphActive = true;
}

void GruxEnemy::HideDashTelegraphVisual()
{
    if (dashTelegraphLineMeshComponent)
        dashTelegraphLineMeshComponent->SetIsVisible(false);
    if (dashTelegraphFanMeshComponent)
        dashTelegraphFanMeshComponent->SetIsVisible(false);
    dashTelegraphActive = false;
    dashTelegraphVisualState = DashTelegraphVisualState::Hidden;
    dashTelegraphVisualElapsed = 0.0f;
    dashTelegraphLineExpandProgress = 0.0f;
    dashTelegraphLineDynamicShrinkActive = false;
    dashTelegraphDynamicLineStart = {};
    dashTelegraphCurrentLineLength = 0.0f;
    dashTelegraphLineShrinkProgress = 0.0f;
    dashTelegraphLineFadeOutElapsed = 0.0f;
    dashTelegraphLineAlpha = 0.0f;
    dashTelegraphFanFadeInProgress = 0.0f;
    dashTelegraphFanAlpha = 0.0f;
    dashTelegraphFanFlashActive = false;
    dashTelegraphFadeOutEventReceived = false;
    dashTelegraphFadeOutActive = false;
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

void GruxEnemy::UpdateDashTelegraphVisualDebug()
{
    if (forceShowDashTelegraph)
        ApplyDashTelegraphVisualSnapshot();
}

bool DashPlanAvailable::Judgment()
{
    const auto machine = owner->GetStateMachine();
    return owner->IsBattleAIActive() &&
        (!machine || std::strcmp(machine->GetStateName(), "EnemyStunState") != 0) &&
        owner->CanPlanDashAttack();
}

ActionBase::State CanPlanDashAttack::Run(float)
{
    const bool ready = owner->CanPlanDashAttack();
    owner->SetBehaviorTreeLastJudgment(ready ? "CanPlanDashAttack: true" : "CanPlanDashAttack: false");
    return ready ? State::Complete : State::Failed;
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
    // Same face-complete policy as Jump; no FastCombo range dependency.
    if (context.absoluteAngleDegrees <= owner->GetTurnCompleteAngle())
        return State::Complete;
    owner->RotateTowardsPlayer(context.directionToPlayer, owner->GetTurnSpeed(), deltaTime, "BT_DashFacePlayer");
    return State::Run;
}

ActionBase::State CanExecuteDashAttack::Run(float)
{
    const bool ready = owner->CanExecuteDashAttack();
    owner->SetBehaviorTreeLastJudgment(ready ? "CanExecuteDashAttack: true" : "CanExecuteDashAttack: false");
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

bool GruxEnemy::CanPlanDashAttack() const
{
    if (IsDead() || !BuildTargetContext().valid ||
        (stateMachine_ && std::strcmp(stateMachine_->GetStateName(), "EnemyDeathState") == 0))
        return false;
    // Cooldowns are indexed by Action, not by the independently ordered Attack table.
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
    SetBehaviorAttackResult(BehaviorAttackResult::None);
    StopAttackSetupMovement();
    ClearAttackSetupTarget();

    DirectX::XMFLOAT3 target{};
    float chosenDistance = 0.0f;
    int candidateCount = 0;
    if (!FindAttackSetupTarget(dashSetupDistanceMin, dashSetupDistanceMax,
        attackSetupCandidateAngleStep, attackSetupClampTolerance, dashSetupMinimumMoveDistance,
        target, chosenDistance, candidateCount))
    {
        CleanupDashAttackBT();
        return false;
    }
    // The only write of the setup destination for this plan. Movement never resamples it.
    attackSetupTarget = {};
    attackSetupTarget.valid = true;
    attackSetupTarget.targetPosition = target;
    // The shared mover compares per-frame displacement; avoid false stalls at high FPS.
    // This context value is Dash-only and does not change Jump's movement settings.
    attackSetupTarget.stuckMovementThreshold = 0.01f;
    const auto position = GetPosition();
    const float dx = target.x - position.x;
    const float dz = target.z - position.z;
    attackSetupChosenDistance = chosenDistance;
    attackSetupCandidateCount = candidateCount;
    attackSetupPlannedMoveDistance = std::sqrt(dx * dx + dz * dz);
    attackSetupRemainingDistance = attackSetupPlannedMoveDistance;
    return true;
}

bool GruxEnemy::CanExecuteDashAttack() const
{
    return !IsDead() && BuildTargetContext().valid &&
        dashBTPhase == DashBTPhase::Facing && !dashBTAttackStarted &&
        selectedAttackType == BossAttackType::DashAttack && attackSetupTarget.valid &&
        !attackSetupMovementActive && !dashAttackMovementActive &&
        characterMovementComponent && rotationComponent && GetBodyAnimationController();
}

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
    StartAttack(); // Exactly once for the entire three-stage attack.
    dashBTAttackStarted = true;
    // Reuse the exact final-phase predicate already used by Triple Jump; Dash's
    // animation stages remain 0=Pre, 1=Stampede, 2=Knockup in either phase.
    dashBTTripleDashActive = IsTripleJumpPhase2();
    dashBTDashIndex = 0;
    dashBTDashMaxCount = dashBTTripleDashActive ? 3 : 1;
    dashBTCurrentDashHitStartCount = GetCurrentAttackHitCount();
    dashBTAbortRemainingDashes = false;
    dashBTTransitionElapsed = 0.0f;
    dashBTTelegraphElapsed = 0.0f;
    dashBTTelegraphHoldDuration = 0.0f;
    dashBTDirectionLocked = false;
    dashBTMovementSnapshotPrepared = false;
    dashBTDirectionLockRequested = false;
    dashBTDashStartRequested = false;
    dashBTDirectionLockEventReceived = false;
    dashBTDashStartEventReceived = false;
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

        // Dash 1 stays on Pre_Stampede until its authored events arrive. This
        // makes tracking, lock, telegraph hold and Stampede start follow the
        // Animation Playhead after the Speed Curve has been applied.
        if (!dashBTDirectionLocked)
            RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt, "BT_DashTelegraph");

        if (!dashBTDirectionLocked && dashBTDirectionLockRequested)
        {
            dashBTDirectionLockRequested = false;
            if (!PrepareDashAttackMovementSnapshot())
                return DashBTResult::Failed;
            dashBTDirectionLocked = true;
            dashBTTelegraphHoldDuration = 0.0f;
            BeginDashTelegraphVisual();
        }

        if (dashBTDirectionLocked && dashBTDashStartRequested)
        {
            dashBTDashStartRequested = false;
            BeginAdditionalAttackStage();
            // Stage 1 starts movement from the Direction Lock snapshot only.
            if (!PlayAttackStage(BossAttackType::DashAttack, 1))
                return DashBTResult::Failed;
            BeginDashTelegraphLineShrink();
            dashBTPhase = DashBTPhase::Movement;
            return DashBTResult::Running;
        }

        // A malformed/missing event must fail through the existing cleanup path
        // rather than leave a Telegraph visible forever.
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
            // Do not cut Stampede/Knockup: only prevent another dash once this
            // dash's required animation stages have completed.
            dashBTAbortRemainingDashes = true;
        }
        const auto position = GetPosition();
        dashBTTraveledDistance = (position.x - dashAttackStartPosition.x) * dashAttackDirection.x +
            (position.z - dashAttackStartPosition.z) * dashAttackDirection.z;
        const bool finished = UpdateDashAttackMovement(dt, true);
        if (!finished && controller->IsPlayAnimation())
            return DashBTResult::Running;
        // Movement and hit notifications may be updated in the same frame.
        // Sample once more before resetting the per-animation-stage hit list.
        if (GetCurrentAttackHitCount() > dashBTCurrentDashHitStartCount ||
            WasCurrentAttackSequenceJustDodged())
        {
            dashBTAbortRemainingDashes = true;
        }
        StopDashAttackMovement();
        BeginAdditionalAttackStage();
        dashBTCurrentDashHitStartCount = GetCurrentAttackHitCount();
        // This is the exact actor position at the Stage1 -> Knockup handoff.
        // Compare it with the nominal arrival/timeout prediction captured when
        // this dash leg began; do not alter movement to make them agree.
        dashBTActualKnockupStartPosition = GetPosition();
        const float predictionDx = dashBTActualKnockupStartPosition.x -
            dashBTPredictedKnockupPosition.x;
        const float predictionDz = dashBTActualKnockupStartPosition.z -
            dashBTPredictedKnockupPosition.z;
        dashBTPredictionError = std::sqrt(predictionDx * predictionDx +
            predictionDz * predictionDz);
        dashBTKnockupPositionCaptured = true;
        if (!PlayAttackStage(BossAttackType::DashAttack, 2))
            return DashBTResult::Failed;
        dashBTPhase = DashBTPhase::Knockup;
        return DashBTResult::Running;
    }
    case DashBTPhase::Knockup:
        // Hit / Just Dodge never short-circuits the final attack animation.
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
            dashBTTelegraphHoldDuration = 0.0f;
            dashBTDirectionLocked = false;
            dashBTMovementSnapshotPrepared = false;
            // Dash 2/3 use InterDashTransition, not Pre_Stampede events.
            dashBTDirectionLockRequested = false;
            dashBTDashStartRequested = false;
            dashBTDirectionLockEventReceived = false;
            dashBTDashStartEventReceived = false;
            return DashBTResult::Running;
        }
        SetBehaviorAttackResult(WasCurrentAttackSequenceJustDodged()
            ? BehaviorAttackResult::JustDodged : BehaviorAttackResult::Success);
        OnSelectedAttackCompletedSuccessfully();
        FinishDashAttackBT();
        return DashBTResult::Complete;
    case DashBTPhase::InterDashTransition:
    {
        // The beginning of each inter-dash transition tracks the player. Once
        // locked, its remaining duration is a stable telegraph hold interval.
        StopDashAttackMovement();
        DisableAttackHitBoxes();
        const auto context = BuildTargetContext();
        if (!context.valid)
            return DashBTResult::Failed;
        const float transitionDuration = (std::max)(0.0f, dashBTTransitionDuration);
        const float trackingDuration = std::clamp(dashBTInterDashTrackingDuration,
            0.0f, transitionDuration);
        if (!dashBTDirectionLocked && dashBTTransitionElapsed < trackingDuration)
            RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), dt,
                "BT_DashInterTransition");
        dashBTTransitionElapsed += dt;
        if (!dashBTDirectionLocked && dashBTTransitionElapsed >= trackingDuration)
        {
            if (!PrepareDashAttackMovementSnapshot())
                return DashBTResult::Failed;
            dashBTDirectionLocked = true;
            dashBTTelegraphHoldDuration = (std::max)(0.0f,
                transitionDuration - trackingDuration);
            BeginDashTelegraphVisual();
        }
        if (dashBTTransitionElapsed < transitionDuration)
            return DashBTResult::Running;

        ++dashBTDashIndex;
        // Per-stage reset: hitActors and Notify state are reset, while sequence
        // outcome data (Just Dodge / cumulative hit count) remains intact.
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
    HideDashTelegraphVisual();
    if (!IsDashAttackBTActive())
        return;
    if (dashBTAttackStarted)
    {
        // Match EnemyAttackState::Exit timing, including interruption after StartAttack.
        StartSelectedActionCooldown();
        dashBTAttackStarted = false;
    }
    StopDashAttackMovement();
    ClearAttackSetupTarget();
    // Do not replace an externally started Death / Stun animation with positioning idle.
    const bool externallyInterrupted = IsDead() || (stateMachine_ &&
        (std::strcmp(stateMachine_->GetStateName(), "EnemyDeathState") == 0 ||
            std::strcmp(stateMachine_->GetStateName(), "EnemyStunState") == 0));
    if (!externallyInterrupted)
        EndPositioningAnimation();
    DisableAttackHitBoxes();
    dashBTPhase = DashBTPhase::None;
    dashBTTelegraphElapsed = 0.0f;
    dashBTTraveledDistance = 0.0f;
    dashBTTripleDashActive = false;
    dashBTDashIndex = 0;
    dashBTDashMaxCount = 1;
    dashBTCurrentDashHitStartCount = 0;
    dashBTAbortRemainingDashes = false;
    dashBTTransitionElapsed = 0.0f;
    dashBTTelegraphHoldDuration = 0.0f;
    dashBTDirectionLocked = false;
    dashBTMovementSnapshotPrepared = false;
    dashBTDirectionLockRequested = false;
    dashBTDashStartRequested = false;
    dashBTDirectionLockEventReceived = false;
    dashBTDashStartEventReceived = false;
    dashBTPredictedKnockupPosition = {};
    dashBTActualKnockupStartPosition = {};
    dashBTPredictionError = 0.0f;
    dashBTKnockupPositionCaptured = false;
    dashAttackDirection = {};
    dashAttackStartPosition = {};
    dashTargetPosition = {};
    currentDashAttackPlayerDistance = 0.0f;
    calculatedDashAttackDistance = 0.0f;
    selectedActionType = dashBTPreviousAction;
}
