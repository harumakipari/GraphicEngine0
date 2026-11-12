#include "pch.h"

#include <random>

#include "GruxEnemy.h"

#include "Components/Render/PointLightComponent.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Camera/DarkGameCamera.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Scenes/GameScene.h"
#include "Game/DarkGame/DarkActors/DarkStage.h"
#include "Game/DarkGame/DarkActors/ModelDebrisEmitterActor.h"
#include "Physics/CollisionFunction.h"

#include "Game/DarkGame/BehaviorTree/BehaviorData.h"
#include "Game/DarkGame/BehaviorTree/BehaviorTree.h"
#include "Game/DarkGame/BehaviorTree/ActionBase.h"
#include "Game/DarkGame/BehaviorTree/GruxFastComboBT.h"
#include "Game/DarkGame/BehaviorTree/GruxJumpAttackBT.h"
#include "Game/DarkGame/BehaviorTree/GruxDashAttackBT.h"
#include "Game/DarkGame/BehaviorTree/GruxChargeAttackBT.h"
#include "Game/DarkGame/BehaviorTree/GruxRoarBT.h"
#include "Game/DarkGame/BehaviorTree/AttackRecoveryBT.h"
#include "Game/DarkGame/BehaviorTree/NodeBase.h"

void GruxEnemy::Initialize(const Transform& transform)
{
	maxHp = 50;
	hp = maxHp;
	delayedHp = static_cast<float>(hp);

	const std::string parentName = "GruxEnemy";
	Character::Initialize(transform);

	// モデルとアニメーション生成する
	InitializeSkeletalMeshAndAnimations(parentName);

	// 当たり判定、ポイントライト、回転コンポーネントを追加
	CreateCoreComponents(parentName);

	// 武器の位置コンポーネント追加
	CreateWeaponSocketComponents(parentName);

	// エフェクトコンポーネント追加
	CreateEffectComponents(parentName);

	// 軌跡コンポーネント追加
	InitializeTrailEffects();

	// UIを作成する
	CreateUI();

	// ビヘイビアツリー設定
	CreateBossBehaviorTree();

	// 予兆用のモデルを作成する
	CreateTelegraphMesh(parentName);

	// Roar衝撃時の破片を用意する。
	EnsureRoarImpactDebris();

	// Roar予兆中の浮遊物を用意する。
	EnsureRoarFloatingDebris();
}

void GruxEnemy::Update(float deltaTime)
{
	// 非スケール時間
	const float unscaledDeltaTime = Time::UnscaledDeltaTime();

	// Roar衝撃破片は、Actor固有TimeScaleを含まないグローバル時間で移動・寿命を更新する。
	UpdateRoarImpactDebris(Time::DeltaTime());

	// 第2段階のテクスチャ切り替え範囲をワールド座標で更新する。
	UpdatePhase2TextureBlendWorldYRange();

	const auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene());
	// 第2段階HPバーの演出を更新する。
	UpdatePhase2HpBarPresentation();

	// Phase1終了待ちは遅延HPとBody Animationを進め、後続の通常更新を省略する。
	if (gameScene && gameScene->IsPhase1BreakPending())
	{
		ResetWeaponTelegraphRimLight();
		AdvanceDelayedHpBarForPhaseTransition();
		if (const auto controller = GetBodyAnimationController())
		{
			controller->OnUpdate(deltaTime);
		}
		return;
	}

	// Phase2移行中のAnimationはGamePhase2TransitionController側で進める。
	// GruxEnemy側の通常Animation更新を含め、後続の通常更新は行わない。
	if (gameScene && gameScene->IsBossPhaseTransitionActive())
	{
		ResetWeaponTelegraphRimLight();
		return;
	}

	// Roar・死亡・Chargeの中断整理。
	TickRoarLifecycle(deltaTime);
	if (hp <= 0)
	{
		EndFourthHitReaction();
	}
	const bool phaseTransitionOwnsDefeat = gameScene && gameScene->IsBossPhaseTransitionActive();
	if (hp <= 0 && !isDeathPerform && !phaseTransitionOwnsDefeat && !(gameScene && gameScene->IsPhase1BreakPending()))
	{
		BeginDeathPresentation();
	}

	if (ShouldAbortChargeAttackBT())
	{
		CleanupChargeAttackBT();
		activeNode = nullptr;
		if (behaviorData)
		{
			behaviorData->Init();
		}
	}

	// 通常時のHPバー表示を更新する。
	UpdateHpBar(unscaledDeltaTime);

	// 被弾リアクション。以下の早期returnでは、後続のクールダウン・BT・表示・攻撃判定を省略する。
	if (finalHitReactionHeld)
	{
		const auto controller = GetBodyAnimationController();
		if (controller && controller->GetCurrentAnimationName().starts_with("HitReact_"))
		{
			// HitReact_のポーズ保持中は、Animationも進めない。
			ResetWeaponTelegraphRimLight();
			return;
		}
		finalHitReactionHeld = false;
	}

	// 最終被弾リアクションはBody Animationだけを進める。
	if (finalHitReactionActive)
	{
		ResetWeaponTelegraphRimLight();
		if (const auto controller = GetBodyAnimationController())
			controller->OnUpdate(deltaTime);
		return;
	}

	// 通常攻撃4段目の被弾中は、Body Animationとリアクション残り時間を進める。
	if (fourthHitReactionActive)
	{
		ResetWeaponTelegraphRimLight();
		if (const auto controller = GetBodyAnimationController())
		{
			controller->OnUpdate(deltaTime);
		}
		fourthHitReactionRemaining = (std::max)(0.0f, fourthHitReactionRemaining - (std::max)(0.0f, deltaTime));
		if (fourthHitReactionRemaining <= 0.0f)
		{
			EndFourthHitReaction();
		}
		return;
	}

	// ボイス・行動クールダウン。
	hitVoiceCooldownTimer = (std::max)(0.0f, hitVoiceCooldownTimer - unscaledDeltaTime);

	if (!IsAnimationEditorPreviewActive())
		UpdateActionCooldowns(deltaTime);

	// Rush中はAI移動入力を止め、Characterと予兆を更新する。後続のBT・Trail・攻撃判定は省略する。
	if (rushCombatHoldActive)
	{
		StopAIMovement();
		Character::Update(deltaTime);
		// 攻撃予兆の表示を更新する。
		UpdateAttackTelegraphVisuals(deltaTime);
		return;
	}

	// Charge復帰待ちも同じ更新範囲とし、待ち時間を進める。解除したフレームも後続へ進まない。
	if (interruptedChargeRecoveryActive)
	{
		StopAIMovement();
		interruptedChargeRecoveryTimer += (std::max)(0.0f, deltaTime);
		Character::Update(deltaTime);
		// 攻撃予兆の表示を更新する。
		UpdateAttackTelegraphVisuals(deltaTime);
		if (interruptedChargeRecoveryTimer >= interruptedChargeRecoveryDuration)
		{
			interruptedChargeRecoveryActive = false;
			interruptedChargeRecoveryTimer = 0.0f;
			interruptedChargeRecoveryDuration = 0.0f;
		}
		return;
	}

	// Dashの中断監視。
	if (IsDashAttackBTActive() && (!battleAIActive || IsDead() || IsDeathPresentationActive() || IsStunned()))
	{
		CleanupDashAttackBT();
		activeNode = nullptr;
		if (behaviorData)
		{
			behaviorData->Init();
		}
	}

	// BehaviorTree更新と、その結果に応じたCharge・Dashの後始末。
	if (battleAIActive)
	{
		UpdateBehaviorTree(deltaTime);
		if (IsChargeAttackBTActive() && (!activeNode || aiTree->GetLastRunResult() == ActionBase::State::Failed))
		{
			CleanupChargeAttackBT();
		}
		if (IsDashAttackBTActive() && (!activeNode || aiTree->GetLastRunResult() == ActionBase::State::Failed))
		{
			CleanupDashAttackBT();
		}
	}

	// Character内でAnimation・Notifyを更新してから、予兆・Trailに反映する。
	Character::Update(deltaTime);

	// 攻撃予兆の表示を更新する。
	UpdateAttackTelegraphVisuals(deltaTime);

	// 左右武器のTrailを更新する。
	UpdateWeaponTrails(deltaTime);

	// Previewはここまで更新し、後続のMotionWarp・表示更新・ゲームプレイ判定を省略する。
	if (IsAnimationEditorPreviewActive())
		return;

	// MotionWarp・表示更新。Gruxは遅延移動を有効化していないため、
	// ここで設定する追加速度は次フレームのComponent Tickで消費される。
	UpdateAnimationMotionWarp(deltaTime);

	SetScale({ enemyScale,enemyScale,enemyScale });

	// ImageComponentのalpha更新
	{
		easingRunner->Tick(deltaTime);
		float bossNameImageAlpha = std::lerp(0.0f, 1.0f, easingFactorAlpha);
		gruxNameImageComponent->SetColor(DirectX::XMFLOAT4{ 1.0f,1.0f,1.0f,bossNameImageAlpha });
	}

	// 被弾時のフラッシュ
	{
		const float fadeSpeed = damageFlashDuration > 0.0f ? damageFlashStartValue / damageFlashDuration : damageFlashStartValue;
		skeletalMeshComponent->plusAlphaCBuffer->data.flashValue = (std::max)(0.0f, skeletalMeshComponent->plusAlphaCBuffer->data.flashValue - fadeSpeed * deltaTime);
	}

	// DangerAreaによるJustDodge判定は、成功済み対象を除外する後続の武器命中判定より先に行う。
	if (isDangerWindow)
	{
		if (auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>())
		{
			const DirectX::XMFLOAT3 playerTestPosition = player->GetPosition();
			DirectX::XMFLOAT3 playerCapsuleCenter = playerTestPosition;
			float playerCapsuleRadius = 0.0f;
			float playerCapsuleHeight = 0.0f;
			if (const auto capsule = std::dynamic_pointer_cast<CapsuleComponent>(player->FindComponentByName("capsuleComponent")))
			{
				playerCapsuleCenter = capsule->GetComponentLocation();
				playerCapsuleRadius = capsule->GetRadius();
				playerCapsuleHeight = capsule->GetHeight();
			}
			const DangerArea::CapsuleResult capsuleResult = dangerArea.IntersectsPlayerCapsule(playerCapsuleCenter, playerCapsuleRadius, playerCapsuleHeight);
			const bool inside = capsuleResult.overlap;
			if (inside && player->GetJustDodgeWindow())
			{
				if (!HasJustDodgedAttack(player.get()))
				{
					TryStartJustDodgeSuccess(player.get());
				}
			}
		}
	}

	// 左右武器の攻撃判定とDebug表示を更新する。
	UpdateWeaponHitDetection();
}

void GruxEnemy::RenderTrail(ID3D11DeviceContext* immediateContext)
{
	leftWeaponTrail.SetRushColorEnabled(true, bossTrailColor);
	rightWeaponTrail.SetRushColorEnabled(true, bossTrailColor);
	leftWeaponTrail.SetEmissiveStrength(bossTrailEmissiveStrength);
	rightWeaponTrail.SetEmissiveStrength(bossTrailEmissiveStrength);
	leftWeaponTrail.Render(immediateContext);
	rightWeaponTrail.Render(immediateContext);

	for (auto& streak : chargeWindTrails)
	{
		if (!streak.active)
			continue;
		streak.trail.SetRushColorEnabled(true, chargeWindTrailColor);
		streak.trail.SetEmissiveStrength(chargeWindTrailEmissiveStrength);
		streak.trail.Render(immediateContext);
	}
}


// ボスAIのImGui描画
void GruxEnemy::DrawImGuiDetails()
{
#ifdef USE_IMGUI

	if (ImGui::CollapsingHeader(U8("ゲームプレイ調整")))
	{
		if (ImGui::TreeNode(U8("戦闘攻撃候補")))
		{
			for (size_t i = 0; i < combatAttackData.size(); ++i)
			{
				auto& attack = combatAttackData[i];
				ImGui::PushID(static_cast<int>(i));
				ImGui::SeparatorText(
					attack.type == BossAttackType::PrimaryAttackLA ? U8("左腕の通常攻撃") :
					attack.type == BossAttackType::PrimaryAttackRA ? U8("右腕の通常攻撃") :
					attack.type == BossAttackType::FastCombo ? U8("高速連続攻撃") :
					attack.type == BossAttackType::JumpAttack ? U8("ジャンプ攻撃") :
					attack.type == BossAttackType::DashAttack ? U8("ダッシュ攻撃") :
					attack.type == BossAttackType::ChargeAttack ? U8("突進攻撃") : U8("その他の攻撃"));
				ImGui::DragFloat(U8("攻撃後の硬直時間（秒）"), &attack.recoveryDuration, 0.05f, 0.0f, 30.0f, "%.2f sec");
				ImGui::PopID();
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("咆哮攻撃")))
		{
			ImGui::DragFloat(U8("第一形態 咆哮の攻撃半径（距離）"), &roarRadiusPhase1, 0.1f, 0.0f, 30.0f, "%.2f m");
			ImGui::DragFloat(U8("第二形態 咆哮の攻撃半径（距離）"), &roarRadiusPhase2, 0.1f, 0.0f, 30.0f, "%.2f m");
			ImGui::DragInt(U8("咆哮ダメージ"), &roarDamage, 1.0f, 0, 1000);
			ImGui::DragFloat(U8("咆哮の再使用待ち時間（秒）"), &roarCooldownDuration, 0.1f, 0.0f, 120.0f, "%.2f sec");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("突進攻撃")))
		{
			ImGui::DragFloat(U8("第一形態 突進速度（距離/秒）"), &chargeSpeed, 0.1f, 0.1f, 50.0f, "%.2f m/s");
			ImGui::DragFloat(U8("第二形態 突進速度（距離/秒）"), &chargeSpeedPhase2, 0.1f, 0.1f, 50.0f, "%.2f m/s");
			ImGui::DragFloat(U8("第一形態 突進の当たり判定倍率"), &chargePlayerCastRadiusScale, 0.01f, 0.1f, 2.0f, "%.2f");
			ImGui::DragFloat(U8("第二形態 突進の当たり判定倍率"), &chargePlayerCastRadiusScalePhase2, 0.01f, 0.1f, 2.0f, "%.2f");
			ImGui::DragFloat(U8("突進ダメージ判定 前方オフセット（距離）"), &chargePlayerDamageForwardOffset, 0.01f, 0.0f, 5.0f, "%.2f m");
			ImGui::DragFloat(U8("第一形態 突進ジャスト回避判定倍率"), &chargeJustDodgeCastRadiusScale, 0.01f, 0.1f, 2.0f, "%.2f");
			ImGui::DragFloat(U8("第二形態 突進ジャスト回避判定倍率"), &chargeJustDodgeCastRadiusScalePhase2, 0.01f, 0.1f, 5.0f, "%.2f");
			ImGui::DragFloat(U8("突進 壁判定の半径倍率"), &chargeWallCastRadiusScale, 0.01f, 0.1f, 2.0f, "%.2f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("ジャンプ攻撃")))
		{
			ImGui::DragFloat(U8("ジャンプ間の遷移時間（秒）"), &tripleJumpTransitionDuration, 0.01f, 0.0f, 1.0f, "%.2f sec");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("近接・高速連続攻撃")))
		{
			const char* stepNames[] = { U8("1段目"), U8("2段目"), U8("3段目") };
			for (int i = 0; i < 3; ++i)
			{
				ImGui::DragFloat((std::string(U8("高速連続攻撃 ")) + stepNames[i] + U8(" 最大踏み込み距離（距離）")).c_str(), &fastComboMaxStepInDistance[i], 0.1f, 0.0f, 6.0f, "%.2f m");
			}
			ImGui::DragFloat(U8("高速連続攻撃 踏み込み最大速度（距離/秒）"), &fastComboStepInMaxSpeed, 0.5f, 1.0f, 60.0f, "%.2f m/s");
			ImGui::DragFloat(U8("高速連続攻撃の準備時間（秒）"), &fastComboPrepareDuration, 0.05f, 0.0f, 10.0f, "%.2f sec");
			ImGui::DragFloat(U8("ボスの武器の攻撃範囲"), &hitWeaponRadius, 0.05f, 0.1f, 2.0f);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("被弾リアクション・攻撃後硬直")))
		{
			ImGui::DragFloat(U8("代替の攻撃後硬直時間（秒）"), &recoveryDuration, 0.05f, 0.0f, 10.0f, "%.2f sec");
			ImGui::DragFloat(U8("ひるみ 被弾リアクション発生確率"), &fourthHitReactionChance, 0.01f, 0.0f, 1.0f, "%.2f");
			ImGui::DragFloat(U8("ひるみ 被弾リアクション時間（秒）"), &fourthHitReactionDuration, 0.01f, 0.0f, 2.0f, "%.2f sec");
			ImGui::TreePop();
		}
	}

	if (ImGui::CollapsingHeader(U8("AI調整")))
	{
		if (ImGui::TreeNode(U8("戦闘距離・行動選択")))
		{
			ImGui::DragFloat(U8("防御行動 至近距離（距離）"), &defensiveTooCloseDistance, 0.1f, 0.0f, 30.0f, "%.2f m");
			ImGui::DragFloat(U8("防御行動 後方判定の最小角度（度）"), &defensiveBackMinAngle, 1.0f, 0.0f, 180.0f, "%.1f deg");
			ImGui::DragFloat(U8("近距離正面 高速連続攻撃の選択確率"), &nearFrontFastComboProbability, 0.01f, 0.0f, 1.0f, "%.2f");
			ImGui::DragFloat(U8("高速連続攻撃 正面判定の最大角度（度）"), &fastComboFrontMaxAngle, 1.0f, 0.0f, 180.0f, "%.1f deg");
			ImGui::DragFloat(U8("待機時間（秒）"), &behaviorIdleDuration, 0.05f, 0.0f, 10.0f, "%.2f sec");
			ImGui::SeparatorText(U8("近距離戦闘設定"));
			ImGui::DragFloat(U8("近距離戦闘 最小距離"), &closeCombatSettings.minRange, 0.1f, 0.0f, 100.0f, "%.2f");
			ImGui::DragFloat(U8("近距離戦闘 実行可能な最大距離"), &closeCombatSettings.executeMaxRange, 0.1f, closeCombatSettings.minRange, 100.0f, "%.2f");
			ImGui::DragFloat(U8("近距離戦闘 計画可能な最大距離"), &closeCombatSettings.planMaxRange, 0.1f, closeCombatSettings.executeMaxRange, 100.0f, "%.2f");
			ImGui::DragFloat(U8("近距離攻撃 正面角度制限（度）"), &closeCombatSettings.facingLimitDegrees, 1.0f, 0.0f, 180.0f, "%.1f deg");
			ImGui::DragFloat(U8("攻撃時の理想距離"), &desiredAttackDistance, 0.05f, 0.0f, 10.0f, "%.2f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("攻撃準備位置")))
		{
			ImGui::SeparatorText(U8("突進攻撃の準備位置"));
			ImGui::DragFloat(U8("突進準備距離 最小（距離）"), &chargeSetupDistanceMin, 0.1f, 0.1f, 30.0f, "%.2f m");
			ImGui::DragFloat(U8("突進準備距離 最大（距離）"), &chargeSetupDistanceMax, 0.1f, 0.1f, 30.0f, "%.2f m");
			ImGui::DragFloat(U8("突進準備 最低移動距離（距離）"), &chargeSetupMinimumMoveDistance, 0.1f, 0.0f, 30.0f, "%.2f m");
			ImGui::DragFloat(U8("三連突進 位置補正 最大距離（距離）"), &tripleChargeRepositionMaxDistance, 0.05f, 0.0f, 2.0f, "%.2f m");
			ImGui::DragFloat(U8("三連突進 位置補正 安全余裕（距離）"), &tripleChargeRepositionSafetyMargin, 0.01f, 0.0f, 1.0f, "%.2f m");
			ImGui::DragFloat(U8("三連突進 位置補正速度（距離/秒）"), &tripleChargeRepositionSpeed, 0.05f, 0.1f, 10.0f, "%.2f m/s");
			ImGui::DragFloat(U8("三連突進 位置補正 横方向の安全余裕（距離）"), &tripleChargeRepositionSideSafetyMargin, 0.01f, 0.0f, 1.0f, "%.2f m");
			ImGui::SeparatorText(U8("ダッシュ攻撃の準備位置"));
			ImGui::DragFloat(U8("ダッシュ準備距離 最小（距離）"), &dashSetupDistanceMin, 0.1f, 0.1f, 30.0f, "%.2f m");
			ImGui::DragFloat(U8("ダッシュ準備距離 最大（距離）"), &dashSetupDistanceMax, 0.1f, 0.1f, 30.0f, "%.2f m");
			ImGui::DragFloat(U8("ダッシュ準備 最低移動距離（距離）"), &dashSetupMinimumMoveDistance, 0.1f, 0.0f, 30.0f, "%.2f m");
			ImGui::DragFloat(U8("高速連続攻撃 接近最大時間（秒）"), &fastComboApproachMaxDuration, 0.05f, 0.0f, 30.0f, "%.2f sec");
			ImGui::DragFloat(U8("高速連続攻撃 接近再試行の待機時間（秒）"), &fastComboApproachRetryCooldown, 0.05f, 0.0f, 30.0f, "%.2f sec");
			ImGui::SeparatorText(U8("ジャンプ攻撃位置調整"));
			ImGui::DragFloat(U8("位置調整距離 最小"), &jumpSetupDistanceMin, 0.1f, 0.1f, 30.0f, "%.2f");
			ImGui::DragFloat(U8("位置調整距離 最大"), &jumpSetupDistanceMax, 0.1f, 0.1f, 30.0f, "%.2f");
			ImGui::DragFloat(U8("ジャンプ最低移動距離"), &jumpSetupMinimumMoveDistance, 0.1f, 0.0f, 30.0f, "%.2f");
			ImGui::DragFloat(U8("最大ジャンプ距離"), &maxJumpDistance, 0.05f, 0.0f, 30.0f, "%.2f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("旋回")))
		{
			ImGui::DragFloat(U8("正面向き完了角度（度）"), &closeCombatSettings.faceCompleteAngleDegrees, 0.5f, 0.0f, 90.0f, "%.1f deg");
			ImGui::DragFloat(U8("攻撃段間の正面向き完了角度（度）"), &interStageFaceCompleteAngle, 0.5f, 0.0f, 90.0f, "%.1f deg");
			ImGui::DragFloat(U8("攻撃段間の最大旋回角度（度）"), &interStageMaxFacingAngle, 0.5f, 0.0f, 180.0f, "%.1f deg");
			ImGui::DragFloat(U8("攻撃段間の旋回待機時間（秒）"), &interStageFaceDelay, 0.01f, 0.0f, 0.5f, "%.2f sec");
			ImGui::DragFloat(U8("旋回速度（度/秒）"), &turnSpeed, 1.0f, 0.0f, 720.0f, "%.1f deg/sec");
			ImGui::DragFloat(U8("旋回完了角度（度）"), &turnCompleteAngle, 1.0f, 0.0f, 180.0f, "%.1f deg");
			ImGui::TreePop();
		}
	}

	if (ImGui::CollapsingHeader(U8("演出調整")))
	{
		if (ImGui::TreeNode(U8("咆哮予兆")))
		{
			ImGui::DragFloat(U8("外周の不透明度"), &roarTelegraphOuterAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
			ImGui::ColorEdit3(U8("第一形態 外周色"), &roarTelegraphPhase1OuterTint.x);
			ImGui::ColorEdit3(U8("第一形態 塗り色 最小"), &roarTelegraphPhase1FillTintMin.x);
			ImGui::ColorEdit3(U8("第一形態 塗り色 最大"), &roarTelegraphPhase1FillTintMax.x);
			ImGui::ColorEdit3(U8("第二形態 外周色"), &roarTelegraphPhase2OuterTint.x);
			ImGui::ColorEdit3(U8("第二形態 塗り色 最小"), &roarTelegraphPhase2FillTintMin.x);
			ImGui::ColorEdit3(U8("第二形態 塗り色 最大"), &roarTelegraphPhase2FillTintMax.x);
			ImGui::DragFloat(U8("塗りの発光強度 最小"), &roarTelegraphFillGlowMin, 0.05f, 0.0f, 20.0f, "%.2f");
			ImGui::DragFloat(U8("塗りの発光強度 最大"), &roarTelegraphFillGlowMax, 0.05f, 0.0f, 20.0f, "%.2f");
			ImGui::DragFloat(U8("塗りの不透明度 最小"), &roarTelegraphFillAlphaMin, 0.01f, 0.0f, 1.0f, "%.2f");
			ImGui::DragFloat(U8("塗りの不透明度 最大"), &roarTelegraphFillAlphaMax, 0.01f, 0.0f, 1.0f, "%.2f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("咆哮の浮遊破片")))
		{
			ImGui::Checkbox(U8("浮遊破片を有効化"), &roarFloatingDebrisEnabled);
			ImGui::DragFloat(U8("浮遊範囲の内周比率"), &roarFloatingDebrisInnerRatio, 0.01f, 0.0f, 1.0f, "%.2f");
			ImGui::DragFloat(U8("浮遊範囲の外周比率"), &roarFloatingDebrisOuterRatio, 0.01f, 0.0f, 1.0f, "%.2f");
			ImGui::DragFloat(U8("浮遊する高さ 最小（距離）"), &roarFloatingDebrisMinHeight, 0.01f, 0.0f, 5.0f, "%.2f m");
			ImGui::DragFloat(U8("浮遊する高さ 最大（距離）"), &roarFloatingDebrisMaxHeight, 0.01f, 0.0f, 5.0f, "%.2f m");
			ImGui::DragFloat(U8("浮遊破片のスケール 最小"), &roarFloatingDebrisScaleMin, 0.01f, 0.0f, 5.0f, "%.2f");
			ImGui::DragFloat(U8("浮遊破片のスケール 最大"), &roarFloatingDebrisScaleMax, 0.01f, 0.0f, 5.0f, "%.2f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("咆哮の衝撃破片")))
		{
			ImGui::Checkbox(U8("衝撃破片を有効化"), &roarImpactDebrisEnabled);
			ImGui::TextDisabled(U8("衝撃破片の初速・スケール・寿命は、新しく生成する破片に反映されます。"));
			ImGui::DragFloat(U8("衝撃破片の発生高さ（距離）"), &roarImpactDebrisSpawnHeight, 0.01f, 0.0f, 1.0f, "%.2f m");
			ImGui::DragFloat(U8("衝撃破片の重力（距離/秒/秒）"), &roarImpactDebrisGravity, 0.1f, 0.0f, 30.0f, "%.2f m/s2");
			ImGui::DragFloatRange2(U8("小破片の水平初速（距離/秒）"), &roarImpactDebrisSmallHorizontalSpeedMin, &roarImpactDebrisSmallHorizontalSpeedMax, 0.05f, 0.0f, 20.0f, "Min: %.2f", "Max: %.2f");
			ImGui::DragFloatRange2(U8("小破片の垂直初速（距離/秒）"), &roarImpactDebrisSmallVerticalSpeedMin, &roarImpactDebrisSmallVerticalSpeedMax, 0.05f, 0.0f, 20.0f, "Min: %.2f", "Max: %.2f");
			ImGui::DragFloatRange2(U8("小破片の寿命（秒）"), &roarImpactDebrisSmallLifetimeMin, &roarImpactDebrisSmallLifetimeMax, 0.01f, 0.0f, 5.0f, "Min: %.2f", "Max: %.2f");
			ImGui::DragFloatRange2(U8("大破片の水平初速（距離/秒）"), &roarImpactDebrisBlockHorizontalSpeedMin, &roarImpactDebrisBlockHorizontalSpeedMax, 0.05f, 0.0f, 20.0f, "Min: %.2f", "Max: %.2f");
			ImGui::DragFloatRange2(U8("大破片の垂直初速（距離/秒）"), &roarImpactDebrisBlockVerticalSpeedMin, &roarImpactDebrisBlockVerticalSpeedMax, 0.05f, 0.0f, 20.0f, "Min: %.2f", "Max: %.2f");
			ImGui::DragFloatRange2(U8("大破片の寿命（秒）"), &roarImpactDebrisBlockLifetimeMin, &roarImpactDebrisBlockLifetimeMax, 0.01f, 0.0f, 5.0f, "Min: %.2f", "Max: %.2f");
			ImGui::DragFloatRange2(U8("小破片のスケール"), &roarImpactDebrisSmallScaleMin, &roarImpactDebrisSmallScaleMax, 0.01f, 0.0f, 10.0f, "Min: %.2f", "Max: %.2f");
			ImGui::DragFloatRange2(U8("大破片のスケール"), &roarImpactDebrisBlockScaleMin, &roarImpactDebrisBlockScaleMax, 0.01f, 0.0f, 10.0f, "Min: %.2f", "Max: %.2f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("咆哮の地面噴出")))
		{
			ImGui::Checkbox(U8("地面噴出を有効化"), &roarGroundBurstEnabled);
			ImGui::DragFloat(U8("噴出範囲の内周比率"), &roarGroundBurstInnerRingRatio, 0.01f, 0.0f, 1.0f, "%.2f");
			ImGui::DragFloat(U8("噴出範囲の外周比率"), &roarGroundBurstOuterRingRatio, 0.01f, 0.0f, 1.0f, "%.2f");
			ImGui::DragFloat(U8("噴出角度のばらつき（度）"), &roarGroundBurstAngleJitterDegrees, 0.25f, 0.0f, 45.0f, "%.1f deg");
			ImGui::DragFloat(U8("噴出半径のばらつき"), &roarGroundBurstRadiusJitter, 0.005f, 0.0f, 0.25f, "%.3f");
			ImGui::DragFloat(U8("噴出位置の高さオフセット（距離）"), &roarGroundBurstYOffset, 0.01f, -1.0f, 1.0f, "%.2f m");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("突進予兆")))
		{
			ImGui::Checkbox(U8("三連突進 予兆の発光を有効化"), &tripleChargeTelegraphEmissiveEnabled);
			ImGui::ColorEdit3(U8("第一形態 三連突進予兆の発光色"), &tripleChargeTelegraphPhase1EmissiveColor.x);
			ImGui::ColorEdit3(U8("第二形態 三連突進予兆の発光色"), &tripleChargeTelegraphPhase2EmissiveColor.x);
			ImGui::DragFloat(U8("三連突進 予兆の発光強度"), &tripleChargeTelegraphEmissiveIntensity, 0.05f, 0.0f, 20.0f, "%.2f");
			ImGui::DragFloat(U8("三連突進 予兆の拡大時間（秒）"), &tripleChargeTelegraphExpandDuration, 0.01f, 0.0f, 1.0f, "%.2f sec");
			ImGui::DragFloat(U8("三連突進 予兆の維持時間（秒）"), &tripleChargeTelegraphHoldDuration, 0.01f, 0.0f, 1.0f, "%.2f sec");
			ImGui::DragFloat(U8("三連突進予兆のワールド高さ"), &tripleChargeTelegraphFixedWorldY, 0.01f, -5.0f, 5.0f, "%.3f");
			ImGui::DragFloat(U8("三連突進 予兆の前方オフセット（距離）"), &tripleChargeTelegraphForwardOffset, 0.01f, -1.0f, 2.0f, "%.2f m");
			ImGui::DragFloat(U8("三連突進 予兆の最大距離（距離）"), &tripleChargeTelegraphMaxDistance, 0.1f, 0.1f, 100.0f, "%.2f m");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("ジャンプ予兆")))
		{
			ImGui::DragFloat(U8("ジャンプ予兆のスケール"), &jumpTelegraphScale, 0.01f, 0.01f, 10.0f, "%.2f");
			ImGui::Checkbox(U8("ジャンプ予兆の発光を有効化"), &jumpTelegraphEmissiveEnabled);
			ImGui::ColorEdit3(U8("第一形態 予兆の発光色"), &jumpTelegraphPhase1EmissiveColor.x);
			ImGui::ColorEdit3(U8("第二形態 予兆の発光色"), &jumpTelegraphPhase2EmissiveColor.x);
			ImGui::DragFloat(U8("予兆の発光強度"), &jumpTelegraphEmissiveIntensity, 0.05f, 0.0f, 20.0f, "%.2f");
			ImGui::DragFloat(U8("内側予兆の開始スケール"), &jumpTelegraphInnerStartScale, 0.01f, 0.01f, 10.0f, "%.2f");
			ImGui::Checkbox(U8("予兆の回転を有効化"), &jumpTelegraphEnableRotation);
			ImGui::DragFloat(U8("外側予兆の回転速度（度/秒）"), &jumpTelegraphOuterRotationSpeed, 1.0f, -360.0f, 360.0f, "%.1f deg/sec");
			ImGui::DragFloat(U8("内側予兆の回転速度（度/秒）"), &jumpTelegraphInnerRotationSpeed, 1.0f, -360.0f, 360.0f, "%.1f deg/sec");
			ImGui::Checkbox(U8("着地時のフラッシュを有効化"), &jumpTelegraphEnableImpactFlash);
			ImGui::DragFloat(U8("着地フラッシュの持続時間（秒）"), &jumpTelegraphImpactFlashDuration, 0.01f, 0.01f, 1.0f, "%.2f sec");
			ImGui::DragFloat(U8("着地フラッシュの強度"), &jumpTelegraphImpactFlashIntensity, 0.05f, 0.0f, 2.0f, "%.2f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("武器発光・攻撃予兆")))
		{
			ImGui::SeparatorText(U8("武器の発光設定（実行時上書き）"));
			bool weaponEmissiveChanged = false;
			weaponEmissiveChanged |= ImGui::Checkbox(U8("武器の発光を有効化"), &weaponEmissiveEnabled);
			weaponEmissiveChanged |= ImGui::ColorEdit3(U8("武器の発光色"), &weaponEmissiveColor.x);
			weaponEmissiveChanged |= ImGui::DragFloat(U8("武器の発光強度"), &weaponEmissiveIntensity, 0.05f, 0.0f, 50.0f, "%.2f");

			if (weaponEmissiveChanged && skeletalMeshComponent)
			{
				skeletalMeshComponent->SetRuntimeMaterialEmissive("M_Grux_Qilin_Weapon", weaponEmissiveEnabled, weaponEmissiveColor, weaponEmissiveIntensity);
			}

			ImGui::ColorEdit3(U8("武器攻撃予兆の輪郭色"), &weaponTelegraphRimColor.x);
			ImGui::DragFloat(U8("武器攻撃予兆のフェードアウト時間（秒）"), &weaponTelegraphFadeOutDuration, 0.01f, 0.01f, 1.0f, "%.2f sec");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("ダッシュ予兆")))
		{
			ImGui::Checkbox(U8("ダッシュ予兆の発光を有効化"), &dashTelegraphEmissiveEnabled);
			ImGui::ColorEdit3(U8("第一形態 ダッシュ予兆の発光色"), &dashTelegraphPhase1EmissiveColor.x);
			ImGui::ColorEdit3(U8("第二形態 ダッシュ予兆の発光色"), &dashTelegraphPhase2EmissiveColor.x);
			ImGui::DragFloat(U8("ダッシュ予兆の発光強度"), &dashTelegraphEmissiveIntensity, 0.05f, 0.0f, 20.0f, "%.2f");

			ImGui::DragFloat(U8("ライン予兆の拡大時間（秒）"), &dashTelegraphLineExpandDuration, 0.005f, 0.001f, 1.0f, "%.3f sec");
			ImGui::DragFloat(U8("ライン予兆のフェードアウト時間（秒）"), &dashTelegraphLineFadeOutDuration, 0.005f, 0.001f, 1.0f, "%.3f sec");
			ImGui::DragFloat(U8("扇形予兆のフェードイン時間（秒）"), &dashTelegraphFanFadeInDuration, 0.005f, 0.001f, 1.0f, "%.3f sec");
			ImGui::DragFloat(U8("扇形予兆のフラッシュ時間（秒）"), &dashTelegraphFanFlashDuration, 0.005f, 0.001f, 1.0f, "%.3f sec");
			ImGui::DragFloat(U8("扇形予兆のフラッシュ強度"), &dashTelegraphFanFlashIntensity, 0.05f, 0.0f, 8.0f, "%.2f");
			ImGui::DragFloat(U8("扇形予兆のフェードアウト時間（秒）"), &dashTelegraphFanFadeOutDuration, 0.005f, 0.001f, 1.0f, "%.3f sec");
			ImGui::DragFloat(U8("ライン予兆の幅（距離）"), &dashTelegraphLineWidth, 0.01f, 0.01f, 8.0f, "%.3f m");
			ImGui::DragFloat(U8("扇形予兆の半径（距離）"), &dashTelegraphFanRadius, 0.05f, 0.01f, 20.0f, "%.2f m");
			ImGui::DragFloat(U8("ライン予兆の高さオフセット（距離）"), &dashTelegraphLineYOffset, 0.001f, -2.0f, 2.0f, "%.3f m");
			ImGui::DragFloat(U8("扇形予兆の高さオフセット（距離）"), &dashTelegraphFanYOffset, 0.001f, -2.0f, 2.0f, "%.3f m");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("移動アニメーション")))
		{
			ImGui::DragFloat(U8("咆哮開始時の足元補正（距離）"), &roarLevelStartFootOffset, 0.01f, -2.0f, 2.0f, "%.3f m");
			ImGui::DragFloat(U8("咆哮の足元補正終了時間（秒）"), &roarLevelStartFootOffsetEndTime, 0.01f, 0.0f, 5.0f, "%.2f sec");
			ImGui::DragFloat(U8("移動開始速度の閾値（距離/秒）"), &positioningMoveStartSpeedThreshold, 0.01f, 0.01f, 10.0f, "%.2f m/s");
			ImGui::DragFloat(U8("移動停止速度の閾値（距離/秒）"), &positioningMoveStopSpeedThreshold, 0.01f, 0.0f, positioningMoveStartSpeedThreshold - 0.01f, "%.2f m/s");
			ImGui::DragFloat(U8("移動停止までの遅延時間（秒）"), &positioningMoveStopDelay, 0.01f, 0.0f, 2.0f, "%.2f sec");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("被弾・ヒットエフェクト")))
		{
			ImGui::DragFloat(U8("ラッシュのヒット位置からのオフセット（距離）"), &rushHitEffectSurfaceOffset, 0.005f, 0.0f, 0.20f, "%.3f m");
			ImGui::DragFloat(U8("ボス被ダメージ時のフラッシュ時間（秒）"), &damageFlashDuration, 0.01f, 0.01f, 2.0f, "%.2f sec");
			ImGui::SliderFloat(U8("ボス被ダメージ時のフラッシュ強度"), &damageFlashStartValue, 0.0f, 1.0f, "%.2f");
			ImGui::DragFloat(U8("敵へのヒットエフェクト 高さオフセット"), &hitEnemyEffectOffsetY, 0.1f);
			ImGui::DragFloat(U8("プレイヤーへのヒットエフェクト 高さオフセット"), &hitPlayerEffectOffsetY, 0.1f);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("プレイヤーに攻撃された時のコントローラー振動")))
		{
			ImGui::DragFloat(U8("通常攻撃 振動強度"), &normalHitRumbleStrength, 0.01f, 0.0f, 3.0f);
			ImGui::DragFloat(U8("通常攻撃 振動時間（秒）"), &normalHitRumbleDuration, 0.005f, 0.0f, 3.0f, "%.3f sec");
			ImGui::DragFloat(U8("コンボ最終段 振動強度"), &comboFinalHitRumbleStrength, 0.01f, 0.0f, 3.0f);
			ImGui::DragFloat(U8("コンボ最終段 振動時間（秒）"), &comboFinalHitRumbleDuration, 0.005f, 0.0f, 3.0f, "%.3f sec");
			ImGui::DragFloat(U8("ラッシュ攻撃 振動強度"), &rushHitRumbleStrength, 0.01f, 0.0f, 3.0f);
			ImGui::DragFloat(U8("ラッシュ攻撃 振動時間（秒）"), &rushHitRumbleDuration, 0.005f, 0.0f, 3.0f, "%.3f sec");
			ImGui::DragFloat(U8("ラッシュ最終段 振動強度"), &rushFinalHitRumbleStrength, 0.01f, 0.0f, 3.0f);
			ImGui::DragFloat(U8("ラッシュ最終段 振動時間（秒）"), &rushFinalHitRumbleDuration, 0.005f, 0.0f, 3.0f, "%.3f sec");
			ImGui::TreePop();
		}

	}

	// カテゴリの開閉に関係なく、従来のフェーズ条件で非表示化する。
	if (dashBTPhase != DashBTPhase::Telegraph && dashBTPhase != DashBTPhase::InterDashTransition)
		HideDashTelegraphVisual();

	if (ImGui::CollapsingHeader(U8("UI調整")))
	{
		if (ImGui::TreeNode(U8("ボス体力")))
		{
			ImGui::DragFloat(U8("遅延体力の追従開始までの時間（秒）"), &delayedHpDelayDuration, 0.05f, 0.0f, 10.0f, "%.2f sec");
			ImGui::DragFloat(U8("遅延体力の追従速度（秒）"), &delayedHpFollowSpeed, 0.05f, 0.0f, 25.0f, "%.2f sec");
			ImGui::DragFloat(U8("ラッシュ時の遅延体力の追従速度（秒）"), &delayedHpRushFollowSpeed, 0.05f, 0.0f, 25.0f, "%.2f sec");
			ImGui::ColorEdit4(U8("現在体力の色"), &bossHpCurrentColor.r);
			ImGui::ColorEdit4(U8("遅延体力の色"), &bossHpDelayedColor.r);
			ImGui::TreePop();
		}
	}
	// Roar の補正・非表示化はカテゴリの開閉によらず、Character より前に実行する。
	if (!std::isfinite(roarLevelStartFootOffset)) roarLevelStartFootOffset = 0.0f;
	defensiveBackMinAngle = std::clamp(defensiveBackMinAngle, 0.0f, 180.0f);
	if (!roarFloatingDebrisEnabled)
		HideRoarFloatingDebris();
	if (!roarImpactDebrisEnabled)
		HideRoarImpactDebris();
	Character::DrawImGuiDetails();

	ImGui::Text(U8("行動ツリーの前回ノード: %s"), behaviorTreePreviousNode.c_str());

#endif
}

void GruxEnemy::PlayBodyAnimation(const std::string& name, const bool loop, const bool blend, const float blendTime, const bool ignoreRootMotion) const
{
	if (phase2CinematicAnimationOwnedExternally)
		return;
	Character::PlayBodyAnimation(name, loop, blend, blendTime, ignoreRootMotion);
}

void GruxEnemy::PlayPhase2CinematicBodyAnimation(const std::string& name, const bool loop, const bool blend, const float blendTime, const bool ignoreRootMotion) const
{
	Character::PlayBodyAnimation(name, loop, blend, blendTime, ignoreRootMotion);
}

void GruxEnemy::TakeDamageFromPlayerAttack(const int damage, const bool isNormalFourthHit, const PlayerAttackHitType hitType, const DirectX::XMFLOAT3& hitSourcePosition)
{
	if (finalHitReactionActive) return;
	skeletalMeshComponent->plusAlphaCBuffer->data.flashValue = damageFlashStartValue;
	// カメラシェイク
	if (auto camera = GetOwnerScene()->GetActorManager()->GetActorOfType<DarkCameraActor>())
	{
		std::string presetName = "BossRoar";
		camera->PlayCameraShakePreset(presetName);
	}

	CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_damage.wav", 0.3f);

	const int hpBeforeDamage = hp;
	hp -= damage;
	if (hp < hpBeforeDamage)
	{
		float rumbleStrength = normalHitRumbleStrength;
		float rumbleDuration = normalHitRumbleDuration;
		switch (hitType)
		{
		case PlayerAttackHitType::ComboFinal:
			rumbleStrength = comboFinalHitRumbleStrength;
			rumbleDuration = comboFinalHitRumbleDuration;
			break;
		case PlayerAttackHitType::Rush:
			rumbleStrength = rushHitRumbleStrength;
			rumbleDuration = rushHitRumbleDuration;
			break;
		case PlayerAttackHitType::RushFinal:
			rumbleStrength = rushFinalHitRumbleStrength;
			rumbleDuration = rushFinalHitRumbleDuration;
			break;
		case PlayerAttackHitType::Normal:
		default:
			break;
		}
		InputSystem::SetVibration(rumbleStrength, rumbleDuration);
	}
	if (const auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene());
		gameScene && gameScene->GetBossPhase() == GameScene::BossPhase::Phase1)
	{
		hp = (std::max)(hp, 0);
		if (!IsPhase2HpBarPresentationActive() && hpCurrentFillUiComponent)
			hpCurrentFillUiComponent->SetValue(static_cast<float>(hp), static_cast<float>(maxHp));
	}
	if (hp > 0 && hitVoiceCooldownTimer <= 0.0f)
	{
		static constexpr std::array<const char*, 4> hitVoicePaths =
		{
			"./Data/Sound/SE/boss_hit_voice1.wav",
			"./Data/Sound/SE/boss_hit_voice2.wav",
			"./Data/Sound/SE/boss_hit_voice3.wav",
			"./Data/Sound/SE/boss_hit_voice4.wav",
		};

		int voiceIndex = lastHitVoiceIndex < 0
			? MathHelper::RandomRange(0, static_cast<int>(hitVoicePaths.size()) - 1)
			: MathHelper::RandomRange(0, static_cast<int>(hitVoicePaths.size()) - 2);
		if (lastHitVoiceIndex >= 0 && voiceIndex >= lastHitVoiceIndex)
			++voiceIndex;


		if (CoreAudio::PlayOneShot(hitVoicePaths[voiceIndex], 0.3f))
		{
			lastHitVoiceIndex = voiceIndex;
			hitVoiceCooldownTimer = 0.5f;// 被弾ボイスの再生間隔。
		}
	}
	if (!rushHpDisplayActive && hp < hpBeforeDamage)
	{
		delayedHp = static_cast<float>(hpBeforeDamage);
		delayedHpDelayTimer = delayedHpDelayDuration;
		useRushDelayedHpFollowSpeed = false;
	}
	Logger::Log(U8("エネミーにダメージ！ HP:") + std::to_string(hp));
	if (isNormalFourthHit && hp > 0)
	{
		static thread_local std::mt19937 randomEngine{ std::random_device{}() };
		std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
		float fourthHitReactionLastRoll = distribution(randomEngine);
		bool fourthHitReactionLastWon = fourthHitReactionLastRoll < std::clamp(fourthHitReactionChance, 0.0f, 1.0f);
		if (fourthHitReactionLastWon && !IsStunned())
			BeginFourthHitReaction(hitSourcePosition);
	}
}

void GruxEnemy::SetHpBarVisible(const bool visible)
{
	hpBarFadeEntries.clear();
	for (const auto& component : hpBarUiComponents)
	{
		if (!component)
			continue;
		component->SetVisible(visible);
		component->SetEnable(visible);
	}
}

void GruxEnemy::BeginHpBarFadeOut()
{
	if (IsPhase2HpBarPresentationActive())
		ResetPhase2HpBarPresentation();

	hpBarFadeEntries.clear();
	for (const auto& core : hpBarUiComponents)
	{
		auto image = std::dynamic_pointer_cast<UIImageComponent>(core);
		if (image && image->IsVisible())
			hpBarFadeEntries.push_back({ image, image->color });
	}
}
void GruxEnemy::SetHpBarFadeAlpha(const float alpha)
{
	const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
	for (auto& entry : hpBarFadeEntries)
	{
		CoreColor color = entry.color;
		color.a *= clampedAlpha;
		entry.component->SetColor(color);
	}
	if (clampedAlpha <= 0.0f)
	{
		for (auto& entry : hpBarFadeEntries)
			entry.component->SetColor(entry.color);
		SetHpBarVisible(false);
	}
}

void GruxEnemy::BeginPhase2HpBarPresentation()
{
	if (!IsPhase2HpBarPresentationActive())
		phase2HpBarPresentationRestoreVisible = hpCurrentFillUiComponent && hpCurrentFillUiComponent->IsVisible();
	phase2HpBarPresentationState = Phase2HpBarPresentationState::RevealingFrame;
	phase2HpBarPresentationElapsed = 0.0f;
	phase2HpBarPresentationFrameProgress = 0.0f;
	phase2HpBarPresentationFillProgress = 0.0f;
	ApplyHpBarVisualProfile(HpBarVisualProfile::Phase2);
	SetHpBarVisible(true);
	UpdatePhase2HpBarPresentationLayout();
}

void GruxEnemy::CompletePhase2HpBarPresentation()
{
	phase2HpBarPresentationState = Phase2HpBarPresentationState::Hidden;
	phase2HpBarPresentationElapsed = 0.0f;
	phase2HpBarPresentationFrameProgress = 1.0f;
	phase2HpBarPresentationFillProgress = 1.0f;
	ApplyHpBarVisualProfile(HpBarVisualProfile::Phase2);
	SetHpBarVisible(true);
	phase2HpBarPresentationRestoreVisible = false;
}

void GruxEnemy::SetHpBarVisualProfileForBattlePhase(const bool phase2)
{
	phase2HpBarPresentationState = Phase2HpBarPresentationState::Hidden;
	phase2HpBarPresentationElapsed = 0.0f;
	phase2HpBarPresentationFrameProgress = phase2 ? 1.0f : 0.0f;
	phase2HpBarPresentationFillProgress = phase2 ? 1.0f : 0.0f;
	phase2HpBarPresentationRestoreVisible = false;
	ApplyHpBarVisualProfile(phase2 ? HpBarVisualProfile::Phase2 : HpBarVisualProfile::Phase1);
	const float maximumHp = static_cast<float>((std::max)(maxHp, 1));
	if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetValue(static_cast<float>((std::max)(hp, 0)), maximumHp);
	if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetValue(delayedHp, maximumHp);
}

void GruxEnemy::StopBattleActions()
{
	// 中断中の状態と復帰情報を解除する。
	rushCombatHoldActive = false;
	interruptedChargeRecoveryPending = false;
	interruptedChargeRecoveryActive = false;
	interruptedChargeRecoveryTimer = 0.0f;
	interruptedChargeRecoveryDuration = 0.0f;

	// TelegraphとRoar演出を停止する。
	ResetWeaponTelegraphRimLight();
	HideDashTelegraphVisual();
	HideRoarTelegraph();
	HideRoarFloatingDebris();
	HideRoarImpactDebris();

	// 実行中のBTアクションをCleanupする。
	if (IsRoarBTActive())
	{
		CleanupRoarBT();
		activeNode = nullptr;
		if (behaviorData) behaviorData->Init();
	}
	if (IsChargeAttackBTActive())
	{
		CleanupChargeAttackBT();
		activeNode = nullptr;
		if (behaviorData) behaviorData->Init();
	}
	if (IsDashAttackBTActive())
	{
		CleanupDashAttackBT();
		activeNode = nullptr;
		if (behaviorData) behaviorData->Init();
	}

	// 攻撃判定・移動・ターゲットを停止する。
	DisableAttackHitBoxes();
	StopDashAttackMovement();
	StopChargeAttackMovement();
	StopAIMovement();
	ClearAttackSetupTarget();

	// 非攻撃のPositioning目標も破棄する。
	ClearPositioningTarget(retreatTarget, retreatMovementRuntime);
	ClearPositioningTarget(repositionTarget, repositionMovementRuntime);
	ClearJumpAttackMotionWarpOverride();
	ClearFastComboStepIn();
	justDodgedActors.clear();

	// 攻撃選択・履歴・演出状態を初期化する。
	beginHuskParticleRequest = false;
	selectedActionType = BossActionType::AttackLA;
	selectedAttackType = BossAttackType::PrimaryAttackLA;
	lastStartedCombatAttack.reset();
	secondLastStartedCombatAttack.reset();
	positioningMoveStopTimer = 0.0f;
	positioningAnimationMoving = false;
	transitionWindow = false;
	hitActors.clear();
	currentAttackHitCount = 0;

	// TrailとRecovery・計測値を初期化する。
	showLeftWeaponTrail = false;
	showRightWeaponTrail = false;
	leftWeaponTrail.Clear();
	rightWeaponTrail.Clear();
	nextRecoveryDuration.reset();
	chargeElapsedTime = 0.0f;
	dashAttackElapsedTime = 0.0f;
	animationMotionWarps.clear();

	// MovementComponentへの入力と速度を停止する。
	characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
	characterMovementComponent->SetInputMagnitude(0.0f);
	characterMovementComponent->SetFrameAdditionalVelocity({ 0.0f, 0.0f, 0.0f });
	characterMovementComponent->AddForcedMove({ 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f);
	characterMovementComponent->ResetFixedSpeed();
	velocity = { 0.0f, 0.0f, 0.0f };
}

void GruxEnemy::AbortBehaviorTreeForDeath()
{
	battleAIActive = false;
	ResetBehaviorTreeRuntime();
}

void GruxEnemy::StopBossDeathCinematicOneShots()
{
	for (const auto& audio : bossDeathCinematicOneShots)
	{
		if (audio)
			audio->Stop(false);
	}
	bossDeathCinematicOneShots.clear();
}

void GruxEnemy::BeginPhase1LastHitReaction(const DirectX::XMFLOAT3& hitSourcePosition)
{
	using namespace DirectX;

	const XMFLOAT3 bossPosition = GetPosition();
	const XMFLOAT4 bossRotation = GetQuaternionRotation();
	XMFLOAT3 hitDirection = { hitSourcePosition.x - bossPosition.x, 0.0f,hitSourcePosition.z - bossPosition.z };
	XMFLOAT3 localDirection = {};
	XMStoreFloat3(&localDirection, XMVector3Rotate(XMLoadFloat3(&hitDirection), XMQuaternionInverse(XMLoadFloat4(&bossRotation))));

	const char* animationName = "Hit_Combat_Large_F";
	if (localDirection.x * localDirection.x + localDirection.z * localDirection.z >= FLT_EPSILON)
	{
		if (std::abs(localDirection.z) > std::abs(localDirection.x))
			animationName = localDirection.z > 0.0f ? "Hit_Combat_Large_F" : "Hit_Combat_Large_B";
		else
			animationName = localDirection.x >= 0.0f ? "Hit_Combat_Large_R" : "Hit_Combat_Large_L";
	}

	StopBattleActions();
	if (const auto controller = GetBodyAnimationController())
		controller->SetRemoveRootTranslationFromPose(animationName, true);
	PlayBodyAnimation(animationName, false, true, 0.1f, true);
}

void GruxEnemy::BeginFinalHitReaction(const std::string& animationName)
{
	if (isDeathPerform)
		return;

	EndRushHpDisplay();
	finalHitReactionActive = true;
	finalHitReactionHeld = false;
	BeginDeathPresentation();
	ResetTimeScale();
	if (const auto controller = GetBodyAnimationController())
	{
		controller->ResetAnimationRate();
		controller->SetRemoveRootTranslationFromPose(animationName, true);
	}
	PlayBodyAnimation(animationName, false, true, 0.1f, true);
}

void GruxEnemy::BeginDeathPresentation()
{
	if (isDeathPerform)
		return;

	isDeathPerform = true;
	StopBattleActions();
	AbortBehaviorTreeForDeath();
	if (cinematicDeathAnimationOwnedExternally || finalHitReactionActive || finalHitReactionHeld)
		return;

	const bool ignoreRootMotion = deathAnimationName == "Knock_Down_Death";
	PlayBodyAnimation(deathAnimationName, false, true, 0.1f, ignoreRootMotion);
}
void GruxEnemy::EndFinalHitReaction()
{
	if (finalHitReactionActive)
		finalHitReactionHeld = true;
	finalHitReactionActive = false;
}

void GruxEnemy::PauseBattleAI()
{
	battleAIActive = false;
	ResetFourthHitReaction();
	StopBattleActions();

	activeNode = nullptr;
	if (behaviorData) behaviorData->Init();
	if (aiTree) aiTree->ResetActionRuntimes();
	behaviorTreePreviousNode = "None";
	EnterBattleIdlePresentation();

}

void GruxEnemy::EnterBattleIdlePresentation()
{
	if (IsDeathPresentationActive() || phase2CinematicAnimationOwnedExternally ||
		IsChargeAttackBTActive())
		return;
	PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.3f, false);
}

void GruxEnemy::PauseBattleAIForPhaseTransition()
{
	phase2CinematicAnimationOwnedExternally = true;
	battleAIActive = false;
	ResetFourthHitReaction();
	StopBattleActions();
	ResetBehaviorTreeRuntime();
	behaviorTreePreviousNode = "None";
}

void GruxEnemy::SetDirectionImmediate(const DirectX::XMFLOAT3& direction) const
{
	if (rotationComponent)
		rotationComponent->SetDirectionImmediate(direction);
}

void GruxEnemy::ResumeBattleAI()
{
	phase2CinematicAnimationOwnedExternally = false;
	battleAIActive = true;
}

void GruxEnemy::ForcePhase2CinematicIdleImmediate() const
{
	if (const auto controller = GetBodyAnimationController())
	{
		controller->PlayAnimationImmediate("TravelMode_Idle_0", true, true);
	}
}

void GruxEnemy::ResetForBattleContinue(const Transform& battleStartTransform)
{
	if (IsPhase2HpBarPresentationActive())
		ResetPhase2HpBarPresentation();
	phase2CinematicAnimationOwnedExternally = false;
	EndFinalHitReaction();
	finalHitReactionHeld = false;
	ResetTimeScale();
	StopBattleActions();
	ResetBehaviorTreeForBattleRestart();
	ResetCombatRuntimeForBattleRestart();

	isDeathPerform = false;
	rushHpDisplayActive = false;
	useRushDelayedHpFollowSpeed = false;
	delayedHp = static_cast<float>((std::max)(hp, 0));
	delayedHpDelayTimer = 0.0f;
	if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
	if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
	EnterBattleIdlePresentation();

	SetPosition(battleStartTransform.GetLocation());
	SetQuaternionRotation(battleStartTransform.GetRotation());
	SetScale(battleStartTransform.GetScale());
	UpdateAllComponentTransforms();
}

void GruxEnemy::ResetBehaviorTreeForBattleRestart()
{
	ResetBehaviorTreeRuntime();
	behaviorTreePreviousNode = "None";
	consecutiveAttackCount = 0;
	repositionDecisionCached = false;
	repositionDecisionResult = false;
}

void GruxEnemy::ResetCombatRuntimeForBattleRestart()
{
	HideDashTelegraphVisual();
	ResetFourthHitReaction();
	ClearAttackSetupTarget();
	ClearPositioningTarget(retreatTarget, retreatMovementRuntime);
	ClearPositioningTarget(repositionTarget, repositionMovementRuntime);

	combatActionCooldownRemaining.fill(0.0f);
	selectedAttackType = BossAttackType::PrimaryAttackLA;
	selectedActionType = BossActionType::AttackLA;
	roarBT = {};
	roarCooldownRemaining = 0.0f;
	retreatRetryCooldownRemaining = 0.0f;
	repositionCooldownRemaining = 0.0f;
	repositionRetryCooldownRemaining = 0.0f;
	initialRepositionPending = true;
	initialRepositionFallbackIdlePending = false;
	fastComboTargetContext = {};
	fastComboStageTargetContexts = {};
	fastComboRuntimeStage = -1;
	ClearFastComboStepIn();
	fastComboApproachRetryRemaining = 0.0f;
	currentAttackSequenceId = 0;
	currentAttackHitCount = 0;
	hitActors.clear();
	justDodgedActors.clear();
	DisableAttackHitBoxes();
	animationMotionWarps.clear();
	jumpMotionWarpOverrideActive = false;
	jumpAttackStartPlayerPosition = {};
	jumpMotionWarpDirection = { 0.0f, 0.0f, 1.0f };
	calculatedJumpDistance = 0.0f;
	nextRecoveryDuration.reset();

	dashBTPhase = DashBTPhase::None;
	dashBTAttackStarted = false;
	dashBTPreviousAction = BossActionType::AttackLA;
	dashBTDashIndex = 0;
	dashBTDashMaxCount = 1;
	dashBTCurrentDashHitStartCount = 0;
	dashBTAbortRemainingDashes = false;
	dashBTTransitionElapsed = 0.0f;
	dashBTDirectionLocked = false;
	dashBTMovementSnapshotPrepared = false;
	dashBTPredictedKnockupPosition = {};
	dashAttackDirection = {};
	dashAttackStartPosition = {};
	dashTargetPosition = {};
	dashAttackElapsedTime = 0.0f;

	dashAttackMovementActive = false;
	interruptedChargeRecoveryPending = false;
	interruptedChargeRecoveryActive = false;
	interruptedChargeRecoveryTimer = 0.0f;
	interruptedChargeRecoveryDuration = 0.0f;
	chargeBT = {};
	chargeMovementActive = false;
	chargeDangerWindowActive = false;
	chargeElapsedTime = 0.0f;

	StopAIMovement();
	EndPositioningAnimation();
	if (characterMovementComponent)
	{
		characterMovementComponent->SetMoveDirection({ 0.0f,0.0f,0.0f });
		characterMovementComponent->SetInputMagnitude(0.0f);
		characterMovementComponent->SetFrameAdditionalVelocity({ 0.0f,0.0f,0.0f });
		characterMovementComponent->ResetFixedSpeed();
	}
}

void GruxEnemy::ResetForBattleRestart(const Transform& battleStartTransform)
{
	ResetForBattleContinue(battleStartTransform);
	if (const auto controller = GetBodyAnimationController())
	{
		controller->PlayAnimationImmediate("TravelMode_Idle_0", true, true);
	}
	hp = maxHp;
	delayedHp = static_cast<float>(hp);
	if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
	if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
}

void GruxEnemy::SetBattleHp(const int currentHp, const int maximumHp)
{
	if (IsPhase2HpBarPresentationActive())
		CompletePhase2HpBarPresentation();
	maxHp = (std::max)(maximumHp, 1);
	hp = std::clamp(currentHp, 0, maxHp);
	delayedHp = static_cast<float>(hp);
	delayedHpDelayTimer = 0.0f;
	rushHpDisplayActive = false;
	useRushDelayedHpFollowSpeed = false;
	if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
	if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
}

void GruxEnemy::SetPhaseTransformProgress(const float progress)
{
	phaseTransformProgress = std::clamp(progress, 0.0f, 1.0f);
	enemyScale = std::lerp(phase1BossScale, phase2BossScale, phaseTransformProgress);
	SetScale({ enemyScale, enemyScale, enemyScale });
	UpdatePhase2TextureBlendWorldYRange();
	if (skeletalMeshComponent)
		skeletalMeshComponent->SetPhase2TextureBlendProgress(phaseTransformProgress);
	UpdateAllComponentTransforms();
}

// フェーズ遷移時の遅延HP表示を進める。
void GruxEnemy::AdvanceDelayedHpBarForPhaseTransition()
{
	const float currentHp = static_cast<float>((std::max)(hp, 0));
	const float uiDeltaTime = Time::UnscaledDeltaTime();
	if (!rushHpDisplayActive && delayedHp > currentHp)
	{
		if (delayedHpDelayTimer > 0.0f)
			delayedHpDelayTimer = (std::max)(0.0f, delayedHpDelayTimer - uiDeltaTime);
		else
		{
			const float followSpeed = useRushDelayedHpFollowSpeed ? delayedHpRushFollowSpeed : delayedHpFollowSpeed;
			delayedHp = (std::max)(currentHp, delayedHp - followSpeed * uiDeltaTime);
			if (delayedHp <= currentHp)
				useRushDelayedHpFollowSpeed = false;
		}
	}
	if (!IsPhase2HpBarPresentationActive())
	{
		if (hpCurrentFillUiComponent)
			hpCurrentFillUiComponent->SetValue(currentHp, static_cast<float>(maxHp));
		if (hpDelayedFillUiComponent)
			hpDelayedFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
	}
}

bool GruxEnemy::IsDelayedHpSettled() const
{
	return !rushHpDisplayActive && delayedHp <= 0.001f;
}

void GruxEnemy::ClearDamageFlash() const
{
	if (skeletalMeshComponent && skeletalMeshComponent->plusAlphaCBuffer)
		skeletalMeshComponent->plusAlphaCBuffer->data.flashValue = 0.0f;
}
void GruxEnemy::ClearDamageVisualsForPhaseTransition() const
{
	ClearDamageFlash();
}

void GruxEnemy::BeginRushHpDisplay()
{
	rushHpDisplayActive = true;
	delayedHp = static_cast<float>((std::max)(hp, 0));
	delayedHpDelayTimer = 0.0f;
}

void GruxEnemy::EndRushHpDisplay()
{
	if (!rushHpDisplayActive)
		return;

	rushHpDisplayActive = false;
	useRushDelayedHpFollowSpeed = true;
	delayedHpDelayTimer = delayedHpDelayDuration;
}
// ヒットエフェクトを生成する
void GruxEnemy::SpawnHitEffect(const DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 playerPos) const
{
	DirectX::XMFLOAT3 enemyCenter = GetPosition();
	enemyCenter.y += hitEnemyEffectOffsetY;
	playerPos.y += hitPlayerEffectOffsetY;

	// 敵→プレイヤー方向
	DirectX::XMFLOAT3 forward = MathHelper::Normalize(MathHelper::Subtract(playerPos, enemyCenter));
	// エフェクト生成位置
	float spawnOffset = 0.8f;
	DirectX::XMFLOAT3 spawnPos = MathHelper::Add(enemyCenter, MathHelper::Multiply(forward, spawnOffset));
	spawnPos = hitPos;

	if (hitSwordEffectComponent)
	{
		hitSwordEffectComponent->SetWorldLocationDirect(spawnPos);
		hitSwordEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(
			hitSwordEffectComponent->GetEffectHandle(),
			hitSwordEffectComponent->GetComponentLocation(),
			{ 0.0f, 0.0f, 0.0f });
	}

}

void GruxEnemy::SpawnRushHitRing(const DirectX::XMFLOAT3 hitPos,
	DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 playerPos,
	bool hasHitPosition, bool hasHitNormal) const
{
	const auto isFiniteVector = [](const DirectX::XMFLOAT3& value)
		{
			return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
		};
	const float normalLengthSq = hitNormal.x * hitNormal.x +
		hitNormal.y * hitNormal.y + hitNormal.z * hitNormal.z;
	const bool useSurfaceSpawn = hasHitPosition && hasHitNormal &&
		isFiniteVector(hitPos) && isFiniteVector(hitNormal) && normalLengthSq > 0.000001f;

	DirectX::XMFLOAT3 rushEffectPosition{};
	if (useSurfaceSpawn)
	{
		const float inverseNormalLength = 1.0f / std::sqrt(normalLengthSq);
		hitNormal = MathHelper::Multiply(hitNormal, inverseNormalLength);
		rushEffectPosition = MathHelper::Add(hitPos,
			MathHelper::Multiply(hitNormal,
				std::clamp(rushHitEffectSurfaceOffset, 0.0f, 0.20f)));
	}
	else
	{
		DirectX::XMFLOAT3 enemyCenter = GetPosition();
		constexpr float rushEffectHeightOffset = 1.3f;
		enemyCenter.y += rushEffectHeightOffset;
		playerPos.y += rushEffectHeightOffset;

		const DirectX::XMFLOAT3 forward = MathHelper::Normalize(MathHelper::Subtract(playerPos, enemyCenter));
		rushEffectPosition = MathHelper::Add(enemyCenter, MathHelper::Multiply(forward, 0.8f));
		rushEffectPosition.x += MathHelper::RandomRange(-0.45f, 0.45f);
		rushEffectPosition.y += MathHelper::RandomRange(-0.20f, 0.80f);
		rushEffectPosition.z += MathHelper::RandomRange(-0.45f, 0.45f);
	}


	if (rushHitSparkEffectComponent)
	{
		rushHitSparkEffectComponent->SetWorldLocationDirect(rushEffectPosition);
		rushHitSparkEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(
			rushHitSparkEffectComponent->GetEffectHandle(),
			rushHitSparkEffectComponent->GetComponentLocation(),
			{ 0.0f, 0.0f, 0.0f });
	}

	if (!rushHitRingEffectComponent)
		return;

	rushHitRingEffectComponent->SetWorldLocationDirect(rushEffectPosition);
	rushHitRingEffectComponent->UpdateComponentToWorld();
	EffectManager::EmitParticle(
		rushHitRingEffectComponent->GetEffectHandle(),
		rushHitRingEffectComponent->GetComponentLocation(),
		rushHitRingEffectComponent->GetComponentEulerRotation());
}

void GruxEnemy::OnAnimationNotifyBegin(const AnimationNotifyState& state)
{
	if (const auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene());
		gameScene && (gameScene->IsPhase1BreakPending() || gameScene->IsBossPhaseTransitionActive())) return;
	if (finalHitReactionActive || fourthHitReactionActive) return;
	switch (state.type)
	{
	case AnimationNotifyState::Type::HitBox:
		activeHitBoxNotifyStates.push_back(&state);
		if (state.parameter == rightWeapon || state.parameter == bothWeapon)
		{
			Logger::Log(U8("右の当たり判定を開始しました"));
			const WeaponHitBoxPoints points = BuildWeaponHitBoxPoints(weaponRightRootComponent, weaponRightMiddleComponent, weaponRightTipComponent, state.hitBoxOffset);
			prevWeaponRightRootPos = points.root;
			prevWeaponRightMidPos = points.middle;
			prevWeaponRightTipPos = points.tip;
			activeRightHitBoxRadius = (state.hitBoxRadius < 0.01f ? 0.01f : state.hitBoxRadius);
			activeRightHitBoxOffset = state.hitBoxOffset;
			rightHitBox = true;
		}
		if (state.parameter == leftWeapon || state.parameter == bothWeapon)
		{
			Logger::Log(U8("左の当たり判定を開始しました"));
			const WeaponHitBoxPoints points = BuildWeaponHitBoxPoints(weaponLeftRootComponent, weaponLeftMiddleComponent, weaponLeftTipComponent, state.hitBoxOffset);
			prevWeaponLeftRootPos = points.root;
			prevWeaponLeftMidPos = points.middle;
			prevWeaponLeftTipPos = points.tip;
			activeLeftHitBoxRadius = (state.hitBoxRadius < 0.01f ? 0.01f : state.hitBoxRadius);
			activeLeftHitBoxOffset = state.hitBoxOffset;
			leftHitBox = true;
		}
		RefreshActiveHitBoxesFromNotifyStates();
		break;
	case AnimationNotifyState::Type::InputWindow:
		Logger::Log(U8("コンボ受付を開始しました"));
		break;
	case AnimationNotifyState::Type::Invincible:
		break;
	case AnimationNotifyState::Type::TransitionWindow:
		transitionWindow = true;
		Logger::Log(U8("遷移許可区間を開始しました"));
		break;
	case AnimationNotifyState::Type::JustDodgeWindow:
		break;
	case AnimationNotifyState::Type::DangerWindow:
		activeDangerNotifyState = &state;
		isDangerWindow = true;
		RefreshActiveDangerAreaFromNotify();
		break;
	case AnimationNotifyState::Type::ShowTrail:
		if (state.parameter == leftWeapon || state.parameter == bothWeapon)
			showLeftWeaponTrail = true;
		if (state.parameter == rightWeapon || state.parameter == bothWeapon)
			showRightWeaponTrail = true;
		break;
	case AnimationNotifyState::Type::ShowEmissive:
		break;
	case AnimationNotifyState::Type::MotionWarp:
	{
		if (IsFastComboMotionWarpNotify(state))
		{
			BeginFastComboStepIn(fastComboRuntimeStage, state.startTime, state.endTime);
			break;
		}
		// アニメーション側で指定したローカル方向
		DirectX::XMFLOAT3 localDirection = state.moveDirection;
		// プレイヤーの向いている方向を考慮してワールド方向へ変換
		float radianAngle = DirectX::XMConvertToRadians(GetEulerRotation().y);
		DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationY(radianAngle);
		DirectX::XMVECTOR dir = DirectX::XMLoadFloat3(&localDirection);
		dir = DirectX::XMVector3TransformNormal(dir, rotation);
		dir = DirectX::XMVector3Normalize(dir);
		DirectX::XMFLOAT3 direction;
		DirectX::XMStoreFloat3(&direction, dir);
		// 区間の時間を取る
		float duration = state.endTime - state.startTime;
		float actualWarpDistance = state.moveDistance;
		if (jumpMotionWarpOverrideActive)
		{
			direction = jumpMotionWarpDirection;
			actualWarpDistance = calculatedJumpDistance;
		}

		float speed = 0.0f;
		if (duration > 0.0f)
		{
			speed = actualWarpDistance / duration;
		}
		AnimationMotionWarp warp{};
		warp.direction = direction;
		warp.speed = speed;
		warp.state = &state;
		warp.notifyMoveDistance = state.moveDistance;
		warp.actualWarpDistance = actualWarpDistance;
		warp.startPosition = GetPosition();
		animationMotionWarps.push_back(warp);
	}
	break;
	}
}
void GruxEnemy::OnAnimationNotifyEnd(const AnimationNotifyState& state)
{
	if (const auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene());
		gameScene && (gameScene->IsPhase1BreakPending() || gameScene->IsBossPhaseTransitionActive())) return;
	if (finalHitReactionActive || fourthHitReactionActive) return;
	switch (state.type)
	{
	case AnimationNotifyState::Type::HitBox:
		activeHitBoxNotifyStates.erase(
			std::remove(activeHitBoxNotifyStates.begin(), activeHitBoxNotifyStates.end(), &state),
			activeHitBoxNotifyStates.end());
		Logger::Log(U8("当たり判定を終了しました"));
		if (state.parameter == rightWeapon || state.parameter == bothWeapon)
		{
			rightHitBox = false;
			activeRightHitBoxRadius = hitWeaponRadius;
		}
		if (state.parameter == leftWeapon || state.parameter == bothWeapon)
		{
			leftHitBox = false;
			activeLeftHitBoxRadius = hitWeaponRadius;
		}
		RefreshActiveHitBoxesFromNotifyStates();
		break;
	case AnimationNotifyState::Type::InputWindow:
		Logger::Log(U8("コンボ受付を終了しました"));
		break;
	case AnimationNotifyState::Type::Invincible:
		break;
	case AnimationNotifyState::Type::TransitionWindow:
		transitionWindow = false;
		Logger::Log(U8("遷移許可区間を終了しました"));
		break;
	case AnimationNotifyState::Type::JustDodgeWindow:
		break;
	case AnimationNotifyState::Type::DangerWindow:
		isDangerWindow = false;
		activeDangerNotifyState = nullptr;
		dangerArea = {};
		break;
	case AnimationNotifyState::Type::ShowTrail:
		if (state.parameter == leftWeapon || state.parameter == bothWeapon)
			showLeftWeaponTrail = false;
		if (state.parameter == rightWeapon || state.parameter == bothWeapon)
			showRightWeaponTrail = false;
		break;
	case AnimationNotifyState::Type::ShowEmissive:
		break;
	case AnimationNotifyState::Type::MotionWarp:
	{
		if (IsFastComboMotionWarpNotify(state))
		{
			ClearFastComboStepIn();
			break;
		}
		animationMotionWarps.erase(
			std::remove_if(
				animationMotionWarps.begin(),
				animationMotionWarps.end(),
				[&](const AnimationMotionWarp& warp)
				{
					return warp.state == &state;
				}),
			animationMotionWarps.end());
	}
	break;
	}
}

void GruxEnemy::OnAnimationNotifyEvent(const AnimationNotifyEvent& event)
{
	HandleCommonAnimationNotifyEvent(event);

	const auto controller = GetBodyAnimationController();
	const bool isDashWindup = IsDashAttackBTActive() &&
		dashBTPhase == DashBTPhase::Telegraph && controller &&
		controller->GetCurrentAnimationName() == "Pre_Stampede_0";
	if (event.type == AnimationNotifyEvent::Type::GameplayEvent &&
		event.parameter == "DashAttackDirectionLock" && isDashWindup)
	{
		dashBTDirectionLockRequested = true;
		return;
	}

	if (event.type == AnimationNotifyEvent::Type::GameplayEvent && event.parameter == "DashAttackStart" && isDashWindup)
	{
		dashBTDashStartRequested = true;
		return;
	}

	if (event.type == AnimationNotifyEvent::Type::SpawnEffect && event.parameter == "GroundImpact")
	{
		BeginJumpTelegraphImpactFlash();
	}

	if (event.type == AnimationNotifyEvent::Type::GameplayEvent && event.parameter == "DashAttackTelegraphFlash")
	{
		BeginDashTelegraphFanFlash();
		return;
	}

	if (event.type == AnimationNotifyEvent::Type::GameplayEvent && event.parameter == "DashAttackTelegraphFadeOut")
	{
		BeginDashTelegraphFadeOut();
		return;
	}

	if (event.type == AnimationNotifyEvent::Type::GameplayEvent && event.parameter == "GameBgmFadeOut")
	{
		if (auto* gameScene = dynamic_cast<GameScene*>(GetOwnerScene()))
			gameScene->BeginGameBgmFadeOut();
		return;
	}
	if (event.type == AnimationNotifyEvent::Type::GameplayEvent && event.parameter == "RoarShockwave")
	{
		ApplyRoarShockwave();
		return;
	}
	if (event.type == AnimationNotifyEvent::Type::CameraShake && event.parameter == "BossRoar")
	{
		if (auto* gameScene = dynamic_cast<GameScene*>(GetOwnerScene()))
			gameScene->NotifyPhase2BossRoar();
	}
	if (finalHitReactionActive || fourthHitReactionActive) return;
	if (event.parameter == "BeginHuskParticle" && isDeathPerform)
	{
		beginHuskParticleRequest = true;
		return;
	}

	switch (event.type)
	{
	case AnimationNotifyEvent::Type::PlaySE:
	{
		const bool isBossDeathCinematicClip = controller &&
			(controller->GetCurrentAnimationName() == "Ultimate_Roar_0" ||
				controller->GetCurrentAnimationName() == "Result_Down_Start" ||
				controller->GetCurrentAnimationName() == "Death_B_0");
		const auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene());
		if (isBossDeathCinematicClip && gameScene &&
			gameScene->IsBossDeathFinishPromptActive())
		{
			return;
		}
		const bool isBossDeathCinematicOneShot = isBossDeathCinematicClip &&
			gameScene && gameScene->IsBossDeathCinematicAudioActive();
		if (event.parameter != "")
		{
			std::string audioPath = "./Data/Sound/SE/" + event.parameter + ".wav";
			auto audio = CoreAudio::PlayOneShot(audioPath, event.value);
			float pitch = pitchBaseValue + GetTimeScale() * (1.0f - pitchBaseValue);
			audio->SetPitch(pitch);
			if (isBossDeathCinematicOneShot && audio)
			{
				std::erase_if(bossDeathCinematicOneShots,
					[](const std::shared_ptr<CoreStandaloneAudioSource>& source)
					{
						return !source || !source->IsPlaying();
					});
				bossDeathCinematicOneShots.push_back(audio);
			}
		}
	}
	break;
	case AnimationNotifyEvent::Type::SpawnEffect:
		if (event.parameter == "GroundImpact")
			SpawnGroundImpactEffect();
		if (event.parameter == "WeaponClash")
			SpawnWeaponClashEffect();
		if (event.parameter == "LeftFootScrape")
			SpawnLeftFootScrapeEffect();
		if (event.parameter == "RightFootScrape")
			SpawnRightFootScrapeEffect();
		if (event.parameter == "GroundDown")
			SpawnGroundDownEffect();
		if (event.parameter == "DropLeftWeapon")
			SpawnDropLeftWeaponEffect();
		if (event.parameter == "DropRightWeapon")
			SpawnDropRightWeaponEffect();

		break;
	case AnimationNotifyEvent::Type::CameraShake:
	{
		if (auto* cameraManager = GetOwnerScene()->GetCameraManager())
			cameraManager->PlayCameraShakePreset(event.parameter);
		break;
	}
	}
}

void GruxEnemy::OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event)
{
	switch (event.type)
	{
	case AnimationNotifyEvent::Type::PlaySE:
	{
		if (event.parameter.empty())
			return;

		const std::string audioPath = "./Data/Sound/SE/" + event.parameter + ".wav";
		auto audio = CoreAudio::PlayOneShot(audioPath, event.value);
		if (audio)
		{
			const float pitch = pitchBaseValue + GetTimeScale() * (1.0f - pitchBaseValue);
			audio->SetPitch(pitch);
		}
		break;
	}
	case AnimationNotifyEvent::Type::CameraShake:
	{
		if (event.parameter.empty())
			return;

		if (auto* cameraManager = GetOwnerScene()->GetCameraManager())
			cameraManager->PlayCameraShakePreset(event.parameter);
		break;
	}
	case AnimationNotifyEvent::Type::SpawnEffect:
		if (event.parameter == "GroundImpact" && groundDustEffectComponent)
		{
			DirectX::XMFLOAT3 spawnPosition = GetPosition();
			if (weaponRightTipComponent)
			{
				const DirectX::XMFLOAT3 weaponTipPosition =
					weaponRightTipComponent->GetComponentLocation();
				spawnPosition.x = weaponTipPosition.x;
				spawnPosition.z = weaponTipPosition.z;
			}

			groundDustEffectComponent->SetWorldLocationDirect(spawnPosition);
			groundDustEffectComponent->UpdateComponentToWorld();
			EffectManager::EmitParticle(
				groundDustEffectComponent->GetEffectHandle(),
				groundDustEffectComponent->GetComponentLocation(),
				{ 0.0f, 0.0f, 0.0f });
		}
		else if (event.parameter == "WeaponClash")
		{
			SpawnWeaponClashEffect();
		}
		else if (event.parameter == "LeftFootScrape")
		{
			SpawnLeftFootScrapeEffect();
		}
		else if (event.parameter == "RightFootScrape")
		{
			SpawnRightFootScrapeEffect();
		}
		else if (event.parameter == "GroundDown")
		{
			SpawnGroundDownEffect();
		}
		else if (event.parameter == "DropLeftWeapon")
		{
			SpawnDropLeftWeaponEffect();
		}
		else if (event.parameter == "DropRightWeapon")
		{
			SpawnDropRightWeaponEffect();
		}

		break;
	}
}

void GruxEnemy::DrawAnimationEditorPreviewState(const AnimationNotifyState& state)
{
	constexpr DirectX::XMFLOAT4 dangerPreviewColor{ 0.15f, 0.85f, 1.0f, 1.0f };
	constexpr DirectX::XMFLOAT4 leftHitBoxPreviewColor{ 0.25f, 1.0f, 0.45f, 1.0f };
	constexpr DirectX::XMFLOAT4 rightHitBoxPreviewColor{ 1.0f, 0.35f, 0.85f, 1.0f };

	if (state.type == AnimationNotifyState::Type::DangerWindow)
	{
		const DangerArea previewArea = BuildDangerArea(
			GetPosition(), GetRight(), GetUp(), GetForward(),
			state.justDodgeAreaOffset, state.justDodgeAreaSize);
		DebugRender::DrawBox(previewArea.WorldTransform(), previewArea.size,
			dangerPreviewColor, 0.0f, true);
		return;
	}
	if (state.type != AnimationNotifyState::Type::HitBox)
		return;

	const auto controller = GetBodyAnimationController();
	const bool drawSweep = controller && controller->IsEditorPreviewPlaying() &&
		controller->IsEditorPreviewHitBoxSweepVisible();
	const float currentTime = controller ? controller->GetCurrentSampledAnimationTime() : 0.0f;
	const auto drawPoints = [](const WeaponHitBoxPoints& points, float radius,
		const DirectX::XMFLOAT4& color)
		{
			DebugRender::DrawSphere(points.root, radius, color, 0.0f, true);
			DebugRender::DrawSphere(points.middle, radius, color, 0.0f, true);
			DebugRender::DrawSphere(points.tip, radius, color, 0.0f, true);
			DebugRender::DrawLine(points.root, points.middle, color, 0.0f, true);
			DebugRender::DrawLine(points.middle, points.tip, color, 0.0f, true);
		};
	const auto drawSweepPoints = [](const WeaponHitBoxPoints& previous,
		const WeaponHitBoxPoints& current, float radius, const DirectX::XMFLOAT4& color)
		{
			DebugRender::DrawLine(previous.root, current.root, color, 0.0f, true);
			DebugRender::DrawLine(previous.middle, current.middle, color, 0.0f, true);
			DebugRender::DrawLine(previous.tip, current.tip, color, 0.0f, true);
			DebugRender::DrawSphere(previous.root, radius, color, 0.0f, true);
			DebugRender::DrawSphere(previous.middle, radius, color, 0.0f, true);
			DebugRender::DrawSphere(previous.tip, radius, color, 0.0f, true);
		};
	const auto drawWeapon = [&](const std::shared_ptr<SceneComponent>& root,
		const std::shared_ptr<SceneComponent>& middle, const std::shared_ptr<SceneComponent>& tip,
		const DirectX::XMFLOAT4& color, WeaponHitBoxPoints& previous,
		const AnimationNotifyState*& previousState, float& previousTime)
		{
			if (!root || !middle || !tip)
				return;
			const WeaponHitBoxPoints current = BuildWeaponHitBoxPoints(root, middle, tip,
				state.hitBoxOffset);
			const bool contiguous = previousState == &state && currentTime >= previousTime &&
				currentTime - previousTime <= 0.1f;
			if (drawSweep && contiguous)
				drawSweepPoints(previous, current, state.hitBoxRadius, color);
			drawPoints(current, state.hitBoxRadius, color);
			previous = current;
			previousState = &state;
			previousTime = currentTime;
		};

	if (state.parameter == leftWeapon || state.parameter == bothWeapon)
		drawWeapon(weaponLeftRootComponent, weaponLeftMiddleComponent, weaponLeftTipComponent,
			leftHitBoxPreviewColor, editorPreviewLeftHitBoxPoints,
			editorPreviewLeftHitBoxState, editorPreviewLeftHitBoxTime);
	if (state.parameter == rightWeapon || state.parameter == bothWeapon)
		drawWeapon(weaponRightRootComponent, weaponRightMiddleComponent, weaponRightTipComponent,
			rightHitBoxPreviewColor, editorPreviewRightHitBoxPoints,
			editorPreviewRightHitBoxState, editorPreviewRightHitBoxTime);
}

void GruxEnemy::OnAnimationChanged()
{
	const auto telegraphController = GetBodyAnimationController();
	if ((!weaponTelegraphActive && !weaponTelegraphFadingOut) || !telegraphController ||
		(!weaponTelegraphFadingOut && telegraphController->GetCurrentAnimationName() != weaponTelegraphAnimationName))
		ResetWeaponTelegraphRimLight();
	showLeftWeaponTrail = false;
	showRightWeaponTrail = false;

	auto controller = GetBodyAnimationController();
	if (!controller || controller->GetCurrentAnimationName() != "PrimaryAttack_LA")
	{
		DisableAttackHitBoxes();
	}
}

void GruxEnemy::StartAttack()
{
	ClearJumpAttackMotionWarpOverride();
	StopDashAttackMovement();
	++currentAttackSequenceId;
	justDodgedActors.clear();
	hitActors.clear();
	currentAttackHitCount = 0;
	DisableAttackHitBoxes();
}

void GruxEnemy::DisableAttackHitBoxes()
{
	leftHitBox = false;
	rightHitBox = false;
	activeLeftHitBoxRadius = hitWeaponRadius;
	activeRightHitBoxRadius = hitWeaponRadius;
	activeLeftHitBoxOffset = {};
	activeRightHitBoxOffset = {};
	activeHitBoxNotifyStates.clear();
	transitionWindow = false;
	isDangerWindow = false;
	activeDangerNotifyState = nullptr;
	dangerArea = {};
}

void GruxEnemy::OnSelectedActionStartedSuccessfully()
{
	if (IsRepeatPenaltyCombatAttack(selectedActionType))
	{
		secondLastStartedCombatAttack = lastStartedCombatAttack;
		lastStartedCombatAttack = selectedActionType;
	}

	// カメラ外にボスがいる時にカメラをボス側に補正する
	float cameraAssistStrength = 0.0f;
	float cameraAssistDuration = 0.0f;
	switch (selectedAttackType)
	{
	case BossAttackType::ChargeAttack:
		cameraAssistStrength = 1.00f;
		cameraAssistDuration = 0.50f;
		break;
	case BossAttackType::DashAttack:
		cameraAssistStrength = 0.65f;
		cameraAssistDuration = 0.80f;
		break;
	default:
		break;
	}

	if (cameraAssistStrength > 0.0f && cameraAssistDuration > 0.0f)
	{
		Camera* activeCamera = GetOwnerScene()->GetActiveCamera();
		if (auto* darkCamera = dynamic_cast<DarkCameraActor*>(activeCamera))
		{
			const DirectX::XMFLOAT3 bossCameraPosition = cameraTargetComponent
				? cameraTargetComponent->GetComponentLocation()
				: GetPosition();
			darkCamera->RequestOffscreenAttackAssist(
				bossCameraPosition, cameraAssistStrength, cameraAssistDuration);
		}
	}

}

void GruxEnemy::RequestJumpAttackCameraAssist() const
{
	Camera* activeCamera = GetOwnerScene()->GetActiveCamera();
	if (auto* darkCamera = dynamic_cast<DarkCameraActor*>(activeCamera))
	{
		const DirectX::XMFLOAT3 bossCameraPosition = cameraTargetComponent
			? cameraTargetComponent->GetComponentLocation()
			: GetPosition();
		darkCamera->RequestOffscreenAttackAssist(
			bossCameraPosition, 0.70f, 0.80f);
	}
}

void GruxEnemy::UpdatePositioningAnimation(float actualSpeed, float deltaTime)
{
	const float positioningAnimationActualSpeed = (std::max)(0.0f, actualSpeed);
	const float safeDeltaTime = (std::max)(0.0f, deltaTime);

	if (!positioningAnimationMoving)
	{
		positioningMoveStopTimer = 0.0f;
		if (positioningAnimationActualSpeed <= positioningMoveStartSpeedThreshold)
			return;

		positioningAnimationMoving = true;
		const auto controller = GetBodyAnimationController();
		const bool movePlaying = controller && controller->IsPlayAnimation() &&
			controller->GetCurrentAnimationName() == "TravelMode_Fwd_0";
		if (!movePlaying)
			PlayBodyAnimation("TravelMode_Fwd_0", true, true, 0.15f, true);
		return;
	}

	if (positioningAnimationActualSpeed >= positioningMoveStopSpeedThreshold)
	{
		positioningMoveStopTimer = 0.0f;
		return;
	}

	positioningMoveStopTimer += safeDeltaTime;
	if (positioningMoveStopTimer < positioningMoveStopDelay)
		return;

	positioningAnimationMoving = false;
	positioningMoveStopTimer = 0.0f;
	const auto controller = GetBodyAnimationController();
	const bool idlePlaying = controller && controller->IsPlayAnimation() &&
		controller->GetCurrentAnimationName() == "TravelMode_Idle_0";
	if (!idlePlaying)
		PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
}

void GruxEnemy::EndPositioningAnimation()
{
	positioningMoveStopTimer = 0.0f;
	positioningAnimationMoving = false;

	if (isDeathPerform && cinematicDeathAnimationOwnedExternally)
		return;

	const auto controller = GetBodyAnimationController();
	const bool idlePlaying = controller && controller->IsPlayAnimation() &&
		controller->GetCurrentAnimationName() == "TravelMode_Idle_0";
	if (!idlePlaying)
		PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
}

void GruxEnemy::StartSelectedActionCooldown()
{
	for (size_t i = 0; i < combatActionData.size(); ++i)
	{
		if (combatActionData[i].type != selectedActionType)
			continue;

		combatActionCooldownRemaining[i] =
			(std::max)(0.0f, combatActionData[i].cooldownDuration);
		return;
	}
}
bool GruxEnemy::PlayAttackStage(BossAttackType type, int stage)
{
	const char* animationName = nullptr;
	switch (type)
	{
	case BossAttackType::PrimaryAttackLA: animationName = "PrimaryAttack_LA"; break;
	case BossAttackType::PrimaryAttackRA: animationName = "PrimaryAttack_RA"; break;
	case BossAttackType::FastCombo:
	{
		static constexpr const char* comboAnimations[] =
		{
			"Attack_A_Fast_0",
			"Attack_B_Fast_0",
			"Attack_C_Fast_0",
		};
		if (stage < 0 || stage >= static_cast<int>(std::size(comboAnimations)))
			return false;
		animationName = comboAnimations[stage];
		break;
	}
	case BossAttackType::JumpAttack:
		if (stage == 0)
		{
			animationName = "Pre_Stampede_0";
		}
		else if (stage == 1)
		{
			PrepareJumpAttackMotionWarpOverride();
			animationName = "PrimaryAttack_JumpAttack";
		}
		else
		{
			return false;
		}
		break;
	case BossAttackType::Dash: animationName = "Stampede_0"; break;
	case BossAttackType::DashAttack:
	{
		static constexpr const char* dashAnimations[] =
		{
			"Pre_Stampede_0",
			"Stampede_0",
			"Stampede_Knockup_0",
		};
		if (stage < 0 || stage >= static_cast<int>(std::size(dashAnimations)))
			return false;
		if (stage == 0)
			StopDashAttackMovement();
		if (stage == 1 && !BeginDashAttackMovement())
			return false;
		animationName = dashAnimations[stage];
		break;
	}
	case BossAttackType::ChargeAttack: return false;
	case BossAttackType::LongRangeAttack: return false;
	}

	transitionWindow = false;
	const bool isJumpTelegraph = type == BossAttackType::JumpAttack && stage == 0;
	const bool ignoreRootMotion = type == BossAttackType::DashAttack || isJumpTelegraph;
	const bool loopAnimation = type == BossAttackType::DashAttack && stage == 1;
	PlayBodyAnimation(animationName, loopAnimation, true, 0.1f, ignoreRootMotion);

	if (isJumpTelegraph)
	{
		const auto controller = GetBodyAnimationController();
		if (!controller ||
			!controller->SetPlaybackRange(
				jumpAttackTelegraphStartTime, jumpAttackTelegraphEndTime))
		{
			return false;
		}
	}
	return true;
}
bool GruxEnemy::PlayAttackAnimationByName(const std::string& animationName)
{
	const auto controller = GetBodyAnimationController();
	if (!controller || !controller->GetAnimationAsset(animationName))
		return false;

	transitionWindow = false;
	PlayBodyAnimation(animationName, false, true, 0.1f);
	return true;
}

void GruxEnemy::BeginAdditionalAttackStage()
{
	hitActors.clear();
}

void GruxEnemy::ClearJumpAttackMotionWarpOverride()
{
	jumpMotionWarpOverrideActive = false;
	jumpMotionWarpDirection = { 0.0f, 0.0f, 1.0f };
	animationMotionWarps.clear();
}

bool GruxEnemy::PrepareDashAttackMovementSnapshot()
{
	const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
	if (!player)
		return false;

	const DirectX::XMFLOAT3 bossPosition = GetPosition();
	const DirectX::XMFLOAT3 playerPosition = player->GetPosition();
	const float dx = playerPosition.x - bossPosition.x;
	const float dz = playerPosition.z - bossPosition.z;
	const float playerDistance = std::sqrt(dx * dx + dz * dz);
	if (playerDistance <= FLT_EPSILON)
		return false;

	const float inverseDistance = 1.0f / playerDistance;
	dashAttackDirection = { dx * inverseDistance, 0.0f, dz * inverseDistance };
	calculatedDashAttackDistance = std::clamp(playerDistance - desiredDashAttackDistance, minDashAttackDistance, maxDashAttackDistance);
	dashAttackStartPosition = bossPosition;
	dashTargetPosition =
	{
		bossPosition.x + dashAttackDirection.x * calculatedDashAttackDistance,
		bossPosition.y,
		bossPosition.z + dashAttackDirection.z * calculatedDashAttackDistance
	};
	const float nominalTravelDistance = std::clamp((std::min)(calculatedDashAttackDistance - dashArrivalDistance, dashAttackSpeed * dashAttackTimeout), 0.0f, calculatedDashAttackDistance);
	dashBTPredictedKnockupPosition =
	{
		bossPosition.x + dashAttackDirection.x * nominalTravelDistance,
		bossPosition.y,
		bossPosition.z + dashAttackDirection.z * nominalTravelDistance
	};
	dashBTMovementSnapshotPrepared = true;

	rotationComponent->SetDirectionImmediate(dashAttackDirection);
	return true;
}


void GruxEnemy::PlayBossTelegraphSE(const char* fileName) const
{
	if (!fileName || !*fileName)
		return;

	const std::string audioPath = std::string("./Data/Sound/SE/") + fileName;
	if (const auto audio = CoreAudio::PlayOneShot(audioPath, 1.0f))
	{
		const float pitch = pitchBaseValue + GetTimeScale() * (1.0f - pitchBaseValue);
		audio->SetPitch(pitch);
	}
}



bool GruxEnemy::BeginDashAttackMovement()
{
	if (!IsDashAttackBTActive())
		HideDashTelegraphVisual();
	StopDashAttackMovement();
	if (!dashBTMovementSnapshotPrepared && !PrepareDashAttackMovementSnapshot())
		return false;

	dashAttackElapsedTime = 0.0f;
	dashAttackMovementActive = true;
	if (characterMovementComponent)
	{
		characterMovementComponent->SetFixedSpeed(dashAttackSpeed);
		characterMovementComponent->SetInputMagnitude(1.0f);
		characterMovementComponent->SetMoveDirection(dashAttackDirection);
	}
	return true;
}

bool GruxEnemy::UpdateDashAttackMovement(float deltaTime, bool keepLockedDirection)
{
	if (!dashAttackMovementActive)
		return true;

	dashAttackElapsedTime += deltaTime;
	const DirectX::XMFLOAT3 currentPosition = GetPosition();
	const float dx = dashTargetPosition.x - currentPosition.x;
	const float dz = dashTargetPosition.z - currentPosition.z;
	const float remainingDistance = std::sqrt(dx * dx + dz * dz);
	const float traveledAlongDash =
		(currentPosition.x - dashAttackStartPosition.x) * dashAttackDirection.x +
		(currentPosition.z - dashAttackStartPosition.z) * dashAttackDirection.z;
	const bool maxDistanceReached =
		traveledAlongDash >= calculatedDashAttackDistance - dashArrivalDistance;

	if (remainingDistance <= dashArrivalDistance || maxDistanceReached ||
		dashAttackElapsedTime >= dashAttackTimeout)
	{
		StopDashAttackMovement();
		return true;
	}

	const float inverseDistance = 1.0f / remainingDistance;
	const DirectX::XMFLOAT3 directionToTarget = keepLockedDirection
		? dashAttackDirection
		: DirectX::XMFLOAT3{ dx * inverseDistance, 0.0f, dz * inverseDistance };
	if (characterMovementComponent)
		characterMovementComponent->SetMoveDirection(directionToTarget);
	rotationComponent->SetDirectionImmediate(directionToTarget);
	return false;
}

void GruxEnemy::StopDashAttackMovement()
{
	dashAttackMovementActive = false;
	dashAttackElapsedTime = 0.0f;
	StopAIMovement();
}

float GruxEnemy::GetActiveChargeSpeed() const
{
	return chargeBT.phase2SettingsLatched ? chargeSpeedPhase2 : chargeSpeed;
}

float GruxEnemy::GetActiveChargePlayerCastRadiusScale() const
{
	return chargeBT.phase2SettingsLatched
		? chargePlayerCastRadiusScalePhase2 : chargePlayerCastRadiusScale;
}

int GruxEnemy::GetActiveChargeDamage() const
{
	for (const BossAttackData& attackData : combatAttackData)
	{
		if (attackData.type == BossAttackType::ChargeAttack)
			return (std::max)(0, attackData.damagePerHit);
	}
	return 1;
}

bool GruxEnemy::EvaluateChargeStartClearance(const DirectX::XMFLOAT3& startPosition,
	const DirectX::XMFLOAT3& direction, float& outClearance, bool& outHit) const
{
	outClearance = 0.0f;
	outHit = false;
	const float directionLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	if (directionLength <= 0.0001f) return false;
	const DirectX::XMFLOAT3 normalizedDirection{ direction.x / directionLength, 0.0f, direction.z / directionLength };
	const float wallCastRadius = (std::max)(0.05f, radius * chargeWallCastRadiusScale);
	const float validationCastDistance = (std::max)(chargeStartValidationClearance, chargeWallCastSafetyMargin + chargeStartValidationClearance);
	DirectX::XMFLOAT3 validationOrigin = startPosition;
	validationOrigin.y += (std::max)(wallCastRadius + 0.05f, height * 0.5f);
	const uint32_t validationWallMask = CollisionHelper::MakeMask({ CollisionLayer::WorldStatic, CollisionLayer::WorldProps, CollisionLayer::WorldPropsNoRaycast, });
	HitResultWithActor validationHit{};
	outHit = Physics::Instance().SphereCast(validationOrigin, normalizedDirection, validationCastDistance, wallCastRadius, validationHit, validationWallMask);
	if (outHit)
		outClearance = (std::max)(0.0f, validationHit.distance);
	else outClearance = validationCastDistance;
	return true;
}

bool GruxEnemy::EvaluateChargeSideClearance(const DirectX::XMFLOAT3& startPosition,
	const DirectX::XMFLOAT3& direction, float& outClearance,
	DirectX::XMFLOAT3& outNormal, bool& outHit) const
{
	outClearance = 0.0f;
	outNormal = {};
	outHit = false;
	const float length = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	if (length <= 0.0001f) return false;
	const DirectX::XMFLOAT3 forward{ direction.x / length, 0.0f, direction.z / length };
	const DirectX::XMFLOAT3 side{ -forward.z, 0.0f, forward.x };
	const float wallRadius = (std::max)(0.05f, radius * chargeWallCastRadiusScale);
	const float required = wallRadius + chargeWallCastSafetyMargin + tripleChargeRepositionSideSafetyMargin;
	const float castDistance = required + tripleChargeRepositionMaxDistance;
	DirectX::XMFLOAT3 origin = startPosition;
	origin.y += (std::max)(wallRadius + 0.05f, height * 0.5f);
	const uint32_t wallMask = CollisionHelper::MakeMask({ CollisionLayer::WorldStatic, CollisionLayer::WorldProps, CollisionLayer::WorldPropsNoRaycast, });
	float nearest = FLT_MAX;
	for (const float sign : { -1.0f, 1.0f })
	{
		const DirectX::XMFLOAT3 castDirection{ side.x * sign, 0.0f, side.z * sign };
		HitResultWithActor hit{};
		if (!Physics::Instance().SphereCast(origin, castDirection, castDistance, wallRadius, hit, wallMask)) continue;
		if (std::abs(hit.normal.y) > chargeWallNormalYThreshold) continue;
		if (hit.distance >= nearest) continue;
		nearest = hit.distance;
		outHit = true;
		outClearance = (std::max)(0.0f, hit.distance);
		outNormal = hit.normal;
	}
	return true;
}

bool GruxEnemy::IsStunned() const
{
	return chargeBT.phase == ChargeBTPhase::Stun;
}

bool GruxEnemy::BeginChargeAttackMovement()
{
	StopChargeAttackMovement();

	const BossTargetContext context = BuildTargetContext();
	if (!context.valid)
	{
		chargeEndReason = ChargeAttackEndReason::SafetyTimeout;
		Logger::Warning(Logger::LogCategory::Gameplay,
			"[BossCharge][StartFailed] reason=InvalidTarget");
		return false;
	}

	if (!chargeDirectionLocked && !LockChargeDirectionToPlayer())
	{
		return false;
	}

	const DirectX::XMFLOAT3 chargeStartPosition = GetPosition();
	float validationClearance = 0.0f;
	bool validationHitWall = false;
	const bool validationMeasured = EvaluateChargeStartClearance(chargeStartPosition, chargeDirection, validationClearance, validationHitWall);
	(void)validationHitWall;
	if (!validationMeasured)
	{
		return false;
	}
	if (validationClearance <= chargeStartValidationClearance)
	{
		return false;
	}
	chargeElapsedTime = 0.0f;
	chargeEndReason = ChargeAttackEndReason::None;
	chargeMovementActive = true;
	ApplyMainChargeWindSettings();
	if (mainChargeWindMeshComponent)
		mainChargeWindMeshComponent->SetIsVisible(mainChargeWindEnabled);

	StartChargeWindTrails();
	if (characterMovementComponent)
	{
		characterMovementComponent->ClearBossRoomProbeChargeWallHitEvent();
		characterMovementComponent->SetBossRoomProbeChargeWallHitArmed(true);
	}
	chargeDangerWindowActive = true;

	PlayBodyAnimation("Stampede_0", true, true, 0.1f, true);
	if (rotationComponent)
	{
		rotationComponent->SetDirectionImmediate(chargeDirection);
	}
	if (characterMovementComponent)
	{
		characterMovementComponent->SetFixedSpeed(GetActiveChargeSpeed());
		characterMovementComponent->SetInputMagnitude(1.0f);
		characterMovementComponent->SetMoveDirection(chargeDirection);
	}
	return true;
}

bool GruxEnemy::LockChargeDirectionToPlayer()
{
	if (chargeDirectionLocked) return true;
	const BossTargetContext context = BuildTargetContext();
	if (!context.valid)
	{
		chargeEndReason = ChargeAttackEndReason::SafetyTimeout;
		Logger::Warning(Logger::LogCategory::Gameplay, "[BossCharge][DirectionLockFailed] reason=InvalidTarget");
		return false;
	}
	chargeDirection = context.directionToPlayer;
	chargeDirection.y = 0.0f;
	const float directionLength = std::sqrt(chargeDirection.x * chargeDirection.x + chargeDirection.z * chargeDirection.z);
	if (directionLength <= FLT_EPSILON)
	{
		chargeEndReason = ChargeAttackEndReason::SafetyTimeout;
		Logger::Warning(Logger::LogCategory::Gameplay, "[BossCharge][DirectionLockFailed] reason=ZeroDirection");
		return false;
	}
	chargeDirection.x /= directionLength;
	chargeDirection.z /= directionLength;
	chargeDirectionLocked = true;
	return true;
}

ChargeAttackEndReason GruxEnemy::UpdateChargeAttackMovement(float deltaTime, bool allowTripleWallTurn)
{
	if (!chargeMovementActive)
		return chargeEndReason;

	UpdateChargeWindTrails(deltaTime);
	chargeElapsedTime += (std::max)(0.0f, deltaTime);
	CharacterMovementComponent::BossRoomProbeMoveBlockEvent bossRoomProbeBlockEvent;
	if (characterMovementComponent &&
		characterMovementComponent->ConsumeBossRoomProbeChargeWallHitEvent(bossRoomProbeBlockEvent))
	{
		const float requestedLength = std::sqrt(
			bossRoomProbeBlockEvent.requestedMove.x * bossRoomProbeBlockEvent.requestedMove.x +
			bossRoomProbeBlockEvent.requestedMove.z * bossRoomProbeBlockEvent.requestedMove.z);
		const float requestedAlongCharge = requestedLength > 0.001f
			? (bossRoomProbeBlockEvent.requestedMove.x * chargeDirection.x +
				bossRoomProbeBlockEvent.requestedMove.z * chargeDirection.z) / requestedLength
			: 0.0f;
		const uint32_t bossRoomWallLayer = CollisionHelper::ToBit(CollisionLayer::WorldPropsNoRaycast);
		if (!bossRoomProbeBlockEvent.initialOverlap &&
			(bossRoomProbeBlockEvent.hitLayer & bossRoomWallLayer) != 0 &&
			requestedAlongCharge > 0.95f && bossRoomProbeBlockEvent.forwardMoveLost > 0.01f)
		{
			if (allowTripleWallTurn && chargeBT.phase2SettingsLatched &&
				chargeBT.tripleChargeIndex < 2)
			{
				chargeEndReason = ChargeAttackEndReason::LegComplete;
				StopChargeAttackMovement();
				return chargeEndReason;
			}

			chargeEndReason = ChargeAttackEndReason::WallHit;
			const DirectX::XMFLOAT3 currentPosition = GetPosition();
			const DirectX::XMFLOAT3 impactPosition = {
				currentPosition.x, bossRoomProbeBlockEvent.hitPosition.y, currentPosition.z };
			SpawnWallImpactEffect(impactPosition, bossRoomProbeBlockEvent.hitNormal);
			Time::SetSlow(0.0f, 0.05f);
			StopChargeAttackMovement();
			return chargeEndReason;
		}
	}

	const float frameMoveDistance = GetActiveChargeSpeed() * (std::max)(0.0f, deltaTime);
	const float castDistance = frameMoveDistance + chargeWallCastSafetyMargin;
	const float playerCastRadius = (std::max)(0.05f, radius * GetActiveChargePlayerCastRadiusScale());
	const float justDodgeCastScale = chargeBT.phase2SettingsLatched
		? chargeJustDodgeCastRadiusScalePhase2 : chargeJustDodgeCastRadiusScale;
	const float justDodgeCastRadius = (std::max)(0.05f, radius * justDodgeCastScale);
	const float wallCastRadius = (std::max)(0.05f, radius * chargeWallCastRadiusScale);
	DirectX::XMFLOAT3 castOrigin = GetPosition();
	castOrigin.y += (std::max)((std::max)(playerCastRadius, wallCastRadius) + 0.05f, height * 0.5f);

	const DirectX::XMFLOAT3 playerDamageCastOrigin{
		castOrigin.x + chargeDirection.x * chargePlayerDamageForwardOffset,
		castOrigin.y,
		castOrigin.z + chargeDirection.z * chargePlayerDamageForwardOffset };

	HitResultWithActor playerHit{};
	const bool playerCastHit = Physics::Instance().SphereCast(
		playerDamageCastOrigin, chargeDirection, castDistance, playerCastRadius, playerHit,
		CollisionHelper::ToBit(CollisionLayer::Player));
	HitResultWithActor justDodgeHit{};
	const bool justDodgeCastHit = Physics::Instance().SphereCast(
		castOrigin, chargeDirection, castDistance, justDodgeCastRadius, justDodgeHit,
		CollisionHelper::ToBit(CollisionLayer::Player));
	HitResultWithActor wallHit{};
	const uint32_t wallMask = CollisionHelper::MakeMask({
		CollisionLayer::WorldStatic,
		CollisionLayer::WorldProps,
		CollisionLayer::WorldPropsNoRaycast,
		});
	const bool wallCastHit = Physics::Instance().SphereCast(
		castOrigin, chargeDirection, castDistance, wallCastRadius, wallHit, wallMask);
	bool wallCandidate = false;
	if (wallCastHit)
	{
		const float chargeWallFacingAmount = -(
			chargeDirection.x * wallHit.normal.x +
			chargeDirection.z * wallHit.normal.z);

		const bool facesCharge =
			chargeWallFacingAmount >= chargeWallFacingThreshold;
		const bool isNotFloor =
			std::abs(wallHit.normal.y) <= chargeWallNormalYThreshold;
		wallCandidate = facesCharge && isNotFloor;
	}

	Player* hitPlayer = playerCastHit
		? dynamic_cast<Player*>(playerHit.actor)
		: nullptr;
	const bool playerCandidate =
		hitPlayer && !hitActors.contains(hitPlayer);
	const bool playerIsFirst = playerCandidate &&
		(!wallCandidate || playerHit.distance <= wallHit.distance);
	Player* justDodgePlayer = justDodgeCastHit
		? dynamic_cast<Player*>(justDodgeHit.actor)
		: nullptr;
	const bool justDodgeCandidate = justDodgePlayer && !hitActors.contains(justDodgePlayer);
	const bool justDodgePlayerIsFirst = justDodgeCandidate &&
		(!wallCandidate || justDodgeHit.distance <= wallHit.distance);
	if (justDodgePlayerIsFirst && chargeDangerWindowActive)
	{
		TryStartJustDodgeSuccess(justDodgePlayer);
	}

	if (playerIsFirst)
	{
		if (!HasJustDodgedAttack(hitPlayer) &&
			hitPlayer->TryTakeDamage(GetActiveChargeDamage(), GetPosition()))
		{
			DirectX::XMFLOAT3 knockBackDirection =
				MathHelper::Subtract(hitPlayer->GetPosition(), GetPosition());
			hitPlayer->StartKnockBack(knockBackDirection);

			hitActors.emplace(hitPlayer);
			++currentAttackHitCount;
			chargeEndReason = ChargeAttackEndReason::PlayerHit;
			StopChargeAttackMovement();
			return chargeEndReason;
		}

	}


	if (wallCandidate)
	{
		if (allowTripleWallTurn && chargeBT.tripleChargeIndex < 2 &&
			chargeBT.phase2SettingsLatched &&
			wallHit.distance <= chargeBT.tripleChargeWallTurnClearance)
		{
			chargeEndReason = ChargeAttackEndReason::LegComplete;
			StopChargeAttackMovement();
			return chargeEndReason;
		}
		chargeEndReason = ChargeAttackEndReason::WallHit;
		const float wallHitDistance = (std::max)(0.0f, wallHit.distance);
		const DirectX::XMFLOAT3 impactPosition = wallHit.hasPosition
			? wallHit.hitPoint
			: DirectX::XMFLOAT3{
				castOrigin.x + chargeDirection.x * wallHitDistance,
				castOrigin.y + chargeDirection.y * wallHitDistance,
				castOrigin.z + chargeDirection.z * wallHitDistance };
		const DirectX::XMFLOAT3 impactNormal = wallHit.hasNormal
			? wallHit.normal
			: DirectX::XMFLOAT3{
				-chargeDirection.x, -chargeDirection.y, -chargeDirection.z };
		SpawnWallImpactEffect(impactPosition, impactNormal);
		Time::SetSlow(0.0f, 0.05f);
		StopChargeAttackMovement();
		return chargeEndReason;
	}

	if (chargeElapsedTime >= chargeSafetyTimeout)
	{
		chargeEndReason = ChargeAttackEndReason::SafetyTimeout;
		const std::string timeoutMessage =
			"[BossCharge][End] reason=SafetyTimeout elapsed=" +
			std::to_string(chargeElapsedTime);
		Logger::Warning(Logger::LogCategory::Gameplay, timeoutMessage.c_str());
		StopChargeAttackMovement();
		return chargeEndReason;
	}

	if (characterMovementComponent)
	{
		characterMovementComponent->SetFixedSpeed(GetActiveChargeSpeed());
		characterMovementComponent->SetInputMagnitude(1.0f);
		characterMovementComponent->SetMoveDirection(chargeDirection);
	}
	return ChargeAttackEndReason::None;
}

void GruxEnemy::StartChargeWindTrails()
{
	if (!chargeWindTrailEnabled)
	{
		StopChargeWindTrails();
		return;
	}

	chargeWindTrailsActive = true;
	const size_t activeCount = static_cast<size_t>(std::clamp(
		chargeWindTrailActiveCount, 1, static_cast<int>(chargeWindTrailCount)));
	for (size_t index = 0; index < chargeWindTrails.size(); ++index)
	{
		auto& streak = chargeWindTrails[index];
		if (index < activeCount)
			SpawnChargeWindTrail(streak);
		else
		{
			streak.active = false;
			streak.trail.Clear();
		}
	}
}

void GruxEnemy::ApplyMainChargeWindSettings()
{
	if (!mainChargeWindMeshComponent)
		return;

	mainChargeWindMeshComponent->SetRelativeLocationDirect(mainChargeWindLocalPosition);
	mainChargeWindMeshComponent->SetRelativeEulerRotationDirect(mainChargeWindLocalRotation);
	mainChargeWindMeshComponent->SetRelativeScaleDirect(mainChargeWindLocalScale);
	mainChargeWindMeshComponent->SetWindConstants(
		mainChargeWindScrollSpeed, std::clamp(mainChargeWindOpacity, 0.0f, 1.0f),
		mainChargeWindRadialSpeed, (std::max)(0.01f, mainChargeWindRadialTiling),
		std::clamp(mainChargeWindMaskFlowMode, 0, 1));
	mainChargeWindMeshComponent->plusAlphaCBuffer->data.cpuColor = {
		mainChargeWindColor.x, mainChargeWindColor.y, mainChargeWindColor.z, 1.0f };
	mainChargeWindMeshComponent->plusAlphaCBuffer->data.emissionPower =
		(std::max)(0.0f, mainChargeWindEmissionPower);
}

void GruxEnemy::UpdateChargeWindTrails(float deltaTime)
{
	if (!chargeWindTrailsActive || !chargeMovementActive)
		return;

	const float safeDeltaTime = (std::max)(0.0f, deltaTime);
	const float directionLength = std::sqrt(
		chargeDirection.x * chargeDirection.x + chargeDirection.z * chargeDirection.z);
	if (directionLength <= FLT_EPSILON)
		return;

	const DirectX::XMFLOAT3 forward{
		chargeDirection.x / directionLength, 0.0f, chargeDirection.z / directionLength };
	const DirectX::XMFLOAT3 right{ forward.z, 0.0f, -forward.x };

	for (auto& streak : chargeWindTrails)
	{
		streak.trail.UpdateTrail(safeDeltaTime);
		if (!streak.active || streak.age >= streak.lifetime)
		{
			SpawnChargeWindTrail(streak);
			continue;
		}

		const float previousAge = streak.age;
		streak.age += safeDeltaTime;
		if (streak.age >= streak.lifetime)
		{
			SpawnChargeWindTrail(streak);
			continue;
		}

		const float previousPhase = streak.randomPhase + previousAge * streak.wobbleFrequency;
		const float currentPhase = streak.randomPhase + streak.age * streak.wobbleFrequency;
		const float lateralDelta = (std::sin(currentPhase) - std::sin(previousPhase)) *
			streak.lateralWobbleAmplitude;
		const float verticalDelta = (std::cos(currentPhase) - std::cos(previousPhase)) *
			streak.verticalWobbleAmplitude;
		streak.center.x += streak.backwardVelocity.x * safeDeltaTime + right.x * lateralDelta;
		streak.center.y += streak.backwardVelocity.y * safeDeltaTime + verticalDelta;
		streak.center.z += streak.backwardVelocity.z * safeDeltaTime + right.z * lateralDelta;

		const float widthScale = std::lerp(chargeWindTrailHeadWidthScale, chargeWindTrailTailWidthScale, std::clamp(streak.age / streak.lifetime, 0.0f, 1.0f));
		const float halfWidth = streak.width * widthScale * 0.5f;
		const DirectX::XMFLOAT3 tip{
			streak.center.x + right.x * halfWidth,
			streak.center.y,
			streak.center.z + right.z * halfWidth };
		const DirectX::XMFLOAT3 root{
			streak.center.x - right.x * halfWidth,
			streak.center.y,
			streak.center.z - right.z * halfWidth };
		streak.trail.trailPoints.push_back({ tip, root, streak.lifetime });
	}
}

void GruxEnemy::StopChargeWindTrails()
{
	chargeWindTrailsActive = false;
	for (auto& streak : chargeWindTrails)
	{
		streak.active = false;
		streak.age = 0.0f;
		streak.lifetime = 0.0f;
		streak.trail.Clear();
	}
}

void GruxEnemy::SpawnChargeWindTrail(ChargeWindTrailRuntime& streak) const
{
	const float directionLength = std::sqrt(
		chargeDirection.x * chargeDirection.x + chargeDirection.z * chargeDirection.z);
	if (directionLength <= FLT_EPSILON)
	{
		streak.active = false;
		streak.trail.Clear();
		return;
	}

	const DirectX::XMFLOAT3 forward{
		chargeDirection.x / directionLength, 0.0f, chargeDirection.z / directionLength };
	const DirectX::XMFLOAT3 right{ forward.z, 0.0f, -forward.x };
	const float lateralSpawn = MathHelper::RandomRange(-chargeWindTrailLateralSpawnRange, chargeWindTrailLateralSpawnRange);
	const float verticalSpawn = MathHelper::RandomRange(chargeWindTrailHeightMin, chargeWindTrailHeightMax);
	const float forwardSpawn = MathHelper::RandomRange(chargeWindTrailForwardMin, chargeWindTrailForwardMax);

	streak.trail.Clear();
	streak.active = true;
	streak.age = 0.0f;
	streak.lifetime = MathHelper::RandomRange(chargeWindTrailLifetimeMin, chargeWindTrailLifetimeMax);
	streak.width = MathHelper::RandomRange(chargeWindTrailWidthMin, chargeWindTrailWidthMax);
	streak.lateralWobbleAmplitude = MathHelper::RandomRange(
		chargeWindTrailLateralWobbleMin, chargeWindTrailLateralWobbleMax);
	streak.verticalWobbleAmplitude = MathHelper::RandomRange(
		chargeWindTrailVerticalWobbleMin, chargeWindTrailVerticalWobbleMax);
	streak.wobbleFrequency = MathHelper::RandomRange(
		chargeWindTrailWobbleFrequencyMin, chargeWindTrailWobbleFrequencyMax);
	streak.randomPhase = MathHelper::RandomRange(0.0f, DirectX::XM_2PI);
	const float backwardSpeed = MathHelper::RandomRange(chargeWindTrailSpeedMin, chargeWindTrailSpeedMax);
	streak.backwardVelocity = { -forward.x * backwardSpeed, 0.0f, -forward.z * backwardSpeed };

	DirectX::XMFLOAT3 pos = GetPosition();
	streak.center = {
		pos.x + right.x * lateralSpawn + forward.x * forwardSpawn,
		pos.y + verticalSpawn,
		pos.z + right.z * lateralSpawn + forward.z * forwardSpawn };

	const float initialLateral = std::sin(streak.randomPhase) * streak.lateralWobbleAmplitude;
	const float initialVertical = std::cos(streak.randomPhase) * streak.verticalWobbleAmplitude;
	streak.center.x += right.x * initialLateral;
	streak.center.y += initialVertical;
	streak.center.z += right.z * initialLateral;
	streak.trail.SetFadeLifetime(streak.lifetime);

	constexpr float seedStep = 0.0125f;
	for (int pointIndex = 3; pointIndex >= 0; --pointIndex)
	{
		const float sampleAge = -seedStep * static_cast<float>(pointIndex);
		const float samplePhase = streak.randomPhase + sampleAge * streak.wobbleFrequency;
		const float lateralOffset = (std::sin(samplePhase) - std::sin(streak.randomPhase)) *
			streak.lateralWobbleAmplitude;
		const float verticalOffset = (std::cos(samplePhase) - std::cos(streak.randomPhase)) *
			streak.verticalWobbleAmplitude;
		const DirectX::XMFLOAT3 sampleCenter{
			streak.center.x + streak.backwardVelocity.x * sampleAge + right.x * lateralOffset,
			streak.center.y + verticalOffset,
			streak.center.z + streak.backwardVelocity.z * sampleAge + right.z * lateralOffset };
		const float widthScale = std::lerp(chargeWindTrailHeadWidthScale, chargeWindTrailTailWidthScale, static_cast<float>(pointIndex) / 3.0f);
		const float halfWidth = streak.width * widthScale * 0.5f;
		const DirectX::XMFLOAT3 tip{
			sampleCenter.x + right.x * halfWidth,
			sampleCenter.y,
			sampleCenter.z + right.z * halfWidth };
		const DirectX::XMFLOAT3 root{
			sampleCenter.x - right.x * halfWidth,
			sampleCenter.y,
			sampleCenter.z - right.z * halfWidth };
		streak.trail.trailPoints.push_back({ tip, root, streak.lifetime });
	}
}

void GruxEnemy::StopChargeAttackMovement()
{
	StopChargeWindTrails();
	if (mainChargeWindMeshComponent)
		mainChargeWindMeshComponent->SetIsVisible(false);
	if (characterMovementComponent)
		characterMovementComponent->SetBossRoomProbeChargeWallHitArmed(false);
	chargeMovementActive = false;
	chargeDangerWindowActive = false;
	StopAIMovement();
}

float GruxEnemy::GetRecoveryDurationForCurrentAttack() const
{
	for (const BossAttackData& attackData : combatAttackData)
	{
		if (attackData.type == selectedAttackType)
			return (std::max)(0.0f, attackData.recoveryDuration);
	}

	return (std::max)(0.0f, recoveryDuration);
}

void GruxEnemy::SetNextRecoveryDuration(float duration)
{
	nextRecoveryDuration = (std::max)(0.0f, duration);
}
float GruxEnemy::ConsumeNextRecoveryDuration()
{
	const bool hasOverride = nextRecoveryDuration.has_value();
	const float duration = hasOverride
		? *nextRecoveryDuration
		: GetRecoveryDurationForCurrentAttack();

	nextRecoveryDuration.reset();
	return duration;
}

int GruxEnemy::GetDamageForCurrentAttack() const
{
	for (const BossAttackData& attackData : combatAttackData)
	{
		if (attackData.type == selectedAttackType)
			return (std::max)(0, attackData.damagePerHit);
	}

	return 1;
}

BossTargetContext GruxEnemy::BuildTargetContext() const
{
	BossTargetContext context{};
	const auto scene = GetOwnerScene();
	const auto player = scene ? scene->GetActorManager()->GetActorOfType<Player>() : nullptr;
	// プレイヤーが存在しない場合や削除予定の場合は無効なコンテキストを返す
	if (!player || player->IsPendingKill())
		return context;

	// プレイヤーとの距離を計算する
	const DirectX::XMFLOAT3 bossPosition = GetPosition();
	const DirectX::XMFLOAT3 playerPosition = player->GetPosition();
	const float dx = playerPosition.x - bossPosition.x;
	const float dz = playerPosition.z - bossPosition.z;
	context.xzDistance = std::sqrt(dx * dx + dz * dz);

	// プレイヤーとの距離に応じた距離領域を取得する
	context.distanceRegion = GetDistanceRegion(context.xzDistance);

	// ボスの前方向ベクトルを計算する
	DirectX::XMVECTOR forwardVector = DirectX::XMVector3Rotate(
		DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
		DirectX::XMLoadFloat4(&GetQuaternionRotation()));
	DirectX::XMFLOAT3 forward{};
	DirectX::XMStoreFloat3(&forward, forwardVector);
	forward.y = 0.0f;
	const float forwardLength = std::sqrt(
		forward.x * forward.x + forward.z * forward.z);
	if (forwardLength > FLT_EPSILON)
	{
		forward.x /= forwardLength;
		forward.z /= forwardLength;
	}
	else
	{
		forward = { 0.0f, 0.0f, 1.0f };
	}

	// プレイヤーへの方向ベクトルを計算する
	if (context.xzDistance > FLT_EPSILON)
	{
		const float inverseDistance = 1.0f / context.xzDistance;
		context.directionToPlayer =
		{ dx * inverseDistance, 0.0f, dz * inverseDistance };
	}
	else
	{
		context.directionToPlayer = forward;
	}

	// ボスの前方向とプレイヤーへの方向の内積を計算する
	context.forwardDot = std::clamp(MathHelper::Dot(forward, context.directionToPlayer), -1.0f, 1.0f);
	const float side = forward.x * context.directionToPlayer.z - forward.z * context.directionToPlayer.x;
	// 内積と外積から角度を計算する
	context.signedAngleDegrees = DirectX::XMConvertToDegrees(std::atan2f(side, context.forwardDot));
	// 内積から絶対角度を計算する
	context.absoluteAngleDegrees = DirectX::XMConvertToDegrees(std::acos(context.forwardDot));

	if (context.absoluteAngleDegrees <= relativeFrontMaxAngle)
		context.region = PlayerRelativeRegion::Front;
	else if (context.absoluteAngleDegrees < relativeBackMinAngle)
		context.region = PlayerRelativeRegion::Side;
	else
		context.region = PlayerRelativeRegion::Back;

	context.valid = true;
	return context;
}

bool GruxEnemy::IsNearFrontForAttackSelection() const
{
	const BossTargetContext context = BuildTargetContext();
	return context.valid && context.distanceRegion == BossDistanceRegion::Near &&
		context.absoluteAngleDegrees <= fastComboFrontMaxAngle;
}

bool GruxEnemy::IsDefensiveBackForBehavior(const BossTargetContext& context) const
{
	return context.valid && context.absoluteAngleDegrees >= defensiveBackMinAngle;
}

float GruxEnemy::GetAttackRepeatPenaltyForPlan(const std::string& planName) const
{
	if (planName == "FastComboPlan")
		return GetRecentAttackPenalty(BossActionType::FastCombo);
	if (planName == "JumpAttackPlan")
		return GetRecentAttackPenalty(BossActionType::JumpAttack);
	if (planName == "DashAttackPlan")
		return GetRecentAttackPenalty(BossActionType::DashAttack);
	if (planName == "ChargeAttackPlan")
		return GetRecentAttackPenalty(BossActionType::ChargeAttack);
	return 1.0f;
}



void GruxEnemy::RefreshFastComboTargetContext(int stage)
{
	if (stage < 0 || stage >= static_cast<int>(fastComboStageTargetContexts.size()))
		return;

	if (stage == 0)
		fastComboStageTargetContexts = {};

	fastComboTargetContext = BuildTargetContext();
	fastComboStageTargetContexts[stage] = fastComboTargetContext;
}


void GruxEnemy::BeginFastComboStepIn(int stage, float notifyStartTime, float notifyEndTime)
{
	ClearFastComboStepIn();
	if (stage < 0 || stage >= static_cast<int>(fastComboMaxStepInDistance.size()) || !characterMovementComponent)
		return;
	const BossTargetContext& context = fastComboStageTargetContexts[stage];
	if (!context.valid || context.region == PlayerRelativeRegion::Back)
		return;
	const float distanceError = context.xzDistance - fastComboDesiredAttackDistance;
	const float stepDistance = std::clamp(distanceError * fastComboStepInRatio,
		0.0f, fastComboMaxStepInDistance[stage]);
	const float notifyDuration = notifyEndTime - notifyStartTime;
	const float calculatedSpeed = notifyDuration > 0.001f ? stepDistance / notifyDuration : 0.0f;
	const float appliedSpeed = (std::min)(calculatedSpeed, fastComboStepInMaxSpeed);
	if (notifyDuration <= 0.001f || stepDistance <= 0.001f || appliedSpeed <= 0.001f)
		return;
	fastComboStepInActive = true;
	characterMovementComponent->AddForcedMove(context.directionToPlayer, appliedSpeed,
		notifyDuration);
}

void GruxEnemy::UpdateBehaviorTree(float dt)
{
	fastComboApproachRetryRemaining = (std::max)(0.0f, fastComboApproachRetryRemaining - dt);
	repositionCooldownRemaining = (std::max)(0.0f, repositionCooldownRemaining - (std::max)(0.0f, dt));
	repositionRetryCooldownRemaining = (std::max)(0.0f, repositionRetryCooldownRemaining - (std::max)(0.0f, dt));
	if (!aiTree || !behaviorData)
		return;
	if (!activeNode)
	{
		BeginCombatDecisionInference();
		activeNode = aiTree->ActiveNodeInference(behaviorData.get());
	}
	if (!activeNode)
	{
		return;
	}
	behaviorTreePreviousNode = activeNode->GetName();
	activeNode = aiTree->Run(activeNode, behaviorData.get(), dt);
	const auto result = aiTree->GetLastRunResult();

	if (!activeNode)
	{
		if (result == ActionBase::State::Complete && (behaviorTreePreviousNode == "ExecuteAttackRecovery" || behaviorTreePreviousNode == "ExecuteChargeRecovery"))
		{// 攻撃の完了を記録する。
			++consecutiveAttackCount;
		}
		return;
	}
}

bool GruxEnemy::IsFastComboStepInActive()
{
	return fastComboStepInActive;
}

void GruxEnemy::ClearFastComboStepIn()
{
	if (characterMovementComponent)
		characterMovementComponent->AddForcedMove({ 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f);
	fastComboStepInActive = false;
}

bool GruxEnemy::IsFastComboMotionWarpNotify(const AnimationNotifyState& state) const
{
	if (selectedAttackType != BossAttackType::FastCombo ||
		fastComboRuntimeStage < 0 || fastComboRuntimeStage > 2)
		return false;

	static constexpr const char* fastComboAnimations[] =
	{
		"Attack_A_Fast_0", "Attack_B_Fast_0", "Attack_C_Fast_0"
	};
	const auto controller = GetBodyAnimationController();
	return controller && state.type == AnimationNotifyState::Type::MotionWarp &&
		controller->GetCurrentAnimationName() == fastComboAnimations[fastComboRuntimeStage];
}

bool GruxEnemy::CanPlanJumpAttack() const
{
	const auto context = BuildTargetContext();
	if (!context.valid || IsDead())
		return false;
	for (size_t i = 0; i < combatAttackData.size(); ++i)
	{
		if (combatAttackData[i].type == BossAttackType::JumpAttack)
			return combatActionCooldownRemaining[i] <= 0.0f;
	}
	return false;
}

bool GruxEnemy::PrepareJumpAttackSetupTarget()
{
	DirectX::XMFLOAT3 target{};
	if (!FindAttackSetupTarget(jumpSetupDistanceMin, jumpSetupDistanceMax,
		attackSetupCandidateAngleStep, attackSetupClampTolerance, jumpSetupMinimumMoveDistance,
		target))
	{
		ClearAttackSetupTarget();
		return false;
	}
	attackSetupTarget = {};
	attackSetupTarget.targetPosition = target;
	attackSetupTarget.valid = true;
	return true;
}
bool GruxEnemy::FindAttackSetupTarget(float minDistance, float maxDistance, float angleStep, float clampTolerance, float minimumMoveDistance, DirectX::XMFLOAT3& outTarget, float boundaryMarginOverride) const
{
	outTarget = {};
	const auto scene = GetOwnerScene();
	const auto player = scene ? scene->GetActorManager()->GetActorOfType<Player>() : nullptr;
	if (!player || player->IsPendingKill())
		return false;

	const DirectX::XMFLOAT3 playerPosition = player->GetPosition();
	const DirectX::XMFLOAT3 bossPosition = GetPosition();
	const float baseX = bossPosition.x - playerPosition.x;
	const float baseZ = bossPosition.z - playerPosition.z;
	const float baseLength = std::sqrt(baseX * baseX + baseZ * baseZ);
	const DirectX::XMFLOAT3 baseDirection = baseLength > FLT_EPSILON
		? DirectX::XMFLOAT3{ baseX / baseLength, 0.0f, baseZ / baseLength }
	: GetForward();
	static thread_local std::mt19937 randomEngine{ std::random_device{}() };
	std::uniform_real_distribution<float> distanceDistribution(
		(std::min)(minDistance, maxDistance), (std::max)(minDistance, maxDistance));
	const float setupDistance = distanceDistribution(randomEngine);
	const float safeAngleStep = (std::max)(1.0f, angleStep);
	const float safeClampTolerance = (std::max)(0.0f, clampTolerance);
	for (int index = 0; index <= 12; ++index)
	{
		const float magnitude = static_cast<float>((index + 1) / 2);
		const float signedStep = index == 0 ? 0.0f : (index % 2 == 1 ? magnitude : -magnitude);
		const float angle = DirectX::XMConvertToRadians(signedStep * safeAngleStep);
		const float rotatedX = baseDirection.x * std::cos(angle) - baseDirection.z * std::sin(angle);
		const float rotatedZ = baseDirection.x * std::sin(angle) + baseDirection.z * std::cos(angle);
		const DirectX::XMFLOAT3 originalCandidate{
			playerPosition.x + rotatedX * setupDistance, playerPosition.y,
			playerPosition.z + rotatedZ * setupDistance };
		RepositionTargetEvaluation evaluation{};
		EvaluateClampedPositioningTarget(bossPosition, originalCandidate, evaluation, boundaryMarginOverride);
		const float clampDeltaX = evaluation.clampedTarget.x - originalCandidate.x;
		const float clampDeltaZ = evaluation.clampedTarget.z - originalCandidate.z;
		const float clampDistance = std::sqrt(clampDeltaX * clampDeltaX + clampDeltaZ * clampDeltaZ);
		if (clampDistance > safeClampTolerance)
			continue;
		const float moveX = evaluation.clampedTarget.x - bossPosition.x;
		const float moveZ = evaluation.clampedTarget.z - bossPosition.z;
		if (std::sqrt(moveX * moveX + moveZ * moveZ) < (std::max)(0.0f, minimumMoveDistance))
			continue;
		outTarget = evaluation.clampedTarget;
		return true;
	}
	return false;
}

void GruxEnemy::ClearAttackSetupTarget()
{
	ClearPositioningTarget(attackSetupTarget, attackSetupRuntime);
}

GruxEnemy::AttackSetupMoveResult GruxEnemy::UpdateAttackSetupMovement(float dt)
{
	const auto result = UpdatePositioningTargetMovement(attackSetupTarget, attackSetupRuntime, dt);
	return result;
}

void GruxEnemy::StopAttackSetupMovement()
{
	attackSetupRuntime.movementActive = false;
	StopAIMovement();
	EndPositioningAnimation();
}

bool GruxEnemy::ResolveBossRoomPositioningMove(const DirectX::XMFLOAT3& startPosition,
	const DirectX::XMFLOAT3& desiredTarget, DirectX::XMFLOAT3& outResolvedTarget,
	bool& outPathBlocked) const
{
	outResolvedTarget = desiredTarget;
	outPathBlocked = false;
	const auto scene = GetOwnerScene();
	const auto stage = scene ? scene->GetActorManager()->GetActorOfType<DarkStage>() : nullptr;
	if (!stage || !stage->IsBossRoomWallCollisionEnabled() || !characterMovementComponent)
		return false;

	const float probeRadius = characterMovementComponent->GetActiveHorizontalWallProbeRadius();
	const float probeOriginY = startPosition.y + (std::max)(1.0f, probeRadius + 0.05f);
	DarkStage::BossRoomHorizontalMoveResult result;
	if (!stage->ResolveBossRoomHorizontalMove(startPosition, desiredTarget,
		probeOriginY, probeRadius, result) || !result.valid)
		return false;
	outResolvedTarget = result.resolvedTarget;
	outPathBlocked = result.pathBlocked;
	return true;
}

bool GruxEnemy::StartJumpAttackTelegraph()
{
	StartAttack();
	StopAIMovement();
	return PlayAttackStage(BossAttackType::JumpAttack, 0);
}

bool GruxEnemy::UpdateJumpAttackTelegraph(float deltaTime) const
{
	const BossTargetContext context = BuildTargetContext();
	if (!context.valid)
		return false;
	RotateTowardsPlayer(context.directionToPlayer, GetTurnSpeed(), deltaTime);
	const auto controller = GetBodyAnimationController();
	return !controller || !controller->IsPlayAnimation();
}

bool GruxEnemy::StartJumpAttackExecution()
{
	if (!PlayAttackStage(BossAttackType::JumpAttack, 1))
		return false;
	OnSelectedActionStartedSuccessfully();
	return true;
}


void GruxEnemy::StartWeaponTelegraph(const std::string& animationName)
{
	ResetWeaponTelegraphRimLight();

	const auto controller = GetBodyAnimationController();
	const auto* asset = controller ? controller->GetAnimationAsset(animationName) : nullptr;
	if (!asset) return;
	const auto state = std::find_if(asset->notifyTrack.states.begin(), asset->notifyTrack.states.end(),
		[](const AnimationNotifyState& value) { return value.type == AnimationNotifyState::Type::ShowEmissive; });
	if (state == asset->notifyTrack.states.end()) return;

	weaponTelegraphActive = true;
	weaponTelegraphAnimationName = animationName;
	weaponTelegraphLeadDuration = initialFrontAttackReadyDuration;
	weaponTelegraphShowEmissiveEndTime = state->endTime;
	weaponTelegraphMaximumPower = (std::max)(0.0f, state->value);
	weaponTelegraphElapsed = 0.0f;
	weaponTelegraphCurrentPower = 0.0f;
}
void GruxEnemy::StopAIMovement() const
{
	if (!characterMovementComponent)
		return;
	characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
	characterMovementComponent->SetInputMagnitude(0.0f);
	characterMovementComponent->ResetFixedSpeed();
}

void GruxEnemy::RotateTowardsPlayer(const DirectX::XMFLOAT3& direction, const float degreesPerSecond, const float deltaTime) const
{
	if (rotationComponent)
	{
		rotationComponent->RotateTowardsDirection(direction, degreesPerSecond, deltaTime);
	}
}

// ボスの名前の演出を開始する
void GruxEnemy::StartGruxNamePerform(float duration, float start, float end)
{
	// 名前テクスチャのalphaが徐々に濃くなる処理
	{
		EasingHandler handler;

		handler.AddEasing(
			TestEaseType::InSine,
			start,
			end,
			duration
		);


		handler.SetCompletedFunction([this]()
			{
				easingFactorAlpha = 1.0f;
			});
		PropertyAccessor<float> accessor;

		accessor.getter = [this]() { return easingFactorAlpha; };
		accessor.setter = [this](float t)
			{
				easingFactorAlpha = t;
			};

		easingRunner->StartHandler(handler, accessor);
	}
}

void GruxEnemy::InitializeSkeletalMeshAndAnimations(const std::string& parentName)
{
	skeletalMeshComponent = AddComponent<SkeletalMeshComponent>(parentName);
	skeletalMeshComponent->SetModel("./Data/Models/Characters/GruxQilin/boss.gltf", false, true);
	const std::weak_ptr<GruxEnemy> roarPoseOwner = std::static_pointer_cast<GruxEnemy>(shared_from_this());
	skeletalMeshComponent->SetRenderLocalYOffsetProvider("pelvis", [roarPoseOwner]()
		{
			const auto owner = roarPoseOwner.lock();
			return owner ? owner->GetRoarRenderFootOffset() : 0.0f;
		});
	skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Enemy;   // オブジェクトの種類を Enemy に設定
	skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.6f;   // 目玉の自己発光の強さを設定
	skeletalMeshComponent->plusAlphaCBuffer->data.cpuColor = { 0.9f,0.08f,0.08f,1.0f };   // 目玉の色を赤にしてみる
	skeletalMeshComponent->overrideDeferredPipelineName = "GltfModelDeferredGruxPS";

	for (auto& material : skeletalMeshComponent->model->materials)
	{
		if (material.name == "MIC_Hair_Demo_Qilin")
		{
			material.materialType = MaterialType::Hair;
		}
		else if (material.name == "M_Grux_Qilin_Eye")
		{// 目だったら、
			material.materialType = MaterialType::Eye;
		}
		else if (material.name == "M_Grux_Qilin_Hore")
		{// つの
			material.materialType = MaterialType::Metallic;
		}
		else if (material.name == "M_Grux_Qilin_Gauntlets")
		{// 腰
			material.materialType = MaterialType::Metallic;
		}
		else if (material.name == "M_Grux_Qilin_Weapon")
		{// 武器
			material.materialType = MaterialType::Metallic;
		}
	}
	skeletalMeshComponent->SetIsShadowMap(true);
	skeletalMeshComponent->SetIsCastShadow(false);

	// 第二形態のテクスチャをロードする
	const auto loadPhase2BaseColor = [this](const char* materialName, const std::wstring& filename)
		{
			skeletalMeshComponent->LoadPhase2BaseColorTexture(materialName, filename);
		};
	loadPhase2BaseColor("M_Grux_Qilin_Torso",
		L"./Data/Models/Characters/GruxQilin/M_Grux_Qilin_Torso_BaseColor_Phase2.DDS");
	loadPhase2BaseColor("M_Grux_Qilin_Gear",
		L"./Data/Models/Characters/GruxQilin/M_Grux_Qilin_Gear_BaseColor_Phase2.png");
	loadPhase2BaseColor("M_Grux_Qilin_Head",
		L"./Data/Models/Characters/GruxQilin/M_Grux_Qilin_Head_BaseColor_Phase2.DDS");
	loadPhase2BaseColor("M_Grux_Qilin_LegsHands",
		L"./Data/Models/Characters/GruxQilin/M_Grux_Qilin_LegsHands_BaseColor_Phase2.DDS");

	// アニメーションコントローラーを作成

	int rootIndex = skeletalMeshComponent->FindIndexByName("root");
	auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootIndex);
	controller->AddAnimation("Idle", 0);
	controller->AddAnimation("PrimaryAttack_JumpAttack", 1);
	controller->AddAnimation("PrimaryAttack_Recall", 2);
	controller->AddAnimation("Stampede_0", 3);
	controller->AddAnimation("Stampede_Knockup_0", 4);
	controller->AddAnimation("Stun_Idle", 5);
	controller->AddAnimation("TravelMode_Fwd_0", 6);
	controller->AddAnimation("TravelMode_Idle_0", 7);
	controller->AddAnimation("Ultimate_Roar_0", 8);
	controller->AddAnimation("LevelStart_0", 9);
	controller->AddAnimation("Death_B_0", 10);
	controller->AddAnimation("Attack_A_Fast_0", 11);
	controller->AddAnimation("Attack_B_Fast_0", 12);
	controller->AddAnimation("Attack_C_Fast_0", 13);
	controller->AddAnimation("Pre_Stampede_0", 14);
	controller->AddAnimation("Pre_FootSlide_0", 15);
	controller->AddAnimation("Knock_Down_End", 16);
	controller->AddAnimation("Knock_Down_Loop", 17);
	controller->AddAnimation("Knock_Down_Start", 18);
	controller->AddAnimation("HitReact_Front", 19);
	controller->AddAnimation("HitReact_Back", 20);
	controller->AddAnimation("HitReact_Left", 21);
	controller->AddAnimation("HitReact_Right", 22);
	controller->AddAnimation("Result_Down_Start", 23);
	controller->AddAnimation("Hit_Combat_Large_B", 24);
	controller->AddAnimation("Hit_Combat_Large_F", 25);
	controller->AddAnimation("Hit_Combat_Large_L", 26);
	controller->AddAnimation("Hit_Combat_Large_R", 27);

	// 全てのNotifyAssetsをロードする
	controller->LoadAllNotifyAssets(GetName());

	// アニメーションコントローラーを character に追加
	this->AddBodyAnimationController(controller);
	// アニメーションコントローラーのオーナーの名前を設定する
	controller->SetOwnerName(GetName());
	PlayBodyAnimation("TravelMode_Idle_0");
}

void GruxEnemy::CreateCoreComponents(const std::string& parentName)
{
	std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("enemyCapsuleComponent", parentName);
	enemyCapsuleComponent = capsuleComponent;
	height = 3.5f;
	radius = 1.95f;
	mass = 300.0f;

	capsuleComponent->SetRadiusAndHeight(radius, height);
	capsuleComponent->SetMass(mass);
	capsuleComponent->SetStatic(true);
	capsuleComponent->SetCapsuleAxis(ShapeComponent::CapsuleAxis::y);
	capsuleComponent->SetLayer(CollisionLayer::Enemy);
	capsuleComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
	capsuleComponent->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Block);
	capsuleComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
	capsuleComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
	capsuleComponent->SetCollisionOffsetY(height * 0.5f);
	capsuleComponent->SetIsVisibleDebugBox(false);
	capsuleComponent->Initialize();

	rotationComponent = this->AddComponent<class RotationComponent>("rotationComponent", parentName);
	rotationComponent->SetDirection({ -1.0f,0.0f,0.0f });
	// キャラクタームーブコンポーネントを追加
	characterMovementComponent = this->AddComponent<CharacterMovementComponent>("movementComponent", parentName);
	characterMovementComponent->SetUseGravity(true);

	// ポイントライトコンポーネントを追加
	auto pointLightComponent = this->AddComponent<PointLightComponent>("pointLightComponent", parentName);
	pointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f, 1.0f });
	// ライトの名前からライトマネージャーの共有ライトを取得して設定
	pointLightComponent->SetSharedLightName("EnemyPointLight");
	// ポイントライトコンポーネントを追加
	auto backPointLightComponent = this->AddComponent<PointLightComponent>("backPointLight", parentName);
	backPointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f,-1.0f });
	// ライトの名前からライトマネージャーの共有ライトを取得して設定
	backPointLightComponent->SetSharedLightName("EnemyBackPointLight");
}

void GruxEnemy::CreateWeaponSocketComponents(const std::string& parentName)
{
	int socketLeftNode = skeletalMeshComponent->FindIndexByName("weapon_l");
	// 左の武器の根本のコンポーネントを追加   
	weaponLeftRootComponent = AddComponent<SceneComponent>("weaponLeftRootComponent", parentName);
	weaponLeftRootComponent->SetRelativeLocationDirect({ 0.0f,0.0f,0.6f });
	weaponLeftRootComponent->AttachToComponent(skeletalMeshComponent, socketLeftNode); // "weapon_l"
	// 左の武器の真ん中のコンポーネントを追加
	weaponLeftMiddleComponent = AddComponent<SceneComponent>("weaponLeftMiddleComponent", "weaponLeftRootComponent");
	weaponLeftMiddleComponent->SetRelativeLocationDirect({ 0.0f,0.0f,0.6f });
	// 左の武器の先端のコンポーネントを追加
	weaponLeftTipComponent = AddComponent<SceneComponent>("weaponLeftTipComponent", "weaponLeftRootComponent");
	weaponLeftTipComponent->SetRelativeLocationDirect({ 0.0f,0.0f,1.1f });

	int socketRightNode = skeletalMeshComponent->FindIndexByName("weapon_r");
	// 右の武器の根本のコンポーネントを追加   
	weaponRightRootComponent = AddComponent<SceneComponent>("weaponRightRootComponent", parentName);
	weaponRightRootComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-0.2f });
	weaponRightRootComponent->AttachToComponent(skeletalMeshComponent, socketRightNode); // "weapon_r"
	// 右の武器の真ん中のコンポーネントを追加
	weaponRightMiddleComponent = AddComponent<SceneComponent>("weaponRightMiddleComponent", "weaponRightRootComponent");
	weaponRightMiddleComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-0.8f });
	// 右の武器の先端のコンポーネントを追加
	weaponRightTipComponent = AddComponent<SceneComponent>("weaponRightTipComponent", "weaponRightRootComponent");
	weaponRightTipComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-1.6f });

	// エフェクト位置のため
	// 左足のコンポーネントを追加
	int socketLeftFootNode = skeletalMeshComponent->FindIndexByName("ik_foot_l");
	leftFootComponent = AddComponent<SceneComponent>("leftFootComponent", parentName);
	leftFootComponent->AttachToComponent(skeletalMeshComponent, socketLeftFootNode); // "ik_foot_l"

	// 右足のコンポーネントを追加
	int socketRightFootNode = skeletalMeshComponent->FindIndexByName("ik_foot_r");
	rightFootComponent = AddComponent<SceneComponent>("rightFootComponent", parentName);
	rightFootComponent->AttachToComponent(skeletalMeshComponent, socketRightFootNode); // "ik_foot_r"

	//　首のコンポーネントを追加
	int socketBeltNode = skeletalMeshComponent->FindIndexByName("neck_03");
	neckComponent = AddComponent<SceneComponent>("beltComponent", parentName);
	neckComponent->AttachToComponent(skeletalMeshComponent, socketBeltNode); // "belt"

	// カメラの注視点の位置のコンポーネントを追加
	cameraTargetComponent = AddComponent<SceneComponent>("cameraTargetComponent", parentName);
	cameraTargetComponent->SetRelativeLocationDirect({ 0.0f,1.5f,-0.0f });
}

// エフェクトコンポーネント追加
void GruxEnemy::CreateEffectComponents(const std::string& parentName)
{
	hitSwordEffectComponent = this->AddComponent<class ParticleComponent>("hitSwordEffectComponent", parentName);
	hitSwordEffectComponent->Load("./Data/Effect/Files/NormalAttackHitEffect.json");

	rushHitRingEffectComponent = this->AddComponent<class ParticleComponent>("rushHitRingEffectComponent", parentName);
	rushHitRingEffectComponent->Load("./Data/Effect/Files/RushHitRingEffect.json");
	// Rush被弾時のエフェクト
	rushHitSparkEffectComponent = this->AddComponent<class ParticleComponent>("rushHitSparkEffectComponent", parentName);
	rushHitSparkEffectComponent->Load("./Data/Effect/Files/RushCoreEffect.json");
	// ジャンプ着地時の砂埃
	groundDustEffectComponent = this->AddComponent<class ParticleComponent>("groundDustEffectComponent", parentName);
	groundDustEffectComponent->Load("./Data/Effect/Files/GroundDustEffect1.json");
	roarGroundBurstEffectComponent = this->AddComponent<class ParticleComponent>("roarGroundBurstEffectComponent", parentName);
	roarGroundBurstEffectComponent->Load("./Data/Effect/Files/RoarGroundBurstEffect.json");
	// 壁に当たった時の砂埃
	wallImpactDustEffectComponent = this->AddComponent<class ParticleComponent>("wallImpactDustEffectComponent", parentName);
	wallImpactDustEffectComponent->Load("./Data/Effect/Files/WallImpactDustEffect1.json");

	wallImpactFlashEffectComponent = this->AddComponent<class ParticleComponent>("wallImpactFlashEffectComponent", parentName);
	wallImpactFlashEffectComponent->Load("./Data/Effect/Files/WallImpactFlashEffect.json");
	// 武器同士が当たった時の火花
	metalSparkEffectComponent = this->AddComponent<class ParticleComponent>("metalSparkEffectComponent", parentName);
	metalSparkEffectComponent->Load("./Data/Effect/Files/MetalSparkEffect1.json");

	// 足摺のエフェクト
	footScrapeEffectComponent = this->AddComponent<class ParticleComponent>("footScrapeEffectComponent", parentName);
	footScrapeEffectComponent->Load("./Data/Effect/Files/FootSlideDustEffect1.json");
}

void GruxEnemy::InitializeTrailEffects()
{
	leftWeaponTrail.Initialize();
	rightWeaponTrail.Initialize();
	leftWeaponTrail.SetRushColorEnabled(true, bossTrailColor);
	rightWeaponTrail.SetRushColorEnabled(true, bossTrailColor);
	leftWeaponTrail.SetEmissiveStrength(bossTrailEmissiveStrength);
	rightWeaponTrail.SetEmissiveStrength(bossTrailEmissiveStrength);
	leftWeaponTrail.SetFadeLifetime(bossTrailLifetime);
	rightWeaponTrail.SetFadeLifetime(bossTrailLifetime);
	for (auto& streak : chargeWindTrails)
	{
		streak.trail.Initialize();
		streak.trail.SetRushColorEnabled(true, chargeWindTrailColor);
		streak.trail.SetEmissiveStrength(chargeWindTrailEmissiveStrength);
	}
}

// 通常時のHPバー表示を更新する。
void GruxEnemy::UpdateHpBar(const float uiDeltaTime)
{
	const float currentHp = static_cast<float>((std::max)(hp, 0));


	if (!rushHpDisplayActive && delayedHp > currentHp)
	{
		if (delayedHpDelayTimer > 0.0f)
		{
			delayedHpDelayTimer = (std::max)(0.0f, delayedHpDelayTimer - uiDeltaTime);
		}
		else
		{
			const float followSpeed = useRushDelayedHpFollowSpeed
				? delayedHpRushFollowSpeed
				: delayedHpFollowSpeed;
			delayedHp = (std::max)(currentHp, delayedHp - followSpeed * uiDeltaTime);
			if (delayedHp <= currentHp)
				useRushDelayedHpFollowSpeed = false;
		}
	}
	else if (!rushHpDisplayActive && delayedHp < currentHp)
	{
		delayedHp = currentHp;
		delayedHpDelayTimer = 0.0f;
		useRushDelayedHpFollowSpeed = false;
	}

	if (!IsPhase2HpBarPresentationActive() && hpCurrentFillUiComponent && hpDelayedFillUiComponent)
	{
		const float maximumHp = static_cast<float>(maxHp);
		hpCurrentFillUiComponent->SetValue(currentHp, maximumHp);
		hpDelayedFillUiComponent->SetValue(delayedHp, maximumHp);
	}

}
// 左右武器のTrailを更新する。
void GruxEnemy::UpdateWeaponTrails(float deltaTime)
{
	leftWeaponTrail.SetFadeLifetime(bossTrailLifetime);
	rightWeaponTrail.SetFadeLifetime(bossTrailLifetime);

	// 軌跡の更新処理
	leftWeaponTrail.UpdateTrail(deltaTime);
	rightWeaponTrail.UpdateTrail(deltaTime);

	if (showLeftWeaponTrail && weaponLeftRootComponent && weaponLeftTipComponent)
	{
		leftWeaponTrail.trailPoints.push_back({
			weaponLeftTipComponent->GetComponentLocation(),
			weaponLeftRootComponent->GetComponentLocation(),
			bossTrailLifetime });
	}
	if (showRightWeaponTrail && weaponRightRootComponent && weaponRightTipComponent)
	{
		rightWeaponTrail.trailPoints.push_back({
			weaponRightTipComponent->GetComponentLocation(),
			weaponRightRootComponent->GetComponentLocation(),
			bossTrailLifetime });
	}

}

// AnimationMotionWarpを更新する
void GruxEnemy::UpdateAnimationMotionWarp(float deltaTime)
{
	if (!GetBodyAnimationController()->IsPlayAnimation())
	{
		animationMotionWarps.clear();
		ClearFastComboStepIn();
	}

	DirectX::XMFLOAT3 motionWarpVelocity = { 0.0f, 0.0f, 0.0f };
	for (const auto& warp : animationMotionWarps)
	{
		motionWarpVelocity.x += warp.direction.x * warp.speed;
		motionWarpVelocity.y += warp.direction.y * warp.speed;
		motionWarpVelocity.z += warp.direction.z * warp.speed;
	}
	characterMovementComponent->SetFrameAdditionalVelocity(motionWarpVelocity);
	characterMovementComponent->TickDeferredMovement(deltaTime);
}

// 左右武器の攻撃判定とDebug表示を更新する。
void GruxEnemy::UpdateWeaponHitDetection()
{
	// 当たり判定
	HitResultWithActor hit;

	// 左の武器の当たり判定
	if (leftHitBox)
	{
		bool isLeftHit = false;

		const WeaponHitBoxPoints weaponLeftPoints = BuildWeaponHitBoxPoints(
			weaponLeftRootComponent, weaponLeftMiddleComponent, weaponLeftTipComponent,
			activeLeftHitBoxOffset);
		const DirectX::XMFLOAT3& weaponLeftRootPos = weaponLeftPoints.root;
		const DirectX::XMFLOAT3& weaponLeftMidPos = weaponLeftPoints.middle;
		const DirectX::XMFLOAT3& weaponLeftTipPos = weaponLeftPoints.tip;

		HitResultWithActor leftRootHit;
		HitResultWithActor leftMidHit;
		HitResultWithActor leftTipHit;
		const bool leftRootSucceeded = CollisionFunction::SphereRayCast(prevWeaponLeftRootPos, weaponLeftRootPos, leftRootHit, activeLeftHitBoxRadius, CollisionHelper::ToBit(CollisionLayer::Player));
		const bool leftMidSucceeded = CollisionFunction::SphereRayCast(prevWeaponLeftMidPos, weaponLeftMidPos, leftMidHit, activeLeftHitBoxRadius, CollisionHelper::ToBit(CollisionLayer::Player));
		const bool leftTipSucceeded = CollisionFunction::SphereRayCast(prevWeaponLeftTipPos, weaponLeftTipPos, leftTipHit, activeLeftHitBoxRadius, CollisionHelper::ToBit(CollisionLayer::Player));
		isLeftHit = leftRootSucceeded || leftMidSucceeded || leftTipSucceeded;
		if (leftRootSucceeded)
			hit = leftRootHit;
		else if (leftMidSucceeded)
			hit = leftMidHit;
		else if (leftTipSucceeded)
			hit = leftTipHit;

		prevWeaponLeftRootPos = weaponLeftRootPos;
		prevWeaponLeftMidPos = weaponLeftMidPos;
		prevWeaponLeftTipPos = weaponLeftTipPos;

		if (isLeftHit)
		{
			if (auto player = dynamic_cast<Player*>(hit.actor))
			{
				if (!hitActors.contains(hit.actor) && !HasJustDodgedAttack(hit.actor))
				{
					Logger::Log(U8("剣にプレイヤーが当たった"));
					if (player->TryTakeDamage(GetDamageForCurrentAttack(), GetPosition()))
					{
						hitActors.emplace(player);
						++currentAttackHitCount;
					}
				}
			}
		}
	}
	// 右の武器の当たり判定
	if (rightHitBox)
	{
		bool isRightHit = false;

		const WeaponHitBoxPoints weaponRightPoints = BuildWeaponHitBoxPoints(
			weaponRightRootComponent, weaponRightMiddleComponent, weaponRightTipComponent,
			activeRightHitBoxOffset);
		const DirectX::XMFLOAT3& weaponRightRootPos = weaponRightPoints.root;
		const DirectX::XMFLOAT3& weaponRightMidPos = weaponRightPoints.middle;
		const DirectX::XMFLOAT3& weaponRightTipPos = weaponRightPoints.tip;

		HitResultWithActor rightRootHit;
		HitResultWithActor rightMidHit;
		HitResultWithActor rightTipHit;
		const bool rightRootSucceeded = CollisionFunction::SphereRayCast(prevWeaponRightRootPos, weaponRightRootPos, rightRootHit, activeRightHitBoxRadius, CollisionHelper::ToBit(CollisionLayer::Player));
		const bool rightMidSucceeded = CollisionFunction::SphereRayCast(prevWeaponRightMidPos, weaponRightMidPos, rightMidHit, activeRightHitBoxRadius, CollisionHelper::ToBit(CollisionLayer::Player));
		const bool rightTipSucceeded = CollisionFunction::SphereRayCast(prevWeaponRightTipPos, weaponRightTipPos, rightTipHit, activeRightHitBoxRadius, CollisionHelper::ToBit(CollisionLayer::Player));
		isRightHit = rightRootSucceeded || rightMidSucceeded || rightTipSucceeded;
		if (rightRootSucceeded)
			hit = rightRootHit;
		else if (rightMidSucceeded)
			hit = rightMidHit;
		else if (rightTipSucceeded)
			hit = rightTipHit;

		prevWeaponRightRootPos = weaponRightRootPos;
		prevWeaponRightMidPos = weaponRightMidPos;
		prevWeaponRightTipPos = weaponRightTipPos;
		if (isRightHit)
		{
			if (auto player = dynamic_cast<Player*>(hit.actor))
			{
				if (!hitActors.contains(hit.actor) && !HasJustDodgedAttack(hit.actor))
				{
					Logger::Log(U8("剣にプレイヤーが当たった"));
					if (player->TryTakeDamage(GetDamageForCurrentAttack(), GetPosition()))
					{
						hitActors.emplace(player);
						++currentAttackHitCount;
					}
				}
			}
		}
	}

}

// ビヘイビアツリーを作成する
void GruxEnemy::CreateBossBehaviorTree()
{
	behaviorData = std::make_unique<BehaviorData>();
	aiTree = std::make_unique<BehaviorTree>(this);

	aiTree->AddNode("", "Root", 0, BehaviorTree::SelectRule::Priority, nullptr, nullptr);

	aiTree->AddNode("Root", "Death", 0, BehaviorTree::SelectRule::Sequence, std::make_unique<DeadJudgment>(this), nullptr);

	aiTree->AddNode("Death", "CheckDead", 0, BehaviorTree::SelectRule::Non, std::make_unique<AlwaysJudgment>(this), std::make_unique<BTCompleteAction>(this));
	aiTree->AddNode("Death", "StartDeath", 1, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<BTStartDeath>(this));
	aiTree->AddNode("Death", "ExecuteDeath", 2, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<BTExecuteDeath>(this));

	aiTree->AddNode("Root", "InitialRepositionPlan", 1, BehaviorTree::SelectRule::Sequence, std::make_unique<::CanPlanInitialReposition>(this), nullptr);
	aiTree->AddNode("InitialRepositionPlan", "WaitBeforeInitialReposition", 0, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::WaitBeforeInitialReposition>(this));
	aiTree->AddNode("InitialRepositionPlan", "PrepareInitialRepositionTarget", 1, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::PrepareInitialRepositionTarget>(this));
	aiTree->AddNode("InitialRepositionPlan", "MoveToInitialRepositionTarget", 2, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::MoveToRepositionTarget>(this));
	aiTree->AddNode("InitialRepositionPlan", "WaitAfterInitialReposition", 3, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::WaitAfterReposition>(this));

	aiTree->AddNode("Root", "Defensive", 2, BehaviorTree::SelectRule::Priority, std::make_unique<::CanPlanAnyDefensive>(this), nullptr);

	aiTree->AddNode("Defensive", "RoarPlan", 0, BehaviorTree::SelectRule::Sequence, std::make_unique<::CanPlanRoar>(this), nullptr);
	aiTree->AddNode("RoarPlan", "StartRoar", 0, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::StartRoar>(this));
	aiTree->AddNode("RoarPlan", "ExecuteRoar", 1, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::ExecuteRoar>(this));
	aiTree->AddNode("RoarPlan", "FinishRoar", 2, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::FinishRoar>(this));

	aiTree->AddNode("Defensive", "RetreatPlan", 1, BehaviorTree::SelectRule::Sequence, std::make_unique<::CanPlanRetreat>(this), nullptr);
	aiTree->AddNode("RetreatPlan", "PrepareRetreatTarget", 0, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::PrepareRetreatTarget>(this));
	aiTree->AddNode("RetreatPlan", "MoveToPositioningTarget", 1, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::MoveToPositioningTarget>(this));

	aiTree->AddNode("Root", "CombatDecision", 3, BehaviorTree::SelectRule::Priority, std::make_unique<::CanPlanAnyCombatDecision>(this), nullptr);
	aiTree->AddNode("CombatDecision", "RepositionPlan", 0, BehaviorTree::SelectRule::Sequence, std::make_unique<::CanPlanReposition>(this), nullptr);
	aiTree->AddNode("RepositionPlan", "PrepareRepositionTarget", 0, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::PrepareRepositionTarget>(this));
	aiTree->AddNode("RepositionPlan", "MoveToRepositionTarget", 1, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::MoveToRepositionTarget>(this));
	aiTree->AddNode("RepositionPlan", "WaitAfterReposition", 2, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::WaitAfterReposition>(this));
	aiTree->AddNode("CombatDecision", "Attack", 1, BehaviorTree::SelectRule::AttackRandom, std::make_unique<::CanPlanAnyAttack>(this), nullptr);
	aiTree->AddNode("Root", "Idle", 4, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<BTIdle>(this));

	aiTree->AddNode("Attack", "FastComboPlan", 0, BehaviorTree::SelectRule::Sequence, std::make_unique<::CanPlanFastCombo>(this), nullptr);

	aiTree->AddNode("FastComboPlan", "ApproachIfNeeded", 1, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::ApproachIfNeeded>(this));
	aiTree->AddNode("FastComboPlan", "FacePlayerIfNeeded", 2, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::FacePlayerIfNeeded>(this));
	aiTree->AddNode("FastComboPlan", "CanExecuteFastCombo", 3, BehaviorTree::SelectRule::Non, std::make_unique<::CanExecuteFastCombo>(this), std::make_unique<BTCompleteAction>(this));
	aiTree->AddNode("FastComboPlan", "PrepareFastCombo", 4, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<PrepareFastCombo>(this));
	aiTree->AddNode("FastComboPlan", "StartFastCombo", 4, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::StartFastCombo>(this));
	aiTree->AddNode("FastComboPlan", "ExecuteFastCombo", 5, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::ExecuteFastCombo>(this));
	aiTree->AddNode("FastComboPlan", "ExecuteAttackRecovery", 6, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<ExecuteAttackRecovery>(this));

	aiTree->AddNode("Attack", "JumpAttackPlan", 0, BehaviorTree::SelectRule::Sequence, std::make_unique<::CanPlanJumpAttack>(this), nullptr);

	aiTree->AddNode("JumpAttackPlan", "PrepareJumpSetupTarget", 1, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::PrepareJumpSetupTarget>(this));
	aiTree->AddNode("JumpAttackPlan", "MoveToAttackSetupTarget", 2, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::MoveToAttackSetupTarget>(this));
	aiTree->AddNode("JumpAttackPlan", "FacePlayerIfNeeded", 3, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::FaceJumpPlayerIfNeeded>(this));
	aiTree->AddNode("JumpAttackPlan", "CanExecuteJumpAttack", 4, BehaviorTree::SelectRule::Non, std::make_unique<::CanExecuteJumpAttack>(this), std::make_unique<BTCompleteAction>(this));
	aiTree->AddNode("JumpAttackPlan", "StartJumpAttack", 5, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::StartJumpAttack>(this));
	aiTree->AddNode("JumpAttackPlan", "ExecuteJumpAttack", 6, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::ExecuteJumpAttack>(this));
	aiTree->AddNode("JumpAttackPlan", "ExecuteAttackRecovery", 7, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<ExecuteAttackRecovery>(this));

	aiTree->AddNode("Attack", "DashAttackPlan", 0, BehaviorTree::SelectRule::Sequence, std::make_unique<DashPlanAvailable>(this), nullptr);

	aiTree->AddNode("DashAttackPlan", "PrepareDashSetupTarget", 1, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<PrepareDashSetupTarget>(this));
	aiTree->AddNode("DashAttackPlan", "MoveToAttackSetupTarget", 2, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::MoveToAttackSetupTarget>(this));
	aiTree->AddNode("DashAttackPlan", "FacePlayerIfNeeded", 3, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<FaceDashPlayerIfNeeded>(this));
	aiTree->AddNode("DashAttackPlan", "CanExecuteDashAttack", 4, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::CanExecuteDashAttack>(this));
	aiTree->AddNode("DashAttackPlan", "StartDashAttack", 5, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::StartDashAttack>(this));
	aiTree->AddNode("DashAttackPlan", "ExecuteDashAttack", 6, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::ExecuteDashAttack>(this));
	aiTree->AddNode("DashAttackPlan", "ExecuteAttackRecovery", 7, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<ExecuteAttackRecovery>(this));

	aiTree->AddNode("Attack", "ChargeAttackPlan", 0, BehaviorTree::SelectRule::Sequence, std::make_unique<::CanPlanChargeAttack>(this), nullptr);

	aiTree->AddNode("ChargeAttackPlan", "PrepareChargeSetupTarget", 0, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::PrepareChargeSetupTarget>(this));
	aiTree->AddNode("ChargeAttackPlan", "MoveToAttackSetupTarget", 1, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::MoveToAttackSetupTarget>(this));
	aiTree->AddNode("ChargeAttackPlan", "FacePlayerIfNeeded", 2, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::FaceChargePlayerIfNeeded>(this));
	aiTree->AddNode("ChargeAttackPlan", "CanExecuteChargeAttack", 3, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::CanExecuteChargeAttack>(this));
	aiTree->AddNode("ChargeAttackPlan", "StartChargeAttack", 4, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::StartChargeAttack>(this));
	aiTree->AddNode("ChargeAttackPlan", "ExecuteChargeAttack", 5, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::ExecuteChargeAttack>(this));
	aiTree->AddNode("ChargeAttackPlan", "ResolveChargeResult", 6, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::ResolveChargeResult>(this));
	aiTree->AddNode("ChargeAttackPlan", "ExecuteChargeRecovery", 7, BehaviorTree::SelectRule::Non, nullptr, std::make_unique<::ExecuteChargeRecovery>(this));


}

// UIを作成する
void GruxEnemy::CreateUI()
{
	auto uiManager = GetOwnerScene()->GetUIManager();

	const DirectX::XMFLOAT2 hpNamePosition = { 980.0f, 85.0f };
	const DirectX::XMFLOAT2 hpPosition = { 650.0f, 115.0f };
	const DirectX::XMFLOAT2 hpPivot = { 0.0f, 0.5f };
	const DirectX::XMFLOAT2 hpScale = { 0.35f, 0.35f };

	hpNameUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/HpBar/boss_hp_name.png", "BossHpName");
	hpNameUiComponent->SetWorldPosition(hpNamePosition);
	hpNameUiComponent->SetSize({ 102.0f, 35.0f });
	hpNameUiComponent->SetPivot({ 0.5f,0.5f });
	hpNameUiComponent->SetScale({ 0.95f,0.95f });
	hpNameUiComponent->SetColor(CoreColor::White);
	hpNameUiComponent->zOrder = 10;
	uiManager->Add(hpNameUiComponent);


	hpBackgroundUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/HpBar/boss_hp_background.png", "BossHpBackground");
	hpBackgroundUiComponent->SetWorldPosition(hpPosition);
	hpBackgroundUiComponent->SetSize({ 1904.0f, 43.0f });
	hpBackgroundUiComponent->SetPivot(hpPivot);
	hpBackgroundUiComponent->SetScale(hpScale);
	hpBackgroundUiComponent->SetColor(CoreColor::White);
	hpBackgroundUiComponent->zOrder = 10;
	uiManager->Add(hpBackgroundUiComponent);

	hpDelayedFillUiComponent = std::make_shared<UIGaugeFillComponent>("./Data/Textures/UI/HpBar/boss_hp_fill.png", "BossHpDelayedFill");
	hpDelayedFillUiComponent->SetWorldPosition(hpPosition);
	hpDelayedFillUiComponent->SetSize({ 1897.0f, 36.0f });
	hpDelayedFillUiComponent->SetPivot(hpPivot);
	hpDelayedFillUiComponent->SetScale(hpScale);
	hpDelayedFillUiComponent->SetColor(bossHpDelayedColor);
	hpDelayedFillUiComponent->zOrder = 11;
	uiManager->Add(hpDelayedFillUiComponent);

	hpCurrentFillUiComponent = std::make_shared<UIGaugeFillComponent>("./Data/Textures/UI/HpBar/boss_hp_fill.png", "BossHpCurrentFill");
	hpCurrentFillUiComponent->SetWorldPosition(hpPosition);
	hpCurrentFillUiComponent->SetSize({ 1897.0f, 36.0f });
	hpCurrentFillUiComponent->SetPivot(hpPivot);
	hpCurrentFillUiComponent->SetScale(hpScale);
	hpCurrentFillUiComponent->SetColor(bossHpCurrentColor);
	hpCurrentFillUiComponent->zOrder = 12;
	uiManager->Add(hpCurrentFillUiComponent);

	hpFrameUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/HpBar/boss_hp_frame.png", "BossHpFrame");
	hpFrameUiComponent->SetWorldPosition(hpPosition);
	hpFrameUiComponent->SetSize({ 1909.0f, 49.0f });
	hpFrameUiComponent->SetPivot(hpPivot);
	hpFrameUiComponent->SetScale(hpScale);
	hpFrameUiComponent->SetColor(CoreColor::White);
	hpFrameUiComponent->zOrder = 15;
	uiManager->Add(hpFrameUiComponent);

	hpPhase1BackgroundTexture = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/UI/HpBar/boss_hp_background.png");
	hpPhase1FillTexture = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/UI/HpBar/boss_hp_fill.png");
	hpPhase1FrameTexture = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/UI/HpBar/boss_hp_frame.png");
	hpPhase2BackgroundTexture = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/UI/HpBar/boss_hp_background_phase2.png");
	hpPhase2FillTexture = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/UI/HpBar/boss_hp_fill_phase2.png");
	hpPhase2FrameTexture = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/UI/HpBar/boss_hp_frame_phase2.png");

	hpBarUiComponents =
	{
		hpNameUiComponent,
		hpBackgroundUiComponent,
		hpDelayedFillUiComponent,
		hpCurrentFillUiComponent,
		hpFrameUiComponent,
	};


	// 登場シーンのボス名前のUIを追加
	gruxNameImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/Grux_name.png", "Grux_name");
	gruxNameImageComponent->SetWorldPosition({ 995, 900 });
	gruxNameImageComponent->SetScale({ 0.8f,0.8f });
	gruxNameImageComponent->SetSize({ 600, 200 });
	gruxNameImageComponent->SetPivot({ 0.5f,0.5f });
	gruxNameImageComponent->SetColor(DirectX::XMFLOAT4{ 1.0f,1.0f,1.0f,0.0f });
	gruxNameImageComponent->SetVisible(true);
	uiManager->Add(gruxNameImageComponent);
	easingRunner = std::make_unique<EasingRunner>();
}

// 予兆用のモデルを作成する
void GruxEnemy::CreateTelegraphMesh(const std::string& parentName)
{
	const auto configureMesh = [](const std::shared_ptr<StaticMeshComponent>& mesh,
		const char* modelPath, const DirectX::XMFLOAT4& color,
		const bool useAbsoluteTransform = true)
		{
			if (!mesh->model)
				mesh->SetModel(modelPath);
			if (mesh->model)
				mesh->overrideDeferredPipelineName = "chargeTelegraphUnlitForward";
			mesh->overrideForwardPipelineName = "chargeTelegraphUnlitForward";
			mesh->SetIsCastShadow(false);
			mesh->SetIsVisible(false);
			mesh->SetUsingAbsoluteLocation(useAbsoluteTransform);
			mesh->SetUsingAbsoluteRotation(useAbsoluteTransform);
			mesh->SetUsingAbsoluteScale(useAbsoluteTransform);
			mesh->plusAlphaCBuffer->data.cpuColor = color;
			mesh->plusAlphaCBuffer->data.emissionPower = 0.0f;
			mesh->plusAlphaCBuffer->data.objectType = ObjectType::NoLighting;
		};


	// Chargeの予兆メッシュ。
	chargeTelegraphMeshComponent = AddComponent<StaticMeshComponent>("tripleChargeTelegraph", parentName);
	configureMesh(chargeTelegraphMeshComponent, "./Data/Models/EffectModel/ChargeTelegraphPlane2.glb", { 1.0f, 0.16f, 0.03f, 1.0f }, false);

	// Chargeの風用のメッシュ
	mainChargeWindMeshComponent = AddComponent<MainChargeWindMeshComponent>("mainChargeWind", parentName);
	mainChargeWindMeshComponent->SetModel("./Data/Models/EffectModel/ChargeWindModel1.glb");
	mainChargeWindMeshComponent->overrideDeferredPipelineName = "mainChargeWindForward";
	mainChargeWindMeshComponent->overrideForwardPipelineName = "mainChargeWindForward";
	mainChargeWindMeshComponent->SetIsCastShadow(false);
	mainChargeWindMeshComponent->SetIsVisible(false);
	mainChargeWindMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::NoLighting;

	// ジャンプ用の予兆Mesh
	jumpTelegraphMeshComponent = AddComponent<StaticMeshComponent>("jumpTelegraphCircle", parentName);
	jumpTelegraphInnerMeshComponent = AddComponent<StaticMeshComponent>("jumpTelegraphInnerCircle", parentName);
	configureMesh(jumpTelegraphMeshComponent, "./Data/Models/EffectModel/JumpTelegraphPlane.glb", { 1.0f, 0.16f, 0.03f, 1.0f });
	configureMesh(jumpTelegraphInnerMeshComponent, "./Data/Models/EffectModel/JumpTelegraphPlane.glb", { 1.0f, 0.16f, 0.03f, 1.0f });

	// Roar予兆の外側メッシュ。
	roarTelegraphOuterMeshComponent = AddComponent<StaticMeshComponent>("roarTelegraphOuter", parentName);
	roarTelegraphFillMeshComponent = AddComponent<StaticMeshComponent>("roarTelegraphFill", parentName);
	configureMesh(roarTelegraphOuterMeshComponent, "./Data/Models/EffectModel/RoarTelegraphOuter1.glb", { 1.0f, 0.16f, 0.03f, 1.0f });
	configureMesh(roarTelegraphFillMeshComponent, "./Data/Models/EffectModel/RoarTelegraphFill.glb", { 1.0f, 0.08f, 0.03f, 1.0f });

	roarTelegraphOuterMeshComponent->overrideDeferredPipelineName = "roarTelegraphUnlitForward";
	roarTelegraphOuterMeshComponent->overrideForwardPipelineName = "roarTelegraphUnlitForward";
	roarTelegraphFillMeshComponent->overrideDeferredPipelineName = "roarTelegraphUnlitForward";
	roarTelegraphFillMeshComponent->overrideForwardPipelineName = "roarTelegraphUnlitForward";

	// Dash予兆のメッシュ
	dashTelegraphLineMeshComponent = AddComponent<StaticMeshComponent>("dashTelegraphLine", parentName);
	dashTelegraphFanMeshComponent = AddComponent<StaticMeshComponent>("dashTelegraphFan", parentName);

	configureMesh(dashTelegraphLineMeshComponent, "./Data/Models/EffectModel/DashTelegraphPlane.glb", { 1.0f, 0.16f, 0.03f, 1.0f });
	configureMesh(dashTelegraphFanMeshComponent, "./Data/Models/EffectModel/DashTelegraphFan.glb", { 1.0f, 0.16f, 0.03f, 1.0f });

}

// 攻撃予兆の表示を更新する。
void GruxEnemy::UpdateAttackTelegraphVisuals(float deltaTime)
{
	UpdateWeaponTelegraphRimLight(deltaTime);
	UpdateJumpTelegraphImpactFlash(deltaTime);
	UpdateDashTelegraphVisual(deltaTime);
}

// 武器の攻撃予兆表示を更新する。
void GruxEnemy::UpdateWeaponTelegraphRimLight(float deltaTime)
{
	if (!weaponTelegraphActive && !weaponTelegraphFadingOut) return;
	if (weaponTelegraphFadingOut)
	{
		weaponTelegraphFadeOutElapsed += (std::max)(0.0f, deltaTime);
		const float duration = (std::max)(0.001f, weaponTelegraphFadeOutDuration);
		weaponTelegraphCurrentPower = weaponTelegraphFadeOutStartPower *
			(1.0f - std::clamp(weaponTelegraphFadeOutElapsed / duration, 0.0f, 1.0f));
		if (weaponTelegraphFadeOutElapsed >= duration) { ResetWeaponTelegraphRimLight(); return; }
	}
	else
	{
		weaponTelegraphElapsed += (std::max)(0.0f, deltaTime);
		float timelineTime = (std::min)(weaponTelegraphElapsed, weaponTelegraphLeadDuration);
		if (const auto controller = GetBodyAnimationController(); controller &&
			controller->GetCurrentAnimationName() == weaponTelegraphAnimationName)
		{
			timelineTime = weaponTelegraphLeadDuration +
				(std::min)(controller->GetCurrentAnimationTime(), weaponTelegraphShowEmissiveEndTime);
			if (controller->GetCurrentAnimationTime() >= weaponTelegraphShowEmissiveEndTime)
			{
				weaponTelegraphActive = false;
				weaponTelegraphFadingOut = true;
				weaponTelegraphFadeOutElapsed = 0.0f;
				weaponTelegraphFadeOutStartPower = weaponTelegraphCurrentPower = weaponTelegraphMaximumPower;
			}
		}
		if (!weaponTelegraphFadingOut)
			weaponTelegraphCurrentPower = weaponTelegraphMaximumPower * std::clamp(
				timelineTime / (weaponTelegraphLeadDuration + weaponTelegraphShowEmissiveEndTime), 0.0f, 1.0f);
	}
	if (skeletalMeshComponent) skeletalMeshComponent->SetRuntimeMaterialRimLight(
		"M_Grux_Qilin_Weapon", weaponTelegraphRimColor, weaponTelegraphCurrentPower);
}

void GruxEnemy::ResetWeaponTelegraphRimLight()
{
	weaponTelegraphActive = false;
	weaponTelegraphFadingOut = false;
	weaponTelegraphCurrentPower = 0.0f;
	weaponTelegraphFadeOutElapsed = 0.0f;
	if (skeletalMeshComponent) skeletalMeshComponent->ClearRuntimeMaterialRimLight("M_Grux_Qilin_Weapon");
}

void GruxEnemy::ResetBehaviorTreeRuntime()
{
	activeNode = nullptr;
	if (behaviorData) behaviorData->Init();
	if (aiTree) aiTree->ResetActionRuntimes();
}

void GruxEnemy::BeginFourthHitReaction(const DirectX::XMFLOAT3& hitSourcePosition)
{
	if (hp <= 0 || isDeathPerform || finalHitReactionActive || finalHitReactionHeld)
		return;

	StopBattleActions();
	ResetBehaviorTreeRuntime();
	ClearPositioningTarget(retreatTarget, retreatMovementRuntime);
	ClearPositioningTarget(repositionTarget, repositionMovementRuntime);

	DirectX::XMFLOAT3 forward = GetForward();
	forward.y = 0.0f;
	DirectX::XMFLOAT3 toPlayer = MathHelper::Subtract(hitSourcePosition, GetPosition());
	toPlayer.y = 0.0f;
	const float forwardLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);
	const float toPlayerLength = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
	float fourthHitReactionLastDirectionDot = forwardLength > FLT_EPSILON && toPlayerLength > FLT_EPSILON ? (forward.x * toPlayer.x + forward.z * toPlayer.z) / (forwardLength * toPlayerLength) : 0.0f;
	const bool frontHit = fourthHitReactionLastDirectionDot >= 0.0f;
	std::string fourthHitReactionAnimation = frontHit ? "HitReact_Front" : "HitReact_Back";
	fourthHitReactionRemaining = std::clamp(fourthHitReactionDuration, 0.0f, 2.0f);
	fourthHitReactionActive = fourthHitReactionRemaining > 0.0f;

	if (const auto controller = GetBodyAnimationController())
		controller->SetRemoveRootTranslationFromPose(fourthHitReactionAnimation, true);
	PlayBodyAnimation(fourthHitReactionAnimation, false, true, 0.1f, true);
}

void GruxEnemy::EndFourthHitReaction()
{
	fourthHitReactionActive = false;
	fourthHitReactionRemaining = 0.0f;
}

void GruxEnemy::ResetFourthHitReaction()
{
	fourthHitReactionActive = false;
	fourthHitReactionRemaining = 0.0f;
}

BossDistanceRegion GruxEnemy::GetDistanceRegion(float distance) const
{
	if (distance <= nearDistanceThreshold)
		return BossDistanceRegion::Near;
	if (distance <= middleDistanceThreshold)
		return BossDistanceRegion::Middle;
	return BossDistanceRegion::Far;
}

void GruxEnemy::EvaluateClampedPositioningTarget(
	const DirectX::XMFLOAT3& startPosition,
	const DirectX::XMFLOAT3& desiredTarget,
	RepositionTargetEvaluation& outEvaluation,
	float boundaryMarginOverride) const
{
	outEvaluation = {};
	outEvaluation.clampedTarget = desiredTarget;
	DirectX::XMFLOAT3 bossRoomTarget{};
	if (ResolveBossRoomPositioningMove(startPosition, desiredTarget, bossRoomTarget,
		outEvaluation.bossRoomPathBlocked))
	{
		outEvaluation.clampedTarget = bossRoomTarget;
	}
	else
	{
		constexpr float maximumMargin =
			(std::min)((bossRoomMaxX - bossRoomMinX) * 0.5f,
				(bossRoomMaxZ - bossRoomMinZ) * 0.5f) - 0.01f;
		const float requestedMargin = boundaryMarginOverride >= 0.0f ? boundaryMarginOverride : bossRoomSafetyMargin;
		const float margin = std::clamp(requestedMargin, 0.0f, maximumMargin);
		outEvaluation.clampedTarget.x = std::clamp(
			outEvaluation.clampedTarget.x, bossRoomMinX + margin, bossRoomMaxX - margin);
		outEvaluation.clampedTarget.z = std::clamp(
			outEvaluation.clampedTarget.z, bossRoomMinZ + margin, bossRoomMaxZ - margin);
	}
	outEvaluation.clampedTarget.y = startPosition.y;

	const float clampX = outEvaluation.clampedTarget.x - desiredTarget.x;
	const float clampZ = outEvaluation.clampedTarget.z - desiredTarget.z;
	outEvaluation.clampDistance = std::sqrt(clampX * clampX + clampZ * clampZ);
}

bool GruxEnemy::IsRepeatPenaltyCombatAttack(BossActionType actionType) const
{
	return actionType == BossActionType::FastCombo ||
		actionType == BossActionType::JumpAttack ||
		actionType == BossActionType::DashAttack ||
		actionType == BossActionType::ChargeAttack;
}

float GruxEnemy::GetRecentAttackPenalty(BossActionType actionType) const
{
	if (!IsRepeatPenaltyCombatAttack(actionType) ||
		!lastStartedCombatAttack || !secondLastStartedCombatAttack ||
		*lastStartedCombatAttack != actionType ||
		*secondLastStartedCombatAttack != actionType)
	{
		return 1.0f;
	}

	return std::clamp(repeatAttackPenalty, 0.05f, 1.0f);
}

// アクションが現在の距離(Region)で候補になるかを判定する関数
//  Action候補とBase WeightからEffective Weightを更新する
void GruxEnemy::UpdateActionCooldowns(float deltaTime)
{
	const float safeDeltaTime = (std::max)(0.0f, deltaTime);
	for (float& remaining : combatActionCooldownRemaining)
		remaining = (std::max)(0.0f, remaining - safeDeltaTime);
}

bool GruxEnemy::HasJustDodgedAttack(const Actor* actor) const
{
	return actor && justDodgedActors.contains(actor);
}

bool GruxEnemy::TryStartJustDodgeSuccess(Player* player)
{
	if (!player || !player->GetJustDodgeWindow() || HasJustDodgedAttack(player))
		return false;

	if (!player->CanJustDodgeAgainst(GetPosition()))
		return false;

	justDodgedActors.insert(player);
	player->StartJustDodgeSuccess(
		std::dynamic_pointer_cast<Enemy>(shared_from_this()));
	return true;
}

void GruxEnemy::RefreshActiveDangerAreaFromNotify()
{
	if (!isDangerWindow || !activeDangerNotifyState)
		return;
	DirectX::XMFLOAT3 justDodgeAreaOffset = activeDangerNotifyState->justDodgeAreaOffset;
	DirectX::XMFLOAT3 justDodgeAreaSize = activeDangerNotifyState->justDodgeAreaSize;
	justDodgeAreaSize.x = (std::max)(0.0f, justDodgeAreaSize.x);
	justDodgeAreaSize.y = (std::max)(0.0f, justDodgeAreaSize.y);
	justDodgeAreaSize.z = (std::max)(0.0f, justDodgeAreaSize.z);
	dangerArea = BuildDangerArea(GetPosition(), GetRight(), GetUp(), GetForward(), justDodgeAreaOffset, justDodgeAreaSize);
}
void GruxEnemy::PrepareJumpAttackMotionWarpOverride()
{
	ClearJumpAttackMotionWarpOverride();

	const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
	if (!player)
	{
		calculatedJumpDistance = 0.0f;
		jumpMotionWarpDirection = { 0.0f, 0.0f, 0.0f };
		jumpMotionWarpOverrideActive = true;
		return;
	}

	const DirectX::XMFLOAT3 bossPosition = GetPosition();
	jumpAttackStartPlayerPosition = player->GetPosition();
	const float dx = jumpAttackStartPlayerPosition.x - bossPosition.x;
	const float dz = jumpAttackStartPlayerPosition.z - bossPosition.z;
	const float playerDistance = std::sqrt(dx * dx + dz * dz);

	const float desiredMoveDistance = playerDistance - desiredAttackDistance;
	calculatedJumpDistance = std::clamp(desiredMoveDistance, 0.0f, maxJumpDistance);

	if (playerDistance > FLT_EPSILON)
	{
		const float inverseDistance = 1.0f / playerDistance;
		jumpMotionWarpDirection = { dx * inverseDistance, 0.0f, dz * inverseDistance };
		rotationComponent->SetDirectionImmediate(jumpMotionWarpDirection);
	}
	else
	{
		jumpMotionWarpDirection = { 0.0f, 0.0f, 0.0f };
	}
	jumpMotionWarpOverrideActive = true;
}
void GruxEnemy::RefreshActiveHitBoxesFromNotifyStates()
{
	leftHitBox = false;
	rightHitBox = false;
	activeLeftHitBoxRadius = hitWeaponRadius;
	activeRightHitBoxRadius = hitWeaponRadius;
	activeLeftHitBoxOffset = {};
	activeRightHitBoxOffset = {};

	for (const AnimationNotifyState* activeState : activeHitBoxNotifyStates)
	{
		if (!activeState)
			continue;
		const float radius = (activeState->hitBoxRadius < 0.01f ? 0.01f : activeState->hitBoxRadius);
		if (activeState->parameter == leftWeapon || activeState->parameter == bothWeapon)
		{
			leftHitBox = true;
			activeLeftHitBoxRadius = radius;
			activeLeftHitBoxOffset = activeState->hitBoxOffset;
		}
		if (activeState->parameter == rightWeapon || activeState->parameter == bothWeapon)
		{
			rightHitBox = true;
			activeRightHitBoxRadius = radius;
			activeRightHitBoxOffset = activeState->hitBoxOffset;
		}
	}
}

GruxEnemy::PositioningMoveResult GruxEnemy::UpdatePositioningTargetMovement(PositioningTargetContext& target, PositioningTargetRuntime& runtime, float dt)
{
	if (!target.valid)
		return PositioningMoveResult::InvalidTarget;
	if (!runtime.movementActive)
	{
		runtime.movementActive = true;
		runtime.previousPosition = GetPosition();
	}
	const float safeDt = (std::max)(0.0f, dt);
	runtime.elapsed += safeDt;
	const auto position = GetPosition();
	const float dx = target.targetPosition.x - position.x;
	const float dz = target.targetPosition.z - position.z;
	float remainingDistance = std::sqrt(dx * dx + dz * dz);
	if (remainingDistance <= target.arrivalTolerance)
		return PositioningMoveResult::Arrived;
	if (runtime.elapsed >= target.timeout)
		return PositioningMoveResult::Timeout;
	const float movedX = position.x - runtime.previousPosition.x;
	const float movedZ = position.z - runtime.previousPosition.z;
	const float frameMovement = std::sqrt(movedX * movedX + movedZ * movedZ);
	runtime.traveledDistance += frameMovement;
	runtime.previousPosition = position;
	runtime.stuckTime = frameMovement < target.stuckMovementThreshold
		? runtime.stuckTime + safeDt : 0.0f;
	if (runtime.traveledDistance >= target.maxMoveDistance)
		return PositioningMoveResult::MaxDistanceReached;
	if (runtime.stuckTime >= target.stuckTimeThreshold)
		return PositioningMoveResult::Stuck;
	if (!characterMovementComponent || !rotationComponent)
		return PositioningMoveResult::InvalidTarget;
	const DirectX::XMFLOAT3 direction{ dx / remainingDistance, 0.0f,dz / remainingDistance };
	characterMovementComponent->SetFixedSpeed(target.moveSpeed);
	characterMovementComponent->SetInputMagnitude(1.0f);
	characterMovementComponent->SetMoveDirection(direction);
	RotateTowardsPlayer(direction, GetTurnSpeed(), safeDt);
	UpdatePositioningAnimation(safeDt > 0.0f ? frameMovement / safeDt : 0.0f, safeDt);
	return PositioningMoveResult::Running;
}

void GruxEnemy::ClearPositioningTarget(PositioningTargetContext& target, PositioningTargetRuntime& runtime)
{
	target = {};
	runtime = {};
}

// ジャンプ攻撃の後に着地の時のエフェクトを生成する
void GruxEnemy::SpawnGroundImpactEffect() const
{
	DirectX::XMFLOAT3 spawnPosition = GetPosition();
	if (weaponRightTipComponent)
	{
		const DirectX::XMFLOAT3 weaponTipPosition = weaponRightTipComponent->GetComponentLocation();
		spawnPosition.x = weaponTipPosition.x;
		spawnPosition.z = weaponTipPosition.z;
	}

	if (groundDustEffectComponent)
	{
		// 武器の場所に生成する
		groundDustEffectComponent->SetWorldLocationDirect(spawnPosition);
		groundDustEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(groundDustEffectComponent->GetEffectHandle(), groundDustEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });

		// 足元に生成する
		DirectX::XMFLOAT3 groundDustPosition = GetPosition();
		groundDustEffectComponent->SetWorldLocationDirect(groundDustPosition);
		groundDustEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(groundDustEffectComponent->GetEffectHandle(), groundDustEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });
	}

	// 瓦礫を生成する
	if (const auto debrisEmitter = GetOwnerScene()->GetActorManager()->GetActorOfType<ModelDebrisEmitterActor>())
	{
		debrisEmitter->Emit(spawnPosition);
	}
}




void GruxEnemy::SpawnWallImpactEffect(
	const DirectX::XMFLOAT3& impactPosition,
	const DirectX::XMFLOAT3& wallNormal) const
{
	DirectX::XMVECTOR normal = DirectX::XMLoadFloat3(&wallNormal);
	if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(normal)) <= FLT_EPSILON)
		normal = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	else
		normal = DirectX::XMVector3Normalize(normal);

	DirectX::XMFLOAT3 normalizedWallNormal{};
	DirectX::XMStoreFloat3(&normalizedWallNormal, normal);

	if (wallImpactFlashEffectComponent)
	{
		constexpr float flashSurfaceOffset = 0.03f;
		const DirectX::XMFLOAT3 flashPosition{
			impactPosition.x + normalizedWallNormal.x * flashSurfaceOffset,
			impactPosition.y + normalizedWallNormal.y * flashSurfaceOffset,
			impactPosition.z + normalizedWallNormal.z * flashSurfaceOffset };
		wallImpactFlashEffectComponent->SetWorldLocationDirect(flashPosition);
		wallImpactFlashEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(
			wallImpactFlashEffectComponent->GetEffectHandle(),
			wallImpactFlashEffectComponent->GetComponentLocation(),
			{ 0.0f, 0.0f, 0.0f });
	}

	if (wallImpactDustEffectComponent)
	{
		const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		const float upDotNormal = std::clamp(
			DirectX::XMVectorGetX(DirectX::XMVector3Dot(up, normal)), -1.0f, 1.0f);
		DirectX::XMVECTOR rotation = DirectX::XMQuaternionIdentity();
		DirectX::XMVECTOR rotationAxis = DirectX::XMVector3Cross(up, normal);
		if (DirectX::XMVectorGetX(
			DirectX::XMVector3LengthSq(rotationAxis)) > FLT_EPSILON)
		{
			rotationAxis = DirectX::XMVector3Normalize(rotationAxis);
			rotation = DirectX::XMQuaternionRotationAxis(
				rotationAxis, std::acos(upDotNormal));
		}
		else if (upDotNormal < 0.0f)
		{
			rotation = DirectX::XMQuaternionRotationAxis(
				DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), DirectX::XM_PI);
		}

		DirectX::XMFLOAT4 wallRotation{};
		DirectX::XMStoreFloat4(&wallRotation, rotation);
		wallImpactDustEffectComponent->SetWorldLocationDirect(impactPosition);
		wallImpactDustEffectComponent->SetWorldRotationDirect(wallRotation);
		wallImpactDustEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(
			wallImpactDustEffectComponent->GetEffectHandle(),
			wallImpactDustEffectComponent->GetComponentLocation(),
			wallImpactDustEffectComponent->GetComponentEulerRotation());
	}

	if (const auto debrisEmitter =
		GetOwnerScene()->GetActorManager()->GetActorOfType<ModelDebrisEmitterActor>())
	{
		debrisEmitter->Emit(impactPosition, normalizedWallNormal);
	}
}

// 武器同士の火花のエフェクトを生成する
void GruxEnemy::SpawnWeaponClashEffect() const
{
	DirectX::XMFLOAT3 spawnPosition = GetPosition();
	if (weaponRightMiddleComponent)
	{
		const DirectX::XMFLOAT3 weaponTipPosition = weaponRightMiddleComponent->GetComponentLocation();
		spawnPosition.x = weaponTipPosition.x;
		spawnPosition.y = weaponTipPosition.y;
		spawnPosition.z = weaponTipPosition.z;
	}

	if (metalSparkEffectComponent)
	{
		metalSparkEffectComponent->SetWorldLocationDirect(spawnPosition);
		metalSparkEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(
			metalSparkEffectComponent->GetEffectHandle(),
			metalSparkEffectComponent->GetComponentLocation(),
			{ 0.0f, 0.0f, 0.0f });
	}
}

// 左足を地面に擦る時のエフェクトを生成する
void  GruxEnemy::SpawnLeftFootScrapeEffect()const
{
	DirectX::XMFLOAT3 spawnPosition = GetPosition();
	if (leftFootComponent)
	{
		const DirectX::XMFLOAT3 leftFootPosition = leftFootComponent->GetComponentLocation();
		spawnPosition.x = leftFootPosition.x;
		spawnPosition.z = leftFootPosition.z;
	}

	if (footScrapeEffectComponent)
	{
		footScrapeEffectComponent->SetWorldLocationDirect(spawnPosition);
		footScrapeEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(
			footScrapeEffectComponent->GetEffectHandle(),
			footScrapeEffectComponent->GetComponentLocation(),
			{ 0.0f, 0.0f, 0.0f });
	}
}


// 右足を地面に擦る時のエフェクトを生成する
void  GruxEnemy::SpawnRightFootScrapeEffect()const
{
	DirectX::XMFLOAT3 spawnPosition = GetPosition();
	if (rightFootComponent)
	{
		const DirectX::XMFLOAT3 rightFootPosition = rightFootComponent->GetComponentLocation();
		spawnPosition.x = rightFootPosition.x;
		spawnPosition.z = rightFootPosition.z;
	}

	if (footScrapeEffectComponent)
	{
		footScrapeEffectComponent->SetWorldLocationDirect(spawnPosition);
		footScrapeEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(footScrapeEffectComponent->GetEffectHandle(), footScrapeEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });
	}
}


// 地面に倒れたときのエフェクトを生成する
void GruxEnemy::SpawnGroundDownEffect()const
{
	DirectX::XMFLOAT3 spawnPosition = GetPosition();
	if (neckComponent)
	{
		const DirectX::XMFLOAT3 beltPosition = neckComponent->GetComponentLocation();
		spawnPosition.x = beltPosition.x;
		spawnPosition.z = beltPosition.z;
	}

	if (groundDustEffectComponent)
	{
		// 武器の場所に生成する
		groundDustEffectComponent->SetWorldLocationDirect(spawnPosition);
		groundDustEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(groundDustEffectComponent->GetEffectHandle(), groundDustEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });

		// 足元に生成する
		DirectX::XMFLOAT3 groundDustPosition = GetPosition();
		groundDustEffectComponent->SetWorldLocationDirect(groundDustPosition);
		groundDustEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(groundDustEffectComponent->GetEffectHandle(), groundDustEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });
	}

	// 瓦礫を生成する
	if (const auto debrisEmitter = GetOwnerScene()->GetActorManager()->GetActorOfType<ModelDebrisEmitterActor>())
	{
		debrisEmitter->Emit(spawnPosition);
	}
}

void GruxEnemy::SpawnDropLeftWeaponEffect()const
{
	DirectX::XMFLOAT3 spawnPosition = GetPosition();
	if (weaponLeftRootComponent)
	{
		const DirectX::XMFLOAT3 weaponPosition = weaponLeftRootComponent->GetComponentLocation();
		spawnPosition.x = weaponPosition.x;
		spawnPosition.z = weaponPosition.z;
	}

	if (groundDustEffectComponent)
	{
		// 武器の場所に生成する
		groundDustEffectComponent->SetWorldLocationDirect(spawnPosition);
		groundDustEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(groundDustEffectComponent->GetEffectHandle(), groundDustEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });

		// 足元に生成する
		DirectX::XMFLOAT3 groundDustPosition = GetPosition();
		groundDustEffectComponent->SetWorldLocationDirect(groundDustPosition);
		groundDustEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(groundDustEffectComponent->GetEffectHandle(), groundDustEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });
	}

	// 瓦礫を生成する
	if (const auto debrisEmitter = GetOwnerScene()->GetActorManager()->GetActorOfType<ModelDebrisEmitterActor>())
	{
		debrisEmitter->Emit(spawnPosition);
	}
}

void GruxEnemy::SpawnDropRightWeaponEffect()const
{
	DirectX::XMFLOAT3 spawnPosition = GetPosition();
	if (weaponRightRootComponent)
	{
		const DirectX::XMFLOAT3 weaponPosition = weaponRightRootComponent->GetComponentLocation();
		spawnPosition.x = weaponPosition.x;
		spawnPosition.z = weaponPosition.z;
	}

	if (groundDustEffectComponent)
	{
		// 武器の場所に生成する
		groundDustEffectComponent->SetWorldLocationDirect(spawnPosition);
		groundDustEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(groundDustEffectComponent->GetEffectHandle(), groundDustEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });

		// 足元に生成する
		DirectX::XMFLOAT3 groundDustPosition = GetPosition();
		groundDustEffectComponent->SetWorldLocationDirect(groundDustPosition);
		groundDustEffectComponent->UpdateComponentToWorld();
		EffectManager::EmitParticle(groundDustEffectComponent->GetEffectHandle(), groundDustEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });
	}

	// 瓦礫を生成する
	if (const auto debrisEmitter = GetOwnerScene()->GetActorManager()->GetActorOfType<ModelDebrisEmitterActor>())
	{
		debrisEmitter->Emit(spawnPosition);
	}
}

void GruxEnemy::ApplyHpBarVisualProfile(const HpBarVisualProfile profile)
{
	hpBarVisualProfile = profile;
	constexpr DirectX::XMFLOAT2 hpPivot{ 0.0f, 0.5f };
	if (profile == HpBarVisualProfile::Phase1)
	{
		const DirectX::XMFLOAT2 position{ 650.0f, 115.0f };
		const DirectX::XMFLOAT2 scale{ 0.35f, 0.35f };
		if (hpBackgroundUiComponent)
		{
			hpBackgroundUiComponent->SetTexture(hpPhase1BackgroundTexture);
			hpBackgroundUiComponent->SetWorldPosition(position);
			hpBackgroundUiComponent->SetSize({ 1904.0f, 43.0f });
			hpBackgroundUiComponent->SetPivot(hpPivot);
			hpBackgroundUiComponent->SetScale(scale);
			hpBackgroundUiComponent->SetColor(CoreColor::White);
		}
		if (hpDelayedFillUiComponent)
		{
			hpDelayedFillUiComponent->SetTexture(hpPhase1FillTexture);
			hpDelayedFillUiComponent->SetWorldPosition(position);
			hpDelayedFillUiComponent->SetSize({ 1897.0f, 36.0f });
			hpDelayedFillUiComponent->SetPivot(hpPivot);
			hpDelayedFillUiComponent->SetScale(scale);
			hpDelayedFillUiComponent->SetColor(bossHpDelayedColor);
		}
		if (hpCurrentFillUiComponent)
		{
			hpCurrentFillUiComponent->SetTexture(hpPhase1FillTexture);
			hpCurrentFillUiComponent->SetWorldPosition(position);
			hpCurrentFillUiComponent->SetSize({ 1897.0f, 36.0f });
			hpCurrentFillUiComponent->SetPivot(hpPivot);
			hpCurrentFillUiComponent->SetScale(scale);
			hpCurrentFillUiComponent->SetColor(bossHpCurrentColor);
		}
		if (hpFrameUiComponent)
		{
			hpFrameUiComponent->SetTexture(hpPhase1FrameTexture);
			hpFrameUiComponent->SetWorldPosition(position);
			hpFrameUiComponent->SetSize({ 1909.0f, 49.0f });
			hpFrameUiComponent->SetPivot(hpPivot);
			hpFrameUiComponent->SetScale(scale);
			hpFrameUiComponent->SetColor(CoreColor::White);
		}
		return;
	}

	if (!IsPhase2HpBarPresentationActive())
	{
		phase2HpBarPresentationFrameProgress = 1.0f;
		phase2HpBarPresentationFillProgress = 1.0f;
	}
	if (hpBackgroundUiComponent) hpBackgroundUiComponent->SetTexture(hpPhase2BackgroundTexture);
	if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetTexture(hpPhase2FillTexture);
	if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetTexture(hpPhase2FillTexture);
	if (hpFrameUiComponent) hpFrameUiComponent->SetTexture(hpPhase2FrameTexture);
	UpdatePhase2HpBarPresentationLayout();
}

void GruxEnemy::ResetPhase2HpBarPresentation()
{
	phase2HpBarPresentationState = Phase2HpBarPresentationState::Hidden;
	phase2HpBarPresentationElapsed = 0.0f;
	phase2HpBarPresentationFrameProgress = 0.0f;
	phase2HpBarPresentationFillProgress = 0.0f;
	const auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene());
	ApplyHpBarVisualProfile(gameScene && gameScene->IsBossInFinalPhase()
		? HpBarVisualProfile::Phase2 : HpBarVisualProfile::Phase1);
	const float maximumHp = static_cast<float>((std::max)(maxHp, 1));
	if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetValue(static_cast<float>((std::max)(hp, 0)), maximumHp);
	if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetValue(delayedHp, maximumHp);
	SetHpBarVisible(phase2HpBarPresentationRestoreVisible);
	phase2HpBarPresentationRestoreVisible = false;
}

// 第2段階HPバーのプレビューを更新する。
void GruxEnemy::UpdatePhase2HpBarPresentation()
{
	if (!IsPhase2HpBarPresentationActive())
		return;

	phase2HpBarPresentationElapsed += (std::max)(0.0f, Time::UnscaledDeltaTime());
	const float frameDuration = (std::max)(0.001f, phase2HpBarPresentationFrameRevealDuration);
	const float fillDuration = (std::max)(0.001f, phase2HpBarPresentationFillDuration);
	phase2HpBarPresentationFrameProgress = std::clamp(phase2HpBarPresentationElapsed / frameDuration, 0.0f, 1.0f);
	phase2HpBarPresentationFillProgress = std::clamp(
		(phase2HpBarPresentationElapsed - phase2HpBarPresentationFillDelay) / fillDuration, 0.0f, 1.0f);
	if (phase2HpBarPresentationFrameProgress < 1.0f)
		phase2HpBarPresentationState = Phase2HpBarPresentationState::RevealingFrame;
	else if (phase2HpBarPresentationElapsed < phase2HpBarPresentationFillDelay)
		phase2HpBarPresentationState = Phase2HpBarPresentationState::WaitingForFill;
	else if (phase2HpBarPresentationFillProgress < 1.0f)
		phase2HpBarPresentationState = Phase2HpBarPresentationState::Filling;
	else
		phase2HpBarPresentationState = Phase2HpBarPresentationState::Complete;
	UpdatePhase2HpBarPresentationLayout();
}


void GruxEnemy::UpdatePhase2HpBarPresentationLayout()
{
	if (hpBarVisualProfile != HpBarVisualProfile::Phase2)
		return;

	constexpr float frameTextureWidth = 2559.0f;
	constexpr float frameTextureHeight = 49.0f;
	constexpr float barTextureWidth = 2554.0f;
	constexpr float barTextureHeight = 45.0f;
	const float frameProgress = std::clamp(phase2HpBarPresentationFrameProgress, 0.0f, 1.0f);
	const float fillProgress = std::clamp(phase2HpBarPresentationFillProgress, 0.0f, 1.0f);
	const float frameWidth = (std::max)(0.0f, phase2HpBarPresentationFrameSize.x);
	const float frameHeight = (std::max)(0.0f, phase2HpBarPresentationFrameSize.y);
	const DirectX::XMFLOAT2 barSize{ frameWidth * (barTextureWidth / frameTextureWidth),
		frameHeight * (barTextureHeight / frameTextureHeight) };
	constexpr DirectX::XMFLOAT2 pivot{ 0.0f, 0.5f };

	if (hpFrameUiComponent)
	{
		hpFrameUiComponent->SetWorldPosition(phase2HpBarPresentationPosition);
		hpFrameUiComponent->SetPivot(pivot);
		hpFrameUiComponent->SetScale({ 1.0f, 1.0f });
		hpFrameUiComponent->SetSize({ frameWidth * frameProgress, frameHeight });
		hpFrameUiComponent->SetUV({ 0.0f, 0.0f, frameTextureWidth * frameProgress, frameTextureHeight });
		hpFrameUiComponent->SetColor(CoreColor::White);
	}
	if (hpBackgroundUiComponent)
	{
		hpBackgroundUiComponent->SetWorldPosition(phase2HpBarPresentationPosition);
		hpBackgroundUiComponent->SetPivot(pivot);
		hpBackgroundUiComponent->SetScale({ 1.0f, 1.0f });
		hpBackgroundUiComponent->SetSize({ barSize.x * frameProgress, barSize.y });
		hpBackgroundUiComponent->SetUV({ 0.0f, 0.0f, barTextureWidth * frameProgress, barTextureHeight });
		hpBackgroundUiComponent->SetColor(CoreColor::White);
	}
	const float displayedFill = (std::min)(fillProgress, frameProgress);
	for (const auto& fill : { hpDelayedFillUiComponent, hpCurrentFillUiComponent })
	{
		if (!fill) continue;
		fill->SetWorldPosition(phase2HpBarPresentationPosition);
		fill->SetPivot(pivot);
		fill->SetScale({ 1.0f, 1.0f });
		fill->SetSize(barSize);
		fill->SetValue(displayedFill);
	}
	if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetColor(bossHpDelayedColor);
	if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetColor(bossHpCurrentColor);
}

bool GruxEnemy::IsPhase2HpBarPresentationActive() const
{
	return phase2HpBarPresentationState != Phase2HpBarPresentationState::Hidden;
}

GruxEnemy::WeaponHitBoxPoints GruxEnemy::BuildWeaponHitBoxPoints(
	const std::shared_ptr<SceneComponent>& root,
	const std::shared_ptr<SceneComponent>& middle,
	const std::shared_ptr<SceneComponent>& tip,
	const DirectX::XMFLOAT3& localOffset) const
{
	const auto offsetPoint = [&localOffset](const std::shared_ptr<SceneComponent>& component)
		{
			if (!component)
				return DirectX::XMFLOAT3{};
			DirectX::XMVECTOR offset = DirectX::XMLoadFloat3(&localOffset);
			const DirectX::XMFLOAT4 componentRotation =
				component->GetComponentWorldTransform().GetRotation();
			offset = DirectX::XMVector3Rotate(offset,
				DirectX::XMLoadFloat4(&componentRotation));
			DirectX::XMFLOAT3 worldOffset{};
			DirectX::XMStoreFloat3(&worldOffset, offset);
			const DirectX::XMFLOAT3 base = component->GetComponentLocation();
			return DirectX::XMFLOAT3{ base.x + worldOffset.x, base.y + worldOffset.y,
				base.z + worldOffset.z };
		};
	return { offsetPoint(root), offsetPoint(middle), offsetPoint(tip) };
}

// 第2段階のテクスチャ切り替え範囲をワールド座標で更新する。
void GruxEnemy::UpdatePhase2TextureBlendWorldYRange()
{
	const float currentBossScale = (std::max)(GetScale().y, 0.0001f);
	const float bossWorldY = GetPosition().y;
	const float phase2WorldMinY = bossWorldY + phase2MaskMinYOffset * currentBossScale;
	const float phase2WorldMaxY = (std::max)(
		bossWorldY + phase2MaskMaxYOffset * currentBossScale,
		phase2WorldMinY + 0.0001f);
	if (skeletalMeshComponent)
		skeletalMeshComponent->SetPhase2TextureBlendWorldYRange(
			phase2WorldMinY, phase2WorldMaxY, phase2MaskSoftness);
}

