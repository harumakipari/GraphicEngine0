#include "pch.h"
#include "GruxRoarBT.h"
#include "NodeBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Actors/Player/Player.h"
#include "Game/State/StateMachine.h"
#include "Engine/Scene/Scene.h"
#include "Core/ActorManager.h"

namespace
{
    bool IsOtherBTActionForRoar(NodeBase* node)
    {
        // Idle can advertise eligibility, but the scheduler still waits for its
        // Complete before inferring Root. Active attacks/recovery remain blocked.
        return node && node->GetName() != "Idle" && node->GetName() != "StartRoar";
    }
}

bool CanPlanRoar::Judgment() { return owner->CanPlanRoar(); }
ActionBase::State StartRoar::Run(float)
{
    return owner->BeginRoarBT() ? State::Complete : State::Failed;
}
ActionBase::State ExecuteRoar::Run(float dt)
{
    const int result = owner->UpdateRoarBT(dt);
    return result < 0 ? State::Failed : result > 0 ? State::Complete : State::Run;
}
ActionBase::State FinishRoar::Run(float)
{
    const bool complete = owner->IsRoarBTActive();
    owner->CleanupRoarBT(complete ? "Complete" : "Failed");
    return complete ? State::Complete : State::Failed;
}

bool GruxEnemy::IsRoarExecutionAllowed() const
{
    if (!behaviorTreeFastComboEnabled || !battleAIActive || IsDead() || IsPendingKill() ||
        isDeathPerform ||
        finalHitReactionActive || finalHitReactionHeld || IsAnimationEditorPreviewActive() ||
        IsChargeAttackBTActive() || IsDashAttackBTActive() || chargeMovementActive ||
        dashAttackMovementActive || jumpMotionWarpOverrideActive || !characterMovementComponent ||
        !GetBodyAnimationController() || !GetOwnerScene())
        return false;
    if (stateMachine_)
    {
        const std::string state = stateMachine_->GetStateName();
        // BT mode suspends the legacy FSM; its last Attack state is not activity.
        // Death/Stun still serve as external interruption guards.
        if (state == "EnemyDeathState" || state == "EnemyStunState")
            return false;
    }
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    return player && !player->IsPendingKill() && player->GetHp() > 0;
}

bool GruxEnemy::CanPlanRoar() const
{
    const bool result = EvaluateRoarPlanForDebug();
    // Preserve the actual selection-time result; drawing ImGui must not replace it
    // with the later Idle/ExecuteRoar-time evaluation.
    roarPlanDebugEvaluated = true;
    roarPlanDebugLastResult = result;
    roarPlanDebugLastRejectReason = result ? U8("なし") : GetRoarRejectReasonForDebug();
    roarPlanDebugLastNode = activeNode ? activeNode->GetName() : "None (Root inference)";
    return result;
}

bool GruxEnemy::EvaluateRoarPlanForDebug() const
{
    if (!IsRoarExecutionAllowed() || IsRoarBTActive() || roarCooldownRemaining > 0.0f ||
        IsOtherBTActionForRoar(activeNode))
        return false;
    const auto context = BuildTargetContext();
    return context.valid && context.region == PlayerRelativeRegion::Back &&
        context.xzDistance <= defensiveTooCloseDistance;
}

std::string GruxEnemy::GetRoarRejectReasonForDebug() const
{
    // Same first-failure order as IsRoarExecutionAllowed and CanPlanRoar.
    // This function is diagnostic only; it never decides BT eligibility.
    if (!behaviorTreeFastComboEnabled) return U8("BTが無効");
    if (!battleAIActive) return U8("戦闘AIが無効");
    if (IsDead()) return U8("Gruxが死亡");
    if (IsPendingKill()) return U8("Gruxが削除待ち");
    if (isDeathPerform) return U8("死亡演出中");
    if (finalHitReactionActive) return U8("FinalHit演出中");
    if (finalHitReactionHeld) return U8("FinalHitの姿勢保持中");
    if (IsAnimationEditorPreviewActive()) return U8("Animation Preview中");
    if (IsChargeAttackBTActive()) return U8("Charge BT実行中");
    if (IsDashAttackBTActive()) return U8("Dash BT実行中");
    if (chargeMovementActive) return U8("Charge移動中");
    if (dashAttackMovementActive) return U8("Dash移動中");
    if (jumpMotionWarpOverrideActive) return U8("Jump MotionWarp実行中");
    if (!characterMovementComponent) return U8("移動Componentがない");
    if (!GetBodyAnimationController()) return U8("AnimationControllerがない");
    if (!GetOwnerScene()) return U8("Sceneがない");
    if (stateMachine_)
    {
        const std::string state = stateMachine_->GetStateName();
        if (state == "EnemyDeathState") return U8("旧Death State中");
        if (state == "EnemyStunState") return U8("旧Stun State中");
    }
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player) return U8("Playerが存在しない");
    if (player->IsPendingKill()) return U8("Playerが削除待ち");
    if (player->GetHp() <= 0) return U8("Playerが死亡");
    if (IsRoarBTActive()) return U8("Roar実行中");
    if (roarCooldownRemaining > 0.0f) return U8("Cooldown中");
    if (IsOtherBTActionForRoar(activeNode))
        return std::string(U8("他BT行動中: ")) + activeNode->GetName();
    const auto context = BuildTargetContext();
    if (!context.valid) return U8("TargetContextが無効");
    if (context.region != PlayerRelativeRegion::Back) return U8("背後ではない");
    if (!(context.xzDistance <= defensiveTooCloseDistance)) return U8("距離が遠い");
    return U8("なし");
}

bool GruxEnemy::BeginRoarBT()
{
    if (!CanPlanRoar()) { roarBTStatus = "Start rejected"; return false; }
    const auto controller = GetBodyAnimationController();
    if (!std::isfinite(roarPreStampedeStartTime) || !std::isfinite(roarPreStampedeEndTime) ||
        roarPreStampedeStartTime < 0.0f || roarPreStampedeEndTime <= roarPreStampedeStartTime ||
        controller->GetAnimationLength("Pre_Stampede_0") < roarPreStampedeEndTime)
    { roarBTStatus = "Invalid playback range"; return false; }
    StopAIMovement();
    DisableAttackHitBoxes();
    roarBT = {};
    PlayBodyAnimation("Pre_Stampede_0", false, false, 0.0f, true);
    if (!controller->SetPlaybackRange(roarPreStampedeStartTime, roarPreStampedeEndTime))
    {
        roarBT.stage = RoarStage::Telegraph;
        CleanupRoarBT("Invalid playback range");
        return false;
    }
    roarBT.stage = RoarStage::Telegraph;
    roarBT.previousTime = roarPreStampedeStartTime;
    roarBT.endTime = roarPreStampedeEndTime;
    roarBTStatus = "Running";
    roarCooldownRemaining = roarCooldownDuration;
    return true;
}

// 0 = Running, 1 = Complete, -1 = Failed. Only this action advances stages.
int GruxEnemy::UpdateRoarBT(float dt)
{
    auto fail = [this](const char* reason) { CleanupRoarBT(reason); return -1; };
    if (!IsRoarBTActive() || !IsRoarExecutionAllowed()) return fail("Interrupted");
    const auto controller = GetBodyAnimationController();
    const char* expected = roarBT.stage == RoarStage::Telegraph ? "Pre_Stampede_0" : "LevelStart_0";
    if (controller->GetCurrentAnimationName() != expected) return fail("Animation replaced");
    const float time = controller->GetCurrentAnimationTime();
    const float safeDt = (std::max)(0.0f, dt);
    roarBT.elapsed += safeDt;
    roarBT.stalled = time > roarBT.previousTime + 0.00001f ? 0.0f : roarBT.stalled + safeDt;
    roarBT.previousTime = time;
    if (roarBT.stalled > 2.0f || roarBT.elapsed > 60.0f) return fail("Playback timeout");
    StopAIMovement();
    if (controller->IsPlayAnimation()) return 0;
    // A stopped clip is only a successful completion at its expected end.
    if (time + 0.02f < roarBT.endTime) return fail("Animation stopped early");
    if (roarBT.stage == RoarStage::Telegraph)
    {
        PlayBodyAnimation("LevelStart_0", false, false, 0.0f, true);
        roarBT.stage = RoarStage::Shockwave;
        roarBT.previousTime = 0.0f;
        roarBT.stalled = 0.0f;
        roarBT.endTime = controller->GetAnimationLength("LevelStart_0");
        if (roarBT.endTime < 0.30f) return fail("Invalid shockwave clip");
        return 0;
    }
    if (!roarBT.shockwaveFired) return fail("Missing RoarShockwave notify");
    return 1;
}

void GruxEnemy::ApplyRoarShockwave()
{
    const auto controller = GetBodyAnimationController();
    if (roarBT.stage != RoarStage::Shockwave || roarBT.shockwaveFired ||
        !IsRoarExecutionAllowed() || !controller || controller->GetCurrentAnimationName() != "LevelStart_0")
        return;
    // Consume the event even on a miss: moving into the radius later must not hit.
    roarBT.shockwaveFired = true;
    SpawnGroundImpactEffect();
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player || roarBT.hitPlayer || !player->CanReceiveKnockBack()) return;
    auto direction = MathHelper::Subtract(player->GetPosition(), GetPosition());
    if (std::abs(direction.y) > roarHeightTolerance) return;
    direction.y = 0.0f;
    if (direction.x * direction.x + direction.z * direction.z > roarRadius * roarRadius) return;
    if (MathHelper::Length(direction) < 0.0001f)
    {
        direction = GetForward();
        direction.y = 0.0f;
    }
    // Damage is deliberately separate from the radial query and knockback response.
    roarBT.hitPlayer = player->StartKnockBack(direction);
}

void GruxEnemy::CleanupRoarBT(const char* status)
{
    if (IsRoarBTActive())
    {
        StopAIMovement();
        if (characterMovementComponent) characterMovementComponent->ResetFixedSpeed();
        DisableAttackHitBoxes();
        const auto controller = GetBodyAnimationController();
        // Do not overwrite death, stun, preview or externally owned animation.
        if (IsRoarExecutionAllowed() && controller &&
            (controller->GetCurrentAnimationName() == "Pre_Stampede_0" ||
             controller->GetCurrentAnimationName() == "LevelStart_0"))
            PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
    }
    roarBT = {};
    roarBTStatus = status;
    // Cooldown belongs to Defensive, not to the lifetime of this execution.
}

void GruxEnemy::TickRoarLifecycle(float dt)
{
    if (!IsAnimationEditorPreviewActive())
        roarCooldownRemaining = (std::max)(0.0f, roarCooldownRemaining - (std::max)(0.0f, dt));
    if (IsRoarBTActive() && (!IsRoarExecutionAllowed() || !activeNode ||
        (activeNode->GetName() != "ExecuteRoar" && activeNode->GetName() != "FinishRoar")))
    {
        CleanupRoarBT("Interrupted");
        activeNode = nullptr;
        if (behaviorData) behaviorData->Init();
    }
}

void GruxEnemy::DrawRoarBTDebug()
{
#ifdef USE_IMGUI
    ImGui::SeparatorText(U8("咆哮BT"));
    ImGui::Checkbox(U8("攻撃行動を無効化"), &disableAttackBehaviorsForDebug);
    const auto roarController = GetBodyAnimationController();
    const float clipDuration = roarController ? roarController->GetAnimationLength("Pre_Stampede_0") : 60.0f;
    const float rangeLimit = std::isfinite(clipDuration) ? (std::max)(0.05f, clipDuration) : 60.0f;
    ImGui::DragFloat(U8("咆哮 開始時間"), &roarPreStampedeStartTime, 0.05f, 0.0f, rangeLimit - 0.05f, "%.2f sec");
    roarPreStampedeStartTime = std::isfinite(roarPreStampedeStartTime)
        ? std::clamp(roarPreStampedeStartTime, 0.0f, rangeLimit - 0.05f) : 0.0f;
    ImGui::DragFloat(U8("咆哮 終了時間"), &roarPreStampedeEndTime, 0.05f,
        roarPreStampedeStartTime + 0.05f, rangeLimit, "%.2f sec");
    roarPreStampedeEndTime = std::isfinite(roarPreStampedeEndTime)
        ? std::clamp(roarPreStampedeEndTime, roarPreStampedeStartTime + 0.05f, rangeLimit) : rangeLimit;
    ImGui::DragFloat(U8("防御行動 至近距離"), &defensiveTooCloseDistance, 0.1f, 0.0f, 30.0f, "%.2f m");
    ImGui::DragFloat(U8("咆哮 範囲"), &roarRadius, 0.1f, 0.0f, 30.0f, "%.2f m");
    ImGui::DragFloat(U8("咆哮 高低差許容"), &roarHeightTolerance, 0.1f, 0.0f, 30.0f, "%.2f m");
    const float previousCooldownDuration = roarCooldownDuration;
    ImGui::DragFloat(U8("咆哮クールタイム"), &roarCooldownDuration, 0.1f, 0.0f, 120.0f, "%.2f sec");
    defensiveTooCloseDistance = (std::max)(0.0f, defensiveTooCloseDistance);
    roarRadius = (std::max)(0.0f, roarRadius);
    roarHeightTolerance = (std::max)(0.0f, roarHeightTolerance);
    roarCooldownDuration = (std::max)(0.0f, roarCooldownDuration);
    if (roarCooldownRemaining > 0.0f && roarCooldownDuration != previousCooldownDuration)
        roarCooldownRemaining = (std::max)(0.0f,
            roarCooldownRemaining + roarCooldownDuration - previousCooldownDuration);
    const auto context = BuildTargetContext();
    const char* region = !context.valid ? "None" : context.region == PlayerRelativeRegion::Back ? "Back" :
        context.region == PlayerRelativeRegion::Side ? "Side" : "Front";
    ImGui::Text(U8("咆哮BT状態: %s"), roarBTStatus.c_str());
    ImGui::Text(U8("咆哮Stage: %d"), static_cast<int>(roarBT.stage));
    ImGui::Text(U8("咆哮衝撃波 発動済み: %s"), roarBT.shockwaveFired ? "true" : "false");
    ImGui::Text(U8("Player距離: %.2f m"), context.xzDistance);
    ImGui::Text(U8("Player位置Region: %s"), region);
    ImGui::Text(U8("咆哮クールタイム残り: %.2f sec"), roarCooldownRemaining);
    const bool currentResult = EvaluateRoarPlanForDebug();
    ImGui::Text(U8("咆哮候補: %s"), currentResult ? "true" : "false");
    const auto scene = GetOwnerScene();
    const auto player = scene ? scene->GetActorManager()->GetActorOfType<Player>() : nullptr;
    const std::string state = stateMachine_ ? stateMachine_->GetStateName() : "None";
    const bool otherBTAction = IsOtherBTActionForRoar(activeNode);
    const auto tf = [](bool value) { return value ? "true" : "false"; };
    ImGui::SeparatorText(U8("Roar候補条件の詳細（現在値）"));
    ImGui::Text(U8("Roar Player有効: %s"), tf(player && !player->IsPendingKill()));
    ImGui::Text(U8("Roar Player生存: %s"), tf(player && player->GetHp() > 0));
    ImGui::Text(U8("Roar Grux生存: %s"), tf(!IsDead()));
    ImGui::Text(U8("Roar 戦闘AI有効: %s"), tf(battleAIActive));
    ImGui::Text(U8("Roar Defensive開始可能: %s"), tf(IsRoarExecutionAllowed()));
    ImGui::Text(U8("Roar TargetContext有効: %s"), tf(context.valid));
    ImGui::Text(U8("Roar Player距離: %.2f"), context.xzDistance);
    ImGui::Text(U8("Roar TooClose距離: %.2f"), defensiveTooCloseDistance);
    ImGui::Text(U8("Roar 距離条件OK: %s"), tf(context.xzDistance <= defensiveTooCloseDistance));
    ImGui::Text(U8("Roar Player Region: %s"), region);
    ImGui::Text(U8("Roar Back条件OK: %s"), tf(context.region == PlayerRelativeRegion::Back));
    ImGui::Text(U8("Roar Cooldown残り: %.2f"), roarCooldownRemaining);
    ImGui::Text(U8("Roar Cooldown条件OK: %s"), tf(!(roarCooldownRemaining > 0.0f)));
    ImGui::Text(U8("Roar 実行中: %s"), tf(IsRoarBTActive()));
    ImGui::Text(U8("Roar Death/Stun中: %s"), tf(IsDead() || state == "EnemyDeathState" || state == "EnemyStunState"));
    ImGui::Text(U8("Roar 他BT行動中: %s"), tf(otherBTAction));
    ImGui::Text(U8("Roar Preview中: %s"), tf(IsAnimationEditorPreviewActive()));
    ImGui::Text(U8("Roar 演出中: %s"), tf(isDeathPerform ||
        finalHitReactionActive || finalHitReactionHeld));
    ImGui::Text(U8("CanPlanRoar: %s"), tf(currentResult));
    // CanPlanAnyDefensive currently delegates directly to CanPlanRoar.
    ImGui::Text(U8("CanPlanAnyDefensive: %s"), tf(currentResult));
    ImGui::Text(U8("Roar候補外理由: %s"), GetRoarRejectReasonForDebug().c_str());
    ImGui::Text(U8("Roar 現在BTノード: %s"), activeNode ? activeNode->GetName().c_str() : "None");
    ImGui::Text(U8("Roar 旧State: %s"), state.c_str());
    ImGui::Text(U8("Roar BT有効: %s"), tf(behaviorTreeFastComboEnabled));
    ImGui::Text(U8("Roar Grux削除待ち: %s"), tf(IsPendingKill()));
    ImGui::Text(U8("Roar 移動Component有効: %s"), tf(characterMovementComponent != nullptr));
    ImGui::Text(U8("Roar AnimationController有効: %s"), tf(GetBodyAnimationController() != nullptr));
    ImGui::Text(U8("Roar Scene有効: %s"), tf(scene != nullptr));
    ImGui::Text(U8("Roar Charge BT実行中: %s"), tf(IsChargeAttackBTActive()));
    ImGui::Text(U8("Roar Dash BT実行中: %s"), tf(IsDashAttackBTActive()));
    ImGui::Text(U8("Roar Charge移動中: %s"), tf(chargeMovementActive));
    ImGui::Text(U8("Roar Dash移動中: %s"), tf(dashAttackMovementActive));
    ImGui::Text(U8("Roar Jump MotionWarp実行中: %s"), tf(jumpMotionWarpOverrideActive));
    ImGui::SeparatorText(U8("Roar候補の直近実判定（描画では更新しない）"));
    ImGui::Text(U8("Roar 実判定済み: %s"), tf(roarPlanDebugEvaluated));
    ImGui::Text(U8("Roar 直近実判定: %s"), roarPlanDebugEvaluated ? tf(roarPlanDebugLastResult) : U8("未判定"));
    ImGui::Text(U8("Roar 直近候補外理由: %s"), roarPlanDebugEvaluated ? roarPlanDebugLastRejectReason.c_str() : U8("未判定"));
    ImGui::Text(U8("Roar 実判定時BTノード: %s"), roarPlanDebugEvaluated ? roarPlanDebugLastNode.c_str() : U8("未判定"));
    ImGui::Checkbox(U8("AI距離をWorld表示"), &showBossAIDebug);
    ImGui::TextColored(ImVec4(0.1f, 0.8f, 1.0f, 1.0f), U8("水色: 防御行動 至近距離"));
    ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.6f, 1.0f), U8("桃色: 咆哮 範囲"));
#endif
}
