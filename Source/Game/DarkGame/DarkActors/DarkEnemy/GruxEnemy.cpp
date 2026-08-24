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
#include <random>

void GruxEnemy::Initialize(const Transform& transform)
{
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
        stateMachine_->RegisterState(std::make_unique<EnemyTurnState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyAttackState>(this));
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


    // Hpバー後ろ
    auto gaugeFrameBackComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/HpBar/bar_back.png", "bar_back_ui");
    DirectX::XMFLOAT2 gaugeSize = { 221.0f,28.0f };
    DirectX::XMFLOAT2 gaugeScale = { 3.0f,3.0f };
    gaugeFrameBackComponent->SetScale(gaugeScale);
    gaugeFrameBackComponent->SetSize(gaugeSize);
    gaugeFrameBackComponent->SetPivot({ 0.0f,0.5f });
    gaugeFrameBackComponent->zOrder = 10;
    gaugeFrameBackComponent->SetWorldPosition({ 650.0f,115.0f });
    gaugeFrameBackComponent->SetColor(CoreColor::White);
    uiManager->Add(gaugeFrameBackComponent);
    // Hpバー
    hpFrameUiComponent = std::make_shared<UIGaugeComponent>("./Data/Textures/UI/HpBar/frame.png","./Data/Textures/UI/HpBar/bar.png", "EnemyHpBar");
    hpFrameUiComponent->SetVisible(true);
    hpFrameUiComponent->SetPivot({ 0.0f,0.5f });
    hpFrameUiComponent->SetSize(gaugeSize);
    hpFrameUiComponent->SetScale(gaugeScale);
    hpFrameUiComponent->SetWorldPosition({ 650.0f,115.0f });
    hpFrameUiComponent->zOrder = 15;
    hpFrameUiComponent->SetGaugeFillSize(gaugeSize);
    hpFrameUiComponent->SetColor(CoreColor::White);
    uiManager->Add(hpFrameUiComponent);
}

void GruxEnemy::Update(float deltaTime)
{
    // HPバーの更新
    if (hpFrameUiComponent)
    {
        hpFrameUiComponent->SetValue(static_cast<float>(hp), static_cast<float>(maxHp));
    }

    if (!IsAnimationEditorPreviewActive())
        UpdateActionCooldowns(deltaTime);

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

void GruxEnemy::OnAnimationEditorPreviewEvent(const AnimationNotifyEvent& event)
{
    if (event.type != AnimationNotifyEvent::Type::PlaySE || event.parameter.empty())
        return;

    const std::string audioPath = "./Data/Sound/SE/" + event.parameter + ".wav";
    auto audio = CoreAudio::PlayOneShot(audioPath, event.value);
    if (audio)
    {
        const float pitch = pitchBaseValue + GetTimeScale() * (1.0f - pitchBaseValue);
        audio->SetPitch(pitch);
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
void GruxEnemy::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    Character::DrawImGuiDetails();
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
        "PrimaryAttack_Dash",
        "PrimaryAttack_DashAttack",
        "LongRangeAttack (Not Implemented)",
    };
    if (ImGui::Combo("Debug Fixed Attack", &attackIndex, attackTypes, static_cast<int>(std::size(attackTypes))))
        debugFixedAttackType = static_cast<BossAttackType>(attackIndex);
    ImGui::DragFloat("Attack Interval", &attackInterval, 0.05f, 0.0f, 10.0f, "%.2f sec");
    ImGui::DragFloat("Fallback Recovery Duration", &recoveryDuration, 0.05f, 0.0f, 10.0f, "%.2f sec");
    const BossTargetContext targetContext = BuildTargetContext();
    const char* relativeRegions[] = { "Front", "Side", "Back" };
    const char* distanceRegions[] = { "Near", "Middle", "Far" };
    const char* actionTypes[] =
    {
        "AttackLA",
        "AttackRA",
        "FastCombo",
        "JumpAttack",
        "Approach",
        "Retreat",
    };

    ImGui::SeparatorText("CombatAI v2 Positioning");
    ImGui::Text("Current Distance: %.3f", targetContext.xzDistance);
    ImGui::Text("Absolute Angle: %.3f", targetContext.absoluteAngleDegrees);
    ImGui::Text("Signed Angle: %.3f", targetContext.signedAngleDegrees);
    ImGui::Text("Forward Dot: %.3f", targetContext.forwardDot);

    ImGui::Text("Relative Region: %s", targetContext.valid ? relativeRegions[static_cast<int>(targetContext.region)] : "Invalid");
    ImGui::Text("Distance Region: %s", targetContext.valid ? distanceRegions[static_cast<int>(targetContext.distanceRegion)] : "Invalid");
    ImGui::Text("Selected Action: %s", actionTypes[static_cast<int>(selectedActionType)]);
    const char* intentTypes[] = { "CloseCombat", "JumpAttack" };
    const char* candidateReasonNames[] =
    {
        "NoActiveIntent",
        "Candidate",
        "NotForCurrentIntent",
        "WrongDistance",
        "Cooldown",
        "ZeroWeight",
    };
    const char* activeIntentName = "None";
    if (activeIntent)
        activeIntentName = intentTypes[static_cast<int>(*activeIntent)];
    ImGui::Text("Active Intent: %s", activeIntentName);
    ImGui::Text("Positioning Attempted: %s", intentPositioningAttempted ? "Yes" : "No");
    ImGui::Text("CloseCombat Lifecycle: %s", intentLifecycleState.c_str());
    ImGui::Text("Lifecycle Reason: %s", intentLifecycleReason.c_str());
    ImGui::TextWrapped("Lifecycle Trace: %s", intentLifecycleTrace.c_str());
    if (ImGui::TreeNode("Intent Selection"))
    {
        const float totalIntentWeight = GetTotalIntentWeight();
        for (size_t i = 0; i < combatIntentData.size(); ++i)
        {
            BossIntentData& data = combatIntentData[i];
            const float probability = totalIntentWeight > 0.0f
                ? (std::max)(0.0f, data.weight) / totalIntentWeight * 100.0f
                : 0.0f;
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%s", intentTypes[i]);
            ImGui::DragFloat("Weight", &data.weight, 1.0f, 0.0f, 1000.0f, "%.1f");
            ImGui::Text("Selection Probability: %.2f%%", probability);
            ImGui::PopID();
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
                ImGui::DragFloat("Max Move Distance", &data.maxMoveDistance, 0.1f, 0.0f, 30.0f, "%.2f");
                ImGui::DragFloat("Move Speed", &data.moveSpeed, 0.1f, 0.0f, 20.0f, "%.2f");
                ImGui::DragFloat("Timeout", &data.timeout, 0.05f, 0.01f, 20.0f, "%.2f sec");
                ImGui::DragFloat("Stuck Time Threshold", &data.stuckTimeThreshold, 0.05f, 0.0f, 10.0f, "%.2f sec");
                ImGui::DragFloat("Stuck Movement Threshold", &data.stuckMovementThreshold, 0.01f, 0.0f, 10.0f, "%.2f m/sec");
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    ImGui::SeparatorText("Positioning Runtime");
    ImGui::Text("Active: %s", positioningDebugActive ? "Yes" : "No");
    ImGui::Text("Positioning Action: %s", actionTypes[static_cast<int>(activePositioningDebugData.actionType)]);
    ImGui::Text("Direction Type: %s", activePositioningDebugData.direction == BossPositioningDirection::TowardPlayer ? "TowardPlayer" : "AwayFromPlayer");
    ImGui::Text("Completion Type: %s", activePositioningDebugData.completionType == BossPositioningCompletionType::TargetDistance ? "TargetDistance" : "TravelDistance");
    ImGui::Text("Current Player Distance: %.2f", targetContext.xzDistance);
    ImGui::Text("Target Distance: %.2f", activePositioningDebugData.targetDistance);
    ImGui::Text("Traveled Distance: %.2f", positioningDebugTraveledDistance);
    ImGui::Text("Max Move Distance: %.2f", activePositioningDebugData.maxMoveDistance);
    ImGui::Text("Move Speed: %.2f", activePositioningDebugData.moveSpeed);
    ImGui::Text("Elapsed Time: %.2f", positioningDebugElapsedTime);
    ImGui::Text("Timeout: %.2f", activePositioningDebugData.timeout);
    ImGui::Text("Stuck Timer: %.2f", positioningDebugStuckTimer);
    ImGui::Text("Positioning End Reason: %s", positioningEndReason.c_str());

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
    CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_damage.wav", 0.3f);
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

void GruxEnemy::BeginPositioning(const BossPositioningData& data)
{
    const auto controller = GetBodyAnimationController();
    const bool isPositioningLocomotionPlaying =
        controller &&
        controller->IsPlayAnimation() &&
        controller->GetCurrentAnimationName() == "TravelMode_Fwd_0";

    if (!isPositioningLocomotionPlaying)
        PlayBodyAnimation("TravelMode_Fwd_0", true, true, 0.15f, true);

    positioningDebugActive = true;
    activePositioningDebugData = data;
    positioningDebugTraveledDistance = 0.0f;
    positioningDebugElapsedTime = 0.0f;
    positioningDebugStuckTimer = 0.0f;
    positioningEndReason = "Running";

    if (characterMovementComponent)
    {
        characterMovementComponent->SetFixedSpeed(data.moveSpeed);
        characterMovementComponent->SetInputMagnitude(1.0f);
    }
}

void GruxEnemy::UpdatePositioningMovement(const DirectX::XMFLOAT3& moveDirection,
    const DirectX::XMFLOAT3& facingDirection, float deltaTime)
{
    if (characterMovementComponent)
        characterMovementComponent->SetMoveDirection(moveDirection);
    RotateTowardsPlayer(facingDirection, GetTurnSpeed(), deltaTime);
}

void GruxEnemy::UpdatePositioningDebug(float traveledDistance, float elapsedTime, float stuckTimer)
{
    positioningDebugTraveledDistance = traveledDistance;
    positioningDebugElapsedTime = elapsedTime;
    positioningDebugStuckTimer = stuckTimer;
}

void GruxEnemy::FinishPositioningDebug(const std::string& reason)
{
    positioningDebugActive = false;
    positioningEndReason = reason;
}

bool GruxEnemy::TryStartIntent(BossIntentType intentType)
{
    if (activeIntent)
        return false;

    activeIntent = intentType;
    intentPositioningAttempted = false;
    intentLifecycleState = "Intent Started";
    intentLifecycleTrace = "Intent Started";
    intentLifecycleReason = "None";
    return true;
}

float GruxEnemy::GetTotalIntentWeight() const
{
    float totalWeight = 0.0f;
    for (const BossIntentData& data : combatIntentData)
        totalWeight += (std::max)(0.0f, data.weight);
    return totalWeight;
}

bool GruxEnemy::SelectIntentByWeight()
{
    if (activeIntent)
        return false;

    const float totalWeight = GetTotalIntentWeight();
    if (totalWeight <= 0.0f)
        return false;

    static std::mt19937 randomEngine{ std::random_device{}() };
    std::uniform_real_distribution<float> distribution(0.0f, totalWeight);
    const float selectionValue = distribution(randomEngine);

    float accumulatedWeight = 0.0f;
    for (const BossIntentData& data : combatIntentData)
    {
        const float effectiveWeight = (std::max)(0.0f, data.weight);
        if (effectiveWeight <= 0.0f)
            continue;

        accumulatedWeight += effectiveWeight;
        if (selectionValue <= accumulatedWeight)
            return TryStartIntent(data.type);
    }

    return false;
}

void GruxEnemy::ClearActiveIntent()
{
    activeIntent = std::nullopt;
    intentPositioningAttempted = false;
    intentLifecycleState = "None";
    intentLifecycleTrace = "None";
    intentLifecycleReason = "Cleared";
}

void GruxEnemy::MarkIntentPositioningAttempted()
{
    if (!activeIntent || *activeIntent != BossIntentType::CloseCombat ||
        selectedActionType != BossActionType::Approach)
        return;

    intentPositioningAttempted = true;
    intentLifecycleState = "Positioning";
    intentLifecycleTrace += " -> Positioning";
}

void GruxEnemy::BeginIntentReevaluation()
{
    if (!activeIntent || *activeIntent != BossIntentType::CloseCombat ||
        !intentPositioningAttempted)
        return;

    intentLifecycleState = "Reevaluate";
    intentLifecycleTrace += " -> Reevaluate";
}

void GruxEnemy::MarkIntentAttackSelected()
{
    if (!activeIntent || *activeIntent != BossIntentType::CloseCombat)
        return;

    if (selectedActionType != BossActionType::AttackLA &&
        selectedActionType != BossActionType::AttackRA &&
        selectedActionType != BossActionType::FastCombo)
        return;

    intentLifecycleState = "Attack Selected";
    intentLifecycleTrace += " -> Attack Selected";
}

void GruxEnemy::OnSelectedActionStartedSuccessfully()
{
    if (!activeIntent || *activeIntent != BossIntentType::CloseCombat)
        return;

    if (selectedActionType != BossActionType::AttackLA &&
        selectedActionType != BossActionType::AttackRA &&
        selectedActionType != BossActionType::FastCombo)
        return;

    activeIntent = std::nullopt;
    intentPositioningAttempted = false;
    intentLifecycleState = "Intent Completed";
    intentLifecycleTrace += " -> Intent Completed";
    intentLifecycleReason = "AttackStarted";
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
    intentPositioningAttempted = false;
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
        PrepareJumpAttackMotionWarpOverride();
        animationName = "PrimaryAttack_JumpAttack";
        break;
    case BossAttackType::Dash: animationName = "PrimaryAttack_Dash"; break;
    case BossAttackType::DashAttack: animationName = "PrimaryAttack_DashAttack"; break;
    case BossAttackType::LongRangeAttack: return false;
    }

    transitionWindow = false;
    PlayBodyAnimation(animationName, false, true, 0.1f);
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
void GruxEnemy::StartAttack()
{
    ClearJumpAttackMotionWarpOverride();
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

    if (context.absoluteAngleDegrees <= 45.0f)
        context.region = PlayerRelativeRegion::Front;
    else if (context.absoluteAngleDegrees < 135.0f)
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

void GruxEnemy::StopAIMovement()
{
    if (!characterMovementComponent)
        return;
    characterMovementComponent->SetMoveDirection({ 0.0f, 0.0f, 0.0f });
    characterMovementComponent->SetInputMagnitude(0.0f);
    characterMovementComponent->ResetFixedSpeed();
}

bool GruxEnemy::RotateTowardsPlayer(
    const DirectX::XMFLOAT3& direction,
    const float degreesPerSecond,
    const float deltaTime)
{
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

// 現在の距離領域に基づいて、候補となる行動を選択する関数
bool GruxEnemy::IsActionForCurrentIntent(BossActionType actionType, const BossTargetContext& context) const
{
    if (!activeIntent || !context.valid)
        return false;

    if (*activeIntent != BossIntentType::CloseCombat)
        return false;

    if (context.distanceRegion == BossDistanceRegion::Near)
    {
        return actionType == BossActionType::AttackLA ||
            actionType == BossActionType::AttackRA ||
            actionType == BossActionType::FastCombo;
    }

    if (intentPositioningAttempted)
        return false;

    return actionType == BossActionType::Approach;
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

        if (!IsActionForCurrentIntent(actionData.type, context))
        {
            combatActionCandidateReasons[i] = BossActionCandidateReason::NotForCurrentIntent;
            continue;
        }

        if (!IsActionCandidateForCurrentDistance(actionData, context.distanceRegion))
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

        combatActionEffectiveWeights[i] = baseWeight;
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
std::optional<BossActionType> GruxEnemy::SelectActionByWeight() const
{
    float totalWeight = GetTotalActionWeight();
    if (totalWeight <= 0.0f)
        return std::nullopt;

    static std::mt19937 randomEngine{ std::random_device{}() };

    std::uniform_real_distribution<float> distribution(
        0.0f,
        totalWeight);

    const float selectionValue = distribution(randomEngine);

    float accumulatedWeight = 0.0f;

    for (size_t i = 0; i < combatActionData.size(); ++i)
    {
        const float effectiveWeight = combatActionEffectiveWeights[i];

        if (effectiveWeight <= 0.0f)
            continue;

        accumulatedWeight += effectiveWeight;

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

    // 現在の距離領域に基づいて、候補となる行動を更新する
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

    const BossPositioningData* positioningData = GetPositioningDataForAction(selectedActionType);
    if (positioningData)
    {
        selectedPositioningData = *positioningData;
        if (selectedActionType == BossActionType::Approach &&
            activeIntent && *activeIntent == BossIntentType::CloseCombat)
        {
            selectedPositioningData->completionType = BossPositioningCompletionType::TargetDistance;
            selectedPositioningData->targetDistance =
                (std::max)(0.0f, nearDistanceThreshold - closeCombatApproachArrivalMargin);
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
