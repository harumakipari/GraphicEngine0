#include "pch.h"
#include "GruxJumpAttackBT.h"
#include "GruxDashAttackBT.h"
#include "NodeBase.h"
#include "Engine/Scene/Scene.h"
#include "Game/Scenes/GameScene.h"
#include "Game/Actors/Player/Player.h"

bool GruxEnemy::IsTripleJumpPhase2() const
{
    const auto scene = dynamic_cast<GameScene*>(GetOwnerScene());
    return scene && scene->IsBossInFinalPhase();
}

void GruxEnemy::EnsureJumpTelegraphMesh()
{
    if (!jumpTelegraphMeshComponent)
        jumpTelegraphMeshComponent = AddComponent<StaticMeshComponent>("jumpTelegraphCircle", "GruxEnemy");
    if (!jumpTelegraphInnerMeshComponent)
        jumpTelegraphInnerMeshComponent = AddComponent<StaticMeshComponent>("jumpTelegraphInnerCircle", "GruxEnemy");
    const auto configureMesh = [](const std::shared_ptr<StaticMeshComponent>& mesh)
    {
        if (!mesh->model)
            mesh->SetModel("./Data/Models/EffectModel/JumpTeregraphPlane.glb");
        if (mesh->model)
            for (auto& material : mesh->model->materials)
                material.data.alphaMode = 2; // BLEND (mask alpha)
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
    configureMesh(jumpTelegraphMeshComponent);
    configureMesh(jumpTelegraphInnerMeshComponent);
}

void GruxEnemy::ShowJumpTelegraphForCurrentJump()
{
    EnsureJumpTelegraphMesh();
    if (!jumpTelegraphMeshComponent)
        return;
    const DirectX::XMFLOAT3 landing = tripleJumpActive
        ? tripleJumpPlannedLandingPosition
        : DirectX::XMFLOAT3{
            GetPosition().x + jumpMotionWarpDirection.x * calculatedJumpDistance,
            GetPosition().y,
            GetPosition().z + jumpMotionWarpDirection.z * calculatedJumpDistance };
    if (!tripleJumpActive)
    {
        tripleJumpPlayerPosition = jumpAttackStartPlayerPosition;
        tripleJumpStartPosition = GetPosition();
        tripleJumpLockedDirection = jumpMotionWarpDirection;
        tripleJumpPlannedDistance = calculatedJumpDistance;
        tripleJumpPlannedLandingPosition = landing;
    }
    // Take one world-space snapshot per jump. With absolute location enabled,
    // relativeLocation_ is interpreted as world space and is deliberately not
    // recomputed while MotionWarp moves Grux.
    jumpTelegraphWorldPosition = { landing.x, 0.350f, landing.z };
    if (showJumpLandingDebug)
    {
        jumpLandingDebugSnapshotValid = true;
        jumpLandingDebugMotionWarpEndCaptured = false;
        jumpLandingDebugGroundImpactCaptured = false;
        jumpLandingDebugWallHit = false;
        jumpLandingDebugPlayerDistance = currentJumpPlayerDistance;
        jumpLandingDebugRequestedDistance = currentJumpPlayerDistance - desiredAttackDistance;
        jumpLandingDebugCalculatedDistance = calculatedJumpDistance;
        jumpLandingDebugTargetErrorXZ = 0.0f;
        jumpLandingDebugPlayerSnapshot = tripleJumpActive
            ? tripleJumpPlayerPosition : jumpAttackStartPlayerPosition;
        jumpLandingDebugPlannedLanding = landing;
        jumpLandingDebugTelegraphCenter = jumpTelegraphWorldPosition;
        jumpLandingDebugMotionWarpEndBossRoot = {};
        jumpLandingDebugGroundImpactBossRoot = {};
        jumpLandingDebugGroundImpactWeaponRoot = {};
        jumpLandingDebugGroundImpactWeaponMiddle = {};
        jumpLandingDebugGroundImpactWeaponTip = {};
        jumpLandingDebugWallCollisionPosition = {};
    }
    jumpTelegraphMeshComponent->SetRelativeLocationDirect(jumpTelegraphWorldPosition);
    jumpTelegraphMeshComponent->SetRelativeRotationDirect({ 0.0f, 0.0f, 0.0f, 1.0f });
    jumpTelegraphMeshComponent->SetRelativeScaleDirect({
        jumpTelegraphScale, jumpTelegraphScale, jumpTelegraphScale });
    jumpTelegraphMeshComponent->SetIsVisible(true);

    jumpTelegraphProgress = 0.0f;
    jumpTelegraphCurrentAnimationTime = 0.0f;
    jumpTelegraphInnerCurrentScale = jumpTelegraphInnerStartScale;
    jumpTelegraphInnerMeshComponent->SetRelativeLocationDirect(
        { jumpTelegraphWorldPosition.x, 0.355f, jumpTelegraphWorldPosition.z });
    jumpTelegraphInnerMeshComponent->SetRelativeRotationDirect({ 0.0f, 0.0f, 0.0f, 1.0f });
    jumpTelegraphInnerMeshComponent->SetRelativeScaleDirect({
        jumpTelegraphInnerCurrentScale, jumpTelegraphInnerCurrentScale,
        jumpTelegraphInnerCurrentScale });
    jumpTelegraphOuterRotation = 0.0f;
    jumpTelegraphInnerRotation = 0.0f;
    jumpTelegraphInnerScaleProgress = 0.0f;
    jumpTelegraphMeshComponent->SetRelativeRotationDirect({ 0.0f, 0.0f, 0.0f, 1.0f });
    jumpTelegraphInnerMeshComponent->SetRelativeRotationDirect({ 0.0f, 0.0f, 0.0f, 1.0f });
    jumpTelegraphInnerMeshComponent->SetIsVisible(true);
    PlayBossTelegraphSE("boss_jump_telegraph.wav");
}

void GruxEnemy::DrawJumpLandingDebugWorld() const
{
#ifdef USE_IMGUI
    if (!showJumpLandingDebug || !jumpLandingDebugSnapshotValid)
        return;

    constexpr DirectX::XMFLOAT4 playerColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    constexpr DirectX::XMFLOAT4 plannedColor{ 0.15f, 1.0f, 0.15f, 1.0f };
    constexpr DirectX::XMFLOAT4 telegraphColor{ 1.0f, 0.1f, 0.1f, 1.0f };
    constexpr DirectX::XMFLOAT4 impactColor{ 0.15f, 0.35f, 1.0f, 1.0f };
    constexpr DirectX::XMFLOAT4 weaponSnapshotColor{ 1.0f, 0.95f, 0.1f, 1.0f };
    constexpr DirectX::XMFLOAT4 weaponSweepColor{ 1.0f, 0.25f, 0.85f, 1.0f };

    DebugRender::DrawSphere(jumpLandingDebugPlayerSnapshot, 0.16f, playerColor, 0.0f, true);
    DebugRender::DrawSphere(jumpLandingDebugPlannedLanding, 0.16f, plannedColor, 0.0f, true);
    DebugRender::DrawSphere(jumpLandingDebugTelegraphCenter, 0.16f, telegraphColor, 0.0f, true);
    if (jumpLandingDebugGroundImpactCaptured)
    {
        DebugRender::DrawSphere(jumpLandingDebugGroundImpactBossRoot, 0.18f, impactColor, 0.0f, true);
        DebugRender::DrawLine(jumpLandingDebugPlannedLanding,
            jumpLandingDebugGroundImpactBossRoot, impactColor, 0.0f, true);
        DebugRender::DrawSphere(jumpLandingDebugGroundImpactWeaponRoot, 0.12f,
            weaponSnapshotColor, 0.0f, true);
        DebugRender::DrawSphere(jumpLandingDebugGroundImpactWeaponMiddle, 0.12f,
            weaponSnapshotColor, 0.0f, true);
        DebugRender::DrawSphere(jumpLandingDebugGroundImpactWeaponTip, 0.12f,
            weaponSnapshotColor, 0.0f, true);
        DebugRender::DrawLine(jumpLandingDebugGroundImpactWeaponRoot,
            jumpLandingDebugGroundImpactWeaponMiddle, weaponSnapshotColor, 0.0f, true);
        DebugRender::DrawLine(jumpLandingDebugGroundImpactWeaponMiddle,
            jumpLandingDebugGroundImpactWeaponTip, weaponSnapshotColor, 0.0f, true);
    }

    const auto controller = GetBodyAnimationController();
    if (rightHitBox && controller &&
        controller->GetCurrentAnimationName() == "PrimaryAttack_JumpAttack")
    {
        const WeaponHitBoxPoints current = BuildWeaponHitBoxPoints(
            weaponRightRootComponent, weaponRightMiddleComponent, weaponRightTipComponent,
            activeRightHitBoxOffset);
        const WeaponHitBoxPoints previous{ prevWeaponRightRootPos, prevWeaponRightMidPos,
            prevWeaponRightTipPos };
        const float radius = activeRightHitBoxRadius;
        DebugRender::DrawLine(previous.root, current.root, weaponSweepColor, 0.0f, true);
        DebugRender::DrawLine(previous.middle, current.middle, weaponSweepColor, 0.0f, true);
        DebugRender::DrawLine(previous.tip, current.tip, weaponSweepColor, 0.0f, true);
        DebugRender::DrawSphere(current.root, radius, weaponSweepColor, 0.0f, true);
        DebugRender::DrawSphere(current.middle, radius, weaponSweepColor, 0.0f, true);
        DebugRender::DrawSphere(current.tip, radius, weaponSweepColor, 0.0f, true);
    }
#endif
}

void GruxEnemy::UpdateJumpTelegraphProgress()
{
    if (jumpTelegraphImpactFlashActive || !jumpTelegraphInnerMeshComponent ||
        !jumpTelegraphInnerMeshComponent->IsVisible())
        return;
    const auto controller = GetBodyAnimationController();
    if (!controller)
        return;
    jumpTelegraphCurrentAnimationTime = controller->GetCurrentAnimationTime();
    jumpTelegraphProgress = std::clamp(
        jumpTelegraphCurrentAnimationTime / jumpTelegraphLandingTime, 0.0f, 1.0f);
    jumpTelegraphInnerScaleProgress = jumpTelegraphInnerScaleCurve == 1
        ? jumpTelegraphProgress * jumpTelegraphProgress
        : jumpTelegraphProgress;
    jumpTelegraphInnerCurrentScale = std::lerp(
        jumpTelegraphInnerStartScale, jumpTelegraphScale, jumpTelegraphInnerScaleProgress);
    if (jumpTelegraphEnableRotation)
    {
        jumpTelegraphOuterRotation = jumpTelegraphCurrentAnimationTime * jumpTelegraphOuterRotationSpeed;
        jumpTelegraphInnerRotation = jumpTelegraphCurrentAnimationTime * jumpTelegraphInnerRotationSpeed;
        const auto toRotation = [](float degrees)
        {
            return DirectX::XMQuaternionRotationRollPitchYaw(
                0.0f, DirectX::XMConvertToRadians(degrees), 0.0f);
        };
        DirectX::XMFLOAT4 outerRotation{}, innerRotation{};
        DirectX::XMStoreFloat4(&outerRotation, toRotation(jumpTelegraphOuterRotation));
        DirectX::XMStoreFloat4(&innerRotation, toRotation(jumpTelegraphInnerRotation));
        jumpTelegraphMeshComponent->SetRelativeRotationDirect(outerRotation);
        jumpTelegraphInnerMeshComponent->SetRelativeRotationDirect(innerRotation);
    }
    else
    {
        jumpTelegraphOuterRotation = 0.0f;
        jumpTelegraphInnerRotation = 0.0f;
        jumpTelegraphMeshComponent->SetRelativeRotationDirect({ 0.0f, 0.0f, 0.0f, 1.0f });
        jumpTelegraphInnerMeshComponent->SetRelativeRotationDirect({ 0.0f, 0.0f, 0.0f, 1.0f });
    }
    jumpTelegraphMeshComponent->SetRelativeScaleDirect({
        jumpTelegraphScale, jumpTelegraphScale, jumpTelegraphScale });
    jumpTelegraphInnerMeshComponent->SetRelativeScaleDirect({
        jumpTelegraphInnerCurrentScale, jumpTelegraphInnerCurrentScale,
        jumpTelegraphInnerCurrentScale });
}

void GruxEnemy::BeginJumpTelegraphImpactFlash()
{
    if (!jumpTelegraphEnableImpactFlash || !jumpTelegraphMeshComponent ||
        !jumpTelegraphInnerMeshComponent || !jumpTelegraphMeshComponent->IsVisible())
        return;
    jumpTelegraphImpactFlashActive = true;
    jumpTelegraphImpactFlashElapsed = 0.0f;
    jumpTelegraphImpactFlashProgress = 0.0f;
    jumpTelegraphInnerCurrentScale = jumpTelegraphScale;
    jumpTelegraphInnerScaleProgress = 1.0f;
    jumpTelegraphInnerMeshComponent->SetRelativeScaleDirect({
        jumpTelegraphScale, jumpTelegraphScale, jumpTelegraphScale });
}

void GruxEnemy::UpdateJumpTelegraphImpactFlash(float deltaTime)
{
    if (!jumpTelegraphImpactFlashActive)
        return;
    const float duration = (std::max)(0.001f, jumpTelegraphImpactFlashDuration);
    jumpTelegraphImpactFlashElapsed += (std::max)(0.0f, deltaTime);
    jumpTelegraphImpactFlashProgress = std::clamp(
        jumpTelegraphImpactFlashElapsed / duration, 0.0f, 1.0f);
    const float remaining = 1.0f - jumpTelegraphImpactFlashProgress;
    const float flash = jumpTelegraphEnableImpactFlash
        ? jumpTelegraphImpactFlashIntensity * remaining : 0.0f;
    const float alphaFade = remaining;
    for (const auto& mesh : { jumpTelegraphMeshComponent, jumpTelegraphInnerMeshComponent })
    {
        if (!mesh || !mesh->plusAlphaCBuffer)
            continue;
        mesh->plusAlphaCBuffer->data.flashValue = flash;
        // The shared telegraph shader uses modelBrightness as a per-component
        // alpha multiplier; Charge Telegraph never writes this value.
        mesh->plusAlphaCBuffer->data.brightness = alphaFade - 1.0f;
    }
    if (jumpTelegraphImpactFlashProgress >= 1.0f)
        HideJumpTelegraph();
}

void GruxEnemy::HideJumpTelegraph()
{
    jumpTelegraphImpactFlashActive = false;
    jumpTelegraphImpactFlashElapsed = 0.0f;
    jumpTelegraphImpactFlashProgress = 0.0f;
    if (jumpTelegraphMeshComponent)
    {
        jumpTelegraphMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
        jumpTelegraphMeshComponent->plusAlphaCBuffer->data.brightness = 0.0f;
        jumpTelegraphMeshComponent->SetIsVisible(false);
    }
    if (jumpTelegraphInnerMeshComponent)
    {
        jumpTelegraphInnerMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
        jumpTelegraphInnerMeshComponent->plusAlphaCBuffer->data.brightness = 0.0f;
        jumpTelegraphInnerMeshComponent->SetIsVisible(false);
    }
}

void GruxEnemy::BeginTripleJumpRuntime()
{
    tripleJumpActive = true;
    tripleJumpIndex = 0;
    tripleJumpPhase = TripleJumpPhase::InitialTelegraph;
    tripleJumpTransitionElapsed = 0.0f;
    tripleJumpTargetLockActive = false;
    tripleJumpPlayerPosition = {};
    tripleJumpStartPosition = {};
    tripleJumpLockedDirection = {};
    tripleJumpPlannedDistance = 0.0f;
    tripleJumpPlannedLandingPosition = {};
}

bool GruxEnemy::StartTripleJumpJump()
{
    if (!tripleJumpActive || tripleJumpIndex < 0 || tripleJumpIndex >= 3)
        return false;
    const auto context = BuildTargetContext();
    if (!context.valid)
        return false;
    const auto scene = GetOwnerScene();
    const auto player = scene ? scene->GetActorManager()->GetActorOfType<Player>() : nullptr;
    if (!player)
        return false;
    tripleJumpPlayerPosition = player->GetPosition();
    tripleJumpStartPosition = GetPosition();
    tripleJumpLockedDirection = context.directionToPlayer;
    tripleJumpLockedDirection.y = 0.0f;
    const float directionLength = std::sqrt(
        tripleJumpLockedDirection.x * tripleJumpLockedDirection.x +
        tripleJumpLockedDirection.z * tripleJumpLockedDirection.z);
    if (directionLength <= FLT_EPSILON)
        return false;
    tripleJumpLockedDirection.x /= directionLength;
    tripleJumpLockedDirection.z /= directionLength;
    tripleJumpPlannedDistance = std::clamp(
        context.xzDistance - desiredAttackDistance, 0.0f, maxJumpDistance);
    tripleJumpPlannedLandingPosition = tripleJumpStartPosition;
    tripleJumpPlannedLandingPosition.x += tripleJumpLockedDirection.x * tripleJumpPlannedDistance;
    tripleJumpPlannedLandingPosition.z += tripleJumpLockedDirection.z * tripleJumpPlannedDistance;
    ShowJumpTelegraphForCurrentJump();
    tripleJumpTargetLockActive = true;
    tripleJumpPhase = TripleJumpPhase::Jumping;
    if (tripleJumpIndex > 0)
        BeginAdditionalAttackStage();
    if (!PlayAttackStage(BossAttackType::JumpAttack, 1))
    {
        tripleJumpTargetLockActive = false;
        return false;
    }
    // PlayAttackStage prepares the normal single-jump warp; replace only its
    // sampled values with this stage's locked target before Notify MotionWarp.
    jumpAttackStartPlayerPosition = tripleJumpPlayerPosition;
    currentJumpPlayerDistance = context.xzDistance;
    calculatedJumpDistance = tripleJumpPlannedDistance;
    jumpMotionWarpDirection = tripleJumpLockedDirection;
    if (rotationComponent)
        rotationComponent->SetDirectionImmediate(tripleJumpLockedDirection);
    tripleJumpTargetLockActive = false;
    if (tripleJumpIndex == 0)
        OnSelectedActionStartedSuccessfully();
    return true;
}

bool GruxEnemy::UpdateTripleJumpTransition(float deltaTime)
{
    if (!tripleJumpActive || tripleJumpPhase != TripleJumpPhase::InterJumpTransition)
        return false;
    StopAIMovement();
    const auto context = BuildTargetContext();
    if (!context.valid)
    {
        AbortTripleJumpRuntime();
        return true;
    }
    RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), deltaTime,
        "BT_TripleJumpInterTransition");
    tripleJumpTransitionElapsed += (std::max)(0.0f, deltaTime);
    if (tripleJumpTransitionElapsed < tripleJumpTransitionDuration)
        return false;
    ++tripleJumpIndex;
    return true;
}

void GruxEnemy::BeginTripleJumpTransition()
{
    if (!tripleJumpActive)
        return;
    tripleJumpPhase = TripleJumpPhase::InterJumpTransition;
    tripleJumpTransitionElapsed = 0.0f;
}

void GruxEnemy::CompleteTripleJumpRuntime()
{
    tripleJumpActive = false;
    tripleJumpPhase = TripleJumpPhase::Completed;
    tripleJumpTargetLockActive = false;
}

void GruxEnemy::AbortTripleJumpRuntime()
{
    tripleJumpActive = false;
    tripleJumpPhase = TripleJumpPhase::Aborted;
    tripleJumpTargetLockActive = false;
    tripleJumpTransitionElapsed = 0.0f;
}

void GruxEnemy::DrawTripleJumpDebug()
{
#ifdef USE_IMGUI
    ImGui::SeparatorText("Triple Jump Settings");
    ImGui::DragFloat("Jump Telegraph Scale", &jumpTelegraphScale,
        0.01f, 0.01f, 10.0f, "%.2f");
    jumpTelegraphScale = (std::max)(0.01f, jumpTelegraphScale);
    ImGui::DragFloat("Inner Start Scale", &jumpTelegraphInnerStartScale,
        0.01f, 0.01f, 10.0f, "%.2f");
    jumpTelegraphInnerStartScale = (std::max)(0.01f, jumpTelegraphInnerStartScale);
    ImGui::Combo("Inner Scale Curve", &jumpTelegraphInnerScaleCurve,
        "Linear\0EaseIn\0\0");
    ImGui::Checkbox("Enable Rotation", &jumpTelegraphEnableRotation);
    ImGui::DragFloat("Outer Rotation Speed", &jumpTelegraphOuterRotationSpeed,
        1.0f, -360.0f, 360.0f, "%.1f deg/sec");
    ImGui::DragFloat("Inner Rotation Speed", &jumpTelegraphInnerRotationSpeed,
        1.0f, -360.0f, 360.0f, "%.1f deg/sec");
    ImGui::Checkbox("Enable Impact Flash", &jumpTelegraphEnableImpactFlash);
    ImGui::DragFloat("Impact Flash Duration", &jumpTelegraphImpactFlashDuration,
        0.01f, 0.01f, 1.0f, "%.2f sec");
    ImGui::DragFloat("Impact Flash Intensity", &jumpTelegraphImpactFlashIntensity,
        0.05f, 0.0f, 2.0f, "%.2f");
    jumpTelegraphImpactFlashDuration = (std::max)(0.001f, jumpTelegraphImpactFlashDuration);
    jumpTelegraphImpactFlashIntensity = (std::max)(0.0f, jumpTelegraphImpactFlashIntensity);
    ImGui::DragFloat("Jump Telegraph Ground Offset",&jumpTelegraphOffset,0.01f);
    ImGui::DragFloat("Inter Jump Transition Duration", &tripleJumpTransitionDuration,
        0.01f, 0.0f, 1.0f, "%.2f sec");
    tripleJumpTransitionDuration = (std::max)(0.0f, tripleJumpTransitionDuration);
    if (!tripleJumpActive && tripleJumpPhase == TripleJumpPhase::None &&
        !(jumpTelegraphMeshComponent && jumpTelegraphMeshComponent->IsVisible()))
        return;
    const char* phaseName = "None";
    switch (tripleJumpPhase)
    {
    case TripleJumpPhase::InitialTelegraph: phaseName = "InitialTelegraph"; break;
    case TripleJumpPhase::Jumping: phaseName = "Jumping"; break;
    case TripleJumpPhase::InterJumpTransition: phaseName = "InterJumpTransition"; break;
    case TripleJumpPhase::Completed: phaseName = "Completed"; break;
    case TripleJumpPhase::Aborted: phaseName = "Aborted"; break;
    default: break;
    }
    ImGui::SeparatorText("Triple Jump Runtime Debug");
    ImGui::Text("Triple Jump Active: %s", tripleJumpActive ? "true" : "false");
    ImGui::Text("Triple Jump Index: %d", tripleJumpIndex);
    ImGui::Text("Triple Jump Phase: %s", phaseName);
    ImGui::Text("Telegraph Visible: %s", jumpTelegraphMeshComponent &&
        jumpTelegraphMeshComponent->IsVisible() ? "true" : "false");
    const auto telegraphPosition = jumpTelegraphMeshComponent
        ? jumpTelegraphMeshComponent->GetComponentLocation() : DirectX::XMFLOAT3{};
    ImGui::Text("Telegraph Position: (%.3f, %.3f, %.3f)", telegraphPosition.x,
        telegraphPosition.y, telegraphPosition.z);
    ImGui::Text("Telegraph World Snapshot: (%.3f, %.3f, %.3f)",
        jumpTelegraphWorldPosition.x, jumpTelegraphWorldPosition.y,
        jumpTelegraphWorldPosition.z);
    ImGui::Text("Jump Telegraph Progress: %.3f", jumpTelegraphProgress);
    ImGui::Text("Scale Progress: %.3f", jumpTelegraphInnerScaleProgress);
    ImGui::Text("Inner Scale Curve: %s", jumpTelegraphInnerScaleCurve == 1 ? "EaseIn" : "Linear");
    ImGui::Text("Current Animation Time: %.6f sec", jumpTelegraphCurrentAnimationTime);
    ImGui::Text("Landing Time: %.6f sec", jumpTelegraphLandingTime);
    ImGui::Text("Outer Scale: %.3f", jumpTelegraphScale);
    ImGui::Text("Current Inner Scale: %.3f", jumpTelegraphInnerCurrentScale);
    ImGui::Text("Impact Flash Active: %s", jumpTelegraphImpactFlashActive ? "true" : "false");
    ImGui::Text("Impact Flash Progress: %.3f", jumpTelegraphImpactFlashProgress);
    ImGui::Text("Impact Flash Elapsed: %.3f sec", jumpTelegraphImpactFlashElapsed);
    ImGui::Text("Outer Rotation: %.3f deg", jumpTelegraphOuterRotation);
    ImGui::Text("Inner Rotation: %.3f deg", jumpTelegraphInnerRotation);
    ImGui::Text("Inter Jump Transition Elapsed: %.3f sec", tripleJumpTransitionElapsed);
    ImGui::Text("Player Position: (%.3f, %.3f, %.3f)", tripleJumpPlayerPosition.x,
        tripleJumpPlayerPosition.y, tripleJumpPlayerPosition.z);
    ImGui::Text("Jump Start Position: (%.3f, %.3f, %.3f)", tripleJumpStartPosition.x,
        tripleJumpStartPosition.y, tripleJumpStartPosition.z);
    ImGui::Text("Locked Direction: (%.3f, %.3f, %.3f)", tripleJumpLockedDirection.x,
        tripleJumpLockedDirection.y, tripleJumpLockedDirection.z);
    ImGui::Text("Planned Jump Distance: %.3f", tripleJumpPlannedDistance);
    ImGui::Text("Planned Landing Position: (%.3f, %.3f, %.3f)",
        tripleJumpPlannedLandingPosition.x, tripleJumpPlannedLandingPosition.y,
        tripleJumpPlannedLandingPosition.z);
#endif
}

bool CanPlanJumpAttack::Judgment()
{
    const bool result = owner->CanPlanJumpAttack(); owner->SetBehaviorTreeLastJudgment(result ? "CanPlanJumpAttack: true" : "CanPlanJumpAttack: false");
    return result;
}

bool CanPlanAnyAttack::Judgment()
{
    if (owner->AreAttackBehaviorsDisabledForDebug())
    {
        owner->SetBehaviorTreeLastJudgment("CanPlanAnyAttack: false (debug disabled)");
        return false;
    }
    if (!owner->CanPlanAttackAgainstCurrentPlayer())
    {
        owner->SetBehaviorTreeLastJudgment("CanPlanAnyAttack: false (player unavailable)");
        return false;
    }
    // In Force BehaviorTree Charge mode, keep the normal Attack gate but let
    // the Charge plan be the sole plan selected by AttackRandom. The plan's
    // internal nodes are still executed normally.
    const bool result = owner->IsForceBehaviorTreeChargeEnabled()
        ? owner->CanPlanChargeAttack()
        : owner->CanPlanFastCombo() || owner->CanPlanJumpAttack() ||
            DashPlanAvailable(owner).Judgment() || owner->CanPlanChargeAttack();
    owner->SetBehaviorTreeLastJudgment(result ? "CanPlanAnyAttack: true" : "CanPlanAnyAttack: false");
    return result;
}

ActionBase::State PrepareJumpSetupTarget::Run(float)
{
    owner->SetSelectedAttackForBehaviorTree(BossAttackType::JumpAttack);
    return owner->PrepareJumpAttackSetupTarget() ? State::Complete : State::Failed;
}

ActionBase::State MoveToAttackSetupTarget::Run(float dt)
{
    if (!owner->HasAttackSetupTarget())
        return State::Failed;

    const auto result = owner->UpdateAttackSetupMovement(dt);

    if (result == GruxEnemy::AttackSetupMoveResult::Running)
        return State::Run;

    owner->StopAttackSetupMovement();

    if (result == GruxEnemy::AttackSetupMoveResult::Arrived)
        return State::Complete;

    owner->ClearAttackSetupTarget();
    return State::Failed;
}

ActionBase::State FaceJumpPlayerIfNeeded::Run(float dt)
{
    const auto context = owner->BuildTargetContext();
    if (!context.valid)
        return State::Failed;
    if (context.absoluteAngleDegrees <= owner->GetTurnCompleteAngle())
        return State::Complete;
    owner->RotateTowardsPlayer(context.directionToPlayer, owner->GetTurnSpeed(), dt, "BT_JumpFacePlayer");
    return State::Run;
}

bool CanExecuteJumpAttack::Judgment()
{
    const auto context = owner->BuildTargetContext();
    const bool result = context.valid && !owner->IsDead();
    owner->SetBehaviorTreeLastJudgment(result ? "CanExecuteJumpAttack: true" : "CanExecuteJumpAttack: false");
    return result;
}

ActionBase::State StartJumpAttack::Run(float)
{
    if (!started)
    {
        owner->SetSelectedAttackForBehaviorTree(BossAttackType::JumpAttack);
        if (!owner->StartJumpAttackTelegraph())
        {
            started = false;
            return State::Failed;
        }
        if (owner->IsTripleJumpPhase2())
            owner->BeginTripleJumpRuntime();
        // Match the former EnemyAttackState path: notify at telegraph start.
        owner->RequestJumpAttackCameraAssist();
        started = true;
    }
    started = false;
    return State::Complete;
}

ActionBase::State ExecuteJumpAttack::RunTripleJump(float dt)
{
    if (!started)
    {
        started = true;
        executionStarted = false;
        finishAfterAnimation = false;
        stageHitCount = owner->GetCurrentAttackHitCount();
    }
    const auto controller = owner->GetBodyAnimationController();
    if (!controller || owner->IsDead())
    {
        owner->AbortTripleJumpRuntime();
        owner->ClearJumpAttackMotionWarpOverride();
        owner->HideJumpTelegraph();
        started = false;
        return State::Complete;
    }
    if (finishAfterAnimation)
    {
        if (controller && controller->IsPlayAnimation())
            return State::Run;
        owner->ClearJumpAttackMotionWarpOverride();
        owner->HideJumpTelegraph();
        owner->CompleteTripleJumpRuntime();
        owner->StartSelectedActionCooldown();
        started = false;
        return State::Complete;
    }

    if (!executionStarted && owner->GetTripleJumpPhase() == GruxEnemy::TripleJumpPhase::InitialTelegraph)
    {
        if (controller && controller->IsPlayAnimation())
        {
            owner->UpdateJumpAttackTelegraph(dt);
            return State::Run;
        }
        if (!owner->StartTripleJumpJump())
        {
            owner->AbortTripleJumpRuntime();
            owner->ClearJumpAttackMotionWarpOverride();
            owner->HideJumpTelegraph();
            started = false;
            return State::Failed;
        }
        executionStarted = true;
        stageHitCount = owner->GetCurrentAttackHitCount();
        return State::Run;
    }

    if (owner->GetTripleJumpPhase() == GruxEnemy::TripleJumpPhase::InterJumpTransition)
    {
        if (!owner->UpdateTripleJumpTransition(dt))
            return State::Run;
        if (!owner->IsTripleJumpActive())
        {
            owner->ClearJumpAttackMotionWarpOverride();
            owner->HideJumpTelegraph();
            started = false;
            return State::Complete;
        }
        if (!owner->StartTripleJumpJump())
        {
            owner->AbortTripleJumpRuntime();
            owner->ClearJumpAttackMotionWarpOverride();
            owner->HideJumpTelegraph();
            started = false;
            return State::Failed;
        }
        executionStarted = true;
        stageHitCount = owner->GetCurrentAttackHitCount();
        return State::Run;
    }

    if (owner->GetTripleJumpPhase() != GruxEnemy::TripleJumpPhase::Jumping)
        return State::Run;
    owner->UpdateJumpTelegraphProgress();
    if (owner->GetCurrentAttackHitCount() > stageHitCount ||
        owner->WasCurrentAttackSequenceJustDodged())
    {
        const bool dodged = owner->WasCurrentAttackSequenceJustDodged();
        owner->OnSelectedAttackCompletedSuccessfully();
        owner->SetBehaviorAttackResult(dodged ? GruxEnemy::BehaviorAttackResult::JustDodged : GruxEnemy::BehaviorAttackResult::Success);
        owner->DisableAttackHitBoxes();
        owner->HideJumpTelegraph();
        owner->CompleteTripleJumpRuntime();
        finishAfterAnimation = true;
        return State::Run;
    }
    if (controller && controller->IsPlayAnimation())
        return State::Run;

    if (owner->GetTripleJumpIndex() < 2)
    {
        owner->DisableAttackHitBoxes();
        owner->ClearJumpAttackMotionWarpOverride();
        owner->HideJumpTelegraph();
        owner->BeginTripleJumpTransition();
        executionStarted = false;
        return State::Run;
    }
    owner->OnSelectedAttackCompletedSuccessfully();
    owner->SetBehaviorAttackResult(GruxEnemy::BehaviorAttackResult::Success);
    owner->ClearJumpAttackMotionWarpOverride();
    owner->HideJumpTelegraph();
    owner->CompleteTripleJumpRuntime();
    owner->StartSelectedActionCooldown();
    started = false;
    return State::Complete;
}

ActionBase::State ExecuteJumpAttack::Run(float dt)
{
    if (owner->IsTripleJumpPhase2())
        return RunTripleJump(dt);
    if (!started)
    {
        started = true;
        executionStarted = false;
        finishAfterAnimation = false;
        stageHitCount = owner->GetCurrentAttackHitCount();
    }

    const auto controller = owner->GetBodyAnimationController();
    if (finishAfterAnimation)
    {
        if (controller && controller->IsPlayAnimation())
            return State::Run;
        // The jump animation and its MotionWarp have finished. Clear before
        // this action completes so the following recovery/root cannot inherit it.
        owner->ClearJumpAttackMotionWarpOverride();
        owner->HideJumpTelegraph();
        owner->StartSelectedActionCooldown();
        started = false;
        return State::Complete;
    }

    if (!executionStarted)
    {
        if (owner->WasCurrentAttackSequenceJustDodged() || owner->GetCurrentAttackHitCount() > stageHitCount)
        {
            const bool dodged = owner->WasCurrentAttackSequenceJustDodged();
            owner->OnSelectedAttackCompletedSuccessfully();
            owner->SetBehaviorAttackResult(dodged ? GruxEnemy::BehaviorAttackResult::JustDodged : GruxEnemy::BehaviorAttackResult::Success);
            owner->DisableAttackHitBoxes();
            owner->HideJumpTelegraph();
            finishAfterAnimation = true;
            return State::Run;
        }
        if (!owner->UpdateJumpAttackTelegraph(dt))
            return State::Run;
        if (!owner->StartJumpAttackExecution())
        {
            owner->ClearJumpAttackMotionWarpOverride();
            owner->HideJumpTelegraph();
            started = false;
            executionStarted = false;
            return State::Failed;
        }
        owner->ShowJumpTelegraphForCurrentJump();
        executionStarted = true;
        stageHitCount = owner->GetCurrentAttackHitCount();
        return State::Run;
    }

    if (owner->GetCurrentAttackHitCount() > stageHitCount || owner->WasCurrentAttackSequenceJustDodged())
    {
        const bool dodged = owner->WasCurrentAttackSequenceJustDodged();
        owner->OnSelectedAttackCompletedSuccessfully();
        owner->SetBehaviorAttackResult(dodged ? GruxEnemy::BehaviorAttackResult::JustDodged : GruxEnemy::BehaviorAttackResult::Success);
        owner->DisableAttackHitBoxes();
        owner->HideJumpTelegraph();
        finishAfterAnimation = true;
        return State::Run;
    }
    owner->UpdateJumpTelegraphProgress();
    if (controller && controller->IsPlayAnimation())
        return State::Run;

    // A controller disappearing or an animation interruption also reaches this
    // terminal path; both must release the Jump-only override.
    owner->OnSelectedAttackCompletedSuccessfully();
    owner->SetBehaviorAttackResult(GruxEnemy::BehaviorAttackResult::Success);
    owner->ClearJumpAttackMotionWarpOverride();
    owner->HideJumpTelegraph();
    owner->StartSelectedActionCooldown();
    started = false;
    return State::Complete;
}

void ExecuteJumpAttack::ResetRuntime()
{
    // BehaviorTree::ResetActionRuntimes is the BT abort path. This clear is
    // idempotent, so resetting an inactive Jump action is harmless.
    owner->ClearJumpAttackMotionWarpOverride();
    owner->HideJumpTelegraph();
    if (owner->IsTripleJumpActive())
        owner->AbortTripleJumpRuntime();
    started = false;
    executionStarted = false;
    finishAfterAnimation = false;
    stageHitCount = 0;
}
