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

#if 1
        // アニメーションコントローラーを作成
        auto controller = std::make_shared<AnimationController>(skeletalMeshComponent.get(), rootNodeIndex);
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
#else
        // アニメーションコントローラーを作成
        auto controller = std::make_shared<AnimationController>(skeletalMeshComponent.get(), rootNodeIndex);
        controller->AddAnimation("Ability_E", 0);
        controller->AddAnimation("Ability_R", 1);
        controller->AddAnimation("Idle", 2);
        controller->AddAnimation("anim_idleLever", 3);
        controller->AddAnimation("anim_openDoor_L", 4);
        controller->AddAnimation("anim_openDoor_R", 5);
        controller->AddAnimation("anim_PullLever", 6);
        controller->AddAnimation("anim_ReachLever", 7);
        controller->AddAnimation("Death", 8);
        controller->AddAnimation("Emote_Ice_Sculpture", 9);
        controller->AddAnimation("FrontEndPose", 10);
        controller->AddAnimation("HitReact_Back", 11);
        controller->AddAnimation("HitReact_Front", 12);
        controller->AddAnimation("HitReact_Left", 13);
        controller->AddAnimation("HitReact_Right", 14);
        controller->AddAnimation("Idle_Noise_A", 15);
        controller->AddAnimation("Idle_Noise_B", 16);
        controller->AddAnimation("Jog_Fwd", 17);
        controller->AddAnimation("Jog_Fwd_Start", 18);
        controller->AddAnimation("Jog_Fwd_Stop", 19);
        controller->AddAnimation("Level_Start", 20);
        controller->AddAnimation("Primary_Attack_Fast_A", 21);
        controller->AddAnimation("Primary_Attack_Fast_B", 22);
        controller->AddAnimation("Primary_Attack_Fast_C", 23);
        controller->AddAnimation("Primary_Attack_Fast_D", 24);
        controller->AddAnimation("Primary_Fire_Fast_0", 25);
        controller->AddAnimation("Recall", 26);
        controller->AddAnimation("Roll_back_0", 27);
        controller->AddAnimation("Roll_back_left_45", 28);
        controller->AddAnimation("Roll_back_right_45", 29);
        controller->AddAnimation("Roll_front_0", 30);
        controller->AddAnimation("Roll_front_left_45", 31);
        controller->AddAnimation("Roll_front_right_45", 32);
        controller->AddAnimation("Roll_left_0", 33);
        controller->AddAnimation("Roll_right_0", 34);

#endif // 0

        controller->AddNotify(0, 0.3f, AnimationNotify::Type::HitStart);
        controller->AddNotify(0, 0.5f, AnimationNotify::Type::HitEnd);

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



    AddHitCallback([&](std::pair<CollisionComponent*, CollisionComponent*> hitPair)
        {
#if 0
            CollisionComponent* own = hitPair.first;
            CollisionComponent* other = hitPair.second;

            // 自分が武器じゃなければ無視
            if (!(own->GetCollisionLayer() & CollisionHelper::ToBit(CollisionLayer::PlayerWeapon)))
                return;

            // 攻撃中じゃなければ無視
            if (stateMachine_->GetStateName() != "Attack")
                return;

            // すでに当たってたら無視
            if (hitTargets.contains(other))
                return;

            hitTargets.insert(other);

            uint32_t layer = other->GetCollisionLayer();

            if (layer & CollisionHelper::ToBit(CollisionLayer::WorldStatic) ||
                layer & CollisionHelper::ToBit(CollisionLayer::WorldProps))
            {
                auto hitPos = swordPointComp->GetComponentLocation();
                SpawnSpark(hitPos);
            }
#endif // 0

        }
    );
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
    swordCollisionComp->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Trigger);
    swordCollisionComp->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Trigger);
    swordCollisionComp->SetCollisionOffsetY(height * 0.5f);
    swordCollisionComp->SetIsVisibleDebugBox(false);
    swordCollisionComp->SetRelativeLocationDirect({ -0.f, -0.f, 0.8f });
    swordCollisionComp->Initialize();
    
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
    //bowMeshComponent->AttachToComponent(skeletalMeshComponent, 23); // "weapon"

    swordPointComp = AddComponent<CapsuleComponent>("SwordPointComponent", "SwordCollision");
    swordPointComp->SetRelativeLocationDirect({ 0.0f,0.0f,0.6f });

    // 火花エフェクト用のコンポーネントを追加
    sparkComponent = this->AddComponent<class ParticleComponent>("particleComponent", parentName);
    sparkComponent->Load("./Data/Effect/Files/DarkStageSparkEffect.json");



    comboAttacks =
    {
       {
        "Primary_Attack_Fast_A",
        0.10f,
        0.25f,
        0.20f,
        0.5f,
        1,
        1.5f
        },

        {
            "Primary_Attack_Fast_B",
            0.08f,
            0.30f,
            0.25f,
            0.50f,
            2,
            1.0f
        },

        {
            "Primary_Attack_Fast_C",
            0.15f,
            0.40f,
            0.1f,
            0.5f,
            3,
            0.0f
        },

        {
            "Primary_Attack_Fast_D",
            0.15f,
            0.50f,
            -1.0f,
            -1.0f,
            -1,
            0.0f
        }

    };
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
    DrawAttackEditorImGui();
    Character::DrawImGuiDetails();
#endif

}

// アタックエディタ
void Player::DrawAttackEditorImGui()
{
    ImGui::SeparatorText("Combo Editor");

    for (int i = 0; i < comboAttacks.size(); i++)
    {
        auto& attack = comboAttacks[i];

        ImGui::PushID(i);

        if (ImGui::TreeNode(
            attack.animationName.c_str()))
        {
            ImGui::SliderFloat("HitStart", &attack.hitStart, 0.0f, 2.0f);

            ImGui::SliderFloat("HitEnd", &attack.hitEnd, 0.0f, 2.0f);

            ImGui::SliderFloat("ComboStart", &attack.comboWindowStart, 0.0f, 2.0f);

            ImGui::SliderFloat("ComboEnd", &attack.comboWindowEnd, 0.0f, 2.0f);

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
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
    if (invincible)
    {// 無敵状態ならダメージを受けない
        Logger::Log(U8("攻撃を回避した"));
        return;
    }
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

