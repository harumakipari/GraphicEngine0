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
        skeletalMeshComponent->SetModel("./Data/Models/Characters/Player/player.gltf", false, true);
        skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Player;   // オブジェクトの種類を Player に設定
        skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 20.9f;   // 自己発光の強さを設定
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

        controller->AddNotifyEvent("Primary_Attack_Fast_A", 0.583f, AnimationNotifyEvent::Type::PlaySE, "start");
        controller->AddNotifyState("Primary_Attack_Fast_A", 0.17f, 0.23f, AnimationNotifyState::Type::HitBox);
        controller->AddNotifyState("Primary_Attack_Fast_A", 0.12f, 0.583f, AnimationNotifyState::Type::ComboWindow);
        controller->AddNotifyState("Primary_Attack_Fast_A", 0.4f, 0.583f, AnimationNotifyState::Type::TransitionWindow);

        controller->AddNotifyEvent("Primary_Attack_Fast_B", 0.315f, AnimationNotifyEvent::Type::PlaySE, "star");
        controller->AddNotifyState("Primary_Attack_Fast_B", 0.138f, 0.265f, AnimationNotifyState::Type::HitBox);
        controller->AddNotifyState("Primary_Attack_Fast_B", 0.138f, 0.583f, AnimationNotifyState::Type::ComboWindow);
        controller->AddNotifyState("Primary_Attack_Fast_B", 0.4f, 0.583f, AnimationNotifyState::Type::TransitionWindow);

        controller->AddNotifyEvent("Primary_Attack_Fast_C", 0.315f, AnimationNotifyEvent::Type::PlaySE, "turning");
        controller->AddNotifyState("Primary_Attack_Fast_C", 0.132f, 0.22f, AnimationNotifyState::Type::HitBox);
        controller->AddNotifyState("Primary_Attack_Fast_C", 0.132f, 0.583f, AnimationNotifyState::Type::ComboWindow);
        controller->AddNotifyState("Primary_Attack_Fast_C", 0.4f, 0.583f, AnimationNotifyState::Type::TransitionWindow);

        controller->AddNotifyEvent("Primary_Attack_Fast_D1_1", 0.315f, AnimationNotifyEvent::Type::PlaySE, "turning");
        controller->AddNotifyState("Primary_Attack_Fast_D1_1", 0.414f, 0.548f, AnimationNotifyState::Type::HitBox);
        controller->AddNotifyState("Primary_Attack_Fast_D1_1", 0.08f, 1.0f, AnimationNotifyState::Type::ComboWindow);
        controller->AddNotifyState("Primary_Attack_Fast_D1_1", 0.52f, 1.0f, AnimationNotifyState::Type::TransitionWindow);

        controller->AddCombo("Primary_Attack_Fast_A", "Primary_Attack_Fast_B");
        controller->AddCombo("Primary_Attack_Fast_B", "Primary_Attack_Fast_C");
        controller->AddCombo("Primary_Attack_Fast_C", "Primary_Attack_Fast_D1_1");

        // ジャスト回避
        controller->AddNotifyState("Ability_RMB_Bwd_0", 0.16f, 0.53f, AnimationNotifyState::Type::JustDodgeWindow);
        controller->AddNotifyState("Ability_RMB_Bwd_0", 0.05f, 0.6f, AnimationNotifyState::Type::Invincible);

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

        // ステートマシンを character に追加
        //this->SetStateMachine(stateMachine);
        // 初期ステートを設定
        stateMachine_->ChangeState("Idle");
    }

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
        characterMovementComponent->SetUseGravity(false);

        // 回転用コンポーネントを追加
        rotationComponent = this->AddComponent<class RotationComponent>("rotationComponent", parentName);
    }

    int weaponSocketNode = skeletalMeshComponent->FindIndexByName("weapon");

    // 剣に当たり判定のコンポーネントを追加
    swordCollisionComp = AddComponent<CapsuleComponent>("SwordCollision", parentName);
    DirectX::XMFLOAT3 size = { 0.1f,1.2f,1.0f };
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

            // 相手のActor取得
            Actor* actor = other->GetOwner();

            if (!actor)
            {
                Logger::Warning("actor is nullptr");
                return;
            }

            if (!hitBox)
                return;

            if (hitActors.contains(actor))
            {// 一度当たったことがあった場合
                return;
            }

            // Enemyへキャスト
            GruxEnemy* enemy = dynamic_cast<GruxEnemy*>(actor);

            if (!enemy)
                return;

            enemy->TakeDamage(10);
            hitActors.insert(actor);

        });

    auto swordMeshComponent = this->AddComponent<SkeletalMeshComponent>("Sword", parentName);
    swordMeshComponent->SetModel("./Data/Models/Weapons/PlayerSword/Sword.gltf", false, true);
    swordMeshComponent->AttachToComponent(skeletalMeshComponent, weaponSocketNode); // "VB root_weapon"

#if 0
    auto bowMeshComponent = this->AddComponent<SkeletalMeshComponent>("Bow", parentName);
    bowMeshComponent->SetModel("./Data/Models/Weapons/PlayerBow/AnimationBow.gltf", false, true);

    // 武器アニメーションコントローラーを作成
    auto weaponController = std::make_shared<AnimationController>(bowMeshComponent.get());
    weaponController->AddAnimation("Bow", 0);
    AddAnimationController("Weapon", weaponController);
    PlayAnimation("Weapon", "Bow");
#endif // 0

    swordPointComp = AddComponent<CapsuleComponent>("SwordPointComponent", "SwordCollision");
    swordPointComp->SetRelativeLocationDirect({ 0.0f,0.0f,0.6f });

    // 火花エフェクト用のコンポーネントを追加
    sparkComponent = this->AddComponent<class ParticleComponent>("particleComponent", parentName);
    sparkComponent->Load("./Data/Effect/Files/DarkStageSparkEffect.json");
}


void Player::Update(float elapsedTime)
{
    using namespace DirectX;

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
    Character::Update(elapsedTime);

    // 剣のデバックの当たり判定を描画するかどうか
    swordCollisionComp->SetIsVisibleDebugShape(hitBox);

    // アニメーション時間から攻撃有効フラグ更新
    auto anim = GetBodyAnimationController();
    float time = anim->GetCurrentAnimationTime(); // ← 秒
    if (stateMachine_->GetStateName() == "Attack" && time >= 0.1f && time <= 0.4f)
    {
        isAttackActive = true;
    }
    else
    {
        isAttackActive = false;
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
            p.life -= elapsedTime;
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
    case AnimationNotifyState::Type::ComboWindow:
        comboWindow = true;
        Logger::Log(U8("コンボ受付を開始しました"));
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
    case AnimationNotifyState::Type::ComboWindow:
        comboWindow = false;
        Logger::Log(U8("コンボ受付を終了しました"));
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
        Logger::Log(U8("SEがなる！"));
#if 1
        std::string audioPath = "./Data/Sound/SE/" + event.parameter + ".wav";
        CoreAudio::PlayOneShot(audioPath, 1.0f);
#endif // 0
    }
    break;
    case AnimationNotifyEvent::Type::SpawnEffect:
        break;
    }
}

// アニメーションステート関連のフラグをリセットする
void Player::ResetAnimationStateFlag()
{
    transitionWindow = false;  // ステート遷移してもいいかどうか
    comboQueued = false;   // コンボ攻撃がキューに入っているかどうか
    comboWindow = false;   // コンボ受付をするかどうか
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

//当たった時の処理
void Player::TakeDamage(int damage)
{
#if 1
    if (invincibleWindow)
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

// 攻撃ヒット時の処理
void Player::DoAttackHit()
{
    auto enemies = GetOwnerScene()->GetActorManager()->GetActorsOfType<Character>();

    for (auto& actor : enemies)
    {
        auto enemy = std::dynamic_pointer_cast<GruxEnemy>(actor);
        if (!enemy) continue;

        auto p = GetPosition();
        auto e = enemy->GetPosition();

        // 敵へのベクトル
        float dx = e.x - p.x;
        float dz = e.z - p.z;

        float distSq = dx * dx + dz * dz;
        float attackRange = 2.5f;

        if (distSq > attackRange * attackRange)
            return;

        // 正規化
        float len = sqrtf(dx * dx + dz * dz);
        dx /= len;
        dz /= len;

        // プレイヤーの前方向（Z+方向）
        DirectX::XMFLOAT3 forward = GetForward();

        float dot = dx * forward.x + dz * forward.z;

        float angleCos = cosf(DirectX::XMConvertToRadians(60.0f)); // 60度

        if (dot > angleCos)
        {
            enemy->TakeDamage(10);
            //　ヒットストップ発動
            Time::timeScale = 0.1f;
            hitStopTimer = 0.35f;
        }
    }
}

// 攻撃開始時の処理
void Player::StartAttack()
{
    hitActors.clear();
}

// ジャスト回避成功時の処理
void Player::StartJustDodgeSuccess()
{
    // ジャスト回避成功フラグをオンにする
    justDodgeSuccess = true;
    // スローモーションにする
    Time::SetSlow(0.2f, 1.0f);

    // 画面の色を変える

    // UIを表示する
}

// インタラクト対象検索
IInteractable* Player::FindInteractable()
{
    float bestDist = 2.0f;
    IInteractable* best = nullptr;

    DirectX::XMFLOAT3 forward = GetForward(); // プレイヤー前方向

    for (auto& actor : GetOwnerScene()->GetActorManager()->GetAllActors())
    {
        auto interactable = dynamic_cast<IInteractable*>(actor.get());
        if (!interactable) continue;

        DirectX::XMFLOAT3 dir = MathHelper::Normalize(
            MathHelper::Subtract(actor->GetPosition(), GetPosition())
        );

        float dot = MathHelper::Dot(forward, dir);

        // 前方60度以内
        if (dot < 0.5f) continue;

        float dist = MathHelper::Distance(GetPosition(), actor->GetPosition());

        if (dist < bestDist)
        {
            bestDist = dist;
            best = interactable;
        }
    }

    return best;
}

