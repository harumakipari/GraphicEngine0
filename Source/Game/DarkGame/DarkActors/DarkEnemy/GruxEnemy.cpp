#include "pch.h"

#include "GruxEnemy.h"

#include "Components/Render/PointLightComponent.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Scene/SceneBase.h"
#include "Game/Actors/Camera/DarkGameCamera.h"
#include "Game/Actors/Enemy/Boss/BossState.h"
#include "Game/Actors/Player/Player.h"
#include "Game/DarkGame/DarkActors/IceFragmentEffectActor.h"
#include "Physics/CollisionFunction.h"

void GruxEnemy::Initialize(const Transform& transform)
{
    int maxHp = 100;
    hp = maxHp;

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
    controller->AddAnimation("PrimaryAttack_Dash", 8);
    controller->AddAnimation("PrimaryAttack_DashAttack", 9);
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

    // 全てのNotifyAssetsをロードする
    controller->LoadAllNotifyAssets(GetName());

    // ステートマシンを作成
    {
        stateMachine_ = std::make_shared<StateMachine>();
        stateMachine_->RegisterState(std::make_unique<EnemyIdleState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyDeathState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyThinkState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyAttackState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyRecoveryState>(this));

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

#if 0
    // 武器に当たり判定のコンポーネントを追加
    int socketLeftNode = skeletalMeshComponent->FindIndexByName("weapon_l");
    leftWeaponCollisionComp = AddComponent<CapsuleComponent>("weaponLeftNode", parentName);
    //DirectX::XMFLOAT3 size = { 0.4f,4.0f,1.0f };
    DirectX::XMFLOAT3 size = { 1.0f,4.0f,1.0f };
    leftWeaponCollisionComp->AttachToComponent(skeletalMeshComponent, socketLeftNode); // "weapon_l"
    leftWeaponCollisionComp->SetRadiusAndHeight(size.x, size.y);
    leftWeaponCollisionComp->SetMass(mass);
    leftWeaponCollisionComp->SetCapsuleAxis(ShapeComponent::CapsuleAxis::z);
    leftWeaponCollisionComp->SetLayer(CollisionLayer::EnemyWeapon);
    leftWeaponCollisionComp->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Trigger);
    leftWeaponCollisionComp->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Trigger);
    leftWeaponCollisionComp->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Trigger);
    leftWeaponCollisionComp->SetCollisionOffsetY(height * 0.5f);
    leftWeaponCollisionComp->SetIsVisibleDebugBox(true);
    leftWeaponCollisionComp->SetRelativeLocationDirect({ -0.f, -0.f, 0.8f });
    leftWeaponCollisionComp->Initialize();
    leftWeaponCollisionComp->SetOnHitCallback([this](CollisionComponent* self, CollisionComponent* other)
        {
            OnWeaponHit(self, other);
        });

    int socketRightNode = skeletalMeshComponent->FindIndexByName("weapon_r");
    rightWeaponCollisionComp = AddComponent<CapsuleComponent>("weaponRightNode", parentName);
    rightWeaponCollisionComp->AttachToComponent(skeletalMeshComponent, socketRightNode); // "weapon_r"
    rightWeaponCollisionComp->SetRadiusAndHeight(size.x, size.y);
    rightWeaponCollisionComp->SetMass(mass);
    rightWeaponCollisionComp->SetCapsuleAxis(ShapeComponent::CapsuleAxis::z);
    rightWeaponCollisionComp->SetLayer(CollisionLayer::EnemyWeapon);
    rightWeaponCollisionComp->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Trigger);
    rightWeaponCollisionComp->SetCollisionOffsetY(height * 0.5f);
    rightWeaponCollisionComp->SetIsVisibleDebugBox(false);
    rightWeaponCollisionComp->SetRelativeLocationDirect({ -0.f, -0.f, -0.9f });
    rightWeaponCollisionComp->Initialize();
    rightWeaponCollisionComp->SetOnHitCallback([this](CollisionComponent* self, CollisionComponent* other)
        {
            OnWeaponHit(self, other);
        });


#endif // 0
#if 0
    AddHitCallback([&](std::pair<CollisionComponent*, CollisionComponent*> hitPair)
        {
            CollisionComponent* own = hitPair.first;
            CollisionComponent* other = hitPair.second;

            uint32_t myLayer = own->GetCollisionLayer();
            uint32_t otherLayer = other->GetCollisionLayer();

            if (myLayer & CollisionHelper::ToBit(CollisionLayer::EnemyWeapon) ||
                otherLayer & CollisionHelper::ToBit(CollisionLayer::Player))
            {
                auto player = dynamic_cast<Player*>(other->GetOwner());
                if (player)
                    player->TryTakeDamage(10, GetPosition());
            }
        }
    );
#endif // 0

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
    hitSwordEffectComponent->Load("./Data/Effect/Files/DarkStageBloodEffect.json");
    //hitSwordEffectComponent->Load("./Data/Effect/Files/DarkGameHitEffect.json");

 


}

void GruxEnemy::Update(float deltaTime)
{
    Character::Update(deltaTime);

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


    SetScale({ enemyScale,enemyScale,enemyScale });

    // ImageComponentのalpha更新
    {
        easingRunner->Tick(deltaTime);
        float bossNameImageAlpha = std::lerp(0.0f, 1.0f, easingFactorAlpha);
        gruxNameImageComponent->SetColor(DirectX::XMFLOAT4{ 1.0f,1.0f,1.0f,bossNameImageAlpha });
    }

    // 被弾時のフラッシュ
    {
        skeletalMeshComponent->plusAlphaCBuffer->data.flashValue = std::max<float>(0.0f, skeletalMeshComponent->plusAlphaCBuffer->data.flashValue - deltaTime / flashDuration);
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
                    justDodgedActors.insert(player.get());
                    player->StartJustDodgeSuccess(
                        std::dynamic_pointer_cast<Enemy>(this->shared_from_this()));
                    Logger::Log(Logger::LogCategory::Gameplay,
                        "[BossAttack][JustDodgeSuccess] attackSequenceId=" +
                        std::to_string(currentAttackSequenceId));
                }
            }
        }
    }




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
        const bool leftRootSucceeded = CollisionFunction::SphereRayCast(prevWeaponLeftRootPos, weaponLeftRootPos, leftRootHit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));
        const bool leftMidSucceeded = CollisionFunction::SphereRayCast(prevWeaponLeftMidPos, weaponLeftMidPos, leftMidHit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));
        const bool leftTipSucceeded = CollisionFunction::SphereRayCast(prevWeaponLeftTipPos, weaponLeftTipPos, leftTipHit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));
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
                    if (player->TryTakeDamage(1, GetPosition()))
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
        const bool rightRootSucceeded = CollisionFunction::SphereRayCast(prevWeaponRightRootPos, weaponRightRootPos, rightRootHit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));
        const bool rightMidSucceeded = CollisionFunction::SphereRayCast(prevWeaponRightMidPos, weaponRightMidPos, rightMidHit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));
        const bool rightTipSucceeded = CollisionFunction::SphereRayCast(prevWeaponRightTipPos, weaponRightTipPos, rightTipHit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));
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
                    if (player->TryTakeDamage(1, GetPosition()))
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

void GruxEnemy::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    Character::DrawImGuiDetails();
    ImGui::SeparatorText("Boss AI");
    int aiModeIndex = static_cast<int>(bossAIMode);
    const char* aiModes[] = { "CombatAI", "DebugFixedAttack" };
    if (ImGui::Combo("AI Mode", &aiModeIndex, aiModes, static_cast<int>(std::size(aiModes))))
        bossAIMode = static_cast<BossAIMode>(aiModeIndex);

    int attackIndex = static_cast<int>(debugFixedAttackType);
    const char* attackTypes[] =
    {
        "PrimaryAttack_LA",
        "PrimaryAttack_RA",
        "FastCombo (A > B > C)",
        "PrimaryAttack_JumpAttack",
        "PrimaryAttack_Dash",
        "PrimaryAttack_DashAttack",
        "LongRangeAttack (Not Implemented)",
    };
    if (ImGui::Combo("Debug Fixed Attack", &attackIndex, attackTypes, static_cast<int>(std::size(attackTypes))))
        debugFixedAttackType = static_cast<BossAttackType>(attackIndex);
    ImGui::DragFloat("Attack Interval", &attackInterval, 0.05f, 0.0f, 10.0f, "%.2f sec");
    ImGui::DragFloat("Recovery Duration", &recoveryDuration, 0.05f, 0.0f, 10.0f, "%.2f sec");
    ImGui::Text("Selected Attack: %s", attackTypes[static_cast<int>(selectedAttackType)]);
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
    ImGui::DragFloat(U8("被弾時のフラッシュ"), &flashDuration, 0.05f, 0.1f, 2.0f);
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
    ImGui::Text("rightHitBox: %s", rightHitBox ? "true" : "false");
#endif
}

//当たった時の処理
void GruxEnemy::TakeDamage(const int damage)
{
    //skeletalMeshComponent->plusAlphaCBuffer->data.flashValue = 1.0f;
    CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_hit.wav", 0.5f);
    hp -= damage;
    Logger::Log(U8("エネミーにダメージ！ HP:") + std::to_string(hp));
}

// ヒットエフェクトを生成する
// ヒットエフェクトを生成する
void GruxEnemy::SpawnHitEffect(const DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 playerPos) const
{
    DirectX::XMFLOAT3 enemyCenter = GetPosition();
    enemyCenter.y += hitEnemyEffectOffsetY;
    playerPos.y += hitPlayerEffectOffsetY;

    if (auto actorManager = GetOwnerScene()->GetActorManager())
    {
        //Transform tr{ enemyCenter,{0.0f,0.0f,0.0f},{1.0f,1.0f,1.0f} };
        //auto iceEffect = actorManager->CreateAndRegisterActorWithTransform<IceFragmentEmitterActor>("iceFragment", tr);
        //iceEffect->SetDirection(hitNormal, enemyCenter, playerPos);

        // 敵→プレイヤー方向
        DirectX::XMFLOAT3 forward = MathHelper::Normalize(MathHelper::Subtract(playerPos, enemyCenter));
        // エフェクト生成位置
        float spawnOffset = 0.8f;
        DirectX::XMFLOAT3 spawnPos = MathHelper::Add(enemyCenter, MathHelper::Multiply(forward, spawnOffset));
        spawnPos = hitPos;

        if (hitSwordEffectComponent)
        {
            hitSwordEffectComponent->SetWorldLocationDirect(spawnPos);
            hitSwordEffectComponent->UpdateComponentToWorld(); // これ入れないと最初に呼ばれる時に位置がずれる
            XMFLOAT3 position = hitSwordEffectComponent->GetComponentLocation();
            XMFLOAT3 rotation = hitSwordEffectComponent->GetComponentEulerRotation();
            EffectManager::EmitParticle(hitSwordEffectComponent->GetEffectHandle(), position, rotation);
        }
    }

}

void GruxEnemy::OnAnimationNotifyBegin(const AnimationNotifyState& state)
{
    switch (state.type)
    {
    case AnimationNotifyState::Type::HitBox:
        if (state.parameter == rightWeapon)
        {
            Logger::Log(U8("右の当たり判定を開始しました"));
            prevWeaponRightRootPos = weaponRightRootComponent->GetComponentLocation();
            prevWeaponRightMidPos = weaponRightMiddleComponent->GetComponentLocation();
            prevWeaponRightTipPos = weaponRightTipComponent->GetComponentLocation();
            rightHitBox = true;
        }
        else if (state.parameter == leftWeapon)
        {
            Logger::Log(U8("左の当たり判定を開始しました"));
            prevWeaponLeftRootPos = weaponLeftRootComponent->GetComponentLocation();
            prevWeaponLeftMidPos = weaponLeftMiddleComponent->GetComponentLocation();
            prevWeaponLeftTipPos = weaponLeftTipComponent->GetComponentLocation();
            leftHitBox = true;
        }
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
        Logger::Log(U8("遷移許可区間を開始しました"));
        break;
    case AnimationNotifyState::Type::JustDodgeWindow:
        break;
    case AnimationNotifyState::Type::DangerWindow:
        Logger::Log(Logger::LogCategory::Gameplay,
            "[BossAttack][DangerWindowBegin] attackSequenceId=" +
            std::to_string(currentAttackSequenceId));
        justDodgeAreaSize = state.justDodgeAreaSize;
        justDodgeAreaOffset = state.justDodgeAreaOffset;
        dangerArea = BuildDangerArea(GetPosition(), GetRight(), GetUp(), GetForward(),
            justDodgeAreaOffset, justDodgeAreaSize);
        isDangerWindow = true;
        break;
    case AnimationNotifyState::Type::ShowTrail:
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
        Logger::Log(U8("当たり判定を終了しました"));
        if (state.parameter == rightWeapon || state.parameter == bothWeapon)
            rightHitBox = false;
        if (state.parameter == leftWeapon || state.parameter == bothWeapon)
            leftHitBox = false;
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
        Logger::Log(U8("遷移許可区間を終了しました"));
        break;
    case AnimationNotifyState::Type::JustDodgeWindow:
        break;
    case AnimationNotifyState::Type::DangerWindow:
        Logger::Log(Logger::LogCategory::Gameplay,
            "[BossAttack][DangerWindowEnd] attackSequenceId=" +
            std::to_string(currentAttackSequenceId));
        isDangerWindow = false;
        ResetDangerArea();
        break;
    case AnimationNotifyState::Type::ShowTrail:
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
        break;
    }
}

void GruxEnemy::OnAnimationChanged()
{
    auto controller = GetBodyAnimationController();
    if (!controller || controller->GetCurrentAnimationName() != "PrimaryAttack_LA")
    {
        DisableAttackHitBoxes();
    }
}

// 攻撃開始時に始める処理
void GruxEnemy::SelectAttackForCurrentMode()
{
    if (bossAIMode == BossAIMode::DebugFixedAttack)
    {
        selectedAttackType = debugFixedAttackType;
        return;
    }

    // CombatAI candidate filtering and weighted selection will be added here.
    selectedAttackType = BossAttackType::PrimaryAttackLA;
}

int GruxEnemy::GetAttackStageCount(BossAttackType type) const
{
    if (type == BossAttackType::FastCombo)
        return 3;
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
    case BossAttackType::JumpAttack: animationName = "PrimaryAttack_JumpAttack"; break;
    case BossAttackType::Dash: animationName = "PrimaryAttack_Dash"; break;
    case BossAttackType::DashAttack: animationName = "PrimaryAttack_DashAttack"; break;
    case BossAttackType::LongRangeAttack: return false;
    }

    PlayBodyAnimation(animationName, false, true, 0.1f);
    return true;
}

void GruxEnemy::BeginAdditionalAttackStage()
{
    // A FastCombo is one attack sequence, but each stage may damage the player once.
    hitActors.clear();
}
void GruxEnemy::StartAttack()
{
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
    isDangerWindow = false;
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

// プレイヤーとの距離を取得する関数
float GruxEnemy::GetDistanceToPlayer()
{
    auto player = GetOwnerScene()->GetActorManager()->GetActorByName("player");
    if (!player) return 9999.0f;

    auto p = player->GetPosition();
    auto b = GetPosition();

    float dx = p.x - b.x;
    float dz = p.z - b.z;

    return sqrtf(dx * dx + dz * dz);
}

// 武器ヒット時の処理
void GruxEnemy::OnWeaponHit(CollisionComponent* self, CollisionComponent* other)
{
    if (!other)
    {
        Logger::Warning("other is nullptr");
        return;
    }

    const uint32_t mask = CollisionHelper::ToBit(CollisionLayer::Player);
    if (!(other->GetCollisionLayer() & mask))
        return;

    Actor* actor = other->GetOwner();
    Player* player = actor ? dynamic_cast<Player*>(actor) : nullptr;
    if (!player)
        return;

    Logger::Log(Logger::LogCategory::Gameplay,
        "[BossAttack][WeaponCollisionHit] attackSequenceId=" +
        std::to_string(currentAttackSequenceId));

    if (!rightHitBox && !leftHitBox)
        return;

    if (HasJustDodgedAttack(actor))
    {
        Logger::Log(Logger::LogCategory::Gameplay,
            "[BossAttack][DamageRejected] reason=justDodged attackSequenceId=" +
            std::to_string(currentAttackSequenceId));
        return;
    }

    if (hitActors.contains(actor))
        return;

    if (player->TryTakeDamage(10, GetPosition()))
    {
        hitActors.insert(actor);
        ++currentAttackHitCount;
        Logger::Log(Logger::LogCategory::Gameplay,
            "[BossAttack][DamageApplied] attackSequenceId=" +
            std::to_string(currentAttackSequenceId) + " source=weaponCollision");
    }
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
