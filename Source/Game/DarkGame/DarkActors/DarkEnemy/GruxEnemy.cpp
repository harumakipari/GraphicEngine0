#include "pch.h"

#include "GruxEnemy.h"

#include "Components/Render/PointLightComponent.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Scene/SceneBase.h"
#include "Game/Actors/Enemy/Boss/BossState.h"
#include "Game/Actors/Player/Player.h"
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
            //material.overridePipelineName = "";
        }
    }
    skeletalMeshComponent->SetIsShadowMap(true);
    skeletalMeshComponent->SetIsCastShadow(false);

#if 0
    {
        // 左肩にカプセルの当たり判定を追加
        int leftUpperArmLeftIndex = skeletalMeshComponent->FindIndexByName("upperarm_l");
        std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("capsuleComponent", parentName);
        height = 0.3f;
        radius = 0.75f;
        capsuleComponent->SetRadiusAndHeight(radius, height);
        capsuleComponent->SetMass(mass);
        capsuleComponent->SetCapsuleAxis(ShapeComponent::CapsuleAxis::y);
        capsuleComponent->SetLayer(CollisionLayer::EnemyBody);
        capsuleComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        capsuleComponent->SetRelativeEulerRotationDirect({ 0.0f,0.0f,90.0f });
        capsuleComponent->SetRelativeLocationDirect({ -0.9f,0.0f,-0.1f });
        capsuleComponent->Initialize();
        capsuleComponent->AttachToComponent(skeletalMeshComponent, leftUpperArmLeftIndex);
    }
    int leftLowerArmLeftIndex = skeletalMeshComponent->FindIndexByName("lowerarm_l");
    // 右肩にカプセルの当たり判定を追加


#endif // 0

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

    // 全てのNotifyAssetsをロードする
    controller->LoadAllNotifyAssets(GetName());

    // ステートマシンを作成
    {
        stateMachine_ = std::make_shared<StateMachine>();
        stateMachine_->RegisterState(std::make_unique<EnemyIdleState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyWalkState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyAttackState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyDeathState>(this));
        stateMachine_->RegisterState(std::make_unique<EnemyCastState>(this));

        // ステートマシンを character に追加
        //this->SetStateMachine(stateMachine);
        // 初期ステートを設定
        stateMachine_->ChangeState("EnemyIdleState");
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
        height = 2.7f;
        radius = 1.5f;

        mass = 300.0f;
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
                player->TakeDamage(10);
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

    // 登場シーンのボス名前のUIを追加
    auto uiManager = GetOwnerScene()->GetUIManager();
    gruxNameImageComponent = std::make_shared<UIImageComponent>("./Data/Textures/UI/Grux_name.png", "Grux_name");
    gruxNameImageComponent->SetWorldPosition({ 995, 900 });
    gruxNameImageComponent->SetScale({ 1.2f,1.2f });
    gruxNameImageComponent->SetSize({ 600, 200 });
    gruxNameImageComponent->SetPivot({ 0.5f,0.5f });
    gruxNameImageComponent->SetColor(DirectX::XMFLOAT4{ 1.0f,1.0f,1.0f,0.0f });
    gruxNameImageComponent->SetVisible(true);
    uiManager->Add(gruxNameImageComponent);
    easingRunner = std::make_unique<EasingRunner>();
}

void GruxEnemy::Update(float deltaTime)
{
    Character::Update(deltaTime);

    // ImageComponentのalpha更新
    {
        easingRunner->Tick(deltaTime);
        float bossNameImageAlpha = std::lerp(0.0f, 1.0f, easingFactorAlpha);
        gruxNameImageComponent->SetColor(DirectX::XMFLOAT4{ 1.0f,1.0f,1.0f,bossNameImageAlpha });
    }

    // 被弾時のフラッシュ
    {
        skeletalMeshComponent->plusAlphaCBuffer->data.flashValue = std::max<float>(0.0f, skeletalMeshComponent->plusAlphaCBuffer->data.flashValue - deltaTime * 8.0f);
    }

    // ボス戦時のカメラの注視点の位置を更新する
    if (auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>())
    {
        DirectX::XMFLOAT3 bossPos = GetPosition();
        DirectX::XMFLOAT3 playerPos = player->GetPosition();
        DirectX::XMFLOAT3 toPlayerDir = MathHelper::Subtract(playerPos, bossPos);
        DirectX::XMFLOAT3 worldUp = { 0.0f,1.0f,0.0f };
        toPlayerDir = MathHelper::Normalize(toPlayerDir);
        DirectX::XMFLOAT3 rightDir = MathHelper::Cross(worldUp, toPlayerDir);
        rightDir = MathHelper::Normalize(rightDir);
        DirectX::XMFLOAT3 targetPos = bossPos;

        targetPos = MathHelper::Add(
            targetPos,
            MathHelper::Multiply(toPlayerDir, bossBattleCameraDistance));

        targetPos = MathHelper::Add(
            targetPos,
            MathHelper::Multiply(rightDir, bossBattleCameraRightDistance));

        targetPos = MathHelper::Add(
            targetPos,
            bossBattleCameraOffset);

        cameraTargetComponent->SetWorldLocationDirect(targetPos);
        DebugRender::DrawSphere(targetPos, 0.5f, { 1.0f,1.0f,0.0f,1.0f }, true);
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

        isLeftHit |= CollisionFunction::SphereRayCast(prevWeaponLeftRootPos, weaponLeftRootPos, hit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));
        isLeftHit |= CollisionFunction::SphereRayCast(prevWeaponLeftMidPos, weaponLeftMidPos, hit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));
        isLeftHit |= CollisionFunction::SphereRayCast(prevWeaponLeftTipPos, weaponLeftTipPos, hit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));

        prevWeaponLeftRootPos = weaponLeftRootPos;
        prevWeaponLeftMidPos = weaponLeftMidPos;
        prevWeaponLeftTipPos = weaponLeftTipPos;

        if (isLeftHit)
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

                if (!hitActors.contains(hit.actor))
                {
                    Logger::Log(U8("剣にプレイヤーが当たった"));
                    player->TakeDamage(1);
                    hitActors.emplace(player);
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

        isRightHit |= CollisionFunction::SphereRayCast(prevWeaponRightRootPos, weaponRightRootPos, hit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));
        isRightHit |= CollisionFunction::SphereRayCast(prevWeaponRightMidPos, weaponRightMidPos, hit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));
        isRightHit |= CollisionFunction::SphereRayCast(prevWeaponRightTipPos, weaponRightTipPos, hit, hitWeaponRadius, CollisionHelper::ToBit(CollisionLayer::Player));

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
                if (!hitActors.contains(hit.actor))
                {
                    Logger::Log(U8("剣にプレイヤーが当たった"));
                    player->TakeDamage(1);
                    hitActors.emplace(player);
                }
            }
        }
    }

    if (rightWeaponCollisionComp)
        rightWeaponCollisionComp->SetIsVisibleDebugShape(rightHitBox);
    if (leftWeaponCollisionComp)
        leftWeaponCollisionComp->SetIsVisibleDebugShape(leftHitBox);

    // 攻撃の危険な時に、
    if (isDangerWindow)
    {
        // ボスの前方向にプレイヤーがいて

        // プレイヤーがジャスト回避したら、

        // ジャスト回避成功

    }

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
    ImGui::DragFloat("pitchBaseValue", &pitchBaseValue, 0.05f);
    if (ImGui::Button("Attack"))
    {
        stateMachine_->ChangeState("EnemyAttackState");
    }
    if (ImGui::Button(U8("ボスの名前の演出")))
    {
        StartGruxNamePerform(2.0f);
    }
    ImGui::DragFloat(U8("ボスの武器の攻撃範囲"), &hitWeaponRadius, 0.05f, 0.1f, 2.0f);
    ImGui::DragFloat(U8("ボス戦時のカメラ距離"), &bossBattleCameraDistance, 0.5f);
    ImGui::DragFloat(U8("ボス戦時のカメラ右方向の距離"), &bossBattleCameraRightDistance, 0.5f);
    ImGui::DragFloat3(U8("ボス戦時のオフセット"), &bossBattleCameraOffset.x, 0.5f);
#endif
}

//当たった時の処理
void GruxEnemy::TakeDamage(int damage)
{
    skeletalMeshComponent->plusAlphaCBuffer->data.flashValue = 1.0f;
    CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_hit.wav", 0.5f);
    hp -= damage;
    Logger::Log(U8("エネミーにダメージ！ HP:") + std::to_string(hp));
}

void GruxEnemy::OnAnimationNotifyBegin(const AnimationNotifyState& state)
{
    switch (state.type)
    {
    case AnimationNotifyState::Type::HitBox:
        if (state.parameter == rightWeapon)
        {
            Logger::Log(U8("右の当たり判定を開始しました"));
            rightHitBox = true;
        }
        else if (state.parameter == leftWeapon)
        {
            Logger::Log(U8("左の当たり判定を開始しました"));
            leftHitBox = true;
        }
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
        Logger::Log(U8("攻撃の危険時間が開始しました。"));
        isDangerWindow = true;
        break;
    }
}

void GruxEnemy::OnAnimationNotifyEnd(const AnimationNotifyState& state)
{
    switch (state.type)
    {
    case AnimationNotifyState::Type::HitBox:
        Logger::Log(U8("当たり判定を終了しました"));
        rightHitBox = false;   // 右の剣の当たり判定
        leftHitBox = false;    // 左の剣の当たり判定
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
        Logger::Log(U8("攻撃の危険時間が終了しました。"));
        isDangerWindow = false;
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

// 攻撃開始時に始める処理
void GruxEnemy::StartAttack()
{
#if 0
    DirectX::XMFLOAT3 size = { 1.0f,4.0f,1.0f };
    leftWeaponCollisionComp->ResizeCapsule(size.x, size.y);
#endif // 0
    hitActors.clear();
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

    uint32_t mask = CollisionHelper::ToBit(CollisionLayer::Player);

    if (!(other->GetCollisionLayer() & mask))
        return;

    Actor* actor = other->GetOwner();

    if (!actor)
    {
        Logger::Warning("actor is nullptr");
        return;
    }

    Player* player = dynamic_cast<Player*>(actor);

    if (!player)
        return;

    if (isDangerWindow && player->GetJustDodgeWindow())
    {
        if (auto enemy = std::dynamic_pointer_cast<Enemy>(shared_from_this()))
        {
            player->StartJustDodgeSuccess(enemy);
            Logger::Log(U8("ジャスト回避成功！"));
        }
    }

    if (!rightHitBox && !leftHitBox)
        return;

    if (hitActors.contains(actor))
        return;

    player->TakeDamage(10);

    hitActors.insert(actor);
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