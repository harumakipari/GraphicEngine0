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
#include "Components/Audio/AudioSourceComponent.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Camera/Camera.h"
#include "Game/Actors/Camera/DarkGameCamera.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/DarkGame/Interactable.h"
#include "Game/DarkGame/DarkActors/InteractableActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Physics/CollisionFunction.h"

namespace
{
    bool forceDitherTransparencyDebug = false;

    constexpr std::array<const char*, 8> RushSwordSEs =
    {
        "enemy_rush_damage3",
        "enemy_rush_damage3",
        "enemy_rush_damage3",
        "enemy_rush_damage3",
        "enemy_rush_damage3",
        "enemy_rush_damage3",
        "enemy_rush_damage3",
        "enemy_rush_damage3",
    };

    std::string MotionWarpPositionString(const DirectX::XMFLOAT3& value)
    {
        return "(" + std::to_string(value.x) + "," +
            std::to_string(value.y) + "," + std::to_string(value.z) + ")";
    }

    float NormalizeAngleDegrees(float angle)
    {
        while (angle > 180.0f) angle -= 360.0f;
        while (angle < -180.0f) angle += 360.0f;
        return angle;
    }

    const char* ToString(Player::ActionType action)
    {
        switch (action)
        {
        case Player::ActionType::None:     return "None";
        case Player::ActionType::Attack:   return "Attack";
        case Player::ActionType::Dodge:    return "Dodge";
        case Player::ActionType::Dash:     return "Dash";
        case Player::ActionType::Jump:     return "Jump";
        case Player::ActionType::Interact: return "Interact";
        }
        return "Unknown";
    }

    int GetActionPriority(Player::ActionType action)
    {
        switch (action)
        {
        case Player::ActionType::Dodge:  return 3;
        case Player::ActionType::Attack: return 2;
        case Player::ActionType::Dash:   return 1;
        default:                         return 0;
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
        playerAliveEmissionPower = 20.9f;
        skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = playerAliveEmissionPower;   // 自己発光の強さを設定
        // 服の色のための色相変更
        skeletalMeshComponent->plusAlphaCBuffer->data.hueShift = -1.0f;
        skeletalMeshComponent->plusAlphaCBuffer->data.saturation = 0.442f;
        skeletalMeshComponent->plusAlphaCBuffer->data.brightness = 0.352f;
        skeletalMeshComponent->plusAlphaCBuffer->data.contrast = 0.8f;
        skeletalMeshComponent->overrideDeferredPipelineName = "GltfModelPlayerDeferredPS";
        skeletalMeshComponent->SetIsCastShadow(false);
        skeletalMeshComponent->SetIsShadowMap(true);
        //skeletalMeshComponent->SetIsVisible(false);
        for (auto& material : skeletalMeshComponent->model->materials)
        {
            if (material.name == "M_Aurora_Hair_Blonde_FrozenHearth")
            {// 髪の毛だったら
                //material.overridePipelineName = "characterFurAndHairSkeletalMesh";
                material.materialType = MaterialType::Hair;
            }
            else if (material.name == "M_Aurora_Fur_FrozenHearth")
            {// Fur部分だったら
                material.overridePipelineName = "characterFurAndHairSkeletalMesh";
                material.materialType = MaterialType::Fur;
            }
            else if (material.name == "M_Aurora_Dress_Skirt_FrozenHearth" ||
                material.name == "M_Aurora_Dress_FrozenHearth" ||
                material.name == "M_Aurora_Body_Metals_FrozenHearth" ||
                material.name == "M_Aurora_Fur_FrozenHearth")
            {// 服部分だったら、
                material.materialType = MaterialType::Cloth;
            }
        }
    }

    // 透明にできるplayerを追加
    skeletalMeshBlendComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
    skeletalMeshBlendComponent->SetModel("./Data/Models/Characters/PlayerNoWeapon/playerBlend.gltf", false, true);
    skeletalMeshBlendComponent->overrideForwardPipelineName = "GltfModelPlayerBlendPS";
    skeletalMeshBlendComponent->overrideDeferredPipelineName = "GltfModelPlayerBlendPS";
    skeletalMeshBlendComponent->plusAlphaCBuffer->data.hueShift = -1.0f;
    skeletalMeshBlendComponent->plusAlphaCBuffer->data.saturation = 0.442f;
    skeletalMeshBlendComponent->plusAlphaCBuffer->data.brightness = 0.352f;
    skeletalMeshBlendComponent->plusAlphaCBuffer->data.contrast = 0.8f;
    skeletalMeshBlendComponent->plusAlphaCBuffer->data.objectType = ObjectType::Player;
    skeletalMeshBlendComponent->plusAlphaCBuffer->data.emissionPower = playerAliveEmissionPower;
    for (auto& material : skeletalMeshBlendComponent->model->materials)
    {
        if (material.name == "M_Aurora_Dress_Skirt_FrozenHearth" ||
            material.name == "M_Aurora_Dress_FrozenHearth" ||
            material.name == "M_Aurora_Body_Metals_FrozenHearth" ||
            material.name == "M_Aurora_Fur_FrozenHearth")
        {// 服部分だったら、
            material.materialType = MaterialType::Cloth;
        }
    }


    {
        PROFILE_SCOPE("Create PlayerAnimationController");

        // ルートノードを設定する
        int rootNodeIndex = skeletalMeshComponent->FindIndexByName("root");
        // アニメーションコントローラーを作成
        auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootNodeIndex);
        // Player movement is input-driven; never apply animation Root Motion.
        controller->SetEnableRootMotion(false);
        controller->SetIgnoreRootMotion(true);
        // 透明なモデルのアニメーションの動きを追加
        controller->AddTarget(skeletalMeshBlendComponent.get());

        controller->AddAnimation("Idle", 0);
        controller->AddAnimation("Jog_Fwd", 1);
        controller->AddAnimation("Roll_front_0", 2);
        controller->AddAnimation("Roll_back_0", 3);
        controller->AddAnimation("Roll_left_0", 4);
        controller->AddAnimation("Roll_right_0", 5);
        controller->AddAnimation("Primary_Attack_Fast_A", 6);
        controller->AddAnimation("Primary_Attack_Fast_B", 7);
        controller->AddAnimation("Primary_Attack_Fast_C", 8);
        controller->AddAnimation("Primary_Attack_Fast_D1_1", 9);
        controller->AddAnimation("Ability_RMB_Bwd_0", 10);
        controller->AddAnimation("Ability_RMB_Left_0", 11);
        controller->AddAnimation("Ability_RMB_Right_0", 12);
        controller->AddAnimation("Ability_RWB_Fwd_0", 13);
        controller->AddAnimation("Jump_Land_0", 14);
        controller->AddAnimation("Jump_Pad_0", 15);
        controller->AddAnimation("Jump_Recovery_0", 16);
        controller->AddAnimation("Jump_Start_0", 17);
        controller->AddAnimation("Attack_Air", 18);
        controller->AddAnimation("CombatRush_Fwd", 19);
        controller->AddAnimation("Hit_Combat_B", 20);
        controller->AddAnimation("Hit_Combat_Death", 21);
        controller->AddAnimation("Hit_Combat_F", 22);
        controller->AddAnimation("Hit_Combat_L", 23);
        controller->AddAnimation("Hit_Combat_Large_B", 24);
        controller->AddAnimation("Hit_Combat_Large_Death", 25);
        controller->AddAnimation("Hit_Combat_Large_F", 26);
        controller->AddAnimation("Hit_Combat_Large_L", 27);
        controller->AddAnimation("Hit_Combat_Large_R", 28);
        controller->AddAnimation("Hit_Combat_R", 29);
        controller->AddAnimation("anim_OpenDoor_L_0", 30);
        controller->AddAnimation("anim_OpenDoor_R_0", 31);
        controller->AddAnimation("Idle_Noise_A_0", 32);
        controller->AddAnimation("Idle_Noise_B_0", 33);
        controller->AddAnimation("Emote_Ice_Sculpture1_0", 34);
        controller->AddAnimation("Level_Start1_0", 35);
        controller->AddAnimation("Recall_0", 36);
        controller->AddAnimation("Level_Start_Cut", 37);
        controller->AddAnimation("Jog_Bwd", 38);
        controller->AddAnimation("Jog_Left", 39);
        controller->AddAnimation("Jog_Right", 40);
        controller->AddAnimation("Rush_Attack_Fast_A", 41);
        controller->AddAnimation("Rush_Attack_Fast_B", 42);
        controller->AddAnimation("Rush_Attack_Fast_C", 43);
        controller->AddAnimation("Rush_Attack_Fast_End", 44);
        controller->AddAnimation("Rush_Attack_Fast_D", 45);
        controller->AddAnimation("Sprint_Fwd", 46);
        controller->AddAnimation("Walk_Fwd", 47);
        controller->AddAnimation("Get_Up", 48);
        controller->AddAnimation("Hit_Large_KnockBack", 49);
        controller->AddAnimation("Emote_Win", 50);
        controller->SetRemoveRootTranslationFromPose("Hit_Large_KnockBack", true);

        // ブレンドスペースに追加
        //controller->AddBlendAnimation("Jog_Fwd", 0.0f, 1.0f);
        //controller->AddBlendAnimation("Jog_Bwd", 0.0f, -1.0f);
        //controller->AddBlendAnimation("Jog_BwdLeft", -1.0f, -1.0f);
        //controller->AddBlendAnimation("Jog_FwdLeft", -1.0f, 1.0f);
        //controller->AddBlendAnimation("Jog_FwdRight", 1.0f, 1.0f);
        //controller->AddBlendAnimation("Jog_BwdRight", 1.0f, -1.0f);

#if 0
        controller->AddForwardBlendAnimation("Jog_Fwd", 0.0f);
        controller->AddForwardBlendAnimation("Jog_FwdLeft90", -90.0f);
        controller->AddForwardBlendAnimation("Jog_FwdRight90", 90.0f);
        controller->AddForwardBlendAnimation("Jog_FwdRight45", 45.0f);
        controller->AddForwardBlendAnimation("Jog_FwdLeft45", -45.0f);

        controller->AddBackwardBlendAnimation("Jog_Bwd", 0.0f);
        controller->AddBackwardBlendAnimation("Jog_BwdLeft90", -90.0f);
        controller->AddBackwardBlendAnimation("Jog_BwdRight90", 90.0f);
        controller->AddBackwardBlendAnimation("Jog_BwdRight45", 45.0f);
        controller->AddBackwardBlendAnimation("Jog_BwdLeft45", -45.0f);
#else
        controller->AddLocomotionBlendAnimation("Jog_Fwd", 0.0f, 0.0f);
        controller->AddLocomotionBlendAnimation("Jog_Right", 90.0f, 0.464f);
        controller->AddLocomotionBlendAnimation("Jog_Bwd", 180.0f, 0.0f);
        controller->AddLocomotionBlendAnimation("Jog_Left", -90.0f, 0.5f);
#endif // 0

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
        stateMachine_->RegisterState(std::make_unique<PlayerJumpState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerJumpAttackState>(this));
        //stateMachine_->RegisterState(std::make_unique<PlayerInteractState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerDashState>(this));

        // 初期ステートを設定
        stateMachine_->ChangeState("Idle");
    }

#if 1
    {
        PROFILE_SCOPE("Create PlayerCollision");

        // 敵からの攻撃を受ける当たり判定用のコンポーネントを追加
        std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("capsuleComponent", parentName);
        DirectX::XMFLOAT3 size = skeletalMeshComponent->GetModelSize();
        height = size.y + 0.5f;
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
#endif // 1


#if 1
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


#endif // 0

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

    swordPointComp = AddComponent<CapsuleComponent>("SwordPointComponent", "Sword");
    swordPointComp->SetRelativeLocationDirect({ 0.0f,0.0f,0.6f });

    // 剣を背中に背負ったとき用の剣のメッシュコンポーネント
    int swordSheathSocketNode = skeletalMeshComponent->FindIndexByName("clavicle_armor_helper");
    swordSheathMeshComponent = this->AddComponent<SkeletalMeshComponent>("Sword", parentName);
    swordSheathMeshComponent->SetModel("./Data/Models/Weapons/PlayerSword/Sword.gltf", false, true);
    swordSheathMeshComponent->AttachToComponent(skeletalMeshComponent, swordSheathSocketNode); // "VB root_weapon"
    swordSheathMeshComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-0.1f });
    swordSheathMeshComponent->SetRelativeEulerRotationDirect({ 83.0f,-180.0f,-105.0f });

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

    // 火花エフェクト用のコンポーネントを追加
    sparkComponent = this->AddComponent<class ParticleComponent>("particleComponent", parentName);
    sparkComponent->Load("./Data/Effect/Files/DarkStageSparkEffect.json");


    // カメラの目の位置のコンポーネントを追加
    cameraEyeComponent = AddComponent<SceneComponent>("cameraEyeComponent", parentName);

    // カメラの注視点の位置のコンポーネントを追加
    cameraTargetComponent = AddComponent<SceneComponent>("cameraTargetComponent", parentName);
    cameraTargetComponent->SetRelativeLocationDirect({ 0.0f,1.0f,0.0f });

    // プレイヤー死亡時のカメラの画角
    const std::string rootParentName = GetRootComponentName();

    deathWideRightAnchor = AddComponent<SceneComponent>("DeathWideRightAnchor", rootParentName);
    deathWideRightAnchor->SetRelativeLocationDirect({ -5.0f,1.4f,-4.5f });

    deathWideLeftAnchor = AddComponent<SceneComponent>("DeathWideLeftAnchor", rootParentName);
    deathWideLeftAnchor->SetRelativeLocationDirect({ 6.6f,1.4f,4.5f });

    deathWideFrontAnchor = AddComponent<SceneComponent>("DeathWideFrontAnchor", rootParentName);
    deathWideFrontAnchor->SetRelativeLocationDirect({ 0.0f,1.8f,5.5f });

    deathWideBackAnchor = AddComponent<SceneComponent>("DeathWideBackAnchor", rootParentName);
    deathWideBackAnchor->SetRelativeLocationDirect({ 0.0f,1.8f,-5.5f });

    deathWideTarget = AddComponent<SceneComponent>("DeathWideTarget", rootParentName);
    deathWideTarget->SetRelativeLocationDirect({ 0.0f, 1.7f, 0.0f });


    // 軌跡初期化
    trail.Initialize();
    SetRushWeaponVisual(false);
    // ラッシュ時のUIを作成
    auto uiManager = GetOwnerScene()->GetUIManager();
    if (!lowHpVignetteTexturePath.empty() &&
        std::filesystem::exists(lowHpVignetteTexturePath))
    {
        lowHpVignetteImageComponent = std::make_shared<UIImageComponent>(
            lowHpVignetteTexturePath, "LowHpVignette");
        lowHpVignetteImageComponent->SetWorldPosition({ 960.0f, 540.0f });
        lowHpVignetteImageComponent->SetSize({ 1920.0f, 1080.0f });
        lowHpVignetteImageComponent->SetPivot({ 0.5f, 0.5f });
        lowHpVignetteImageComponent->SetColor(CoreColor{ 1.0f, 0.12f, 0.08f, 0.0f });
        lowHpVignetteImageComponent->SetVisible(false);
        lowHpVignetteImageComponent->zOrder = 100;
        uiManager->Add(lowHpVignetteImageComponent);
    }

    rushGuideImageComponent = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/Rush/rush_x_a_b1.png", "RushGuide");
    rushGuideImageComponent->SetWorldPosition(rushGuidePosition);
    rushGuideImageComponent->SetScale(rushGuideScale);
    rushGuideImageComponent->SetSize(rushGuideSize);
    rushGuideImageComponent->SetPivot({ 0.5f, 0.5f });
    rushGuideImageComponent->SetVisible(false);
    uiManager->Add(rushGuideImageComponent);

    rushButtonImageComponent = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/Rush/rush_y1.png", "RushY");
    rushButtonImageComponent->SetWorldPosition(rushButtonPosition);
    rushButtonImageComponent->SetScale({
        rushButtonBaseScale.x * 0.8f,
        rushButtonBaseScale.y * 0.8f });
    rushButtonImageComponent->SetSize(rushButtonSize);
    rushButtonImageComponent->SetPivot({ 0.5f, 0.5f });
    rushButtonImageComponent->SetVisible(false);
    uiManager->Add(rushButtonImageComponent);

    rushWordImageComponent = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/Rush/rush_word1.png", "RushWord");
    rushWordImageComponent->SetWorldPosition(rushWordPosition);
    rushWordImageComponent->SetScale(rushWordScale);
    rushWordImageComponent->SetSize(rushWordSize);
    rushWordImageComponent->SetPivot({ 0.5f, 0.5f });
    rushWordImageComponent->SetVisible(false);
    uiManager->Add(rushWordImageComponent);

    lockOnGuideArrowImageComponent = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/LT_arrow.png", "LockOnGuideArrow");
    lockOnGuideArrowImageComponent->SetSize(lockOnGuideArrowSize);
    lockOnGuideArrowImageComponent->SetScale(lockOnGuideArrowBaseScale);
    lockOnGuideArrowImageComponent->SetPivot({ 0.5f, 0.5f });
    lockOnGuideArrowImageComponent->SetVisible(false);
    lockOnGuideArrowImageComponent->zOrder = 20;
    uiManager->Add(lockOnGuideArrowImageComponent);

    lockOnGuideButtonImageComponent = std::make_shared<UIImageComponent>(
        "./Data/Textures/UI/LT_button.png", "LockOnGuideButton");
    lockOnGuideButtonImageComponent->SetSize(lockOnGuideButtonSize);
    lockOnGuideButtonImageComponent->SetScale(lockOnGuideButtonBaseScale);
    lockOnGuideButtonImageComponent->SetPivot({ 0.5f, 0.5f });
    lockOnGuideButtonImageComponent->SetVisible(false);
    lockOnGuideButtonImageComponent->zOrder = 21;
    uiManager->Add(lockOnGuideButtonImageComponent);
    SetEulerRotation({ 0.0f,90.0f,0.0f });

    // 操作説明UIを入れる
    operateUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/operate_ui1.png", "operate_ui");
    operateUiComponent->SetWorldPosition({ 1000, 1015 });
    operateUiComponent->SetSize({ 1130, 285 });
    operateUiComponent->SetScale({ 0.35f,0.35f });
    operateUiComponent->SetPivot({ 0.5f,0.5f });
    uiManager->Add(operateUiComponent);

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

    // Low HP feedback must observe death/recovery even while battle actions are suspended.
    UpdateLowHpEffects();
    UpdateDamageFlash();
    UpdateLockOnGuideUI();

    if (battleActionsSuspended)
        return;

    const bool gameplayInputEnabled = !IsInWinState();

    // Player HP UI uses unscaled time so HitStop / Slow do not pause the delayed gauge.
    const float currentHp = static_cast<float>((std::max)(hp, 0));
    const float uiDeltaTime = Time::UnscaledDeltaTime();
    if (delayedHp > currentHp)
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
        // HP recovery is reflected immediately without a delayed decrease effect.
        delayedHp = currentHp;
        delayedHpDelayTimer = 0.0f;
    }

    if (hpCurrentFillUiComponent && hpDelayedFillUiComponent)
    {
        const float maximumHp = static_cast<float>(maxHp);
        hpCurrentFillUiComponent->SetValue(currentHp, maximumHp);
        hpDelayedFillUiComponent->SetValue(delayedHp, maximumHp);
    }


    // プレイヤーの透明化処理
    if (moviePerform || deathCameraTransparencyDisabled)
    {// 演出中は壁の近くでも透明化しない
        skeletalMeshComponent->SetIsVisible(true);
        skeletalMeshBlendComponent->SetIsVisible(false);
    }
    else
    {
        if (auto camera = GetOwnerScene()->GetActorManager()->GetActorOfType<DarkCameraActor>())
        {
            float transparencyStartRation = 0.3f;
            float ratio = camera->GetCameraCollisionRatio();
            if (ratio < transparencyStartRation)
            {
                skeletalMeshComponent->SetIsVisible(false);
                skeletalMeshBlendComponent->SetIsVisible(true);

                float t = std::clamp(ratio / transparencyStartRation, 0.0f, 1.0f);
                t = t * t * (3.0f - 2.0f * t);

                float alpha = std::lerp(transparencyMinAlpha, transparencyMaxAlpha, t);
                if (forceDitherTransparencyDebug)
                    alpha = 0.6f;
                skeletalMeshBlendComponent->plusAlphaCBuffer->data.cpuColor.w = alpha;
            }
            else
            {
                skeletalMeshComponent->SetIsVisible(true);
                skeletalMeshBlendComponent->SetIsVisible(false);
            }
        }
    }

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
        const float t = justDodgeSlowReturnDuration > FLT_EPSILON
            ? std::clamp(playerSlowReturnElapsed / justDodgeSlowReturnDuration, 0.0f, 1.0f)
            : 1.0f;
        const float smoothT = t * t * (3.0f - 2.0f * t);
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

    if (characterMovementComponent)
    {
        characterMovementComponent->SetMoveSpeedSetting(walkSpeed, runSpeed);
    }
    if (gameplayInputEnabled && InputSystem::GetInputState("1"))
    {
        stateMachine_->ChangeState("Rush");
    }
    DirectX::XMFLOAT3 bossPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 playerPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 toEnemyDir = { 0.0f,0.0f,0.0f };

    // ボス戦時のカメラのeyeの位置を更新する
    if (auto gruxEnemy = GetOwnerScene()->GetActorManager()->GetActorOfType<GruxEnemy>())
    {
        bossPos = gruxEnemy->GetPosition();
        playerPos = GetPosition();
        toEnemyDir = MathHelper::Subtract(bossPos, playerPos);
        toEnemyDir = MathHelper::Normalize(toEnemyDir);
        DirectX::XMFLOAT3 eyePos = MathHelper::Add(
            MathHelper::Add(
                playerPos,
                MathHelper::Multiply(toEnemyDir, bossBattleCameraDistance)
            ),
            bossBattleCameraOffset
        );
        cameraEyeComponent->SetWorldLocationDirect(eyePos);
        DebugRender::DrawSphere(eyePos, 0.5f, { 1.0f,1.0f,0.0f,1.0f }, true);
    }

    // 剣の真ん中、根本、先の座標を取得する
    DirectX::XMFLOAT3 swordRootPos = swordRootComponent->GetComponentLocation();
    DirectX::XMFLOAT3 swordMidPos = swordMiddleComponent->GetComponentLocation();
    DirectX::XMFLOAT3 swordTipPos = swordTipComponent->GetComponentLocation();

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
            prevSwordRootPos, swordRootPos, sweeps[0].result, weaponSphereRadius, enemyLayer, true);
        sweeps[1].hit = CollisionFunction::SphereRayCast(
            prevSwordMidPos, swordMidPos, sweeps[1].result, weaponSphereRadius, enemyLayer, true);
        sweeps[2].hit = CollisionFunction::SphereRayCast(
            prevSwordTipPos, swordTipPos, sweeps[2].result, weaponSphereRadius, enemyLayer, true);

        constexpr float minSweepLength = 0.0001f;
        const SwordSweepResult* selectedNormalHit = nullptr;
        const SwordSweepResult* selectedOverlapHit = nullptr;
        const SwordSweepResult* selectedDamageHit = nullptr;
        float selectedNormalToi = FLT_MAX;
        float selectedOverlapDepth = FLT_MAX;
        float selectedDamageToi = FLT_MAX;
        const bool anySweepHit = sweeps[0].hit || sweeps[1].hit || sweeps[2].hit;

        for (const SwordSweepResult& sweep : sweeps)
        {
            const bool validSweepLength = sweep.sweepLength > minSweepLength;
            const float toi = sweep.hit && validSweepLength
                ? sweep.result.distance / sweep.sweepLength
                : -1.0f;
            const std::string actorName = sweep.result.actor
                ? sweep.result.actor->GetName()
                : "null";
            if (swordHitDebug && anySweepHit)
            {
                Logger::Log(Logger::LogCategory::Physics, std::format(
                    "[SwordHit] source={} success={} actor={} hasPosition={} hasNormal={} initialOverlap={} distance={} penetrationDepth={} sweepLength={} toi={} hitPoint={} normal={} prevPos={} currentPos={}",
                    sweep.source,
                    sweep.hit,
                    actorName,
                    sweep.result.hasPosition,
                    sweep.result.hasNormal,
                    sweep.result.initialOverlap,
                    sweep.result.distance,
                    sweep.result.penetrationDepth,
                    sweep.sweepLength,
                    toi,
                    MotionWarpPositionString(sweep.result.hitPoint),
                    MotionWarpPositionString(sweep.result.normal),
                    MotionWarpPositionString(sweep.start),
                    MotionWarpPositionString(sweep.end)));
            }

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
                if (auto enemy = dynamic_cast<GruxEnemy*>(hit.actor))
                {
                    Logger::Log(U8("剣に敵が当たった"));
                    enemy->TakeDamage(GetCurrentAttackDamage());
                    const bool isRushHit = stateMachine_ &&
                        std::string(stateMachine_->GetStateName()) == "Rush";
                    if (!isRushHit && selectedEffectHit && hit.hasPosition && hit.hasNormal)
                    {
                        enemy->SpawnHitEffect(hit.hitPoint, hit.normal, playerPos);
                    }
                    else if (!isRushHit)
                    {
                        if (swordHitDebug)
                            Logger::Log(Logger::LogCategory::Physics,
                                "[SwordHit] effect skipped: no valid hit position/normal");
                    }
                    hitActors.emplace(enemy);
                    if (isRushHit)
                    {
                        enemy->SpawnRushHitRing(hit.hitPoint, hit.normal, playerPos);
                    }
                    Time::SetSlow(0.0f,
                        isRushHit ? rushHitStopDuration : normalAttackHitStopDuration);

                }
            }
        }
    }

    // 剣のエミッシブを表示する
    if (swordMeshComponent)
    {// 剣にエミッシブを追加
        //swordMeshComponent->plusAlphaCBuffer->data.emissionPower = 8.0f;
        swordMeshComponent->plusAlphaCBuffer->data.emissionPower = swordEmissivePower;
    }

    // 軌跡の更新処理
    trail.UpdateTrail(deltaTime);

    // 軌跡と残像を表示する
    if (showTrail)
    {
        // 軌跡を追加
        trail.trailPoints.push_back({ swordTipPos,swordRootPos, trailRemainTime });

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

        //ghost.swordMeshComp->SetIsVisible(ghost.isVisible);

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

    // ヒットストップ処理
    if (hitStopTimer > 0.0f)
    {
        hitStopTimer -= Time::UnscaledDeltaTime();

        if (hitStopTimer <= 0.0f)
        {
            Time::timeScale = 1.0f; // 元に戻す
        }
    }


    if (gameplayInputEnabled)
    {
        // 入力処理
        CaptureActionRequest(deltaTime);

        // 現在フレームの入力から移動方向を確定し、その直後に一度だけ位置を更新する。
        UpdateMovement();
    }

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

    // これは絶対入れる　アニメーションの更新をしているから
    Character::Update(deltaTime);

    // Dodge State更新後の受付状態を、そのフレームのUIへ反映する。
    UpdateRushPromptUI();

    // 剣のデバックの当たり判定を描画するかどうか
    if (swordCollisionComp)
        swordCollisionComp->SetIsVisibleDebugShape(hitBox);

    if (gameplayInputEnabled)
        FindInteractable();

    switch (swordState)
    {
    case SwordState::Equipped:
        swordSheathMeshComponent->SetIsVisible(false);
        swordMeshComponent->SetIsVisible(true);
        break;
    case SwordState::Sheathed:
        swordSheathMeshComponent->SetIsVisible(true);
        swordMeshComponent->SetIsVisible(false);
        break;
    }

    //skeletalMeshComponent->UpdateCloth(elapsedTime);
    //skeletalMeshComponent->UpdateGlobalTransforms();

    if (gameplayInputEnabled)
    {
        if (InputSystem::GetInputState("RB", InputStateMask::Trigger))
        {
            Logger::Log("RBが押された");
        }
        if (InputSystem::GetInputState("LockOn", InputStateMask::Trigger))
        {
            Logger::Log("LockOnが押された");
        }
        if (InputSystem::GetInputState("RT", InputStateMask::Trigger))
        {
            Logger::Log("RTが押された");
        }
        if (InputSystem::GetInputState("GamePadA", InputStateMask::Trigger))
        {
            if (IInteractable* interactable = FindInteractable())
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

// 軌跡を描画する処理
void Player::RenderTrail(ID3D11DeviceContext* immediateContext)
{
    trail.Render(immediateContext);
}

void Player::SetRushWeaponVisual(const bool enabled)
{
    rushWeaponVisualEnabled = enabled;
    const DirectX::XMFLOAT3 swordColor = enabled
        ? rushSwordColor
        : DirectX::XMFLOAT3{ 0.0f, 0.8f, 1.0f };

    if (swordMeshComponent && swordMeshComponent->plusAlphaCBuffer)
    {
        swordMeshComponent->plusAlphaCBuffer->data.cpuColor =
        { swordColor.x, swordColor.y, swordColor.z, 0.0f };
    }

    trail.SetRushColorEnabled(enabled, rushSwordColor);
    activeGhostBaseColor = enabled ? rushSwordColor : swordGhostColor;
    activeGhostEdgeColor = enabled ? rushSwordColor : ghostEdgeColor;
}

void Player::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader(U8("目を閉じるデバック")))
    {
        const bool manualControlsEnabled = !deathEyeCloseActive;
        if (!manualControlsEnabled)
            ImGui::BeginDisabled();

        if (ImGui::Button("Close Eyes Preview"))
        {
            closeEyePreviewActive = RebuildEyeClosePoseOverride();
            closeEyeWeight = closeEyePreviewActive ? 1.0f : 0.0f;
            if (closeEyePreviewActive)
                GetBodyAnimationController()->SetLocalPoseOverrideWeight(closeEyeWeight);
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Eyes / Reset"))
            ResetEyeCloseOverride();

        if (ImGui::SliderFloat("Close Eye Weight", &closeEyeWeight, 0.0f, 1.0f))
        {
            if (!closeEyePreviewActive)
                closeEyePreviewActive = RebuildEyeClosePoseOverride();
            if (closeEyePreviewActive)
                GetBodyAnimationController()->SetLocalPoseOverrideWeight(closeEyeWeight);
        }

        if (!manualControlsEnabled)
            ImGui::EndDisabled();

        if (ImGui::SliderFloat("Closed Eye Pose Time", &closedEyePoseTime,
            0.0f, 7.083f, "%.3f sec"))
        {
            if (closeEyePreviewActive || deathEyeCloseActive)
                RebuildEyeClosePoseOverride();
        }
        ImGui::SliderFloat("Close Eye Start Time", &closeEyeStartTime,
            0.0f, 3.0f, "%.3f sec");
        ImGui::SliderFloat("Close Eye Duration", &closeEyeDuration,
            0.01f, 2.0f, "%.3f sec");

        ImGui::SliderFloat(U8("死亡時：目を閉じるまでの時間"), &deathEyeCloseDelay, 0.0f, 10.0f, "%.3f sec");
        ImGui::SliderFloat(U8("死亡時：目を閉じる時間"), &deathEyeCloseDuration, 0.01f, 5.0f, "%.3f sec");
        ImGui::SliderFloat(U8("死亡時：青い光が消え始まるまでの時間"), &deathColorFadeDelay, 0.0f, 10.0f, "%.3f sec");
        ImGui::SliderFloat(U8("死亡時：青い光が消える時間"), &deathColorFadeDuration, 0.01f, 5.0f, "%.3f sec");
        ImGui::ColorEdit3(U8("死亡時：発光部の色"), &deathDeadEmissiveColor.x);
        ImGui::Text("Death Eye Weight: %.3f", closeEyeWeight);
        ImGui::Text("Death Visual Fade: %.3f", deathVisualFade);

        ImGui::Separator();
        ImGui::Text("Closed Pose Time : %.3f", closedEyePoseTime);
        ImGui::Text("Start Time        : %.3f", closeEyeStartTime);
        ImGui::Text("Duration          : %.3f", closeEyeDuration);
        ImGui::Text("Weight            : %.3f", closeEyeWeight);
        ImGui::Text("Control           : %s",
            deathEyeCloseActive ? "Death Auto" :
            closeEyePreviewActive ? "Manual Preview" : "Animation");
    }

    if (ImGui::CollapsingHeader(U8("ジャスト回避のプレイヤーの残像")))
    {
        ImGui::DragFloat(U8("プレイヤーの残像の初回の透明度"), &playerPoseGhostInitialAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat(U8("プレイヤーの残像のライフタイム"), &playerPoseGhostLifetime, 0.01f, 0.0f, 5.0f, "%.2f sec");
        ImGui::DragFloat("Player Ghost Spawn Interval", &playerGhostSpawnInterval,
            0.001f, 0.0f, 1.0f, "%.3f sec");
        ImGui::ColorEdit3("Player Ghost Color", &playerPoseGhostColor.x);
        ImGui::DragFloat("Player Ghost Emissive", &playerPoseGhostEmissive,
            0.1f, 0.0f, 50.0f);
        ImGui::ColorEdit3("Player Ghost Edge Color", &playerPoseGhostEdgeColor.x);
        ImGui::ColorEdit3("Player Ghost Inner Color", &playerPoseGhostInnerColor.x);
        ImGui::DragFloat("Player Ghost Edge Width", &playerPoseGhostEdgeWidth,
            0.01f, 0.0f, 10.0f);
    }

    if (ImGui::CollapsingHeader(U8("プレイヤー瀕死状態時の数値調整")))
    {
        ImGui::DragInt(U8("瀕死判定HP"), &lowHpThreshold, 1, 0, 50);
        ImGui::Checkbox(U8("瀕死演出有効"), &lowHpActive);
        ImGui::DragFloat(U8("心拍間隔"), &heartbeatInterval, 0.01f, 0.0f, 5.0f, "%.2f sec");
        ImGui::DragFloat(U8("脈動時間"), &lowHpPulseDuration, 0.01f, 0.0f, 5.0f, "%.2f sec");
        ImGui::DragFloat(U8("瀕死赤発光強度"), &lowHpPulseFlashAmount, 0.01f, 0.0f, 5.0f, "%.2f");
        ImGui::DragFloat(U8("瀕死ビネット濃度"), &lowHpVignetteBaseAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat(U8("ビネット脈動最大濃度"), &lowHpVignettePulseAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
    }



    if (ImGui::CollapsingHeader("Player HP UI"))
    {
        ImGui::DragFloat("Delayed HP Hold Duration", &delayedHpDelayDuration,
            0.01f, 0.0f, 2.0f, "%.2f sec");
        ImGui::DragFloat("Delayed HP Follow Speed", &delayedHpFollowSpeed,
            0.1f, 0.0f, 100.0f, "%.2f HP/sec");
        if (ImGui::ColorEdit4("Current HP Color", &playerHpCurrentColor.r) && hpCurrentFillUiComponent)
            hpCurrentFillUiComponent->SetColor(playerHpCurrentColor);
        if (ImGui::ColorEdit4("Delayed HP Color", &playerHpDelayedColor.r) && hpDelayedFillUiComponent)
            hpDelayedFillUiComponent->SetColor(playerHpDelayedColor);
        // 被ダメージ時の赤くする
        ImGui::DragFloat("Damage Flash Duration", &damageFlashDuration,
            0.01f, 0.01f, 1.0f, "%.2f sec");
        ImGui::DragFloat("Damage Flash Body Tint Strength", &damageFlashBodyTintStrength,
            0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Damage Flash Rim Strength", &damageFlashRimStrength,
            0.05f, 0.0f, 10.0f, "%.2f");

    }
    if (ImGui::CollapsingHeader("Just Dodge Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enable World HitBox Debug", &justDodgeDebugEnabled);
        const bool isDodging = stateMachine_ && std::string(stateMachine_->GetStateName()) == "Dodge";
        const auto controller = GetBodyAnimationController();
        const float animationTime = controller ? controller->GetCurrentAnimationTime() : 0.0f;
        const float dodgeDurationDebug = controller ? controller->GetAnimationLength("Ability_RWB_Fwd_0") : 0.0f;
        const AnimationNotifyAsset* dodgeAsset = controller
            ? controller->GetAnimationAsset(isDodging ? controller->GetCurrentAnimationName() : "Ability_RWB_Fwd_0")
            : nullptr;
        float invincibleStart = 0.0f, invincibleEnd = 0.0f;
        float justStart = 0.0f, justEnd = 0.0f;
        float dodgeStateEnd = dodgeDurationDebug;
        if (dodgeAsset)
        {
            for (const auto& state : dodgeAsset->notifyTrack.states)
            {
                if (state.type == AnimationNotifyState::Type::Invincible)
                {
                    invincibleStart = state.startTime;
                    invincibleEnd = state.endTime;
                }
                else if (state.type == AnimationNotifyState::Type::JustDodgeWindow)
                {
                    justStart = state.startTime;
                    justEnd = state.endTime;
                }
                else if (state.type == AnimationNotifyState::Type::TransitionWindow)
                {
                    dodgeStateEnd = state.startTime;
                }
            }
        }

        ImGui::Text("Dodge Active: %s", isDodging ? "true" : "false");
        ImGui::Text("Dodge Elapsed: %.3f sec", dodgeDebugElapsed);
        ImGui::Text("Animation Time / Duration: %.3f / %.3f sec", animationTime, dodgeDurationDebug);
        ImGui::Text("Dodge State End (Transition Start): %.3f sec", dodgeStateEnd);
        ImGui::TextColored(invincibleWindow ? ImVec4(0.25f, 0.75f, 1.0f, 1.0f) : ImVec4(0.65f, 0.65f, 0.65f, 1.0f),
            "Invincible Active: %s", invincibleWindow ? "true" : "false");
        ImGui::TextColored(justDodgeWindow ? ImVec4(1.0f, 0.55f, 0.1f, 1.0f) : ImVec4(0.65f, 0.65f, 0.65f, 1.0f),
            "Just Dodge Active: %s", justDodgeWindow ? "true" : "false");
        ImGui::Text("Invincible: %.3f - %.3f sec", invincibleStart, invincibleEnd);
        ImGui::Text("Just Dodge: %.3f - %.3f sec", justStart, justEnd);

        const float timelineWidth = (std::max)(120.0f, ImGui::GetContentRegionAvail().x);
        constexpr float timelineHeight = 34.0f;
        const ImVec2 timelinePos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(timelinePos, ImVec2(timelinePos.x + timelineWidth, timelinePos.y + timelineHeight), IM_COL32(70, 70, 70, 255));
        if (dodgeDurationDebug > FLT_EPSILON)
        {
            const auto timelineX = [&](float time)
                {
                    return timelinePos.x + timelineWidth * std::clamp(time / dodgeDurationDebug, 0.0f, 1.0f);
                };
            drawList->AddRectFilled(ImVec2(timelineX(invincibleStart), timelinePos.y + 5.0f),
                ImVec2(timelineX(invincibleEnd), timelinePos.y + 19.0f), IM_COL32(45, 150, 235, 255));
            drawList->AddRectFilled(ImVec2(timelineX(justStart), timelinePos.y + 19.0f),
                ImVec2(timelineX(justEnd), timelinePos.y + 33.0f), IM_COL32(255, 135, 25, 255));
            if (isDodging)
            {
                const float currentX = timelineX(animationTime);
                drawList->AddLine(ImVec2(currentX, timelinePos.y), ImVec2(currentX, timelinePos.y + timelineHeight), IM_COL32(255, 255, 255, 255), 2.0f);
            }
        }
        ImGui::InvisibleButton("##DodgeTimeline", ImVec2(timelineWidth, timelineHeight));
        ImGui::TextDisabled("Blue: Invincible  Orange: Just  White: Current");

        const float justRate = dodgeDebugAttempts > 0
            ? 100.0f * static_cast<float>(dodgeDebugJustCount) / static_cast<float>(dodgeDebugAttempts)
            : 0.0f;
        ImGui::Text("Attempts: %d  Just: %d  Normal: %d  Damage: %d",
            dodgeDebugAttempts, dodgeDebugJustCount, dodgeDebugNormalCount, dodgeDebugDamageCount);
        ImGui::Text("Just / Attempt: %.1f%%", justRate);
        if (ImGui::Button("Reset Debug Stats"))
        {
            dodgeDebugAttempts = dodgeDebugJustCount = dodgeDebugNormalCount = dodgeDebugDamageCount = 0;
            lastJustDodgeValid = false;
        }
        ImGui::SeparatorText("Last Just Dodge");
        if (lastJustDodgeValid)
        {
            ImGui::Text("Attack: %s", lastJustDodgeAttack.c_str());
            ImGui::Text("Dodge / Animation Time: %.3f / %.3f sec", lastJustDodgeTime, lastJustAnimationTime);
            ImGui::Text("Just Window: %.3f - %.3f sec (Hit %.1f%%)",
                lastJustWindowStart, lastJustWindowEnd, lastJustWindowRatio * 100.0f);
            if (lastJustBossHitBoxElapsed >= 0.0f)
                ImGui::Text("Boss HitBox Active Elapsed: %.3f sec", lastJustBossHitBoxElapsed);
            else
                ImGui::Text("Boss HitBox Active Elapsed: N/A (not active)");
        }
        else ImGui::TextDisabled("No Just Dodge recorded");
    }
    ImGui::Checkbox(U8("ボス戦カメラ"), &isBossBattle);
    ImGui::DragFloat(U8("剣の球の当たり判定の半径"), &weaponSphereRadius, 0.05f);
    ImGui::Checkbox("Sword Hit Debug", &swordHitDebug);
    ImGui::DragFloat("dodgeSpeed", &dodgeSpeed, 0.1f);
    ImGui::DragFloat("dodgeDuration", &dodgeDuration, 0.1f);
    ImGui::DragFloat(U8("剣の軌跡が残る時間"), &trailRemainTime, 0.1f);
    ImGui::DragFloat(U8("剣の残像が残る時間"), &ghostFadeTime, 0.1f);
    const bool normalGhostBaseColorChanged =
        ImGui::ColorEdit3(U8("剣の残像の色"), &swordGhostColor.x);
    ImGui::DragFloat(U8("残像のemissiveColor"), &swordGhostEmissive, 0.1f);
    ImGui::DragFloat(U8("残像を出す間隔"), &ghostInterval, 0.001f, 0.0f, 1.0f, "%.5f");
    ImGui::DragFloat(U8("剣の残像の輪郭"), &ghostEdgeWidth);
    const bool normalGhostEdgeColorChanged =
        ImGui::ColorEdit3(U8("剣の残像のエッジの色"), &ghostEdgeColor.x);
    ImGui::ColorEdit3(U8("剣の残像の内部の色"), &ghostInnerColor.x);
    if (!rushWeaponVisualEnabled && (normalGhostBaseColorChanged || normalGhostEdgeColorChanged))
    {
        SetRushWeaponVisual(false);
    }

    if (ImGui::CollapsingHeader(U8("剣のラッシュ中の見た目"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::ColorEdit3(U8("ラッシの色"), &rushSwordColor.x) && rushWeaponVisualEnabled)
        {
            SetRushWeaponVisual(true);
        }
        ImGui::Checkbox("Rush Visual Active", &rushWeaponVisualEnabled);
    }
    ImGui::DragFloat(U8("ボス戦時のカメラ距離"), &bossBattleCameraDistance, 0.5f);
    ImGui::DragFloat3(U8("ボス戦時のオフセット"), &bossBattleCameraOffset.x, 0.5f);
    ImGui::SliderFloat("Walk Speed", &walkSpeed, 0.25f, 10.0f);
    ImGui::SliderFloat("Run Speed", &runSpeed, 0.25f, 15.0f);
    ImGui::DragFloat(U8("ヒットストップの時間"), &normalAttackHitStopDuration, 0.01f);
    ImGui::DragFloat(U8("ラッシュ時のヒットストップの時間"), &rushHitStopDuration,
        0.005f, 0.0f, 0.8f, "%.3f sec");
    ImGui::SliderFloat(
        "Forward Speed Scale", &forwardSpeedScale, 0.25f, 1.5f);
    ImGui::SliderFloat(
        "Side Speed Scale", &sideSpeedScale, 0.25f, 1.5f);
    ImGui::SliderFloat(
        "Backward Speed Scale", &backwardSpeedScale, 0.25f, 1.5f);
    ImGui::DragFloat("dashSpeed", &dashSpeed, 0.05f);
    if (ImGui::TreeNode("Just Dodge Slow Motion"))
    {
        ImGui::SliderFloat("Time Scale", &justDodgeTimeScale, 0.10f, 1.00f);
        ImGui::SliderFloat("Hold Duration", &justDodgeSlowHoldDuration, 0.00f, 2.0f, "%.2f sec");
        ImGui::SliderFloat("Return Duration", &justDodgeSlowReturnDuration, 0.00f, 0.50f, "%.2f sec");
        ImGui::SliderFloat("Rush Boss Slow Scale", &rushBossSlowScale, 0.10f, 1.00f);
        ImGui::SliderFloat("Rush Boss Return Duration", &rushBossReturnDuration, 0.00f, 0.50f, "%.2f sec");
        ImGui::Text("Player Phase: %d / Scale: %.3f", static_cast<int>(playerSlowPhase), GetTimeScale());
        ImGui::Text("Boss Phase: %d", static_cast<int>(bossSlowPhase));
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Rush Input Prompt"))
    {
        ImGui::DragFloat2("Guide Position", &rushGuidePosition.x, 1.0f);
        ImGui::DragFloat2("Guide Size", &rushGuideSize.x, 1.0f, 1.0f, 1024.0f);
        ImGui::DragFloat2("Guide Scale", &rushGuideScale.x, 0.01f, 0.01f, 4.0f);
        ImGui::DragFloat2("Y Position", &rushButtonPosition.x, 1.0f);
        ImGui::DragFloat2("Y Size", &rushButtonSize.x, 1.0f, 1.0f, 1024.0f);
        ImGui::DragFloat2("Y Base Scale", &rushButtonBaseScale.x, 0.01f, 0.01f, 4.0f);
        ImGui::DragFloat2("Word Position", &rushWordPosition.x, 1.0f);
        ImGui::DragFloat2("Word Size", &rushWordSize.x, 1.0f, 1.0f, 1024.0f);
        ImGui::DragFloat2("Word Scale", &rushWordScale.x, 0.01f, 0.01f, 4.0f);
        ImGui::DragFloat("Fade In Duration", &rushPromptFadeInDuration, 0.01f, 0.0f, 1.0f);
        ImGui::Separator();
        ImGui::Text("CanAcceptInitialRushInput: %s", CanAcceptInitialRushInput() ? "true" : "false");
        ImGui::Text("CanShowInitialRushGuide: %s", CanShowInitialRushGuide() ? "true" : "false");
        ImGui::Text("CanShowRushComboGuide: %s", CanShowRushComboGuide() ? "true" : "false");
        ImGui::Text("CanShowRushPrompt: %s", CanShowRushPrompt() ? "true" : "false");
        ImGui::Text("judgeSuccess: %s", rushJudgeSuccessDebug ? "true" : "false");
        ImGui::Text("rushRequested: %s", rushRequestedDebug ? "true" : "false");
        ImGui::Text("rushTargetValid: %s", rushTarget.expired() ? "false" : "true");
        ImGui::Text("UI Alpha: %.3f", rushPromptAlpha);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("LockOn Guide"))
    {
        ImGui::DragFloat("Delay", &lockOnGuideDelay, 0.05f, 0.0f, 10.0f);
        ImGui::DragFloat("Pulse Period", &lockOnGuidePulsePeriod, 0.05f, 0.1f, 5.0f);
        ImGui::DragFloat("Pulse Min Scale", &lockOnGuidePulseMinScale, 0.01f, 0.1f, 1.0f);
        ImGui::DragFloat2("Arrow Size", &lockOnGuideArrowSize.x, 1.0f, 1.0f, 1024.0f);
        ImGui::DragFloat2("Arrow Base Scale", &lockOnGuideArrowBaseScale.x, 0.01f, 0.01f, 4.0f);
        ImGui::DragFloat2("Button Size", &lockOnGuideButtonSize.x, 1.0f, 1.0f, 1024.0f);
        ImGui::DragFloat2("Button Base Scale", &lockOnGuideButtonBaseScale.x, 0.01f, 0.01f, 4.0f);
        ImGui::DragFloat("Button Offset", &lockOnGuideButtonOffset, 1.0f, 0.0f, 500.0f);
        ImGui::DragFloat("Edge Margin", &lockOnGuideEdgeMargin, 1.0f, 0.0f, 500.0f);
        ImGui::DragFloat("Arrow Rotation Offset", &lockOnGuideArrowRotationOffset, 1.0f, -360.0f, 360.0f);
        ImGui::Text("Visible: %s / Offscreen: %.2f", lockOnGuideVisible ? "true" : "false", lockOnGuideOffscreenElapsed);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Initial Rush Debug"))
    {
        const auto controller = GetBodyAnimationController();
        const std::string currentState = stateMachine_ ? stateMachine_->GetStateName() : "None";
        const bool attackRequestActive = bufferCommand.type == ActionType::Attack;
        const bool attackTriggeredThisFrame =
            InputSystem::GetInputState("Attack", InputStateMask::Trigger);
        const bool initialGuideVisible = CanShowInitialRushGuide();
        const bool comboGuideVisible = CanShowRushComboGuide();
        const bool uiVisible = initialGuideVisible || comboGuideVisible;
        const bool acceptsInitialInput = CanAcceptInitialRushInput();

        const char* playerSlowPhaseName = "Inactive";
        switch (playerSlowPhase)
        {
        case JustDodgeSlowPhase::Hold:     playerSlowPhaseName = "Hold"; break;
        case JustDodgeSlowPhase::Return:   playerSlowPhaseName = "Return"; break;
        case JustDodgeSlowPhase::RushHold: playerSlowPhaseName = "RushHold"; break;
        case JustDodgeSlowPhase::Inactive: break;
        }

        ImGui::Text("Just Dodge Success: %s", justDodgeSuccess ? "true" : "false");
        ImGui::Text("Rush Input Accepting: %s", rushInputAccepting ? "true" : "false");
        ImGui::TextColored(
            acceptsInitialInput ? ImVec4(0.25f, 1.0f, 0.35f, 1.0f) : ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
            "Can Accept Initial Rush Input: %s",
            acceptsInitialInput ? "ACCEPTING" : "NOT ACCEPTING");
        ImGui::Text("Rush Requested: %s", rushRequestedDebug ? "true" : "false");
        ImGui::Text("Current Player State: %s", currentState.c_str());
        ImGui::Separator();
        ImGui::Text("Player Slow Phase: %s", playerSlowPhaseName);
        ImGui::Text("Player Time Scale: %.3f", GetTimeScale());
        ImGui::Text("Slow Hold Timer: %.3f sec", playerSlowHoldTimer);
        ImGui::Text("Slow Return Elapsed: %.3f sec", playerSlowReturnElapsed);
        ImGui::Separator();
        ImGui::Text("Dodge Animation Time: %.3f sec",
            controller ? controller->GetCurrentAnimationTime() : 0.0f);
        ImGui::Text("Dodge Animation Duration: %.3f sec",
            controller ? controller->GetAnimationLength(controller->GetCurrentAnimationName()) : 0.0f);
        ImGui::Text("Transition Window Active: %s", transitionWindow ? "true" : "false");
        ImGui::Separator();
        ImGui::TextColored(
            attackRequestActive ? ImVec4(1.0f, 0.85f, 0.2f, 1.0f) : ImVec4(0.65f, 0.65f, 0.65f, 1.0f),
            "Attack ActionRequest Active: %s", attackRequestActive ? "BUFFERED" : "false");
        ImGui::Text("Attack ActionRequest Remaining Time: %.3f sec",
            attackRequestActive ? bufferCommand.remainTime : 0.0f);
        ImGui::Text("Attack Triggered This Frame: %s", attackTriggeredThisFrame ? "true" : "false");
        ImGui::Separator();
        ImGui::Text("Initial Rush Guide Visible: %s", initialGuideVisible ? "true" : "false");
        ImGui::Text("Rush Combo Guide Visible: %s", comboGuideVisible ? "true" : "false");
        ImGui::Text("UI Visible: %s", uiVisible ? "true" : "false");
        ImGui::Text("Accepts Input: %s", acceptsInitialInput ? "true" : "false");
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Rush Combo Debug"))
    {
        const auto controller = GetBodyAnimationController();
        const auto* rushState = stateMachine_
            ? dynamic_cast<const PlayerRushState*>(stateMachine_->GetCurrentState())
            : nullptr;
        const bool attackRequestActive = bufferCommand.type == ActionType::Attack;
        const bool animationPlaying = controller && controller->IsPlayAnimation();
        const bool transitionObserved =
            rushState && rushState->WasTransitionWindowObservedForDebug();
        const int queuedAttackCount =
            rushState ? rushState->GetQueuedAttackCountForDebug() : 0;
        const bool finalRushAttack = rushState &&
            rushState->GetComboIndex() + 1 >= GetMaxRushAttackCount();
        const bool lateInputGraceActive = transitionObserved && animationPlaying &&
            queuedAttackCount == 0 && !finalRushAttack;
        const bool acceptsRushInput = rushState && !finalRushAttack &&
            (inputWindow || lateInputGraceActive);
        const bool initialGuideVisible = CanShowInitialRushGuide();
        const bool comboGuideVisible = CanShowRushComboGuide();
        const bool uiVisible = initialGuideVisible || comboGuideVisible;

        ImGui::Text("Combo Index: %d", rushState ? rushState->GetComboIndex() : -1);
        ImGui::Text("Current Rush Animation: %s", rushState
            ? rushState->GetCurrentAttackAnimationForDebug().c_str() : "None");
        ImGui::Text("Input Window: %s", inputWindow ? "true" : "false");
        ImGui::Text("Transition Window: %s", transitionWindow ? "true" : "false");
        ImGui::Text("Rush Transition Window Observed: %s", transitionObserved ? "true" : "false");
        ImGui::Text("Late Input Grace Active: %s", lateInputGraceActive ? "true" : "false");
        ImGui::TextColored(
            acceptsRushInput ? ImVec4(0.25f, 1.0f, 0.35f, 1.0f) : ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
            "Accepts Rush Input: %s", acceptsRushInput ? "ACCEPTING" : "NOT ACCEPTING");
        ImGui::TextColored(
            queuedAttackCount > 0 ? ImVec4(0.25f, 0.8f, 1.0f, 1.0f) : ImVec4(0.65f, 0.65f, 0.65f, 1.0f),
            "Queued Attack Count: %d%s", queuedAttackCount,
            queuedAttackCount > 0 ? " (QUEUED)" : "");
        ImGui::Separator();
        ImGui::TextColored(
            attackRequestActive ? ImVec4(1.0f, 0.85f, 0.2f, 1.0f) : ImVec4(0.65f, 0.65f, 0.65f, 1.0f),
            "Attack ActionRequest Active: %s", attackRequestActive ? "BUFFERED" : "false");
        ImGui::Text("Attack ActionRequest Remaining Time: %.3f sec",
            attackRequestActive ? bufferCommand.remainTime : 0.0f);
        ImGui::Text("Attack Triggered This Frame: %s",
            InputSystem::GetInputState("Attack", InputStateMask::Trigger) ? "true" : "false");
        ImGui::Separator();
        ImGui::Text("Animation Time: %.3f sec",
            controller ? controller->GetCurrentAnimationTime() : 0.0f);
        ImGui::Text("Animation Duration: %.3f sec", controller
            ? controller->GetAnimationLength(controller->GetCurrentAnimationName()) : 0.0f);
        ImGui::Text("Animation Finished: %s", animationPlaying ? "false" : "true");
        ImGui::Separator();
        ImGui::Text("Initial Rush Guide Visible: %s", initialGuideVisible ? "true" : "false");
        ImGui::Text("Rush Combo Guide Visible: %s", comboGuideVisible ? "true" : "false");
        ImGui::Text("UI Visible: %s", uiVisible ? "true" : "false");
        ImGui::Text("Accepts Input: %s", acceptsRushInput ? "true" : "false");
        ImGui::TreePop();
    }
    ImGui::DragFloat("transparencyMinAlpha", &transparencyMinAlpha, 0.05f);
    ImGui::DragFloat("transparencyMaxAlpha", &transparencyMaxAlpha, 0.05f);
    ImGui::Checkbox("Force Dither Alpha 0.6 (Debug)", &forceDitherTransparencyDebug);
    ImGui::DragFloat(U8("ラッシュ後の敵までへのダッシュにかかる時間"), &moveToEnemyInterval, 0.05f);
    ImGui::DragFloat("MotionWarp attack surface distance",
        &motionWarpDesiredAttackSurfaceDistance, 0.01f, 0.3f, 1.0f);
    ImGui::DragFloat("Attack rotation max correction (deg)",
        &attackRotationMaxCorrectionDegrees, 1.0f, 45.0f, 60.0f);
    ImGui::DragFloat("Attack rotation speed (deg/sec)",
        &attackRotationSpeedDegrees, 5.0f, 30.0f, 720.0f);


    // コンボの始まりを設定する
    if (ImGui::BeginCombo(U8("コンボの始まり"), startAttackAnimation.c_str()))
    {
        for (auto clip : GetBodyAnimationController()->animationAssetOrder)
        {
            auto& comboAsset = GetBodyAnimationController()->animationNotifyAssets[clip];

            bool selected = comboAsset.animationName == startAttackAnimation;

            if (ImGui::Selectable(comboAsset.animationName.c_str(), selected))
            {
                startAttackAnimation = comboAsset.animationName;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }


    Character::DrawImGuiDetails();

    float speed = characterMovementComponent->GetCurrentInputNormalizeSpeed();
    auto move = characterMovementComponent->GetInputMagnitude();

    ImGui::Text("characterMovementComponentSpeed: %.4f", speed);
    ImGui::Text("CurrentInputSpeed: %.4f", move);
    ImGui::Text("actualHorizontalSpeed: %.4f",
        characterMovementComponent->GetActualHorizontalSpeed());
    ImGui::Text("finalMoveSpeed: %.4f",
        characterMovementComponent->GetFinalMoveSpeed());
    ImGui::SeparatorText("Action Request");
    ImGui::Text("ActionRequest: %s", ToString(bufferCommand.type));
    ImGui::Text("remainTime: %.3f", bufferCommand.remainTime);
    ImGui::Text("currentState: %s", stateMachine_->GetStateName());
    ImGui::SeparatorText("Damage");
    ImGui::Text("Player HP: %d / %d", hp, maxHp);
    ImGui::Text("invincibleWindow: %s", invincibleWindow ? "true" : "false");
    ImGui::Text("invincible: %s", invincible ? "true" : "false");
    ImGui::SeparatorText("KnockBack Debug");
    const auto* knockBackState = stateMachine_
        ? dynamic_cast<const PlayerKnockBackState*>(stateMachine_->GetCurrentState())
        : nullptr;
    const auto animationController = GetBodyAnimationController();
    ImGui::Text("KnockBack Active: %s", knockBackActive ? "true" : "false");
    ImGui::Text("KnockBack Phase: %s",
        knockBackState ? knockBackState->GetPhaseName() : "None");
    ImGui::Text("KnockBack Direction: %.3f, %.3f, %.3f",
        knockBackDirection.x, knockBackDirection.y, knockBackDirection.z);
    ImGui::Text("ForcedMove Active: %s",
        knockBackForcedMoveActive ? "true" : "false");
    ImGui::Text("KnockBack Elapsed: %.3f sec", knockBackElapsed);
    ImGui::DragFloat("KnockBack Duration", &knockBackDuration,
        0.01f, 0.01f, 3.0f, "%.2f sec");
    ImGui::DragFloat("KnockBack Initial Speed", &knockBackInitialSpeed,
        0.1f, 0.0f, 30.0f, "%.2f");
    ImGui::Text("KnockBack Speed: %.3f",
        knockBackForcedMoveActive ? knockBackInitialSpeed : 0.0f);
    ImGui::Text("Current Player State: %s",
        stateMachine_ ? stateMachine_->GetStateName() : "None");
    ImGui::Text("Current Animation: %s", animationController
        ? animationController->GetCurrentAnimationName().c_str() : "None");
    ImGui::Text("Animation Time: %.3f sec", animationController
        ? animationController->GetCurrentAnimationTime() : 0.0f);
    ImGui::Text("Animation Duration: %.3f sec", animationController
        ? animationController->GetAnimationLength(
            animationController->GetCurrentAnimationName()) : 0.0f);
    ImGui::Text("Animation Finished: %s",
        animationController && animationController->IsPlayAnimation()
        ? "false" : "true");
    ImGui::Text("Remove Root Translation From Pose: %s",
        animationController &&
        animationController->IsRemovingRootTranslationFromPose()
        ? "true" : "false");
    if (animationController)
    {
        const auto& rawRoot = animationController->GetRawRootLocalTranslation();
        const auto& appliedRoot = animationController->GetAppliedRootLocalTranslation();
        ImGui::Text("Root Local Translation Raw: %.3f, %.3f, %.3f",
            rawRoot.x, rawRoot.y, rawRoot.z);
        ImGui::Text("Root Local Translation Applied: %.3f, %.3f, %.3f",
            appliedRoot.x, appliedRoot.y, appliedRoot.z);
    }

#endif
}

void Player::OnAnimationNotifyBegin(const AnimationNotifyState& state)
{
    switch (state.type)
    {
    case AnimationNotifyState::Type::HitBox:
        Logger::Log(U8("当たり判定を開始しました"));
        hitBox = true;
        if (stateMachine_->GetStateName() == "Attack")
            StopAttackTargetRotation();
        break;
    case AnimationNotifyState::Type::InputWindow:
        inputWindow = true;
        Logger::Log(U8("入力受付を開始しました"));
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
        bool targetClampApplied = false;

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

                const float centerStopDistance = playerRadius + enemyRadius +
                    motionWarpDesiredAttackSurfaceDistance;
                remainingApproachDistance = (std::max)(
                    currentCenterDistance - centerStopDistance, 0.0f);
                actualWarpDistance = (std::min)(
                    state.moveDistance, remainingApproachDistance);
                targetClampApplied = true;
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
        Logger::Log(Logger::LogCategory::Gameplay,
            "[MotionWarp][Begin] notifyMoveDistance=" + std::to_string(state.moveDistance) +
            " currentCenterDistance=" + std::to_string(currentCenterDistance) +
            " playerRadius=" + std::to_string(playerRadius) +
            " enemyRadius=" + std::to_string(enemyRadius) +
            " desiredAttackSurfaceDistance=" + std::to_string(motionWarpDesiredAttackSurfaceDistance) +
            " remainingApproachDistance=" + std::to_string(remainingApproachDistance) +
            " actualWarpDistance=" + std::to_string(actualWarpDistance) +
            " targetClampApplied=" + std::string(targetClampApplied ? "true" : "false") +
            " positionBefore=" + MotionWarpPositionString(warp.startPosition));
    }
    break;
    }
}

void Player::OnAnimationNotifyEnd(const AnimationNotifyState& state)
{
    switch (state.type)
    {
    case AnimationNotifyState::Type::HitBox:
        //Logger::Log(U8("当たり判定を終了しました"));
        hitBox = false;
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
        if (activeWarp != animationMotionWarps.end())
        {
            float endCenterDistance = 0.0f;
            if (const auto target = attackTarget.lock())
            {
                DirectX::XMFLOAT3 delta = MathHelper::Subtract(target->GetPosition(), GetPosition());
                delta.y = 0.0f;
                endCenterDistance = MathHelper::Length(delta);
            }

            Logger::Log(Logger::LogCategory::Gameplay,
                "[MotionWarp][End] notifyMoveDistance=" +
                std::to_string(activeWarp->notifyMoveDistance) +
                " actualWarpDistance=" + std::to_string(activeWarp->actualWarpDistance) +
                " endCenterDistance=" + std::to_string(endCenterDistance) +
                " positionBefore=" + MotionWarpPositionString(activeWarp->startPosition) +
                " positionAfter=" + MotionWarpPositionString(GetPosition()));
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

void Player::HandleAnimationPlaySE(const AnimationNotifyEvent& event)
{
    if (event.parameter.empty())
        return;

    std::string soundName = event.parameter;
    if (event.parameter == "RushSword")
    {
        if (std::string(stateMachine_->GetStateName()) != "Rush")
        {
            Logger::Log(Logger::LogCategory::Gameplay,
                "[Rush][SwordSE] ignored: RushSword event fired outside Rush state");
            return;
        }

        const auto* rushState =
            dynamic_cast<const PlayerRushState*>(stateMachine_->GetCurrentState());
        if (!rushState)
        {
            Logger::Log(Logger::LogCategory::Gameplay,
                "[Rush][SwordSE] ignored: current state is not PlayerRushState");
            return;
        }

        const int comboIndex = rushState->GetComboIndex();
        if (comboIndex < 0 ||
            comboIndex >= static_cast<int>(RushSwordSEs.size()))
        {
            Logger::Log(Logger::LogCategory::Gameplay,
                "[Rush][SwordSE] ignored: comboIndex out of range: " +
                std::to_string(comboIndex));
            return;
        }

        soundName = RushSwordSEs[comboIndex];
    }

    const std::string audioPath = "./Data/Sound/SE/" + soundName + ".wav";
    CoreAudio::PlayOneShot(audioPath, event.value);
}

void Player::OnAnimationNotifyEvent(const AnimationNotifyEvent& event)
{
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
            SetRushInputAcceptance(false);
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

void Player::OnAnimationChanged()
{
    // Reset active warps even when the previous animation's NotifyEnd was skipped.
    animationMotionWarps.clear();
    transitionWindow = false;  // ステート遷移してもいいかどうか
    comboQueued = false;   // コンボ攻撃がキューに入っているかどうか
    inputWindow = false;   // コンボ受付をするかどうか
    hitBox = false;     // 当たり判定
    justDodgeWindow = false;    // ジャスト回避を受け付けるかどうか
    invincibleWindow = false;   // 無敵状態かどうか
    justDodgeSuccess = false; // ジャスト回避が成功したかどうか
    showTrail = false;
    swordEmissivePower = 0.0f;
    hitActors.clear();
    //Logger::Log(U8("playerのAnimationが切り替わった"));
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
    case LocomotionMode::Dash:
        if (speed >= 0.6f)
            SetLocomotionMode(LocomotionMode::TPSRun);
        else if (speed > 0.0f)
            SetLocomotionMode(LocomotionMode::TPSWalk);
        else
            SetLocomotionMode(LocomotionMode::Idle);
        break;
        // Focus / LockOn から TPS に戻った時
    case LocomotionMode::LockOnBlendWalk:
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
        if (speed >= 0.6f)
            SetLocomotionMode(LocomotionMode::LockOnBlendRun);
        else
            SetLocomotionMode(LocomotionMode::LockOnBlendWalk);
        break;
    case LocomotionMode::Idle:
        if (speed > 0.0f)
            SetLocomotionMode(LocomotionMode::LockOnBlendWalk);
        break;

    case LocomotionMode::LockOnBlendWalk:
        if (speed <= 0.0f)
            SetLocomotionMode(LocomotionMode::Idle);
        else if (speed >= 0.6f)
            SetLocomotionMode(LocomotionMode::LockOnBlendRun);
        break;
    case LocomotionMode::LockOnBlendRun:
        if (speed < 0.55f)
            SetLocomotionMode(LocomotionMode::LockOnBlendWalk);
        break;
    case LocomotionMode::Dash:
        if (speed >= 0.6f)
            SetLocomotionMode(LocomotionMode::LockOnBlendRun);
        else if (speed > 0.0f)
            SetLocomotionMode(LocomotionMode::LockOnBlendWalk);
        else
            SetLocomotionMode(LocomotionMode::Idle);
        break;
    case LocomotionMode::TPSWalk:
    case LocomotionMode::TPSRun:
        if (speed >= 0.6f)
        {
            SetLocomotionMode(LocomotionMode::LockOnBlendRun);
        }
        else if (speed > 0.0f)
        {
            SetLocomotionMode(LocomotionMode::LockOnBlendWalk);
        }
        else
        {
            SetLocomotionMode(LocomotionMode::Idle);
        }
        break;
    }

}

// アニメーションステート関連のフラグをリセットする
void Player::ResetAnimationStateFlag()
{
    //transitionWindow = false;  // ステート遷移してもいいかどうか
    //comboQueued = false;   // コンボ攻撃がキューに入っているかどうか
    //inputWindow = false;   // コンボ受付をするかどうか
    //hitBox = false;     // 当たり判定
    //justDodgeWindow = false;    // ジャスト回避を受け付けるかどうか
    //invincibleWindow = false;   // 無敵状態かどうか
    //justDodgeSuccess = false; // ジャスト回避が成功したかどうか
    //showTrail = false;
    //swordEmissivePower = 0.0f;
    //Logger::Log(U8("アニメーションステート関連のフラグをリセットする"));
}

// イベントシーン開始時に呼ぶ処理
void Player::StartEvent()
{
    // 操作UIを非表示する
    if (operateUiComponent)
    {
        operateUiComponent->SetVisible(false);
    }
    // 入力を受け付けない
    InputSystem::SetInputEnabled(false);
    //　イベント中はplayerの透過処理をなくす
    this->moviePerform = true;
}

// イベントシーン終了時に呼ぶ処理
void Player::EndEvent()
{
    // 操作UIを表示する
    if (operateUiComponent)
    {
        operateUiComponent->SetVisible(true);
    }

    //　イベントが終わったのでplayerの透過処理を戻す
    this->moviePerform = false;
}

void Player::SetGameplayHudVisible(const bool visible)
{
    gameplayHudFadeEntries.clear();
    SetHpBarVisible(visible);
    if (rushGuideImageComponent) rushGuideImageComponent->SetVisible(visible);
    if (rushButtonImageComponent) rushButtonImageComponent->SetVisible(visible);
    if (rushWordImageComponent) rushWordImageComponent->SetVisible(visible);
    if (visible)
        UpdateRushPromptUI();
    else
    {
        if (operateUiComponent) operateUiComponent->SetVisible(false);
        HideAndResetLockOnGuideUI();
        if (lowHpVignetteImageComponent) lowHpVignetteImageComponent->SetVisible(false);
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
    capture(operateUiComponent);
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

void Player::SetHpBarVisible(const bool visible)
{
    for (const auto& component : hpBarUiComponents)
    {
        if (!component)
            continue;
        component->SetVisible(visible);
        component->SetEnable(visible);
    }
}

void Player::StopBattleActions()
{
    battleActionsSuspended = true;
    SetTimeScale(0.0f);
    ClearTransientBattleActions();
}

void Player::EnterWinState()
{
    battleActionsSuspended = false;
    if (stateMachine_)
        stateMachine_->ChangeState("Win");
}

bool Player::IsInWinState() const
{
    return stateMachine_ && std::string(stateMachine_->GetStateName()) == "Win";
}

void Player::ClearTransientBattleActions()
{
    ClearActionRequest("battle_end");
    ClearAttackTarget();
    EndAttack();
    SetRushInputAcceptance(false);
    SetRushInputDebugState(false, false);
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
    trail.trailPoints.clear();
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

void Player::ResetForBattleContinue(const Transform& battleStartTransform)
{
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
    hitStopTimer = 0.0f;
    ApplyDamageFlash(0.0f);

    hp = maxHp;
    delayedHp = static_cast<float>(hp);
    delayedHpDelayTimer = 0.0f;
    if (hpCurrentFillUiComponent) hpCurrentFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));
    if (hpDelayedFillUiComponent) hpDelayedFillUiComponent->SetValue(delayedHp, static_cast<float>(maxHp));

    SetPosition(battleStartTransform.GetLocation());
    SetQuaternionRotation(battleStartTransform.GetRotation());
    SetScale(battleStartTransform.GetScale());
    UpdateAllComponentTransforms();

    if (stateMachine_)
        stateMachine_->ChangeState("Idle");
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
    closeEyePreviewActive = false;
    deathEyeCloseActive = false;
    deathEyeCloseStarted = false;
    deathVisualFadeStarted = false;
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

void Player::UpdateDeathVisualFade(const float deathElapsedTime)
{
    if (!deathVisualFadeStarted && deathElapsedTime >= deathColorFadeDelay)
        deathVisualFadeStarted = true;

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

void Player::EndDeathEyeClose()
{
    ResetEyeCloseOverride();
}

// 火花エフェクトの生成
void Player::SpawnSpark(DirectX::XMFLOAT3 pos)
{
    DebugRender::DrawSphere(pos, 0.2f, { 1, 0.5f, 0, 1 }, 0.3f, true);
    if (sparkComponent)
    {
        sparkComponent->SetWorldLocationDirect(pos);
        sparkComponent->Play();
    }
}

// 剣の攻撃判定
void Player::CheckSwordLineHit(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end)
{
    if (stateMachine_->GetStateName() != "Attack")
        return;

    // 1フレーム1回制御
    if (hasSpawnedThisAttack)
        return;

    if (!isAttackActive)
        return;

    XMVECTOR s = XMLoadFloat3(&start);
    XMVECTOR e = XMLoadFloat3(&end);

    XMVECTOR diff = e - s;
    float length = XMVectorGetX(XMVector3Length(diff));

    int steps = std::max<int>(1, (int)(length / 0.05f));

    for (int i = 0; i < steps; i++)
    {
        float t0 = (float)i / steps;
        float t1 = (float)(i + 1) / steps;

        XMVECTOR p0 = XMVectorLerp(s, e, t0);
        XMVECTOR p1 = XMVectorLerp(s, e, t1);

        XMFLOAT3 segStart, segEnd;
        XMStoreFloat3(&segStart, p0);
        XMStoreFloat3(&segEnd, p1);

        HitResultWithActor hit;

        if (CollisionFunction::SphereRayCast(
            segStart,
            segEnd,
            hit,
            0.1f,
            CollisionHelper::ToBit(CollisionLayer::WorldStatic)))
        {
            // 床無視
            if (hit.normal.y > 0.7f)
                continue;

            // 押し出し
            XMVECTOR pos = XMLoadFloat3(&hit.hitPoint);
            XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(&hit.normal));
            pos += normal * 0.03f;

            XMFLOAT3 finalPos;
            XMStoreFloat3(&finalPos, pos);

            SpawnSpark(finalPos);

            hasSpawnedThisAttack = true; // ←これが本質

            return;
        }
    }

    DebugRender::DrawLine(start, end, { 1,0,0,1 });
}

// 入力処理をまとめる
void Player::CaptureActionRequest(float deltaTime)
{
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
            Logger::Log(Logger::LogCategory::Gameplay,
                "[ActionRequest][Expired] action=" + std::string(ToString(bufferCommand.type)) +
                " remainTime=" + std::to_string(bufferCommand.remainTime) +
                " state=" + stateMachine_->GetStateName());
            bufferCommand = {};
        }
    }

    if (InputSystem::GetInputState("Jump", InputStateMask::Trigger))
    {
        Logger::Log(Logger::LogCategory::Gameplay,
            std::string("[ActionRequest][Detected] action=Dodge state=") + stateMachine_->GetStateName());
        DecideLockOnDodgeDirection();
        StoreActionRequest(ActionType::Dodge, 0.3f);
    }
    if (InputSystem::GetInputState("Attack", InputStateMask::Trigger))
    {
        Logger::Log(Logger::LogCategory::Gameplay,
            std::string("[ActionRequest][Detected] action=Attack state=") + stateMachine_->GetStateName());
        StoreActionRequest(ActionType::Attack, 0.5f);
    }
    if (InputSystem::GetInputState("GamePadB", InputStateMask::Trigger))
    {
        Logger::Log(Logger::LogCategory::Gameplay,
            std::string("[ActionRequest][Detected] action=Dash state=") + stateMachine_->GetStateName());
        StoreActionRequest(ActionType::Dash, 0.3f);
    }
    //if (InputSystem::GetInputState("Interact", InputStateMask::Trigger))
    //{
    //    StoreActionRequest(ActionType::Interact, 0.3f);
    //}
}

bool Player::StoreActionRequest(ActionType type, float remainTime)
{
    const int newPriority = GetActionPriority(type);
    const int currentPriority = GetActionPriority(bufferCommand.type);
    if (bufferCommand.type != ActionType::None && newPriority < currentPriority)
    {
        Logger::Log(Logger::LogCategory::Gameplay,
            "[ActionRequest][Rejected] action=" + std::string(ToString(type)) +
            " reason=lower_priority current=" + std::string(ToString(bufferCommand.type)) +
            " state=" + stateMachine_->GetStateName());
        return false;
    }

    const ActionType previous = bufferCommand.type;
    bufferCommand.type = type;
    bufferCommand.remainTime = remainTime;
    if (type == ActionType::Dodge)
    {
        bufferCommand.dodgeDirection = dodgeDirection;
        bufferCommand.dodgeWorldDirection = dodgeWorldDirection;
        bufferCommand.useDodgeWorldDirection = useDodgeWorldDirection;
    }

    const char* reason = previous == ActionType::None ? "empty" :
        (previous == type ? "refreshed" : "higher_priority");
    Logger::Log(Logger::LogCategory::Gameplay,
        "[ActionRequest][Stored] action=" + std::string(ToString(type)) +
        " remainTime=" + std::to_string(remainTime) +
        " reason=" + reason +
        " previous=" + std::string(ToString(previous)) +
        " state=" + stateMachine_->GetStateName());
    return true;
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
    case ActionType::Dash:
        if (characterMovementComponent->GetInputMagnitude() <= 0.01f)
        {
            Logger::Log(Logger::LogCategory::Gameplay,
                std::string("[ActionRequest][Deferred] action=Dash reason=no_move_input state=") +
                stateMachine_->GetStateName());
            return false;
        }
        if (!InputSystem::GetInputState("GamePadB", InputStateMask::Press))
        {
            Logger::Log(Logger::LogCategory::Gameplay,
                std::string("[ActionRequest][Deferred] action=Dash reason=button_released state=") +
                stateMachine_->GetStateName());
            return false;
        }
        SetLocomotionMode(LocomotionMode::Dash);
        targetState = "Dash";
        break;
    case ActionType::Jump:
        targetState = "Jump";
        break;
        //case ActionType::Interact:
        //    targetState = "Interact";
        break;
    }

    if (requestedType != ActionType::Dash)
    {
        stateMachine_->ChangeState(targetState);
    }

    if (stateMachine_->GetStateName() != targetState)
    {
        Logger::Log(Logger::LogCategory::Gameplay,
            "[ActionRequest][ExecutionFailed] action=" + std::string(ToString(requestedType)) +
            " target=" + targetState + " state=" + stateMachine_->GetStateName());
        return false;
    }

    Logger::Log(Logger::LogCategory::Gameplay,
        "[ActionRequest][Executed] action=" + std::string(ToString(requestedType)) +
        " state=" + targetState);
    ConsumeActionRequest(requestedType);
    return true;
}

void Player::AcquireAttackTarget()
{
    attackTarget.reset();
    const auto enemies = GetOwnerScene()->GetActorManager()->GetActorsOfType<Enemy>();

    // Prefer the camera's LockOn/Focus target when it is an Enemy in this scene.
    if (const auto camera = dynamic_cast<DarkCameraActor*>(GetOwnerScene()->GetActiveCamera()))
    {
        if (const auto targetHead = camera->GetEnemyHead())
        {
            Actor* targetOwner = targetHead->GetOwner();
            for (const auto& enemy : enemies)
            {
                if (enemy && enemy.get() == targetOwner)
                {
                    attackTarget = enemy;
                    break;
                }
            }
        }
    }
    // TPS/no lock-on fallback: nearest valid Enemy.
    if (attackTarget.expired())
    {
        float nearestDistance = FLT_MAX;
        for (const auto& enemy : enemies)
        {
            if (!enemy)
                continue;
            DirectX::XMFLOAT3 delta = MathHelper::Subtract(enemy->GetPosition(), GetPosition());
            delta.y = 0.0f;
            const float distance = MathHelper::Length(delta);
            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                attackTarget = enemy;
            }
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
    if (!target)
    {
        attackRotationTracking = false;
        return;
    }

    DirectX::XMFLOAT3 direction = MathHelper::Subtract(target->GetPosition(), GetPosition());
    direction.y = 0.0f;
    if (MathHelper::Length(direction) <= FLT_EPSILON)
        return;

    const float targetYaw = DirectX::XMConvertToDegrees(std::atan2f(direction.x, direction.z));
    const float correctionFromStart = std::clamp(
        NormalizeAngleDegrees(targetYaw - attackRotationStartYaw),
        -attackRotationMaxCorrectionDegrees,
        attackRotationMaxCorrectionDegrees);
    const float limitedTargetYaw = attackRotationStartYaw + correctionFromStart;

    DirectX::XMFLOAT3 rotation = GetEulerRotation();
    const float remainingYaw = NormalizeAngleDegrees(limitedTargetYaw - rotation.y);
    const float maxStep = attackRotationSpeedDegrees * deltaTime;
    rotation.y += std::clamp(remainingYaw, -maxStep, maxStep);
    SetEulerRotation(rotation);
}

void Player::ClearAttackTarget()
{
    attackRotationTracking = false;
    attackTarget.reset();
}

// 動作更新処理
void Player::UpdateMovement()
{
    const std::string currentState = GetStateMachine()->GetStateName();
    bool isDash = currentState == "Dash";
    const bool suppressMovementInput =
        currentState == "Damage" || currentState == "KnockBack" ||
        currentState == "DeathPending" || currentState == "Death";

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
#if 1
        if (isDash)
        {
            camera->SetRequestMode(DarkCameraActor::CameraMode::TPS);
            if (InputSystem::GetInputState("LockOn", InputStateMask::Trigger))
            {
                camera->RotateToPlayerForward();
            }
        }
        else
        {
            if (isBossBattle)
            {
                if (focus)
                {
                    camera->SetRequestMode(DarkCameraActor::CameraMode::LockOn);
                }
                else
                {
                    camera->SetRequestMode(DarkCameraActor::CameraMode::TPS);
                }
            }
            else
            {
                if (focus)
                {
                    camera->SetRequestMode(DarkCameraActor::CameraMode::Focus);
                }
                else
                {
                    camera->SetRequestMode(DarkCameraActor::CameraMode::TPS);
                }
            }
        }
#endif // 0

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
             //newLength = std::pow(newLength,1.5f);// より繊細な入力
            //newLength *= newLength;
            // newLength = sqrtf(newLength); // 少し倒しただけで速い
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
                state != "Damage" && state != "DeathPending" && state != "Death")
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
            if (currentState != "Attack")
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
                if (currentState != "Attack")
                    rotationComponent->SetDirection(forward);
            }
            break;
        }
        }
        characterMovementComponent->SetMoveSpeedScale(directionSpeedScale);
        characterMovementComponent->SetMoveDirection(moveDir);
    }
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

#if 0
    switch (locomotionMode)
    {
    case LocomotionMode::None:
        controller->SetUseBlendSpace(false);
        break;

    case LocomotionMode::TPSWalk:
        controller->SetUseBlendSpace(false);
        PlayBodyAnimation("Walk_Fwd", true);
        break;

    case LocomotionMode::TPSRun:
        controller->SetUseBlendSpace(false);
        PlayBodyAnimation("Jog_Fwd", true);
        break;

    case LocomotionMode::LockOnBlendWalk:
    case LocomotionMode::LockOnBlendRun:
        controller->SetUseBlendSpace(true);
        break;
    }

#else
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

    case LocomotionMode::LockOnBlendWalk:
    {

        controller->SetUseBlendSpace(true);
        characterMovementComponent->ResetFixedSpeed();
    }

    break;

    case LocomotionMode::LockOnBlendRun:
    {
        characterMovementComponent->ResetFixedSpeed();
        controller->SetUseBlendSpace(true);
    }
    break;

    case LocomotionMode::Dash:
        controller->SetUseBlendSpace(false);
        GetStateMachine()->ChangeState("Dash");
        break;
    case LocomotionMode::Idle:
        controller->SetUseBlendSpace(false);
        GetStateMachine()->ChangeState("Idle");
        break;
    }
#endif // 0

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

// 入力を消費する処理
void Player::ConsumeActionRequest(ActionType expectedType)
{
    if (bufferCommand.type != expectedType)
    {
        Logger::Log(Logger::LogCategory::Gameplay,
            "[ActionRequest][ConsumeRejected] expected=" + std::string(ToString(expectedType)) +
            " current=" + std::string(ToString(bufferCommand.type)) +
            " state=" + stateMachine_->GetStateName());
        return;
    }
    if (expectedType == ActionType::Dodge)
    {
        dodgeDirection = bufferCommand.dodgeDirection;
        dodgeWorldDirection = bufferCommand.dodgeWorldDirection;
        useDodgeWorldDirection = bufferCommand.useDodgeWorldDirection;
    }
    Logger::Log(Logger::LogCategory::Gameplay,
        "[ActionRequest][Consumed] action=" + std::string(ToString(bufferCommand.type)) +
        " remainTime=" + std::to_string(bufferCommand.remainTime) +
        " state=" + stateMachine_->GetStateName());
    bufferCommand = {};
}

void Player::BeginDodgeDebug()
{
    dodgeDebugElapsed = 0.0f;
    normalDodgeRecordedThisDodge = false;
    justDodgeRecordedThisDodge = false;
    ++dodgeDebugAttempts;
}

void Player::UpdateDodgeDebug(float deltaTime)
{
    dodgeDebugElapsed += (std::max)(0.0f, deltaTime);
}

void Player::RecordNormalDodgeDebug()
{
    if (!normalDodgeRecordedThisDodge)
    {
        normalDodgeRecordedThisDodge = true;
        ++dodgeDebugNormalCount;
    }
}

//当たった時の処理
bool Player::TryTakeDamage(int damage, const DirectX::XMFLOAT3& attackerPosition)
{
    if (invincibleWindow)
    {
        if (stateMachine_ && std::string(stateMachine_->GetStateName()) == "Dodge")
            RecordNormalDodgeDebug();
        Logger::Log(Logger::LogCategory::Gameplay,
            "[PlayerDamage][Rejected] reason=invincibleWindow hp=" + std::to_string(hp));
        return false;
    }
    if (invincible)
    {
        Logger::Log(Logger::LogCategory::Gameplay,
            "[PlayerDamage][Rejected] reason=invincible hp=" + std::to_string(hp));
        return false;
    }

    const std::string currentState = stateMachine_->GetStateName();
    if (currentState == "Damage" || currentState == "DeathPending" || currentState == "Death")
    {
        Logger::Log(Logger::LogCategory::Gameplay,
            "[PlayerDamage][Rejected] reason=damage_state state=" + currentState);
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
    damageKnockbackDirection = direction;

    const int hpBeforeDamage = hp;
    const int appliedDamage = (std::max)(0, damage);
    hp = (std::max)(0, hp - appliedDamage);
    if (hp < hpBeforeDamage)
    {
        delayedHp = static_cast<float>(hpBeforeDamage);
        delayedHpDelayTimer = delayedHpDelayDuration;
        StartDamageFlash();
    }
    ++dodgeDebugDamageCount;
    CoreAudio::PlayOneShot("./Data/Sound/SE/player_damage_voice.wav", 0.3f);
    CoreAudio::PlayOneShot("./Data/Sound/SE/player_damage.wav", 0.5f);
    // コントローラー振動
    InputSystem::SetVibration(0.8f, 0.15f);
    ClearActionRequest("damage_applied");
    Logger::Log(U8("プレイヤーにダメージ！ HP:") + std::to_string(hp));
    //if (sparkComponent)
    //{
    //    sparkComponent->Play();
    //}

    const char* targetState = hp > 0 ? "Damage" : "DeathPending";
    Logger::Log(Logger::LogCategory::Gameplay, "[PlayerDamage][Applied] targetState=" + std::string(targetState) + " knockback=" + std::to_string(direction.x) + "," +
        std::to_string(direction.y) + "," + std::to_string(direction.z));
    stateMachine_->ChangeState(targetState);
    return true;
}

void Player::StartDamageFlash()
{
    damageFlashTimer = (std::max)(0.0f, damageFlashDuration);
    ApplyDamageFlash(damageFlashTimer > 0.0f ? 1.0f : 0.0f);
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

void Player::UpdateLowHpEffects()
{
    const bool shouldBeLowHp = hp > 0 && hp <= lowHpThreshold;
    if (!shouldBeLowHp)
    {
        if (lowHpActive || heartbeatTimer > 0.0f || lowHpPulseTimer > 0.0f ||
            lowHpPulseFlashAmount > 0.0f)
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

void Player::ApplyDamageFlash(float flashAmount)
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
    applyToMesh(skeletalMeshBlendComponent);
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
    stateMachine_->ChangeState("KnockBack");
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
void Player::ClearActionRequest(const char* reason)
{
    Logger::Log(Logger::LogCategory::Gameplay,
        "[ActionRequest][Cleared] action=" + std::string(ToString(bufferCommand.type)) +
        " reason=" + (reason ? reason : "unknown") +
        " state=" + stateMachine_->GetStateName());
    bufferCommand = {};
}

// 攻撃開始時の処理
void Player::StartAttack()
{
    isAttackActive = true;
    hitActors.clear();
}

// 攻撃終了時の処理
void Player::EndAttack()
{
    isAttackActive = false;
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

bool Player::CanAcceptInitialRushInput() const
{
    return rushInputAccepting &&
        justDodgeSuccess &&
        stateMachine_ &&
        std::string(stateMachine_->GetStateName()) == "Dodge" &&
        !rushTarget.expired();
}

bool Player::CanShowRushComboGuide() const
{
    return stateMachine_ &&
        std::string(stateMachine_->GetStateName()) == "Rush" &&
        !rushTarget.expired();
}

bool Player::CanShowInitialRushGuide() const
{
    if (CanAcceptInitialRushInput())
        return true;

    return stateMachine_ &&
        std::string(stateMachine_->GetStateName()) == "Dodge" &&
        justDodgeSuccess &&
        rushRequestedDebug &&
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

void Player::SetRushInputAcceptance(bool accepting)
{
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

void Player::SetRushInputDebugState(bool judgeSuccess, bool rushRequested)
{
    rushJudgeSuccessDebug = judgeSuccess;
    rushRequestedDebug = rushRequested;
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

void Player::UpdateLockOnGuideUI()
{
    using namespace DirectX;
    if (!lockOnGuideArrowImageComponent || !lockOnGuideButtonImageComponent ||
        !IsBossBattle() || GetHp() <= 0 || IsInWinState())
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
// ジャスト回避成功時の処理
void Player::StartJustDodgeSuccess(const std::shared_ptr<Enemy>& enemy)
{
    int debugFrame = -1;
#ifdef USE_IMGUI
    if (ImGui::GetCurrentContext())
        debugFrame = ImGui::GetFrameCount();
#endif
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
    if (!justDodgeRecordedThisDodge)
    {
        justDodgeRecordedThisDodge = true;
        ++dodgeDebugJustCount;
    }
    lastJustDodgeValid = true;
    lastJustDodgeTime = dodgeDebugElapsed;
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
    Logger::Log(Logger::LogCategory::Gameplay, std::format(
        "[JustDodgeSuccess] frame={} this={} enemy={} attackSequenceId={} justDodgeSuccess={} state={} animation={} animationTime={}",
        debugFrame,
        static_cast<const void*>(this),
        static_cast<const void*>(enemy.get()),
        attackSequenceId,
        justDodgeSuccess,
        stateMachine_ ? stateMachine_->GetStateName() : "",
        animationName,
        animationTime));

    // SEの再生
    Logger::Log(Logger::LogCategory::Gameplay, std::format(
        "[JustDodgeSE] frame={} sound=just_dodge3.wav", debugFrame));
    CoreAudio::PlayOneShot("./Data/Sound/SE/just_dodge3.wav", 1.5f);

    // ジャスト回避成功フラグをオンにする
    justDodgeSuccess = true;
    CapturePlayerPoseGhost();
    // スローモーションにする
    // rush時のtargetを保存する
    rushTarget = enemy;

    const float scale = std::clamp(justDodgeTimeScale, 0.10f, 1.0f);
    enemy->SetTimeScale(scale);
    SetTimeScale(scale);
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

    // 画面の色を変える

    // UIを表示する
    //rushButtonImageComponent->SetVisible(true);
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
    if (bossSlowPhase == JustDodgeSlowPhase::Inactive ||
        bossSlowPhase == JustDodgeSlowPhase::Return)
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

// ラッシュ受付期間終了
void Player::EndRushAttackInput()
{
    // UIを非表示にする
    //rushButtonImageComponent->SetVisible(false);

    // SEの再生を終了する

    // playerのスロー再生を終了する


}

// インタラクト対象検索
IInteractable* Player::FindInteractable()
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
