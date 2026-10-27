#include "pch.h"
#include "Player.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include <imgui.h>
#endif

#include "Graphics/Core/Graphics.h"
#include "Physics/Physics.h"
#include "Core/ActorManager.h"

#include "Components/Render/PointLightComponent.h"

#include "PlayerStateDerived.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/Scenes/GameScene.h"
#include "Game/DarkGame/Interactable.h"
#include "Game/DarkGame/DarkActors/InteractableActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Physics/CollisionFunction.h"

namespace
{
    float currentDitherTransparencyAlpha = 1.0f;
    float currentCameraCollisionRatio = 1.0f;

    int GetActionPriority(Player::ActionType action)
    {
        switch (action)
        {
        case Player::ActionType::Dodge:  return 3;
        case Player::ActionType::Attack: return 2;
        default:                         return 1;
        }
    }
}


void Player::Initialize(const Transform& transform)
{

    delayedHp = static_cast<float>(hp);
    delayedHpDelayTimer = 0.0f;

    std::string parentName = "skeletalComponent";
    // 描画用コンポーネントを追加
    {
        PROFILE_SCOPE("Create PlayerModel");

        skeletalMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
        skeletalMeshComponent->SetModel("./Data/Models/Characters/PlayerNoWeapon/player.gltf", false, true);
        skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Player;   // オブジェクトの種類を Player に設定
        skeletalMeshComponent->plusAlphaCBuffer->data.cpuColor.w = 1.0f;
        skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = playerAliveEmissionPower;   // 自己発光の強さを設定
        // 服の色のための色相変更
        skeletalMeshComponent->plusAlphaCBuffer->data.hueShift = -1.0f;
        skeletalMeshComponent->plusAlphaCBuffer->data.saturation = 0.442f;
        skeletalMeshComponent->plusAlphaCBuffer->data.brightness = 0.352f;
        skeletalMeshComponent->plusAlphaCBuffer->data.contrast = 0.8f;
        skeletalMeshComponent->overrideDeferredPipelineName = "GltfModelPlayerDeferredPS";
        skeletalMeshComponent->SetIsCastShadow(false);
        skeletalMeshComponent->SetIsShadowMap(true);
        for (auto& material : skeletalMeshComponent->model->materials)
        {
            if (material.name == "M_Aurora_Hair_Blonde_FrozenHearth")
            {// 髪の毛だったら
                material.materialType = MaterialType::Hair;
            }
            else if (material.name == "M_Aurora_Fur_FrozenHearth")
            {// Fur部分だったら
                material.overridePipelineName = "characterFurAndHairSkeletalMesh";
                material.materialType = MaterialType::Fur;
            }
            else if (material.name == "M_Aurora_Dress_Skirt_FrozenHearth" ||
                material.name == "M_Aurora_Dress_FrozenHearth" ||
                material.name == "M_Aurora_Body_Metals_FrozenHearth")
            {// 服部分だったら、
                material.materialType = MaterialType::Cloth;
            }
        }
    }

    {
        PROFILE_SCOPE("Create PlayerAnimationController");

        // ルートノードを設定する
        int rootNodeIndex = skeletalMeshComponent->FindIndexByName("root");
        // アニメーションコントローラーを作成
        auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootNodeIndex);

        controller->SetEnableRootMotion(false);
        controller->SetIgnoreRootMotion(true);
        controller->AddAnimation("Idle", 0);
        controller->AddAnimation("Jog_Fwd", 1);
        controller->AddAnimation("Primary_Attack_Fast_A", 2);
        controller->AddAnimation("Primary_Attack_Fast_B", 3);
        controller->AddAnimation("Primary_Attack_Fast_C", 4);
        controller->AddAnimation("Primary_Attack_Fast_D1_1", 5);
        controller->AddAnimation("Ability_RMB_Bwd_0", 6);
        controller->AddAnimation("Ability_RMB_Left_0", 7);
        controller->AddAnimation("Ability_RMB_Right_0", 8);
        controller->AddAnimation("Ability_RWB_Fwd_0", 9);
        controller->AddAnimation("Hit_Combat_B", 10);
        controller->AddAnimation("Hit_Combat_Death", 11);
        controller->AddAnimation("Hit_Combat_F", 12);
        controller->AddAnimation("Hit_Combat_L", 13);
        controller->AddAnimation("Hit_Combat_R", 14);
        controller->AddAnimation("Recall_0", 15);
        controller->AddAnimation("Level_Start_Cut", 16);
        controller->AddAnimation("Jog_Bwd", 17);
        controller->AddAnimation("Jog_Left", 18);
        controller->AddAnimation("Jog_Right", 19);
        controller->AddAnimation("Rush_Attack_Fast_A", 20);
        controller->AddAnimation("Rush_Attack_Fast_B", 21);
        controller->AddAnimation("Rush_Attack_Fast_C", 22);
        controller->AddAnimation("Rush_Attack_Fast_End", 23);
        controller->AddAnimation("Rush_Attack_Fast_D", 24);
        controller->AddAnimation("Walk_Fwd", 25);
        controller->AddAnimation("Get_Up", 26);
        controller->AddAnimation("Hit_Large_KnockBack", 27);
        controller->AddAnimation("Emote_Win", 28);
        controller->AddAnimation("Walk_Fwd_Stop_Fast", 29);
        controller->AddAnimation("Result_Recall", 30);
        controller->AddAnimation("Emote_Phase2", 31);
        controller->AddAnimation("Standing", 32);
        controller->SetRemoveRootTranslationFromPose("Hit_Large_KnockBack", true);

        controller->AddLocomotionBlendAnimation("Jog_Fwd", 0.0f, 0.0f);
        controller->AddLocomotionBlendAnimation("Jog_Right", 90.0f, 0.464f);
        controller->AddLocomotionBlendAnimation("Jog_Bwd", 180.0f, 0.0f);
        controller->AddLocomotionBlendAnimation("Jog_Left", -90.0f, 0.5f);

        std::string name = GetName();
        // アニメーションコントローラーのオーナーの名前を設定する
        controller->SetOwnerName(name);
        // 全てのNotifyAssetsをロードする
        controller->LoadAllNotifyAssets(name);
        // アニメーションコントローラーを character に追加
        this->AddBodyAnimationController(controller);
    }

    {
        PROFILE_SCOPE("Create PlayerStateMachine");
        // ステートマシンを作成
        stateMachine_ = std::make_shared<StateMachine>();
        stateMachine_->RegisterState(std::make_unique<PlayerIdleState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerRunningState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerAttackState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerDodgeState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerDamageState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerKnockBackState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerDeathPendingState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerDeathState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerWinState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerRushState>(this));

        // 初期ステートを設定
        stateMachine_->ChangeState("Idle");
    }

    {
        PROFILE_SCOPE("Create PlayerCollision");

        // 敵からの攻撃を受ける当たり判定用のコンポーネントを追加
        std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("capsuleComponent", parentName);
        DirectX::XMFLOAT3 size = skeletalMeshComponent->GetModelSize();
        height = size.y + 1.5f;
        radius = size.x * 0.5f;
        capsuleComponent->SetRadiusAndHeight(radius, height);
        capsuleComponent->SetMass(mass);
        capsuleComponent->SetCapsuleAxis(ShapeComponent::CapsuleAxis::y);
        capsuleComponent->SetLayer(CollisionLayer::Player);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::EnemyWeapon, CollisionComponent::CollisionResponse::Trigger);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Floor, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::WorldPropsNoRaycast, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetCollisionOffsetY(height * 0.5f);
        capsuleComponent->SetIsVisibleDebugBox(false);
        capsuleComponent->Initialize();
    }


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



    {
        PROFILE_SCOPE("Create PlayerComponent");

        // 入力用のコンポーネントを追加
        inputComponent = this->AddComponent<class InputComponent>("inputComponent", parentName);

        // 移動用コンポーネントを追加
        characterMovementComponent = this->AddComponent<CharacterMovementComponent>("movementComponent", parentName);
        characterMovementComponent->SetUseGravity(true);
        characterMovementComponent->SetDeferredMovementTick(true);

        // 回転用コンポーネントを追加
        rotationComponent = this->AddComponent<class RotationComponent>("rotationComponent", parentName);
        rotationComponent->SetRotateTime(0.1f);
    }

    int weaponSocketNode = skeletalMeshComponent->FindIndexByName("weapon");

    {
        // 剣の根本のコンポーネントを追加   
        swordRootComponent = AddComponent<SceneComponent>("swordRootComponent", parentName);
        swordRootComponent->SetRelativeLocationDirect({ 0.0f,0.0f,0.3f });
        swordRootComponent->AttachToComponent(skeletalMeshComponent, weaponSocketNode); // "VB root_weapon"

        // 剣の真ん中のコンポーネントを追加
        swordMiddleComponent = AddComponent<SceneComponent>("swordMiddleComponent", "swordRootComponent");
        swordMiddleComponent->SetRelativeLocationDirect({ 0.0f,0.0f,0.4f });

        // 剣の先端のコンポーネントを追加
        swordTipComponent = AddComponent<SceneComponent>("swordTipComponent", "swordRootComponent");
        swordTipComponent->SetRelativeLocationDirect({ 0.0f,0.0f,0.8f });
    }

    // 剣のメッシュコンポーネントを追加
    swordMeshComponent = this->AddComponent<SkeletalMeshComponent>("Sword", parentName);
    swordMeshComponent->SetModel("./Data/Models/Weapons/PlayerSwordGhost/Sword.gltf", false, true);
    swordMeshComponent->AttachToComponent(skeletalMeshComponent, weaponSocketNode); // "VB root_weapon"
    swordMeshComponent->plusAlphaCBuffer->data.cpuColor = { 0.0f,0.8f,1.0f ,0.0f };
    swordMeshComponent->plusAlphaCBuffer->data.flashValue = 9.5f;
    swordMeshComponent->overrideDeferredPipelineName = "GltfModelPlayerWeaponForwardPS";
    swordMeshComponent->overrideForwardPipelineName = "GltfModelPlayerWeaponForwardPS";

    // 剣の残像用の剣のメッシュコンポーネント
    for (auto& ghost : ghosts)
    {
        // マテリアル　ブレンド
        ghost.swordMeshComp = this->AddComponent<SkeletalMeshComponent>("Sword", parentName);
        ghost.swordMeshComp->SetModel("./Data/Models/Weapons/PlayerSwordGhost/Sword.gltf", false, true);
        ghost.swordMeshComp->SetIsVisible(false);
        ghost.swordMeshComp->overrideForwardPipelineName = "PlayerSwordGhostPS";
        ghost.swordMeshComp->overrideDeferredPipelineName = "PlayerSwordGhostPS";
    }

    // 固定Pose Ghostは通常のRender Queueへ出さず、SceneBaseから手動描画する。
    for (size_t ghostIndex = 0; ghostIndex < playerPoseGhosts.size(); ++ghostIndex)
    {
        auto& ghost = playerPoseGhosts[ghostIndex];
        ghost.renderConstantsComponent = this->AddComponent<SkeletalMeshComponent>(
            "PlayerPoseGhost" + std::to_string(ghostIndex), parentName);
        ghost.renderConstantsComponent->SetModel(
            "./Data/Models/Characters/PlayerNoWeapon/player.gltf", false, true);
        ghost.renderConstantsComponent->SetIsVisible(false);
        ghost.renderConstantsComponent->SetIsCastShadow(false);
        ghost.renderConstantsComponent->overrideForwardPipelineName = "PlayerSwordGhostPS";
        ghost.renderConstantsComponent->overrideDeferredPipelineName = "PlayerSwordGhostPS";
    }
    ResetPlayerPoseGhost();


    // カメラの注視点の位置のコンポーネントを追加
    cameraTargetComponent = AddComponent<SceneComponent>("cameraTargetComponent", parentName);
    cameraTargetComponent->SetRelativeLocationDirect({ 0.0f,1.0f,0.0f });

    // 軌跡初期化
    trail.Initialize();
    SetRushWeaponVisual(false);


    // UIの生成
    auto uiManager = GetOwnerScene()->GetUIManager();

    // 低HPの時に表示するビネットテクスチャ
    {
        lowHpVignetteImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/hp_low.png", "LowHpVignette");
        lowHpVignetteImageComponent->SetWorldPosition({ 960.0f, 540.0f });
        lowHpVignetteImageComponent->SetSize({ 1920.0f, 1080.0f });
        lowHpVignetteImageComponent->SetPivot({ 0.5f, 0.5f });
        lowHpVignetteImageComponent->SetColor(CoreColor{ 1.0f, 0.12f, 0.08f, 0.0f });
        lowHpVignetteImageComponent->SetVisible(false);
        lowHpVignetteImageComponent->zOrder = 100;
        uiManager->Add(lowHpVignetteImageComponent);
    }

    // ラッシュガイドUIの生成
    {
        rushGuideImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/Rush/rush_x_a_b.png", "RushGuide");
        rushGuideImageComponent->SetWorldPosition(rushGuidePosition);
        rushGuideImageComponent->SetScale(rushGuideScale);
        rushGuideImageComponent->SetSize(rushGuideSize);
        rushGuideImageComponent->SetPivot({ 0.5f, 0.5f });
        rushGuideImageComponent->SetVisible(false);
        uiManager->Add(rushGuideImageComponent);

        rushButtonImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/Rush/rush_y.png", "RushY");
        rushButtonImageComponent->SetWorldPosition(rushButtonPosition);
        rushButtonImageComponent->SetScale({ rushButtonBaseScale.x * 0.8f,rushButtonBaseScale.y * 0.8f });
        rushButtonImageComponent->SetSize(rushButtonSize);
        rushButtonImageComponent->SetPivot({ 0.5f, 0.5f });
        rushButtonImageComponent->SetVisible(false);
        uiManager->Add(rushButtonImageComponent);

        rushWordImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/Rush/rush_word.png", "RushWord");
        rushWordImageComponent->SetWorldPosition(rushWordPosition);
        rushWordImageComponent->SetScale(rushWordScale);
        rushWordImageComponent->SetSize(rushWordSize);
        rushWordImageComponent->SetPivot({ 0.5f, 0.5f });
        rushWordImageComponent->SetVisible(false);
        uiManager->Add(rushWordImageComponent);
    }

    // 敵がカメラ外に行ったときに表示するロックオン補助UI
    {
        lockOnGuideArrowImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/LT_arrow.png", "LockOnGuideArrow");
        lockOnGuideArrowImageComponent->SetSize(lockOnGuideArrowSize);
        lockOnGuideArrowImageComponent->SetScale(lockOnGuideArrowBaseScale);
        lockOnGuideArrowImageComponent->SetPivot({ 0.5f, 0.5f });
        lockOnGuideArrowImageComponent->SetVisible(false);
        lockOnGuideArrowImageComponent->zOrder = 20;
        uiManager->Add(lockOnGuideArrowImageComponent);

        lockOnGuideButtonImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/LT_button.png", "LockOnGuideButton");
        lockOnGuideButtonImageComponent->SetSize(lockOnGuideButtonSize);
        lockOnGuideButtonImageComponent->SetScale(lockOnGuideButtonBaseScale);
        lockOnGuideButtonImageComponent->SetPivot({ 0.5f, 0.5f });
        lockOnGuideButtonImageComponent->SetVisible(false);
        lockOnGuideButtonImageComponent->zOrder = 21;
        uiManager->Add(lockOnGuideButtonImageComponent);
    }


    // 操作説明UIを入れる
    InitializeOperationGuideUI();

    // Hpバー後ろ
    const DirectX::XMFLOAT2 hpBarPosition = { 110.0f, 88.0f };
    const DirectX::XMFLOAT2 hpIconPosition = { 50.0f, 75.0f };
    const DirectX::XMFLOAT2 hpBarPivot = { 0.0f, 0.5f };
    const DirectX::XMFLOAT2 hpBarScale = { 0.37f, 0.37f };

    auto hpBackgroundUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/HpBar/player_hp_background.png", "PlayerHpBackground");
    hpBackgroundUiComponent->SetWorldPosition(hpBarPosition);
    hpBackgroundUiComponent->SetSize({ 891.0f, 26.0f });
    hpBackgroundUiComponent->SetPivot(hpBarPivot);
    hpBackgroundUiComponent->SetScale(hpBarScale);
    hpBackgroundUiComponent->SetColor(CoreColor::White);
    hpBackgroundUiComponent->zOrder = 10;
    uiManager->Add(hpBackgroundUiComponent);

    hpDelayedFillUiComponent = std::make_shared<UIGaugeFillComponent>("./Data/Textures/UI/HpBar/player_hp_fill.png", "PlayerHpDelayedFill");
    hpDelayedFillUiComponent->SetWorldPosition(hpBarPosition);
    hpDelayedFillUiComponent->SetSize({ 889.0f, 22.0f });
    hpDelayedFillUiComponent->SetPivot(hpBarPivot);
    hpDelayedFillUiComponent->SetScale(hpBarScale);
    hpDelayedFillUiComponent->SetColor(playerHpDelayedColor);
    hpDelayedFillUiComponent->zOrder = 11;
    hpDelayedFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
    uiManager->Add(hpDelayedFillUiComponent);

    hpCurrentFillUiComponent = std::make_shared<UIGaugeFillComponent>("./Data/Textures/UI/HpBar/player_hp_fill.png", "PlayerHpCurrentFill");
    hpCurrentFillUiComponent->SetWorldPosition(hpBarPosition);
    hpCurrentFillUiComponent->SetSize({ 889.0f, 22.0f });
    hpCurrentFillUiComponent->SetPivot(hpBarPivot);
    hpCurrentFillUiComponent->SetScale(hpBarScale);
    hpCurrentFillUiComponent->SetColor(playerHpCurrentColor);
    hpCurrentFillUiComponent->zOrder = 12;
    hpCurrentFillUiComponent->SetValue(static_cast<float>(hp), static_cast<float>(maxHp));
    uiManager->Add(hpCurrentFillUiComponent);

    auto hpFrameUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/HpBar/player_hp_frame.png", "PlayerHpFrame");
    hpFrameUiComponent->SetWorldPosition(hpBarPosition);
    hpFrameUiComponent->SetSize({ 897.0f, 32.0f });
    hpFrameUiComponent->SetPivot(hpBarPivot);
    hpFrameUiComponent->SetScale(hpBarScale);
    hpFrameUiComponent->SetColor(CoreColor::White);
    hpFrameUiComponent->zOrder = 15;
    uiManager->Add(hpFrameUiComponent);

    auto hpIconUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/HpBar/player_hp_icon.png", "PlayerHpIcon");
    hpIconUiComponent->SetWorldPosition(hpIconPosition);
    hpIconUiComponent->SetSize({ 54.0f, 55.0f });
    hpIconUiComponent->SetPivot({ 0.0f, 0.5f });
    hpIconUiComponent->SetScale({ 1.4f, 1.4f });
    hpIconUiComponent->SetColor(CoreColor::White);
    hpIconUiComponent->zOrder = 16;
    uiManager->Add(hpIconUiComponent);

    hpBarUiComponents =
    {
        hpBackgroundUiComponent,
        hpDelayedFillUiComponent,
        hpCurrentFillUiComponent,
        hpFrameUiComponent,
        hpIconUiComponent,
    };

}

void Player::Update(float deltaTime)
{
    using namespace DirectX;

    // Just Dodge SlowやHitStopの影響を受けない実時間Fade。
    UpdatePlayerPoseGhost();

    // 瀕死状態のUIの更新
    UpdateLowHpEffects();

    if (const auto scene = GetOwnerScene(); scene && scene->IsPaused())
    {
        if (bufferCommand.type != ActionType::None)
            ClearActionRequest("scene_paused");
        if (inputComponent)
            inputComponent->ClearIntent();
        return;
    }

    UpdateOperationGuideUI();
    UpdateDamageFlash();
    UpdateLockOnGuideUI();

    // The prison opening owns this Player's transform and pose for its whole duration.
    // Do this before normal state-machine, locomotion, deferred movement, and Character::Update.
    if (prisonOpeningSitting || prisonOpeningStanding)
    {
        if (inputComponent)
            inputComponent->ClearIntent();
        characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
        characterMovementComponent->SetInputMagnitude(0.0f);
        characterMovementComponent->SetFrameAdditionalVelocity({ 0.0f, 0.0f, 0.0f });
        velocity = { 0.0f, 0.0f, 0.0f };

        // Reapply every frame: deferred movement and physics may otherwise correct the transform.
        SetPosition(prisonOpeningSittingPosition);
        SetEulerRotation({ 0.0f, prisonOpeningSittingYawDegrees, 0.0f });
        UpdateAllComponentTransforms();

        if (const auto controller = GetBodyAnimationController())
        {
            if (prisonOpeningSitting && prisonOpeningUseStandingPingPong)
            {
                UpdatePrisonOpeningStandingPingPong(deltaTime);
            }
            else if (prisonOpeningSitting &&
                (controller->GetCurrentAnimationName() != "Sitting" || !controller->IsPlayAnimation()))
            {
                controller->PlayAnimationImmediate("Sitting", true, true);
            }
            else
            {
                controller->OnUpdate(deltaTime);
            }
        }

        // The cinematic prison-opening path returns before the regular Dither update below.
        ApplyDitherTransparencyAlpha(1.0f);
        return;
    }
    if (battleActionsSuspended)
    {
        const auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene());
        if (gameScene && gameScene->IsBossPhaseTransitionActive())
        {
            UpdateWeaponVisualPresentation(deltaTime);
        }
        return;
    }

    if (finalHitWaiting)
    {
        if (finalHitAttackDrainActive && stateMachine_)
            stateMachine_->Update(deltaTime);
        if (const auto controller = GetBodyAnimationController())
            controller->OnUpdate(deltaTime);

        UpdateWeaponVisualPresentation(deltaTime);
        return;
    }

    const bool gameplayInputEnabled = !IsInWinState();

    // Hpバーの更新
    UpdateHpBarUI();

    // プレイヤーの透明化処理
    UpdateCameraOcclusionTransparency();

    // ジャスト回避のタイムスケールを更新する。
    UpdateJustDodgeTimeScales();

    if (characterMovementComponent)
    {
        characterMovementComponent->SetMoveSpeedSetting(walkSpeed, runSpeed);
    }

    const DirectX::XMFLOAT3 playerPos = GetPosition();

    // 剣の真ん中、根本、先の座標を取得する
    DirectX::XMFLOAT3 swordRootPos = GetHitBoxPoint(swordRootComponent, activeHitBoxOffset);
    DirectX::XMFLOAT3 swordMidPos = GetHitBoxPoint(swordMiddleComponent, activeHitBoxOffset);
    DirectX::XMFLOAT3 swordTipPos = GetHitBoxPoint(swordTipComponent, activeHitBoxOffset);

    // 当たり判定が有効な時にスフィアキャストをする
    if (hitBox)
    {
        struct SwordSweepResult
        {
            const char* source;
            bool hit;
            float sweepLength;
            DirectX::XMFLOAT3 start;
            DirectX::XMFLOAT3 end;
            HitResultWithActor result;
        };

        const auto sweepLength = [](const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end)
            {
                return MathHelper::Length(MathHelper::Subtract(end, start));
            };

        SwordSweepResult sweeps[] =
        {
            { "Root", false, sweepLength(prevSwordRootPos, swordRootPos), prevSwordRootPos, swordRootPos, {} },
            { "Mid",  false, sweepLength(prevSwordMidPos, swordMidPos),   prevSwordMidPos,  swordMidPos,  {} },
            { "Tip",  false, sweepLength(prevSwordTipPos, swordTipPos),   prevSwordTipPos,  swordTipPos,  {} },
        };


        const uint32_t enemyLayer = CollisionHelper::ToBit(CollisionLayer::Enemy);
        sweeps[0].hit = CollisionFunction::SphereRayCast(
            prevSwordRootPos, swordRootPos, sweeps[0].result, activeHitBoxRadius, enemyLayer, true);
        sweeps[1].hit = CollisionFunction::SphereRayCast(
            prevSwordMidPos, swordMidPos, sweeps[1].result, activeHitBoxRadius, enemyLayer, true);
        sweeps[2].hit = CollisionFunction::SphereRayCast(
            prevSwordTipPos, swordTipPos, sweeps[2].result, activeHitBoxRadius, enemyLayer, true);

        constexpr float minSweepLength = 0.0001f;
        const SwordSweepResult* selectedNormalHit = nullptr;
        const SwordSweepResult* selectedOverlapHit = nullptr;
        const SwordSweepResult* selectedDamageHit = nullptr;
        float selectedNormalToi = FLT_MAX;
        float selectedOverlapDepth = FLT_MAX;
        float selectedDamageToi = FLT_MAX;

        for (const SwordSweepResult& sweep : sweeps)
        {
            const bool validSweepLength = sweep.sweepLength > minSweepLength;
            const float toi = sweep.hit && validSweepLength
                ? sweep.result.distance / sweep.sweepLength
                : -1.0f;

            if (!sweep.hit || !validSweepLength)
                continue;

            const float damageOrder = sweep.result.initialOverlap ? 0.0f : toi;
            if (damageOrder < selectedDamageToi)
            {
                selectedDamageToi = damageOrder;
                selectedDamageHit = &sweep;
            }
            if (!sweep.result.initialOverlap && sweep.result.hasPosition &&
                sweep.result.hasNormal && toi < selectedNormalToi)
            {
                selectedNormalToi = toi;
                selectedNormalHit = &sweep;
            }
            else if (sweep.result.initialOverlap && sweep.result.hasPosition &&
                sweep.result.hasNormal && sweep.result.penetrationDepth < selectedOverlapDepth)
            {
                selectedOverlapDepth = sweep.result.penetrationDepth;
                selectedOverlapHit = &sweep;
            }
        }

        // 有効Positionがあれば、そのHitのActorをDamage対象にも優先する。
        const SwordSweepResult* selectedEffectHit = selectedNormalHit
            ? selectedNormalHit
            : selectedOverlapHit;
        const SwordSweepResult* selectedHit = selectedEffectHit
            ? selectedEffectHit
            : selectedDamageHit;
        if (selectedHit)
        {
            const HitResultWithActor& hit = selectedHit->result;
            if (!hitActors.contains(hit.actor))
            {
                const bool isRushHit = stateMachine_ &&
                    std::string(stateMachine_->GetStateName()) == "Rush";
                if (auto enemy = dynamic_cast<GruxEnemy*>(hit.actor))
                {
                    Logger::Log(U8("剣に敵が当たった"));
                    const int previousHP = enemy->GetHp();
                    const bool isNormalFourthHit = !isRushHit &&
                        currentAttackAnimation == "Primary_Attack_Fast_D1_1";
                    GruxEnemy::PlayerAttackHitType hitType =
                        isNormalFourthHit ? GruxEnemy::PlayerAttackHitType::ComboFinal
                        : GruxEnemy::PlayerAttackHitType::Normal;
                    if (isRushHit)
                    {
                        const auto* rushState = dynamic_cast<const PlayerRushState*>(
                            stateMachine_->GetCurrentState());
                        hitType = rushState &&
                            rushState->GetComboIndex() + 1 >= GetMaxRushAttackCount()
                            ? GruxEnemy::PlayerAttackHitType::RushFinal
                            : GruxEnemy::PlayerAttackHitType::Rush;
                    }
                    enemy->TakeDamageFromPlayerAttack(GetCurrentAttackDamage(),
                        isNormalFourthHit, hitType, playerPos);
                    const bool lethalHit = previousHP > 0 && enemy->GetHp() <= 0;
                    if (!isRushHit && selectedEffectHit && hit.hasPosition && hit.hasNormal)
                    {
                        enemy->SpawnHitEffect(hit.hitPoint, hit.normal, playerPos);
                    }
                    hitActors.emplace(enemy);
                    if (isRushHit)
                    {
                        enemy->SpawnRushHitRing(hit.hitPoint, hit.normal, playerPos,
                            hit.hasPosition, hit.hasNormal);
                    }
                    Time::SetSlow(0.0f,
                        isRushHit ? rushHitStopDuration : normalAttackHitStopDuration);
                    if (lethalHit && finalHitCallback)
                    {
                        if (isRushHit)
                        {
                            const auto controller = GetBodyAnimationController();
                            const auto* rushState = stateMachine_
                                ? dynamic_cast<const PlayerRushState*>(stateMachine_->GetCurrentState())
                                : nullptr;
                            const std::string animationName = controller ? controller->GetCurrentAnimationName() : std::string{};
                            const bool animationPlaying = controller && controller->IsPlayAnimation();
                            const float animationTime = controller ? controller->GetCurrentAnimationTime() : 0.0f;
                            const float animationLength = controller ? controller->GetCurrentAnimationLength() : 0.0f;
                        }
                        finalHitCallback(enemy, GetPosition());
                    }
                }
                else if (auto enemy = dynamic_cast<Enemy*>(hit.actor);
                    enemy && !enemy->IsDefeated() &&
                    enemy->TakeDamageFromPlayer(GetCurrentAttackDamage()))
                {
                    if (!isRushHit && selectedEffectHit && hit.hasPosition && hit.hasNormal)
                        enemy->SpawnPlayerHitEffect(hit.hitPoint, hit.normal, playerPos);
                    hitActors.emplace(enemy);
                    if (isRushHit)
                    {
                        enemy->SpawnPlayerRushHitEffect(hit.hitPoint, hit.normal, playerPos,
                            hit.hasPosition, hit.hasNormal);
                    }
                    Time::SetSlow(0.0f, isRushHit ? rushHitStopDuration : normalAttackHitStopDuration);
                }
            }
        }
    }

    // 最終ヒット待機とWinは、アニメーション更新後の特殊演出を優先する。
    const bool deferSwordVisuals = finalHitWaiting || IsInWinState();
    bool swordTrailUpdated = false;
    if (!deferSwordVisuals)
    {
        UpdateSwordVisuals(deltaTime, &swordRootPos, &swordTipPos, true);
        swordTrailUpdated = true;
    }
    UpdateSwordGhosts(deltaTime);

    // ヒットストップ処理
    if (finalHitWaiting)
    {
        if (const auto controller = GetBodyAnimationController())
            controller->OnUpdate(deltaTime);
        UpdateWeaponVisualPresentation(deltaTime);
        return;
    }

    if (gameplayInputEnabled)
    {
        // 入力処理
        CaptureActionRequest(deltaTime);

        // 現在フレームの入力から移動方向を確定し、その直後に一度だけ位置を更新する。
        UpdateMovement();
    }

    // モーションワープを考慮してプレイヤーを更新する
    UpdatePlayerMovementWithMotionWarp(deltaTime);


    // これは絶対入れる　アニメーションの更新をしているから
    Character::Update(deltaTime);

    if (IsInWinState())
    {
        // 更新途中にWinへ遷移した場合も、同じフレームで軌跡を二重更新しない。
        UpdateWeaponVisualPresentation(deltaTime, !swordTrailUpdated);
    }
    else if (!swordTrailUpdated)
    {
        // 更新途中にWinを抜けた場合は、通常時に採取した座標で更新する。
        UpdateSwordVisuals(deltaTime, &swordRootPos, &swordTipPos, true);
    }
    // Dodge State更新後の受付状態を、そのフレームのUIへ反映する。
    UpdateRushPromptUI();

    if (gameplayInputEnabled)
    {
        IInteractable* interactable = FindInteractable();
        if (InputSystem::GetInputState("GamePadA", InputStateMask::Trigger))
        {
            if (interactable)
            {
                interactable->Interact();
            }
        }
    }

    // 剣の真ん中、根本、先の座標を保存する
    prevSwordRootPos = swordRootPos;
    prevSwordMidPos = swordMidPos;
    prevSwordTipPos = swordTipPos;

}

void Player::Finalize()
{
    ResetLowHpEffects();
    ResetPlayerPoseGhost();
    ResetEyeCloseOverride();
    ForceResetPlayerSlow();
    ForceResetBossSlow();
    SetDeathCameraStartCallback(nullptr);
}

void Player::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader(U8("ゲームプレイ調整")))
    {
        if (ImGui::TreeNode(U8("移動")))
        {
            ImGui::SliderFloat(U8("歩行速度"), &walkSpeed, 0.25f, 10.0f);
            ImGui::SliderFloat(U8("走行速度"), &runSpeed, 0.25f, 15.0f);
            ImGui::SliderFloat(U8("前方移動速度倍率"), &forwardSpeedScale, 0.25f, 1.5f);
            ImGui::SliderFloat(U8("横移動速度倍率"), &sideSpeedScale, 0.25f, 1.5f);
            ImGui::SliderFloat(U8("後方移動速度倍率"), &backwardSpeedScale, 0.25f, 1.5f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("通常攻撃")))
        {
            ImGui::DragFloat(U8("MotionWarp攻撃距離（m）"), &motionWarpDesiredAttackSurfaceDistance, 0.01f, 0.3f, 1.0f);
            ImGui::DragFloat(U8("攻撃回頭最大補正角度（度）"), &attackRotationMaxCorrectionDegrees, 1.0f, 45.0f, 60.0f);
            ImGui::DragFloat(U8("攻撃回頭速度（度/秒）"), &attackRotationSpeedDegrees, 5.0f, 30.0f, 720.0f);
            ImGui::DragFloat(U8("攻撃補助探索半径（m）"), &attackAssistMaxDistance, 0.05f, 0.0f, 20.0f, "%.2f m");
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("ジャスト回避")))
        {
            ImGui::SliderFloat(U8("スロー時間倍率"), &justDodgeTimeScale, 0.10f, 1.00f);
            ImGui::SliderFloat(U8("スロー維持時間（秒）"), &justDodgeSlowHoldDuration, 0.00f, 2.0f, "%.2f sec");
            ImGui::SliderFloat(U8("スロー復帰時間（秒）"), &justDodgeSlowReturnDuration, 0.00f, 0.50f, "%.2f sec");
            ImGui::SliderFloat(U8("Rush中ボス時間倍率"), &rushBossSlowScale, 0.10f, 1.00f);
            ImGui::SliderFloat(U8("Rush中ボス復帰時間（秒）"), &rushBossReturnDuration, 0.00f, 0.50f, "%.2f sec");
            float angleDegrees = DirectX::XMConvertToDegrees(std::acos(std::clamp(justDodgeMaxTowardAttackerDot, -1.0f, 1.0f)));
            if (ImGui::SliderFloat(U8("ジャスト回避成功の最小角度"), &angleDegrees, 0.0f, 180.0f, "%.1f deg"))
            {
                justDodgeMaxTowardAttackerDot = std::cos(DirectX::XMConvertToRadians(angleDegrees));
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("ラッシュ攻撃")))
        {
            ImGui::DragFloat(U8("通常敵Rush停止余白（m）"), &normalEnemyRushSurfaceMargin, 0.01f, 0.0f, 1.0f, "%.3f m");
            ImGui::DragFloat(U8("ラッシュ後の敵までへのダッシュにかかる時間"), &moveToEnemyInterval, 0.05f);
            ImGui::DragFloat(U8("Rush接近ポーズBlend時間（秒）"), &rushApproachPoseBlendTime, 0.005f, 0.0f, 0.5f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(U8("Rush予備動作時間（秒）"), &rushWindupDuration, 0.005f, 0.0f, 0.5f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("ヒットストップ")))
        {
            ImGui::DragFloat(U8("ヒットストップの時間"), &normalAttackHitStopDuration, 0.01f);
            ImGui::DragFloat(U8("ラッシュ時のヒットストップの時間"), &rushHitStopDuration, 0.005f, 0.0f, 0.8f, "%.3f sec");
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("ノックバック")))
        {
            ImGui::DragFloat(U8("ノックバック時間（秒）"), &knockBackDuration, 0.01f, 0.01f, 3.0f, "%.2f sec");
            ImGui::DragFloat(U8("ノックバック初速"), &knockBackInitialSpeed, 0.1f, 0.0f, 30.0f, "%.2f");
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader(U8("演出調整")))
    {
        if (ImGui::TreeNode(U8("死亡時")))
        {
            ImGui::SliderFloat(U8("目を閉じるまでの時間"), &deathEyeCloseDelay, 0.0f, 10.0f, "%.3f sec");
            ImGui::SliderFloat(U8("目を閉じる時間"), &deathEyeCloseDuration, 0.01f, 5.0f, "%.3f sec");
            ImGui::SliderFloat(U8("青い光が消え始まるまでの時間"), &deathColorFadeDelay, 0.0f, 10.0f, "%.3f sec");
            ImGui::SliderFloat(U8("青い光が消える時間"), &deathColorFadeDuration, 0.01f, 5.0f, "%.3f sec");
            ImGui::ColorEdit3(U8("最終発光部の色"), &deathDeadEmissiveColor.x);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("ジャスト回避のプレイヤーの残像")))
        {
            ImGui::DragFloat(U8("プレイヤーの残像の初回の透明度"), &playerPoseGhostInitialAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat(U8("プレイヤーの残像のライフタイム"), &playerPoseGhostLifetime, 0.01f, 0.0f, 5.0f, "%.2f sec");
            ImGui::DragFloat(U8("残像生成間隔（秒）"), &playerGhostSpawnInterval, 0.001f, 0.0f, 1.0f, "%.3f sec");
            ImGui::ColorEdit3(U8("残像色"), &playerPoseGhostColor.x);
            ImGui::DragFloat(U8("残像発光強度"), &playerPoseGhostEmissive, 0.1f, 0.0f, 50.0f);
            ImGui::ColorEdit3(U8("残像輪郭色"), &playerPoseGhostEdgeColor.x);
            ImGui::ColorEdit3(U8("残像内部色"), &playerPoseGhostInnerColor.x);
            ImGui::DragFloat(U8("残像輪郭幅 "), &playerPoseGhostEdgeWidth, 0.01f, 0.0f, 10.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("瀕死状態時")))
        {
            ImGui::DragInt(U8("瀕死判定HP"), &lowHpThreshold, 1, 0, 50);
            ImGui::DragFloat(U8("心拍間隔"), &heartbeatInterval, 0.01f, 0.0f, 5.0f, "%.2f sec");
            ImGui::DragFloat(U8("脈動時間"), &lowHpPulseDuration, 0.01f, 0.0f, 5.0f, "%.2f sec");
            ImGui::DragFloat(U8("瀕死赤発光強度"), &lowHpPulseFlashAmount, 0.01f, 0.0f, 5.0f, "%.2f");
            ImGui::DragFloat(U8("瀕死ビネット濃度"), &lowHpVignetteBaseAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat(U8("ビネット脈動最大濃度"), &lowHpVignettePulseAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("プレイヤー被ダメージ時")))
        {
            ImGui::DragFloat(U8("被ダメージ発光時間（秒）"), &damageFlashDuration, 0.01f, 0.01f, 1.0f, "%.2f sec");
            ImGui::DragFloat(U8(" 被ダメージ本体色強度 "), &damageFlashBodyTintStrength, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat(U8("被ダメージリム光強度"), &damageFlashRimStrength, 0.05f, 0.0f, 10.0f, "%.2f");
            ImGui::ColorEdit4(U8("被ダメージ本体色"), &damageFlashColor.x);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("剣の軌跡と残像")))
        {
            ImGui::DragFloat(U8("軌跡が残る時間"), &trailRemainTime, 0.1f);
            ImGui::DragFloat(U8("残像が残る時間"), &ghostFadeTime, 0.1f);
            ImGui::DragFloat(U8("残像のemissive色"), &swordGhostEmissive, 0.1f);
            ImGui::DragFloat(U8("残像を出す間隔"), &ghostInterval, 0.001f, 0.0f, 1.0f, "%.5f");
            ImGui::DragFloat(U8("残像の輪郭"), &ghostEdgeWidth);
            const bool normalGhostEdgeColorChanged = ImGui::ColorEdit3(U8("残像のエッジの色"), &swordGhostColor.x);
            const bool rushGhostEdgeColorChanged = ImGui::ColorEdit3(U8("ラッシュ残像のエッジの色"), &rushSwordColor.x);
            if (normalGhostEdgeColorChanged)
            {
                SetRushWeaponVisual(false);
            }
            if (rushGhostEdgeColorChanged)
            {
                SetRushWeaponVisual(true);
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("プレイヤーDither")))
        {
            ImGui::SliderFloat(U8("透過範囲"), &transparencyStartRatio, 0.0f, 1.0f);
            if (ImGui::SliderFloat(U8("最小表示率"), &transparencyMinAlpha, 0.0f, 1.0f))
                transparencyMinAlpha = (std::min)(transparencyMinAlpha, transparencyMaxAlpha);
            if (ImGui::SliderFloat(U8("最大表示率"), &transparencyMaxAlpha, 0.0f, 1.0f))
                transparencyMaxAlpha = (std::max)(transparencyMaxAlpha, transparencyMinAlpha);
            ImGui::TreePop();
        }

    }
    if (ImGui::CollapsingHeader(U8("UI調整")))
    {
        if (ImGui::TreeNode(U8("プレイヤーHP UI")))
        {
            ImGui::DragFloat(U8("遅延HP保持時間（秒）"), &delayedHpDelayDuration, 0.01f, 0.0f, 2.0f, "%.2f sec");
            ImGui::DragFloat(U8("遅延HP追従速度（HP/秒）"), &delayedHpFollowSpeed, 0.1f, 0.0f, 100.0f, "%.2f HP/sec");
            ImGui::DragFloat(U8("HP回復UI表示時間（秒）"), &hpRecoveryUiDuration, 0.01f, 0.0f, 2.0f, "%.2f sec");
            if (ImGui::ColorEdit4(U8("現在HPバー色"), &playerHpCurrentColor.r) && hpCurrentFillUiComponent)
                hpCurrentFillUiComponent->SetColor(playerHpCurrentColor);
            if (ImGui::ColorEdit4(U8("遅延HPバー色"), &playerHpDelayedColor.r) && hpDelayedFillUiComponent)
                hpDelayedFillUiComponent->SetColor(playerHpDelayedColor);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("操作ガイドUI")))
        {
            ImGui::DragFloat(U8("点滅の強さ"), &operationGuidePulseAmplitude, 0.005f, 0.0f, 0.50f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(U8(" 点滅速度（rad/秒）"), &operationGuidePulseSpeed, 0.05f, 0.0f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            constexpr std::array<const char*, static_cast<size_t>(OperationGuideItem::Count)> labels = { "L", "LT", "Y", "X", "R" };
            for (size_t index = 0; index < operationGuideItems.size(); ++index)
            {
                auto& item = operationGuideItems[index];
                ImGui::PushID(static_cast<int>(index));
                ImGui::SeparatorText(labels[index]);
                ImGui::DragFloat2(U8("位置"), &item.position.x, 1.0f, 0.0f, 1920.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragFloat2(U8("基準スケール "), &item.baseScale.x, 0.005f, 0.01f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("Rush入力UI")))
        {
            ImGui::DragFloat2(U8("ガイド位置"), &rushGuidePosition.x, 1.0f);
            ImGui::DragFloat2(U8("ガイドサイズ"), &rushGuideSize.x, 1.0f, 1.0f, 1024.0f);
            ImGui::DragFloat2(U8("ガイドスケール"), &rushGuideScale.x, 0.01f, 0.01f, 4.0f);
            ImGui::DragFloat2(U8("Yボタン位置"), &rushButtonPosition.x, 1.0f);
            ImGui::DragFloat2(U8("Yボタンサイズ"), &rushButtonSize.x, 1.0f, 1.0f, 1024.0f);
            ImGui::DragFloat2(U8("Yボタンスケール"), &rushButtonBaseScale.x, 0.01f, 0.01f, 4.0f);
            ImGui::DragFloat2(U8("テキスト位置"), &rushWordPosition.x, 1.0f);
            ImGui::DragFloat2(U8("テキストサイズ"), &rushWordSize.x, 1.0f, 1.0f, 1024.0f);
            ImGui::DragFloat2(U8("テキストスケール"), &rushWordScale.x, 0.01f, 0.01f, 4.0f);
            ImGui::DragFloat(U8("フェードイン時間（秒）"), &rushPromptFadeInDuration, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(U8("ロックオンガイド")))
        {
            ImGui::DragFloat(U8("表示遅延（秒）"), &lockOnGuideDelay, 0.05f, 0.0f, 10.0f);
            ImGui::DragFloat(U8("点滅周期（秒）"), &lockOnGuidePulsePeriod, 0.05f, 0.1f, 5.0f);
            ImGui::DragFloat(U8("点滅時最小スケール"), &lockOnGuidePulseMinScale, 0.01f, 0.1f, 1.0f);
            ImGui::DragFloat2(U8("矢印サイズ"), &lockOnGuideArrowSize.x, 1.0f, 1.0f, 1024.0f);
            ImGui::DragFloat2(U8("矢印基準スケール"), &lockOnGuideArrowBaseScale.x, 0.01f, 0.01f, 4.0f);
            ImGui::DragFloat2(U8("ボタンサイズ"), &lockOnGuideButtonSize.x, 1.0f, 1.0f, 1024.0f);
            ImGui::DragFloat2(U8("ボタン基準スケール"), &lockOnGuideButtonBaseScale.x, 0.01f, 0.01f, 4.0f);
            ImGui::DragFloat(U8("ボタンオフセット"), &lockOnGuideButtonOffset, 1.0f, 0.0f, 500.0f);
            ImGui::DragFloat(U8("画面端マージン "), &lockOnGuideEdgeMargin, 1.0f, 0.0f, 500.0f);
            ImGui::DragFloat(U8("矢印回転オフセット（度）"), &lockOnGuideArrowRotationOffset, 1.0f, -360.0f, 360.0f);
            ImGui::TreePop();
        }
    }

    Character::DrawImGuiDetails();
#endif
}

void Player::OnAnimationNotifyBegin(const AnimationNotifyState& state)
{
    if (finalHitWaiting) return;
    switch (state.type)
    {
    case AnimationNotifyState::Type::HitBox:
    {
        Logger::Log(U8("当たり判定を開始しました"));
        activeHitBoxRadius = (state.hitBoxRadius < 0.01f ? 0.01f : state.hitBoxRadius);
        activeHitBoxOffset = state.hitBoxOffset;
        prevSwordRootPos = GetHitBoxPoint(swordRootComponent, activeHitBoxOffset);
        prevSwordMidPos = GetHitBoxPoint(swordMiddleComponent, activeHitBoxOffset);
        prevSwordTipPos = GetHitBoxPoint(swordTipComponent, activeHitBoxOffset);
        hitBox = true;
        if (stateMachine_->GetStateName() == "Attack")
            StopAttackTargetRotation();
        break;
    }
    case AnimationNotifyState::Type::InputWindow:
        // 入力受付を開始
        inputWindow = true;
        break;
    case AnimationNotifyState::Type::Invincible:
        invincibleWindow = true;
        Logger::Log(U8("無敵状態を開始しました"));
        break;
    case AnimationNotifyState::Type::TransitionWindow:
        transitionWindow = true;
        Logger::Log(U8("遷移許可区間を開始しました"));
        break;
    case AnimationNotifyState::Type::JustDodgeWindow:
        Logger::Log(U8("ジャスト回避許可区間を開始しました"));
        justDodgeWindow = true;
        break;
    case AnimationNotifyState::Type::DangerWindow:
        break;
    case AnimationNotifyState::Type::ShowTrail:
        showTrail = true;
        break;
    case AnimationNotifyState::Type::ShowEmissive:
        swordEmissivePower = state.value;
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
        float currentCenterDistance = 0.0f;
        float playerRadius = 0.0f;
        float enemyRadius = 0.0f;
        float remainingApproachDistance = 0.0f;

        // Rotation and MotionWarp share the target fixed at Attack Enter.
        if (stateMachine_->GetStateName() == "Attack")
        {
            if (const auto target = attackTarget.lock())
            {
                DirectX::XMFLOAT3 targetDelta = MathHelper::Subtract(
                    target->GetPosition(), GetPosition());
                targetDelta.y = 0.0f;
                currentCenterDistance = MathHelper::Length(targetDelta);
                if (const auto capsule = std::dynamic_pointer_cast<CapsuleComponent>(
                    FindComponentByName("capsuleComponent")))
                    playerRadius = capsule->GetRadius();
                if (const auto capsule = std::dynamic_pointer_cast<CapsuleComponent>(
                    target->FindComponentByName("capsuleComponent")))
                    enemyRadius = capsule->GetRadius();

                const float centerStopDistance = playerRadius + enemyRadius + motionWarpDesiredAttackSurfaceDistance;
                remainingApproachDistance = (std::max)(currentCenterDistance - centerStopDistance, 0.0f);
                actualWarpDistance = (std::min)(state.moveDistance, remainingApproachDistance);
            }
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

void Player::OnAnimationNotifyEnd(const AnimationNotifyState& state)
{
    if (finalHitWaiting) return;
    switch (state.type)
    {
    case AnimationNotifyState::Type::HitBox:
        hitBox = false;
        activeHitBoxRadius = weaponSphereRadius;
        activeHitBoxOffset = {};
        break;
    case AnimationNotifyState::Type::InputWindow:
        inputWindow = false;
        Logger::Log(U8("入力受付を終了しました"));
        break;
    case AnimationNotifyState::Type::Invincible:
        invincibleWindow = false;
        Logger::Log(U8("無敵状態を終了しました"));
        break;
    case AnimationNotifyState::Type::TransitionWindow:
        transitionWindow = false;
        Logger::Log(U8("遷移許可区間を終了しました"));
        break;
    case AnimationNotifyState::Type::JustDodgeWindow:
        Logger::Log(U8("ジャスト回避許可区間を終了しました"));
        justDodgeWindow = false;
        break;
    case AnimationNotifyState::Type::DangerWindow:
        break;
    case AnimationNotifyState::Type::ShowTrail:
        showTrail = false;
        break;
    case AnimationNotifyState::Type::ShowEmissive:
        swordEmissivePower = 0.0f;
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

void Player::OnAnimationNotifyEvent(const AnimationNotifyEvent& event)
{
    HandleCommonAnimationNotifyEvent(event);

    if (finalHitWaiting) return;
    switch (event.type)
    {
    case AnimationNotifyEvent::Type::PlaySE:
        HandleAnimationPlaySE(event);
        break;
    case AnimationNotifyEvent::Type::SpawnEffect:
        break;
    case AnimationNotifyEvent::Type::GameplayEvent:
        if (event.parameter == "DeathCameraStart" && stateMachine_ &&
            std::string(stateMachine_->GetStateName()) == "Death")
        {
            if (deathCameraStartCallback)
            {
                auto callback = std::move(deathCameraStartCallback);
                callback();
            }
        }
        else if (event.parameter == "rush_input_end" && stateMachine_ &&
            std::string(stateMachine_->GetStateName()) == "Dodge")
        {
            rushInputEndNotifyReceivedDebug = true;
        }
        break;
    }
}

void Player::OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event)
{
    switch (event.type)
    {
    case AnimationNotifyEvent::Type::PlaySE:
        HandleAnimationPlaySE(event);
        break;
    default:
        break;
    }
}

void Player::DrawAnimationEditorPreviewState(const AnimationNotifyState& state)
{
    if (state.type != AnimationNotifyState::Type::HitBox ||
        !swordRootComponent || !swordMiddleComponent || !swordTipComponent)
    {
        return;
    }

    constexpr DirectX::XMFLOAT4 hitBoxPreviewColor{ 0.25f, 1.0f, 0.45f, 1.0f };
    const float radius = (state.hitBoxRadius < 0.01f ? 0.01f : state.hitBoxRadius);
    const DirectX::XMFLOAT3 root = GetHitBoxPoint(swordRootComponent, state.hitBoxOffset);
    const DirectX::XMFLOAT3 middle = GetHitBoxPoint(swordMiddleComponent, state.hitBoxOffset);
    const DirectX::XMFLOAT3 tip = GetHitBoxPoint(swordTipComponent, state.hitBoxOffset);

    DebugRender::DrawSphere(root, radius, hitBoxPreviewColor, 0.0f, true);
    DebugRender::DrawSphere(middle, radius, hitBoxPreviewColor, 0.0f, true);
    DebugRender::DrawSphere(tip, radius, hitBoxPreviewColor, 0.0f, true);
    DebugRender::DrawLine(root, middle, hitBoxPreviewColor, 0.0f, true);
    DebugRender::DrawLine(middle, tip, hitBoxPreviewColor, 0.0f, true);
}

void Player::OnAnimationChanged()
{
    animationMotionWarps.clear();
    transitionWindow = false;  // ステート遷移してもいいかどうか
    comboQueued = false;   // コンボ攻撃がキューに入っているかどうか
    inputWindow = false;   // コンボ受付をするかどうか
    hitBox = false;     // 当たり判定
    activeHitBoxRadius = weaponSphereRadius;
    activeHitBoxOffset = {};
    justDodgeWindow = false;    // ジャスト回避を受け付けるかどうか
    invincibleWindow = false;   // 無敵状態かどうか
    justDodgeSuccess = false; // ジャスト回避が成功したかどうか
    showTrail = false;
    swordEmissivePower = 0.0f;
    hitActors.clear();
}

// ブレンドスペースのアニメーションを使用するかの更新関数
void Player::UpdateLocomotionAnimation()
{
    if (GetStateMachine()->GetStateName() != "Running")
    {
        //locomotionMode = LocomotionMode::Idle;
        return;
    }

    if (auto camera = dynamic_cast<DarkCameraActor*>(GetOwnerScene()->GetActiveCamera()))
    {
        switch (camera->GetMovementMode())
        {
        case DarkCameraActor::CameraMode::TPS:
            UpdateTPSLocomotion();
            break;
        case DarkCameraActor::CameraMode::Focus:
        case DarkCameraActor::CameraMode::LockOn:
            UpdateLockOnLocomotion();
            break;
        }
    }
}


void Player::RestartLocomotionAnimation()
{
    locomotionMode = LocomotionMode::None;
    UpdateLocomotionAnimation();
}

// TPSモードの移動時の更新処理
void Player::UpdateTPSLocomotion()
{
    float speed = characterMovementComponent->GetInputMagnitude();

    switch (locomotionMode)
    {
    case LocomotionMode::None:
        if (speed >= 0.6f)
            SetLocomotionMode(LocomotionMode::TPSRun);
        else
            SetLocomotionMode(LocomotionMode::TPSWalk);
        break;
    case LocomotionMode::Idle:
        if (speed > 0.0f)
            SetLocomotionMode(LocomotionMode::TPSWalk);
        break;
    case LocomotionMode::TPSWalk:
        if (speed <= 0.0f)
            SetLocomotionMode(LocomotionMode::Idle);
        else if (speed >= 0.6f)
            SetLocomotionMode(LocomotionMode::TPSRun);
        break;

    case LocomotionMode::TPSRun:
        if (speed < 0.55f)
            SetLocomotionMode(LocomotionMode::TPSWalk);
        break;
    case LocomotionMode::LockOnBlendRun:
        if (speed >= 0.6f)
        {
            SetLocomotionMode(LocomotionMode::TPSRun);
        }
        else if (speed > 0.0f)
        {
            SetLocomotionMode(LocomotionMode::TPSWalk);
        }
        else
        {
            SetLocomotionMode(LocomotionMode::Idle);
        }
        break;
    }

}
void Player::UpdateLockOnLocomotion()
{
    float speed = characterMovementComponent->GetInputMagnitude();

    switch (locomotionMode)
    {
    case LocomotionMode::None:
        if (speed > 0.f)
            SetLocomotionMode(LocomotionMode::LockOnBlendRun);
        break;
    case LocomotionMode::Idle:
        if (speed > 0.0f)
            SetLocomotionMode(LocomotionMode::LockOnBlendRun);
        break;
    case LocomotionMode::LockOnBlendRun:
        if (speed <= 0.0f)
            SetLocomotionMode(LocomotionMode::Idle);
        break;
    case LocomotionMode::TPSWalk:
    case LocomotionMode::TPSRun:
        if (speed >= 0.6f)
        {
            SetLocomotionMode(LocomotionMode::LockOnBlendRun);
        }
        else
        {
            SetLocomotionMode(LocomotionMode::Idle);
        }
        break;
    }

}

// イベントシーン開始時に呼ぶ処理
void Player::StartEvent()
{
    SetLowHpPresentationCinematicSuppressed(true);
    // 操作UIを非表示する
    operationGuideEventVisible = false;
    RefreshOperationGuideUIVisibility();
    // 入力を受け付けない
    InputSystem::SetInputEnabled(false);
    //　イベント中はplayerの透過処理をなくす
    this->moviePerform = true;
}

// イベントシーン終了時に呼ぶ処理
void Player::EndEvent()
{
    // 操作UIを表示する
    operationGuideEventVisible = true;
    RefreshOperationGuideUIVisibility();

    //　イベントが終わったのでplayerの透過処理を戻す
    this->moviePerform = false;
}

void Player::StopLowHpPresentation()
{
    lowHpPresentationSuppressed = true;
    ResetLowHpEffects();
}

void Player::SetLowHpPresentationCinematicSuppressed(const bool suppressed)
{
    lowHpPresentationCinematicSuppressed = suppressed;
    if (suppressed)
        ResetLowHpEffects();
}

bool Player::BeginPrisonOpeningSitting(
    const DirectX::XMFLOAT3& position, const float yawDegrees)
{
    const auto controller = GetBodyAnimationController();
    if (!controller)
        return false;

    prisonOpeningInputWasEnabled = InputSystem::IsInputEnabled();
    CancelGameplayActionsForCinematic();
    StartEvent();

    prisonOpeningSittingPosition = position;
    prisonOpeningSittingYawDegrees = yawDegrees;
    prisonOpeningSitting = true;
    prisonOpeningStanding = false;
    SetPosition(prisonOpeningSittingPosition);
    SetEulerRotation({ 0.0f, prisonOpeningSittingYawDegrees, 0.0f });
    UpdateAllComponentTransforms();

    controller->ReleaseHeldAnimationPose();
    controller->ResetAnimationRate();
    controller->SetUseBlendSpace(false);
    controller->SetIgnoreRootMotion(true);
    prisonOpeningSittingPingPongPhase = 0.0f;
    prisonOpeningSittingSampleTime = prisonOpeningSittingPingPongStartTime;
    prisonOpeningSittingPingPongForward = true;
    const bool started = prisonOpeningUseStandingPingPong
        ? controller->PlayAnimationImmediate("Standing", true, true) &&
        controller->HoldAnimationPose("Standing", prisonOpeningSittingSampleTime)
        : controller->PlayAnimationImmediate("Sitting", true, true);
    if (!started)
    {
        prisonOpeningSitting = false;
        controller->ReleaseHeldAnimationPose();
        EndEvent();
        InputSystem::SetInputEnabled(prisonOpeningInputWasEnabled);
        return false;
    }
    return true;
}

bool Player::BeginPrisonOpeningStanding()
{
    if (!prisonOpeningSitting || prisonOpeningStanding)
        return false;

    const auto controller = GetBodyAnimationController();
    if (!controller)
        return false;

    EndPrisonOpeningEyeOverride();
    prisonOpeningSitting = false;
    prisonOpeningStanding = true;
    if (prisonOpeningUseStandingPingPong)
        controller->ReleaseHeldAnimationPose(true);
    else
        controller->ReleaseHeldAnimationPose();
    controller->ResetAnimationRate();
    controller->SetUseBlendSpace(false);
    controller->ResetRootMotion("Standing", false, true, prisonOpeningSittingToStandingBlendTime);
    controller->SetIgnoreRootMotion(true);
    return controller->GetCurrentAnimationName() == "Standing";
}

void Player::ConfigurePrisonOpeningSittingMotion(const bool useStandingPingPong,
    const float startTime, const float endTime, const float speed,
    const float standingBlendTime)
{
    const bool modeChanged = prisonOpeningUseStandingPingPong != useStandingPingPong;
    prisonOpeningUseStandingPingPong = useStandingPingPong;
    prisonOpeningSittingPingPongStartTime = (std::max)(0.0f, startTime);
    prisonOpeningSittingPingPongEndTime = (std::max)(
        prisonOpeningSittingPingPongStartTime + 0.001f, endTime);
    prisonOpeningSittingPingPongSpeed = std::clamp(speed, 0.1f, 2.0f);
    prisonOpeningSittingToStandingBlendTime = std::clamp(standingBlendTime, 0.0f, 0.5f);

    if (!prisonOpeningSitting || !modeChanged)
        return;

    const auto controller = GetBodyAnimationController();
    if (!controller)
        return;

    controller->ReleaseHeldAnimationPose(true);
    controller->SetIgnoreRootMotion(true);
    if (prisonOpeningUseStandingPingPong)
    {
        prisonOpeningSittingPingPongPhase = 0.0f;
        prisonOpeningSittingSampleTime = prisonOpeningSittingPingPongStartTime;
        prisonOpeningSittingPingPongForward = true;
        if (controller->PlayAnimationImmediate("Standing", true, true))
            controller->HoldAnimationPose("Standing", prisonOpeningSittingSampleTime);
    }
    else
    {
        controller->ResetRootMotion("Sitting", true, true, prisonOpeningSittingToStandingBlendTime);
    }
}

void Player::EndPrisonOpeningSitting()

{
    EndPrisonOpeningEyeOverride();
    if (!prisonOpeningSitting && !prisonOpeningStanding)
        return;

    prisonOpeningSitting = false;
    prisonOpeningStanding = false;
    if (const auto controller = GetBodyAnimationController())
    {
        if (prisonOpeningUseStandingPingPong)
            controller->ReleaseHeldAnimationPose(true);
        controller->ResetAnimationRate();
    }
    if (stateMachine_)
        stateMachine_->ChangeState("Idle");
    EndEvent();
    InputSystem::SetInputEnabled(prisonOpeningInputWasEnabled);
}

bool Player::BeginPrisonOpeningEyeClosed()
{
    if (deathEyeCloseActive)
        return false;

    ResetEyeCloseOverride();
    closeEyeWeight = 1.0f;
    if (!RebuildEyeClosePoseOverride())
        return false;

    eyeCloseOverrideOwner = EyeCloseOverrideOwner::PrisonOpening;
    return true;
}

void Player::SetPrisonOpeningEyeClosedWeight(const float weight)
{
    if (eyeCloseOverrideOwner != EyeCloseOverrideOwner::PrisonOpening || deathEyeCloseActive)
        return;

    closeEyeWeight = std::clamp(weight, 0.0f, 1.0f);
    if (const auto controller = GetBodyAnimationController())
        controller->SetLocalPoseOverrideWeight(closeEyeWeight);
}

void Player::EndPrisonOpeningEyeOverride()
{
    if (eyeCloseOverrideOwner != EyeCloseOverrideOwner::PrisonOpening)
        return;

    if (const auto controller = GetBodyAnimationController())
        controller->ClearLocalPoseOverride();
    closeEyeWeight = 0.0f;
    eyeCloseOverrideOwner = EyeCloseOverrideOwner::None;
}

bool Player::IsPrisonOpeningEyeOverrideActive() const
{
    return eyeCloseOverrideOwner == EyeCloseOverrideOwner::PrisonOpening && !deathEyeCloseActive;
}

bool Player::IsPrisonOpeningStandingFinished() const
{
    if (!prisonOpeningStanding)
        return false;

    const auto controller = GetBodyAnimationController();
    return controller && controller->GetCurrentAnimationName() == "Standing" &&
        !controller->IsPlayAnimation();
}

void Player::SetHpBarVisible(const bool visible) const
{
    for (const auto& component : hpBarUiComponents)
    {
        if (!component)
            continue;
        component->SetVisible(visible);
        component->SetEnable(visible);
    }
}

void Player::SetGameplayHudVisible(const bool visible)
{
    gameplayHudFadeEntries.clear();
    SetHpBarVisible(visible);
    SetOperationGuideHudVisible(visible);
    if (rushGuideImageComponent) rushGuideImageComponent->SetVisible(visible);
    if (rushButtonImageComponent) rushButtonImageComponent->SetVisible(visible);
    if (rushWordImageComponent) rushWordImageComponent->SetVisible(visible);
    if (visible)
        UpdateRushPromptUI();
    else
    {
        SetOperationGuideHudVisible(false);
        HideAndResetLockOnGuideUI();
        if (lowHpVignetteImageComponent) lowHpVignetteImageComponent->SetVisible(false);
    }
}


void Player::SetOperationGuideHudVisible(const bool visible)
{
    operationGuideGameplayHudVisible = visible;
    RefreshOperationGuideUIVisibility();
}

void Player::SetOperationGuideItemVisible(const OperationGuideItem item, const bool visible)
{
    auto& state = operationGuideItems[static_cast<size_t>(item)];
    state.visible = visible;
    if (!visible)
        state.animationTimer = 0.0f;
    RefreshOperationGuideUIVisibility();
}

void Player::SetOperationGuideItemLearned(const OperationGuideItem item, const bool learned)
{
    auto& state = operationGuideItems[static_cast<size_t>(item)];
    state.learned = learned;
    if (learned)
        state.animationTimer = 0.0f;
}

bool Player::IsOperationGuideItemVisible(const OperationGuideItem item) const
{
    return operationGuideItems[static_cast<size_t>(item)].visible;
}

bool Player::IsOperationGuideItemLearned(const OperationGuideItem item) const
{
    return operationGuideItems[static_cast<size_t>(item)].learned;
}

void Player::SetMinimumHp(const int value)
{
    minimumHp = std::clamp(value, 0, maxHp);
}

void Player::RestoreHpToMaxWithUiAnimation()
{
    const float hpBeforeRecovery = static_cast<float>((std::max)(hp, 0));
    hp = maxHp;
    delayedHpDelayTimer = 0.0f;
    hpRecoveryUiStart = hpBeforeRecovery;
    hpRecoveryUiElapsed = 0.0f;
    hpRecoveryUiActive = hpRecoveryUiDuration > FLT_EPSILON && hpBeforeRecovery < static_cast<float>(maxHp);
    if (!hpRecoveryUiActive)
    {
        delayedHp = static_cast<float>(hp);
        hpRecoveryUiStart = delayedHp;
    }
    else
    {
        delayedHp = hpRecoveryUiStart;
    }
}

void Player::BeginGameplayHudFadeOut()
{
    gameplayHudFadeEntries.clear();
    const auto capture = [this](const std::shared_ptr<UICoreComponent>& core)
        {
            auto image = std::dynamic_pointer_cast<UIImageComponent>(core);
            if (image && image->IsVisible())
                gameplayHudFadeEntries.push_back({ image, image->color });
        };
    for (const auto& component : hpBarUiComponents) capture(component);
    capture(rushGuideImageComponent);
    capture(rushButtonImageComponent);
    capture(rushWordImageComponent);
    capture(lockOnGuideArrowImageComponent);
    capture(lockOnGuideButtonImageComponent);
    for (const auto& item : operationGuideItems) capture(item.image);
    capture(lowHpVignetteImageComponent);
}

void Player::SetGameplayHudFadeAlpha(const float alpha)
{
    const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
    for (auto& entry : gameplayHudFadeEntries)
    {
        CoreColor color = entry.color;
        color.a *= clampedAlpha;
        entry.component->SetColor(color);
    }
    if (clampedAlpha <= 0.0f)
    {
        for (auto& entry : gameplayHudFadeEntries)
            entry.component->SetColor(entry.color);
        SetGameplayHudVisible(false);
    }
}

void Player::ResetForBattleContinue(const Transform& battleStartTransform)
{
    finalHitWaiting = false;
    finalHitVisualPoseLatchRequested = false;
    finalHitRushFollowUpStopRequested = false;
    finalHitAttackDrainActive = false;
    finalHitAttackDrainCompleted = false;
    finalHitAttackDrainFailed = false;
    if (const auto controller = GetBodyAnimationController())
        controller->ReleaseLatchedVisualPose();
    lowHpPresentationSuppressed = false;
    ResetLowHpEffects();
    ResetEyeCloseOverride();
    deathVisualFade = 0.0f;
    deathEyeCloseStarted = false;
    deathVisualFadeStarted = false;
    if (skeletalMeshComponent && skeletalMeshComponent->plusAlphaCBuffer)
    {
        skeletalMeshComponent->plusAlphaCBuffer->data.deathVisualFade = 0.0f;
        skeletalMeshComponent->plusAlphaCBuffer->data.deathDeadEmissiveColor = deathDeadEmissiveColor;
        skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = playerAliveEmissionPower;
    }

    battleActionsSuspended = false;
    StopBattleActions();
    battleActionsSuspended = false;

    ForceResetPlayerSlow();
    ForceResetBossSlow();
    rushTarget.reset();
    locomotionMode = LocomotionMode::None;
    swordGhostElapsedTime = 0.0f;
    swordGhostIndex = 0;
    isPrevSwordWorldValid = false;
    rushPromptAlpha = 0.0f;
    rushPromptAnimationPhase = RushPromptAnimationPhase::Hidden;
    rushPromptAnimationTimer = 0.0f;
    rushPromptWasVisible = false;
    damageFlashTimer = 0.0f;
    ApplyDamageFlash(0.0f);

    hp = maxHp;
    delayedHp = static_cast<float>(hp);
    delayedHpDelayTimer = 0.0f;
    hpRecoveryUiActive = false;
    hpRecoveryUiElapsed = 0.0f;
    if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
    if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));

    SetPosition(battleStartTransform.GetLocation());
    SetQuaternionRotation(battleStartTransform.GetRotation());
    SetScale(battleStartTransform.GetScale());
    UpdateAllComponentTransforms();

    if (stateMachine_)
        stateMachine_->ChangeState("Idle");

    if (const auto controller = GetBodyAnimationController())
    {
        controller->ReleaseHeldAnimationPose();
        controller->ResetAnimationRate();
        controller->PlayAnimationImmediate("Idle", true, true);
    }
}

void Player::ForceDirectionImmediate(const DirectX::XMFLOAT3& direction) const
{
    if (rotationComponent)
        rotationComponent->ForceDirectionImmediate(direction);
}
void Player::BeginDeathEyeClose()
{
    ResetEyeCloseOverride();
    deathVisualFade = 0.0f;
    deathEyeCloseStarted = false;
    deathVisualFadeStarted = false;
    if (skeletalMeshComponent && skeletalMeshComponent->plusAlphaCBuffer)
    {
        skeletalMeshComponent->plusAlphaCBuffer->data.deathVisualFade = 0.0f;
        skeletalMeshComponent->plusAlphaCBuffer->data.deathDeadEmissiveColor = deathDeadEmissiveColor;
    }
    deathEyeCloseActive = true;
    eyeCloseOverrideOwner = EyeCloseOverrideOwner::Death;
    RebuildEyeClosePoseOverride();
}

void Player::UpdateDeathEyeClose(const float deathElapsedTime)
{
    if (!deathEyeCloseActive)
        return;

    if (!deathEyeCloseStarted && deathElapsedTime >= deathEyeCloseDelay)
        deathEyeCloseStarted = true;

    const float normalizedCloseEyeTime = std::clamp(
        (deathElapsedTime - deathEyeCloseDelay) /
        (std::max)(deathEyeCloseDuration, FLT_EPSILON),
        0.0f,
        1.0f);
    closeEyeWeight = normalizedCloseEyeTime * normalizedCloseEyeTime *
        (3.0f - 2.0f * normalizedCloseEyeTime);
    GetBodyAnimationController()->SetLocalPoseOverrideWeight(closeEyeWeight);
}

void Player::EndDeathEyeClose()
{
    ResetEyeCloseOverride();
}

void Player::UpdateDeathVisualFade(const float deathElapsedTime)
{
    if (!deathVisualFadeStarted && deathElapsedTime >= deathColorFadeDelay)
    {
        deathVisualFadeStarted = true;
        // 死亡時の意思が消える時のSE再生
        CoreAudio::PlayOneShot("./Data/Sound/SE/player_stone_fade_start.wav", 2.0f);
    }

    deathVisualFade = std::clamp(
        (deathElapsedTime - deathColorFadeDelay) /
        (std::max)(deathColorFadeDuration, FLT_EPSILON),
        0.0f, 1.0f);
    if (skeletalMeshComponent && skeletalMeshComponent->plusAlphaCBuffer)
    {
        skeletalMeshComponent->plusAlphaCBuffer->data.deathVisualFade = deathVisualFade;
        skeletalMeshComponent->plusAlphaCBuffer->data.deathDeadEmissiveColor = deathDeadEmissiveColor;
    }
}

bool Player::BeginFinalHitWait()
{
    finalHitWaiting = true;
    ForceResetPlayerSlow();
    ForceResetBossSlow();

    finalHitAttackDrainActive = false;
    finalHitAttackDrainCompleted = false;
    finalHitAttackDrainFailed = false;
    const auto controller = GetBodyAnimationController();
    const std::string stateName = stateMachine_ ? stateMachine_->GetStateName() : "";
    const bool supportedAttackState = stateName == "Attack" || stateName == "Rush";
    const std::string animationName = controller
        ? controller->GetCurrentAnimationName() : std::string{};
    const auto* rushState = stateMachine_ && stateName == "Rush"
        ? dynamic_cast<const PlayerRushState*>(stateMachine_->GetCurrentState()) : nullptr;
    const std::string rushAttackAnimation = rushState
        ? rushState->GetCurrentAttackAnimationForDebug() : std::string{};
    const bool expectedAttackClip = stateName == "Attack"
        ? animationName == currentAttackAnimation
        : stateName == "Rush" && rushState && animationName == rushAttackAnimation;
    const bool validCurrentAttack = supportedAttackState && controller &&
        controller->IsPlayAnimation() && expectedAttackClip &&
        controller->GetAnimationLength(animationName) > FLT_EPSILON;

    if (!validCurrentAttack)
    {
        // An unexpected state/clip is not treated as a completed attack pose.
        // Fall through to the existing safe cleanup without creating a wait.
        finalHitAttackDrainFailed = true;
        ClearTransientBattleActions();
        return false;
    }

    finalHitAttackDrainActive = true;
    RequestFinalHitVisualPoseLatch();
    RequestFinalHitRushFollowUpStop();
    ClearActionRequest("final_hit_attack_drain");
    ClearAttackTarget();
    SetRushInputAcceptance(false, "FinalHit");
    hitBox = false;
    inputWindow = false;
    transitionWindow = false;
    comboQueued = false;
    invincible = false;
    invincibleWindow = false;
    justDodgeWindow = false;
    justDodgeSuccess = false;
    animationMotionWarps.clear();
    hitActors.clear();
    showTrail = false;
    swordEmissivePower = 0.0f;
    trail.Clear();
    StopAttackTargetRotation();
    StopKnockBackForcedMove();
    knockBackActive = false;
    knockBackElapsed = 0.0f;
    characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->SetInputMagnitude(0.0f);
    characterMovementComponent->SetFrameAdditionalVelocity({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->MoveToActor(std::shared_ptr<Actor>{}, 0.0f, 0.0f);
    characterMovementComponent->AddForcedMove({ 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f);
    characterMovementComponent->ResetFixedSpeed();
    velocity = { 0.0f, 0.0f, 0.0f };
    return true;
}

void Player::StopBattleActions()
{
    battleActionsSuspended = true;
    SetTimeScale(0.0f);
    ClearTransientBattleActions();
}

void Player::ResumeBattleActionsAfterEvent()
{
    battleActionsSuspended = false;
    ResetTimeScale();
    if (stateMachine_)
        stateMachine_->ChangeState("Idle");
}

bool Player::IsFinalHitAttackDrainComplete() const
{
    if (finalHitAttackDrainFailed || !finalHitAttackDrainActive)
        return true;
    if (!finalHitAttackDrainCompleted)
        return false;
    if (!stateMachine_)
        return true;
    const std::string stateName = stateMachine_->GetStateName();
    return stateName != "Attack" && stateName != "Rush";
}

void Player::MarkFinalHitAttackDrainInterrupted()
{
    finalHitAttackDrainActive = false;
    finalHitAttackDrainCompleted = false;
    finalHitAttackDrainFailed = true;
    finalHitVisualPoseLatchRequested = false;
}

void Player::ClearTransientBattleActions()
{
    ClearActionRequest("battle_end");
    finalHitVisualPoseLatchRequested = false;
    finalHitRushFollowUpStopRequested = false;
    ClearAttackTarget();
    SetRushInputAcceptance(false);
    hitBox = false;
    inputWindow = false;
    transitionWindow = false;
    comboQueued = false;
    invincible = false;
    invincibleWindow = false;
    justDodgeWindow = false;
    justDodgeSuccess = false;
    animationMotionWarps.clear();
    hitActors.clear();
    showTrail = false;
    swordEmissivePower = 0.0f;
    trail.Clear();
    SetRushWeaponVisual(false);
    if (rushGuideImageComponent) rushGuideImageComponent->SetVisible(false);
    if (rushButtonImageComponent) rushButtonImageComponent->SetVisible(false);
    if (rushWordImageComponent) rushWordImageComponent->SetVisible(false);
    rushPromptAnimationPhase = RushPromptAnimationPhase::Hidden;
    rushPromptAnimationTimer = 0.0f;
    rushPromptWasVisible = false;
    if (rushButtonImageComponent)
        rushButtonImageComponent->SetScale({
            rushButtonBaseScale.x * 0.8f,
            rushButtonBaseScale.y * 0.8f });
    for (auto& ghost : ghosts)
    {
        ghost.isVisible = false;
        if (ghost.swordMeshComp) ghost.swordMeshComp->SetIsVisible(false);
    }
    ResetPlayerPoseGhost();
    StopAttackTargetRotation();
    StopKnockBackForcedMove();
    knockBackActive = false;
    knockBackElapsed = 0.0f;
    characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->SetInputMagnitude(0.0f);
    characterMovementComponent->SetFrameAdditionalVelocity({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->MoveToActor(std::shared_ptr<Actor>{}, 0.0f, 0.0f);
    characterMovementComponent->AddForcedMove({ 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f);
    characterMovementComponent->ResetFixedSpeed();
    velocity = { 0.0f, 0.0f, 0.0f };
}

void Player::ClearBattleVisualsForPhaseTransition()
{
    ClearTransientBattleActions();
    ClearSwordTrailForPhaseTransition();
    swordGhostElapsedTime = 0.0f;
    swordGhostIndex = 0;
    damageFlashTimer = 0.0f;
    ApplyDamageFlash(0.0f);
}

void Player::CancelGameplayActionsForCinematic()
{
    ClearBattleVisualsForPhaseTransition();
    if (inputComponent)
        inputComponent->ClearIntent();

    // StopBattleActions clears movement, but make the cinematic contract explicit.
    characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->SetInputMagnitude(0.0f);
    characterMovementComponent->SetFrameAdditionalVelocity({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->MoveToActor(std::shared_ptr<Actor>{}, 0.0f, 0.0f);
    characterMovementComponent->AddForcedMove({ 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f);
    characterMovementComponent->ResetFixedSpeed();
    velocity = { 0.0f, 0.0f, 0.0f };

    currentAttackAnimation = startAttackAnimation;
    rushTarget.reset();
    locomotionMode = LocomotionMode::Idle;
    HideAndResetLockOnGuideUI();
    if (stateMachine_)
        stateMachine_->ChangeState("Idle");
    if (const auto controller = GetBodyAnimationController())
    {
        controller->ReleaseHeldAnimationPose();
        controller->SetUseBlendSpace(false);
        controller->PlayAnimationImmediate("Idle", true, true);
    }
}

void Player::NeutralizeForPhase2Cinematic()
{
    CancelGameplayActionsForCinematic();
}

void Player::ClearSwordTrailForPhaseTransition()
{
    // The trail is CPU-side history. Clear both its active notify result and
    // cached sword transform so a cinematic teleport cannot bridge old/new poses.
    showTrail = false;
    trail.Clear();
    isPrevSwordWorldValid = false;
    prevSwordRootPos = { 0.0f, 0.0f, 0.0f };
    prevSwordMidPos = { 0.0f, 0.0f, 0.0f };
    prevSwordTipPos = { 0.0f, 0.0f, 0.0f };
    swordEmissivePower = 0.0f;
    if (swordMeshComponent && swordMeshComponent->plusAlphaCBuffer)
        swordMeshComponent->plusAlphaCBuffer->data.emissionPower = 0.0f;
}

void Player::EnterWinState()
{
    finalHitWaiting = false;
    battleActionsSuspended = false;
    if (stateMachine_)
        stateMachine_->ChangeState("Win");
}
bool Player::IsInWinState() const
{
    return stateMachine_ && std::string(stateMachine_->GetStateName()) == "Win";
}
// 軌跡を描画する処理
void Player::RenderTrail(ID3D11DeviceContext* immediateContext)
{
    trail.Render(immediateContext);
}

void Player::CapturePlayerPoseGhost()
{
    ResetPlayerPoseGhost();
    playerPoseGhostSpawnSequenceActive = true;
    playerPoseGhostSpawnTimer = 0.0f;
    nextPlayerPoseGhostIndex = 0;

    if (CapturePlayerPoseGhost(nextPlayerPoseGhostIndex))
        ++nextPlayerPoseGhostIndex;
    else
        playerPoseGhostSpawnSequenceActive = false;
}

void Player::ResetPlayerPoseGhost()
{
    for (auto& ghost : playerPoseGhosts)
    {
        ghost.nodes.clear();
        ghost.alpha = 0.0f;
        ghost.elapsedTime = 0.0f;
        ghost.isVisible = false;
    }
    playerPoseGhostSpawnTimer = 0.0f;
    nextPlayerPoseGhostIndex = 0;
    playerPoseGhostSpawnSequenceActive = false;
}

void Player::CaptureDodgeWorldDirectionSnapshot()
{
    DirectX::XMFLOAT3 direction{};
    if (useDodgeWorldDirection)
    {
        direction = dodgeWorldDirection;
    }
    else
    {
        switch (dodgeDirection)
        {
        case DodgeDirection::Forward:  direction = { 0.0f, 0.0f, 1.0f }; break;
        case DodgeDirection::Backward: direction = { 0.0f, 0.0f, -1.0f }; break;
        case DodgeDirection::Left:     direction = { -1.0f, 0.0f, 0.0f }; break;
        case DodgeDirection::Right:    direction = { 1.0f, 0.0f, 0.0f }; break;
        }

        const float yaw = DirectX::XMConvertToRadians(GetEulerRotation().y);
        const float sinYaw = std::sinf(yaw);
        const float cosYaw = std::cosf(yaw);
        direction =
        {
            direction.x * cosYaw + direction.z * sinYaw,
            0.0f,
            -direction.x * sinYaw + direction.z * cosYaw
        };
    }

    direction.y = 0.0f;
    const float length = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    dodgeWorldDirectionSnapshotValid = length > 0.0001f;
    dodgeWorldDirectionSnapshot = dodgeWorldDirectionSnapshotValid
        ? DirectX::XMFLOAT3{ direction.x / length, 0.0f, direction.z / length }
    : DirectX::XMFLOAT3{};
}

// モード変更用関数
void Player::SetLocomotionMode(LocomotionMode mode)
{
    if (locomotionMode == mode)
    {
        return;
    }

    locomotionMode = mode;

    auto controller = GetBodyAnimationController();

    switch (mode)
    {
    case LocomotionMode::TPSWalk:
        controller->SetUseBlendSpace(false);
        characterMovementComponent->ResetFixedSpeed();
        PlayBodyAnimation("Walk_Fwd", true, true, 0.2f, true);
        break;
    case LocomotionMode::TPSRun:
        controller->SetUseBlendSpace(false);
        characterMovementComponent->ResetFixedSpeed();
        PlayBodyAnimation("Jog_Fwd", true, true, 0.2f, true);
        break;
    case LocomotionMode::LockOnBlendRun:
        characterMovementComponent->ResetFixedSpeed();
        controller->SetUseBlendSpace(true);
        break;
    case LocomotionMode::Idle:
        controller->SetUseBlendSpace(false);
        GetStateMachine()->ChangeState("Idle");
        break;
    }

}

// 保存済み要求を現在のステートから実行する
bool Player::TryExecuteActionRequest()
{
    const ActionType requestedType = bufferCommand.type;
    std::string targetState;
    switch (requestedType)
    {
    case ActionType::None:
        return false;
    case ActionType::Attack:
        targetState = "Attack";
        break;
    case ActionType::Dodge:
        dodgeDirection = bufferCommand.dodgeDirection;
        dodgeWorldDirection = bufferCommand.dodgeWorldDirection;
        useDodgeWorldDirection = bufferCommand.useDodgeWorldDirection;
        targetState = "Dodge";
        break;
    }

    // 要求されたアクションのStateへ遷移する
    stateMachine_->ChangeState(targetState);

    if (stateMachine_->GetStateName() != targetState)
    {
        return false;
    }

    ConsumeActionRequest(requestedType);
    return true;
}

// 入力を消費する処理
void Player::ConsumeActionRequest(ActionType expectedType)
{
    if (bufferCommand.type != expectedType)
    {
        return;
    }
    if (expectedType == ActionType::Dodge)
    {
        dodgeDirection = bufferCommand.dodgeDirection;
        dodgeWorldDirection = bufferCommand.dodgeWorldDirection;
        useDodgeWorldDirection = bufferCommand.useDodgeWorldDirection;
    }
    bufferCommand = {};
}

//当たった時の処理
bool Player::TryTakeDamage(int damage, const DirectX::XMFLOAT3& attackerPosition,
    const DamagePolicy policy)
{
    if (finalHitWaiting) return false;
    const std::string currentState = stateMachine_->GetStateName();
    const bool ignoreDodgeIFrames =
        policy == DamagePolicy::IgnoreDodgeIFrames && currentState == "Dodge";
    if (invincibleWindow && !ignoreDodgeIFrames)
    {
        return false;
    }
    if (invincible)
    {
        return false;
    }

    if (currentState == "Damage" || currentState == "DeathPending" || currentState == "Death")
    {
        return false;
    }

    DirectX::XMFLOAT3 direction = MathHelper::Subtract(GetPosition(), attackerPosition);
    direction.y = 0.0f;
    if (MathHelper::Length(direction) > 0.0001f)
    {
        direction = MathHelper::Normalize(direction);
    }
    else
    {
        direction = GetForward();
        direction.y = 0.0f;
    }

    const int hpBeforeDamage = hp;
    const int appliedDamage = (std::max)(0, damage);
    hpRecoveryUiActive = false;
    hp = (std::max)(minimumHp, (std::max)(0, hp - appliedDamage));
    if (hp < hpBeforeDamage)
    {
        delayedHp = static_cast<float>(hpBeforeDamage);
        delayedHpDelayTimer = delayedHpDelayDuration;
        StartDamageFlash();
    }
    if (hp <= 0)
        RefreshOperationGuideUIVisibility();
    CoreAudio::PlayOneShot("./Data/Sound/SE/player_damage_voice.wav", 0.4f);
    CoreAudio::PlayOneShot("./Data/Sound/SE/player_damage.wav", 0.5f);
    // コントローラー振動
    InputSystem::SetVibration(0.8f, 0.15f);
    SetRushInputAcceptance(false, "Damage");
    ClearActionRequest("damage_applied");
    Logger::Log(U8("プレイヤーにダメージ！ HP:") + std::to_string(hp));


    if (hp > 0)
    {
        DirectX::XMFLOAT3 toAttacker =
        {
            attackerPosition.x - GetPosition().x,
            0.0f,
            attackerPosition.z - GetPosition().z
        };
        const float attackerLength = std::sqrt(
            toAttacker.x * toAttacker.x + toAttacker.z * toAttacker.z);
        if (attackerLength <= 0.0001f)
        {
            hitReactionDirection = HitReactionDirection::Front;
        }
        else
        {
            toAttacker.x /= attackerLength;
            toAttacker.z /= attackerLength;

            DirectX::XMFLOAT3 forward = GetForward();
            forward.y = 0.0f;
            const float forwardLength = std::sqrt(
                forward.x * forward.x + forward.z * forward.z);
            DirectX::XMFLOAT3 right = GetRight();
            right.y = 0.0f;
            const float rightLength = std::sqrt(
                right.x * right.x + right.z * right.z);

            if (forwardLength <= 0.0001f || rightLength <= 0.0001f)
            {
                hitReactionDirection = HitReactionDirection::Front;
            }
            else
            {
                forward.x /= forwardLength;
                forward.z /= forwardLength;
                right.x /= rightLength;
                right.z /= rightLength;

                const float forwardDot = toAttacker.x * forward.x + toAttacker.z * forward.z;
                const float rightDot = toAttacker.x * right.x + toAttacker.z * right.z;
                if (std::abs(forwardDot) >= std::abs(rightDot))
                    hitReactionDirection = forwardDot >= 0.0f
                    ? HitReactionDirection::Front : HitReactionDirection::Back;
                else
                    hitReactionDirection = rightDot >= 0.0f
                    ? HitReactionDirection::Left : HitReactionDirection::Right;
            }
        }
    }
    const char* targetState = hp > 0 ? "Damage" : "DeathPending";
    stateMachine_->ChangeState(targetState);
    return true;
}

bool Player::StartKnockBack(const DirectX::XMFLOAT3& direction)
{
    if (!stateMachine_ || hp <= 0 ||
        std::string(stateMachine_->GetStateName()) == "DeathPending" ||
        std::string(stateMachine_->GetStateName()) == "Death")
    {
        return false;
    }

    DirectX::XMFLOAT3 horizontalDirection = direction;
    horizontalDirection.y = 0.0f;
    if (MathHelper::Length(horizontalDirection) <= FLT_EPSILON)
    {
        horizontalDirection = GetForward();
        horizontalDirection.y = 0.0f;
    }
    if (MathHelper::Length(horizontalDirection) <= FLT_EPSILON)
        return false;

    knockBackDirection = MathHelper::Normalize(horizontalDirection);
    SetRushInputAcceptance(false, "KnockBack");
    stateMachine_->ChangeState("KnockBack");
    return true;
}

bool Player::IsBossAttackTargetAvailable() const
{
    if (!stateMachine_ || hp <= 0 || IsPendingKill())
        return false;

    const State* currentState = stateMachine_->GetCurrentState();
    if (!currentState)
        return false;

    // PlayerKnockBackState owns both Hit_Large_KnockBack and Get_Up.
    // Keep the Boss independent from Player state-name strings.
    if (dynamic_cast<const PlayerKnockBackState*>(currentState))
        return false;

    return !dynamic_cast<const PlayerDeathPendingState*>(currentState) &&
        !dynamic_cast<const PlayerDeathState*>(currentState);
}
void Player::ClearActionRequest(const char* reason)
{
    bufferCommand = {};
}

// 攻撃開始時の処理
void Player::StartAttack()
{
    hitActors.clear();
}

// ジャスト回避成功時の処理
void Player::StartJustDodgeSuccess(const std::shared_ptr<Enemy>& enemy)
{
    const auto gruxEnemy = std::dynamic_pointer_cast<GruxEnemy>(enemy);
    const uint64_t attackSequenceId = gruxEnemy
        ? gruxEnemy->GetCurrentAttackSequenceId()
        : 0;
    const auto controller = GetBodyAnimationController();
    const std::string animationName = controller
        ? controller->GetCurrentAnimationName()
        : "";
    const float animationTime = controller
        ? controller->GetCurrentAnimationTime()
        : -1.0f;
    float justWindowStart = 0.0f;
    float justWindowEnd = 0.0f;
    if (controller)
    {
        const auto assetIt = controller->animationNotifyAssets.find(
            controller->GetAnimationClip());
        if (assetIt != controller->animationNotifyAssets.end())
        {
            for (const auto& state : assetIt->second.notifyTrack.states)
            {
                if (state.type == AnimationNotifyState::Type::JustDodgeWindow)
                {
                    justWindowStart = state.startTime;
                    justWindowEnd = state.endTime;
                    break;
                }
            }
        }
    }
    lastJustAnimationTime = animationTime;
    lastJustWindowStart = justWindowStart;
    lastJustWindowEnd = justWindowEnd;
    lastJustWindowRatio = justWindowEnd > justWindowStart
        ? std::clamp((animationTime - justWindowStart) /
            (justWindowEnd - justWindowStart), 0.0f, 1.0f)
        : 0.0f;
    lastJustDodgeAttack = gruxEnemy
        ? gruxEnemy->GetCurrentAttackNameForDebug()
        : "Unknown";
    lastJustBossHitBoxElapsed = gruxEnemy
        ? gruxEnemy->GetActiveHitBoxElapsedForDebug()
        : -1.0f;

    // SEの再生
    CoreAudio::PlayOneShot("./Data/Sound/SE/just_dodge3.wav", 1.5f);
    // コントローラー振動
    InputSystem::SetVibration(0.30f, 0.06f);

    // ジャスト回避成功フラグをオンにする
    justDodgeSuccess = true;
    if (auto* scene = dynamic_cast<SceneBase*>(GetOwnerScene()))
        scene->StartJustDodgeRadialBlur();

    rushInputEndNotifyReceivedDebug = false;
    rushInputEndReasonDebug = "None";
    CapturePlayerPoseGhost();
    // スローモーションにする
    // rush時のtargetを保存する
    rushTarget = enemy;
    const float scale = std::clamp(justDodgeTimeScale, 0.10f, 1.0f);
    enemy->SetTimeScale(scale);
    SetTimeScale(scale);
    // Playerとボスをスローにする
    playerSlowPhase = JustDodgeSlowPhase::Hold;
    bossSlowPhase = JustDodgeSlowPhase::Hold;
    playerSlowHoldTimer = std::max<float>(0.0f, justDodgeSlowHoldDuration);
    bossSlowHoldTimer = playerSlowHoldTimer;
    playerSlowReturnElapsed = 0.0f;
    bossSlowReturnElapsed = 0.0f;
    playerSlowReturnStartScale = scale;
    bossSlowReturnStartScale = scale;
    if (playerSlowHoldTimer <= FLT_EPSILON)
    {
        BeginPlayerSlowReturn();
        BeginBossSlowReturn();
    }
}

bool Player::CanAcceptInitialRushInput() const
{
    return rushInputAccepting &&
        justDodgeSuccess &&
        stateMachine_ &&
        std::string(stateMachine_->GetStateName()) == "Dodge" &&
        !rushTarget.expired();
}
bool Player::LatchFinalHitVisualPoseAfterAnimationUpdate()
{
    if (!finalHitVisualPoseLatchRequested)
        return false;

    finalHitVisualPoseLatchRequested = false;
    const auto controller = GetBodyAnimationController();
    const bool latched = controller && controller->LatchCurrentVisualPose();
    if (finalHitAttackDrainActive)
    {
        finalHitAttackDrainCompleted = latched;
        finalHitAttackDrainFailed = !latched;
        if (!latched)
            finalHitAttackDrainActive = false;
    }
    return latched;
}

bool Player::CanShowInitialRushGuide() const
{
    if (CanAcceptInitialRushInput())
        return true;

    return stateMachine_ &&
        std::string(stateMachine_->GetStateName()) == "Dodge" &&
        justDodgeSuccess &&
        !rushTarget.expired();
}

bool Player::CanShowRushComboGuide() const
{
    return stateMachine_ &&
        std::string(stateMachine_->GetStateName()) == "Rush" &&
        !rushTarget.expired();
}

bool Player::CanShowRushPrompt() const
{
    return CanShowInitialRushGuide() || CanShowRushComboGuide();
}


bool Player::IsRushOpportunityActive() const
{
    if (!stateMachine_ || rushTarget.expired())
        return false;

    const std::string stateName = stateMachine_->GetStateName();
    return (stateName == "Dodge" && justDodgeSuccess) || stateName == "Rush";
}

void Player::SetRushInputAcceptance(bool accepting, const char* endReason)
{
    if (accepting)
    {
        rushInputEndReasonDebug = "None";
    }
    else if (endReason)
    {
        rushInputEndReasonDebug = endReason;
    }

    if (rushInputAccepting == accepting)
    {
        return;
    }

    rushInputAccepting = accepting;

    if (accepting)
    {
        rushPromptAlpha = 0.0f;
    }
    else
    {
        // 初回Rush入力済みなら、Dodge TransitionWindow待ちの間も
        // 受付時の表示とAlphaをそのまま維持する。
        if (CanShowInitialRushGuide())
            return;

        rushPromptAlpha = 0.0f;
        rushPromptAnimationPhase = RushPromptAnimationPhase::Hidden;
        rushPromptAnimationTimer = 0.0f;
        rushPromptWasVisible = false;
        if (rushGuideImageComponent) rushGuideImageComponent->SetVisible(false);
        if (rushButtonImageComponent)
        {
            rushButtonImageComponent->SetVisible(false);
            rushButtonImageComponent->SetScale({
                rushButtonBaseScale.x * 0.8f,
                rushButtonBaseScale.y * 0.8f });
        }
        if (rushWordImageComponent) rushWordImageComponent->SetVisible(false);
    }
}

// ジャスト回避のタイムスケールを更新する。
void Player::UpdateJustDodgeTimeScales()
{
    // Player/BossのSlowは独立させ、どちらも実時間で更新する。
    if (playerSlowPhase == JustDodgeSlowPhase::Hold)
    {
        playerSlowHoldTimer -= Time::UnscaledDeltaTime();
        if (playerSlowHoldTimer <= 0.0f)
        {
            BeginPlayerSlowReturn();
        }
    }
    else if (playerSlowPhase == JustDodgeSlowPhase::Return)
    {
        playerSlowReturnElapsed += Time::UnscaledDeltaTime();
        const float t = justDodgeSlowReturnDuration > FLT_EPSILON ? std::clamp(playerSlowReturnElapsed / justDodgeSlowReturnDuration, 0.0f, 1.0f) : 1.0f;
        const float smoothT = MathHelper::SmoothStep01(t);
        SetTimeScale(std::lerp(playerSlowReturnStartScale, 1.0f, smoothT));
        if (t >= 1.0f)
        {
            ForceResetPlayerSlow();
        }
    }
    if (bossSlowPhase == JustDodgeSlowPhase::Hold)
    {
        bossSlowHoldTimer -= Time::UnscaledDeltaTime();
        if (bossSlowHoldTimer <= 0.0f)
        {
            BeginBossSlowReturn();
        }
    }
    else if (bossSlowPhase == JustDodgeSlowPhase::Return)
    {
        if (auto target = rushTarget.lock())
        {
            bossSlowReturnElapsed += Time::UnscaledDeltaTime();
            const float duration = activeBossSlowReturnDuration;
            const float t = duration > FLT_EPSILON
                ? std::clamp(bossSlowReturnElapsed / duration, 0.0f, 1.0f) : 1.0f;
            const float smoothT = t * t * (3.0f - 2.0f * t);
            target->SetTimeScale(std::lerp(bossSlowReturnStartScale, 1.0f, smoothT));
            if (t >= 1.0f) ForceResetBossSlow();
        }
        else
        {
            bossSlowPhase = JustDodgeSlowPhase::Inactive;
        }
    }
}


void Player::BeginPlayerSlowReturn()
{
    if (playerSlowPhase == JustDodgeSlowPhase::Inactive || playerSlowPhase == JustDodgeSlowPhase::Return)
    {
        return;
    }

    playerSlowPhase = JustDodgeSlowPhase::Return;
    playerSlowReturnElapsed = 0.0f;
    playerSlowReturnStartScale = GetTimeScale();
    if (justDodgeSlowReturnDuration <= FLT_EPSILON)
    {
        ForceResetPlayerSlow();
    }
}

void Player::BeginBossSlowReturn(bool afterRush)
{
    if (bossSlowPhase == JustDodgeSlowPhase::Inactive || bossSlowPhase == JustDodgeSlowPhase::Return)
    {
        return;
    }
    if (auto target = rushTarget.lock())
    {
        bossSlowPhase = JustDodgeSlowPhase::Return;
        bossSlowReturnElapsed = 0.0f;
        bossSlowReturnStartScale = target->GetTimeScale();
        activeBossSlowReturnDuration = std::max<float>(0.0f,
            afterRush ? rushBossReturnDuration : justDodgeSlowReturnDuration);
        if (activeBossSlowReturnDuration <= FLT_EPSILON) ForceResetBossSlow();
    }
    else bossSlowPhase = JustDodgeSlowPhase::Inactive;
}

// Rush中、対象のスローを維持する。
void Player::HoldBossSlowForRush()
{
    if (auto target = rushTarget.lock())
    {
        const float scale = std::clamp(rushBossSlowScale, 0.10f, 1.0f);
        target->SetTimeScale(scale);
        bossSlowPhase = JustDodgeSlowPhase::RushHold;
    }
    else bossSlowPhase = JustDodgeSlowPhase::Inactive;
}

void Player::ForceResetPlayerSlow()
{
    ResetTimeScale();
    playerSlowPhase = JustDodgeSlowPhase::Inactive;
    playerSlowHoldTimer = 0.0f;
    playerSlowReturnElapsed = 0.0f;
    playerSlowReturnStartScale = 1.0f;
}

void Player::ForceResetBossSlow()
{
    if (auto target = rushTarget.lock()) target->ResetTimeScale();
    bossSlowPhase = JustDodgeSlowPhase::Inactive;
    bossSlowHoldTimer = 0.0f;
    bossSlowReturnElapsed = 0.0f;
    bossSlowReturnStartScale = 1.0f;
    activeBossSlowReturnDuration = justDodgeSlowReturnDuration;
}

void Player::SetRushWeaponVisual(const bool enabled)
{
    const DirectX::XMFLOAT3 swordColor = enabled ? rushSwordColor : DirectX::XMFLOAT3{ 0.0f, 0.8f, 1.0f };

    if (swordMeshComponent && swordMeshComponent->plusAlphaCBuffer)
    {
        auto& cpuColor = swordMeshComponent->plusAlphaCBuffer->data.cpuColor;
        cpuColor.x = swordColor.x;
        cpuColor.y = swordColor.y;
        cpuColor.z = swordColor.z;
    }

    trail.SetRushColorEnabled(enabled, rushSwordColor);
    activeGhostBaseColor = enabled ? rushSwordColor : swordGhostColor;
    activeGhostEdgeColor = enabled ? rushSwordColor : swordGhostColor;
}

bool Player::CanJustDodgeAgainst(const DirectX::XMFLOAT3& attackerPosition)
{
    DirectX::XMFLOAT3 toAttacker =
    {
        attackerPosition.x - GetPosition().x,
        0.0f,
        attackerPosition.z - GetPosition().z
    };
    const float attackerLength = std::sqrt(
        toAttacker.x * toAttacker.x + toAttacker.z * toAttacker.z);
    directionToAttackerDebug = attackerLength > 0.0001f
        ? DirectX::XMFLOAT3{ toAttacker.x / attackerLength, 0.0f, toAttacker.z / attackerLength }
    : DirectX::XMFLOAT3{};

    if (!dodgeWorldDirectionSnapshotValid || attackerLength <= 0.0001f)
    {
        justDodgeDirectionDotDebug = 0.0f;
        justDodgeDirectionResultDebug = false;
        return false;
    }

    justDodgeDirectionDotDebug =
        dodgeWorldDirectionSnapshot.x * directionToAttackerDebug.x +
        dodgeWorldDirectionSnapshot.z * directionToAttackerDebug.z;
    justDodgeDirectionResultDebug = justDodgeDirectionDotDebug <= justDodgeMaxTowardAttackerDot;
    return justDodgeDirectionResultDebug;
}




void Player::AcquireAttackTarget()
{
    attackTarget.reset();
    float nearestDistance = FLT_MAX;
    const auto enemies = GetOwnerScene()->GetActorManager()->GetActorsOfType<Enemy>();
    for (const auto& enemy : enemies)
    {
        if (!enemy || enemy->IsDefeated() || enemy->IsPendingKill())
            continue;

        DirectX::XMFLOAT3 delta = MathHelper::Subtract(enemy->GetPosition(), GetPosition());
        delta.y = 0.0f;
        const float distance = MathHelper::Length(delta);
        if (distance > attackAssistMaxDistance)
            continue;

        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            attackTarget = enemy;
        }
    }

    attackRotationStartYaw = GetEulerRotation().y;
    attackRotationTracking = !attackTarget.expired();


}

void Player::UpdateAttackTargetRotation(float deltaTime)
{
    if (!attackRotationTracking || hitBox)
        return;

    const auto target = attackTarget.lock();
    if (!target || target->IsDefeated())
    {
        attackRotationTracking = false;
        return;
    }

    DirectX::XMFLOAT3 direction = MathHelper::Subtract(target->GetPosition(), GetPosition());
    direction.y = 0.0f;
    if (MathHelper::Length(direction) <= FLT_EPSILON)
        return;

    const float targetYaw = DirectX::XMConvertToDegrees(std::atan2f(direction.x, direction.z));
    const float correctionFromStart = std::clamp(MathHelper::NormalizeAngleDegrees(targetYaw - attackRotationStartYaw),
        -attackRotationMaxCorrectionDegrees,
        attackRotationMaxCorrectionDegrees);
    const float limitedTargetYaw = attackRotationStartYaw + correctionFromStart;

    DirectX::XMFLOAT3 rotation = GetEulerRotation();
    const float remainingYaw = MathHelper::NormalizeAngleDegrees(limitedTargetYaw - rotation.y);
    const float maxStep = attackRotationSpeedDegrees * deltaTime;
    rotation.y += std::clamp(remainingYaw, -maxStep, maxStep);
    SetEulerRotation(rotation);
}

void Player::ClearAttackTarget()
{
    attackRotationTracking = false;
    attackTarget.reset();
}

bool Player::RebuildEyeClosePoseOverride()
{
    static const std::vector<std::string> eyelidBoneNames =
    {
        "R_eye_lid_upper_mid",
        "R_eye_lid_lower_mid",
        "L_eye_lid_upper_mid",
        "L_eye_lid_lower_mid"
    };

    const auto controller = GetBodyAnimationController();
    if (!controller)
        return false;

    const float preservedWeight = closeEyeWeight;
    if (!controller->ConfigureLocalPoseOverride(
        "Idle", closedEyePoseTime, eyelidBoneNames))
    {
        closeEyeWeight = 0.0f;
        return false;
    }

    controller->SetLocalPoseOverrideWeight(preservedWeight);
    return true;
}

void Player::ResetEyeCloseOverride()
{
    if (const auto controller = GetBodyAnimationController())
        controller->ClearLocalPoseOverride();

    closeEyeWeight = 0.0f;
    eyeCloseOverrideOwner = EyeCloseOverrideOwner::None;
    deathEyeCloseActive = false;
    deathEyeCloseStarted = false;
    deathVisualFadeStarted = false;
}

void Player::HandleAnimationPlaySE(const AnimationNotifyEvent& event) const
{
    if (event.parameter.empty())
        return;

    std::string soundName = event.parameter;
    if (event.parameter == "RushSword")
    {
        if (std::string(stateMachine_->GetStateName()) != "Rush")
            return;

        const auto* rushState = dynamic_cast<const PlayerRushState*>(stateMachine_->GetCurrentState());
        if (!rushState)
            return;

        const int comboIndex = rushState->GetComboIndex();
        if (comboIndex < 0)
            return;
        // ラッシュ時のSE
        soundName = "enemy_rush_damage3";
    }

    const std::string audioPath = "./Data/Sound/SE/" + soundName + ".wav";
    CoreAudio::PlayOneShot(audioPath, event.value);
}
// 剣の攻撃判定
DirectX::XMFLOAT3 Player::GetHitBoxPoint(
    const std::shared_ptr<SceneComponent>& component,
    const DirectX::XMFLOAT3& localOffset) const
{
    const DirectX::XMFLOAT3 base = component->GetComponentLocation();
    DirectX::XMVECTOR offset = DirectX::XMLoadFloat3(&localOffset);
    const DirectX::XMFLOAT4 rotation = component->GetComponentWorldTransform().GetRotation();
    offset = DirectX::XMVector3Rotate(offset, DirectX::XMLoadFloat4(&rotation));
    DirectX::XMFLOAT3 worldOffset{};
    DirectX::XMStoreFloat3(&worldOffset, offset);
    return { base.x + worldOffset.x, base.y + worldOffset.y, base.z + worldOffset.z };
}

// 入力処理をまとめる
void Player::CaptureActionRequest(float deltaTime)
{
    constexpr float dodgeInputBufferSeconds = 0.3f;
    constexpr float attackInputBufferSeconds = 0.5f;

    const std::string currentState = stateMachine_->GetStateName();
    if (currentState == "Damage" || currentState == "KnockBack" ||
        currentState == "DeathPending" || currentState == "Death")
    {
        if (bufferCommand.type != ActionType::None)
            ClearActionRequest("damage_or_death_state");
        return;
    }

    if (bufferCommand.type != ActionType::None)
    {
        bufferCommand.remainTime -= deltaTime;
        if (bufferCommand.remainTime <= 0.0f)
        {
            bufferCommand = {};
        }
    }

    if (InputSystem::GetInputState("Jump", InputStateMask::Trigger))
    {
        DecideLockOnDodgeDirection();
        StoreActionRequest(ActionType::Dodge, dodgeInputBufferSeconds);
    }
    if (InputSystem::GetInputState("Attack", InputStateMask::Trigger))
    {
        StoreActionRequest(ActionType::Attack, attackInputBufferSeconds);
    }
}


bool Player::StoreActionRequest(ActionType type, float remainTime)
{
    const int newPriority = GetActionPriority(type);
    const int currentPriority = GetActionPriority(bufferCommand.type);
    if (bufferCommand.type != ActionType::None && newPriority < currentPriority)
    {
        return false;
    }

    bufferCommand.type = type;
    bufferCommand.remainTime = remainTime;
    if (type == ActionType::Dodge)
    {
        bufferCommand.dodgeDirection = dodgeDirection;
        bufferCommand.dodgeWorldDirection = dodgeWorldDirection;
        bufferCommand.useDodgeWorldDirection = useDodgeWorldDirection;
    }

    return true;
}

void Player::BeginKnockBackMovement()
{
    ClearActionRequest("knockback_enter");
    knockBackInitialSpeed = (std::max)(0.0f, knockBackInitialSpeed);
    knockBackDuration = (std::max)(0.01f, knockBackDuration);
    knockBackElapsed = 0.0f;
    knockBackActive = true;
    knockBackForcedMoveActive = true;
    characterMovementComponent->SetFixedSpeed(0.0f);
    characterMovementComponent->SetInputMagnitude(0.0f);
    characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->AddForcedMove(knockBackDirection, knockBackInitialSpeed, knockBackDuration);
    DirectX::XMFLOAT3 rotation = MathHelper::Multiply(knockBackDirection, -1.0f);
    rotationComponent->SetDirection(rotation);
}

void Player::UpdateKnockBackMovement(float deltaTime)
{
    knockBackElapsed = (std::min)(
        knockBackElapsed + (std::max)(0.0f, deltaTime), knockBackDuration);
    if (knockBackForcedMoveActive && knockBackElapsed >= knockBackDuration)
        StopKnockBackForcedMove();
}

void Player::StopKnockBackForcedMove()
{
    knockBackForcedMoveActive = false;
    characterMovementComponent->AddForcedMove({ 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f);
}

void Player::EndKnockBackMovement()
{
    knockBackActive = false;
    StopKnockBackForcedMove();
    characterMovementComponent->ResetFixedSpeed();
}

// 動作更新処理
void Player::UpdateMovement()
{
    const std::string currentState = GetStateMachine()->GetStateName();
    const bool suppressMovementInput = currentState == "Damage" || currentState == "KnockBack" || currentState == "DeathPending" || currentState == "Death";

    if (currentState == "DeathPending" || currentState == "Death")
    {
        characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
        characterMovementComponent->SetInputMagnitude(0.0f);
        return;
    }

    auto intent = inputComponent->GetIntent();
    if (suppressMovementInput)
        intent.leftMove = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 moveDir = { 0,0,0 };
    bool focus = InputSystem::GetInputState("LockOn", InputStateMask::Press);


    if (auto camera = dynamic_cast<DarkCameraActor*>(GetOwnerScene()->GetActiveCamera()))
    {

        GameScene::LockOnTargetSelectionResult selectionResult =
            GameScene::LockOnTargetSelectionResult::NoCandidate;
        if (focus)
        {
            if (auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene()))
                selectionResult = gameScene->UpdateLockOnTargetSelection();
        }

        if (!focus || selectionResult == GameScene::LockOnTargetSelectionResult::Invalidated)
        {
            if (!camera->IsBlendingToFocus())
            { // Focusへの回転中は、ボタンを離しても完了まで待つ
                camera->SetRequestMode(DarkCameraActor::CameraMode::TPS);
            }
        }
        else if (selectionResult == GameScene::LockOnTargetSelectionResult::Selected)
        {
            camera->SetRequestMode(DarkCameraActor::CameraMode::LockOn);
        }
        else
        {
            camera->SetRequestMode(DarkCameraActor::CameraMode::Focus);
        }


        // カメラモードに応じたプレイヤー移動処理
        // 左スティック入力
        float rawStickX = intent.leftMove.x;
        float rawStickZ = intent.leftMove.z;
        GetBodyAnimationController()->SetBlendDebugRawInput(
            rawStickX, rawStickZ);
        // Rotation用
        float stickX = rawStickX;
        float stickZ = rawStickZ;
        // Movement用
        float moveStickX = rawStickX;
        float moveStickZ = rawStickZ;
        const float rawLength = sqrtf(
            moveStickX * moveStickX + moveStickZ * moveStickZ);
        const float deadZone = 0.18f;
        float processedMagnitude = 0.0f;
        if (rawLength < deadZone)
        {
            moveStickX = 0.0f;
            moveStickZ = 0.0f;
            characterMovementComponent->SetInputMagnitude(0.0f);
        }
        else
        {
            // 円形入力として長さを1.0に制限し、斜め入力で速度が増えないようにする。
            const float clampedLength = std::clamp(rawLength, 0.0f, 1.0f);
            float newLength =
                (clampedLength - deadZone) / (1.0f - deadZone);
            processedMagnitude = newLength;
            // 好みでコメントアウトを切り替え
            moveStickX = moveStickX / rawLength * newLength;
            moveStickZ = moveStickZ / rawLength * newLength;
            characterMovementComponent->SetInputMagnitude(newLength);
        }

        const auto calculateDirectionalSpeedScale = [&](const float x, const float z)
            {
                const float forwardWeight = z > 0.0f ? z : 0.0f;
                const float backwardWeight = z < 0.0f ? -z : 0.0f;
                const float sideWeight = std::fabs(x);
                const float totalWeight =
                    forwardWeight + sideWeight + backwardWeight;

                if (totalWeight <= FLT_EPSILON)
                    return 1.0f;

                return
                    (forwardWeight * forwardSpeedScale +
                        sideWeight * sideSpeedScale +
                        backwardWeight * backwardSpeedScale) /
                    totalWeight;
            };

        float directionSpeedScale = 1.0f;

        switch (camera->GetMovementMode())
        {
        case DarkCameraActor::CameraMode::TPS:
        {
            auto camForward = camera->CameraForwardXZ();
            auto camRight = camera->CameraRightXZ();

            // カメラ基準の移動方向
            moveDir.x = camForward.x * moveStickZ + camRight.x * moveStickX;
            moveDir.z = camForward.z * moveStickZ + camRight.z * moveStickX;

            // 回転はすぐに向きを変えてほしいため
            const std::string& state = GetStateMachine()->GetStateName();
            if (state != "Dodge" && state != "Attack" &&
                state != "Damage" && state != "DeathPending" && state != "Death" && state != "KnockBack")
            {// 回避ではないかつ攻撃でないときは
                rotationComponent->SetDirection(moveDir);
            }
            float normalizeSpeed = characterMovementComponent->GetCurrentInputNormalizeSpeed();
            GetBodyAnimationController()->SetBlendInput(
                0.0f, processedMagnitude > 0.0f ? 1.0f : 0.0f, normalizeSpeed);
        }
        break;
        case DarkCameraActor::CameraMode::Focus:
        {
            // 最初に決定したfocus 方向
            DirectX::XMFLOAT3 forward = focusDirection;
            forward = MathHelper::Normalize(forward);
            DirectX::XMFLOAT3 up = { 0.0f,1.0f,0.0f };
            DirectX::XMFLOAT3 right = MathHelper::Normalize(MathHelper::Cross(up, forward));
            moveDir.x = forward.x * moveStickZ + right.x * moveStickX;
            moveDir.z = forward.z * moveStickZ + right.z * moveStickX;
            directionSpeedScale = calculateDirectionalSpeedScale(
                moveStickX, moveStickZ);
            float normalizeSpeed = characterMovementComponent->GetCurrentInputNormalizeSpeed();
            GetBodyAnimationController()->SetBlendInput(
                moveStickX, moveStickZ, normalizeSpeed);
            if (currentState != "Attack" && currentState != "KnockBack")
                rotationComponent->SetDirection(forward);
            break;
        }
        case DarkCameraActor::CameraMode::LockOn:
        {
            // 最初に決定したfocus 方向
            if (auto target = camera->GetEnemyHead())
            {
                XMFLOAT3 playerPos = GetPosition();
                XMFLOAT3 enemyPos = target->GetComponentLocation();
                XMFLOAT3 forward = MathHelper::Subtract(enemyPos, playerPos);
                forward.y = 0.0f;
                forward = MathHelper::Normalize(forward);
                DirectX::XMFLOAT3 up = { 0.0f,1.0f,0.0f };
                DirectX::XMFLOAT3 right = MathHelper::Normalize(MathHelper::Cross(up, forward));
                moveDir.x = forward.x * moveStickZ + right.x * moveStickX;
                moveDir.z = forward.z * moveStickZ + right.z * moveStickX;
                directionSpeedScale = calculateDirectionalSpeedScale(
                    moveStickX, moveStickZ);
                float normalizeSpeed = characterMovementComponent->GetCurrentInputNormalizeSpeed();
                GetBodyAnimationController()->SetBlendInput(
                    moveStickX, moveStickZ, normalizeSpeed);
                if (currentState != "Attack" && currentState != "KnockBack")
                    rotationComponent->SetDirection(forward);
            }
            break;
        }
        }
        characterMovementComponent->SetMoveSpeedScale(directionSpeedScale);
        characterMovementComponent->SetMoveDirection(moveDir);
    }
}

// 有効なMotionWarpを追加速度として適用し、移動と衝突判定を更新する。
void Player::UpdatePlayerMovementWithMotionWarp(float deltaTime)
{
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
}


// 剣の残像を生成し、透明度と描画パラメータを更新する。
void Player::UpdateSwordGhosts(const float deltaTime)
{
    using namespace DirectX;
    if (showTrail)
    {
        XMFLOAT4X4 currentWorld = swordMeshComponent->GetComponentWorldTransform().ToWorldTransform();

        if (!isPrevSwordWorldValid)
        {
            prevSwordWorld = currentWorld;
            isPrevSwordWorldValid = true;
        }

        XMFLOAT3 prevPos =
        {
            prevSwordWorld._41,
            prevSwordWorld._42,
            prevSwordWorld._43
        };

        XMFLOAT3 currentPos =
        {
            currentWorld._41,
            currentWorld._42,
            currentWorld._43
        };

        swordGhostElapsedTime += deltaTime;
        while (swordGhostElapsedTime >= ghostInterval)
        {
            swordGhostElapsedTime -= ghostInterval;

            float t = 1.0f - swordGhostElapsedTime / deltaTime;

            XMFLOAT3 pos = MathHelper::Lerp(prevPos, currentPos, t);

            XMFLOAT4X4 world = currentWorld;
            world._41 = pos.x;
            world._42 = pos.y;
            world._43 = pos.z;

            ghosts[swordGhostIndex].world = world;
            ghosts[swordGhostIndex].alpha = 1.0f;
            ghosts[swordGhostIndex].isVisible = true;

            swordGhostIndex = (swordGhostIndex + 1) % ghosts.size();
        }
        // 前回の姿勢を保存する
        prevSwordWorld = currentWorld;
    }

    // 剣の残像用の剣のメッシュコンポーネント
    for (auto& ghost : ghosts)
    {
        ghost.alpha -= deltaTime / ghostFadeTime;

        if (ghost.alpha <= 0)
        {
            ghost.alpha = 0;
            ghost.isVisible = false;
        }

        if (!ghost.isVisible)
            continue;
        if (ghost.swordMeshComp)
        {
            ghost.swordMeshComp->plusAlphaCBuffer->data.emissionPower = swordGhostEmissive;
            ghost.swordMeshComp->plusAlphaCBuffer->data.cpuColor = { activeGhostBaseColor.x,activeGhostBaseColor.y,activeGhostBaseColor.z, ghost.alpha };
            ghost.swordMeshComp->plusAlphaCBuffer->data.effectParameters.edgeColor = { activeGhostEdgeColor.x,activeGhostEdgeColor.y,activeGhostEdgeColor.z,1.0f };
            ghost.swordMeshComp->plusAlphaCBuffer->data.effectParameters.innerColor = { ghostInnerColor.x,ghostInnerColor.y,ghostInnerColor.z,1.0f };
            ghost.swordMeshComp->plusAlphaCBuffer->data.effectParameters.edgeWidth = ghostEdgeWidth;
        }
    }

}

// 現在の発光状態を反映し、指定された座標で軌跡を更新する。
void Player::UpdateSwordVisuals(const float deltaTime, const DirectX::XMFLOAT3* swordRootPos, const DirectX::XMFLOAT3* swordTipPos, const bool updateTrail)
{
    if (swordMeshComponent && swordMeshComponent->plusAlphaCBuffer)
        swordMeshComponent->plusAlphaCBuffer->data.emissionPower = swordEmissivePower;

    if (!updateTrail || !swordRootPos || !swordTipPos)
        return;

    trail.UpdateTrail(deltaTime);
    if (showTrail)
        trail.trailPoints.push_back({ *swordTipPos, *swordRootPos, trailRemainTime });
}

// 特殊演出ではアニメーション時刻から剣の表示状態を再評価する。
void Player::UpdateWeaponVisualPresentation(const float deltaTime, const bool updateTrail)
{
    const auto controller = GetBodyAnimationController();
    bool evaluatedShowTrail = false;
    float evaluatedEmissivePower = 0.0f;
    bool hasShowTrailState = false;
    bool hasShowEmissiveState = false;
    bool hasWeaponVisualState = false;
    if (controller != nullptr)
    {
        hasWeaponVisualState = controller->EvaluateWeaponVisualState(evaluatedShowTrail, evaluatedEmissivePower, hasShowTrailState, hasShowEmissiveState);
    }

    // 剣の演出へ反映するかどうか
    showTrail = hasWeaponVisualState && evaluatedShowTrail;
    swordEmissivePower = hasWeaponVisualState ? evaluatedEmissivePower : 0.0f;

    if (!hasWeaponVisualState)
    {
        UpdateSwordVisuals(deltaTime, nullptr, nullptr, false);
        trail.Clear();
        return;
    }
    if (!swordRootComponent || !swordTipComponent)
    {
        UpdateSwordVisuals(deltaTime, nullptr, nullptr, false);
        return;
    }

    const DirectX::XMFLOAT3 swordRootPos = swordRootComponent->GetComponentLocation();
    const DirectX::XMFLOAT3 swordTipPos = swordTipComponent->GetComponentLocation();
    UpdateSwordVisuals(deltaTime, &swordRootPos, &swordTipPos, updateTrail);
}

void Player::UpdateRushPromptUI()
{
    if (!rushGuideImageComponent || !rushButtonImageComponent ||
        !rushWordImageComponent)
    {
        return;
    }

    const bool visible = CanShowInitialRushGuide() || CanShowRushComboGuide();
    if (!visible)
    {
        rushPromptAlpha = 0.0f;
        rushPromptAnimationPhase = RushPromptAnimationPhase::Hidden;
        rushPromptAnimationTimer = 0.0f;
        rushPromptWasVisible = false;
        rushGuideImageComponent->SetVisible(false);
        rushButtonImageComponent->SetVisible(false);
        rushWordImageComponent->SetVisible(false);
        rushButtonImageComponent->SetScale({
            rushButtonBaseScale.x * 0.8f,
            rushButtonBaseScale.y * 0.8f });
        rushWordImageComponent->SetScale({
            rushWordScale.x * 0.9f,
            rushWordScale.y * 0.9f });
        return;
    }

    const bool becameVisible = !rushPromptWasVisible;
    if (becameVisible)
    {
        rushPromptWasVisible = true;
        rushPromptAnimationPhase = RushPromptAnimationPhase::AppearGrow;
        rushPromptAnimationTimer = 0.0f;
        rushPromptAlpha = 0.0f;
    }

    const float uiDeltaTime = Time::UnscaledDeltaTime();
    if (rushPromptFadeInDuration <= FLT_EPSILON)
    {
        rushPromptAlpha = 1.0f;
    }
    else
    {
        rushPromptAlpha = std::clamp(
            rushPromptAlpha + uiDeltaTime / rushPromptFadeInDuration,
            0.0f, 1.0f);
    }

    if (!becameVisible)
        rushPromptAnimationTimer += uiDeltaTime;

    float buttonAnimationScale = 1.0f;
    float wordAnimationScale = 1.0f;
    switch (rushPromptAnimationPhase)
    {
    case RushPromptAnimationPhase::Hidden:
        rushPromptAnimationPhase = RushPromptAnimationPhase::AppearGrow;
        rushPromptAnimationTimer = 0.0f;
        buttonAnimationScale = 0.8f;
        wordAnimationScale = 0.9f;
        break;

    case RushPromptAnimationPhase::AppearGrow:
    {
        constexpr float duration = 0.10f;
        const float time = std::clamp(rushPromptAnimationTimer, 0.0f, duration);
        buttonAnimationScale = std::clamp(
            Easing::OutBack(time, duration, 1.0f, 1.15f, 0.8f),
            0.8f, 1.15f);
        wordAnimationScale = std::clamp(
            Easing::OutBack(time, duration, 1.0f, 1.08f, 0.9f),
            0.9f, 1.08f);
        if (rushPromptAnimationTimer >= duration)
        {
            rushPromptAnimationPhase = RushPromptAnimationPhase::AppearSettle;
            rushPromptAnimationTimer = 0.0f;
            buttonAnimationScale = 1.15f;
            wordAnimationScale = 1.08f;
        }
        break;
    }

    case RushPromptAnimationPhase::AppearSettle:
    {
        constexpr float duration = 0.08f;
        const float time = std::clamp(rushPromptAnimationTimer, 0.0f, duration);
        buttonAnimationScale = Easing::OutQuad(time, duration, 1.0f, 1.15f);
        wordAnimationScale = Easing::OutQuad(time, duration, 1.0f, 1.08f);
        if (rushPromptAnimationTimer >= duration)
        {
            rushPromptAnimationPhase = RushPromptAnimationPhase::PulseGrow;
            rushPromptAnimationTimer = 0.0f;
            buttonAnimationScale = 1.0f;
            wordAnimationScale = 1.0f;
        }
        break;
    }

    case RushPromptAnimationPhase::PulseGrow:
    {
        constexpr float duration = 0.25f;
        const float time = std::clamp(rushPromptAnimationTimer, 0.0f, duration);
        buttonAnimationScale = Easing::InOutSine(time, duration, 1.12f, 1.0f);
        wordAnimationScale = Easing::InOutSine(time, duration, 1.05f, 1.0f);
        if (rushPromptAnimationTimer >= duration)
        {
            rushPromptAnimationPhase = RushPromptAnimationPhase::PulseReturn;
            rushPromptAnimationTimer = 0.0f;
            buttonAnimationScale = 1.12f;
            wordAnimationScale = 1.05f;
        }
        break;
    }

    case RushPromptAnimationPhase::PulseReturn:
    {
        constexpr float duration = 0.35f;
        const float time = std::clamp(rushPromptAnimationTimer, 0.0f, duration);
        buttonAnimationScale = Easing::OutQuad(time, duration, 1.0f, 1.12f);
        wordAnimationScale = Easing::OutQuad(time, duration, 1.0f, 1.05f);
        if (rushPromptAnimationTimer >= duration)
        {
            rushPromptAnimationPhase = RushPromptAnimationPhase::PulseGrow;
            rushPromptAnimationTimer = 0.0f;
            buttonAnimationScale = 1.0f;
            wordAnimationScale = 1.0f;
        }
        break;
    }
    }

    const DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, rushPromptAlpha };

    rushGuideImageComponent->SetWorldPosition(rushGuidePosition);
    rushGuideImageComponent->SetSize(rushGuideSize);
    rushGuideImageComponent->SetScale(rushGuideScale);
    rushGuideImageComponent->SetColor(color);
    rushGuideImageComponent->SetVisible(true);

    rushButtonImageComponent->SetWorldPosition(rushButtonPosition);
    rushButtonImageComponent->SetSize(rushButtonSize);
    rushButtonImageComponent->SetScale({
        rushButtonBaseScale.x * buttonAnimationScale,
        rushButtonBaseScale.y * buttonAnimationScale });
    rushButtonImageComponent->SetColor(color);
    rushButtonImageComponent->SetVisible(true);

    rushWordImageComponent->SetWorldPosition(rushWordPosition);
    rushWordImageComponent->SetSize(rushWordSize);
    rushWordImageComponent->SetScale({
        rushWordScale.x * wordAnimationScale,
        rushWordScale.y * wordAnimationScale });
    rushWordImageComponent->SetColor(color);
    rushWordImageComponent->SetVisible(true);
}

void Player::UpdateLockOnGuideUI()
{
    using namespace DirectX;
    if (const auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene());
        gameScene && gameScene->IsBossPhaseTransitionActive())
    {
        HideAndResetLockOnGuideUI();
        return;
    }
    if (!lockOnGuideArrowImageComponent || !lockOnGuideButtonImageComponent ||
        !IsBossBattle() || GetHp() <= 0 || IsInWinState() ||
        IsRushActive() || IsJustDodgePresentationActive())
    {
        HideAndResetLockOnGuideUI();
        return;
    }

    auto boss = GetOwnerScene()->GetActorManager()->GetActorOfType<GruxEnemy>();
    auto camera = dynamic_cast<DarkCameraActor*>(GetOwnerScene()->GetActiveCamera());
    if (!boss || boss->IsDead() || !camera ||
        camera->GetMovementMode() == DarkCameraActor::CameraMode::LockOn)
    {
        HideAndResetLockOnGuideUI();
        return;
    }

    const auto target = boss->GetCameraTargetComponent();
    if (!target)
    {
        HideAndResetLockOnGuideUI();
        return;
    }
    const auto projection = camera->ProjectWorldPositionForUI(
        target->GetComponentLocation());
    float vx = 0.0f, vy = 0.0f, vw = 0.0f, vh = 0.0f;
    Graphics::GetViewport(vx, vy, vw, vh);
    if (!projection.valid || vw <= 1.0f || vh <= 1.0f || projection.insideViewport)
    {
        HideAndResetLockOnGuideUI();
        return;
    }

    const float dt = Time::UnscaledDeltaTime();
    lockOnGuideOffscreenElapsed += dt;
    if (!lockOnGuideVisible && lockOnGuideOffscreenElapsed < lockOnGuideDelay)
        return;

    XMFLOAT2 center{ vx + vw * 0.5f, vy + vh * 0.5f };
    XMFLOAT2 direction{
        projection.screenPosition.x - center.x,
        projection.screenPosition.y - center.y };
    if (!projection.inFront)
    {
        direction.x = -direction.x;
        direction.y = -direction.y;
    }
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= FLT_EPSILON)
        direction = { 0.0f, 1.0f };
    else
    {
        direction.x /= length;
        direction.y /= length;
    }

    const float uiScale = (std::min)(vw / 1920.0f, vh / 1080.0f);
    const float marginX = lockOnGuideEdgeMargin * uiScale;
    const float marginY = lockOnGuideEdgeMargin * uiScale;
    const float left = vx + marginX;
    const float right = vx + vw - marginX;
    const float top = vy + marginY;
    const float bottom = vy + vh - marginY;
    float t = FLT_MAX;
    if (direction.x > FLT_EPSILON) t = (std::min)(t, (right - center.x) / direction.x);
    if (direction.x < -FLT_EPSILON) t = (std::min)(t, (left - center.x) / direction.x);
    if (direction.y > FLT_EPSILON) t = (std::min)(t, (bottom - center.y) / direction.y);
    if (direction.y < -FLT_EPSILON) t = (std::min)(t, (top - center.y) / direction.y);
    if (!std::isfinite(t) || t <= 0.0f)
    {
        HideAndResetLockOnGuideUI();
        return;
    }

    const XMFLOAT2 arrowScreen{ center.x + direction.x * t, center.y + direction.y * t };
    const XMFLOAT2 arrowUi = ConvertScreenToUI(arrowScreen);
    const XMFLOAT2 buttonUi = ConvertScreenToUI({
        arrowScreen.x - direction.x * lockOnGuideButtonOffset * uiScale,
        arrowScreen.y - direction.y * lockOnGuideButtonOffset * uiScale });

    lockOnGuideVisible = true;
    lockOnGuidePulseElapsed = std::fmod(
        lockOnGuidePulseElapsed + dt, (std::max)(lockOnGuidePulsePeriod, 0.01f));
    const float half = (std::max)(lockOnGuidePulsePeriod * 0.5f, 0.01f);
    const float phase = lockOnGuidePulseElapsed;
    const float pulseScale = phase < half
        ? Easing::InOutSine(phase, half, 1.0f, lockOnGuidePulseMinScale)
        : Easing::InOutSine(phase - half, half, lockOnGuidePulseMinScale, 1.0f);

    lockOnGuideArrowImageComponent->SetWorldPosition(arrowUi);
    lockOnGuideArrowImageComponent->SetSize(lockOnGuideArrowSize);
    lockOnGuideArrowImageComponent->SetScale(lockOnGuideArrowBaseScale);
    lockOnGuideArrowImageComponent->SetWorldAngleDegree(
        XMConvertToDegrees(std::atan2(direction.y, direction.x)) +
        lockOnGuideArrowRotationOffset);
    lockOnGuideArrowImageComponent->SetVisible(true);

    lockOnGuideButtonImageComponent->SetWorldPosition(buttonUi);
    lockOnGuideButtonImageComponent->SetSize(lockOnGuideButtonSize);
    lockOnGuideButtonImageComponent->SetScale({
        lockOnGuideButtonBaseScale.x * pulseScale,
        lockOnGuideButtonBaseScale.y * pulseScale });
    lockOnGuideButtonImageComponent->SetVisible(true);
}

void Player::HideAndResetLockOnGuideUI()
{
    lockOnGuideOffscreenElapsed = 0.0f;
    lockOnGuidePulseElapsed = 0.0f;
    lockOnGuideVisible = false;
    if (lockOnGuideArrowImageComponent)
    {
        lockOnGuideArrowImageComponent->SetVisible(false);
        lockOnGuideArrowImageComponent->SetScale(lockOnGuideArrowBaseScale);
    }
    if (lockOnGuideButtonImageComponent)
    {
        lockOnGuideButtonImageComponent->SetVisible(false);
        lockOnGuideButtonImageComponent->SetScale(lockOnGuideButtonBaseScale);
    }
}

void Player::InitializeOperationGuideUI()
{
    const auto uiManager = GetOwnerScene()->GetUIManager();
    constexpr std::array<const char*, static_cast<size_t>(OperationGuideItem::Count)> texturePaths =
    {
        "./Data/Textures/UI/operate_L.png", "./Data/Textures/UI/operate_LT.png",
        "./Data/Textures/UI/operate_Y.png", "./Data/Textures/UI/operate_X.png",
        "./Data/Textures/UI/operate_R.png",
    };
    constexpr std::array<const char*, static_cast<size_t>(OperationGuideItem::Count)> names =
    {
        "OperationGuideL", "OperationGuideLT", "OperationGuideY", "OperationGuideX", "OperationGuideR",
    };
    constexpr std::array<DirectX::XMFLOAT2, static_cast<size_t>(OperationGuideItem::Count)> positions =
    {
        DirectX::XMFLOAT2{ 844.7f, 1015.0f }, DirectX::XMFLOAT2{ 926.6f, 1013.0f },
        DirectX::XMFLOAT2{ 999.5f, 1011.0f }, DirectX::XMFLOAT2{ 1068.6f, 1010.0f },
        DirectX::XMFLOAT2{ 1143.2f, 1013.0f },
    };
    constexpr std::array<DirectX::XMFLOAT2, static_cast<size_t>(OperationGuideItem::Count)> sizes =
    {
        DirectX::XMFLOAT2{ 178.0f, 258.0f }, DirectX::XMFLOAT2{ 345.0f, 270.0f },
        DirectX::XMFLOAT2{ 184.0f, 279.0f }, DirectX::XMFLOAT2{ 182.0f, 271.0f },
        DirectX::XMFLOAT2{ 233.0f, 249.0f },
    };
    for (size_t index = 0; index < operationGuideItems.size(); ++index)
    {
        auto& item = operationGuideItems[index];
        item.position = positions[index];
        item.baseScale = { 0.35f, 0.35f };
        item.visible = index == static_cast<size_t>(OperationGuideItem::L) || index == static_cast<size_t>(OperationGuideItem::R);
        item.learned = false;
        item.animationTimer = 0.0f;
        item.image = std::make_shared<UIImageComponent>(texturePaths[index], names[index]);
        item.image->SetWorldPosition(item.position);
        item.image->SetSize(sizes[index]);
        item.image->SetScale(item.baseScale);
        item.image->SetPivot({ 0.5f, 0.5f });
        item.image->SetVisible(false);
        uiManager->Add(item.image);
    }
    RefreshOperationGuideUIVisibility();
}

void Player::UpdateOperationGuideUI()
{
    const float deltaTime = (std::max)(0.0f, Time::UnscaledDeltaTime());
    const float amplitude = std::clamp(operationGuidePulseAmplitude, 0.0f, 1.0f);
    const float speed = (std::max)(0.0f, operationGuidePulseSpeed);
    const auto stickWasUsed = [](const DirectX::XMFLOAT2& stick)
        {
            constexpr float threshold = 0.20f;
            return stick.x * stick.x + stick.y * stick.y >= threshold * threshold;
        };
    if (operationGuideGameplayHudVisible && operationGuideEventVisible)
    {
        if (IsOperationGuideItemVisible(OperationGuideItem::L) &&
            stickWasUsed(InputSystem::GetLeftStick()))
            SetOperationGuideItemLearned(OperationGuideItem::L);
        if (IsOperationGuideItemVisible(OperationGuideItem::R) &&
            stickWasUsed(InputSystem::GetRightStick()))
            SetOperationGuideItemLearned(OperationGuideItem::R);
    }
    for (auto& item : operationGuideItems)
    {
        if (!item.image) continue;
        item.image->SetWorldPosition(item.position);
        float pulseScale = 1.0f;
        if (item.visible && !item.learned)
        {
            item.animationTimer += deltaTime;
            pulseScale += amplitude * 0.5f * (std::sin(item.animationTimer * speed) + 1.0f);
        }
        else
        {
            item.animationTimer = 0.0f;
        }
        item.image->SetScale({ item.baseScale.x * pulseScale, item.baseScale.y * pulseScale });
    }
    RefreshOperationGuideUIVisibility();
}

// Hpバーの更新
void Player::UpdateHpBarUI()
{
    // Player HP UI uses unscaled time so HitStop / Slow do not pause its animations.
    const float currentHp = static_cast<float>((std::max)(hp, 0));
    const float uiDeltaTime = Time::UnscaledDeltaTime();
    float displayedHp = currentHp;
    if (hpRecoveryUiActive)
    {
        hpRecoveryUiElapsed += uiDeltaTime;
        const float t = hpRecoveryUiDuration > FLT_EPSILON
            ? std::clamp(hpRecoveryUiElapsed / hpRecoveryUiDuration, 0.0f, 1.0f)
            : 1.0f;
        displayedHp = std::lerp(hpRecoveryUiStart, currentHp, t);
        delayedHp = displayedHp;
        delayedHpDelayTimer = 0.0f;
        if (t >= 1.0f)
        {
            hpRecoveryUiActive = false;
            displayedHp = currentHp;
            delayedHp = currentHp;
        }
    }
    else if (delayedHp > currentHp)
    {
        if (delayedHpDelayTimer > 0.0f)
        {
            delayedHpDelayTimer = (std::max)(0.0f, delayedHpDelayTimer - uiDeltaTime);
        }
        else
        {
            delayedHp = (std::max)(currentHp, delayedHp - delayedHpFollowSpeed * uiDeltaTime);
        }
    }
    else if (delayedHp < currentHp)
    {
        delayedHp = currentHp;
        delayedHpDelayTimer = 0.0f;
    }

    if (hpCurrentFillUiComponent && hpDelayedFillUiComponent)
    {
        const float maximumHp = static_cast<float>(maxHp);
        hpCurrentFillUiComponent->SetValue(displayedHp, maximumHp);
        hpDelayedFillUiComponent->SetValue(delayedHp, maximumHp);
    }
}

void Player::RefreshOperationGuideUIVisibility() const
{
    for (auto& item : operationGuideItems)
    {
        if (item.image)
        {
            item.image->SetVisible(operationGuideGameplayHudVisible && operationGuideEventVisible && hp > 0 && item.visible);
        }
    }
}

float Player::GetRushDamageMultiplier() const
{
    if (!stateMachine_ || std::string(stateMachine_->GetStateName()) != "Rush")
        return 1.0f;

    const auto* rushState =
        dynamic_cast<const PlayerRushState*>(stateMachine_->GetCurrentState());
    if (!rushState)
        return 1.0f;

    const int attackNumber = rushState->GetComboIndex() + 1;
    const int rushAttackCount = GetMaxRushAttackCount();
    return attackNumber == rushAttackCount
        ? (std::max)(0.0f, finalRushDamageMultiplier)
        : (std::max)(0.0f, rushDamageMultiplier);
}

int Player::GetCurrentAttackDamage() const
{
    const int baseDamage = (std::max)(0, normalAttackDamage);
    if (!stateMachine_ || std::string(stateMachine_->GetStateName()) != "Rush")
        return baseDamage;

    return static_cast<int>(std::lround(
        static_cast<float>(baseDamage) * GetRushDamageMultiplier()));
}
// 回避の方向を決定する処理
void Player::DecideLockOnDodgeDirection()
{
    auto camera = dynamic_cast<DarkCameraActor*>(GetOwnerScene()->GetActiveCamera());
    if (!camera)
        return;

    auto intent = inputComponent->GetIntent();

    float x = intent.leftMove.x;
    float z = intent.leftMove.z;

    dodgeWorldDirection = {};
    useDodgeWorldDirection = false;

    if (camera->GetMovementMode() == DarkCameraActor::CameraMode::TPS)
    {
        const float inputLength = sqrtf(x * x + z * z);
        if (inputLength <= 0.1f)
        {
            // Explicitly preserve the previous no-input behavior.
            dodgeDirection = DodgeDirection::Backward;
            return;
        }

        x /= inputLength;
        z /= inputLength;
        const DirectX::XMFLOAT3 cameraForward = camera->CameraForwardXZ();
        const DirectX::XMFLOAT3 cameraRight = camera->CameraRightXZ();
        dodgeWorldDirection =
        {
            cameraForward.x * z + cameraRight.x * x,
            0.0f,
            cameraForward.z * z + cameraRight.z * x
        };
        dodgeWorldDirection = MathHelper::Normalize(dodgeWorldDirection);
        dodgeDirection = DodgeDirection::Forward;
        useDodgeWorldDirection = true;
        return;
    }

    // 入力なしなら後ろ回避
    if (fabs(x) < 0.1f && fabs(z) < 0.1f)
    {
        dodgeDirection = DodgeDirection::Backward;
        return;
    }

    // 横入力の方が強い
    if (fabs(x) > fabs(z))
    {
        dodgeDirection = (x > 0.0f) ? DodgeDirection::Right : DodgeDirection::Left;
    }
    else
    {
        dodgeDirection = (z > 0.0f) ? DodgeDirection::Forward : DodgeDirection::Backward;
    }
}

// インタラクト対象検索
IInteractable* Player::FindInteractable() const
{
    IInteractable* best = nullptr;
    float bestDist = FLT_MAX;

    DirectX::XMFLOAT3 forward = GetForward();
    DirectX::XMFLOAT3 playerPos = GetPosition();

    for (auto actor : GetOwnerScene()->GetActorManager()->GetActorsOfType<InteractableActor>())
    {
        auto interactable = dynamic_cast<IInteractable*>(actor.get());
        if (!interactable)
            continue;

        DirectX::XMFLOAT3 interactablePos =
            MathHelper::Add(actor->GetPosition(), actor->GetInteractOffset());


        DirectX::XMFLOAT3 dir =
            MathHelper::Normalize(
                MathHelper::Subtract(interactablePos, playerPos)
            );


        float dot = MathHelper::Dot(forward, dir);

        float interactableRadian = actor->GetInteractRadian();

        if (dot < interactableRadian)
        {
            interactable->SetCanInteract(false);
            continue;
        }


        float dist = MathHelper::Distance(playerPos, interactablePos);


        // 範囲外なら無視
        if (dist > actor->GetInteractRange())
        {
            interactable->SetCanInteract(false);
            continue;
        }


        // 一番近いものを選択
        if (dist < bestDist)
        {
            bestDist = dist;
            best = interactable;
        }
    }


    // 最終的に選ばれたものだけtrue
    for (auto actor : GetOwnerScene()->GetActorManager()->GetActorsOfType<InteractableActor>())
    {
        auto interactable = dynamic_cast<IInteractable*>(actor.get());

        if (interactable)
        {
            interactable->SetCanInteract(interactable == best);
        }
    }

    return best;

}
void Player::UpdateLowHpEffects()
{
    const auto gameScene = dynamic_cast<GameScene*>(GetOwnerScene());
    const bool phase2Cinematic = gameScene && gameScene->IsBossPhaseTransitionActive();
    if (lowHpPresentationSuppressed || lowHpPresentationCinematicSuppressed || phase2Cinematic)
    {
        if (lowHpActive || heartbeatTimer > 0.0f || lowHpPulseTimer > 0.0f || lowHpPulseFlashAmount > 0.0f || (lowHpVignetteImageComponent && lowHpVignetteImageComponent->IsVisible()))
        {
            ResetLowHpEffects();
        }
        return;
    }

    const bool shouldBeLowHp = hp > 0 && hp <= lowHpThreshold;
    if (!shouldBeLowHp)
    {
        if (lowHpActive || heartbeatTimer > 0.0f || lowHpPulseTimer > 0.0f || lowHpPulseFlashAmount > 0.0f)
        {
            ResetLowHpEffects();
        }
        return;
    }

    if (!lowHpActive)
    {
        lowHpActive = true;
        heartbeatTimer = 0.0f;
        TriggerLowHpPulse();
    }

    if (!CoreAudio::GetSystemPaused())
    {
        const float unscaledDeltaTime = (std::max)(0.0f, Time::UnscaledDeltaTime());
        heartbeatTimer += unscaledDeltaTime;
        lowHpPulseTimer = (std::max)(0.0f, lowHpPulseTimer - unscaledDeltaTime);

        if (heartbeatInterval > FLT_EPSILON && heartbeatTimer >= heartbeatInterval)
        {
            heartbeatTimer = std::fmod(heartbeatTimer, heartbeatInterval);
            TriggerLowHpPulse();
        }
    }

    const float pulseLinear = lowHpPulseDuration > FLT_EPSILON
        ? std::clamp(lowHpPulseTimer / lowHpPulseDuration, 0.0f, 1.0f)
        : 0.0f;
    const float pulseAmount = pulseLinear * pulseLinear * (3.0f - 2.0f * pulseLinear);
    lowHpPulseFlashAmount = pulseAmount;

    if (lowHpVignetteImageComponent)
    {
        const float alpha = std::lerp(
            lowHpVignetteBaseAlpha, lowHpVignettePulseAlpha, pulseAmount);
        lowHpVignetteImageComponent->SetColor(CoreColor{ 1.0f, 0.12f, 0.08f, alpha });
        lowHpVignetteImageComponent->SetVisible(true);
    }
}

void Player::TriggerLowHpPulse()
{
    lowHpPulseTimer = (std::max)(0.0f, lowHpPulseDuration);
    lowHpPulseFlashAmount = lowHpPulseTimer > 0.0f ? 1.0f : 0.0f;

    if (!heartbeatSEPath.empty() && std::filesystem::exists(heartbeatSEPath))
    {
        CoreAudio::PlayOneShot(heartbeatSEPath, 0.3f);
    }
}

void Player::ResetLowHpEffects()
{
    lowHpActive = false;
    heartbeatTimer = 0.0f;
    lowHpPulseTimer = 0.0f;
    lowHpPulseFlashAmount = 0.0f;

    if (lowHpVignetteImageComponent)
    {
        lowHpVignetteImageComponent->SetColor(CoreColor{ 1.0f, 0.12f, 0.08f, 0.0f });
        lowHpVignetteImageComponent->SetVisible(false);
    }
}

void Player::StartDamageFlash()
{
    damageFlashTimer = (std::max)(0.0f, damageFlashDuration);
    ApplyDamageFlash(damageFlashTimer > 0.0f ? 1.0f : 0.0f);
}

void Player::ApplyDamageFlash(float flashAmount) const
{
    const auto applyToMesh = [this, flashAmount](
        const std::shared_ptr<SkeletalMeshComponent>& mesh)
        {
            if (!mesh)
                return;

            auto& constants = mesh->plusAlphaCBuffer->data;
            constants.cpuColor.x = damageFlashColor.x;
            constants.cpuColor.y = damageFlashColor.y;
            constants.cpuColor.z = damageFlashColor.z;
            constants.flashValue = flashAmount;
            constants.effectParameters.edgeWidth = damageFlashBodyTintStrength;
            constants.effectParameters.edgePower = damageFlashRimStrength;
        };

    applyToMesh(skeletalMeshComponent);
}

void Player::UpdateDamageFlash()
{
    if (damageFlashTimer > 0.0f)
    {
        damageFlashTimer = (std::max)(
            0.0f, damageFlashTimer - Time::UnscaledDeltaTime());
    }

    const float damageFlashAmount = damageFlashDuration > FLT_EPSILON
        ? std::clamp(damageFlashTimer / damageFlashDuration, 0.0f, 1.0f)
        : 0.0f;
    ApplyDamageFlash((std::max)(damageFlashAmount, lowHpPulseFlashAmount));
}

void Player::UpdatePlayerPoseGhost()
{
    const float unscaledDeltaTime = Time::UnscaledDeltaTime();
    const float lifetime = (std::max)(playerPoseGhostLifetime, FLT_EPSILON);
    for (auto& ghost : playerPoseGhosts)
    {
        if (!ghost.isVisible)
            continue;

        ghost.elapsedTime += unscaledDeltaTime;
        const float normalizedAge = std::clamp(
            ghost.elapsedTime / lifetime, 0.0f, 1.0f);
        ghost.alpha = std::clamp(playerPoseGhostInitialAlpha, 0.0f, 1.0f) *
            (1.0f - normalizedAge);
        if (ghost.renderConstantsComponent)
        {
            auto& constants = ghost.renderConstantsComponent->plusAlphaCBuffer->data;
            constants.emissionPower = playerPoseGhostEmissive;
            constants.cpuColor = {
                playerPoseGhostColor.x,
                playerPoseGhostColor.y,
                playerPoseGhostColor.z,
                ghost.alpha };
            constants.effectParameters.edgeColor = {
                playerPoseGhostEdgeColor.x,
                playerPoseGhostEdgeColor.y,
                playerPoseGhostEdgeColor.z,
                1.0f };
            constants.effectParameters.innerColor = {
                playerPoseGhostInnerColor.x,
                playerPoseGhostInnerColor.y,
                playerPoseGhostInnerColor.z,
                1.0f };
            constants.effectParameters.edgeWidth = playerPoseGhostEdgeWidth;
        }

        if (normalizedAge >= 1.0f)
        {
            ghost.nodes.clear();
            ghost.alpha = 0.0f;
            ghost.elapsedTime = 0.0f;
            ghost.isVisible = false;
        }
    }

    if (!playerPoseGhostSpawnSequenceActive)
        return;

    const float spawnInterval = (std::max)(playerGhostSpawnInterval, 0.0f);
    playerPoseGhostSpawnTimer += unscaledDeltaTime;
    while (nextPlayerPoseGhostIndex < playerPoseGhosts.size() &&
        (spawnInterval <= FLT_EPSILON || playerPoseGhostSpawnTimer >= spawnInterval))
    {
        if (spawnInterval > FLT_EPSILON)
            playerPoseGhostSpawnTimer -= spawnInterval;
        CapturePlayerPoseGhost(nextPlayerPoseGhostIndex);
        ++nextPlayerPoseGhostIndex;
    }

    if (nextPlayerPoseGhostIndex >= playerPoseGhosts.size())
    {
        playerPoseGhostSpawnSequenceActive = false;
        playerPoseGhostSpawnTimer = 0.0f;
    }
}

void Player::UpdateCameraOcclusionTransparency() const
{
    skeletalMeshComponent->SetIsVisible(true);
    float ditherAlpha = 1.0f;
    currentCameraCollisionRatio = 1.0f;

    if (moviePerform || deathCameraTransparencyDisabled)
    {// 演出中は壁の近くでも透明化しない
        ditherAlpha = 1.0f;
    }
    else
    {
        if (auto camera = GetOwnerScene()->GetActorManager()->GetActorOfType<DarkCameraActor>())
        {
            float ratio = camera->GetCameraCollisionRatio();
            currentCameraCollisionRatio = ratio;
            if (ratio < transparencyStartRatio)
            {
                float t = std::clamp(ratio / transparencyStartRatio, 0.0f, 1.0f);
                t = t * t * (3.0f - 2.0f * t);

                ditherAlpha = std::lerp(transparencyMinAlpha, transparencyMaxAlpha, t);
            }
        }
    }
    ApplyDitherTransparencyAlpha(ditherAlpha);
}

void Player::ApplyDitherTransparencyAlpha(float alpha) const
{
    alpha = std::clamp(alpha, 0.0f, 1.0f);

    skeletalMeshComponent->plusAlphaCBuffer->data.cpuColor.w = alpha;
    swordMeshComponent->plusAlphaCBuffer->data.cpuColor.w = alpha;
    currentDitherTransparencyAlpha = alpha;
}

bool Player::CapturePlayerPoseGhost(const size_t ghostIndex)
{
    if (ghostIndex >= playerPoseGhosts.size() || !skeletalMeshComponent)
        return false;

    auto& ghost = playerPoseGhosts[ghostIndex];
    if (!ghost.renderConstantsComponent)
        return false;

    const auto& currentNodes = skeletalMeshComponent->GetNodes();
    if (currentNodes.empty())
        return false;

    ghost.nodes = currentNodes;
    ghost.world = skeletalMeshComponent->GetComponentWorldTransform().ToWorldTransform();
    ghost.elapsedTime = 0.0f;
    ghost.alpha = std::clamp(playerPoseGhostInitialAlpha, 0.0f, 1.0f);
    ghost.isVisible = ghost.alpha > 0.0f &&
        playerPoseGhostLifetime > FLT_EPSILON;

    auto& constants = ghost.renderConstantsComponent->plusAlphaCBuffer->data;
    constants.emissionPower = playerPoseGhostEmissive;
    constants.cpuColor = {
        playerPoseGhostColor.x,
        playerPoseGhostColor.y,
        playerPoseGhostColor.z,
        ghost.alpha };
    constants.effectParameters.edgeColor = {
        playerPoseGhostEdgeColor.x,
        playerPoseGhostEdgeColor.y,
        playerPoseGhostEdgeColor.z,
        1.0f };
    constants.effectParameters.innerColor = {
        playerPoseGhostInnerColor.x,
        playerPoseGhostInnerColor.y,
        playerPoseGhostInnerColor.z,
        1.0f };
    constants.effectParameters.edgeWidth = playerPoseGhostEdgeWidth;
    return true;
}


void Player::UpdatePrisonOpeningStandingPingPong(const float deltaTime)
{
    const auto controller = GetBodyAnimationController();
    if (!controller)
        return;

    const float clipLength = controller->GetAnimationLength("Standing");
    const float startTime = std::clamp(prisonOpeningSittingPingPongStartTime, 0.0f, clipLength);
    const float endTime = std::clamp(prisonOpeningSittingPingPongEndTime,
        startTime + 0.001f, clipLength);
    const float range = endTime - startTime;
    if (range <= FLT_EPSILON)
        return;

    prisonOpeningSittingPingPongPhase += (std::max)(deltaTime, 0.0f) *
        prisonOpeningSittingPingPongSpeed;
    const float period = range * 2.0f;
    const float wrappedPhase = std::fmod(prisonOpeningSittingPingPongPhase, period);
    if (wrappedPhase <= range)
    {
        prisonOpeningSittingSampleTime = startTime + wrappedPhase;
        prisonOpeningSittingPingPongForward = true;
    }
    else
    {
        prisonOpeningSittingSampleTime = endTime - (wrappedPhase - range);
        prisonOpeningSittingPingPongForward = false;
    }

    controller->HoldAnimationPose("Standing", prisonOpeningSittingSampleTime);
}
