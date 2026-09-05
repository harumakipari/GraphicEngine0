#include "pch.h"

#include "GruxEnemy.h"

#include "Components/Render/PointLightComponent.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Camera/DarkGameCamera.h"
#include "Game/Actors/Enemy/Boss/BossState.h"
#include "Game/Actors/Player/Player.h"
#include "Game/DarkGame/DarkActors/IceFragmentEffectActor.h"
#include "Game/DarkGame/DarkActors/ModelDebrisEmitterActor.h"
#include "Physics/CollisionFunction.h"
#include <random>

#ifdef USE_IMGUI
namespace
{
    void DrawIntentWeightBar(const char* label, float& weight,
        float totalWeight, const bool editable = true)
    {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);

        const float barWidth = (std::max)(180.0f,
            (std::min)(420.0f, ImGui::GetContentRegionAvail().x - 105.0f));
        constexpr float barHeight = 22.0f;
        const float previousWeight = weight;
        ImGui::InvisibleButton("##IntentWeightBar",
            ImVec2(barWidth, barHeight));
        if (editable && ImGui::IsItemActive())
        {
            constexpr float weightPerPixel = 1.0f;
            weight = std::clamp(weight +
                ImGui::GetIO().MouseDelta.x * weightPerPixel,
                0.0f, 1000.0f);
        }

        totalWeight += weight - previousWeight;
        const float probability = totalWeight > 0.0f
            ? std::clamp(weight / totalWeight, 0.0f, 1.0f)
            : 0.0f;
        const ImVec2 barMin = ImGui::GetItemRectMin();
        const ImVec2 barMax = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(barMin, barMax,
            ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f);
        if (probability > 0.0f)
        {
            const ImVec2 fillMax{
                barMin.x + (barMax.x - barMin.x) * probability,
                barMax.y };
            drawList->AddRectFilled(barMin, fillMax,
                ImGui::GetColorU32(editable
                    ? ImGuiCol_PlotHistogram
                    : ImGuiCol_TextDisabled), 4.0f);
        }
        drawList->AddRect(barMin, barMax,
            ImGui::GetColorU32(ImGuiCol_Border), 4.0f);

        char probabilityText[32]{};
        sprintf_s(probabilityText, "%.1f%%", probability * 100.0f);
        const ImVec2 textSize = ImGui::CalcTextSize(probabilityText);
        drawList->AddText(
            ImVec2(barMin.x + (barWidth - textSize.x) * 0.5f,
                barMin.y + (barHeight - textSize.y) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_Text), probabilityText);

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Base Weight: %.1f", weight);
            ImGui::Text("Base Probability: %.1f%%", probability * 100.0f);
            if (editable)
                ImGui::TextDisabled("Drag left/right to change weight");
            else
                ImGui::TextDisabled("Disabled for Back region");
            ImGui::EndTooltip();
        }

        if (editable)
        {
            ImGui::SetNextItemWidth(140.0f);
            ImGui::DragFloat("Weight", &weight, 0.5f, 0.0f, 1000.0f, "%.1f");
            weight = std::clamp(weight, 0.0f, 1000.0f);
        }
        else
        {
            ImGui::TextDisabled("Disabled / 0%%");
        }
        ImGui::Spacing();
        ImGui::PopID();
    }
}
#endif

void GruxEnemy::Initialize(const Transform& transform)
{
    maxHp = 10;
    //maxHp = 75;
    hp = maxHp;
    delayedHp = static_cast<float>(hp);

    std::string parentName = "GruxEnemy";
    Character::Initialize(transform);
    skeletalMeshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    skeletalMeshComponent->SetModel("./Data/Models/Characters/GruxQilin/boss.gltf", false, true);
    skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Enemy;   // オブジェクトの種類を Enemy に設定
    skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.6f;   // 目玉の自己発光の強さを設定
    skeletalMeshComponent->plusAlphaCBuffer->data.cpuColor = { 0.9f,0.08f,0.08f,1.0f };   // 目玉の色を赤にしてみる
    skeletalMeshComponent->overrideDeferredPipelineName = "GltfModelDeferredGruxPS";

    for (auto& material : skeletalMeshComponent->model->materials)
    {
        if (material.name == "M_Grux_Qilin_Eye")
        {// 目だったら、
            material.materialType = MaterialType::Eye;
        }
        else if (material.name == "M_Grux_Qilin_Gear")
        {// 腕輪
            material.materialType = MaterialType::Metallic;
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

    // アニメーションコントローラーを作成
    int rootIndex = skeletalMeshComponent->FindIndexByName("root");
    auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootIndex);
    controller->AddAnimation("Idle", 0);
    controller->AddAnimation("Jog_Fwd_0", 1);
    controller->AddAnimation("PrimaryAttack_LA", 2);
    controller->AddAnimation("PrimaryAttack_RA", 3);
    controller->AddAnimation("PrimaryAttack_JumpAttack", 4);
    controller->AddAnimation("PrimaryAttack_SweepAway", 5);
    controller->AddAnimation("PrimaryAttack_Recall", 6);
    controller->AddAnimation("PrimaryAttack_Fire", 7);
    controller->AddAnimation("Stampede_0", 8);
    controller->AddAnimation("Stampede_Knockup_0", 9);
    controller->AddAnimation("Stun_Idle", 10);
    controller->AddAnimation("TravelMode_Fwd_0", 11);
    controller->AddAnimation("TravelMode_Idle_0", 12);
    controller->AddAnimation("Ultimate_Roar_0", 13);
    controller->AddAnimation("Jump_Land_0", 14);
    controller->AddAnimation("Jump_Loop_0", 15);
    controller->AddAnimation("Jump_Start_0", 16);
    controller->AddAnimation("Jump_Land_1", 17);
    controller->AddAnimation("Death_A_0", 18);
    controller->AddAnimation("Death_B_0", 19);
    controller->AddAnimation("Attack_A_Fast_0", 20);
    controller->AddAnimation("Attack_B_Fast_0", 21);
    controller->AddAnimation("Attack_C_Fast_0", 22);
    controller->AddAnimation("Dodge_B_180_Seq_0", 23);
    controller->AddAnimation("TravelMode_Start_0", 24);
    controller->AddAnimation("Pre_Stampede_0", 25);
    controller->AddAnimation("Pre_FootSlide_0", 26);
    controller->AddAnimation("Knock_Down_Death", 27);
    controller->AddAnimation("Knock_Down_End", 28);
    controller->AddAnimation("Knock_Down_Loop", 29);
    controller->AddAnimation("Knock_Down_Start", 30);
    // Death clipだけRoot Translationを含むため、Actor位置へ適用せずPoseもin-place化する。
    controller->SetRemoveRootTranslationFromPose("Knock_Down_Death", true);

    // 全てのNotifyAssetsをロードする
    controller->LoadAllNotifyAssets(GetName());
    CaptureInitialDangerObbSettings();

    // ステートマシンを作成
    {
        stateMachine_ = std::make_shared<StateMachine>();
        stateMachine_->RegisterState(std::make_unique<EnemyIdleState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyDeathState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyThinkState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyTurnState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyAttackReadyState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyAttackState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyChargeAttackState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyStunState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyRecoveryState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyPositioningState>(this));

        // ステートマシンを character に追加
        this->SetStateMachine(stateMachine_);
        // 初期ステートを設定
        //stateMachine_->ChangeState("EnemyIdleState");
    }

    // アニメーションコントローラーを character に追加
    this->AddBodyAnimationController(controller);
    // アニメーションコントローラーのオーナーの名前を設定する
    controller->SetOwnerName(GetName());
    PlayBodyAnimation("TravelMode_Idle_0");

#if 1
    //　身体の当たり判定
    {
        std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("enemyCapsuleComponent", parentName);
        //DirectX::XMFLOAT3 size = skeletalMeshComponent->GetModelSize();
        //size = MathHelper::Multiply(size, GetScale().x);
        //height = size.y;
        //radius = size.x * 0.3f;
        height = 2.7f * 1.3f;
        radius = 1.5f * 1.3f;

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
    }

#endif // 0

    // 回転用コンポーネントを追加
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

    // 武器に当たり判定のコンポーネントを追加
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

    // 左足のコンポーネントを追加
    int socketLeftFootNode = skeletalMeshComponent->FindIndexByName("ik_foot_l");
    leftFootComponent = AddComponent<SceneComponent>("leftFootComponent", parentName);
    leftFootComponent->AttachToComponent(skeletalMeshComponent, socketLeftFootNode); // "ik_foot_l"

    // 右足のコンポーネントを追加
    int socketRightFootNode = skeletalMeshComponent->FindIndexByName("ik_foot_r");
    rightFootComponent = AddComponent<SceneComponent>("rightFootComponent", parentName);
    rightFootComponent->AttachToComponent(skeletalMeshComponent, socketRightFootNode); // "ik_foot_r"

    int leftEyeSocketNode = skeletalMeshComponent->FindIndexByName("L_eye");
    int rightEyeSocketNode = skeletalMeshComponent->FindIndexByName("R_eye");

    // 左目の位置用コンポーネントを追加　暗闇で光る目の表現用
    leftEyeSceneComponent = this->AddComponent<SceneComponent>("leftEye", parentName);
    leftEyeSceneComponent->AttachToComponent(skeletalMeshComponent, leftEyeSocketNode);
    leftEyeSceneComponent->SetRelativeLocationDirect({ 0.0f,0.03f,-0.1f });

    // 右目の位置用コンポーネントを追加　暗闇で光る目の表現用
    rightEyeSceneComponent = this->AddComponent<SceneComponent>("rightEye", parentName);
    rightEyeSceneComponent->AttachToComponent(skeletalMeshComponent, rightEyeSocketNode);
    rightEyeSceneComponent->SetRelativeLocationDirect({ 0.0f,0.03f,-0.1f });

    // カメラの注視点の位置のコンポーネントを追加
    cameraTargetComponent = AddComponent<SceneComponent>("cameraTargetComponent", parentName);
    cameraTargetComponent->SetRelativeLocationDirect({ 0.0f,1.5f,-0.0f });
    // 登場シーンのボス名前のUIを追加
    auto uiManager = GetOwnerScene()->GetUIManager();
    gruxNameImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/Grux_name.png", "Grux_name");
    gruxNameImageComponent->SetWorldPosition({ 995, 900 });
    gruxNameImageComponent->SetScale({ 0.8f,0.8f });
    gruxNameImageComponent->SetSize({ 600, 200 });
    gruxNameImageComponent->SetPivot({ 0.5f,0.5f });
    gruxNameImageComponent->SetColor(DirectX::XMFLOAT4{ 1.0f,1.0f,1.0f,0.0f });
    gruxNameImageComponent->SetVisible(true);
    uiManager->Add(gruxNameImageComponent);
    easingRunner = std::make_unique<EasingRunner>();

    // ロックオンのモデル
    lockOnTargetMeshComponent = AddComponent<SkeletalMeshComponent>("lockOn", parentName);
    lockOnTargetMeshComponent->SetModel("./Data/Models/LockOnTarget/LockOnTargetModel1.gltf");
    lockOnTargetMeshComponent->SetRelativeScaleDirect({ 0.68f,0.68f, 0.68f });
    lockOnTargetMeshComponent->SetIsCastShadow(false);
    lockOnTargetMeshComponent->SetIsVisible(false);
    lockOnTargetMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::NoLighting;

    lockOnTargetImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/lock_on.png", "lockOn");
    lockOnTargetImageComponent->SetVisible(true);
    lockOnTargetImageComponent->SetPivot({ 0.5f,0.5f });
    lockOnTargetImageComponent->SetSize({ 150.0f,150.0f });
    uiManager->Add(lockOnTargetImageComponent);

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


    leftWeaponTrail.Initialize();
    rightWeaponTrail.Initialize();
    leftWeaponTrail.SetRushColorEnabled(true, bossTrailColor);
    rightWeaponTrail.SetRushColorEnabled(true, bossTrailColor);
    leftWeaponTrail.SetEmissiveStrength(bossTrailEmissiveStrength);
    rightWeaponTrail.SetEmissiveStrength(bossTrailEmissiveStrength);
    leftWeaponTrail.SetFadeLifetime(bossTrailLifetime);
    rightWeaponTrail.SetFadeLifetime(bossTrailLifetime);

    //  ボスHP UI：名前 -> 背景 → 遅延塗りつぶし → 現在の塗りつぶし → フレーム。
    const DirectX::XMFLOAT2 hpNamePosition = { 980.0f, 85.0f };
    const DirectX::XMFLOAT2 hpPosition = { 650.0f, 115.0f };
    const DirectX::XMFLOAT2 hpPivot = { 0.0f, 0.5f };
    const DirectX::XMFLOAT2 hpScale = { 0.35f, 0.35f };

    auto hpNameUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/HpBar/boss_hp_name.png", "BossHpName");
    hpNameUiComponent->SetWorldPosition(hpNamePosition);
    hpNameUiComponent->SetSize({ 102.0f, 35.0f });
    hpNameUiComponent->SetPivot({ 0.5f,0.5f });
    hpNameUiComponent->SetScale({ 0.95f,0.95f });
    hpNameUiComponent->SetColor(CoreColor::White);
    hpNameUiComponent->zOrder = 10;
    uiManager->Add(hpNameUiComponent);


    auto hpBackgroundUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/HpBar/boss_hp_background.png", "BossHpBackground");
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

    auto hpFrameUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/HpBar/boss_hp_frame.png", "BossHpFrame");
    hpFrameUiComponent->SetWorldPosition(hpPosition);
    hpFrameUiComponent->SetSize({ 1909.0f, 49.0f });
    hpFrameUiComponent->SetPivot(hpPivot);
    hpFrameUiComponent->SetScale(hpScale);
    hpFrameUiComponent->SetColor(CoreColor::White);
    hpFrameUiComponent->zOrder = 15;
    uiManager->Add(hpFrameUiComponent);

    hpBarUiComponents =
    {
        hpNameUiComponent,
        hpBackgroundUiComponent,
        hpDelayedFillUiComponent,
        hpCurrentFillUiComponent,
        hpFrameUiComponent,
    };
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

void GruxEnemy::SetDirectionImmediate(const DirectX::XMFLOAT3& direction)
{
    if (rotationComponent)
        rotationComponent->SetDirectionImmediate(direction);
}

void GruxEnemy::PauseBattleAI()
{
    battleAIActive = false;
    StopBattleActions();
    if (stateMachine_)
        stateMachine_->ChangeState("EnemyIdleState");
}

void GruxEnemy::ResumeBattleAI()
{
    battleAIActive = true;
}

void GruxEnemy::StopBattleActions()
{
    DisableAttackHitBoxes();
    StopDashAttackMovement();
    StopChargeAttackMovement();
    StopAIMovement();
    ClearActiveIntent();
    ClearPendingAttackFacing();
    ClearJumpAttackMotionWarpOverride();
    ResetJustDodgeRecords("battle_stop");

    pendingAttackActionValid = false;
    beginHuskParticleRequest = false;
    selectedActionType = BossActionType::AttackLA;
    selectedAttackType = BossAttackType::PrimaryAttackLA;
    selectedPositioningData.reset();
    hasSelectedActionDebug = false;
    hasLastAttack = false;
    lastStartedCombatAttack.reset();
    secondLastStartedCombatAttack.reset();
    lastSelectedIntent.reset();
    fixedPositioningTargetValid = false;
    combatRepositionSettling = false;
    combatRepositionSettleRemaining = 0.0f;
    positioningMoveStopTimer = 0.0f;
    positioningAnimationActualSpeed = 0.0f;
    positioningAnimationMoving = false;
    transitionWindow = false;
    hitActors.clear();
    currentAttackHitCount = 0;
    showLeftWeaponTrail = false;
    showRightWeaponTrail = false;
    leftWeaponTrail.trailPoints.clear();
    rightWeaponTrail.trailPoints.clear();
    nextRecoveryDuration.reset();
    nextRecoverySource = "Default";
    pendingChargeRecoveryResult = ChargeAttackEndReason::None;
    combatRepositionIntentPending = false;
    attackReadyActive = false;
    attackReadyDebugTimer = 0.0f;
    attackReadySEFired = false;
    stunElapsedDebug = 0.0f;
    recoveryElapsedDebug = 0.0f;
    chargeElapsedTime = 0.0f;
    dashAttackElapsedTime = 0.0f;
    animationMotionWarps.clear();

    characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->SetInputMagnitude(0.0f);
    characterMovementComponent->SetFrameAdditionalVelocity({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->AddForcedMove({ 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f);
    characterMovementComponent->ResetFixedSpeed();
    velocity = { 0.0f, 0.0f, 0.0f };
}

void GruxEnemy::ResetForBattleRestart(const Transform& battleStartTransform)
{
    ResetForBattleContinue(battleStartTransform);
    hp = maxHp;
    delayedHp = static_cast<float>(hp);
    if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
    if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
}

void GruxEnemy::ResetForBattleContinue(const Transform& battleStartTransform)
{
    ResetTimeScale();
    if (stateMachine_)
        stateMachine_->ChangeState("EnemyIdleState");
    StopBattleActions();

    isDeathPerform = false;
    rushHpDisplayActive = false;
    useRushDelayedHpFollowSpeed = false;
    delayedHp = static_cast<float>((std::max)(hp, 0));
    delayedHpDelayTimer = 0.0f;
    if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
    if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));

    SetPosition(battleStartTransform.GetLocation());
    SetQuaternionRotation(battleStartTransform.GetRotation());
    SetScale(battleStartTransform.GetScale());
    UpdateAllComponentTransforms();
}

void GruxEnemy::Update(float deltaTime)
{
    // HPバーの更新
    const float currentHp = static_cast<float>((std::max)(hp, 0));
    const float uiDeltaTime = Time::UnscaledDeltaTime();
    hitVoiceCooldownTimer = (std::max)(0.0f, hitVoiceCooldownTimer - uiDeltaTime);
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
        // HP recovery synchronizes the delayed display immediately.
        delayedHp = currentHp;
        delayedHpDelayTimer = 0.0f;
        useRushDelayedHpFollowSpeed = false;
    }

    if (hpCurrentFillUiComponent && hpDelayedFillUiComponent)
    {
        const float maximumHp = static_cast<float>(maxHp);
        hpCurrentFillUiComponent->SetValue(currentHp, maximumHp);
        hpDelayedFillUiComponent->SetValue(delayedHp, maximumHp);
    }

    if (!IsAnimationEditorPreviewActive())
        UpdateActionCooldowns(deltaTime);

    BeginRotationDebugFrame();
    Character::Update(deltaTime);

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

    // Animation Editor Preview中はこのBossだけGameplay/AI/攻撃判定を停止する。
    // Character::Update内のPreview Pose更新は上で継続している。
    if (IsAnimationEditorPreviewActive())
        return;


    // NotifyEnd is not guaranteed when an animation is interrupted or finishes
// before the notify range ends. Do not carry that animation's warp forward.
    if (!GetBodyAnimationController()->IsPlayAnimation())
    {
        animationMotionWarps.clear();
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
    FinishRotationDebugFrame();

#ifdef USE_IMGUI
    aiDebugTargetContext = BuildTargetContext();
    if (showBossAIDebug)
        DrawBossAIDebugWorld(aiDebugTargetContext);
    if (showRotationDebug)
        DrawRotationDebugWorld(aiDebugTargetContext);
    if (positioningWorldDebug)
        DrawPositioningDebugWorld();
#endif

    SetScale({ enemyScale,enemyScale,enemyScale });

    // ImageComponentのalpha更新
    {
        easingRunner->Tick(deltaTime);
        float bossNameImageAlpha = std::lerp(0.0f, 1.0f, easingFactorAlpha);
        gruxNameImageComponent->SetColor(DirectX::XMFLOAT4{ 1.0f,1.0f,1.0f,bossNameImageAlpha });
    }

    // 被弾時のフラッシュ
    {
        const float fadeSpeed = damageFlashDuration > 0.0f
            ? damageFlashStartValue / damageFlashDuration
            : damageFlashStartValue;
        skeletalMeshComponent->plusAlphaCBuffer->data.flashValue = (std::max)(
            0.0f,
            skeletalMeshComponent->plusAlphaCBuffer->data.flashValue - fadeSpeed * deltaTime);
    }

    DirectX::XMFLOAT3 bossPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 playerPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 toPlayerDir = { 0.0f,0.0f,0.0f };
#if 1
    // ボス戦時のカメラの注視点の位置を更新する
    if (auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>())
    {
        bossPos = GetPosition();
        playerPos = player->GetPosition();
        toPlayerDir = MathHelper::Subtract(playerPos, bossPos);
        DirectX::XMFLOAT3 worldUp = { 0.0f,1.0f,0.0f };
        toPlayerDir = MathHelper::Normalize(toPlayerDir);
        DirectX::XMFLOAT3 rightDir = MathHelper::Cross(worldUp, toPlayerDir);
        rightDir = MathHelper::Normalize(rightDir);
        DirectX::XMFLOAT3 targetPos = bossPos;
        // ターゲットの位置をプレイヤーとボスの中点にする
        DirectX::XMFLOAT3 middlePos =
        {
            (bossPos.x + playerPos.x) * 0.5f,
            (bossPos.y + playerPos.y) * 0.5f,
            (bossPos.z + playerPos.z) * 0.5f
        };

        if (auto mainCamera = GetOwnerScene()->GetActorManager()->GetActorOfType<MainCamera>())
        {
            float blendLookTarget = mainCamera->tpsController.blendLookTarget;
            targetPos = MathHelper::Lerp(middlePos, playerPos, blendLookTarget);
        }

        targetPos = MathHelper::Add(
            targetPos,
            MathHelper::Multiply(toPlayerDir, bossBattleCameraDistance));

        targetPos = MathHelper::Add(
            targetPos,
            MathHelper::Multiply(rightDir, bossBattleCameraRightDistance));

        targetPos = MathHelper::Add(
            targetPos,
            bossBattleCameraOffset);

        //cameraTargetComponent->SetWorldLocationDirect(targetPos);
        //DebugRender::DrawSphere(targetPos, 0.5f, { 1.0f,1.0f,0.0f,1.0f }, true);
    }

    DirectX::XMFLOAT3 lockOnPos = MathHelper::Add(bossPos, MathHelper::Multiply(toPlayerDir, lockOnOffset));
    lockOnPos.y = lockOnOffsetY;
    lockOnTargetMeshComponent->SetWorldLocationDirect({ lockOnPos });
    DirectX::XMFLOAT4 rotation = MathHelper::LookRotation(toPlayerDir, { 0,1,0 });
    lockOnTargetMeshComponent->SetRelativeRotationDirect(rotation);

    DirectX::XMFLOAT2 lockOnUiPos = WorldToUI(lockOnPos);
    lockOnTargetImageComponent->SetWorldPosition(lockOnUiPos);

    if (auto camera = GetOwnerScene()->GetActorManager()->GetActorOfType<DarkCameraActor>())
    {
        // カメラがロックオンモードの時のみ表示する
        if (camera->GetMovementMode() == DarkCameraActor::CameraMode::LockOn)
        {
            lockOnTargetImageComponent->SetVisible(true);
        }
        else
        {
            lockOnTargetImageComponent->SetVisible(false);
        }
    }

#endif // 0

#if 0 // ジャスト回避のタイミングをわかりやすくするため
    if (isDangerWindow)
    {
        skeletalMeshComponent->plusAlphaCBuffer->data.chargePower = 2.0f;
    }
    else
    {
        skeletalMeshComponent->plusAlphaCBuffer->data.chargePower = 0.0f;
    }

#endif // 0


    // 攻撃の危険な時に、
    if (isDangerWindow)
    {
        if (auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>())
        {
            const DirectX::XMFLOAT3 playerTestPosition = player->GetPosition();
            DirectX::XMFLOAT3 playerCapsuleCenter = playerTestPosition;
            float playerCapsuleRadius = 0.0f;
            float playerCapsuleHeight = 0.0f;
            if (const auto capsule = std::dynamic_pointer_cast<CapsuleComponent>(
                player->FindComponentByName("capsuleComponent")))
            {
                playerCapsuleCenter = capsule->GetComponentLocation();
                playerCapsuleRadius = capsule->GetRadius();
                playerCapsuleHeight = capsule->GetHeight();
            }
            const DangerArea::PointResult pointResult = dangerArea.ContainsPoint(playerTestPosition);
            const DangerArea::CapsuleResult capsuleResult = dangerArea.IntersectsPlayerCapsule(
                playerCapsuleCenter, playerCapsuleRadius, playerCapsuleHeight);
            const bool inside = capsuleResult.overlap;
            const bool justDodgeWindow = player->GetJustDodgeWindow();
            DirectX::XMFLOAT4 debugColor = inside ? DirectX::XMFLOAT4{ 1,0,0,1 } : DirectX::XMFLOAT4{ 1,1,1,1 };
            if (inside && justDodgeWindow)
                debugColor = { 0,1,0,1 };

            DebugRender::DrawSphere(dangerArea.origin, 0.1f, { 1,1,0,1 }, 0.0f, true);
            DebugRender::DrawSphere(playerTestPosition, 0.12f, { 1,1,0,1 }, 0.0f, true);
            DebugRender::DrawSphere(playerCapsuleCenter, 0.09f, { 0,1,1,1 }, 0.0f, true);

            // CPU-computed logical OBB markers. Do not use WorldTransform here.
            for (int rightSign : { -1, 1 })
            {
                for (int upSign : { -1, 1 })
                {
                    for (int forwardSign : { -1, 1 })
                    {
                        DirectX::XMFLOAT3 corner = dangerArea.center;
                        corner = MathHelper::Add(corner, MathHelper::Multiply(
                            dangerArea.right, dangerArea.halfExtent.x * static_cast<float>(rightSign)));
                        corner = MathHelper::Add(corner, MathHelper::Multiply(
                            dangerArea.up, dangerArea.halfExtent.y * static_cast<float>(upSign)));
                        corner = MathHelper::Add(corner, MathHelper::Multiply(
                            dangerArea.forward, dangerArea.halfExtent.z * static_cast<float>(forwardSign)));
                        DebugRender::DrawSphere(corner, 0.06f, { 1,1,0,1 }, 0.0f, true);
                    }
                }
            }

            DebugRender::DrawSphere(dangerArea.center, 0.08f, { 1,0,0,1 }, 0.0f, true);
            DebugRender::DrawSphere(MathHelper::Add(dangerArea.center,
                MathHelper::Multiply(dangerArea.right, dangerArea.halfExtent.x)),
                0.08f, { 0,1,0,1 }, 0.0f, true);
            DebugRender::DrawSphere(MathHelper::Add(dangerArea.center,
                MathHelper::Multiply(dangerArea.up, dangerArea.halfExtent.y)),
                0.08f, { 0,0,1,1 }, 0.0f, true);
            DebugRender::DrawSphere(MathHelper::Add(dangerArea.center,
                MathHelper::Multiply(dangerArea.forward, dangerArea.halfExtent.z)),
                0.08f, { 1,0,1,1 }, 0.0f, true);
            DebugRender::DrawBox(dangerArea.WorldTransform(), dangerArea.size, debugColor, 0.0f, true);

            if (inside && player->GetJustDodgeWindow())
            {
                if (HasJustDodgedAttack(player.get()))
                {
                    Logger::Log(Logger::LogCategory::Gameplay,
                        "[BossAttack][JustDodgeRejected] reason=duplicate attackSequenceId=" +
                        std::to_string(currentAttackSequenceId));
                }
                else
                {
                    TryStartJustDodgeSuccess(player.get());
                }
            }
        }
    }

    DrawDangerObbWorldDebug();



    // 当たり判定
    HitResultWithActor hit;

    // 左の武器の当たり判定
    if (leftHitBox)
    {
        bool isLeftHit = false;

        DirectX::XMFLOAT3 weaponLeftRootPos = weaponLeftRootComponent->GetComponentLocation();
        DirectX::XMFLOAT3 weaponLeftMidPos = weaponLeftMiddleComponent->GetComponentLocation();
        DirectX::XMFLOAT3 weaponLeftTipPos = weaponLeftTipComponent->GetComponentLocation();

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
                Logger::Log(Logger::LogCategory::Gameplay,
                    "[BossAttack][SphereRayCastHit] attackSequenceId=" + std::to_string(currentAttackSequenceId) +
                    " side=left");
                if (HasJustDodgedAttack(hit.actor))
                {
                    Logger::Log(Logger::LogCategory::Gameplay,
                        "[BossAttack][DamageRejected] reason=justDodged attackSequenceId=" +
                        std::to_string(currentAttackSequenceId));
                }
                else if (!hitActors.contains(hit.actor))
                {
                    Logger::Log(U8("剣にプレイヤーが当たった"));
                    if (player->TryTakeDamage(GetDamageForCurrentAttack(), GetPosition()))
                    {
                        hitActors.emplace(player);
                        ++currentAttackHitCount;
                        Logger::Log(Logger::LogCategory::Gameplay,
                            "[BossAttack][DamageApplied] attackSequenceId=" +
                            std::to_string(currentAttackSequenceId) + " source=sphereRayLeft");
                    }
                }
            }
        }
    }
    // 右の武器の当たり判定
    if (rightHitBox)
    {
        bool isRightHit = false;

        DirectX::XMFLOAT3 weaponRightRootPos = weaponRightRootComponent->GetComponentLocation();
        DirectX::XMFLOAT3 weaponRightMidPos = weaponRightMiddleComponent->GetComponentLocation();
        DirectX::XMFLOAT3 weaponRightTipPos = weaponRightTipComponent->GetComponentLocation();

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
                /*                if (isDangerWindow && player->GetJustDodgeWindow())
                                {
                                    if (auto enemy = std::dynamic_pointer_cast<Enemy>(shared_from_this()))
                                    {
                                        player->StartJustDodgeSuccess(enemy);
                                        Logger::Log(U8("ジャスト回避成功！"));
                                    }
                                }
                                else*/
                Logger::Log(Logger::LogCategory::Gameplay,
                    "[BossAttack][SphereRayCastHit] attackSequenceId=" + std::to_string(currentAttackSequenceId) +
                    " side=right");
                if (HasJustDodgedAttack(hit.actor))
                {
                    Logger::Log(Logger::LogCategory::Gameplay,
                        "[BossAttack][DamageRejected] reason=justDodged attackSequenceId=" +
                        std::to_string(currentAttackSequenceId));
                }
                else if (!hitActors.contains(hit.actor))
                {
                    Logger::Log(U8("剣にプレイヤーが当たった"));
                    if (player->TryTakeDamage(GetDamageForCurrentAttack(), GetPosition()))
                    {
                        hitActors.emplace(player);
                        ++currentAttackHitCount;
                        Logger::Log(Logger::LogCategory::Gameplay,
                            "[BossAttack][DamageApplied] attackSequenceId=" +
                            std::to_string(currentAttackSequenceId) + " source=sphereRayRight");
                    }
                }
            }
        }
    }

    if (rightWeaponCollisionComp)
        rightWeaponCollisionComp->SetIsVisibleDebugShape(rightHitBox);
    if (leftWeaponCollisionComp)
        leftWeaponCollisionComp->SetIsVisibleDebugShape(leftHitBox);
    if (const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
        player && player->IsJustDodgeDebugEnabled())
    {
        const auto drawWeaponHitSpheres = [](const std::shared_ptr<SceneComponent>& root,
            const std::shared_ptr<SceneComponent>& middle,
            const std::shared_ptr<SceneComponent>& tip, float radius,
            const DirectX::XMFLOAT4& color)
            {
                if (root) DebugRender::DrawSphere(root->GetComponentLocation(), radius, color, 0.0f, true);
                if (middle) DebugRender::DrawSphere(middle->GetComponentLocation(), radius, color, 0.0f, true);
                if (tip) DebugRender::DrawSphere(tip->GetComponentLocation(), radius, color, 0.0f, true);
            };
        if (leftHitBox)
            drawWeaponHitSpheres(weaponLeftRootComponent, weaponLeftMiddleComponent,
                weaponLeftTipComponent, activeLeftHitBoxRadius, { 0.2f, 1.0f, 0.35f, 1.0f });
        if (rightHitBox)
            drawWeaponHitSpheres(weaponRightRootComponent, weaponRightMiddleComponent,
                weaponRightTipComponent, activeRightHitBoxRadius, { 1.0f, 0.25f, 0.8f, 1.0f });
    }


    //DebugRender::DrawBox(bossPos, { 3,3,3 }, { 1,1,1,1 });

#if 0
    if (InputSystem::GetInputState("0"))
    {
        stateMachine_->ChangeState("EnemyAttackState");
    }
#endif // 0

#if 1
    if (hp <= 0 && !isDeathPerform)
    {
        isDeathPerform = true;
        stateMachine_->ChangeState("EnemyDeathState");
    }
#endif // 0
}

void GruxEnemy::RenderTrail(ID3D11DeviceContext* immediateContext)
{
    leftWeaponTrail.SetRushColorEnabled(true, bossTrailColor);
    rightWeaponTrail.SetRushColorEnabled(true, bossTrailColor);
    leftWeaponTrail.SetEmissiveStrength(bossTrailEmissiveStrength);
    rightWeaponTrail.SetEmissiveStrength(bossTrailEmissiveStrength);
    leftWeaponTrail.Render(immediateContext);
    rightWeaponTrail.Render(immediateContext);
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

        Camera* activeCamera = GetOwnerScene()->GetActiveCamera();
        if (auto* darkCamera = dynamic_cast<DarkCameraActor*>(activeCamera))
            darkCamera->PlayCameraShakePreset(event.parameter);
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

    const auto drawWeapon = [this](
        const std::shared_ptr<SceneComponent>& root,
        const std::shared_ptr<SceneComponent>& middle,
        const std::shared_ptr<SceneComponent>& tip,
        const DirectX::XMFLOAT4& color,
        const float radius)
        {
            if (!root || !middle || !tip)
                return;

            const DirectX::XMFLOAT3 rootPos = root->GetComponentLocation();
            const DirectX::XMFLOAT3 middlePos = middle->GetComponentLocation();
            const DirectX::XMFLOAT3 tipPos = tip->GetComponentLocation();
            DebugRender::DrawSphere(rootPos, radius, color, 0.0f, true);
            DebugRender::DrawSphere(middlePos, radius, color, 0.0f, true);
            DebugRender::DrawSphere(tipPos, radius, color, 0.0f, true);
            DebugRender::DrawLine(rootPos, middlePos, color, 0.0f, true);
            DebugRender::DrawLine(middlePos, tipPos, color, 0.0f, true);
        };

    if (state.parameter == leftWeapon || state.parameter == bothWeapon)
        drawWeapon(weaponLeftRootComponent, weaponLeftMiddleComponent,
            weaponLeftTipComponent, leftHitBoxPreviewColor, state.hitBoxRadius);
    if (state.parameter == rightWeapon || state.parameter == bothWeapon)
        drawWeapon(weaponRightRootComponent, weaponRightMiddleComponent,
            weaponRightTipComponent, rightHitBoxPreviewColor, state.hitBoxRadius);
}
void GruxEnemy::DrawBossAIDebugWorld(const BossTargetContext& context) const
{
#ifdef USE_IMGUI
    constexpr int segmentCount = 64;
    constexpr float twoPi = DirectX::XM_2PI;
    const DirectX::XMFLOAT3 bossPosition = GetPosition();
    const float debugHeight = bossPosition.y + 1.0f;

    const auto drawRing = [&](float radius, const DirectX::XMFLOAT4& color)
        {
            for (int i = 0; i < segmentCount; ++i)
            {
                const float angle0 = twoPi * static_cast<float>(i) / static_cast<float>(segmentCount);
                const float angle1 = twoPi * static_cast<float>(i + 1) / static_cast<float>(segmentCount);
                const DirectX::XMFLOAT3 start =
                {
                    bossPosition.x + std::cos(angle0) * radius,
                    debugHeight,
                    bossPosition.z + std::sin(angle0) * radius
                };
                const DirectX::XMFLOAT3 end =
                {
                    bossPosition.x + std::cos(angle1) * radius,
                    debugHeight,
                    bossPosition.z + std::sin(angle1) * radius
                };
                DebugRender::DrawLine(start, end, color, 0.0f, true);
            }
        };

    drawRing(nearDistanceThreshold, { 0.2f, 1.0f, 0.2f, 1.0f });
    drawRing(middleDistanceThreshold, { 1.0f, 0.7f, 0.1f, 1.0f });

    if (const BossIntentData* intentData = GetActiveIntentData())
    {
        drawRing(intentData->preferredMinDistance, { 0.15f, 0.85f, 1.0f, 1.0f });
        drawRing(intentData->preferredMaxDistance, { 1.0f, 0.25f, 0.85f, 1.0f });
    }

    if (context.valid)
    {
        const DirectX::XMFLOAT3 playerXZPosition =
        {
            bossPosition.x + context.directionToPlayer.x * context.xzDistance,
            debugHeight,
            bossPosition.z + context.directionToPlayer.z * context.xzDistance
        };
        DirectX::XMFLOAT4 playerLineColor{ 0.2f, 0.8f, 1.0f, 1.0f };
        if (context.distanceRegion == BossDistanceRegion::Near)
            playerLineColor = { 0.2f, 1.0f, 0.2f, 1.0f };
        else if (context.distanceRegion == BossDistanceRegion::Far)
            playerLineColor = { 0.8f, 0.3f, 1.0f, 1.0f };
        DebugRender::DrawLine(
            { bossPosition.x, debugHeight, bossPosition.z },
            playerXZPosition, playerLineColor, 0.0f, true);
    }

    if (dashAttackMovementActive)
    {
        const DirectX::XMFLOAT3 dashStart =
        {
            dashAttackStartPosition.x,
            dashAttackStartPosition.y + 0.15f,
            dashAttackStartPosition.z
        };
        const DirectX::XMFLOAT3 dashTarget =
        {
            dashTargetPosition.x,
            dashTargetPosition.y + 0.15f,
            dashTargetPosition.z
        };
        DebugRender::DrawLine(dashStart, dashTarget, { 1.0f, 0.15f, 0.15f, 1.0f }, 0.0f, true);
        DebugRender::DrawSphere(dashTarget, 0.25f, { 1.0f, 0.15f, 0.15f, 1.0f }, 0.0f, true);
    }
#else
    (void)context;
#endif
}

void GruxEnemy::DrawRotationDebugWorld(const BossTargetContext& context) const
{
#ifdef USE_IMGUI
    const DirectX::XMFLOAT3 position = GetPosition();
    const float y = position.y + 0.35f;
    const DirectX::XMFLOAT3 origin{ position.x, y, position.z };
    DirectX::XMVECTOR forwardVector = DirectX::XMVector3Rotate(
        DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
        DirectX::XMLoadFloat4(&GetQuaternionRotation()));
    DirectX::XMFLOAT3 forward{};
    DirectX::XMStoreFloat3(&forward, forwardVector);
    forward.y = 0.0f;
    DebugRender::DrawLine(origin,
        { origin.x + forward.x * 4.0f, y, origin.z + forward.z * 4.0f },
        { 0.2f, 1.0f, 0.25f, 1.0f }, 0.0f, true);
    if (context.valid)
    {
        DebugRender::DrawLine(origin,
            { origin.x + context.directionToPlayer.x * 5.0f, y,
                origin.z + context.directionToPlayer.z * 5.0f },
            { 0.2f, 0.65f, 1.0f, 1.0f }, 0.0f, true);
    }
    if (positioningDebugActive)
    {
        const DirectX::XMFLOAT3& move = currentPositioningDebug.requestedMoveDirection;
        DebugRender::DrawLine(origin,
            { origin.x + move.x * 4.0f, y, origin.z + move.z * 4.0f },
            { 1.0f, 0.75f, 0.1f, 1.0f }, 0.0f, true);
    }
    if (rotationDebugTurnTargetValid)
    {
        DebugRender::DrawLine(origin,
            { origin.x + rotationDebugTurnTargetDirection.x * 4.5f, y,
                origin.z + rotationDebugTurnTargetDirection.z * 4.5f },
            { 1.0f, 0.2f, 0.8f, 1.0f }, 0.0f, true);
    }
#else
    (void)context;
#endif
}


void GruxEnemy::DrawPositioningDebugWorld() const
{
#ifdef USE_IMGUI
    const PositioningDebugSnapshot& snapshot = positioningDebugActive
        ? currentPositioningDebug
        : lastPositioningDebug;
    const float debugY = snapshot.valid ? snapshot.startBossPosition.y + 0.12f : GetPosition().y + 0.12f;
    const auto drawBounds = [debugY](float minX, float maxX, float minZ, float maxZ,
        const DirectX::XMFLOAT4& color)
        {
            const DirectX::XMFLOAT3 a{ minX, debugY, minZ };
            const DirectX::XMFLOAT3 b{ maxX, debugY, minZ };
            const DirectX::XMFLOAT3 c{ maxX, debugY, maxZ };
            const DirectX::XMFLOAT3 d{ minX, debugY, maxZ };
            DebugRender::DrawLine(a, b, color, 0.0f, true);
            DebugRender::DrawLine(b, c, color, 0.0f, true);
            DebugRender::DrawLine(c, d, color, 0.0f, true);
            DebugRender::DrawLine(d, a, color, 0.0f, true);
        };

    drawBounds(bossRoomMinX, bossRoomMaxX, bossRoomMinZ, bossRoomMaxZ,
        { 0.45f, 0.45f, 0.5f, 1.0f });
    constexpr float maximumMargin =
        (std::min)((bossRoomMaxX - bossRoomMinX) * 0.5f,
            (bossRoomMaxZ - bossRoomMinZ) * 0.5f) - 0.01f;
    const float margin = std::clamp(bossRoomSafetyMargin, 0.0f, maximumMargin);
    drawBounds(bossRoomMinX + margin, bossRoomMaxX - margin,
        bossRoomMinZ + margin, bossRoomMaxZ - margin,
        { 0.2f, 1.0f, 0.45f, 1.0f });

    if (!snapshot.valid)
        return;
    DirectX::XMFLOAT3 start = snapshot.startBossPosition;
    DirectX::XMFLOAT3 desired = snapshot.desiredTargetPosition;
    DirectX::XMFLOAT3 target = snapshot.clampedTargetPosition;
    DirectX::XMFLOAT3 current = positioningDebugActive ? GetPosition() : snapshot.currentPosition;
    start.y = desired.y = target.y = current.y = debugY + 0.08f;
    DebugRender::DrawSphere(start, 0.22f, { 0.2f, 0.8f, 1.0f, 1.0f }, 0.0f, true);
    DebugRender::DrawSphere(desired, 0.22f, { 1.0f, 0.2f, 0.9f, 1.0f }, 0.0f, true);
    DebugRender::DrawSphere(target, 0.28f, { 1.0f, 0.25f, 0.2f, 1.0f }, 0.0f, true);
    DebugRender::DrawSphere(current, 0.18f, { 0.25f, 1.0f, 0.35f, 1.0f }, 0.0f, true);
    DebugRender::DrawLine(start, desired, { 1.0f, 0.2f, 0.9f, 1.0f }, 0.0f, true);
    DebugRender::DrawLine(desired, target, { 1.0f, 0.45f, 0.15f, 1.0f }, 0.0f, true);
    DebugRender::DrawLine(start, target, { 0.2f, 0.8f, 1.0f, 1.0f }, 0.0f, true);
    DebugRender::DrawLine(current, target, { 1.0f, 0.75f, 0.15f, 1.0f }, 0.0f, true);
#else
    (void)this;
#endif
}
//　ボスAIのImGui描画



void GruxEnemy::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    Character::DrawImGuiDetails();

    ImGui::SeparatorText(U8("HPのUI"));
    ImGui::DragFloat("delayedHpDelayDuration", &delayedHpDelayDuration, 0.05f, 0.0f, 10.0f, "%.2f sec");
    ImGui::DragFloat("delayedHpFollowSpeed", &delayedHpFollowSpeed, 0.05f, 0.0f, 25.0f, "%.2f sec");
    ImGui::DragFloat("delayedHpRushFollowSpeed", &delayedHpRushFollowSpeed, 0.05f, 0.0f, 25.0f, "%.2f sec");
    ImGui::ColorEdit4("bossHpCurrentColor", &bossHpCurrentColor.r);
    ImGui::ColorEdit4("bossHpDelayedColor", &bossHpDelayedColor.r);

    ImGui::SeparatorText("Boss AI");
    int aiModeIndex = static_cast<int>(bossAIMode);
    const char* aiModes[] = { "CombatAI", "DebugFixedAttack" };
    if (ImGui::Combo("AI Mode", &aiModeIndex, aiModes, static_cast<int>(std::size(aiModes))))
    {
        bossAIMode = static_cast<BossAIMode>(aiModeIndex);
        if (bossAIMode == BossAIMode::DebugFixedAttack)
            ClearActiveIntent();
    }

    int attackIndex = static_cast<int>(debugFixedAttackType);
    const char* attackTypes[] =
    {
        "PrimaryAttack_LA",
        "PrimaryAttack_RA",
        "FastCombo (A > B > C)",
        "PrimaryAttack_JumpAttack",
        "Stampede_0",
        "Stampede_0 > Stampede_Knockup_0",
        "ChargeAttack",
        "LongRangeAttack (Not Implemented)",
    };
    if (ImGui::Combo("Debug Fixed Attack", &attackIndex, attackTypes, static_cast<int>(std::size(attackTypes))))
        debugFixedAttackType = static_cast<BossAttackType>(attackIndex);
    ImGui::DragFloat("Attack Interval", &attackInterval, 0.05f, 0.0f, 10.0f, "%.2f sec");
    ImGui::DragFloat("Fallback Recovery Duration", &recoveryDuration, 0.05f, 0.0f, 10.0f, "%.2f sec");
    if (ImGui::CollapsingHeader("Danger OBB Tuning"))
    {
        auto controller = GetBodyAnimationController();
        AnimationNotifyState* selectedState = GetSelectedDangerNotifyState();
        std::string selectedName = "None";
        if (controller)
        {
            if (const auto* asset = controller->GetNotifyAssetForRuntimeTuning(dangerObbSelectedClip))
                selectedName = asset->animationName;

            if (ImGui::BeginCombo("Attack Animation", selectedName.c_str()))
            {
                for (const size_t clip : controller->GetAnimationAssetOrderForRuntimeTuning())
                {
                    auto* asset = controller->GetNotifyAssetForRuntimeTuning(clip);
                    if (!asset)
                        continue;
                    auto& states = asset->notifyTrack.states;
                    const size_t dangerCount = std::count_if(states.begin(), states.end(),
                        [](const AnimationNotifyState& state)
                        { return state.type == AnimationNotifyState::Type::DangerWindow; });
                    for (size_t stateIndex = 0; stateIndex < states.size(); ++stateIndex)
                    {
                        if (states[stateIndex].type != AnimationNotifyState::Type::DangerWindow)
                            continue;
                        const bool selected = clip == dangerObbSelectedClip &&
                            stateIndex == dangerObbSelectedStateIndex;
                        std::string label = asset->animationName;
                        if (dangerCount > 1)
                            label += " [Danger " + std::to_string(stateIndex) + "]";
                        if (ImGui::Selectable(label.c_str(), selected))
                        {
                            dangerObbSelectedClip = clip;
                            dangerObbSelectedStateIndex = stateIndex;
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        selectedState = GetSelectedDangerNotifyState();
        if (selectedState)
        {
            bool changed = false;
            ImGui::TextDisabled("Center Offset (Local: X=Right, Y=Up, Z=Forward)");
            changed |= ImGui::DragFloat3("Center Offset##DangerOBB", &selectedState->justDodgeAreaOffset.x,
                0.05f, -20.0f, 20.0f, "%.2f");
            ImGui::TextDisabled("Full Size (not Half Extent)");
            changed |= ImGui::DragFloat3("Full Size##DangerOBB", &selectedState->justDodgeAreaSize.x,
                0.05f, 0.0f, 40.0f, "%.2f");
            selectedState->justDodgeAreaSize.x = (std::max)(0.0f, selectedState->justDodgeAreaSize.x);
            selectedState->justDodgeAreaSize.y = (std::max)(0.0f, selectedState->justDodgeAreaSize.y);
            selectedState->justDodgeAreaSize.z = (std::max)(0.0f, selectedState->justDodgeAreaSize.z);
            if (changed && selectedState == activeDangerNotifyState)
                RefreshActiveDangerAreaFromNotify();

            ImGui::Text("Danger Window: %.3f - %.3f sec", selectedState->startTime, selectedState->endTime);
            const bool selectedDangerActive = isDangerWindow && selectedState == activeDangerNotifyState;
            const DangerArea selectedArea = BuildDangerArea(GetPosition(), GetRight(), GetUp(), GetForward(),
                selectedState->justDodgeAreaOffset, selectedState->justDodgeAreaSize);
            const bool playerInside = GetPlayerDangerOverlapForDebug(selectedArea);
            const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
            const bool justActive = player && player->GetJustDodgeWindow();
            const float activeTime = selectedDangerActive && controller
                ? (std::max)(0.0f, controller->GetCurrentAnimationTime() - selectedState->startTime)
                : 0.0f;
            ImGui::Text("Danger Active: %s", selectedDangerActive ? "YES" : "NO");
            ImGui::Text("Window Active Time: %.3f sec", activeTime);
            ImGui::TextColored(playerInside ? ImVec4(1.0f, 0.25f, 0.2f, 1.0f) : ImVec4(0.7f, 0.85f, 1.0f, 1.0f),
                "Player Inside: %s", playerInside ? "YES / PLAYER INSIDE" : "NO / OUTSIDE");
            ImGui::Text("Just Window: %s", justActive ? "ACTIVE" : "INACTIVE");
            ImGui::Text("Just Possible Now: %s",
                selectedDangerActive && playerInside && justActive ? "YES" : "NO");
            ImGui::Checkbox("Danger OBB World Debug", &dangerObbWorldDebug);

            const uint64_t key = (static_cast<uint64_t>(dangerObbSelectedClip) << 32) |
                static_cast<uint64_t>(dangerObbSelectedStateIndex);
            const auto savedIt = savedDangerObbSettings.find(key);
            const bool modified = savedIt == savedDangerObbSettings.end() ||
                savedIt->second.centerOffset.x != selectedState->justDodgeAreaOffset.x ||
                savedIt->second.centerOffset.y != selectedState->justDodgeAreaOffset.y ||
                savedIt->second.centerOffset.z != selectedState->justDodgeAreaOffset.z ||
                savedIt->second.fullSize.x != selectedState->justDodgeAreaSize.x ||
                savedIt->second.fullSize.y != selectedState->justDodgeAreaSize.y ||
                savedIt->second.fullSize.z != selectedState->justDodgeAreaSize.z;
            ImGui::TextColored(modified
                ? ImVec4(1.0f, 0.75f, 0.2f, 1.0f)
                : ImVec4(0.35f, 1.0f, 0.45f, 1.0f),
                "Status: %s", modified ? "Modified / Unsaved" : "Saved");

            if (ImGui::Button("Save Selected Danger OBB") && controller)
            {
                const auto result = controller->SaveDangerObbForRuntimeTuning(
                    dangerObbSelectedClip, dangerObbSelectedStateIndex,
                    *selectedState, dangerObbSavePath);
                dangerObbLastSaveSucceeded = result ==
                    AnimationController::RuntimeDangerObbSaveResult::Saved;
                switch (result)
                {
                case AnimationController::RuntimeDangerObbSaveResult::Saved:
                    dangerObbSaveStatus = "Saved";
                    savedDangerObbSettings[key] = {
                        selectedState->justDodgeAreaOffset, selectedState->justDodgeAreaSize };
                    break;
                case AnimationController::RuntimeDangerObbSaveResult::FileNotFound:
                    dangerObbSaveStatus = "Save Failed: File not found";
                    break;
                case AnimationController::RuntimeDangerObbSaveResult::DangerWindowNotFound:
                    dangerObbSaveStatus = "Save Failed: DangerWindow not found or does not match";
                    break;
                case AnimationController::RuntimeDangerObbSaveResult::JsonParseFailed:
                    dangerObbSaveStatus = "Save Failed: JSON parse failed";
                    break;
                case AnimationController::RuntimeDangerObbSaveResult::JsonWriteFailed:
                    dangerObbSaveStatus = "Save Failed: JSON write failed";
                    break;
                }
            }
            ImGui::TextColored(dangerObbLastSaveSucceeded
                ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f)
                : ImVec4(1.0f, 0.45f, 0.3f, 1.0f),
                "Save Status: %s", dangerObbSaveStatus.c_str());
            if (!dangerObbSavePath.empty())
                ImGui::TextWrapped("File: %s", dangerObbSavePath.c_str());
            if (ImGui::Button("Reset Selected Danger OBB"))
            {
                if (const auto initialIt = initialDangerObbSettings.find(key);
                    initialIt != initialDangerObbSettings.end())
                {
                    selectedState->justDodgeAreaOffset = initialIt->second.centerOffset;
                    selectedState->justDodgeAreaSize = initialIt->second.fullSize;
                    if (selectedState == activeDangerNotifyState)
                        RefreshActiveDangerAreaFromNotify();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset All Danger OBB") && controller)
            {
                for (const size_t clip : controller->GetAnimationAssetOrderForRuntimeTuning())
                {
                    auto* asset = controller->GetNotifyAssetForRuntimeTuning(clip);
                    if (!asset)
                        continue;
                    auto& states = asset->notifyTrack.states;
                    for (size_t stateIndex = 0; stateIndex < states.size(); ++stateIndex)
                    {
                        if (states[stateIndex].type != AnimationNotifyState::Type::DangerWindow)
                            continue;
                        const uint64_t stateKey = (static_cast<uint64_t>(clip) << 32) |
                            static_cast<uint64_t>(stateIndex);
                        if (const auto initialIt = initialDangerObbSettings.find(stateKey);
                            initialIt != initialDangerObbSettings.end())
                        {
                            states[stateIndex].justDodgeAreaOffset = initialIt->second.centerOffset;
                            states[stateIndex].justDodgeAreaSize = initialIt->second.fullSize;
                        }
                    }
                }
                RefreshActiveDangerAreaFromNotify();
            }
            ImGui::TextDisabled("Reset changes Runtime only. JSON changes only when Save Selected is pressed: %s.json",
                selectedName.c_str());
        }
        else
        {
            ImGui::TextDisabled("No DangerWindow notify data loaded.");
        }
    }
    const BossTargetContext& targetContext = aiDebugTargetContext;
    const char* relativeRegions[] = { "Front", "Side", "Back" };
    const char* distanceRegions[] = { "Near", "Middle", "Far" };
    const char* actionTypes[] =
    {
        "AttackLA",
        "AttackRA",
        "FastCombo",
        "JumpAttack",
        "DashAttack",
        "ChargeAttack",
        "Approach",
        "Retreat",
        "RepositionLeft",
        "RepositionRight",
    };

    ImGui::SeparatorText("CombatAI v2 Positioning");
    ImGui::Text("Current Distance: %.3f", targetContext.xzDistance);
    ImGui::Text("Absolute Angle: %.3f", targetContext.absoluteAngleDegrees);
    ImGui::Text("Signed Angle: %.3f", targetContext.signedAngleDegrees);
    ImGui::Text("Forward Dot: %.3f", targetContext.forwardDot);

    ImGui::Text("Relative Region: %s", targetContext.valid ? relativeRegions[static_cast<int>(targetContext.region)] : "Invalid");
    ImGui::Text("Front Max Angle: %.1f deg", relativeFrontMaxAngle);
    ImGui::Text("Back Min Angle: %.1f deg", relativeBackMinAngle);
    ImGui::Text("Distance Region: %s", targetContext.valid ? distanceRegions[static_cast<int>(targetContext.distanceRegion)] : "Invalid");
    ImGui::Text("Selected Action: %s", actionTypes[static_cast<int>(selectedActionType)]);
    ImGui::Text("Last Started Attack: %s",
        lastStartedCombatAttack
        ? actionTypes[static_cast<int>(*lastStartedCombatAttack)] : "None");
    ImGui::Text("Second Last Started Attack: %s",
        secondLastStartedCombatAttack
        ? actionTypes[static_cast<int>(*secondLastStartedCombatAttack)] : "None");
    ImGui::DragFloat("Last Attack Penalty", &recentAttackPenaltyLast,
        0.01f, 0.01f, 1.0f, "%.2f");
    ImGui::DragFloat("Second Last Attack Penalty", &recentAttackPenaltySecond,
        0.01f, 0.01f, 1.0f, "%.2f");
    recentAttackPenaltyLast = std::clamp(recentAttackPenaltyLast, 0.01f, 1.0f);
    recentAttackPenaltySecond = std::clamp(recentAttackPenaltySecond, 0.01f, 1.0f);
    const char* intentTypes[] = { "CloseCombat", "DashAttackPlan", "JumpAttackPlan", "CombatReposition" };
    const char* intentStepNames[] =
    {
        "Selecting",
        "Positioning",
        "AttackPending",
        "Completed",
        "Failed",
    };
    UpdateIntentEffectiveWeights(targetContext);
    const char* candidateReasonNames[] =
    {
        "NoActiveIntent",
        "Candidate",
        "NotForCurrentIntent",
        "WrongDistance",
        "Cooldown",
        "ZeroWeight",
        "WrongRelativeRegion",
        "NoSafeDirection",
    };
    const char* activeIntentName = "None";
    if (activeIntent)
        activeIntentName = intentTypes[static_cast<int>(*activeIntent)];
    const char* activeIntentGoal = "None";
    if (activeIntent)
    {
        switch (*activeIntent)
        {
        case BossIntentType::CloseCombat:
            activeIntentGoal = "CloseCombat Attack";
            break;
        case BossIntentType::DashAttackPlan:
            activeIntentGoal = "DashAttack / ChargeAttack";
            break;
        case BossIntentType::JumpAttackPlan:
            activeIntentGoal = "JumpAttack";
            break;
        case BossIntentType::CombatReposition:
            activeIntentGoal = "Reposition";
            break;
        }
    }
    const BossIntentData* activeIntentData = GetActiveIntentData();
    const char* activeRangeStatusName = "None";
    const char* activeIntentNextAction = "None";
    if (activeIntentData && targetContext.valid)
    {
        const BossIntentRangeStatus rangeStatus =
            GetIntentRangeStatus(*activeIntentData, targetContext.xzDistance);
        const char* rangeStatusNames[] = { "Too Close", "In Range", "Too Far" };
        activeRangeStatusName = rangeStatusNames[static_cast<int>(rangeStatus)];
        if (*activeIntent == BossIntentType::CloseCombat)
        {
            activeIntentNextAction = rangeStatus == BossIntentRangeStatus::TooFar
                ? "Approach"
                : "CloseCombat Attack";
        }
        else if (*activeIntent == BossIntentType::DashAttackPlan)
        {
            activeIntentNextAction = rangeStatus == BossIntentRangeStatus::TooClose
                ? "Retreat"
                : (rangeStatus == BossIntentRangeStatus::TooFar ? "Approach" : "DashAttack / ChargeAttack");
        }
        else if (*activeIntent == BossIntentType::JumpAttackPlan)
        {
            activeIntentNextAction = rangeStatus == BossIntentRangeStatus::TooClose
                ? "Retreat"
                : (rangeStatus == BossIntentRangeStatus::TooFar ? "Approach" : "JumpAttack");
        }
        else if (*activeIntent == BossIntentType::CombatReposition)
        {
            activeIntentNextAction = "Reposition Left / Right";
        }
    }



    float attackValidMin = 0.0f;
    float attackValidMax = 0.0f;
    const bool hasAttackValidRange =
        GetIntentAttackValidRange(attackValidMin, attackValidMax);
    const float effectiveAttackValidMin =
        activeIntentStep == BossIntentStep::AttackPending ? 0.0f : attackValidMin;
    const bool attackValid = hasAttackValidRange && targetContext.valid &&
        targetContext.xzDistance >= effectiveAttackValidMin &&
        targetContext.xzDistance <= attackValidMax;
    if (activeIntentStep == BossIntentStep::AttackPending && hasAttackValidRange)
    {
        if (targetContext.xzDistance > attackValidMax)
            activeIntentNextAction = "Reposition: Too Far For Attack";
        else
            activeIntentNextAction = activeIntentGoal;
    }


    ImGui::SeparatorText("Selected Action");
    if (hasSelectedActionDebug)
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "-> %s", actionTypes[static_cast<int>(selectedActionType)]);
    else
        ImGui::TextDisabled("-> None");

    ImGui::SeparatorText("Last Decision");
    ImGui::TextWrapped("%s", lastAIDecisionReason.c_str());

    ImGui::SeparatorText("Last Weighted Random");
    if (hasLastActionRandomRoll)
    {
        ImGui::Text("Roll: %.3f / Total: %.3f", lastActionRandomRoll, lastActionRandomTotalWeight);
        for (size_t i = 0; i < combatActionData.size(); ++i)
        {
            if (lastActionRandomWeights[i] <= 0.0f)
                continue;
            ImGui::Text("%s  %.3f - %.3f  (Weight %.3f)",
                actionTypes[i], lastActionRandomRangeBegin[i],
                lastActionRandomRangeEnd[i], lastActionRandomWeights[i]);
        }
    }
    else
    {
        ImGui::TextDisabled("No weighted Action roll recorded");
    }

    if (dashAttackMovementActive)
    {
        const DirectX::XMFLOAT3 currentPosition = GetPosition();
        const float traveledX = currentPosition.x - dashAttackStartPosition.x;
        const float traveledZ = currentPosition.z - dashAttackStartPosition.z;
        const float targetX = dashTargetPosition.x - currentPosition.x;
        const float targetZ = dashTargetPosition.z - currentPosition.z;
        ImGui::SeparatorText("DashAttack Runtime");
        ImGui::Text("Start: (%.2f, %.2f, %.2f)", dashAttackStartPosition.x, dashAttackStartPosition.y, dashAttackStartPosition.z);
        ImGui::Text("Target: (%.2f, %.2f, %.2f)", dashTargetPosition.x, dashTargetPosition.y, dashTargetPosition.z);
        ImGui::Text("Direction: (%.3f, %.3f, %.3f)", dashAttackDirection.x, dashAttackDirection.y, dashAttackDirection.z);
        ImGui::Text("Planned Distance: %.2f", calculatedDashAttackDistance);
        ImGui::Text("Traveled Distance: %.2f", std::sqrt(traveledX * traveledX + traveledZ * traveledZ));
        ImGui::Text("Remaining Distance: %.2f", std::sqrt(targetX * targetX + targetZ * targetZ));
        ImGui::Text("Speed: %.2f", dashAttackSpeed);
        ImGui::Text("Elapsed / Timeout: %.2f / %.2f", dashAttackElapsedTime, dashAttackTimeout);
    }



    ImGui::Text("Active Intent: %s", activeIntentName);
    ImGui::Text("Intent Goal: %s", activeIntentGoal);
    ImGui::Text("Intent Step: %s", intentStepNames[static_cast<int>(activeIntentStep)]);
    if (activeIntentData)
    {
        ImGui::Text("Preferred Range: %.2f - %.2f m",
            activeIntentData->preferredMinDistance,
            activeIntentData->preferredMaxDistance);
    }
    ImGui::Text("Range Status: %s", activeRangeStatusName);
    ImGui::Text("Next Action: %s", activeIntentNextAction);
    ImGui::Text("Positioning Attempted: %s", intentPositioningAttempted ? "Yes" : "No");
    ImGui::Text("Positioning Completed: %s", intentPositioningCompleted ? "YES" : "NO");
    ImGui::Text("Positioning Attempts: %d / %d",
        intentPositioningAttemptCount, maxIntentPositioningAttempts);
    if (hasAttackValidRange)
    {
        ImGui::Text("%s: %.2f - %.2f m",
            activeIntentStep == BossIntentStep::AttackPending
            ? "AttackPending Effective Valid Range"
            : "Attack Valid Range",
            effectiveAttackValidMin, attackValidMax);
        ImGui::Text("Current Player Distance: %.2f m", targetContext.xzDistance);
        ImGui::Text("Attack Valid: %s", attackValid ? "YES" : "NO");
    }
    else
    {
        ImGui::TextDisabled("Attack Valid Range: Not used for this Intent");
    }
    ImGui::Text("Reposition Reason: %s", intentRepositionReason.c_str());
    ImGui::Text("Post Attack Reposition Boost Pending: %s",
        postAttackCombatRepositionBoostPending ? "YES" : "NO");
    ImGui::Text("Post Attack Reposition Weight: %.1f",
        postAttackCombatRepositionWeight);
    ImGui::Text("Post Attack Boost Applied: %s",
        postAttackCombatRepositionBoostApplied ? "YES" : "NO");
    ImGui::Text("Intent Lifecycle: %s", intentLifecycleState.c_str());
    ImGui::Text("Lifecycle Reason: %s", intentLifecycleReason.c_str());
    ImGui::TextWrapped("Lifecycle Trace: %s", intentLifecycleTrace.c_str());
    if (ImGui::TreeNode("Intent Selection"))
    {
        const float totalIntentWeight = GetTotalIntentWeight();
        for (size_t i = 0; i < combatIntentData.size(); ++i)
        {
            BossIntentData& data = combatIntentData[i];
            const float probability = totalIntentWeight > 0.0f
                ? combatIntentEffectiveWeights[i] / totalIntentWeight * 100.0f
                : 0.0f;
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%s", intentTypes[i]);
            ImGui::DragFloat("Near Weight", &data.nearWeight, 1.0f, 0.0f, 1000.0f, "%.1f");
            ImGui::DragFloat("Middle Weight", &data.middleWeight, 1.0f, 0.0f, 1000.0f, "%.1f");
            ImGui::DragFloat("Far Weight", &data.farWeight, 1.0f, 0.0f, 1000.0f, "%.1f");
            data.nearWeight = (std::max)(0.0f, data.nearWeight);
            data.middleWeight = (std::max)(0.0f, data.middleWeight);
            data.farWeight = (std::max)(0.0f, data.farWeight);
            ImGui::Text("Effective Weight: %.2f", combatIntentEffectiveWeights[i]);
            ImGui::Text("Selection Probability: %.2f%%", probability);
            ImGui::Text("Candidate Status: %s",
                combatIntentEffectiveWeights[i] > 0.0f ? "Candidate" : "ZeroWeight");
            ImGui::PopID();
        }
        ImGui::Text("Total Intent Weight: %.2f", totalIntentWeight);
        if (hasLastIntentRandomRoll)
        {
            ImGui::Text("Last Intent Roll: %.2f / %.2f",
                lastIntentRandomRoll, lastIntentRandomTotalWeight);
            for (size_t i = 0; i < combatIntentData.size(); ++i)
            {
                if (lastIntentRandomWeights[i] <= 0.0f)
                    continue;
                ImGui::Text("%s  %.2f - %.2f  (Weight %.2f)",
                    intentTypes[i], lastIntentRandomRangeBegin[i],
                    lastIntentRandomRangeEnd[i], lastIntentRandomWeights[i]);
            }
            ImGui::Text("Selected Intent: %s",
                lastSelectedIntent ? intentTypes[static_cast<int>(*lastSelectedIntent)] : "None");
        }
        if (ImGui::Button("Select Intent Once"))
            SelectIntentByWeight();
        ImGui::SameLine();
        if (ImGui::Button("Clear Active Intent"))
            ClearActiveIntent();
        if (ImGui::Button("Mark Positioning Attempted"))
            MarkIntentPositioningAttempted();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Combat Action Candidates"))
    {
        ImGui::Text("Total Effective Weight: %.2f", GetTotalActionWeight());

        for (size_t i = 0; i < combatActionData.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            BossActionData& actionData = combatActionData[i];
            ImGui::Text("%s", actionTypes[i]);
            ImGui::DragFloat("Cooldown Duration", &actionData.cooldownDuration, 0.05f, 0.0f, 30.0f, "%.2f sec");
            ImGui::Text("Cooldown Remaining: %.2f sec", combatActionCooldownRemaining[i]);
            ImGui::Text("Cooldown State: %s", combatActionCooldownRemaining[i] <= 0.0f ? "Ready" : "Cooldown");
            ImGui::Text("Required By Intent: %s", IsActionForCurrentIntent(actionData.type, targetContext) ? "Yes" : "No");
            ImGui::Text("Candidate Reason: %s", candidateReasonNames[static_cast<int>(combatActionCandidateReasons[i])]);
            ImGui::Text("Candidate: %s", combatActionCandidateFlags[i] ? "Yes" : "No");
            ImGui::Text("Effective Weight: %.2f", combatActionEffectiveWeights[i]);
            ImGui::Separator();
            ImGui::PopID();
        }
        ImGui::TreePop();
    }



    if (ImGui::TreeNode("Positioning Tuning"))
    {
        for (BossPositioningData& data : combatPositioningData)
        {
            const char* actionName = actionTypes[static_cast<int>(data.actionType)];
            if (ImGui::TreeNode(actionName))
            {
                for (BossActionData& actionData : combatActionData)
                {
                    if (actionData.type == data.actionType)
                    {
                        ImGui::DragFloat("Weight", &actionData.weight, 0.5f, 0.0f, 100.0f, "%.2f");
                        break;
                    }
                }
                if (IsRepositionAction(data.actionType))
                {
                    ImGui::TextDisabled("Max Move Distance: Combat Reposition Distance");
                    ImGui::TextDisabled("Move Speed: Combat Reposition Move Speed");
                }
                else
                {
                    ImGui::DragFloat("Max Move Distance", &data.maxMoveDistance, 0.1f, 0.0f, 30.0f, "%.2f");
                    ImGui::DragFloat("Move Speed", &data.moveSpeed, 0.1f, 0.0f, 20.0f, "%.2f");
                }
                ImGui::DragFloat("Timeout", &data.timeout, 0.05f, 0.01f, 20.0f, "%.2f sec");
                ImGui::DragFloat("Stuck Time Threshold", &data.stuckTimeThreshold, 0.05f, 0.0f, 10.0f, "%.2f sec");
                ImGui::DragFloat("Stuck Movement Threshold", &data.stuckMovementThreshold, 0.01f, 0.0f, 10.0f, "%.2f m/sec");
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    const char* positioningIntentNames[] = { "CloseCombat", "DashAttackPlan", "JumpAttackPlan", "CombatReposition" };
    const auto drawPosition = [](const char* label, const DirectX::XMFLOAT3& position)
        {
            ImGui::Text("%s: (%.2f, %.2f, %.2f)", label, position.x, position.y, position.z);
        };
    const auto drawPositioningSnapshot = [&](const PositioningDebugSnapshot& snapshot, bool active)
        {
            if (!snapshot.valid)
            {
                ImGui::TextDisabled("No positioning record");
                return;
            }
            const char* intentName = snapshot.intent
                ? positioningIntentNames[static_cast<int>(*snapshot.intent)]
                : "None";
            ImGui::Text("Intent: %s", intentName);
            ImGui::Text("Action: %s", actionTypes[static_cast<int>(snapshot.data.actionType)]);
            ImGui::Text("Preferred Range: %.2f - %.2f m", snapshot.preferredMin, snapshot.preferredMax);
            ImGui::Text("Start Player Distance: %.3f m", snapshot.startPlayerDistance);
            ImGui::Text("Current/End Player Distance: %.3f m", snapshot.currentPlayerDistance);
            ImGui::Text("Target Distance: %.3f m", snapshot.data.targetDistance);
            drawPosition("Start Boss Position", snapshot.startBossPosition);
            drawPosition("Start Player Position", snapshot.startPlayerPosition);
            drawPosition("Desired Target Before Clamp", snapshot.desiredTargetPosition);
            drawPosition("Positioning Target After Clamp", snapshot.clampedTargetPosition);
            ImGui::Text("Was Clamped: %s", snapshot.targetWasClamped ? "YES" : "NO");
            ImGui::Text("Clamp Distance: %.3f m", snapshot.targetClampDistance);
            ImGui::Text("Available Move Distance: %.3f m", snapshot.availableMoveDistance);
            ImGui::TextColored(snapshot.availableMoveDistance < minimumPositioningMoveDistance
                ? ImVec4(1.0f, 0.45f, 0.2f, 1.0f) : ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                "Below Minimum Move Distance: %s (%.2f m)",
                snapshot.availableMoveDistance < minimumPositioningMoveDistance ? "YES" : "NO",
                minimumPositioningMoveDistance);
            drawPosition(active ? "Current Position" : "End Position", snapshot.currentPosition);
            ImGui::Text("Planned Move: %.3f m", snapshot.plannedMoveDistance);
            ImGui::Text("Actual Move (Start -> Current): %.3f m", snapshot.actualMoveDistance);
            ImGui::Text("Traveled Path: %.3f m", snapshot.traveledPathDistance);
            ImGui::Text("Target Remaining: %.3f m", snapshot.remainingDistance);
            ImGui::Text("Requested Move Direction: (%.3f, %.3f, %.3f)",
                snapshot.requestedMoveDirection.x, snapshot.requestedMoveDirection.y,
                snapshot.requestedMoveDirection.z);
            ImGui::Text("Requested Speed: %.3f m/s", snapshot.data.moveSpeed);
            ImGui::Text("Input Magnitude: %.3f", snapshot.inputMagnitude);
            ImGui::Text("Actual Move This Frame: %.4f m", snapshot.frameMovement);
            ImGui::Text("Actual Speed (Position Delta): %.3f m/s", snapshot.actualSpeed);
            if (characterMovementComponent)
                ImGui::Text("Movement Component Actual Speed: %.3f m/s",
                    characterMovementComponent->GetActualHorizontalSpeed());
            ImGui::Text("Positioning Time: %.3f / %.3f sec", snapshot.elapsedTime, snapshot.data.timeout);
            ImGui::Text("Stuck: %.3f / %.3f sec (speed < %.3f m/s)",
                snapshot.stuckTimer, snapshot.data.stuckTimeThreshold,
                snapshot.data.stuckMovementThreshold);
            ImGui::Text("Safe Bounds at Start: X %.2f - %.2f, Z %.2f - %.2f (Margin %.2f)",
                snapshot.safeMinX, snapshot.safeMaxX, snapshot.safeMinZ, snapshot.safeMaxZ,
                snapshot.safetyMargin);
            ImGui::Text("Target In BossRoom Safe Bounds: YES (clamped before use)");
            ImGui::TextDisabled("Wall Hit/Normal: Unavailable; movement RayCast result is not exposed");
            ImGui::Text("Rotation: RotateTowardsPlayer(requested move direction), %.1f deg/s", turnSpeed);
            const auto controller = GetBodyAnimationController();
            if (active)
            {
                ImGui::SeparatorText("Positioning Animation");
                ImGui::Text("Actual Speed: %.3f m/s", positioningAnimationActualSpeed);
                ImGui::Text("Animation State: %s", positioningAnimationMoving ? "Moving" : "Idle");
                ImGui::Text("Move Start Threshold: %.3f m/s", positioningMoveStartSpeedThreshold);
                ImGui::Text("Move Stop Threshold: %.3f m/s", positioningMoveStopSpeedThreshold);
                ImGui::Text("Move Stop Timer: %.3f / %.3f sec",
                    positioningMoveStopTimer, positioningMoveStopDelay);
                ImGui::Text("Current Animation: %s",
                    controller ? controller->GetCurrentAnimationName().c_str() : "None");
            }
            ImGui::Text("End Reason: %s", snapshot.endReason.c_str());
        };

    ImGui::SeparatorText("Boss Room Positioning");
    ImGui::Text("BossRoom Bounds X: %.2f - %.2f", bossRoomMinX, bossRoomMaxX);
    ImGui::Text("BossRoom Bounds Z: %.2f - %.2f", bossRoomMinZ, bossRoomMaxZ);
    constexpr float maximumBossRoomMargin =
        (std::min)((bossRoomMaxX - bossRoomMinX) * 0.5f,
            (bossRoomMaxZ - bossRoomMinZ) * 0.5f) - 0.01f;
    if (ImGui::DragFloat("Safety Margin", &bossRoomSafetyMargin, 0.05f,
        0.0f, maximumBossRoomMargin, "%.2f m"))
        bossRoomSafetyMargin = std::clamp(bossRoomSafetyMargin, 0.0f, maximumBossRoomMargin);
    ImGui::DragFloat("Positioning Arrival Distance", &positioningArrivalDistance,
        0.01f, 0.01f, 2.0f, "%.2f m");
    positioningArrivalDistance = std::clamp(positioningArrivalDistance, 0.01f, 2.0f);
    ImGui::DragFloat("Minimum Move Distance",
        &minimumPositioningMoveDistance, 0.05f, 0.0f, 5.0f, "%.2f m");
    minimumPositioningMoveDistance = (std::max)(0.0f, minimumPositioningMoveDistance);
    ImGui::SeparatorText("Positioning Animation Tuning");
    ImGui::DragFloat("Move Start Speed Threshold",
        &positioningMoveStartSpeedThreshold, 0.01f, 0.01f, 10.0f, "%.2f m/s");
    positioningMoveStartSpeedThreshold =
        std::clamp(positioningMoveStartSpeedThreshold, 0.01f, 10.0f);
    ImGui::DragFloat("Move Stop Speed Threshold",
        &positioningMoveStopSpeedThreshold, 0.01f, 0.0f,
        positioningMoveStartSpeedThreshold - 0.01f, "%.2f m/s");
    positioningMoveStopSpeedThreshold = std::clamp(
        positioningMoveStopSpeedThreshold, 0.0f,
        positioningMoveStartSpeedThreshold - 0.01f);
    ImGui::DragFloat("Move Stop Delay", &positioningMoveStopDelay,
        0.01f, 0.0f, 2.0f, "%.2f sec");
    positioningMoveStopDelay = (std::max)(0.0f, positioningMoveStopDelay);
    ImGui::Text("Safe X: %.2f - %.2f",
        bossRoomMinX + bossRoomSafetyMargin, bossRoomMaxX - bossRoomSafetyMargin);
    ImGui::Text("Safe Z: %.2f - %.2f",
        bossRoomMinZ + bossRoomSafetyMargin, bossRoomMaxZ - bossRoomSafetyMargin);
    ImGui::TextDisabled("Margin changes apply to the next Positioning target; active target stays fixed.");
    ImGui::SeparatorText("Boss Positioning Debug");
    ImGui::Checkbox("Positioning World Debug", &positioningWorldDebug);
    ImGui::SeparatorText("Combat Reposition Debug");
    const char* repositionReasonName = repositionReason == BossRepositionReason::BackTaken
        ? "BackTaken" : "Normal";
    const char* repositionDirectionName = selectedRepositionDirection == BossRepositionDirection::Left
        ? "Left" : (selectedRepositionDirection == BossRepositionDirection::Right ? "Right" : "None");
    ImGui::Text("Reposition Intent Weight: %.2f",
        combatIntentEffectiveWeights[static_cast<int>(BossIntentType::CombatReposition)]);
    ImGui::Text("Reposition Reason: %s", repositionReasonName);
    ImGui::Text("Reposition Direction: %s", repositionDirectionName);
    ImGui::Text("Left Available Distance: %.3f m", repositionLeftAvailableDistance);
    ImGui::Text("Right Available Distance: %.3f m", repositionRightAvailableDistance);
    ImGui::Text("Desired Target: (%.2f, %.2f, %.2f)",
        repositionDesiredTarget.x, repositionDesiredTarget.y, repositionDesiredTarget.z);
    ImGui::Text("Clamped Target: (%.2f, %.2f, %.2f)",
        repositionClampedTarget.x, repositionClampedTarget.y, repositionClampedTarget.z);
    ImGui::Text("Completion / Failure Reason: %s", repositionCompletionReason.c_str());
    ImGui::Text("No Safe Direction: %s", repositionNoSafeDirection ? "YES" : "NO");
    if (IsCombatRepositionActive() && positioningDebugActive)
    {
        ImGui::SeparatorText("Active Combat Reposition");
        if (combatRepositionSettling)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.15f, 1.0f),
                "*** REPOSITION SETTLE ***");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.15f, 1.0f),
                "*** REPOSITIONING ***");
        }
        ImGui::Text("Reposition Phase: %s",
            combatRepositionSettling ? "Settling" : "Moving");
        ImGui::Text("Settle Duration: %.3f sec", combatRepositionSettleDuration);
        ImGui::Text("Settle Remaining: %.3f sec", combatRepositionSettleRemaining);
        ImGui::Text("Current Intent: CombatReposition");
        ImGui::Text("Current Action: %s",
            actionTypes[static_cast<int>(selectedActionType)]);
        ImGui::Text("Reposition Distance: %.2f m",
            combatRepositionMoveDistance);
        ImGui::Text("Combat Reposition Move Speed: %.2f m/s",
            combatRepositionMoveSpeed);
        ImGui::Text("Max Move Distance Actually Used: %.2f m",
            currentPositioningDebug.data.maxMoveDistance);
        ImGui::Text("Traveled Distance: %.3f m",
            currentPositioningDebug.traveledPathDistance);
        drawPosition("Target Position",
            currentPositioningDebug.clampedTargetPosition);
        ImGui::Text("Remaining Distance: %.3f m",
            currentPositioningDebug.remainingDistance);
        ImGui::Text("Completion Reason: %s",
            currentPositioningDebug.endReason.c_str());
        ImGui::Text("Reposition Suppression: %s",
            suppressCombatRepositionForNextIntentSelection ? "YES" : "NO");
    }
    ImGui::SeparatorText("Current Positioning");
    ImGui::Text("Active: %s", positioningDebugActive ? "YES" : "NO");
    if (positioningDebugActive)
        drawPositioningSnapshot(currentPositioningDebug, true);
    else
        ImGui::TextDisabled("No active positioning; see Last Positioning below.");
    ImGui::SeparatorText("Last Positioning");
    drawPositioningSnapshot(lastPositioningDebug, false);

    ImGui::DragFloat("Attack Facing Angle", &attackFacingAngle, 1.0f, 0.0f, 180.0f, "%.1f deg");
    ImGui::DragFloat("Turn Speed", &turnSpeed, 1.0f, 0.0f, 720.0f, "%.1f deg/sec");
    ImGui::DragFloat("Turn Complete Angle", &turnCompleteAngle, 1.0f, 0.0f, 180.0f, "%.1f deg");
    ImGui::DragFloat("Turn Timeout", &turnTimeout, 0.05f, 0.0f, 10.0f, "%.2f sec");
    ImGui::Text("Current AI State: %s", stateMachine_ ? stateMachine_->GetStateName() : "None");
    ImGui::Text("Last Decision Reason: %s", lastAIDecisionReason.c_str());
    ImGui::SeparatorText("JumpAttack Debug");
    ImGui::DragFloat("Max Jump Distance", &maxJumpDistance, 0.05f, 0.0f, 30.0f, "%.2f");
    ImGui::DragFloat("Desired Attack Distance", &desiredAttackDistance, 0.05f, 0.0f, 10.0f, "%.2f");
    currentJumpPlayerDistance = GetDistanceToPlayer();
    ImGui::Text("Current Player Distance: %.3f", currentJumpPlayerDistance);
    ImGui::Text("Calculated Jump Distance: %.3f", calculatedJumpDistance);
    ImGui::Text("Jump Override: %s", jumpMotionWarpOverrideActive ? "Active" : "Inactive");
    ImGui::Text("Selected Attack: %s", attackTypes[static_cast<int>(selectedAttackType)]);
    ImGui::Text("Current AI Mode: %s", aiModes[static_cast<int>(bossAIMode)]);
    ImGui::Text("Last Attack: %s",
        hasLastAttack ? attackTypes[static_cast<int>(lastAttackType)] : "None");
    ImGui::DragFloat("Repeat Weight Scale", &repeatWeightScale, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::Text("Last Think Distance: %.3f", lastCombatSelectionDistance);
    if (ImGui::TreeNode("Combat Attack Candidates"))
    {
        for (size_t i = 0; i < combatAttackData.size(); ++i)
        {
            auto& attack = combatAttackData[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::SeparatorText(attack.animationName.c_str());
            ImGui::DragFloat("Min Distance", &attack.minDistance, 0.1f, 0.0f, 100.0f, "%.2f");
            ImGui::DragFloat("Max Distance", &attack.maxDistance, 0.1f, 0.0f, 100.0f, "%.2f");
            attack.minDistance = (std::max)(0.0f, attack.minDistance);
            attack.maxDistance = (std::max)(attack.minDistance, attack.maxDistance);
            attack.weight = (std::max)(0.0f, attack.weight);
            ImGui::DragFloat("Recovery Duration", &attack.recoveryDuration, 0.05f, 0.0f, 30.0f, "%.2f sec");
            attack.recoveryDuration = (std::max)(0.0f, attack.recoveryDuration);
            ImGui::Text("Effective Weight: %.3f", combatEffectiveWeights[i]);
            ImGui::Text("Last Think: %s", combatCandidateFlags[i] ? "Candidate" : "OutOfRange");
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    ImGui::DragFloat("pitchBaseValue", &pitchBaseValue, 0.05f);
    if (ImGui::Button("Attack"))
    {
        stateMachine_->ChangeState("EnemyAttackState");
    }
    if (ImGui::Button(U8("ボスの名前の演出")))
    {
        StartGruxNamePerform(2.0f);
    }
    ImGui::DragFloat(U8("lockOnOffsetY"), &lockOnOffsetY, 0.05f, 0.1f, 10.0f);
    ImGui::DragFloat(U8("ロックオンプレイヤー側に押し出すオフセット"), &lockOnOffset, 0.05f, 0.1f, 10.0f);
    ImGui::DragFloat("Boss Damage Flash Duration", &damageFlashDuration, 0.01f, 0.01f, 2.0f, "%.2f sec");
    ImGui::SliderFloat("Boss Damage Flash Strength", &damageFlashStartValue, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Boss Hit Voice Cooldown", &hitVoiceCooldown, 0.01f, 0.0f, 2.0f, "%.2f sec");
    ImGui::DragFloat(U8("ボス戦時のカメラ距離"), &bossBattleCameraDistance, 0.5f);
    ImGui::DragFloat(U8("ボス戦時のカメラ右方向の距離"), &bossBattleCameraRightDistance, 0.5f);
    ImGui::DragFloat(U8("ボスの武器の攻撃範囲"), &hitWeaponRadius, 0.05f, 0.1f, 2.0f);
    ImGui::DragFloat(U8("enemyScale"), &enemyScale, 0.1f);
    ImGui::DragFloat(U8("hitEnemyEffectOffsetY"), &hitEnemyEffectOffsetY, 0.1f);
    ImGui::DragFloat(U8("hitPlayerEffectOffsetY"), &hitPlayerEffectOffsetY, 0.1f);
    ImGui::DragFloat3(U8("ボス戦時のオフセット"), &bossBattleCameraOffset.x, 0.5f);
    ImGui::SeparatorText("PrimaryAttack_LA Hit Debug");
    ImGui::Text("hitActors: %zu", hitActors.size());
    ImGui::Text("attackHitCount: %d", currentAttackHitCount);
    ImGui::Text("leftHitBox: %s", leftHitBox ? "true" : "false");
    ImGui::Text("rightHitBox: %s", rightHitBox ? "true" : "false");    ImGui::Text("Active HitBox Radius L/R: %.3f / %.3f",
        activeLeftHitBoxRadius, activeRightHitBoxRadius);
    const float activeHitBoxElapsed = GetActiveHitBoxElapsedForDebug();
    if (activeHitBoxElapsed >= 0.0f)
        ImGui::Text("Active HitBox Elapsed: %.3f sec", activeHitBoxElapsed);
    else
        ImGui::Text("Active HitBox Elapsed: N/A");


    ImGui::Begin("Boss AI Debug");
    ImGui::Checkbox("Show AI World Debug", &showBossAIDebug);
    ImGui::SeparatorText("Boss Rotation Debug");
    ImGui::Checkbox("Rotation Debug / World Lines", &showRotationDebug);
    const char* rotationStateName = stateMachine_ ? stateMachine_->GetStateName() : "";
    ImGui::Text("Current State: %s", rotationStateName[0] ? rotationStateName : "None");
    ImGui::Text("Current Intent: %s", activeIntentName);
    ImGui::Text("Intent Step: %s", intentStepNames[static_cast<int>(activeIntentStep)]);
    ImGui::Text("Selected Attack: %s", attackTypes[static_cast<int>(selectedAttackType)]);
    ImGui::Text("Facing Evaluation: %s", attackFacingEvaluationValid ? "Valid" : "None");
    ImGui::Text("Facing Tolerance: %.1f deg", attackFacingEvaluationTolerance);
    ImGui::Text("Angle To Player At Selection: %.3f deg", attackFacingEvaluationAngle);
    ImGui::Text("CloseCombat Ready Facing Angle: %.1f deg",
        closeCombatReadyFacingAngle);
    ImGui::Text("Current Facing Angle Before Ready: %.3f deg",
        currentFacingAngleBeforeReady);
    ImGui::Text("Facing Required: %s", attackFacingEvaluationRequired ? "YES" : "NO");
    ImGui::Text("Pending Facing Valid: %s", pendingAttackFacingValid ? "YES" : "NO");
    const float pendingFacingYaw = pendingAttackFacingValid
        ? DirectX::XMConvertToDegrees(std::atan2f(
            pendingAttackFacingDirection.x, pendingAttackFacingDirection.z))
        : 0.0f;
    ImGui::Text("Pending Facing Yaw: %.3f deg", pendingFacingYaw);
    ImGui::Text("Resume Attack: %s",
        lastResumedAttackValid
        ? attackTypes[static_cast<int>(lastResumedAttackType)]
        : "None");
    ImGui::Text("Attack Ready Active: %s", attackReadyActive ? "YES" : "NO");
    ImGui::Text("Attack Ready Reason: %s",
        attackReadyReason == AttackReadyReason::AfterTurn ? "AfterTurn" : "Front");
    ImGui::Text("Attack Ready Duration: %.3f sec", GetAttackReadyDuration());
    ImGui::Text("Attack Ready Timer: %.3f sec", attackReadyDebugTimer);
    ImGui::Text("Ready Attack Type: %s",
        attackTypes[static_cast<int>(attackReadyDebugType)]);
    ImGui::Text("Facing Complete SE Fired: %s",
        attackReadySEFired ? "YES" : "NO");

    ImGui::Text("Current Yaw: %.3f deg", rotationDebugCurrentYaw);
    ImGui::Text("Player Direction Yaw: %.3f deg", rotationDebugPlayerYaw);
    ImGui::Text("Angle To Player: %.3f deg", targetContext.absoluteAngleDegrees);
    ImGui::Text("Actual Yaw Delta This Frame: %.4f deg", rotationDebugActualYawDelta);
    ImGui::Text("Requested Turn Speed: %.1f deg/s", rotationDebugRequestedTurnSpeed);
    ImGui::Text("Rotation Sources This Frame: %d", rotationDebugSourceCount);
    if (rotationDebugSourceCount == 0)
        ImGui::TextDisabled("  None");
    for (int i = 0; i < rotationDebugSourceCount; ++i)
    {
        const RotationSourceDebug& source = rotationDebugSources[i];
        ImGui::Text("  %d. %s | Target Yaw %.2f | Speed %.1f",
            i + 1, source.source.c_str(), source.targetYaw, source.requestedTurnSpeed);
    }
    ImGui::TextDisabled("World: Forward=Green, Player=Blue, Positioning=Yellow, Turn Target=Magenta");
    ImGui::SeparatorText("Last Turn");
    if (lastTurnDebug.valid)
    {
        ImGui::Text("From State: %s", lastTurnDebug.fromState.c_str());
        ImGui::Text("Start Angle: %.3f deg", lastTurnDebug.startAngle);
        ImGui::Text("Start Target Yaw: %.3f deg", lastTurnDebug.startTargetYaw);
        ImGui::Text("End Angle: %.3f deg", lastTurnDebug.endAngle);
        ImGui::Text("Duration: %.3f sec", lastTurnDebug.duration);
        ImGui::Text("Turn Start / End Threshold: %.1f / %.1f deg",
            attackFacingAngle, turnCompleteAngle);
    }
    else
    {
        ImGui::TextDisabled("No completed Turn recorded");
    }

    const char* fullStateName = stateMachine_ ? stateMachine_->GetStateName() : "";
    const char* stateDisplayName = "None";
    if (std::strcmp(fullStateName, "EnemyIdleState") == 0) stateDisplayName = "Idle";
    else if (std::strcmp(fullStateName, "EnemyThinkState") == 0) stateDisplayName = "Think";
    else if (std::strcmp(fullStateName, "EnemyTurnState") == 0) stateDisplayName = "Turn";
    else if (std::strcmp(fullStateName, "EnemyPositioningState") == 0) stateDisplayName = "Positioning";
    else if (std::strcmp(fullStateName, "EnemyAttackReadyState") == 0) stateDisplayName = "Attack Ready";
    else if (std::strcmp(fullStateName, "EnemyAttackState") == 0) stateDisplayName = "Attack";
    else if (std::strcmp(fullStateName, "EnemyChargeAttackState") == 0) stateDisplayName = "Charge Attack";
    else if (std::strcmp(fullStateName, "EnemyRecoveryState") == 0) stateDisplayName = "Recovery";
    else if (std::strcmp(fullStateName, "EnemyDeathState") == 0) stateDisplayName = "Death";

    ImGui::Text("State: %s (%s)", stateDisplayName, fullStateName[0] ? fullStateName : "None");
    ImGui::Text("Intent: %s", activeIntentName);
    ImGui::Text("Intent Goal: %s", activeIntentGoal);
    ImGui::Text("Intent Step: %s", intentStepNames[static_cast<int>(activeIntentStep)]);
    if (activeIntentData)
    {
        ImGui::Text("Preferred Range: %.2f - %.2f m",
            activeIntentData->preferredMinDistance,
            activeIntentData->preferredMaxDistance);
    }
    ImGui::Text("Range Status: %s", activeRangeStatusName);
    ImGui::Text("Next Action: %s", activeIntentNextAction);
    ImGui::Separator();

    if (targetContext.valid)
    {
        const int distanceRegionIndex = static_cast<int>(targetContext.distanceRegion);
        ImGui::Text("Distance: %.2f", targetContext.xzDistance);
        ImGui::Text("Angle to Player: %.2f deg", targetContext.absoluteAngleDegrees);
        ImGui::Text("Relative Region: %s", relativeRegions[static_cast<int>(targetContext.region)]);
        ImGui::Text("Front Max Angle: %.1f deg", relativeFrontMaxAngle);
        ImGui::Text("Back Min Angle: %.1f deg", relativeBackMinAngle);
        ImGui::Text("Distance Region:");
        for (int i = 0; i < static_cast<int>(std::size(distanceRegions)); ++i)
        {
            if (i == distanceRegionIndex)
                ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.35f, 1.0f), "-> %s", distanceRegions[i]);
            else
                ImGui::TextDisabled("   %s", distanceRegions[i]);
        }
    }
    else
    {
        ImGui::TextDisabled("Target Context: Invalid");
    }

    if (ImGui::CollapsingHeader("Runtime Tuning", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SeparatorText("Boss Sword Trail");
        ImGui::ColorEdit3("Trail Color", &bossTrailColor.x);
        ImGui::DragFloat("Trail Emissive Strength", &bossTrailEmissiveStrength,
            0.05f, 0.0f, 30.0f, "%.2f");
        ImGui::DragFloat("Trail Lifetime", &bossTrailLifetime,
            0.01f, 0.05f, 5.0f, "%.2f sec");
        bossTrailEmissiveStrength = std::clamp(bossTrailEmissiveStrength, 0.0f, 30.0f);
        bossTrailLifetime = std::clamp(bossTrailLifetime, 0.05f, 5.0f);

        constexpr float minimumDistanceGap = 0.1f;
        ImGui::SeparatorText("Distance");
        if (ImGui::DragFloat("Near Distance", &nearDistanceThreshold, 0.1f, 0.0f, 100.0f, "%.2f"))
        {
            nearDistanceThreshold = (std::max)(0.0f, nearDistanceThreshold);
            if (nearDistanceThreshold >= middleDistanceThreshold)
                middleDistanceThreshold = nearDistanceThreshold + minimumDistanceGap;
        }
        if (ImGui::DragFloat("Middle Distance", &middleDistanceThreshold, 0.1f, 0.1f, 200.0f, "%.2f"))
        {
            middleDistanceThreshold = (std::max)(
                nearDistanceThreshold + minimumDistanceGap,
                middleDistanceThreshold);
        }
        constexpr float minimumRelativeSideAngleWidth = 5.0f;
        ImGui::SeparatorText("Player Relative Region");
        if (ImGui::DragFloat("Front Max Angle", &relativeFrontMaxAngle,
            1.0f, 10.0f, 90.0f, "%.1f deg"))
        {
            relativeFrontMaxAngle = std::clamp(relativeFrontMaxAngle,
                10.0f, relativeBackMinAngle - minimumRelativeSideAngleWidth);
        }
        if (ImGui::DragFloat("Back Min Angle", &relativeBackMinAngle,
            1.0f, 90.0f, 170.0f, "%.1f deg"))
        {
            relativeBackMinAngle = std::clamp(relativeBackMinAngle,
                relativeFrontMaxAngle + minimumRelativeSideAngleWidth, 170.0f);
        }
        relativeFrontMaxAngle = std::clamp(relativeFrontMaxAngle,
            10.0f, relativeBackMinAngle - minimumRelativeSideAngleWidth);
        relativeBackMinAngle = std::clamp(relativeBackMinAngle,
            relativeFrontMaxAngle + minimumRelativeSideAngleWidth, 170.0f);

        ImGui::SeparatorText("Combat Behavior");
        const auto getIntentData = [&](const BossIntentType type) -> BossIntentData&
            {
                for (BossIntentData& data : combatIntentData)
                {
                    if (data.type == type)
                        return data;
                }
                return combatIntentData.front();
            };
        BossIntentData& closeCombatIntent =
            getIntentData(BossIntentType::CloseCombat);
        BossIntentData& repositionIntent =
            getIntentData(BossIntentType::CombatReposition);
        BossIntentData& dashIntent =
            getIntentData(BossIntentType::DashAttackPlan);
        BossIntentData& jumpIntent =
            getIntentData(BossIntentType::JumpAttackPlan);

        const auto drawDistanceRegionWeights = [&](const char* regionName,
            float BossIntentData::* weightMember)
            {
                if (!ImGui::TreeNodeEx(regionName, ImGuiTreeNodeFlags_DefaultOpen))
                    return;
                ImGui::PushID(regionName);
                const auto totalBaseWeight = [&]()
                    {
                        return closeCombatIntent.*weightMember +
                            repositionIntent.*weightMember +
                            dashIntent.*weightMember +
                            jumpIntent.*weightMember;
                    };
                DrawIntentWeightBar("Close Combat",
                    closeCombatIntent.*weightMember, totalBaseWeight());
                DrawIntentWeightBar("Combat Reposition",
                    repositionIntent.*weightMember, totalBaseWeight());
                DrawIntentWeightBar("Dash Attack",
                    dashIntent.*weightMember, totalBaseWeight());
                DrawIntentWeightBar("Jump Attack",
                    jumpIntent.*weightMember, totalBaseWeight());
                ImGui::PopID();
                ImGui::TreePop();
            };

        drawDistanceRegionWeights("Near", &BossIntentData::nearWeight);
        drawDistanceRegionWeights("Middle", &BossIntentData::middleWeight);
        drawDistanceRegionWeights("Far", &BossIntentData::farWeight);

        if (ImGui::TreeNodeEx("Back", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto totalBackWeight = [&]()
                {
                    return combatRepositionBackWeight +
                        dashAttackPlanBackWeight + jumpAttackPlanBackWeight;
                };
            float disabledCloseCombatWeight = 0.0f;
            DrawIntentWeightBar("Close Combat", disabledCloseCombatWeight,
                totalBackWeight(), false);
            DrawIntentWeightBar("Combat Reposition",
                combatRepositionBackWeight, totalBackWeight());
            DrawIntentWeightBar("Dash Attack",
                dashAttackPlanBackWeight, totalBackWeight());
            DrawIntentWeightBar("Jump Attack",
                jumpAttackPlanBackWeight, totalBackWeight());
            ImGui::TreePop();
        }

        ImGui::SeparatorText("Post Attack Modifier");
        ImGui::DragFloat("Post Attack CombatReposition Weight",
            &postAttackCombatRepositionWeight, 1.0f, 0.0f, 1000.0f, "%.1f");
        postAttackCombatRepositionWeight = std::clamp(
            postAttackCombatRepositionWeight, 0.0f, 1000.0f);
        ImGui::Text("Boost Pending: %s",
            postAttackCombatRepositionBoostPending ? "YES" : "NO");
        ImGui::Text("Reposition Suppression: %s",
            suppressCombatRepositionForNextIntentSelection ? "YES" : "NO");

        ImGui::SeparatorText("Combat Reposition");
        ImGui::DragFloat("Combat Reposition Distance", &combatRepositionMoveDistance,
            0.1f, 0.0f, 15.0f, "%.2f m");
        combatRepositionMoveDistance = std::clamp(
            combatRepositionMoveDistance, 0.0f, 15.0f);
        ImGui::DragFloat("Combat Reposition Move Speed", &combatRepositionMoveSpeed,
            0.1f, 0.5f, 10.0f, "%.2f m/s");
        combatRepositionMoveSpeed = std::clamp(
            combatRepositionMoveSpeed, 0.5f, 10.0f);
        ImGui::DragFloat("Combat Reposition Settle Duration",
            &combatRepositionSettleDuration, 0.01f, 0.0f, 10.0f, "%.2f sec");
        combatRepositionSettleDuration = std::clamp(
            combatRepositionSettleDuration, 0.0f, 10.0f);

        ImGui::SeparatorText("Attack Ready");
        ImGui::DragFloat("Front Attack Ready Duration", &frontAttackReadyDuration,
            0.01f, 0.0f, 3.0f, "%.2f sec");
        ImGui::DragFloat("Side Attack Ready Duration", &sideAttackReadyDuration,
            0.01f, 0.0f, 3.0f, "%.2f sec");
        ImGui::DragFloat("CloseCombat Ready Facing Angle",
            &closeCombatReadyFacingAngle, 0.1f, 0.0f, 30.0f, "%.1f deg");
        frontAttackReadyDuration = std::clamp(frontAttackReadyDuration, 0.0f, 3.0f);
        sideAttackReadyDuration = std::clamp(sideAttackReadyDuration, 0.0f, 3.0f);
        closeCombatReadyFacingAngle = std::clamp(
            closeCombatReadyFacingAngle, 0.0f, 30.0f);


        const float evaluationTotalWeight = GetTotalActionWeight();
        ImGui::SeparatorText("Positioning Plans");
        for (size_t i = 0; i < combatIntentData.size(); ++i)
        {
            BossIntentData& intentData = combatIntentData[i];
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::TreeNode(intentTypes[i]))
            {
                constexpr float minimumPreferredRangeWidth = 0.1f;
                if (ImGui::DragFloat("Preferred Min", &intentData.preferredMinDistance,
                    0.1f, 0.0f, 100.0f, "%.2f m"))
                {
                    intentData.preferredMinDistance =
                        (std::max)(0.0f, intentData.preferredMinDistance);
                    if (intentData.preferredMinDistance >= intentData.preferredMaxDistance)
                    {
                        intentData.preferredMaxDistance =
                            intentData.preferredMinDistance + minimumPreferredRangeWidth;
                    }
                }
                if (ImGui::DragFloat("Preferred Max", &intentData.preferredMaxDistance,
                    0.1f, 0.1f, 100.0f, "%.2f m"))
                {
                    intentData.preferredMaxDistance = (std::max)(
                        intentData.preferredMinDistance + minimumPreferredRangeWidth,
                        intentData.preferredMaxDistance);
                }
                ImGui::DragFloat(U8("位置取り完了とみなす値"), &intentData.positioningArrivalInset,
                    0.05f, 0.0f, 10.0f, "%.2f m");
                intentData.positioningArrivalInset =
                    (std::max)(0.0f, intentData.positioningArrivalInset);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::SeparatorText("Charge Attack");
        ImGui::DragFloat("Charge Windup End Time", &chargeWindupEndTime,
            0.01f, 0.0f, 10.0f, "%.2f sec");
        ImGui::DragFloat("Charge Speed", &chargeSpeed,
            0.1f, 0.1f, 50.0f, "%.2f m/s");
        ImGui::DragFloat("Charge PlayerHit Recovery Duration",
            &chargePlayerHitRecoveryDuration, 0.05f, 0.0f, 10.0f, "%.2f sec");
        ImGui::DragFloat("Charge JustDodge Recovery Duration",
            &chargeJustDodgeRecoveryDuration, 0.05f, 0.0f, 10.0f, "%.2f sec");
        for (BossAttackData& attackData : combatAttackData)
        {
            if (attackData.type != BossAttackType::ChargeAttack)
                continue;
            ImGui::DragInt("Charge Damage", &attackData.damagePerHit, 1.0f, 0, 1000);
            attackData.damagePerHit = (std::max)(0, attackData.damagePerHit);
            break;
        }
        ImGui::DragFloat("Charge Safety Timeout", &chargeSafetyTimeout,
            0.1f, 0.5f, 30.0f, "%.2f sec");
        ImGui::DragFloat("Wall Cast Safety Margin", &chargeWallCastSafetyMargin,
            0.01f, 0.0f, 2.0f, "%.2f m");
        ImGui::DragFloat("Wall Facing Threshold", &chargeWallFacingThreshold,
            0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Wall Normal Y Threshold", &chargeWallNormalYThreshold,
            0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Wall Cast Radius Scale", &chargeWallCastRadiusScale,
            0.01f, 0.1f, 1.0f, "%.2f");
        chargeWindupEndTime = (std::max)(0.0f, chargeWindupEndTime);
        chargeSpeed = (std::max)(0.1f, chargeSpeed);
        chargePlayerHitRecoveryDuration =
            (std::max)(0.0f, chargePlayerHitRecoveryDuration);
        chargeJustDodgeRecoveryDuration =
            (std::max)(0.0f, chargeJustDodgeRecoveryDuration);
        chargeSafetyTimeout = (std::max)(0.5f, chargeSafetyTimeout);
        chargeWallCastSafetyMargin = (std::max)(0.0f, chargeWallCastSafetyMargin);
        chargeWallFacingThreshold = std::clamp(chargeWallFacingThreshold, 0.0f, 1.0f);
        chargeWallNormalYThreshold = std::clamp(chargeWallNormalYThreshold, 0.0f, 1.0f);
        chargeWallCastRadiusScale = std::clamp(chargeWallCastRadiusScale, 0.1f, 1.0f);

        const char* chargeEndReasonName = "None";
        switch (chargeEndReasonDebug)
        {
        case ChargeAttackEndReason::PlayerHit: chargeEndReasonName = "PlayerHit"; break;
        case ChargeAttackEndReason::JustDodge: chargeEndReasonName = "JustDodge"; break;
        case ChargeAttackEndReason::WallHit: chargeEndReasonName = "WallHit"; break;
        case ChargeAttackEndReason::SafetyTimeout: chargeEndReasonName = "SafetyTimeout"; break;
        default: break;
        }
        ImGui::Text("Charge Phase: %s", chargePhaseDebug.c_str());
        ImGui::Text("Windup Animation Time: %.3f sec", chargeWindupAnimationTimeDebug);
        ImGui::Text("Charge Direction: %.3f, %.3f, %.3f",
            chargeDirection.x, chargeDirection.y, chargeDirection.z);
        ImGui::Text("Charge Elapsed Time: %.3f sec", chargeElapsedTime);
        ImGui::Text("Charge Speed (Active Setting): %.3f", chargeSpeed);
        ImGui::Text("DangerWindow Active: %s",
            chargeDangerWindowActive ? "YES" : "NO");
        ImGui::Text("Just Dodge Success: %s",
            chargeJustDodgeSuccessDebug ? "YES" : "NO");
        ImGui::Text("Charge Movement Active: %s",
            chargeMovementActive ? "YES" : "NO");
        ImGui::Text("Player Cast Hit: %s", chargePlayerCastHitDebug ? "YES" : "NO");
        ImGui::Text("Player Hit Distance: %.3f", chargePlayerHitDistanceDebug);
        ImGui::Text("Player Hit Actor: %s", chargePlayerHitActorDebug.c_str());
        ImGui::Text("Selected Hit: %s", chargeSelectedHitDebug.c_str());
        ImGui::Text("Charge Damage: %d", GetDamageForCurrentAttack());
        ImGui::Text("Wall Cast Hit: %s", chargeWallCastHitDebug ? "YES" : "NO");
        ImGui::Text("Wall Facing Amount: %.3f", chargeWallFacingAmountDebug);
        ImGui::Text("Wall Hit Normal: %.3f, %.3f, %.3f",
            chargeWallHitNormalDebug.x, chargeWallHitNormalDebug.y, chargeWallHitNormalDebug.z);
        ImGui::Text("Wall Hit Distance: %.3f", chargeWallHitDistanceDebug);
        ImGui::Text("Charge End Reason: %s", chargeEndReasonName);
        ImGui::Text("Safety Timeout: %.2f sec (%s)", chargeSafetyTimeout,
            chargeEndReasonDebug == ChargeAttackEndReason::SafetyTimeout
            ? "TRIGGERED" : "Not Triggered");

        ImGui::SeparatorText("Recovery Override");
        ImGui::DragFloat("Post Stun Recovery Duration",
            &postStunRecoveryDuration, 0.01f, 0.0f, 10.0f, "%.2f sec");
        postStunRecoveryDuration = (std::max)(0.0f, postStunRecoveryDuration);
        ImGui::Text("Current Recovery Duration: %.3f sec",
            currentRecoveryDurationDebug);
        ImGui::Text("Recovery Elapsed: %.3f sec", recoveryElapsedDebug);
        ImGui::Text("Recovery Source: %s", recoverySourceDebug.c_str());

        ImGui::SeparatorText("Jump Attack Telegraph");
        const auto animationController = GetBodyAnimationController();
        const float telegraphClipDuration = animationController
            ? animationController->GetAnimationLength("Pre_Stampede_0")
            : 7.5f;
        constexpr float minimumTelegraphRange = 0.01f;
        if (ImGui::DragFloat("Jump Telegraph Start", &jumpAttackTelegraphStartTime,
            0.01f, 0.0f, telegraphClipDuration - minimumTelegraphRange, "%.2f sec"))
        {
            jumpAttackTelegraphStartTime = std::clamp(
                jumpAttackTelegraphStartTime, 0.0f,
                jumpAttackTelegraphEndTime - minimumTelegraphRange);
        }
        if (ImGui::DragFloat("Jump Telegraph End", &jumpAttackTelegraphEndTime,
            0.01f, minimumTelegraphRange, telegraphClipDuration, "%.2f sec"))
        {
            jumpAttackTelegraphEndTime = std::clamp(
                jumpAttackTelegraphEndTime,
                jumpAttackTelegraphStartTime + minimumTelegraphRange,
                telegraphClipDuration);
        }

        ImGui::SeparatorText("Action Tuning");
        for (size_t i = 0; i < combatActionData.size(); ++i)
        {
            BossActionData& actionData = combatActionData[i];
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::TreeNode(actionTypes[i]))
            {
                ImGui::DragFloat("Weight", &actionData.weight, 0.5f, 0.0f, 1000.0f, "%.1f");
                actionData.weight = (std::max)(0.0f, actionData.weight);
                ImGui::DragFloat("Base Cooldown", &actionData.cooldownDuration, 0.05f, 0.0f, 60.0f, "%.2f sec");
                actionData.cooldownDuration = (std::max)(0.0f, actionData.cooldownDuration);
                ImGui::Text("Cooldown Remaining: %s",
                    combatActionCooldownRemaining[i] <= 0.0f ? "READY" : "Active");

                if (actionData.attackType)
                {
                    for (BossAttackData& attackData : combatAttackData)
                    {
                        if (attackData.type != *actionData.attackType)
                            continue;
                        ImGui::DragFloat("Recovery", &attackData.recoveryDuration, 0.05f, 0.0f, 30.0f, "%.2f sec");
                        attackData.recoveryDuration = (std::max)(0.0f, attackData.recoveryDuration);
                        ImGui::DragInt("Damage / Hit", &attackData.damagePerHit, 1.0f, 0, 1000);
                        attackData.damagePerHit = (std::max)(0, attackData.damagePerHit);
                        break;
                    }
                }

                const bool candidate =
                    combatActionCandidateReasons[i] == BossActionCandidateReason::Candidate;
                const float probability = candidate && evaluationTotalWeight > 0.0f
                    ? combatActionEffectiveWeights[i] / evaluationTotalWeight
                    : 0.0f;
                char probabilityLabel[32]{};
                sprintf_s(probabilityLabel, "%.1f%%", probability * 100.0f);
                ImGui::Text("Status: %s", candidateReasonNames[static_cast<int>(combatActionCandidateReasons[i])]);
                ImGui::ProgressBar(probability, ImVec2(-1.0f, 0.0f), probabilityLabel);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (ImGui::Button("Reset AI Tuning"))
        {
            combatIntentData = initialCombatIntentData;
            for (size_t i = 0; i < combatActionData.size(); ++i)
            {
                combatActionData[i].weight = initialCombatActionData[i].weight;
                combatActionData[i].cooldownDuration = initialCombatActionData[i].cooldownDuration;
            }
            for (size_t i = 0; i < combatAttackData.size(); ++i)
            {
                combatAttackData[i].recoveryDuration = initialCombatAttackData[i].recoveryDuration;
                combatAttackData[i].damagePerHit = initialCombatAttackData[i].damagePerHit;
            }
            nearDistanceThreshold = initialNearDistanceThreshold;
            middleDistanceThreshold = initialMiddleDistanceThreshold;
            relativeFrontMaxAngle = initialRelativeFrontMaxAngle;
            relativeBackMinAngle = initialRelativeBackMinAngle;
            combatRepositionMoveDistance = initialCombatRepositionMoveDistance;
            combatRepositionMoveSpeed = initialCombatRepositionMoveSpeed;
            combatRepositionSettleDuration = initialCombatRepositionSettleDuration;
            combatRepositionBackWeight = initialCombatRepositionBackWeight;
            dashAttackPlanBackWeight = initialDashAttackPlanBackWeight;
            jumpAttackPlanBackWeight = initialJumpAttackPlanBackWeight;
            postAttackCombatRepositionWeight =
                initialPostAttackCombatRepositionWeight;
            frontAttackReadyDuration = initialFrontAttackReadyDuration;
            sideAttackReadyDuration = initialSideAttackReadyDuration;
            closeCombatReadyFacingAngle = initialCloseCombatReadyFacingAngle;
            recentAttackPenaltyLast = initialRecentAttackPenaltyLast;
            recentAttackPenaltySecond = initialRecentAttackPenaltySecond;
        }
        ImGui::TextDisabled("Runtime only. Reset does not change active Cooldown Remaining.");
        ImGui::TextDisabled("Recovery edits affect an active Recovery on its next update.");
    }

    ImGui::SeparatorText("Action Evaluation (Last AI Evaluation)");
    const float actionEvaluationTotalWeight = GetTotalActionWeight();
    if (ImGui::BeginTable("BossAIActionTable", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Action");
        ImGui::TableSetupColumn("Base");
        ImGui::TableSetupColumn("Effective");
        ImGui::TableSetupColumn("Cooldown");
        ImGui::TableSetupColumn("Probability");
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < combatActionData.size(); ++i)
        {
            const BossActionCandidateReason reason = combatActionCandidateReasons[i];
            const bool candidate = reason == BossActionCandidateReason::Candidate;
            const bool selected = hasSelectedActionDebug && combatActionData[i].type == selectedActionType;
            const ImVec4 rowColor = selected
                ? ImVec4(1.0f, 0.85f, 0.25f, 1.0f)
                : (candidate ? ImVec4(0.35f, 1.0f, 0.35f, 1.0f) : ImVec4(0.55f, 0.55f, 0.55f, 1.0f));

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(rowColor, "%s%s", selected ? "-> " : "", actionTypes[i]);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.1f", combatActionData[i].weight);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", combatActionEffectiveWeights[i]);
            ImGui::TableSetColumnIndex(3);
            if (combatActionCooldownRemaining[i] <= 0.0f)
                ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.35f, 1.0f), "READY");
            else
                ImGui::TextDisabled("%.2fs", combatActionCooldownRemaining[i]);
            ImGui::TableSetColumnIndex(4);
            const float probability = candidate && actionEvaluationTotalWeight > 0.0f
                ? combatActionEffectiveWeights[i] / actionEvaluationTotalWeight
                : 0.0f;
            ImGui::ProgressBar(probability, ImVec2(-1.0f, 0.0f), "");
            ImGui::SameLine();
            ImGui::Text("%.1f%%", probability * 100.0f);
            ImGui::TableSetColumnIndex(5);
            ImGui::TextColored(rowColor, "%s", candidateReasonNames[static_cast<int>(reason)]);
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Weighted Random Result (Last Selection)");
    //if (hasLastActionRandomDebug)
    {
        ImGui::Text("Total Candidate Weight: %.2f", lastActionRandomTotalWeight);
        ImGui::Text("Last Random Roll: %.2f / %.2f",
            lastActionRandomRoll,
            lastActionRandomTotalWeight);

        float rangeStart = 0.0f;
        for (size_t i = 0; i < lastActionRandomWeights.size(); ++i)
        {
            const float weight = lastActionRandomWeights[i];
            if (weight <= 0.0f)
                continue;

            const float rangeEnd = rangeStart + weight;
            const float probability = lastActionRandomTotalWeight > 0.0f
                ? weight / lastActionRandomTotalWeight
                : 0.0f;
            ImGui::Text("%s  %.2f - %.2f  (%.1f%%)",
                actionTypes[i],
                rangeStart,
                rangeEnd,
                probability * 100.0f);
            rangeStart = rangeEnd;
        }
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f),
            "Selected: %s",
            hasSelectedActionDebug ? actionTypes[static_cast<int>(selectedActionType)] : "None");
    }
    //else
    //{
    //    ImGui::TextDisabled("No weighted-random selection recorded yet.");
    //}

    ImGui::End();


#endif
}

//当たった時の処理
float GruxEnemy::GetActiveHitBoxElapsedForDebug() const
{
    if (activeHitBoxNotifyStates.empty())
        return -1.0f;
    const auto controller = GetBodyAnimationController();
    if (!controller)
        return -1.0f;
    float startTime = FLT_MAX;
    for (const auto* state : activeHitBoxNotifyStates)
    {
        if (state)
            startTime = (std::min)(startTime, state->startTime);
    }
    return startTime < FLT_MAX
        ? (std::max)(0.0f, controller->GetCurrentAnimationTime() - startTime)
        : -1.0f;
}

std::string GruxEnemy::GetCurrentAttackNameForDebug() const
{
    const auto controller = GetBodyAnimationController();
    const std::string animationName = controller
        ? controller->GetCurrentAnimationName()
        : "UnknownAnimation";
    const char* attackName = "UnknownAttack";
    switch (selectedAttackType)
    {
    case BossAttackType::PrimaryAttackLA: attackName = "PrimaryAttackLA"; break;
    case BossAttackType::PrimaryAttackRA: attackName = "PrimaryAttackRA"; break;
    case BossAttackType::FastCombo: attackName = "FastCombo"; break;
    case BossAttackType::JumpAttack: attackName = "JumpAttack"; break;
    case BossAttackType::Dash: attackName = "Dash"; break;
    case BossAttackType::DashAttack: attackName = "DashAttack"; break;
    case BossAttackType::ChargeAttack: attackName = "ChargeAttack"; break;
    case BossAttackType::LongRangeAttack: attackName = "LongRangeAttack"; break;
    }
    return std::string(attackName) + " / " + animationName;
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

void GruxEnemy::TakeDamage(const int damage)
{
    skeletalMeshComponent->plusAlphaCBuffer->data.flashValue = damageFlashStartValue;
    // コントローラー振動
    InputSystem::SetVibration(0.8f, 0.1f);
    CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_damage.wav", 0.3f);

    const int hpBeforeDamage = hp;
    hp -= damage;
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
            hitVoiceCooldownTimer = hitVoiceCooldown;
        }
    }
    if (!rushHpDisplayActive && hp < hpBeforeDamage)
    {
        // Restart from the HP immediately before each successful hit.
        delayedHp = static_cast<float>(hpBeforeDamage);
        delayedHpDelayTimer = delayedHpDelayDuration;
        useRushDelayedHpFollowSpeed = false;
    }
    Logger::Log(U8("エネミーにダメージ！ HP:") + std::to_string(hp));
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

void GruxEnemy::SpawnRushHitRing(const DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 playerPos) const
{
    DirectX::XMFLOAT3 enemyCenter = GetPosition();
    constexpr float rushEffectHeightOffset = 1.3f;
    enemyCenter.y += rushEffectHeightOffset;
    playerPos.y += rushEffectHeightOffset;

    // 敵→プレイヤー方向
    DirectX::XMFLOAT3 forward = MathHelper::Normalize(MathHelper::Subtract(playerPos, enemyCenter));
    // エフェクト生成位置
    float spawnOffset = 0.8f;
    DirectX::XMFLOAT3 rushEffectPosition = MathHelper::Add(enemyCenter, MathHelper::Multiply(forward, spawnOffset));

    rushEffectPosition.x += MathHelper::RandomRange(-0.45f, 0.45f);
    rushEffectPosition.y += MathHelper::RandomRange(-0.20f, 0.80f);
    rushEffectPosition.z += MathHelper::RandomRange(-0.45f, 0.45f);


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
        EffectManager::EmitParticle(
            footScrapeEffectComponent->GetEffectHandle(),
            footScrapeEffectComponent->GetComponentLocation(),
            { 0.0f, 0.0f, 0.0f });
    }
}

void GruxEnemy::OnAnimationNotifyBegin(const AnimationNotifyState& state)
{
    switch (state.type)
    {
    case AnimationNotifyState::Type::HitBox:
        activeHitBoxNotifyStates.push_back(&state);
        if (state.parameter == rightWeapon || state.parameter == bothWeapon)
        {
            Logger::Log(U8("右の当たり判定を開始しました"));
            prevWeaponRightRootPos = weaponRightRootComponent->GetComponentLocation();
            prevWeaponRightMidPos = weaponRightMiddleComponent->GetComponentLocation();
            prevWeaponRightTipPos = weaponRightTipComponent->GetComponentLocation();
            activeRightHitBoxRadius = (state.hitBoxRadius < 0.01f ? 0.01f : state.hitBoxRadius);
            rightHitBox = true;
        }
        if (state.parameter == leftWeapon || state.parameter == bothWeapon)
        {
            Logger::Log(U8("左の当たり判定を開始しました"));
            prevWeaponLeftRootPos = weaponLeftRootComponent->GetComponentLocation();
            prevWeaponLeftMidPos = weaponLeftMiddleComponent->GetComponentLocation();
            prevWeaponLeftTipPos = weaponLeftTipComponent->GetComponentLocation();
            activeLeftHitBoxRadius = (state.hitBoxRadius < 0.01f ? 0.01f : state.hitBoxRadius);
            leftHitBox = true;
        }
        RefreshActiveHitBoxesFromNotifyStates();
        Logger::Log(Logger::LogCategory::Gameplay,
            "[BossAttack][HitBoxBegin] attackSequenceId=" + std::to_string(currentAttackSequenceId) +
            " left=" + std::string(leftHitBox ? "true" : "false") +
            " right=" + (rightHitBox ? "true" : "false"));
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
        Logger::Log(Logger::LogCategory::Gameplay,
            "[BossAttack][DangerWindowBegin] attackSequenceId=" +
            std::to_string(currentAttackSequenceId));
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
        Logger::Log(Logger::LogCategory::Gameplay,
            "[BossAttack][HitBoxEnd] attackSequenceId=" + std::to_string(currentAttackSequenceId) +
            " left=" + std::string(leftHitBox ? "true" : "false") +
            " right=" + (rightHitBox ? "true" : "false") +
            " hitCount=" + std::to_string(currentAttackHitCount));
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
        Logger::Log(Logger::LogCategory::Gameplay,
            "[BossAttack][DangerWindowEnd] attackSequenceId=" +
            std::to_string(currentAttackSequenceId));
        isDangerWindow = false;
        activeDangerNotifyState = nullptr;
        ResetDangerArea();
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
        const auto activeWarp = std::find_if(
            animationMotionWarps.begin(), animationMotionWarps.end(),
            [&](const AnimationMotionWarp& warp)
            {
                return warp.state == &state;
            });
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
    if (event.parameter == "BeginHuskParticle" && isDeathPerform)
    {
        beginHuskParticleRequest = true;
        return;
    }

    switch (event.type)
    {
    case AnimationNotifyEvent::Type::PlaySE:
    {
        if (event.parameter != "")
        {
            std::string audioPath = "./Data/Sound/SE/" + event.parameter + ".wav";
            auto audio = CoreAudio::PlayOneShot(audioPath, event.value);
            float pitch = pitchBaseValue + GetTimeScale() * (1.0f - pitchBaseValue);
            audio->SetPitch(pitch);
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
        break;
    case AnimationNotifyEvent::Type::CameraShake:
    {
        Camera* activeCamera = GetOwnerScene()->GetActiveCamera();
        if (auto* darkCamera = dynamic_cast<DarkCameraActor*>(activeCamera))
            darkCamera->PlayCameraShakePreset(event.parameter);
        break;
    }
    }
}

void GruxEnemy::OnAnimationChanged()
{
    showLeftWeaponTrail = false;
    showRightWeaponTrail = false;

    auto controller = GetBodyAnimationController();
    if (!controller || controller->GetCurrentAnimationName() != "PrimaryAttack_LA")
    {
        DisableAttackHitBoxes();
    }
}

std::optional<BossAttackType> GruxEnemy::GetAttackTypeForAction(BossActionType actionType) const
{
    for (const BossActionData& data : combatActionData)
    {
        if (data.type == actionType)
            return data.attackType;
    }

    return std::nullopt;
}

bool GruxEnemy::PrepareAttackForSelectedAction()
{
    const std::optional<BossAttackType> attackType = GetAttackTypeForAction(selectedActionType);
    if (!attackType)
        return false;

    selectedAttackType = *attackType;
    return true;
}

const BossPositioningData* GruxEnemy::GetPositioningDataForAction(BossActionType actionType) const
{
    for (const BossPositioningData& data : combatPositioningData)
    {
        if (data.actionType == actionType)
            return &data;
    }
    return nullptr;
}

bool GruxEnemy::IsRepositionAction(const BossActionType actionType) const
{
    return actionType == BossActionType::RepositionLeft ||
        actionType == BossActionType::RepositionRight;
}

const GruxEnemy::RepositionTargetEvaluation& GruxEnemy::GetRepositionTargetEvaluation(
    const BossActionType actionType) const
{
    return actionType == BossActionType::RepositionRight
        ? rightRepositionTarget : leftRepositionTarget;
}

const char* GruxEnemy::GetRepositionFailureReason() const
{
    return leftRepositionTarget.wasClamped && rightRepositionTarget.wasClamped
        ? "BoundsBlocked" : "NoSafeDirection";
}

void GruxEnemy::EvaluateClampedPositioningTarget(
    const DirectX::XMFLOAT3& startPosition,
    const DirectX::XMFLOAT3& desiredTarget,
    RepositionTargetEvaluation& outEvaluation) const
{
    outEvaluation = {};
    outEvaluation.desiredTarget = desiredTarget;
    outEvaluation.clampedTarget = desiredTarget;
    constexpr float maximumMargin =
        (std::min)((bossRoomMaxX - bossRoomMinX) * 0.5f,
            (bossRoomMaxZ - bossRoomMinZ) * 0.5f) - 0.01f;
    const float margin = std::clamp(bossRoomSafetyMargin, 0.0f, maximumMargin);
    outEvaluation.clampedTarget.x = std::clamp(
        outEvaluation.clampedTarget.x, bossRoomMinX + margin, bossRoomMaxX - margin);
    outEvaluation.clampedTarget.z = std::clamp(
        outEvaluation.clampedTarget.z, bossRoomMinZ + margin, bossRoomMaxZ - margin);
    outEvaluation.clampedTarget.y = startPosition.y;

    const float clampX = outEvaluation.clampedTarget.x - desiredTarget.x;
    const float clampZ = outEvaluation.clampedTarget.z - desiredTarget.z;
    outEvaluation.clampDistance = std::sqrt(clampX * clampX + clampZ * clampZ);
    outEvaluation.wasClamped = outEvaluation.clampDistance > 0.0001f;
    const float availableX = outEvaluation.clampedTarget.x - startPosition.x;
    const float availableZ = outEvaluation.clampedTarget.z - startPosition.z;
    outEvaluation.availableDistance = std::sqrt(
        availableX * availableX + availableZ * availableZ);
    outEvaluation.sufficientlyMovable =
        outEvaluation.availableDistance >= minimumPositioningMoveDistance;
}

void GruxEnemy::EvaluateRepositionTargets(const BossTargetContext& context)
{
    repositionTargetsEvaluated = false;
    repositionNoSafeDirection = false;
    leftRepositionTarget = {};
    rightRepositionTarget = {};
    if (!context.valid)
    {
        repositionNoSafeDirection = true;
        repositionCompletionReason = "NoSafeDirection";
        return;
    }

    const DirectX::XMFLOAT3 bossPosition = GetPosition();
    const DirectX::XMFLOAT3 leftDirection =
    { -context.directionToPlayer.z, 0.0f, context.directionToPlayer.x };
    const DirectX::XMFLOAT3 rightDirection =
    { context.directionToPlayer.z, 0.0f, -context.directionToPlayer.x };
    const DirectX::XMFLOAT3 leftDesired =
    { bossPosition.x + leftDirection.x * combatRepositionMoveDistance,
      bossPosition.y,
      bossPosition.z + leftDirection.z * combatRepositionMoveDistance };
    const DirectX::XMFLOAT3 rightDesired =
    { bossPosition.x + rightDirection.x * combatRepositionMoveDistance,
      bossPosition.y,
      bossPosition.z + rightDirection.z * combatRepositionMoveDistance };
    EvaluateClampedPositioningTarget(
        bossPosition, leftDesired, leftRepositionTarget);
    EvaluateClampedPositioningTarget(
        bossPosition, rightDesired, rightRepositionTarget);

    if (leftRepositionTarget.sufficientlyMovable &&
        rightRepositionTarget.sufficientlyMovable &&
        leftRepositionTarget.wasClamped && rightRepositionTarget.wasClamped)
    {
        constexpr float distanceEpsilon = 0.001f;
        if (leftRepositionTarget.availableDistance + distanceEpsilon <
            rightRepositionTarget.availableDistance)
            leftRepositionTarget.sufficientlyMovable = false;
        else if (rightRepositionTarget.availableDistance + distanceEpsilon <
            leftRepositionTarget.availableDistance)
            rightRepositionTarget.sufficientlyMovable = false;
    }

    repositionTargetsEvaluated = true;
    repositionLeftAvailableDistance = leftRepositionTarget.availableDistance;
    repositionRightAvailableDistance = rightRepositionTarget.availableDistance;
    repositionNoSafeDirection = !leftRepositionTarget.sufficientlyMovable &&
        !rightRepositionTarget.sufficientlyMovable;
    if (repositionNoSafeDirection)
        repositionCompletionReason = GetRepositionFailureReason();
}
void GruxEnemy::BeginPositioning(const BossPositioningData& data)
{
    positioningAnimationMoving = false;
    positioningMoveStopTimer = 0.0f;
    positioningAnimationActualSpeed = 0.0f;
    const auto controller = GetBodyAnimationController();
    const bool isIdlePlaying = controller && controller->IsPlayAnimation() &&
        controller->GetCurrentAnimationName() == "TravelMode_Idle_0";
    if (!isIdlePlaying)
        PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);

    positioningDebugActive = true;
    activePositioningDebugData = data;
    positioningDebugTraveledDistance = 0.0f;
    positioningDebugElapsedTime = 0.0f;
    positioningDebugStuckTimer = 0.0f;
    positioningEndReason = "Running";

    currentPositioningDebug = {};
    currentPositioningDebug.valid = true;
    currentPositioningDebug.data = data;
    currentPositioningDebug.intent = activeIntent;
    currentPositioningDebug.startBossPosition = GetPosition();
    currentPositioningDebug.currentPosition = GetPosition();
    currentPositioningDebug.endReason = "Running";
    if (const BossIntentData* intentData = GetActiveIntentData())
    {
        currentPositioningDebug.preferredMin = intentData->preferredMinDistance;
        currentPositioningDebug.preferredMax = intentData->preferredMaxDistance;
    }

    fixedPositioningTargetValid = false;
    if (const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>())
    {
        currentPositioningDebug.startPlayerPosition = player->GetPosition();
        const float toPlayerX = currentPositioningDebug.startPlayerPosition.x - currentPositioningDebug.startBossPosition.x;
        const float toPlayerZ = currentPositioningDebug.startPlayerPosition.z - currentPositioningDebug.startBossPosition.z;
        const float distance = std::sqrt(toPlayerX * toPlayerX + toPlayerZ * toPlayerZ);
        currentPositioningDebug.startPlayerDistance = distance;
        currentPositioningDebug.currentPlayerDistance = distance;

        if (IsRepositionAction(data.actionType))
        {
            currentPositioningDebug.desiredTargetPosition = repositionDesiredTarget;
        }
        else
        {
            float directionX = distance > 0.0001f ? toPlayerX / distance : GetForward().x;
            float directionZ = distance > 0.0001f ? toPlayerZ / distance : GetForward().z;
            float moveDistance = 0.0f;
            if (data.actionType == BossActionType::Retreat ||
                data.direction == BossPositioningDirection::AwayFromPlayer)
            {
                directionX *= -1.0f;
                directionZ *= -1.0f;
                moveDistance = (std::max)(0.0f, data.targetDistance - distance);
            }
            else
            {
                moveDistance = (std::max)(0.0f, distance - data.targetDistance);
            }
            currentPositioningDebug.desiredTargetPosition = {
                currentPositioningDebug.startBossPosition.x + directionX * moveDistance,
                currentPositioningDebug.startBossPosition.y,
                currentPositioningDebug.startBossPosition.z + directionZ * moveDistance };
        }

        RepositionTargetEvaluation targetEvaluation{};
        EvaluateClampedPositioningTarget(currentPositioningDebug.startBossPosition,
            currentPositioningDebug.desiredTargetPosition, targetEvaluation);
        constexpr float maximumMargin =
            (std::min)((bossRoomMaxX - bossRoomMinX) * 0.5f,
                (bossRoomMaxZ - bossRoomMinZ) * 0.5f) - 0.01f;
        const float margin = std::clamp(bossRoomSafetyMargin, 0.0f, maximumMargin);
        currentPositioningDebug.safetyMargin = margin;
        currentPositioningDebug.safeMinX = bossRoomMinX + margin;
        currentPositioningDebug.safeMaxX = bossRoomMaxX - margin;
        currentPositioningDebug.safeMinZ = bossRoomMinZ + margin;
        currentPositioningDebug.safeMaxZ = bossRoomMaxZ - margin;
        currentPositioningDebug.clampedTargetPosition = targetEvaluation.clampedTarget;
        const float plannedX = currentPositioningDebug.desiredTargetPosition.x - currentPositioningDebug.startBossPosition.x;
        const float plannedZ = currentPositioningDebug.desiredTargetPosition.z - currentPositioningDebug.startBossPosition.z;
        currentPositioningDebug.plannedMoveDistance = std::sqrt(plannedX * plannedX + plannedZ * plannedZ);
        currentPositioningDebug.targetClampDistance = targetEvaluation.clampDistance;
        currentPositioningDebug.targetWasClamped = targetEvaluation.wasClamped;
        currentPositioningDebug.availableMoveDistance = targetEvaluation.availableDistance;
        currentPositioningDebug.remainingDistance = currentPositioningDebug.availableMoveDistance;

        fixedPositioningTarget = currentPositioningDebug.clampedTargetPosition;
        fixedPositioningTargetValid = IsRepositionAction(data.actionType)
            ? targetEvaluation.sufficientlyMovable : true;
    }

    if (characterMovementComponent)
    {
        characterMovementComponent->SetFixedSpeed(data.moveSpeed);
        characterMovementComponent->SetInputMagnitude(1.0f);
        currentPositioningDebug.inputMagnitude = characterMovementComponent->GetInputMagnitude();
    }
}

void GruxEnemy::UpdatePositioningAnimation(float actualSpeed, float deltaTime)
{
    positioningAnimationActualSpeed = (std::max)(0.0f, actualSpeed);
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
    positioningAnimationActualSpeed = 0.0f;
    positioningMoveStopTimer = 0.0f;
    positioningAnimationMoving = false;
    const auto controller = GetBodyAnimationController();
    const bool idlePlaying = controller && controller->IsPlayAnimation() &&
        controller->GetCurrentAnimationName() == "TravelMode_Idle_0";
    if (!idlePlaying)
        PlayBodyAnimation("TravelMode_Idle_0", true, true, 0.15f, true);
}

void GruxEnemy::UpdatePositioningMovement(const DirectX::XMFLOAT3& moveDirection,
    const DirectX::XMFLOAT3& facingDirection, float deltaTime)
{
    if (characterMovementComponent)
        characterMovementComponent->SetMoveDirection(moveDirection);
    RotateTowardsPlayer(facingDirection, GetTurnSpeed(), deltaTime, "PositioningMovement");
}

void GruxEnemy::UpdatePositioningDebug(float traveledDistance, float elapsedTime, float stuckTimer,
    float frameMovement, float actualSpeed, const DirectX::XMFLOAT3& requestedDirection)
{
    positioningDebugTraveledDistance = traveledDistance;
    positioningDebugElapsedTime = elapsedTime;
    positioningDebugStuckTimer = stuckTimer;

    if (!currentPositioningDebug.valid)
        return;
    currentPositioningDebug.currentPosition = GetPosition();
    currentPositioningDebug.traveledPathDistance = traveledDistance;
    currentPositioningDebug.elapsedTime = elapsedTime;
    currentPositioningDebug.stuckTimer = stuckTimer;
    currentPositioningDebug.frameMovement = frameMovement;
    currentPositioningDebug.actualSpeed = actualSpeed;
    currentPositioningDebug.requestedMoveDirection = requestedDirection;
    if (characterMovementComponent)
        currentPositioningDebug.inputMagnitude = characterMovementComponent->GetInputMagnitude();

    const float actualX = currentPositioningDebug.currentPosition.x - currentPositioningDebug.startBossPosition.x;
    const float actualZ = currentPositioningDebug.currentPosition.z - currentPositioningDebug.startBossPosition.z;
    currentPositioningDebug.actualMoveDistance = std::sqrt(actualX * actualX + actualZ * actualZ);
    const float remainingX = currentPositioningDebug.clampedTargetPosition.x - currentPositioningDebug.currentPosition.x;
    const float remainingZ = currentPositioningDebug.clampedTargetPosition.z - currentPositioningDebug.currentPosition.z;
    currentPositioningDebug.remainingDistance = std::sqrt(remainingX * remainingX + remainingZ * remainingZ);
    const BossTargetContext context = BuildTargetContext();
    if (context.valid)
        currentPositioningDebug.currentPlayerDistance = context.xzDistance;
}

void GruxEnemy::FinishPositioningDebug(const std::string& reason)
{
    positioningDebugActive = false;
    positioningEndReason = reason;
    if (!currentPositioningDebug.valid)
        return;

    currentPositioningDebug.currentPosition = GetPosition();
    currentPositioningDebug.endReason = reason;
    const float actualX = currentPositioningDebug.currentPosition.x - currentPositioningDebug.startBossPosition.x;
    const float actualZ = currentPositioningDebug.currentPosition.z - currentPositioningDebug.startBossPosition.z;
    currentPositioningDebug.actualMoveDistance = std::sqrt(actualX * actualX + actualZ * actualZ);
    const float remainingX = currentPositioningDebug.clampedTargetPosition.x - currentPositioningDebug.currentPosition.x;
    const float remainingZ = currentPositioningDebug.clampedTargetPosition.z - currentPositioningDebug.currentPosition.z;
    currentPositioningDebug.remainingDistance = std::sqrt(remainingX * remainingX + remainingZ * remainingZ);
    const BossTargetContext context = BuildTargetContext();
    if (context.valid)
        currentPositioningDebug.currentPlayerDistance = context.xzDistance;
    lastPositioningDebug = currentPositioningDebug;
}

bool GruxEnemy::InvalidateCloseCombatIntentForBack(const BossTargetContext& context)
{
    if (!activeIntent || *activeIntent != BossIntentType::CloseCombat ||
        !context.valid || context.region != PlayerRelativeRegion::Back)
    {
        return false;
    }

    FailActiveIntent("WrongRelativeRegion");
    return true;
}

bool GruxEnemy::IsCombatRepositionActive() const
{
    return activeIntent && *activeIntent == BossIntentType::CombatReposition;
}

void GruxEnemy::CompleteCombatReposition()
{
    if (!IsCombatRepositionActive())
        return;

    activeIntent = std::nullopt;
    suppressCombatRepositionForNextIntentSelection = true;
    activeIntentStep = BossIntentStep::Completed;
    intentPositioningAttempted = false;
    intentPositioningCompleted = true;
    intentRepositionReason = "None";
    intentLifecycleState = "Intent Completed";
    intentLifecycleTrace += " -> Reposition Completed";
    intentLifecycleReason = "RepositionCompleted";
    repositionCompletionReason = "RepositionCompleted";
}

bool GruxEnemy::TryStartIntent(BossIntentType intentType)
{
    if (activeIntent)
        return false;

    activeIntent = intentType;
    activeIntentStep = BossIntentStep::Selecting;
    intentPositioningAttempted = false;
    intentPositioningCompleted = false;
    intentPositioningAttemptCount = 0;
    intentRepositionReason = "None";
    intentLifecycleState = "Intent Started";
    intentLifecycleTrace = "Intent Started";
    intentLifecycleReason = "None";
    if (intentType == BossIntentType::CombatReposition)
    {
        const BossTargetContext context = BuildTargetContext();
        repositionReason = context.valid && context.region == PlayerRelativeRegion::Back
            ? BossRepositionReason::BackTaken : BossRepositionReason::Normal;
        repositionCompletionReason = "Running";
        selectedRepositionDirection = BossRepositionDirection::None;
    }
    return true;

}
float GruxEnemy::GetTotalIntentWeight() const
{
    float totalWeight = 0.0f;
    for (float weight : combatIntentEffectiveWeights)
        totalWeight += weight;
    return totalWeight;
}

float GruxEnemy::GetIntentWeightForDistance(
    const BossIntentData& data, BossDistanceRegion region) const
{
    switch (region)
    {
    case BossDistanceRegion::Near:
        return (std::max)(0.0f, data.nearWeight);
    case BossDistanceRegion::Middle:
        return (std::max)(0.0f, data.middleWeight);
    case BossDistanceRegion::Far:
        return (std::max)(0.0f, data.farWeight);
    }
    return 0.0f;
}

void GruxEnemy::UpdateIntentEffectiveWeights(const BossTargetContext& context)
{
    combatIntentEffectiveWeights.fill(0.0f);
    if (!context.valid)
        return;

    for (size_t i = 0; i < combatIntentData.size(); ++i)
    {
        combatIntentEffectiveWeights[i] =
            GetIntentWeightForDistance(combatIntentData[i], context.distanceRegion);
        if (context.region == PlayerRelativeRegion::Back)
        {
            if (combatIntentData[i].type == BossIntentType::CloseCombat)
                combatIntentEffectiveWeights[i] = 0.0f;
            else if (combatIntentData[i].type == BossIntentType::CombatReposition)
                combatIntentEffectiveWeights[i] =
                (std::max)(0.0f, combatRepositionBackWeight);
            else if (combatIntentData[i].type == BossIntentType::DashAttackPlan)
                combatIntentEffectiveWeights[i] =
                (std::max)(0.0f, dashAttackPlanBackWeight);
            else if (combatIntentData[i].type == BossIntentType::JumpAttackPlan)
                combatIntentEffectiveWeights[i] =
                (std::max)(0.0f, jumpAttackPlanBackWeight);
        }

        combatIntentEffectiveWeights[i] *=
            GetIntentRecentAttackPenalty(combatIntentData[i].type);
    }

    if (postAttackCombatRepositionBoostPending)
    {
        for (size_t i = 0; i < combatIntentData.size(); ++i)
        {
            if (combatIntentData[i].type != BossIntentType::CombatReposition)
                continue;
            combatIntentEffectiveWeights[i] = (std::max)(
                combatIntentEffectiveWeights[i],
                (std::max)(0.0f, postAttackCombatRepositionWeight));
            break;
        }
    }
}

bool GruxEnemy::SelectIntentByWeight()
{
    if (activeIntent)
        return false;

    if (combatRepositionIntentPending)
    {
        combatRepositionIntentPending = false;
        postAttackCombatRepositionBoostPending = false;
        postAttackCombatRepositionBoostApplied = false;
        lastSelectedIntent = BossIntentType::CombatReposition;
        return TryStartIntent(BossIntentType::CombatReposition);
    }

    const BossTargetContext context = BuildTargetContext();
    const bool applyPostAttackBoost = postAttackCombatRepositionBoostPending;
    UpdateIntentEffectiveWeights(context);
    postAttackCombatRepositionBoostApplied = applyPostAttackBoost;
    postAttackCombatRepositionBoostPending = false;
    if (suppressCombatRepositionForNextIntentSelection)
    {
        for (size_t i = 0; i < combatIntentData.size(); ++i)
        {
            if (combatIntentData[i].type == BossIntentType::CombatReposition)
            {
                combatIntentEffectiveWeights[i] = 0.0f;
                break;
            }
        }
        suppressCombatRepositionForNextIntentSelection = false;
    }
    const float totalWeight = GetTotalIntentWeight();
    lastIntentRandomTotalWeight = totalWeight;
    lastIntentRandomRangeBegin.fill(0.0f);
    lastIntentRandomRangeEnd.fill(0.0f);
    lastIntentRandomWeights = combatIntentEffectiveWeights;
    if (totalWeight <= 0.0f)
    {
        hasLastIntentRandomRoll = false;
        return false;
    }

    static std::mt19937 randomEngine{ std::random_device{}() };
    std::uniform_real_distribution<float> distribution(0.0f, totalWeight);
    const float selectionValue = distribution(randomEngine);
    lastIntentRandomRoll = selectionValue;
    hasLastIntentRandomRoll = true;

    float accumulatedWeight = 0.0f;
    for (size_t i = 0; i < combatIntentData.size(); ++i)
    {
        const float effectiveWeight = combatIntentEffectiveWeights[i];
        if (effectiveWeight <= 0.0f)
            continue;

        lastIntentRandomRangeBegin[i] = accumulatedWeight;
        accumulatedWeight += effectiveWeight;
        lastIntentRandomRangeEnd[i] = accumulatedWeight;
    }

    for (size_t i = 0; i < combatIntentData.size(); ++i)
    {
        if (combatIntentEffectiveWeights[i] <= 0.0f)
            continue;
        if (selectionValue <= lastIntentRandomRangeEnd[i])
        {
            lastSelectedIntent = combatIntentData[i].type;
            return TryStartIntent(combatIntentData[i].type);
        }
    }

    return false;
}

void GruxEnemy::ClearActiveIntent()
{
    activeIntent = std::nullopt;
    activeIntentStep = BossIntentStep::Selecting;
    intentPositioningAttempted = false;
    intentPositioningCompleted = false;
    intentPositioningAttemptCount = 0;
    intentRepositionReason = "None";
    intentLifecycleState = "None";
    intentLifecycleTrace = "None";
    intentLifecycleReason = "Cleared";
}

void GruxEnemy::MarkIntentPositioningAttempted()
{
    if (!activeIntent)
        return;

    const bool validPositioning =
        (*activeIntent == BossIntentType::CloseCombat &&
            selectedActionType == BossActionType::Approach) ||
        (*activeIntent == BossIntentType::DashAttackPlan &&
            (selectedActionType == BossActionType::Approach ||
                selectedActionType == BossActionType::Retreat)) ||
        (*activeIntent == BossIntentType::JumpAttackPlan &&
            (selectedActionType == BossActionType::Approach ||
                selectedActionType == BossActionType::Retreat)) ||
        (*activeIntent == BossIntentType::CombatReposition &&
            IsRepositionAction(selectedActionType));
    if (!validPositioning)
        return;

    intentPositioningAttempted = true;
    intentPositioningCompleted = false;
    ++intentPositioningAttemptCount;
    activeIntentStep = BossIntentStep::Positioning;
    intentLifecycleState = "Positioning";
    intentLifecycleTrace += " -> Positioning";
}

void GruxEnemy::MarkIntentPositioningCompleted()
{
    if (!activeIntent || activeIntentStep != BossIntentStep::Positioning)
        return;

    intentPositioningCompleted = true;
    activeIntentStep = BossIntentStep::AttackPending;
    intentRepositionReason = "None";
    intentLifecycleState = "Attack Pending";
    intentLifecycleTrace += " -> WorldTargetReached -> AttackPending";
}

void GruxEnemy::BeginIntentReevaluation()
{
    if (!activeIntent || !intentPositioningAttempted ||
        activeIntentStep != BossIntentStep::Positioning)
        return;

    activeIntentStep = BossIntentStep::AttackPending;
    intentLifecycleState = "Reevaluate";
    intentLifecycleTrace += " -> Reevaluate";
}

void GruxEnemy::MarkIntentAttackSelected()
{
    if (!activeIntent)
        return;

    const bool isCloseCombatAttack =
        *activeIntent == BossIntentType::CloseCombat &&
        (selectedActionType == BossActionType::AttackLA ||
            selectedActionType == BossActionType::AttackRA ||
            selectedActionType == BossActionType::FastCombo);
    const bool isDashAttackPlanAttack =
        *activeIntent == BossIntentType::DashAttackPlan &&
        (selectedActionType == BossActionType::DashAttack ||
            selectedActionType == BossActionType::ChargeAttack);
    const bool isJumpAttackPlanAttack =
        *activeIntent == BossIntentType::JumpAttackPlan &&
        selectedActionType == BossActionType::JumpAttack;
    if (!isCloseCombatAttack && !isDashAttackPlanAttack && !isJumpAttackPlanAttack)
        return;

    activeIntentStep = BossIntentStep::AttackPending;
    intentLifecycleState = "Attack Selected";
    intentLifecycleTrace += " -> Attack Selected";
}

void GruxEnemy::RequestJumpAttackCameraAssist()
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

void GruxEnemy::OnSelectedActionStartedSuccessfully()
{
    if (bossAIMode == BossAIMode::CombatAI &&
        IsCombatAttackAction(selectedActionType) &&
        (!lastStartedCombatAttack || *lastStartedCombatAttack != selectedActionType))
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

    if (!activeIntent)
        return;

    const bool closeCombatGoalReached =
        *activeIntent == BossIntentType::CloseCombat &&
        (selectedActionType == BossActionType::AttackLA ||
            selectedActionType == BossActionType::AttackRA ||
            selectedActionType == BossActionType::FastCombo);
    const bool dashAttackGoalReached =
        *activeIntent == BossIntentType::DashAttackPlan &&
        (selectedActionType == BossActionType::DashAttack ||
            selectedActionType == BossActionType::ChargeAttack);
    const bool jumpAttackGoalReached =
        *activeIntent == BossIntentType::JumpAttackPlan &&
        selectedActionType == BossActionType::JumpAttack;
    if (!closeCombatGoalReached && !dashAttackGoalReached && !jumpAttackGoalReached)
        return;

    activeIntent = std::nullopt;
    activeIntentStep = BossIntentStep::Completed;
    intentPositioningAttempted = false;
    intentPositioningCompleted = true;
    intentRepositionReason = "None";
    intentLifecycleState = "Intent Completed";
    intentLifecycleTrace += " -> Intent Completed";
    intentLifecycleReason = "AttackStarted";
}

void GruxEnemy::OnSelectedAttackCompletedSuccessfully()
{
    if (bossAIMode != BossAIMode::CombatAI)
        return;

    postAttackCombatRepositionBoostPending = true;
    postAttackCombatRepositionBoostApplied = false;
}

void GruxEnemy::OnSelectedActionStartFailed()
{
    FailActiveIntent("AttackStartFailed");
}

void GruxEnemy::FailActiveIntent(const char* reason)
{
    if (!activeIntent)
        return;

    activeIntent = std::nullopt;
    activeIntentStep = BossIntentStep::Failed;
    intentPositioningAttempted = false;
    intentPositioningCompleted = false;
    intentRepositionReason = "None";
    intentLifecycleState = "Intent Failed";
    intentLifecycleTrace += " -> Intent Failed";
    intentLifecycleReason = reason ? reason : "Unknown";
}

void GruxEnemy::StartSelectedActionCooldown()
{
    if (bossAIMode != BossAIMode::CombatAI)
        return;

    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        if (combatActionData[i].type != selectedActionType)
            continue;

        combatActionCooldownRemaining[i] =
            (std::max)(0.0f, combatActionData[i].cooldownDuration);
        return;
    }
}

// 攻撃開始時に始める処理
bool GruxEnemy::SelectAttackForCurrentMode()
{
    if (bossAIMode == BossAIMode::DebugFixedAttack)
    {
        selectedAttackType = debugFixedAttackType;
        return true;
    }
    // プレイヤーまでの距離を取得する
    currentCombatPlayerDistance = GetDistanceToPlayer();

    // プレイヤーまでの距離から、候補のアクションフラグを更新する
    const BossTargetContext context = BuildTargetContext();
    UpdateActionCandidateFlags(context);

    // 候補のアクションフラグから、候補の攻撃の確率の重みを更新する
    UpdateActionEffectiveWeights();

    lastCombatSelectionDistance = currentCombatPlayerDistance;
    combatEffectiveWeights.fill(0.0f);
    combatCandidateFlags.fill(false);

    float totalWeight = 0.0f;
    for (size_t i = 0; i < combatAttackData.size(); ++i)
    {
        const auto& attack = combatAttackData[i];
        const bool inRange =
            attack.minDistance <= currentCombatPlayerDistance &&
            currentCombatPlayerDistance <= attack.maxDistance;
        combatCandidateFlags[i] = inRange;
        if (!inRange || attack.weight <= 0.0f)
            continue;

        float effectiveWeight = attack.weight;
        if (hasLastAttack && attack.type == lastAttackType)
            effectiveWeight *= repeatWeightScale;
        combatEffectiveWeights[i] = effectiveWeight;
        totalWeight += effectiveWeight;
    }

    if (totalWeight <= 0.0f)
        return false;

    static std::mt19937 randomEngine{ std::random_device{}() };
    std::uniform_real_distribution<float> distribution(0.0f, totalWeight);
    const float selectionValue = distribution(randomEngine);
    float accumulatedWeight = 0.0f;
    size_t selectedIndex = combatAttackData.size();
    for (size_t i = 0; i < combatAttackData.size(); ++i)
    {
        if (combatEffectiveWeights[i] <= 0.0f)
            continue;
        accumulatedWeight += combatEffectiveWeights[i];
        if (selectionValue <= accumulatedWeight)
        {
            selectedIndex = i;
            break;
        }
    }

    if (selectedIndex >= combatAttackData.size())
    {
        for (size_t i = combatAttackData.size(); i-- > 0;)
        {
            if (combatEffectiveWeights[i] > 0.0f)
            {
                selectedIndex = i;
                break;
            }
        }
    }
    if (selectedIndex >= combatAttackData.size())
        return false;

    selectedAttackType = combatAttackData[selectedIndex].type;
    lastAttackType = selectedAttackType;
    hasLastAttack = true;
    return true;
}
int GruxEnemy::GetAttackStageCount(BossAttackType type) const
{
    if (type == BossAttackType::FastCombo)
        return 3;
    if (type == BossAttackType::DashAttack)
        return 3;
    if (type == BossAttackType::JumpAttack)
        return 2;
    if (type == BossAttackType::LongRangeAttack)
        return 0;
    return 1;
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

float GruxEnemy::GetRecoveryDurationForCurrentAttack() const
{
    for (const BossAttackData& attackData : combatAttackData)
    {
        if (attackData.type == selectedAttackType)
            return (std::max)(0.0f, attackData.recoveryDuration);
    }

    return (std::max)(0.0f, recoveryDuration);
}

void GruxEnemy::SetNextRecoveryDuration(float duration, const char* source)
{
    nextRecoveryDuration = (std::max)(0.0f, duration);
    nextRecoverySource = source ? source : "Custom";
}

void GruxEnemy::SetPendingChargeRecoveryResult(ChargeAttackEndReason reason)
{
    pendingChargeRecoveryResult = reason;
}

ChargeAttackEndReason GruxEnemy::ConsumePendingChargeRecoveryResult()
{
    const ChargeAttackEndReason result = pendingChargeRecoveryResult;
    pendingChargeRecoveryResult = ChargeAttackEndReason::None;
    return result;
}

void GruxEnemy::RequestCombatRepositionIntent()
{
    combatRepositionIntentPending = true;
    postAttackCombatRepositionBoostPending = false;
    postAttackCombatRepositionBoostApplied = false;
}

float GruxEnemy::ConsumeNextRecoveryDuration()
{
    const bool hasOverride = nextRecoveryDuration.has_value();
    const float duration = hasOverride
        ? *nextRecoveryDuration
        : GetRecoveryDurationForCurrentAttack();
    recoverySourceDebug = hasOverride ? nextRecoverySource : "Default";
    currentRecoveryDurationDebug = duration;
    recoveryElapsedDebug = 0.0f;

    nextRecoveryDuration.reset();
    nextRecoverySource = "Default";
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

void GruxEnemy::BeginAdditionalAttackStage()
{
    // A FastCombo is one attack sequence, but each stage may damage the player once.
    hitActors.clear();
}

void GruxEnemy::PrepareJumpAttackMotionWarpOverride()
{
    ClearJumpAttackMotionWarpOverride();

    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player)
    {
        currentJumpPlayerDistance = 0.0f;
        calculatedJumpDistance = 0.0f;
        jumpMotionWarpDirection = { 0.0f, 0.0f, 0.0f };
        jumpMotionWarpOverrideActive = true;
        return;
    }

    const DirectX::XMFLOAT3 bossPosition = GetPosition();
    jumpAttackStartPlayerPosition = player->GetPosition();
    const float dx = jumpAttackStartPlayerPosition.x - bossPosition.x;
    const float dz = jumpAttackStartPlayerPosition.z - bossPosition.z;
    currentJumpPlayerDistance = std::sqrt(dx * dx + dz * dz);

    const float desiredMoveDistance = currentJumpPlayerDistance - desiredAttackDistance;
    calculatedJumpDistance = std::clamp(desiredMoveDistance, 0.0f, maxJumpDistance);

    if (currentJumpPlayerDistance > FLT_EPSILON)
    {
        const float inverseDistance = 1.0f / currentJumpPlayerDistance;
        jumpMotionWarpDirection = { dx * inverseDistance, 0.0f, dz * inverseDistance };
        if (rotationComponent)
            RecordRotationDebugSource("JumpAttackDirectionLock", jumpMotionWarpDirection, 0.0f);
        rotationComponent->SetDirectionImmediate(jumpMotionWarpDirection);
    }
    else
    {
        jumpMotionWarpDirection = { 0.0f, 0.0f, 0.0f };
    }
    jumpMotionWarpOverrideActive = true;
}

void GruxEnemy::ClearJumpAttackMotionWarpOverride()
{
    jumpMotionWarpOverrideActive = false;
    jumpMotionWarpDirection = { 0.0f, 0.0f, 1.0f };
    animationMotionWarps.clear();
}

bool GruxEnemy::BeginDashAttackMovement()
{
    StopDashAttackMovement();

    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player)
        return false;

    const DirectX::XMFLOAT3 bossPosition = GetPosition();
    const DirectX::XMFLOAT3 playerPosition = player->GetPosition();
    const float dx = playerPosition.x - bossPosition.x;
    const float dz = playerPosition.z - bossPosition.z;
    currentDashAttackPlayerDistance = std::sqrt(dx * dx + dz * dz);
    if (currentDashAttackPlayerDistance <= FLT_EPSILON)
        return false;

    const float inverseDistance = 1.0f / currentDashAttackPlayerDistance;
    dashAttackDirection = { dx * inverseDistance, 0.0f, dz * inverseDistance };
    calculatedDashAttackDistance = std::clamp(
        currentDashAttackPlayerDistance - desiredDashAttackDistance,
        minDashAttackDistance, maxDashAttackDistance);
    dashAttackStartPosition = bossPosition;
    dashTargetPosition =
    {
        bossPosition.x + dashAttackDirection.x * calculatedDashAttackDistance,
        bossPosition.y,
        bossPosition.z + dashAttackDirection.z * calculatedDashAttackDistance
    };
    dashAttackElapsedTime = 0.0f;
    dashAttackMovementActive = true;

    if (rotationComponent)
        RecordRotationDebugSource("DashAttackDirectionLock", dashAttackDirection, 0.0f);
    rotationComponent->SetDirectionImmediate(dashAttackDirection);
    if (characterMovementComponent)
    {
        characterMovementComponent->SetFixedSpeed(dashAttackSpeed);
        characterMovementComponent->SetInputMagnitude(1.0f);
        characterMovementComponent->SetMoveDirection(dashAttackDirection);
    }
    return true;
}

bool GruxEnemy::UpdateDashAttackMovement(float deltaTime)
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
    const DirectX::XMFLOAT3 directionToTarget = { dx * inverseDistance, 0.0f, dz * inverseDistance };
    if (characterMovementComponent)
        characterMovementComponent->SetMoveDirection(directionToTarget);
    if (rotationComponent)
        RecordRotationDebugSource("DashMovementTarget", directionToTarget, 0.0f);
    rotationComponent->SetDirectionImmediate(directionToTarget);
    return false;
}

void GruxEnemy::StopDashAttackMovement()
{
    dashAttackMovementActive = false;
    dashAttackElapsedTime = 0.0f;
    StopAIMovement();
}
bool GruxEnemy::BeginChargeAttackMovement()
{
    StopChargeAttackMovement();
    pendingChargeRecoveryResult = ChargeAttackEndReason::None;

    const BossTargetContext context = BuildTargetContext();
    if (!context.valid)
    {
        chargeEndReasonDebug = ChargeAttackEndReason::SafetyTimeout;
        Logger::Warning(Logger::LogCategory::Gameplay,
            "[BossCharge][StartFailed] reason=InvalidTarget");
        return false;
    }

    chargeDirection = context.directionToPlayer;
    chargeDirection.y = 0.0f;
    const float directionLength = std::sqrt(
        chargeDirection.x * chargeDirection.x +
        chargeDirection.z * chargeDirection.z);
    if (directionLength <= FLT_EPSILON)
    {
        chargeEndReasonDebug = ChargeAttackEndReason::SafetyTimeout;
        Logger::Warning(Logger::LogCategory::Gameplay,
            "[BossCharge][StartFailed] reason=ZeroDirection");
        return false;
    }

    chargeDirection.x /= directionLength;
    chargeDirection.z /= directionLength;
    chargeElapsedTime = 0.0f;
    chargePlayerCastHitDebug = false;
    chargePlayerHitDistanceDebug = 0.0f;
    chargePlayerHitActorDebug = "None";
    chargeSelectedHitDebug = "None";
    chargeWallCastHitDebug = false;
    chargeWallFacingAmountDebug = 0.0f;
    chargeWallHitNormalDebug = {};
    chargeWallHitDistanceDebug = 0.0f;
    chargeEndReasonDebug = ChargeAttackEndReason::None;
    chargeMovementActive = true;
    chargeDangerWindowActive = true;
    chargeJustDodgeSuccessDebug = false;

    PlayBodyAnimation("Stampede_0", true, true, 0.1f, true);
    if (rotationComponent)
    {
        RecordRotationDebugSource("ChargeDirectionLock", chargeDirection, 0.0f);
        rotationComponent->SetDirectionImmediate(chargeDirection);
    }
    if (characterMovementComponent)
    {
        characterMovementComponent->SetFixedSpeed(chargeSpeed);
        characterMovementComponent->SetInputMagnitude(1.0f);
        characterMovementComponent->SetMoveDirection(chargeDirection);
    }
    return true;
}

ChargeAttackEndReason GruxEnemy::UpdateChargeAttackMovement(float deltaTime)
{
    if (!chargeMovementActive)
        return chargeEndReasonDebug;

    chargeElapsedTime += (std::max)(0.0f, deltaTime);
    chargePlayerCastHitDebug = false;
    chargePlayerHitDistanceDebug = 0.0f;
    chargePlayerHitActorDebug = "None";
    chargeSelectedHitDebug = "None";
    chargeWallCastHitDebug = false;
    chargeWallFacingAmountDebug = 0.0f;
    chargeWallHitNormalDebug = {};
    chargeWallHitDistanceDebug = 0.0f;

    const float frameMoveDistance = chargeSpeed * (std::max)(0.0f, deltaTime);
    const float castDistance = frameMoveDistance + chargeWallCastSafetyMargin;
    const float bodyCastRadius = (std::max)(0.05f, radius * chargeWallCastRadiusScale);
    DirectX::XMFLOAT3 castOrigin = GetPosition();
    castOrigin.y += (std::max)(bodyCastRadius + 0.05f, height * 0.5f);

    HitResultWithActor playerHit{};
    const bool playerCastHit = Physics::Instance().SphereCast(
        castOrigin, chargeDirection, castDistance, bodyCastRadius, playerHit,
        CollisionHelper::ToBit(CollisionLayer::Player));
    chargePlayerCastHitDebug = playerCastHit;
    if (playerCastHit)
    {
        chargePlayerHitDistanceDebug = playerHit.distance;
        if (playerHit.actor)
            chargePlayerHitActorDebug = playerHit.actor->GetName();
    }

    HitResultWithActor wallHit{};
    const uint32_t wallMask = CollisionHelper::MakeMask({
        CollisionLayer::WorldStatic,
        CollisionLayer::WorldProps,
        CollisionLayer::WorldPropsNoRaycast,
        });
    const bool wallCastHit = Physics::Instance().SphereCast(
        castOrigin, chargeDirection, castDistance, bodyCastRadius, wallHit, wallMask);
    bool wallCandidate = false;
    if (wallCastHit)
    {
        chargeWallCastHitDebug = true;
        chargeWallHitNormalDebug = wallHit.normal;
        chargeWallHitDistanceDebug = wallHit.distance;
        chargeWallFacingAmountDebug = -(
            chargeDirection.x * wallHit.normal.x +
            chargeDirection.z * wallHit.normal.z);

        const bool facesCharge =
            chargeWallFacingAmountDebug >= chargeWallFacingThreshold;
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

    if (playerIsFirst)
    {
        if (chargeDangerWindowActive && TryStartJustDodgeSuccess(hitPlayer))
        {
            chargeJustDodgeSuccessDebug = true;
            chargeSelectedHitDebug = "JustDodge";
            chargeEndReasonDebug = ChargeAttackEndReason::JustDodge;
            Logger::Log(Logger::LogCategory::Gameplay,
                "[BossCharge][End] reason=JustDodge distance=" +
                std::to_string(playerHit.distance));
            StopChargeAttackMovement();
            return chargeEndReasonDebug;
        }

        if (hitPlayer->TryTakeDamage(GetDamageForCurrentAttack(), GetPosition()))
        {
            DirectX::XMFLOAT3 knockBackDirection =
                MathHelper::Subtract(hitPlayer->GetPosition(), GetPosition());
            hitPlayer->StartKnockBack(knockBackDirection);

            hitActors.emplace(hitPlayer);
            ++currentAttackHitCount;
            chargeSelectedHitDebug = "PlayerHit";
            chargeEndReasonDebug = ChargeAttackEndReason::PlayerHit;
            Logger::Log(Logger::LogCategory::Gameplay,
                "[BossCharge][End] reason=PlayerHit distance=" +
                std::to_string(playerHit.distance) +
                " damage=" + std::to_string(GetDamageForCurrentAttack()));
            StopChargeAttackMovement();
            return chargeEndReasonDebug;
        }

        chargeSelectedHitDebug = "PlayerDamageRejected";
    }

    if (wallCandidate)
    {
        chargeSelectedHitDebug = "WallHit";
        chargeEndReasonDebug = ChargeAttackEndReason::WallHit;
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
        Logger::Log(Logger::LogCategory::Gameplay,
            "[BossCharge][End] reason=WallHit distance=" +
            std::to_string(wallHit.distance));
        StopChargeAttackMovement();
        return chargeEndReasonDebug;
    }

    if (chargeElapsedTime >= chargeSafetyTimeout)
    {
        chargeSelectedHitDebug = "SafetyTimeout";
        chargeEndReasonDebug = ChargeAttackEndReason::SafetyTimeout;
        const std::string timeoutMessage =
            "[BossCharge][End] reason=SafetyTimeout elapsed=" +
            std::to_string(chargeElapsedTime);
        Logger::Warning(Logger::LogCategory::Gameplay, timeoutMessage.c_str());
        StopChargeAttackMovement();
        return chargeEndReasonDebug;
    }

    if (characterMovementComponent)
    {
        characterMovementComponent->SetFixedSpeed(chargeSpeed);
        characterMovementComponent->SetInputMagnitude(1.0f);
        characterMovementComponent->SetMoveDirection(chargeDirection);
    }
    return ChargeAttackEndReason::None;
}

void GruxEnemy::StopChargeAttackMovement()
{
    chargeMovementActive = false;
    chargeDangerWindowActive = false;
    StopAIMovement();
}

void GruxEnemy::StartAttack()
{
    ClearJumpAttackMotionWarpOverride();
    StopDashAttackMovement();
#if 0
    DirectX::XMFLOAT3 size = { 1.0f,4.0f,1.0f };
    leftWeaponCollisionComp->ResizeCapsule(size.x, size.y);
#endif // 0
    ++currentAttackSequenceId;
    ResetJustDodgeRecords("start_attack");
    Logger::Log(Logger::LogCategory::Gameplay,
        "[BossAttack][Start] attackSequenceId=" + std::to_string(currentAttackSequenceId) +
        " previousHitActors=" + std::to_string(hitActors.size()));
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
    activeHitBoxNotifyStates.clear();
    transitionWindow = false;
    isDangerWindow = false;
    activeDangerNotifyState = nullptr;
    ResetDangerArea();
    Logger::Log(Logger::LogCategory::Gameplay,
        "[BossAttack][HitBoxesDisabled] attackSequenceId=" + std::to_string(currentAttackSequenceId) +
        " hitCount=" + std::to_string(currentAttackHitCount));
}

void GruxEnemy::ResetJustDodgeRecords(const char* reason)
{
    Logger::Log(Logger::LogCategory::Gameplay,
        "[BossAttack][JustDodgeReset] attackSequenceId=" +
        std::to_string(currentAttackSequenceId) +
        " count=" + std::to_string(justDodgedActors.size()) +
        " reason=" + (reason ? reason : "unknown"));
    justDodgedActors.clear();
}

bool GruxEnemy::HasJustDodgedAttack(const Actor* actor) const
{
    return actor && justDodgedActors.contains(actor);
}

bool GruxEnemy::TryStartJustDodgeSuccess(Player* player)
{
    if (!player || !player->GetJustDodgeWindow() || HasJustDodgedAttack(player))
        return false;

    justDodgedActors.insert(player);
    player->StartJustDodgeSuccess(
        std::dynamic_pointer_cast<Enemy>(shared_from_this()));
    Logger::Log(Logger::LogCategory::Gameplay,
        "[BossAttack][JustDodgeSuccess] attackSequenceId=" +
        std::to_string(currentAttackSequenceId));
    return true;
}

void GruxEnemy::RefreshActiveHitBoxesFromNotifyStates()
{
    leftHitBox = false;
    rightHitBox = false;
    activeLeftHitBoxRadius = hitWeaponRadius;
    activeRightHitBoxRadius = hitWeaponRadius;

    for (const AnimationNotifyState* activeState : activeHitBoxNotifyStates)
    {
        if (!activeState)
            continue;
        const float radius = (activeState->hitBoxRadius < 0.01f ? 0.01f : activeState->hitBoxRadius);
        if (activeState->parameter == leftWeapon || activeState->parameter == bothWeapon)
        {
            leftHitBox = true;
            activeLeftHitBoxRadius = radius;
        }
        if (activeState->parameter == rightWeapon || activeState->parameter == bothWeapon)
        {
            rightHitBox = true;
            activeRightHitBoxRadius = radius;
        }
    }
}
void GruxEnemy::CaptureInitialDangerObbSettings()
{
    initialDangerObbSettings.clear();
    savedDangerObbSettings.clear();
    const auto controller = GetBodyAnimationController();
    if (!controller)
        return;
    for (const size_t clip : controller->GetAnimationAssetOrderForRuntimeTuning())
    {
        const auto* asset = controller->GetNotifyAssetForRuntimeTuning(clip);
        if (!asset)
            continue;
        const auto& states = asset->notifyTrack.states;
        for (size_t stateIndex = 0; stateIndex < states.size(); ++stateIndex)
        {
            const auto& state = states[stateIndex];
            if (state.type != AnimationNotifyState::Type::DangerWindow)
                continue;
            const uint64_t key = (static_cast<uint64_t>(clip) << 32) |
                static_cast<uint64_t>(stateIndex);
            initialDangerObbSettings[key] = { state.justDodgeAreaOffset, state.justDodgeAreaSize };
            savedDangerObbSettings[key] = initialDangerObbSettings[key];
            if (dangerObbSelectedClip == static_cast<size_t>(-1))
            {
                dangerObbSelectedClip = clip;
                dangerObbSelectedStateIndex = stateIndex;
            }
        }
    }
}

AnimationNotifyState* GruxEnemy::GetSelectedDangerNotifyState()
{
    const auto controller = GetBodyAnimationController();
    if (!controller)
        return nullptr;
    auto* asset = controller->GetNotifyAssetForRuntimeTuning(dangerObbSelectedClip);
    if (!asset || dangerObbSelectedStateIndex >= asset->notifyTrack.states.size())
        return nullptr;
    AnimationNotifyState& state = asset->notifyTrack.states[dangerObbSelectedStateIndex];
    return state.type == AnimationNotifyState::Type::DangerWindow ? &state : nullptr;
}

const AnimationNotifyState* GruxEnemy::GetSelectedDangerNotifyState() const
{
    return const_cast<GruxEnemy*>(this)->GetSelectedDangerNotifyState();
}

bool GruxEnemy::GetPlayerDangerOverlapForDebug(const DangerArea& area) const
{
    const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player)
        return false;
    DirectX::XMFLOAT3 center = player->GetPosition();
    float radius = 0.0f;
    float height = 0.0f;
    if (const auto capsule = std::dynamic_pointer_cast<CapsuleComponent>(
        player->FindComponentByName("capsuleComponent")))
    {
        center = capsule->GetComponentLocation();
        radius = capsule->GetRadius();
        height = capsule->GetHeight();
    }
    return area.IntersectsPlayerCapsule(center, radius, height).overlap;
}

void GruxEnemy::RefreshActiveDangerAreaFromNotify()
{
    if (!isDangerWindow || !activeDangerNotifyState)
        return;
    justDodgeAreaOffset = activeDangerNotifyState->justDodgeAreaOffset;
    justDodgeAreaSize = activeDangerNotifyState->justDodgeAreaSize;
    justDodgeAreaSize.x = (std::max)(0.0f, justDodgeAreaSize.x);
    justDodgeAreaSize.y = (std::max)(0.0f, justDodgeAreaSize.y);
    justDodgeAreaSize.z = (std::max)(0.0f, justDodgeAreaSize.z);
    dangerArea = BuildDangerArea(GetPosition(), GetRight(), GetUp(), GetForward(),
        justDodgeAreaOffset, justDodgeAreaSize);
}

void GruxEnemy::DrawDangerObbWorldDebug()
{
    if (!dangerObbWorldDebug)
        return;
    const AnimationNotifyState* state = GetSelectedDangerNotifyState();
    if (!state)
        return;
    const DangerArea selectedArea = BuildDangerArea(GetPosition(), GetRight(), GetUp(), GetForward(),
        state->justDodgeAreaOffset, state->justDodgeAreaSize);
    const bool inside = GetPlayerDangerOverlapForDebug(selectedArea);
    const DirectX::XMFLOAT4 color = inside
        ? DirectX::XMFLOAT4{ 1.0f, 0.1f, 0.1f, 1.0f }
    : DirectX::XMFLOAT4{ 1.0f, 0.8f, 0.15f, 1.0f };
    DebugRender::DrawBox(selectedArea.WorldTransform(), selectedArea.size, color, 0.0f, true);
    DebugRender::DrawSphere(selectedArea.center, 0.1f, color, 0.0f, true);
}

void GruxEnemy::ResetDangerArea()
{
    dangerArea = {};
}

// ボスの名前の演出を開始する
void GruxEnemy::StartGruxNamePerform(float duration, float start, float end)
{
    // 名前テクスチャのalphaが徐々に濃くなる処理
    {
        TestEasingHandler handler;

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

bool GruxEnemy::IsFacingPlayerForAttack(
    const BossTargetContext& context) const
{
    return context.valid &&
        context.absoluteAngleDegrees <= attackFacingAngle;
}

namespace
{
    float GetAttackFacingTolerance(const BossAttackType attackType)
    {
        switch (attackType)
        {
        case BossAttackType::PrimaryAttackLA:
        case BossAttackType::PrimaryAttackRA:
        case BossAttackType::FastCombo:
            return 45.0f;
        case BossAttackType::DashAttack:
        case BossAttackType::ChargeAttack:
        case BossAttackType::JumpAttack:
            return 90.0f;
        default:
            return 0.0f;
        }
    }
}

bool GruxEnemy::PreparePendingAttackFacing(const BossTargetContext& context)
{
    ClearPendingAttackFacing();
    attackFacingEvaluationValid = context.valid;
    attackFacingEvaluationTolerance = GetAttackFacingTolerance(selectedAttackType);
    attackFacingEvaluationAngle = context.valid ? context.absoluteAngleDegrees : 0.0f;
    const bool attackFacingRequired = context.valid &&
        attackFacingEvaluationTolerance > 0.0f &&
        attackFacingEvaluationAngle > attackFacingEvaluationTolerance;
    const bool isCloseCombat = IsCloseCombatAttackType(selectedAttackType);
    currentFacingAngleBeforeReady = isCloseCombat
        ? attackFacingEvaluationAngle
        : 0.0f;
    const bool readyFacingRequired = context.valid && isCloseCombat &&
        attackFacingEvaluationAngle > closeCombatReadyFacingAngle;
    attackFacingEvaluationRequired = attackFacingRequired || readyFacingRequired;

    if (!attackFacingEvaluationRequired)
        return false;

    pendingAttackActionValid = true;
    pendingAttackFacingValid = true;
    pendingAttackFacingDirection = context.directionToPlayer;
    pendingAttackFacingCompleteAngle = isCloseCombat
        ? closeCombatReadyFacingAngle
        : turnCompleteAngle;
    return true;
}

float GruxEnemy::GetPendingAttackFacingAngle() const
{
    if (!pendingAttackFacingValid)
        return 0.0f;

    DirectX::XMVECTOR forwardVector = DirectX::XMVector3Rotate(
        DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
        DirectX::XMLoadFloat4(&GetQuaternionRotation()));
    DirectX::XMFLOAT3 forward{};
    DirectX::XMStoreFloat3(&forward, forwardVector);
    forward.y = 0.0f;
    const float forwardLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);
    if (forwardLength <= FLT_EPSILON)
        return 180.0f;
    forward.x /= forwardLength;
    forward.z /= forwardLength;

    const float dot = std::clamp(
        forward.x * pendingAttackFacingDirection.x +
        forward.z * pendingAttackFacingDirection.z,
        -1.0f, 1.0f);
    return DirectX::XMConvertToDegrees(std::acos(dot));
}

bool GruxEnemy::IsCloseCombatAttackType(const BossAttackType attackType) const
{
    return attackType == BossAttackType::PrimaryAttackLA ||
        attackType == BossAttackType::PrimaryAttackRA ||
        attackType == BossAttackType::FastCombo;
}

void GruxEnemy::BeginAttackReadyDebug()
{
    attackReadyActive = true;
    attackReadyDebugTimer = 0.0f;
    attackReadyDebugType = selectedAttackType;
    attackReadySEFired = false;
}

void GruxEnemy::UpdateAttackReadyDebug(const float elapsedTime)
{
    attackReadyDebugTimer = elapsedTime;
}

void GruxEnemy::EndAttackReadyDebug()
{
    attackReadyActive = false;
}

bool GruxEnemy::PlayAttackReadySE()
{
    attackReadySEFired = false;
    if (attackReadySEName.empty())
        return false;

    const std::string audioPath =
        "./Data/Sound/SE/" + attackReadySEName + ".wav";
    auto audio = CoreAudio::PlayOneShot(audioPath, attackReadySEVolume);
    if (!audio)
        return false;

    const float pitch =
        pitchBaseValue + GetTimeScale() * (1.0f - pitchBaseValue);
    audio->SetPitch(pitch);
    attackReadySEFired = true;
    return true;
}

bool GruxEnemy::ResumeSelectedAttackAfterTurn()
{
    if (!pendingAttackActionValid || !pendingAttackFacingValid)
        return false;

    const std::optional<BossAttackType> mappedAttack =
        GetAttackTypeForAction(selectedActionType);
    if (!mappedAttack || *mappedAttack != selectedAttackType)
    {
        ClearPendingAttackFacing();
        return false;
    }

    lastResumedAttackType = selectedAttackType;
    lastResumedAttackValid = true;
    ClearPendingAttackFacing();
    if (IsCloseCombatAttackType(selectedAttackType))
    {
        attackReadyReason = AttackReadyReason::AfterTurn;
        stateMachine_->ChangeState("EnemyAttackReadyState");
    }
    else
    {
        stateMachine_->ChangeState(
            selectedAttackType == BossAttackType::ChargeAttack
            ? "EnemyChargeAttackState"
            : "EnemyAttackState");
    }
    return true;
}

void GruxEnemy::ClearPendingAttackFacing()
{
    pendingAttackActionValid = false;
    pendingAttackFacingValid = false;
    pendingAttackFacingCompleteAngle = turnCompleteAngle;
}

void GruxEnemy::StopAIMovement()
{
    if (!characterMovementComponent)
        return;
    characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->SetInputMagnitude(0.0f);
    characterMovementComponent->ResetFixedSpeed();
}

void GruxEnemy::BeginRotationDebugFrame()
{
    rotationDebugSourceCount = 0;
    rotationDebugRequestedTurnSpeed = 0.0f;
    rotationDebugTurnTargetValid = false;
}

void GruxEnemy::FinishRotationDebugFrame()
{
    rotationDebugCurrentYaw = GetEulerRotation().y;
    if (!rotationDebugYawInitialized)
    {
        rotationDebugPreviousYaw = rotationDebugCurrentYaw;
        rotationDebugYawInitialized = true;
    }
    rotationDebugActualYawDelta = std::remainder(
        rotationDebugCurrentYaw - rotationDebugPreviousYaw, 360.0f);
    rotationDebugPreviousYaw = rotationDebugCurrentYaw;

    const BossTargetContext context = BuildTargetContext();
    if (context.valid)
        rotationDebugPlayerYaw = DirectX::XMConvertToDegrees(
            std::atan2f(context.directionToPlayer.x, context.directionToPlayer.z));
}

void GruxEnemy::RecordRotationDebugSource(const char* source,
    const DirectX::XMFLOAT3& targetDirection, float requestedTurnSpeed)
{
    const float directionLengthSq = targetDirection.x * targetDirection.x +
        targetDirection.z * targetDirection.z;
    const float targetYaw = directionLengthSq > FLT_EPSILON
        ? DirectX::XMConvertToDegrees(std::atan2f(targetDirection.x, targetDirection.z))
        : rotationDebugCurrentYaw;
    rotationDebugRequestedTurnSpeed = requestedTurnSpeed;
    if (rotationDebugSourceCount < static_cast<int>(rotationDebugSources.size()))
    {
        RotationSourceDebug& entry = rotationDebugSources[rotationDebugSourceCount++];
        entry.source = source ? source : "Unknown";
        entry.targetYaw = targetYaw;
        entry.requestedTurnSpeed = requestedTurnSpeed;
    }
    if (source && std::strcmp(source, "TurnState") == 0)
    {
        rotationDebugTurnTargetDirection = targetDirection;
        rotationDebugTurnTargetValid = directionLengthSq > FLT_EPSILON;
    }
}

void GruxEnemy::BeginTurnRotationDebug(const char* fromState)
{
    const BossTargetContext context = BuildTargetContext();
    activeTurnDebugFromState = fromState ? fromState : "Unknown";
    activeTurnDebugStartAngle = context.valid ? context.absoluteAngleDegrees : 0.0f;
    activeTurnDebugTargetYaw = context.valid
        ? DirectX::XMConvertToDegrees(std::atan2f(
            context.directionToPlayer.x, context.directionToPlayer.z))
        : GetEulerRotation().y;
}

void GruxEnemy::EndTurnRotationDebug(float duration)
{
    const BossTargetContext context = BuildTargetContext();
    lastTurnDebug.valid = true;
    lastTurnDebug.fromState = activeTurnDebugFromState;
    lastTurnDebug.startAngle = activeTurnDebugStartAngle;
    lastTurnDebug.startTargetYaw = activeTurnDebugTargetYaw;
    lastTurnDebug.endAngle = context.valid ? context.absoluteAngleDegrees : 0.0f;
    lastTurnDebug.duration = (std::max)(0.0f, duration);
}

bool GruxEnemy::RotateTowardsPlayer(
    const DirectX::XMFLOAT3& direction,
    const float degreesPerSecond,
    const float deltaTime,
    const char* debugSource)
{
    RecordRotationDebugSource(debugSource, direction, degreesPerSecond);
    return rotationComponent && rotationComponent->RotateTowardsDirection(
        direction, degreesPerSecond, deltaTime);
}


// プレイヤーとの距離を取得する関数
float GruxEnemy::GetDistanceToPlayer()
{
    auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player) return 9999.0f;

    auto p = player->GetPosition();
    auto b = GetPosition();

    float dx = p.x - b.x;
    float dz = p.z - b.z;

    return sqrtf(dx * dx + dz * dz);
}

// プレイヤーとの距離に応じた距離領域を取得する関数
BossDistanceRegion GruxEnemy::GetDistanceRegion(float distance) const
{
    if (distance <= nearDistanceThreshold)
        return BossDistanceRegion::Near;
    if (distance <= middleDistanceThreshold)
        return BossDistanceRegion::Middle;
    return BossDistanceRegion::Far;
}

const BossIntentData* GruxEnemy::GetActiveIntentData() const
{
    if (!activeIntent)
        return nullptr;

    for (const BossIntentData& data : combatIntentData)
    {
        if (data.type == *activeIntent)
            return &data;
    }
    return nullptr;
}

BossIntentRangeStatus GruxEnemy::GetIntentRangeStatus(
    const BossIntentData& intentData, float distance) const
{
    if (distance < intentData.preferredMinDistance)
        return BossIntentRangeStatus::TooClose;
    if (distance > intentData.preferredMaxDistance)
        return BossIntentRangeStatus::TooFar;
    return BossIntentRangeStatus::InRange;
}

// 現在の距離領域に基づいて、候補となる行動を選択する関数
bool GruxEnemy::GetIntentAttackValidRange(float& outMinDistance, float& outMaxDistance) const
{
    if (!activeIntent)
        return false;

    if (*activeIntent == BossIntentType::DashAttackPlan)
    {
        outMinDistance = dashAttackValidMinDistance;
        outMaxDistance = dashAttackValidMaxDistance;
        return true;
    }
    if (*activeIntent == BossIntentType::JumpAttackPlan)
    {
        for (const BossAttackData& attackData : combatAttackData)
        {
            if (attackData.type != BossAttackType::JumpAttack)
                continue;
            outMinDistance = attackData.minDistance;
            outMaxDistance = attackData.maxDistance;
            return true;
        }
    }
    return false;
}

bool GruxEnemy::IsAttackPendingGoalAction(
    BossActionType actionType, const BossTargetContext& context) const
{
    if (!activeIntent || activeIntentStep != BossIntentStep::AttackPending || !context.valid)
        return false;

    float validMin = 0.0f;
    float validMax = 0.0f;
    if (!GetIntentAttackValidRange(validMin, validMax) ||
        context.xzDistance > validMax)
        return false;

    return (*activeIntent == BossIntentType::DashAttackPlan &&
        (actionType == BossActionType::DashAttack ||
            actionType == BossActionType::ChargeAttack)) ||
        (*activeIntent == BossIntentType::JumpAttackPlan &&
            actionType == BossActionType::JumpAttack);
}

bool GruxEnemy::IsActionForCurrentIntent(BossActionType actionType, const BossTargetContext& context) const
{
    if (!activeIntent || !context.valid)
        return false;

    const BossIntentData* intentData = GetActiveIntentData();
    if (!intentData)
        return false;

    if (activeIntentStep == BossIntentStep::AttackPending)
    {
        float validMin = 0.0f;
        float validMax = 0.0f;
        if (GetIntentAttackValidRange(validMin, validMax))
        {
            if (context.xzDistance > validMax)
                return actionType == BossActionType::Approach;
            return IsAttackPendingGoalAction(actionType, context);
        }
    }

    const BossIntentRangeStatus rangeStatus =
        GetIntentRangeStatus(*intentData, context.xzDistance);
    switch (*activeIntent)
    {
    case BossIntentType::CloseCombat:
        if (rangeStatus != BossIntentRangeStatus::TooFar)
        {
            return actionType == BossActionType::AttackLA ||
                actionType == BossActionType::AttackRA ||
                actionType == BossActionType::FastCombo;
        }
        return actionType == BossActionType::Approach;

    case BossIntentType::DashAttackPlan:
        if (rangeStatus == BossIntentRangeStatus::TooClose)
            return actionType == BossActionType::Retreat;
        if (rangeStatus == BossIntentRangeStatus::TooFar)
            return actionType == BossActionType::Approach;
        return actionType == BossActionType::DashAttack ||
            actionType == BossActionType::ChargeAttack;

    case BossIntentType::JumpAttackPlan:
        if (rangeStatus == BossIntentRangeStatus::TooClose)
            return actionType == BossActionType::Retreat;
        if (rangeStatus == BossIntentRangeStatus::TooFar)
            return actionType == BossActionType::Approach;
        return actionType == BossActionType::JumpAttack;
    case BossIntentType::CombatReposition:
        return actionType == BossActionType::RepositionLeft ||
            actionType == BossActionType::RepositionRight;
    }

    return false;
}

bool GruxEnemy::ShouldWaitForActiveIntentCooldown(const BossTargetContext& context) const
{
    if (!activeIntent || !context.valid)
        return false;

    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        const BossActionData& actionData = combatActionData[i];
        if (!IsActionForCurrentIntent(actionData.type, context))
            continue;
        if (!IsAttackPendingGoalAction(actionData.type, context) &&
            !IsActionCandidateForCurrentDistance(actionData, context.distanceRegion))
            continue;
        if (actionData.weight <= 0.0f)
            continue;
        if (combatActionCooldownRemaining[i] > 0.0f)
            return true;
    }
    return false;
}

bool GruxEnemy::ShouldFailIntentForPositioningRetryLimit(
    const BossTargetContext& context) const
{
    if (!activeIntent || activeIntentStep != BossIntentStep::AttackPending || !context.valid ||
        intentPositioningAttemptCount < maxIntentPositioningAttempts)
        return false;

    float validMin = 0.0f;
    float validMax = 0.0f;
    return GetIntentAttackValidRange(validMin, validMax) &&
        context.xzDistance > validMax;
}

void GruxEnemy::UpdateActionCandidateFlags(const BossTargetContext& context)
{
    combatActionCandidateFlags.fill(false);

    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        const BossActionData& actionData = combatActionData[i];

        if (!activeIntent)
        {
            combatActionCandidateReasons[i] = BossActionCandidateReason::NoActiveIntent;
            continue;
        }

        const bool isCloseCombatAttack =
            actionData.type == BossActionType::AttackLA ||
            actionData.type == BossActionType::AttackRA ||
            actionData.type == BossActionType::FastCombo;
        if (*activeIntent == BossIntentType::CloseCombat &&
            context.region == PlayerRelativeRegion::Back && isCloseCombatAttack)
        {
            combatActionCandidateReasons[i] = BossActionCandidateReason::WrongRelativeRegion;
            continue;
        }

        if (!IsActionForCurrentIntent(actionData.type, context))
        {
            combatActionCandidateReasons[i] = BossActionCandidateReason::NotForCurrentIntent;
            continue;
        }


        if (*activeIntent == BossIntentType::CombatReposition &&
            IsRepositionAction(actionData.type) &&
            !GetRepositionTargetEvaluation(actionData.type).sufficientlyMovable)
        {
            combatActionCandidateReasons[i] = BossActionCandidateReason::NoSafeDirection;
            continue;
        }

        if (!IsAttackPendingGoalAction(actionData.type, context) &&
            !IsActionCandidateForCurrentDistance(actionData, context.distanceRegion))
        {
            combatActionCandidateReasons[i] = BossActionCandidateReason::WrongDistance;
            continue;
        }

        if (combatActionCooldownRemaining[i] > 0.0f)
        {
            combatActionCandidateReasons[i] = BossActionCandidateReason::Cooldown;
            continue;
        }

        if (actionData.weight <= 0.0f)
        {
            combatActionCandidateReasons[i] = BossActionCandidateReason::ZeroWeight;
            continue;
        }

        // Future action-specific conditions are evaluated here.
        combatActionCandidateFlags[i] = true;
        combatActionCandidateReasons[i] = BossActionCandidateReason::Candidate;
    }
}

// アクションが現在の距離(Region)で候補になるかを判定する関数
bool GruxEnemy::IsActionCandidateForCurrentDistance(const BossActionData& actionData, BossDistanceRegion currentRegion) const
{
    if (actionData.minDistanceRegion <= currentRegion &&
        currentRegion <= actionData.maxDistanceRegion)
    {
        return true;
    }
    return false;
}

//  Action候補とBase WeightからEffective Weightを更新する
void GruxEnemy::UpdateActionCooldowns(float deltaTime)
{
    const float safeDeltaTime = (std::max)(0.0f, deltaTime);
    for (float& remaining : combatActionCooldownRemaining)
        remaining = (std::max)(0.0f, remaining - safeDeltaTime);
}

bool GruxEnemy::IsCombatAttackAction(BossActionType actionType) const
{
    for (const BossActionData& actionData : combatActionData)
    {
        if (actionData.type == actionType)
            return actionData.attackType.has_value();
    }
    return false;
}

bool GruxEnemy::IsAttackActionForIntent(
    BossActionType actionType, BossIntentType intentType) const
{
    switch (intentType)
    {
    case BossIntentType::CloseCombat:
        return actionType == BossActionType::AttackLA ||
            actionType == BossActionType::AttackRA ||
            actionType == BossActionType::FastCombo;
    case BossIntentType::DashAttackPlan:
        return actionType == BossActionType::DashAttack ||
            actionType == BossActionType::ChargeAttack;
    case BossIntentType::JumpAttackPlan:
        return actionType == BossActionType::JumpAttack;
    case BossIntentType::CombatReposition:
        return false;
    }
    return false;
}

float GruxEnemy::GetRecentAttackPenalty(BossActionType actionType) const
{
    if (lastStartedCombatAttack && *lastStartedCombatAttack == actionType)
        return std::clamp(recentAttackPenaltyLast, 0.01f, 1.0f);
    if (secondLastStartedCombatAttack && *secondLastStartedCombatAttack == actionType)
        return std::clamp(recentAttackPenaltySecond, 0.01f, 1.0f);
    return 1.0f;
}

float GruxEnemy::GetIntentRecentAttackPenalty(BossIntentType intentType) const
{
    float leastRestrictiveReadyPenalty = 0.0f;
    float leastRestrictiveFallbackPenalty = 0.0f;
    bool hasReadyAttack = false;
    bool hasDefinedAttack = false;
    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        const BossActionData& actionData = combatActionData[i];
        if (!actionData.attackType || actionData.weight <= 0.0f ||
            !IsAttackActionForIntent(actionData.type, intentType))
        {
            continue;
        }

        hasDefinedAttack = true;
        const float penalty = GetRecentAttackPenalty(actionData.type);
        leastRestrictiveFallbackPenalty = (std::max)(
            leastRestrictiveFallbackPenalty, penalty);
        if (combatActionCooldownRemaining[i] > 0.0f)
            continue;

        hasReadyAttack = true;
        leastRestrictiveReadyPenalty = (std::max)(
            leastRestrictiveReadyPenalty, penalty);
    }

    if (hasReadyAttack)
        return leastRestrictiveReadyPenalty;
    return hasDefinedAttack ? leastRestrictiveFallbackPenalty : 1.0f;
}

void GruxEnemy::UpdateActionEffectiveWeights()
{
    // 前回の計算結果をリセット
    combatActionEffectiveWeights.fill(0.0f);

    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        // 候補フラグが立っていない場合はスキップ
        if (!combatActionCandidateFlags[i])
            continue;

        // 基本の重みを取得
        const float baseWeight = combatActionData[i].weight;
        if (baseWeight <= 0.0f)
            continue;

        float effectiveWeight = baseWeight;
        if (combatActionData[i].attackType)
            effectiveWeight *= GetRecentAttackPenalty(combatActionData[i].type);
        combatActionEffectiveWeights[i] = (std::max)(0.0f, effectiveWeight);
    }
}

// ActionのEffective Weight合計を求める関数
float GruxEnemy::GetTotalActionWeight() const
{
    float totalWeight = 0.0f;
    for (const float effectiveWeight : combatActionEffectiveWeights)
    {
        totalWeight += effectiveWeight;
    }

    return totalWeight;
}

// Weightに基づいて行動を選択する関数。候補がない場合はstd::nulloptを返す
std::optional<BossActionType> GruxEnemy::SelectActionByWeight()
{
    const float totalWeight = GetTotalActionWeight();
    lastActionRandomTotalWeight = totalWeight;
    lastActionRandomRangeBegin.fill(0.0f);
    lastActionRandomRangeEnd.fill(0.0f);
    lastActionRandomWeights = combatActionEffectiveWeights;
    if (totalWeight <= 0.0f)
    {
        hasLastActionRandomRoll = false;
        return std::nullopt;
    }

    static std::mt19937 randomEngine{ std::random_device{}() };

    std::uniform_real_distribution<float> distribution(
        0.0f,
        totalWeight);

    const float selectionValue = distribution(randomEngine);
    lastActionRandomRoll = selectionValue;
    hasLastActionRandomRoll = true;

    float accumulatedWeight = 0.0f;

    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        const float effectiveWeight = combatActionEffectiveWeights[i];

        if (effectiveWeight <= 0.0f)
            continue;

        lastActionRandomRangeBegin[i] = accumulatedWeight;
        accumulatedWeight += effectiveWeight;
        lastActionRandomRangeEnd[i] = accumulatedWeight;

        if (selectionValue <= accumulatedWeight)
            return combatActionData[i].type;
    }

    return std::nullopt;
}

// 抽選結果をmemberへ保存する関数
bool GruxEnemy::SelectCombatAction()
{
    // 現在の状況を取得する
    const BossTargetContext context = BuildTargetContext();
    currentCombatPlayerDistance = context.xzDistance;
    lastCombatSelectionDistance = currentCombatPlayerDistance;

    intentRepositionReason = "None";
    if (activeIntent && activeIntentStep == BossIntentStep::AttackPending)
    {
        float validMin = 0.0f;
        float validMax = 0.0f;
        if (GetIntentAttackValidRange(validMin, validMax) &&
            context.xzDistance > validMax)
        {
            intentRepositionReason = "Too Far For Attack";
        }
    }


    // 現在の距離領域に基づいて、候補となる行動を更新する
    if (IsCombatRepositionActive())
        EvaluateRepositionTargets(context);

    UpdateActionCandidateFlags(context);
    // 候補となる行動の重みを更新する
    UpdateActionEffectiveWeights();

    // 重みに基づいて行動を選択する
    const std::optional<BossActionType> action = SelectActionByWeight();

    if (!action)
        return false;

    // 現在の選択結果を保存する
    lastActionType = selectedActionType;
    // 新しい選択結果を保存する
    selectedActionType = *action;
    hasSelectedActionDebug = true;

    const BossPositioningData* positioningData = GetPositioningDataForAction(selectedActionType);
    if (positioningData)
    {
        selectedPositioningData = *positioningData;
        if (IsRepositionAction(selectedActionType))
        {
            selectedPositioningData->maxMoveDistance = combatRepositionMoveDistance;
            selectedPositioningData->moveSpeed = combatRepositionMoveSpeed;
            const RepositionTargetEvaluation& evaluation =
                GetRepositionTargetEvaluation(selectedActionType);
            fixedPositioningTarget = evaluation.clampedTarget;
            fixedPositioningTargetValid = evaluation.sufficientlyMovable;
            repositionDesiredTarget = evaluation.desiredTarget;
            repositionClampedTarget = evaluation.clampedTarget;
            repositionSelectedTargetWasClamped = evaluation.wasClamped;
            selectedRepositionDirection =
                selectedActionType == BossActionType::RepositionLeft
                ? BossRepositionDirection::Left : BossRepositionDirection::Right;
            repositionCompletionReason = evaluation.sufficientlyMovable
                ? "Running" : GetRepositionFailureReason();
        }

        const BossIntentData* intentData = GetActiveIntentData();
        if (intentData && !IsRepositionAction(selectedActionType))
        {
            const float rangeWidth = (std::max)(0.0f,
                intentData->preferredMaxDistance - intentData->preferredMinDistance);
            const float safeInset = (std::min)(
                (std::max)(0.0f, intentData->positioningArrivalInset),
                rangeWidth * 0.25f);
            selectedPositioningData->completionType = BossPositioningCompletionType::TargetDistance;
            if (selectedActionType == BossActionType::Approach)
            {
                selectedPositioningData->targetDistance =
                    intentData->preferredMaxDistance - safeInset;
            }
            else if (selectedActionType == BossActionType::Retreat)
            {
                selectedPositioningData->targetDistance =
                    intentData->preferredMinDistance + safeInset;
            }
        }
    }
    else
        selectedPositioningData = std::nullopt;

    return true;
}

void KnightActor::Initialize(const Transform& transform)
{
    std::string parentName = "KnightActor";
    Character::Initialize(transform);
    skeletalMeshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    skeletalMeshComponent->SetModel("./Data/Models/Characters/Greystone/Greystone.gltf", false, true);

    // アニメーションコントローラーを作成
    int rootIndex = skeletalMeshComponent->FindIndexByName("root");
    auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootIndex);
    controller->AddAnimation("Idle", 0);
    controller->AddAnimation("Attack_A", 1);

    // アニメーションコントローラーを character に追加
    this->AddBodyAnimationController(controller);
    PlayBodyAnimation("Idle");

    // 当たり判定
    {
        std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("capsuleComponent", parentName);
        DirectX::XMFLOAT3 size = skeletalMeshComponent->GetModelSize();
        height = size.y;
        radius = size.x * 0.5f;
        mass = 60.0f;
        capsuleComponent->SetRadiusAndHeight(radius, height);
        capsuleComponent->SetMass(mass);
        capsuleComponent->SetCapsuleAxis(ShapeComponent::CapsuleAxis::y);
        capsuleComponent->SetLayer(CollisionLayer::Enemy);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetCollisionOffsetY(height * 0.5f);
        capsuleComponent->SetIsVisibleDebugBox(false);
        capsuleComponent->Initialize();
    }

    // 回転用コンポーネントを追加
    rotationComponent = this->AddComponent<class RotationComponent>("rotationComponent", parentName);

    // ポイントライトコンポーネントを追加
    auto pointLightComponent = this->AddComponent<PointLightComponent>("pointLightComponent", parentName);
    pointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f, 1.0f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    pointLightComponent->SetSharedLightName("PlayerPointLight");

    // ポイントライトコンポーネントを追加
    auto backPointLightComponent = this->AddComponent<PointLightComponent>("PlayerBackPointLight", parentName);
    backPointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f,-1.0f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    backPointLightComponent->SetSharedLightName("PlayerBackPointLight");

}

void KnightActor::Update(float elapsedTime)
{
    Character::Update(elapsedTime);

}


void SavarogEnemy::Initialize(const Transform& transform)
{
    std::string parentName = "SkeletonWarriorMeshComponent";
    Character::Initialize(transform);
    skeletalMeshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    skeletalMeshComponent->SetModel("./Data/Models/Characters/SevarogBloodred/animation.gltf", false, true);
    skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Enemy;   // オブジェクトの種類を Enemy に設定
    skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.6f;   // emissionPowerの値を大きくして、自己発光の強さを上げてみる
    skeletalMeshComponent->overrideDeferredPipelineName = "deferredFightStage";
    skeletalMeshComponent->plusAlphaCBuffer->data.brightness = 5.0f;
    skeletalMeshComponent->plusAlphaCBuffer->data.saturation = 1.4f;
    //skeletalMeshComponent->plusAlphaCBuffer->data.cpuColor = { 0.9f,0.08f,0.08f,1.0f };   // 目玉の色を赤にしてみる
    //for (auto& material : skeletalMeshComponent->model->materials)
    //{
    //    if (material.name == "MI_Grux_Eye")
    //    {// 目だったら、
    //        material.materialType = MaterialType::Eye;
    //    }
    //}


    // アニメーションコントローラーを作成
    int rootIndex = skeletalMeshComponent->FindIndexByName("root");
    auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootIndex);
    controller->AddAnimation("Idle", 0);
    controller->AddAnimation("Emote_Pull", 1);
    controller->AddAnimation("Swing1_Medium", 2);
    controller->AddAnimation("Swing1_Return_Idle", 3);
    controller->AddAnimation("Swing2_Medium", 4);
    controller->AddAnimation("Swing2_Return_Idle", 5);
    controller->AddAnimation("Swing3_Medium", 6);
    controller->AddAnimation("Swing3_Return_Idle", 7);
    controller->AddAnimation("Victory_Emote", 8);
    controller->AddAnimation("Recall", 9);
    controller->AddAnimation("LevelStart", 10);
    controller->AddAnimation("Emote_Pointing", 11);

    // アニメーションコントローラーを character に追加
    this->AddBodyAnimationController(controller);
    PlayBodyAnimation("Idle");

    // 当たり判定
    {
        std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("capsuleComponent", parentName);
        DirectX::XMFLOAT3 size = skeletalMeshComponent->GetModelSize();
        height = size.y;
        radius = size.x * 0.5f;
        mass = 60.0f;
        capsuleComponent->SetRadiusAndHeight(radius, height);
        capsuleComponent->SetMass(mass);
        capsuleComponent->SetCapsuleAxis(ShapeComponent::CapsuleAxis::y);
        capsuleComponent->SetLayer(CollisionLayer::Enemy);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetCollisionOffsetY(height * 0.5f);
        capsuleComponent->SetIsVisibleDebugBox(false);
        capsuleComponent->Initialize();
    }

    // 回転用コンポーネントを追加
    rotationComponent = this->AddComponent<class RotationComponent>("rotationComponent", parentName);

#if 1
    // ポイントライトコンポーネントを追加
    auto pointLightComponent = this->AddComponent<PointLightComponent>("pointLightComponent", parentName);
    pointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f, 1.0f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    pointLightComponent->SetSharedLightName("EnemyPointLight");
#endif // 0


    // エミッションを発生させるためにモデルを追加
    auto sphereMeshLeftComponent = this->AddComponent<SkeletalMeshComponent>("eye_left", parentName);
    int socketNode = skeletalMeshComponent->model->FindNodeIndexByName("head_cloth_big_l_01");
    sphereMeshLeftComponent->AttachToComponent(skeletalMeshComponent, socketNode); // ""
    sphereMeshLeftComponent->SetModel("./Data/Models/Primitives/Sphere.glb");
    sphereMeshLeftComponent->overrideDeferredPipelineName = "pointLightSkeletalMesh";
    sphereMeshLeftComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    sphereMeshLeftComponent->SetRelativeLocationDirect({ 0.0f, -0.04f, 0.0f });
    sphereMeshLeftComponent->SetRelativeScaleDirect({ 0.01f,0.01f,0.01f });
    sphereMeshLeftComponent->plusAlphaCBuffer->data.cpuColor = { 1.0f,0.0f,0.0f,1.0f };
    sphereMeshLeftComponent->plusAlphaCBuffer->data.emissionPower = 1.07f;

    // エミッションを発生させるためにモデルを追加
    auto sphereMeshComponent = this->AddComponent<SkeletalMeshComponent>("eye_right", parentName);
    socketNode = skeletalMeshComponent->model->FindNodeIndexByName("head_cloth_big_r_01");
    sphereMeshComponent->AttachToComponent(skeletalMeshComponent, socketNode); // ""
    sphereMeshComponent->SetModel("./Data/Models/Primitives/Sphere.glb");
    sphereMeshComponent->overrideDeferredPipelineName = "pointLightSkeletalMesh";
    sphereMeshComponent->SetIsCastShadow(false);    // 影を落とさないようにする
    sphereMeshComponent->SetRelativeLocationDirect({ 0.0f, 0.03f, -0.01f });
    sphereMeshComponent->SetRelativeScaleDirect({ 0.01f,0.01f,0.01f });
    sphereMeshComponent->plusAlphaCBuffer->data.cpuColor = { 1.0f,0.0f,0.0f,1.0f };
    sphereMeshComponent->plusAlphaCBuffer->data.emissionPower = 1.07f;

}

void SavarogEnemy::Update(float elapsedTime)
{
    Character::Update(elapsedTime);

    if (InputSystem::GetInputState("K", InputStateMask::Trigger))
    {
        PlayBodyAnimation("Victory_Emote", false, true, 0.1f);
    }
    if (!GetBodyAnimationController()->IsPlayAnimation())
    {
        PlayBodyAnimation("Idle");
    }
}

void GracialEnemy::Initialize(const Transform& transform)
{
    std::string parentName = "SkeletonWarriorMeshComponent";
    Character::Initialize(transform);
    skeletalMeshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    skeletalMeshComponent->SetModel("./Data/Models/Characters/Aurora_Gracial/animation.gltf", false, true);
    skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Enemy;   // オブジェクトの種類を Enemy に設定
    skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.6f;   // emissionPowerの値を大きくして、自己発光の強さを上げてみる
    //skeletalMeshComponent->plusAlphaCBuffer->data.cpuColor = { 0.9f,0.08f,0.08f,1.0f };   // 目玉の色を赤にしてみる
    //for (auto& material : skeletalMeshComponent->model->materials)
    //{
    //    if (material.name == "MI_Grux_Eye")
    //    {// 目だったら、
    //        material.materialType = MaterialType::Eye;
    //    }
    //}


    // アニメーションコントローラーを作成
    int rootIndex = skeletalMeshComponent->FindIndexByName("root");
    auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootIndex);
    controller->AddAnimation("Idle", 0);
    controller->AddAnimation("Ability_E", 1);
    controller->AddAnimation("Ability_R", 2);
    controller->AddAnimation("Primary_Attack_Fast_A", 3);
    controller->AddAnimation("Primary_Attack_Fast_B", 4);
    controller->AddAnimation("Primary_Attack_Fast_C", 5);
    controller->AddAnimation("Primary_Attack_Fast_D", 6);
    controller->AddAnimation("Jog_Fwd", 7);
    controller->AddAnimation("Jog_Fwd_Start", 8);
    controller->AddAnimation("Jog_Fwd_Stop", 9);
    controller->AddAnimation("HitReact_Back", 10);
    controller->AddAnimation("HitReact_Front", 11);
    controller->AddAnimation("HitReact_Left", 12);
    controller->AddAnimation("HitReact_Right", 13);
    controller->AddAnimation("Emote_Ice_Sculpture", 14);
    controller->AddAnimation("FrontEndPose", 15);
    controller->AddAnimation("Idle_Noise_A", 16);
    controller->AddAnimation("Idle_Noise_B", 17);
    controller->AddAnimation("Recall", 18);
    // アニメーションコントローラーを character に追加
    this->AddBodyAnimationController(controller);
    PlayBodyAnimation("Idle");

    // 当たり判定
    {
        std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("capsuleComponent", parentName);
        DirectX::XMFLOAT3 size = skeletalMeshComponent->GetModelSize();
        height = size.y;
        radius = size.x * 0.5f;
        mass = 60.0f;
        capsuleComponent->SetRadiusAndHeight(radius, height);
        capsuleComponent->SetMass(mass);
        capsuleComponent->SetCapsuleAxis(ShapeComponent::CapsuleAxis::y);
        capsuleComponent->SetLayer(CollisionLayer::Enemy);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetCollisionOffsetY(height * 0.5f);
        capsuleComponent->SetIsVisibleDebugBox(false);
        capsuleComponent->Initialize();
    }

    // 回転用コンポーネントを追加
    rotationComponent = this->AddComponent<class RotationComponent>("rotationComponent", parentName);



}

void GracialEnemy::Update(float elapsedTime)
{
    Character::Update(elapsedTime);

    if (InputSystem::GetInputState("K", InputStateMask::Trigger))
    {
        PlayBodyAnimation("Primary_Attack_Fast_D", false, true, 0.1f);
    }
    if (!GetBodyAnimationController()->IsPlayAnimation())
    {
        PlayBodyAnimation("Idle");
    }


}
