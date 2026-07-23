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
        controller->AddAnimation("Walk_Bwd", 46);
        controller->AddAnimation("Walk_Fwd", 47);
        controller->AddAnimation("Walk_Left", 48);
        controller->AddAnimation("Walk_Right", 49);
        controller->AddAnimation("Jog_Fwd1", 50);
        controller->AddAnimation("Walk_Fwd1", 51);
        controller->AddAnimation("Sprint_Fwd", 52);
        controller->AddAnimation("Walk_Fwd2", 53);  // これだめ
        controller->AddAnimation("Jog_Fwd2", 54);// これだめ
        controller->AddAnimation("Jog_Bwd2", 55);
        controller->AddAnimation("Jog_BwdLeft", 56);
        controller->AddAnimation("Jog_BwdRight", 57);
        controller->AddAnimation("Jog_FwdLeft", 58);
        controller->AddAnimation("Jog_FwdRight", 59);
        controller->AddAnimation("Jog_Left2", 60);
        controller->AddAnimation("Jog_Right2", 61);
        controller->AddAnimation("Rush_Attack_Fast_A", 62);
        controller->AddAnimation("Rush_Attack_Fast_B", 63);
        controller->AddAnimation("Rush_Attack_Fast_C", 64);
        controller->AddAnimation("Rush_Attack_Fast_End", 65);
        controller->AddAnimation("Walk_Fwd3", 66);
        controller->AddAnimation("Jump", 67);
        controller->AddAnimation("Rush_Attack_Fast_D", 68);

        // ブレンドスペースに追加
        controller->AddBlendAnimation("Jog_Fwd", 0.0f, 1.0f);
        controller->AddBlendAnimation("Jog_Bwd", 0.0f, -1.0f);
        controller->AddBlendAnimation("Jog_Left", -1.0f, 0.0f);
        controller->AddBlendAnimation("Jog_Right", 1.0f, 0.0f);
#if 0
        controller->AddBlendAnimation("Walk_Fwd", 0.0f, 0.5f);
        controller->AddBlendAnimation("Walk_Bwd", 0.0f, -0.5f);
        controller->AddBlendAnimation("Walk_Left", -0.5f, 0.0f);
        controller->AddBlendAnimation("Walk_Right", 0.5f, 0.0f);
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
        stateMachine_->RegisterState(std::make_unique<PlayerRushState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerJumpState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerJumpAttackState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerInteractState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerDashState>(this));

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

    // ヒット時の剣のエフェクトコンポーネントを追加
    hitSwordEffectComponent = this->AddComponent<class ParticleComponent>("hitSwordEffectComponent", parentName);
    hitSwordEffectComponent->Load("./Data/Effect/Files/DarkGameHitEffect.json");


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
    rushButtonImageComponent->SetWorldPosition({ 50, 50 });
    rushButtonImageComponent->SetScale({ 1.2f,1.2f });
    rushButtonImageComponent->SetSize({ 200, 200 });
    rushButtonImageComponent->SetPivot({ 0.5f,0.5f });
    rushButtonImageComponent->SetVisible(false);
    uiManager->Add(rushButtonImageComponent);
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

    // スローモーション
    if (slowMotionActive)
    {
        slowMotionTimer -= Time::UnscaledDeltaTime();

        if (slowMotionTimer <= 0.0f)
        {
            slowMotionActive = false;
            ResetTimeScale();
            Logger::Log(U8("スローモーション終了"));
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
        HitResultWithActor hit;
        bool isHit = false;
        HitResultWithActor tmp;

        if (CollisionFunction::SphereRayCast(prevSwordRootPos, swordRootPos, tmp,
            weaponSphereRadius, CollisionHelper::ToBit(CollisionLayer::Enemy)))
        {
            hit = tmp;
            isHit = true;
        }

        if (CollisionFunction::SphereRayCast(prevSwordMidPos, swordMidPos, tmp,
            weaponSphereRadius, CollisionHelper::ToBit(CollisionLayer::Enemy)))
        {
            hit = tmp;
            isHit = true;
        }

        if (CollisionFunction::SphereRayCast(prevSwordTipPos, swordTipPos, tmp,
            weaponSphereRadius, CollisionHelper::ToBit(CollisionLayer::Enemy)))
        {
            hit = tmp;
            isHit = true;
        }
        if (isHit)
        {
            if (!hitActors.contains(hit.actor))
            {
                if (auto enemy = dynamic_cast<GruxEnemy*>(hit.actor))
                {
                    Logger::Log(U8("剣に敵が当たった"));
                    enemy->TakeDamage(1);
                    enemy->SpawnHitEffect(hit.hitPoint, hit.normal, playerPos);
                    hitActors.emplace(enemy);

                    if (hitSwordEffectComponent)
                    {
                        hitSwordEffectComponent->SetWorldLocationDirect(hit.hitPoint);
                        hitSwordEffectComponent->UpdateComponentToWorld(); // これ入れないと最初に呼ばれる時に位置がずれる
                        XMFLOAT3 position = hitSwordEffectComponent->GetComponentLocation();
                        XMFLOAT3 rotation = hitSwordEffectComponent->GetComponentEulerRotation();
                        EffectManager::EmitParticle(hitSwordEffectComponent->GetEffectHandle(), position, rotation);
                    }

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


    // これは絶対入れる　アニメーションの更新をしているから
    Character::Update(deltaTime);

    // 入力処理
    HandleInput(deltaTime);

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
    if (InputSystem::GetInputState("ok", InputStateMask::Trigger))
    {
        if (IInteractable* interactable = FindInteractable())
        {
            interactable->Interact();
        }
    }

#if 0
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
#endif // 0

    UpdateMovement();

    playerPos = GetPosition();

    for (auto& warp : animationMotionWarps)
    {
        DirectX::XMVECTOR move = DirectX::XMLoadFloat3(&warp.direction);
        move *= warp.speed * deltaTime;

        DirectX::XMFLOAT3 velocity;
        DirectX::XMStoreFloat3(&velocity, move);

        playerPos.x += velocity.x;
        playerPos.y += velocity.y;
        playerPos.z += velocity.z;
    }

    SetPosition(playerPos);

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
    ImGui::DragFloat("walkSpeed", &walkSpeed, 0.05f);
    ImGui::DragFloat("runSpeed", &runSpeed, 0.05f);
    ImGui::DragFloat("dashSpeed", &dashSpeed, 0.05f);
    ImGui::DragFloat("slowMotionInterval", &slowMotionInterval, 0.05f);
    ImGui::DragFloat("slowMotionPlayerTimeScale", &slowMotionPlayerTimeScale, 0.05f);
    ImGui::DragFloat("slowMotionEnemyTimeScale", &slowMotionEnemyTimeScale, 0.05f);
    ImGui::DragFloat("transparencyMinAlpha", &transparencyMinAlpha, 0.05f);
    ImGui::DragFloat("transparencyMaxAlpha", &transparencyMaxAlpha, 0.05f);
    ImGui::DragFloat(U8("ラッシュ後の敵までへのダッシュにかかる時間"), &moveToEnemyInterval, 0.05f);


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
        float speed = 0.0f;
        if (duration > 0.0f)
        {
            speed = state.moveDistance / duration;
        }
        AnimationMotionWarp warp{};
        warp.direction = direction;
        warp.speed = speed;
        warp.state = &state;
        animationMotionWarps.push_back(warp);
        Logger::Log(U8("MotionWarp開始"));
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
        animationMotionWarps.erase(
            std::remove_if(
                animationMotionWarps.begin(),
                animationMotionWarps.end(),
                [&](const AnimationMotionWarp& warp)
                {
                    return warp.state == &state;
                }),
            animationMotionWarps.end());
        Logger::Log(U8("MotionWarp終了"));
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
        locomotionMode = LocomotionMode::Idle;
        return;
    }

    auto camera = dynamic_cast<DarkCameraActor*>(GetOwnerScene()->GetActiveCamera());

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

// TPSモードの移動時の更新処理
void Player::UpdateTPSLocomotion()
{
#if 1
    float speed = characterMovementComponent->GetInputMagnitude();
    bool dash = InputSystem::GetInputState("GamePadB", InputStateMask::Press);

    // ダッシュ条件
    if (dash && speed >= 0.0f)
    {
        SetLocomotionMode(LocomotionMode::Dash);
        return;
    }

    switch (locomotionMode)
    {
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
        if (!dash)
        {
            if (speed >= 0.6f)
                SetLocomotionMode(LocomotionMode::TPSRun);
            else if (speed > 0.0f)
                SetLocomotionMode(LocomotionMode::TPSWalk);
            else
                SetLocomotionMode(LocomotionMode::Idle);
        }
        break;
    }

    auto move = inputComponent->GetMoveInput();
    GetBodyAnimationController()->SetBlendInput(move.x, move.z, speed);

#else
    float speed = characterMovementComponent->GetCurrentInputNormalizeSpeed();
    bool dash = InputSystem::GetInputState("GamePadB", InputStateMask::Press);

    // ダッシュ条件
    if (dash && speed >= 0.6f)
    {
        SetLocomotionMode(LocomotionMode::Dash);
        return;
    }

    switch (locomotionMode)
    {
    case LocomotionMode::Idle:
        if (speed > 0.4f)
            SetLocomotionMode(LocomotionMode::TPSWalk);
        break;

    case LocomotionMode::TPSWalk:
        if (speed <= 0.4f)
            SetLocomotionMode(LocomotionMode::Idle);
        else if (speed >= 0.6f)
            SetLocomotionMode(LocomotionMode::TPSRun);
        break;

    case LocomotionMode::TPSRun:
        if (speed < 0.55f)
            SetLocomotionMode(LocomotionMode::TPSWalk);
        break;
    case LocomotionMode::Dash:
        if (!dash)
        {
            if (speed >= 0.6f)
                SetLocomotionMode(LocomotionMode::TPSRun);
            else if (speed > 0.4f)
                SetLocomotionMode(LocomotionMode::TPSWalk);
            else
                SetLocomotionMode(LocomotionMode::Idle);
        }
        break;
    }

    auto move = inputComponent->GetMoveInput();
    GetBodyAnimationController()->SetBlendInput(move.x, move.z, speed);

#endif // 0
}

void Player::UpdateLockOnLocomotion()
{
    float speed = characterMovementComponent->GetInputMagnitude();
    bool dash = InputSystem::GetInputState("GamePadB", InputStateMask::Press);

    // ダッシュ条件
    if (dash && speed >= 0.0f)
    {
        SetLocomotionMode(LocomotionMode::Dash);
        return;
    }

    switch (locomotionMode)
    {
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
        if (!dash)
        {
            if (speed >= 0.6f)
                SetLocomotionMode(LocomotionMode::LockOnBlendRun);
            else if (speed > 0.0f)
                SetLocomotionMode(LocomotionMode::LockOnBlendWalk);
            else
                SetLocomotionMode(LocomotionMode::Idle);
        }
        break;
    }

    auto move = inputComponent->GetMoveInput();
    GetBodyAnimationController()->SetBlendInput(move.x, move.z, speed);

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
    if (InputSystem::GetInputState("Jump", InputStateMask::Trigger))
    {
        bufferCommand.command = InputCommand::Dodge;
        DecideLockOnDodgeDirection();
        bufferCommand.remainTime = 0.3f;
        return;
    }
    if (InputSystem::GetInputState("Attack", InputStateMask::Trigger))
    {
        bufferCommand.command = InputCommand::Attack;
        bufferCommand.remainTime = 0.5f;
        return;
    }
    //if (InputSystem::GetInputState("Jump", InputStateMask::Trigger))
    {
        //bufferCommand.command = InputCommand::Jump;
        //bufferCommand.remainTime = 0.5f;
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

// 動作更新処理
void Player::UpdateMovement()
{
    bool isDash = GetStateMachine()->GetStateName() == "Dash";

    auto intent = inputComponent->GetIntent();
    DirectX::XMFLOAT3 moveDir = { 0,0,0 };
    bool focus = InputSystem::GetInputState("LockOn", InputStateMask::Press);

    if (auto camera = dynamic_cast<DarkCameraActor*>(GetOwnerScene()->GetActiveCamera()))
    {
#if 1
        if (isDash)
        {
            camera->SetRequestMode(DarkCameraActor::CameraMode::TPS);
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
        // Rotation用
        float stickX = rawStickX;
        float stickZ = rawStickZ;
        // Movement用
        float moveStickX = rawStickX;
        float moveStickZ = rawStickZ;
        float length = sqrtf(moveStickX * moveStickX + moveStickZ * moveStickZ);
        const float deadZone = 0.18f;
        if (length < deadZone)
        {
            moveStickX = 0.0f;
            moveStickZ = 0.0f;
            characterMovementComponent->SetInputMagnitude(0.0f);
        }
        else
        {
            float newLength = (length - deadZone) / (1.0f - deadZone);
            // 好みでコメントアウトを切り替え
             //newLength = std::pow(newLength,1.5f);// より繊細な入力
            //newLength *= newLength;
            // newLength = sqrtf(newLength); // 少し倒しただけで速い
            moveStickX = moveStickX / length * newLength;
            moveStickZ = moveStickZ / length * newLength;

            characterMovementComponent->SetInputMagnitude(newLength);
        }

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
            DirectX::XMFLOAT3 lookDir = { 0,0,0 };
            lookDir.x = camForward.x * rawStickZ + camRight.x * rawStickX;
            lookDir.z = camForward.z * rawStickZ + camRight.z * rawStickX;
            const std::string& state = GetStateMachine()->GetStateName();
            if (state != "Dodge" && state != "Attack")
            {// 回避ではないかつ攻撃でないときは
                rotationComponent->SetDirection(lookDir);
            }
            float normalizeSpeed = characterMovementComponent->GetCurrentInputNormalizeSpeed();
            GetBodyAnimationController()->SetBlendInput(0.0f, 1.0f, normalizeSpeed);
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
            float normalizeSpeed = characterMovementComponent->GetCurrentInputNormalizeSpeed();
            GetBodyAnimationController()->SetBlendInput(rawStickX, rawStickZ, normalizeSpeed);
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
                float normalizeSpeed = characterMovementComponent->GetCurrentInputNormalizeSpeed();
                GetBodyAnimationController()->SetBlendInput(rawStickX, rawStickZ, normalizeSpeed);
                rotationComponent->SetDirection(forward);
            }
            break;
        }
        }
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

    switch (mode)
    {
    case LocomotionMode::TPSWalk:
        controller->SetUseBlendSpace(false);
        PlayBodyAnimation("Walk_Fwd", true, true, 0.2f, true);
        break;

    case LocomotionMode::TPSRun:
        controller->SetUseBlendSpace(false);
        PlayBodyAnimation("Jog_Fwd", true, true, 0.2f, true);
        break;

    case LocomotionMode::LockOnBlendWalk:
        controller->SetUseBlendSpace(true);
        break;

    case LocomotionMode::LockOnBlendRun:
        controller->SetUseBlendSpace(true);
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
}


// 回避の方向を決定する処理
void Player::DecideLockOnDodgeDirection()
{
    auto camera = dynamic_cast<DarkCameraActor*>(GetOwnerScene()->GetActiveCamera());
    if (!camera)
        return;

    // TPSでは使用しない
    if (camera->GetMovementMode() == DarkCameraActor::CameraMode::TPS)
        return;

    auto intent = inputComponent->GetIntent();

    float x = intent.leftMove.x;
    float z = intent.leftMove.z;

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
    enemy->SetTimeScale(slowMotionEnemyTimeScale);
    this->SetTimeScale(slowMotionPlayerTimeScale);

    slowMotionActive = true;
    slowMotionTimer = slowMotionInterval;

    // rush時のtargetを保存する
    rushTarget = enemy;

    // 画面の色を変える

    // UIを表示する
    //rushButtonImageComponent->SetVisible(true);
    // SEの再生

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

    DirectX::XMFLOAT3 forward = GetForward(); // プレイヤー前方向


    for (auto actor : GetOwnerScene()->GetActorManager()->GetActorsOfType<InteractableActor>())
    {
        auto interactable = dynamic_cast<IInteractable*>(actor.get());
        if (!interactable) continue;
        bestDist = actor->GetInteractRange();
        DirectX::XMFLOAT3 playerPos = GetPosition();
        DirectX::XMFLOAT3 interactablePos = MathHelper::Add(actor->GetPosition(), actor->GetInteractOffset());

        DirectX::XMFLOAT3 dir = MathHelper::Normalize(
            MathHelper::Subtract(interactablePos, playerPos)
        );

        float dot = MathHelper::Dot(forward, dir);

        // playerが反応する角度
        float interactableRadian = actor->GetInteractRadian();

        if (dot < interactableRadian)
        {
            interactable->SetCanInteract(false);
            continue;
        }
        DebugRender::DrawSphere(interactablePos, bestDist, { 0,1,1,1 }, 0);


        float dist = MathHelper::Distance(playerPos, interactablePos);

        if (dist < bestDist)
        {
            DebugRender::DrawSphere(interactablePos, bestDist, { 1,1,1,1 }, 0);
            bestDist = dist;
            best = interactable;
            interactable->SetCanInteract(true);
        }
    }
    return best;
}

