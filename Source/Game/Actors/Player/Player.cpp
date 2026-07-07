#include "pch.h"
#include "Player.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "../External/imgui/imgui.h"
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
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/Actors/Stage/Stage.h"
#include "Game/DarkGame/Interactable.h"
#include "Game/DarkGame/DarkActors/InteractableActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Physics/CollisionFunction.h"


void Player::Initialize(const Transform& transform)
{
    std::string parentName = "skeletalComponent";
    // 描画用コンポーネントを追加
    {
        PROFILE_SCOPE("Create PlayerModel");

        skeletalMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
        //skeletalMeshComponent->SetModel("./Data/Models/Characters/Aurora_FrozenHealth/animation.gltf", false, true);
        //skeletalMeshComponent->SetModel("./Data/Models/Characters/Player/player.gltf", false, true);
        skeletalMeshComponent->SetModel("./Data/Models/Characters/PlayerNoWeapon/player.gltf", false, true);
        skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Player;   // オブジェクトの種類を Player に設定
        skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 20.9f;   // 自己発光の強さを設定

        skeletalMeshComponent->SetIsCastShadow(false);
        skeletalMeshComponent->SetIsShadowMap(true);
#if 1
        for (auto& material : skeletalMeshComponent->model->materials)
        {
            if (material.name == "M_Aurora_Hair_Blonde_FrozenHearth")
            {// 髪の毛だったら
                //material.overridePipelineName = "characterFurAndHairSkeletalMesh";
                material.materialType = MaterialType::Hair;
            }
            else if (material.name == "M_Aurora_Fur_FrozenHearth")
            {// 髪の毛だったら
                material.overridePipelineName = "characterFurAndHairSkeletalMesh";
                material.materialType = MaterialType::Fur;
            }
            else if (material.name == "MI_Aurora_Sword_FrozenHearth")
            {// 髪の毛だったら
                material.overridePipelineName = "DarkStagePlayerWeaponPS";
            }
        }
#endif // 0
    }
    {
        PROFILE_SCOPE("Create PlayerAnimationController");

        // ルートノードを設定する
        int rootNodeIndex = skeletalMeshComponent->FindIndexByName("root");
        // アニメーションコントローラーを作成
        auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootNodeIndex);
        controller->AddAnimation("Idle", 0);
        controller->AddAnimation("Jog_Fwd", 1);
        controller->AddAnimation("Roll_front_0", 2);
        controller->AddAnimation("Roll_back_0", 3);
        controller->AddAnimation("Roll_left_0", 4);
        controller->AddAnimation("Roll_right_0", 5);
        controller->AddAnimation("Primary_Attack_Fast_A", 6);
        controller->AddAnimation("Primary_Attack_Fast_B", 7);
        controller->AddAnimation("Primary_Attack_Fast_C", 8);
        controller->AddAnimation("Primary_Attack_Fast_D", 9);
        controller->AddAnimation("Anim_DKF_Attack_01", 10);
        controller->AddAnimation("Anim_DKF_Attack_02", 11);
        controller->AddAnimation("Anim_DKF_Attack_03", 12);
        controller->AddAnimation("Ability_E_0", 13);
        controller->AddAnimation("Ability_Q_0", 14);
        controller->AddAnimation("Ability_R1_0", 15);
        controller->AddAnimation("Primary_Attack_Fast_D1_1", 16);
        controller->AddAnimation("Ability_RMB_Bwd_0", 17);
        controller->AddAnimation("Ability_RMB_BwdLeft_0", 18);
        controller->AddAnimation("Ability_RMB_BwdRight_0", 19);
        controller->AddAnimation("Ability_RMB_FwdRight_0", 20);
        controller->AddAnimation("Ability_RMB_Left_0", 21);
        controller->AddAnimation("Ability_RMB_Right_0", 22);
        controller->AddAnimation("Ability_RWB_Fwd_0", 23);
        controller->AddAnimation("Jump_Land_0", 24);
        controller->AddAnimation("Jump_Pad_0", 25);
        controller->AddAnimation("Jump_Recovery_0", 26);
        controller->AddAnimation("Jump_Start_0", 27);
        controller->AddAnimation("Attack_Air", 28);
        controller->AddAnimation("Combo_Attack_01_01", 29);
        controller->AddAnimation("Combo_Attack_01_02", 30);
        controller->AddAnimation("Combo_Attack_01_03", 31);
        controller->AddAnimation("Combo_Attack_01_04", 32);
        controller->AddAnimation("Combo_Attack_02_01", 33);
        controller->AddAnimation("Combo_Attack_02_02", 34);
        controller->AddAnimation("Combo_Attack_02_03", 35);
        controller->AddAnimation("Combo_Attack_02_04", 36);
        controller->AddAnimation("Combo_Attack_03_01", 37);
        controller->AddAnimation("Combo_Attack_03_02", 38);
        controller->AddAnimation("Combo_Attack_03_03", 39);
        controller->AddAnimation("Combo_Attack_03_04", 40);
        controller->AddAnimation("Combo_Attack_04_01", 41);
        controller->AddAnimation("Combo_Attack_04_02", 42);
        controller->AddAnimation("Combo_Attack_04_03", 43);
        controller->AddAnimation("Combo_Attack_04_04", 44);
        controller->AddAnimation("Combo_Attack_05_01", 45);
        controller->AddAnimation("Combo_Attack_05_02", 46);
        controller->AddAnimation("Combo_Attack_05_03", 47);
        controller->AddAnimation("Combo_Attack_05_04", 48);
        controller->AddAnimation("Dodge_Combat_F", 49);
        controller->AddAnimation("Hit_Combat_B", 50);
        controller->AddAnimation("Hit_Combat_Death", 51);
        controller->AddAnimation("Hit_Combat_F", 52);
        controller->AddAnimation("Hit_Combat_L", 53);
        controller->AddAnimation("Hit_Combat_Large_B", 54);
        controller->AddAnimation("Hit_Combat_Large_Death", 55);
        controller->AddAnimation("Hit_Combat_Large_F", 56);
        controller->AddAnimation("Hit_Combat_Large_L", 57);
        controller->AddAnimation("Hit_Combat_Large_R", 58);
        controller->AddAnimation("Hit_Combat_R", 59);
        controller->AddAnimation("Run_Combat_Loop_F", 60);
        controller->AddAnimation("Run_Fast_Combat_Loop_F", 61);
        controller->AddAnimation("Idle_Combat_to_Idle_Seq_0", 62);
        controller->AddAnimation("Idle_Idle_to_Combat_Seq_0", 63);
        controller->AddAnimation("anim_OpenDoor_L_0", 64);
        controller->AddAnimation("anim_OpenDoor_R_0", 65);
        controller->AddAnimation("Idle_Noise_A_0", 66);
        controller->AddAnimation("Idle_Noise_B_0", 67);
        controller->AddAnimation("Aurora_Combat", 68);
        controller->AddAnimation("Emote_Ice_Sculpture1_0", 69);
        controller->AddAnimation("Level_Start1_0", 70);
        controller->AddAnimation("Recall_0", 71);

        controller->AddNotifyEvent("Primary_Attack_Fast_A", 0.17f, AnimationNotifyEvent::Type::PlaySE, "player_attack");
        controller->AddNotifyState("Primary_Attack_Fast_A", 0.17f, 0.23f, AnimationNotifyState::Type::HitBox);
        controller->AddNotifyState("Primary_Attack_Fast_A", 0.12f, 0.583f, AnimationNotifyState::Type::InputWindow);
        controller->AddNotifyState("Primary_Attack_Fast_A", 0.4f, 0.583f, AnimationNotifyState::Type::TransitionWindow);

        controller->AddNotifyEvent("Primary_Attack_Fast_B", 0.138f, AnimationNotifyEvent::Type::PlaySE, "player_attack");
        controller->AddNotifyState("Primary_Attack_Fast_B", 0.138f, 0.265f, AnimationNotifyState::Type::HitBox);
        controller->AddNotifyState("Primary_Attack_Fast_B", 0.138f, 0.583f, AnimationNotifyState::Type::InputWindow);
        controller->AddNotifyState("Primary_Attack_Fast_B", 0.4f, 0.583f, AnimationNotifyState::Type::TransitionWindow);

        controller->AddNotifyEvent("Primary_Attack_Fast_C", 0.15f, AnimationNotifyEvent::Type::PlaySE, "player_attack");
        controller->AddNotifyState("Primary_Attack_Fast_C", 0.132f, 0.27f, AnimationNotifyState::Type::HitBox);
        controller->AddNotifyState("Primary_Attack_Fast_C", 0.132f, 0.583f, AnimationNotifyState::Type::InputWindow);
        controller->AddNotifyState("Primary_Attack_Fast_C", 0.4f, 0.583f, AnimationNotifyState::Type::TransitionWindow);

        controller->AddNotifyEvent("Primary_Attack_Fast_D1_1", 0.4f, AnimationNotifyEvent::Type::PlaySE, "player_attack");
        controller->AddNotifyState("Primary_Attack_Fast_D1_1", 0.414f, 0.548f, AnimationNotifyState::Type::HitBox);
        controller->AddNotifyState("Primary_Attack_Fast_D1_1", 0.08f, 1.0f, AnimationNotifyState::Type::InputWindow);
        controller->AddNotifyState("Primary_Attack_Fast_D1_1", 0.52f, 1.0f, AnimationNotifyState::Type::TransitionWindow);

        controller->AddCombo("Primary_Attack_Fast_A", "Primary_Attack_Fast_B");
        controller->AddCombo("Primary_Attack_Fast_B", "Primary_Attack_Fast_C");
        controller->AddCombo("Primary_Attack_Fast_C", "Primary_Attack_Fast_D1_1");

        // ジャスト回避
        controller->AddNotifyState("Ability_RMB_Bwd_0", 0.16f, 0.53f, AnimationNotifyState::Type::JustDodgeWindow);
        controller->AddNotifyState("Ability_RMB_Bwd_0", 0.05f, 0.6f, AnimationNotifyState::Type::Invincible);
        controller->AddNotifyState("Ability_RMB_Bwd_0", 0.48f, 0.6f, AnimationNotifyState::Type::TransitionWindow);
        controller->AddNotifyEvent("Ability_RMB_Bwd_0", 0.16f, AnimationNotifyEvent::Type::PlaySE, "dodge_start");
        controller->AddNotifyEvent("Ability_RMB_Bwd_0", 0.48f, AnimationNotifyEvent::Type::PlaySE, "dodge_land");
#if 0
        controller->AddNotifyState("Ability_RMB_Fwd_0", 0.16f, 0.53f, AnimationNotifyState::Type::JustDodgeWindow);
        controller->AddNotifyState("Ability_RMB_Fwd_0", 0.05f, 0.6f, AnimationNotifyState::Type::Invincible);
        controller->AddNotifyState("Ability_RMB_Fwd_0", 0.48f, 0.6f, AnimationNotifyState::Type::TransitionWindow);
        controller->AddNotifyEvent("Ability_RMB_Fwd_0", 0.16f, AnimationNotifyEvent::Type::PlaySE, "dodge_start");
        controller->AddNotifyEvent("Ability_RMB_Fwd_0", 0.48f, AnimationNotifyEvent::Type::PlaySE, "dodge_land");

        controller->AddNotifyState("Ability_RMB_Left_0", 0.16f, 0.53f, AnimationNotifyState::Type::JustDodgeWindow);
        controller->AddNotifyState("Ability_RMB_Left_0", 0.05f, 0.6f, AnimationNotifyState::Type::Invincible);
        controller->AddNotifyState("Ability_RMB_Left_0", 0.48f, 0.6f, AnimationNotifyState::Type::TransitionWindow);
        controller->AddNotifyEvent("Ability_RMB_Left_0", 0.16f, AnimationNotifyEvent::Type::PlaySE, "dodge_start");
        controller->AddNotifyEvent("Ability_RMB_Left_0", 0.48f, AnimationNotifyEvent::Type::PlaySE, "dodge_land");

        controller->AddNotifyState("Ability_RMB_Right_0", 0.16f, 0.53f, AnimationNotifyState::Type::JustDodgeWindow);
        controller->AddNotifyState("Ability_RMB_Right_0", 0.05f, 0.6f, AnimationNotifyState::Type::Invincible);
        controller->AddNotifyState("Ability_RMB_Right_0", 0.48f, 0.6f, AnimationNotifyState::Type::TransitionWindow);
        controller->AddNotifyEvent("Ability_RMB_Right_0", 0.16f, AnimationNotifyEvent::Type::PlaySE, "dodge_start");
        controller->AddNotifyEvent("Ability_RMB_Right_0", 0.48f, AnimationNotifyEvent::Type::PlaySE, "dodge_land");
#endif // 0
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
        stateMachine_->RegisterState(std::make_unique<PlayerRushState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerJumpState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerJumpAttackState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerInteractState>(this));

        // ステートマシンを character に追加
        //this->SetStateMachine(stateMachine);
        // 初期ステートを設定
        stateMachine_->ChangeState("Idle");
    }

#if 1
    {
        PROFILE_SCOPE("Create PlayerCollision");

        // 敵からの攻撃を受ける当たり判定用のコンポーネントを追加
        std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("capsuleComponent", parentName);
        DirectX::XMFLOAT3 size = skeletalMeshComponent->GetModelSize();
        height = size.y;
        radius = size.x * 0.5f;
        capsuleComponent->SetRadiusAndHeight(radius, height);
        capsuleComponent->SetMass(mass);
        capsuleComponent->SetCapsuleAxis(ShapeComponent::CapsuleAxis::y);
        capsuleComponent->SetLayer(CollisionLayer::Player);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::EnemyWeapon, CollisionComponent::CollisionResponse::Trigger);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Floor, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Block);
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

        // 回転用コンポーネントを追加
        rotationComponent = this->AddComponent<class RotationComponent>("rotationComponent", parentName);
    }

    int weaponSocketNode = skeletalMeshComponent->FindIndexByName("weapon");

#if 1
    // 剣に当たり判定のコンポーネントを追加
    swordCollisionComp = AddComponent<CapsuleComponent>("SwordCollision", parentName);
    //DirectX::XMFLOAT3 size = { 0.6f,1.5f,1.0f };
    DirectX::XMFLOAT3 size = { 0.3f,1.5f,1.0f };
    swordCollisionComp->AttachToComponent(skeletalMeshComponent, weaponSocketNode); // "VB root_weapon"
    swordCollisionComp->SetRadiusAndHeight(size.x, size.y);
    swordCollisionComp->SetMass(mass);
    swordCollisionComp->SetCapsuleAxis(ShapeComponent::CapsuleAxis::z);
    swordCollisionComp->SetLayer(CollisionLayer::PlayerWeapon);
    swordCollisionComp->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Trigger);
    swordCollisionComp->SetResponseToLayer(CollisionLayer::Boss, CollisionComponent::CollisionResponse::Trigger);
    swordCollisionComp->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Trigger);
    swordCollisionComp->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Trigger);
    swordCollisionComp->SetCollisionOffsetY(height * 0.5f);
    swordCollisionComp->SetIsVisibleDebugBox(false);
    swordCollisionComp->SetRelativeLocationDirect({ -0.f, -0.f, 0.8f });
    swordCollisionComp->Initialize();
    swordCollisionComp->SetOnHitCallback([this](CollisionComponent* self, CollisionComponent* other)
        {
            if (!other)
            {
                Logger::Warning("other is nullptr");
                return;
            }

            uint32_t mask = CollisionHelper::ToBit(CollisionLayer::Enemy) | CollisionHelper::ToBit(CollisionLayer::Boss);

            // Enemyレイヤーか確認
            if (!(other->GetCollisionLayer() & mask))
                return;

            //Logger::Log(U8("swordCollisionComponent を通っている"));

            // 相手のActor取得
            Actor* actor = other->GetOwner();

            if (!actor)
            {
                Logger::Warning("actor is nullptr");
                return;
            }

            if (!hitBox)
            {
                //Logger::Log(U8("hitBoxがtrueではない！"));
                return;
            }

            if (hitActors.contains(actor))
            {// 一度当たったことがあった場合
                Logger::Log(U8("敵に当たったことがある"));
                return;
            }

            // Enemyへキャスト
            GruxEnemy* enemy = dynamic_cast<GruxEnemy*>(actor);

            if (!enemy)
                return;

            enemy->TakeDamage(10);
            hitActors.insert(actor);

        });

    swordPointComp = AddComponent<CapsuleComponent>("SwordPointComponent", "SwordCollision");
    swordPointComp->SetRelativeLocationDirect({ 0.0f,0.0f,0.6f });

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

#endif // 0

    // 剣のメッシュコンポーネントを追加
    swordMeshComponent = this->AddComponent<SkeletalMeshComponent>("Sword", parentName);
    swordMeshComponent->SetModel("./Data/Models/Weapons/PlayerSword/Sword.gltf", false, true);
    swordMeshComponent->AttachToComponent(skeletalMeshComponent, weaponSocketNode); // "VB root_weapon"

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

#if 0
    auto bowMeshComponent = this->AddComponent<SkeletalMeshComponent>("Bow", parentName);
    bowMeshComponent->SetModel("./Data/Models/Weapons/PlayerBow/AnimationBow.gltf", false, true);

    // 武器アニメーションコントローラーを作成
    auto weaponController = std::make_shared<AnimationController>(bowMeshComponent.get());
    weaponController->AddAnimation("Bow", 0);
    AddAnimationController("Weapon", weaponController);
    PlayAnimation("Weapon", "Bow");
#endif // 0
    // 火花エフェクト用のコンポーネントを追加
    sparkComponent = this->AddComponent<class ParticleComponent>("particleComponent", parentName);
    sparkComponent->Load("./Data/Effect/Files/DarkStageSparkEffect.json");

    // 走り用のSEのコンポーネントを追加
    runAudioComp = AddComponent<AudioSourceComponent>("runAudioComponent", parentName);
    runAudioComp->SetSource(L"./Data/Sound/SE/run_heel.wav");
    //runAudioComp->SetSource(L"./Data/Sound/SE/run.wav");
    runAudioComp->SetVolume(0.3f);
    runAudioComp->SetLoop(true);

}


void Player::Update(float deltaTime)
{
    using namespace DirectX;

    if (InputSystem::GetInputState("1"))
    {
        stateMachine_->ChangeState("Rush");
    }

    if (isAttackActive)
    {
        swordGhostElapsedTime += deltaTime;
        if (swordGhostElapsedTime >= ghostInterval)
        {
            swordGhostElapsedTime = 0.0f;
            ghosts[swordGhostIndex].world = swordMeshComponent->GetComponentWorldTransform().ToWorldTransform();
            ghosts[swordGhostIndex].alpha = 1.0f;
            ghosts[swordGhostIndex].isVisible = true;
            //ghosts[swordGhostIndex].swordMeshComp->SetWorldMatrixDirect(ghosts[swordGhostIndex].world);
            //ghosts[swordGhostIndex].swordMeshComp->UpdateTransformImmediate();
            swordGhostIndex++;
            if (swordGhostIndex >= ghosts.size())
            {
                swordGhostIndex = 0;
            }
        }
    }

    // 剣の残像用の剣のメッシュコンポーネント
    for (auto& ghost : ghosts)
    {
        ghost.alpha -= deltaTime * 2.0f;

        if (ghost.alpha <= 0)
        {
            ghost.alpha = 0;
            ghost.isVisible = false;
        }

        //ghost.swordMeshComp->SetIsVisible(ghost.isVisible);

        if (!ghost.isVisible)
            continue;

        //ghost.swordMeshComp->plusAlphaCBuffer->data.cpuColor.w = ghost.alpha;
        ghost.swordMeshComp->plusAlphaCBuffer->data.cpuColor = { 1,1,0, ghost.alpha };
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
    HandleInput(deltaTime);

    // これは絶対入れる　アニメーションの更新をしているから
    Character::Update(deltaTime);

    // 剣のデバックの当たり判定を描画するかどうか
    if (swordCollisionComp)
        swordCollisionComp->SetIsVisibleDebugShape(hitBox);

    FindInteractable();

    HitResultWithActor hit;

    bool isHit = false;

    DirectX::XMFLOAT3 swordRootPos = swordRootComponent->GetComponentLocation();
    DirectX::XMFLOAT3 swordMidPos = swordMiddleComponent->GetComponentLocation();
    DirectX::XMFLOAT3 swordTipPos = swordTipComponent->GetComponentLocation();

    isHit |= CollisionFunction::SphereRayCast(prevSwordRootPos, swordRootPos, hit, 0.2f, CollisionHelper::ToBit(CollisionLayer::Enemy));
    isHit |= CollisionFunction::SphereRayCast(prevSwordMidPos, swordMidPos, hit, 0.2f, CollisionHelper::ToBit(CollisionLayer::Enemy));
    isHit |= CollisionFunction::SphereRayCast(prevSwordTipPos, swordTipPos, hit, 0.2f, CollisionHelper::ToBit(CollisionLayer::Enemy));

    prevSwordRootPos = swordRootPos;
    prevSwordMidPos = swordMidPos;
    prevSwordTipPos = swordTipPos;

    if (isHit)
    {
        Logger::Log(U8("剣に敵が当たった"));
    }

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
    if (InputSystem::GetInputState("ok", InputStateMask::Trigger))
    {
        if (IInteractable* interactable = FindInteractable())
        {
            interactable->Interact();
        }
    }

    if (swordPointComp)
    {

        auto currentTip = swordPointComp->GetComponentLocation();

        if (hasPrevSwordTip)
        {
            CheckSwordLineHit(prevSwordTip, currentTip);
        }

        prevSwordTip = currentTip;
        hasPrevSwordTip = true;

        DebugRender::DrawSphere(swordPointComp->GetComponentLocation(), 0.1f, { 1,1,0,1 }, 0.0f, true);

        // 剣先取得
        XMFLOAT3 tip = swordPointComp->GetComponentLocation();

        // トレイル追加（毎フレーム）
        trailPoints.push_back({ tip, 0.3f }); // ←長さ調整

        // 更新
        for (auto& p : trailPoints)
        {
            p.life -= deltaTime;
        }

        // 削除
        trailPoints.erase(
            std::remove_if(trailPoints.begin(), trailPoints.end(),
                [](const TrailPoint& p) { return p.life <= 0; }),
            trailPoints.end());
    }

#if 1
    auto intent = inputComponent->GetIntent();
    //characterMovementComponent->SetMoveDirection({ 1,0,0 });
    DirectX::XMFLOAT3 moveDir = { 0,0,0 };

    if (auto camera = dynamic_cast<MainCamera*>(GetOwnerScene()->GetActiveCamera()))
    {
        auto camForward = camera->CameraForwardXZ();
        auto camRight = camera->CameraRightXZ();

        // 左スティック入力
        float stickX = intent.leftMove.x;
        float stickZ = intent.leftMove.z;

        // カメラ基準の移動方向
        moveDir.x = camForward.x * stickZ + camRight.x * stickX;
        moveDir.z = camForward.z * stickZ + camRight.z * stickX;
    }

    characterMovementComponent->SetMoveDirection(moveDir);
    rotationComponent->SetDirection(moveDir);

#endif // 0

}

void Player::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("dodgeSpeed", &dodgeSpeed, 0.1f);
    ImGui::DragFloat("dodgeDuration", &dodgeDuration, 0.1f);
    Character::DrawImGuiDetails();
#endif
}

void Player::OnAnimationNotifyBegin(const AnimationNotifyState& state)
{
    switch (state.type)
    {
    case AnimationNotifyState::Type::HitBox:
        Logger::Log(U8("当たり判定を開始しました"));
        hitBox = true;
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
    }
}

void Player::OnAnimationNotifyEnd(const AnimationNotifyState& state)
{
    switch (state.type)
    {
    case AnimationNotifyState::Type::HitBox:
        Logger::Log(U8("当たり判定を終了しました"));
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
    }
}

void Player::OnAnimationNotifyEvent(const AnimationNotifyEvent& event)
{
    switch (event.type)
    {
    case AnimationNotifyEvent::Type::PlaySE:
    {
        std::string audioPath = "./Data/Sound/SE/" + event.parameter + ".wav";
        CoreAudio::PlayOneShot(audioPath, 1.0f);
    }
    break;
    case AnimationNotifyEvent::Type::SpawnEffect:
        break;
    }
}

void Player::OnAnimationChanged()
{
    transitionWindow = false;  // ステート遷移してもいいかどうか
    comboQueued = false;   // コンボ攻撃がキューに入っているかどうか
    inputWindow = false;   // コンボ受付をするかどうか
    hitBox = false;     // 当たり判定
    justDodgeWindow = false;    // ジャスト回避を受け付けるかどうか
    invincibleWindow = false;   // 無敵状態かどうか
    justDodgeSuccess = false; // ジャスト回避が成功したかどうか
    hitActors.clear();
    //Logger::Log(U8("playerのAnimationが切り替わった"));
}

// アニメーションステート関連のフラグをリセットする
void Player::ResetAnimationStateFlag()
{
    transitionWindow = false;  // ステート遷移してもいいかどうか
    comboQueued = false;   // コンボ攻撃がキューに入っているかどうか
    inputWindow = false;   // コンボ受付をするかどうか
    hitBox = false;     // 当たり判定
    justDodgeWindow = false;    // ジャスト回避を受け付けるかどうか
    invincibleWindow = false;   // 無敵状態かどうか
    justDodgeSuccess = false; // ジャスト回避が成功したかどうか
    Logger::Log(U8("アニメーションステート関連のフラグをリセットする"));
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
void Player::HandleInput(float deltaTime)
{
    bufferCommand.remainTime -= deltaTime;
    if (bufferCommand.remainTime <= 0.0f)
    {
        bufferCommand.command = InputCommand::None;
    }
    if (InputSystem::GetInputState("Dodge", InputStateMask::Trigger))
    {
        bufferCommand.command = InputCommand::Dodge;
        dodgeDirection = inputComponent->GetMoveInput();
        bufferCommand.remainTime = 0.3f;
        return;
    }
    if (InputSystem::GetInputState("Attack", InputStateMask::Trigger))
    {
        bufferCommand.command = InputCommand::Attack;
        bufferCommand.remainTime = 0.3f;
        return;
    }

    if (InputSystem::GetInputState("Jump", InputStateMask::Trigger))
    {
        bufferCommand.command = InputCommand::Jump;
        bufferCommand.remainTime = 0.3f;
        return;
    }
    if (InputSystem::GetInputState("Interact", InputStateMask::Trigger))
    {
        bufferCommand.command = InputCommand::Interact;
        bufferCommand.remainTime = 0.3f;
    }
}

// 入力コマンドによってステートを変える
bool Player::TryHandleGlobalTransition()
{
    switch (bufferCommand.command)
    {
    case InputCommand::None:
        return false;
    case InputCommand::Attack:
        stateMachine_->ChangeState("Attack");
        return true;
    case InputCommand::Dodge:
        stateMachine_->ChangeState("Dodge");
        return true;
    case InputCommand::Jump:
        stateMachine_->ChangeState("Jump");
        return true;
    case InputCommand::Interact:
        stateMachine_->ChangeState("Interact");
        break;
    }
    return false;
}

// 入力を消費する処理
void Player::ConsumeBufferCommand()
{
    bufferCommand.command = InputCommand::None;
    bufferCommand.remainTime = 0.0f;
}

//当たった時の処理
void Player::TakeDamage(int damage)
{
#if 1
    if (invincibleWindow)
    {// 無敵状態ならダメージを受けない
        Logger::Log(U8("animationによる無敵状態ならダメージを受けない"));
        return;
    }
    if (invincible)
    {// 無敵状態ならダメージを受けない
        Logger::Log(U8("無敵状態ならダメージを受けない"));
        return;
    }
#endif // 0
    hp -= damage;
    Logger::Log(U8("プレイヤーにダメージ！ HP:") + std::to_string(hp));
    if (sparkComponent)
    {
        sparkComponent->Play();
    }
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

// ジャスト回避成功時の処理
void Player::StartJustDodgeSuccess(const std::shared_ptr<Enemy>& enemy)
{
    // ジャスト回避成功フラグをオンにする
    justDodgeSuccess = true;
    // スローモーションにする
    //Time::SetSlow(0.2f, 1.0f);
    enemy->SetTimeScale(0.2f);
    this->SetTimeScale(0.2f);

    rushInputTimer = 0.5f;

    // rush時のtargetを保存する
    rushTarget = enemy;

    // 画面の色を変える

    // UIを表示する
}

// インタラクト対象検索
IInteractable* Player::FindInteractable()
{
    IInteractable* best = nullptr;

    DirectX::XMFLOAT3 forward = GetForward(); // プレイヤー前方向

    for (auto actor : GetOwnerScene()->GetActorManager()->GetActorsOfType<InteractableActor>())
    {
        auto interactable = dynamic_cast<IInteractable*>(actor.get());
        if (!interactable) continue;
        float bestDist = actor->GetInteractRange();
        DirectX::XMFLOAT3 playerPos = GetPosition();
        DirectX::XMFLOAT3 interactablePos = MathHelper::Add(actor->GetPosition(), actor->GetInteractOffset());

        DirectX::XMFLOAT3 dir = MathHelper::Normalize(
            MathHelper::Subtract(interactablePos, playerPos)
        );

        float dot = MathHelper::Dot(forward, dir);

        // 前方60度以内
        if (dot < 0.5f) continue;

        DebugRender::DrawSphere(interactablePos, bestDist, { 0,1,1,1 }, 0);


        float dist = MathHelper::Distance(playerPos, interactablePos);

        if (dist < bestDist)
        {
            DebugRender::DrawSphere(interactablePos, bestDist, { 1,1,1,1 }, 0);
            bestDist = dist;
            best = interactable;
        }


    }

    return best;
}

