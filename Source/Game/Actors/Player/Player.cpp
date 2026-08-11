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
    std::string parentName = "skeletalComponent";
    // 描画用コンポーネントを追加
    {
        PROFILE_SCOPE("Create PlayerModel");

        skeletalMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
        skeletalMeshComponent->SetModel("./Data/Models/Characters/PlayerNoWeapon/player.gltf", false, true);
        skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Player;   // オブジェクトの種類を Player に設定
        skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 20.9f;   // 自己発光の強さを設定
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
        controller->AddAnimation("Run_Combat_Loop_F", 30);
        controller->AddAnimation("Run_Fast_Combat_Loop_F", 31);
        controller->AddAnimation("Idle_Combat_to_Idle_Seq_0", 32);
        controller->AddAnimation("Idle_Idle_to_Combat_Seq_0", 33);
        controller->AddAnimation("anim_OpenDoor_L_0", 34);
        controller->AddAnimation("anim_OpenDoor_R_0", 35);
        controller->AddAnimation("Idle_Noise_A_0", 36);
        controller->AddAnimation("Idle_Noise_B_0", 37);
        controller->AddAnimation("Aurora_Combat", 38);
        controller->AddAnimation("Emote_Ice_Sculpture1_0", 39);
        controller->AddAnimation("Level_Start1_0", 40);
        controller->AddAnimation("Recall_0", 41);
        controller->AddAnimation("Level_Start_Cut", 42);
        controller->AddAnimation("Jog_Bwd", 43);
        controller->AddAnimation("Jog_Left", 44);
        controller->AddAnimation("Jog_Right", 45);
        controller->AddAnimation("Walk_Bwd1", 46);
        controller->AddAnimation("Walk_Fwd1", 47);
        controller->AddAnimation("Walk_Left1", 48);
        controller->AddAnimation("Walk_Right1", 49);
        controller->AddAnimation("Jog_Fwd1", 50);
        controller->AddAnimation("Walk_Fwd1", 51);
        controller->AddAnimation("Sprint_Fwd5", 52);
        controller->AddAnimation("Walk_Fwd2", 53);  // これだめ
        controller->AddAnimation("Jog_Fwd2", 54);// これだめ
        controller->AddAnimation("Jog_Bwd2", 55);
        controller->AddAnimation("Jog_BwdLeft5", 56);
        controller->AddAnimation("Jog_BwdRight5", 57);
        controller->AddAnimation("Jog_FwdLeft5", 58);
        controller->AddAnimation("Jog_FwdRight5", 59);
        controller->AddAnimation("Jog_Left2", 60);
        controller->AddAnimation("Jog_Right2", 61);
        controller->AddAnimation("Rush_Attack_Fast_A", 62);
        controller->AddAnimation("Rush_Attack_Fast_B", 63);
        controller->AddAnimation("Rush_Attack_Fast_C", 64);
        controller->AddAnimation("Rush_Attack_Fast_End", 65);
        controller->AddAnimation("Walk_Fwd3", 66);
        controller->AddAnimation("Jump", 67);
        controller->AddAnimation("Rush_Attack_Fast_D", 68);
        controller->AddAnimation("Jog_Bwd5", 69);
        controller->AddAnimation("Jog_Fwd5", 70);
        controller->AddAnimation("Jog_Left5", 71);
        controller->AddAnimation("Jog_Right5", 72);

        controller->AddAnimation("1_Jog_Bwd", 73);
        controller->AddAnimation("1_Jog_BwdLeft", 74);
        controller->AddAnimation("1_Jog_BwdRight", 75);
        controller->AddAnimation("1_Jog_Fwd", 76);
        controller->AddAnimation("1_Jog_FwdLeft", 77);
        controller->AddAnimation("1_Jog_FwdRight", 78);
        controller->AddAnimation("1_Sprint_Fwd", 79);
        controller->AddAnimation("1_Walk_Bwd", 80);
        controller->AddAnimation("1_Walk_BwdLeft", 81);
        controller->AddAnimation("1_Walk_BwdRight", 82);
        controller->AddAnimation("1_Walk_Fwd", 83);
        controller->AddAnimation("1_Walk_FwdLeft", 84);
        controller->AddAnimation("1_Walk_FwdRight", 85);

        controller->AddAnimation("Attack1", 86);
        controller->AddAnimation("Attack2", 87);
        controller->AddAnimation("Attack3", 88);

        controller->AddAnimation("1_Jog_BwdLeft45", 89);
        controller->AddAnimation("1_Jog_BwdRight45", 90);
        controller->AddAnimation("1_Jog_FwdLeft45", 91);
        controller->AddAnimation("1_Jog_FwdRight45", 92);

        controller->AddAnimation("0_Jog_Bwd", 93);
        controller->AddAnimation("0_Jog_BwdLeft45", 94);
        controller->AddAnimation("0_Jog_BwdLeft90", 95);
        controller->AddAnimation("0_Jog_BwdRight45", 96);
        controller->AddAnimation("0_Jog_BwdRight90", 97);
        controller->AddAnimation("0_Jog_Fwd", 98);
        controller->AddAnimation("0_Jog_FwdLeft45", 99);
        controller->AddAnimation("0_Jog_FwdLeft90", 100);
        controller->AddAnimation("0_Jog_FwdRight45", 101);
        controller->AddAnimation("0_Jog_FwdRight90", 102);
        controller->AddAnimation("Sprint_Fwd", 103);
        controller->AddAnimation("Walk_Fwd", 104);

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
        stateMachine_->RegisterState(std::make_unique<PlayerDeathState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerRushState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerJumpState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerJumpAttackState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerInteractState>(this));
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
        height = size.y+0.5f;
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

    // 火花エフェクト用のコンポーネントを追加
    sparkComponent = this->AddComponent<class ParticleComponent>("particleComponent", parentName);
    sparkComponent->Load("./Data/Effect/Files/DarkStageSparkEffect.json");


    // カメラの目の位置のコンポーネントを追加
    cameraEyeComponent = AddComponent<SceneComponent>("cameraEyeComponent", parentName);

    // カメラの注視点の位置のコンポーネントを追加
    cameraTargetComponent = AddComponent<SceneComponent>("cameraTargetComponent", parentName);
    cameraTargetComponent->SetRelativeLocationDirect({ 0.0f,1.0f,0.0f });
    // 軌跡初期化
    trail.Initialize();
    // ラッシュ時のUIを作成
    auto uiManager = GetOwnerScene()->GetUIManager();
    rushButtonImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/Y.png", "Y");
    rushButtonImageComponent->SetWorldPosition(rushPromptPosition);
    rushButtonImageComponent->SetScale({ 1.0f,1.0f });
    rushButtonImageComponent->SetSize({ rushPromptIconSize, rushPromptIconSize });
    rushButtonImageComponent->SetPivot({ 0.5f,0.5f });
    rushButtonImageComponent->SetVisible(false);
    uiManager->Add(rushButtonImageComponent);

    rushPromptTextComponent = std::make_shared<UITextComponent>("RushPromptText");
    rushPromptTextComponent->SetText(L"RUSH");
    rushPromptTextComponent->SetWorldPosition({ rushPromptPosition.x + rushPromptTextOffset.x,rushPromptPosition.y + rushPromptTextOffset.y });
    rushPromptTextComponent->SetPivot({ 0.0f, 0.5f });
    rushPromptTextComponent->SetScale({ 0.8f, 0.8f });
    rushPromptTextComponent->SetVisible(false);
    uiManager->Add(rushPromptTextComponent);
    SetEulerRotation({ 0.0f,90.0f,0.0f });

    // 操作説明UIを入れる
    operateUiComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/operate_ui.png", "operate_ui");
    operateUiComponent->SetWorldPosition({ 1000, 1000 });
    operateUiComponent->SetSize({ 2400, 300 });
    operateUiComponent->SetScale({ 0.3f,0.3f });
    operateUiComponent->SetPivot({ 0.5f,0.5f });
    uiManager->Add(operateUiComponent);

}

void Player::Update(float deltaTime)
{
    using namespace DirectX;

    // プレイヤーの透明化処理
    if (moviePerform)
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
    if (InputSystem::GetInputState("1"))
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
                    enemy->TakeDamage(1);
                    if (selectedEffectHit && hit.hasPosition && hit.hasNormal)
                    {
                        enemy->SpawnHitEffect(hit.hitPoint, hit.normal, playerPos);
                    }
                    else
                    {
                        if (swordHitDebug)
                            Logger::Log(Logger::LogCategory::Physics,
                                "[SwordHit] effect skipped: no valid hit position/normal");
                    }
                    hitActors.emplace(enemy);

                }
            }
        }
    }

    // 剣のエミッシブを表示する
    if (swordMeshComponent)
    {// 剣にエミッシブを追加
        swordMeshComponent->plusAlphaCBuffer->data.emissionPower = 8.0f;
        //swordMeshComponent->plusAlphaCBuffer->data.emissionPower = swordEmissivePower;
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
            ghost.swordMeshComp->plusAlphaCBuffer->data.cpuColor = { swordGhostColor.x,swordGhostColor.y,swordGhostColor.z, ghost.alpha };
            ghost.swordMeshComp->plusAlphaCBuffer->data.effectParameters.edgeColor = { ghostEdgeColor.x,ghostEdgeColor.y,ghostEdgeColor.z,1.0f };
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


    // 入力処理
    CaptureActionRequest(deltaTime);

    // 現在フレームの入力から移動方向を確定し、その直後に一度だけ位置を更新する。
    UpdateMovement();

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

void Player::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::Checkbox(U8("ボス戦カメラ"), &isBossBattle);
    ImGui::DragFloat(U8("剣の球の当たり判定の半径"), &weaponSphereRadius, 0.05f);
    ImGui::Checkbox("Sword Hit Debug", &swordHitDebug);
    ImGui::DragFloat("dodgeSpeed", &dodgeSpeed, 0.1f);
    ImGui::DragFloat("dodgeDuration", &dodgeDuration, 0.1f);
    ImGui::DragFloat(U8("剣の軌跡が残る時間"), &trailRemainTime, 0.1f);
    ImGui::DragFloat(U8("剣の残像が残る時間"), &ghostFadeTime, 0.1f);
    ImGui::ColorEdit3(U8("剣の残像の色"), &swordGhostColor.x);
    ImGui::DragFloat(U8("残像のemissiveColor"), &swordGhostEmissive, 0.1f);
    ImGui::DragFloat(U8("残像を出す間隔"), &ghostInterval, 0.001f, 0.0f, 1.0f, "%.5f");
    ImGui::DragFloat(U8("剣の残像の輪郭"), &ghostEdgeWidth);
    ImGui::ColorEdit3(U8("剣の残像のエッジの色"), &ghostEdgeColor.x);
    ImGui::ColorEdit3(U8("剣の残像の内部の色"), &ghostInnerColor.x);
    ImGui::DragFloat(U8("ボス戦時のカメラ距離"), &bossBattleCameraDistance, 0.5f);
    ImGui::DragFloat3(U8("ボス戦時のオフセット"), &bossBattleCameraOffset.x, 0.5f);
    ImGui::SliderFloat("Walk Speed", &walkSpeed, 0.25f, 10.0f);
    ImGui::SliderFloat("Run Speed", &runSpeed, 0.25f, 15.0f);
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
        ImGui::SliderFloat("Hold Duration", &justDodgeSlowHoldDuration, 0.00f, 1.00f, "%.2f sec");
        ImGui::SliderFloat("Return Duration", &justDodgeSlowReturnDuration, 0.00f, 0.50f, "%.2f sec");
        ImGui::SliderFloat("Rush Boss Slow Scale", &rushBossSlowScale, 0.10f, 1.00f);
        ImGui::SliderFloat("Rush Boss Return Duration", &rushBossReturnDuration, 0.00f, 0.50f, "%.2f sec");
        ImGui::Text("Player Phase: %d / Scale: %.3f", static_cast<int>(playerSlowPhase), GetTimeScale());
        ImGui::Text("Boss Phase: %d", static_cast<int>(bossSlowPhase));
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Rush Input Prompt"))
    {
        ImGui::DragFloat("Position X", &rushPromptPosition.x, 1.0f);
        ImGui::DragFloat("Position Y", &rushPromptPosition.y, 1.0f);
        ImGui::DragFloat("Icon Size", &rushPromptIconSize, 1.0f, 1.0f, 512.0f);
        ImGui::DragFloat2("Text Offset", &rushPromptTextOffset.x, 1.0f);
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
    ImGui::DragFloat("transparencyMinAlpha", &transparencyMinAlpha, 0.05f);
    ImGui::DragFloat("transparencyMaxAlpha", &transparencyMaxAlpha, 0.05f);
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

void Player::OnAnimationNotifyEvent(const AnimationNotifyEvent& event)
{
    switch (event.type)
    {
    case AnimationNotifyEvent::Type::PlaySE:
    {
        if (event.parameter != "")
        {
            std::string audioPath = "./Data/Sound/SE/" + event.parameter + ".wav";
            CoreAudio::PlayOneShot(audioPath, event.value);
        }
    }
    break;
    case AnimationNotifyEvent::Type::SpawnEffect:
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
    // 入力を受け付ける
    InputSystem::SetInputEnabled(true);
    //　イベントが終わったのでplayerの透過処理を戻す
    this->moviePerform = false;
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
    if (currentState == "Damage" || currentState == "Death")
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
    if (InputSystem::GetInputState("Interact", InputStateMask::Trigger))
    {
        StoreActionRequest(ActionType::Interact, 0.3f);
    }
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
    case ActionType::Interact:
        targetState = "Interact";
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
    const bool suppressMovementInput = currentState == "Damage" || currentState == "Death";

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
                state != "Damage" && state != "Death")
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

//当たった時の処理
bool Player::TryTakeDamage(int damage, const DirectX::XMFLOAT3& attackerPosition)
{
    if (invincibleWindow)
    {
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
    if (currentState == "Damage" || currentState == "Death")
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

    const int appliedDamage = (std::max)(0, damage);
    hp = (std::max)(0, hp - appliedDamage);
    CoreAudio::PlayOneShot("./Data/Sound/SE/player_damage_voice.wav",0.3f);
    CoreAudio::PlayOneShot("./Data/Sound/SE/player_damage.wav",0.5f);
    ClearActionRequest("damage_applied");
    Logger::Log(U8("プレイヤーにダメージ！ HP:") + std::to_string(hp));
    //if (sparkComponent)
    //{
    //    sparkComponent->Play();
    //}

    const char* targetState = hp > 0 ? "Damage" : "Death";
    Logger::Log(Logger::LogCategory::Gameplay,
        "[PlayerDamage][Applied] targetState=" + std::string(targetState) +
        " knockback=" + std::to_string(direction.x) + "," +
        std::to_string(direction.y) + "," + std::to_string(direction.z));
    stateMachine_->ChangeState(targetState);
    return true;
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
        if (rushButtonImageComponent) rushButtonImageComponent->SetVisible(false);
        if (rushPromptTextComponent) rushPromptTextComponent->SetVisible(false);
    }
}

void Player::SetRushInputDebugState(bool judgeSuccess, bool rushRequested)
{
    rushJudgeSuccessDebug = judgeSuccess;
    rushRequestedDebug = rushRequested;
}

void Player::UpdateRushPromptUI()
{
    if (!rushButtonImageComponent || !rushPromptTextComponent)
    {
        return;
    }

    const bool initialRush = CanShowInitialRushGuide();
    const bool comboGuide = CanShowRushComboGuide();
    const bool visible = initialRush || comboGuide;
    if (!visible)
    {
        rushPromptAlpha = 0.0f;
        rushButtonImageComponent->SetVisible(false);
        rushPromptTextComponent->SetVisible(false);
        return;
    }

    if (rushPromptFadeInDuration <= FLT_EPSILON)
    {
        rushPromptAlpha = 1.0f;
    }
    else
    {
        rushPromptAlpha = std::clamp(
            rushPromptAlpha + Time::UnscaledDeltaTime() / rushPromptFadeInDuration,
            0.0f, 1.0f);
    }

    rushButtonImageComponent->SetWorldPosition(rushPromptPosition);
    rushButtonImageComponent->SetSize({ rushPromptIconSize, rushPromptIconSize });
    rushButtonImageComponent->SetColor(
        DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, rushPromptAlpha });
    rushButtonImageComponent->SetVisible(true);

    rushPromptTextComponent->SetWorldPosition({
        rushPromptPosition.x + rushPromptTextOffset.x,
        rushPromptPosition.y + rushPromptTextOffset.y });
    rushPromptTextComponent->SetText(initialRush ? L"RUSH" : L"ATTACK");
    rushPromptTextComponent->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, rushPromptAlpha });
    rushPromptTextComponent->SetVisible(true);
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
