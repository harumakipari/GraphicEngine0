#include "pch.h"
#include "SkeletonWarriorEnemy.h"

#include "Components/Render/PointLightComponent.h"
#include "Engine/Scene/SceneBase.h"
#include "Game/Actors/Player/Player.h"
#include "Physics/CollisionFunction.h"

void SkeletonWarriorActor::Initialize(const Transform& transform)
{
    std::string parentName = "SkeletonWarriorMeshComponent";
    Enemy::Initialize(transform);
    skeletalMeshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    //skeletalMeshComponent->SetModel("./Data/Models/Characters/Skeleton/Skeleton.gltf");
    skeletalMeshComponent->SetModel("./Data/Models/Characters/Skeleton/Skeleton.gltf", false, true);
    skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Enemy;   // オブジェクトの種類を Enemy に設定
    skeletalMeshComponent->SetRelativeLocationDirect({ 0.0f,-0.3f,0.0f });

    // アニメーションコントローラーを作成
    int rootIndex = skeletalMeshComponent->FindIndexByName("root");
    auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootIndex);
    controller->AddAnimation("Walk", 0);
    controller->AddAnimation("Attack", 1);
    controller->AddAnimation("Idle", 2);
    // アニメーションコントローラーを character に追加
    this->AddBodyAnimationController(controller);
    // アニメーションコントローラーのオーナーの名前を設定する
    controller->SetOwnerName(GetName());
    // 全てのNotifyAssetsをロードする
    controller->LoadAllNotifyAssets(GetName());


    PlayBodyAnimation("Idle");

    // 盾
    //shield = AddComponent<SkeletalMeshComponent>("ShieldMesh", parentName);
    //shield->SetModel("./Data/Models/Weapons/Shield/Shield.gltf");
    //shield->AttachToComponent(skeletalMeshComponent, 11); // "Hand_l_end"


    int handRightSocketNode = skeletalMeshComponent->FindIndexByName("Hand_r_end");

    // 剣
    sword = AddComponent<SkeletalMeshComponent>("SwordMesh", parentName);
    sword->SetModel("./Data/Models/Weapons/Sword/Sword.gltf");
    sword->AttachToComponent(skeletalMeshComponent, handRightSocketNode); // "Hand_r_end"
    sword->SetRelativeLocationDirect({ -0.f, -0.1f, -0.0f });
    sword->SetRelativeEulerRotationDirect({ 0.0f, 90.f, 0.0f });
    sword->SetRelativeScaleDirect({ 1.37f,1.37f,1.37f });

    // 剣コンポーネントを親とする    剣の先と剣の元のコンポーネント
    weaponRootPoint = AddComponent<SceneComponent>("SwordHitRoot", "SwordMesh");
    weaponRootPoint->SetRelativeLocationDirect(weaponRootOffset);
    weaponTipPoint = AddComponent<SceneComponent>("SwordHitTip", "SwordMesh");
    weaponTipPoint->SetRelativeLocationDirect(weaponTipOffset);

    // 当たり判定
    {
        std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("capsuleComponent", parentName);
        DirectX::XMFLOAT3 size = skeletalMeshComponent->GetModelSize();
        height = size.y * 1.3f;
        radius = size.x * 0.5f;
        mass = 150.0f;
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

    // 回転用コンポーネントを追加
    rotationComponent = this->AddComponent<class RotationComponent>("rotationComponent", parentName);
    hp = maxHp;

#if 0
    auto scene = dynamic_cast<SceneBase*>(Scene::GetCurrentScene());
    // ポイントライトコンポーネントを追加
    auto pointLightComponent = this->AddComponent<PointLightComponent>("pointLightComponent", parentName);
    pointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f, 1.0f });
    auto lightManager = scene->GetLightManager();
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    if (auto shared = lightManager->FindSharedLight("EnemyPointLight"))
    {
        pointLightComponent->SetSharedParam(shared);
    }
#endif // 0

}

void SkeletonWarriorActor::Update(float elapsedTime)
{
    Enemy::Update(elapsedTime);
    if (state == State::Dead)
        return;

    auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
    if (!player || player->IsPendingKill() || player->GetHp() <= 0)
        return;

    DirectX::XMFLOAT3 toPlayer = MathHelper::Subtract(player->GetPosition(), GetPosition());
    toPlayer.y = 0.0f;
    const float playerDistance = MathHelper::Length(toPlayer);
    const DirectX::XMFLOAT3 directionToPlayer = playerDistance > 0.0001f
        ? MathHelper::Normalize(toPlayer) : GetForward();

    switch (state)
    {
    case State::Idle:
        if (playerDistance <= facePlayerDistance && rotationComponent)
            rotationComponent->SetDirection(directionToPlayer);
        if (playerDistance <= attackRange)
            BeginAttack(directionToPlayer);
        break;
    case State::Attacking:
        UpdateAttack(elapsedTime, *player);
        break;
    case State::Recovery:
        UpdateRecovery(elapsedTime);
        break;
    case State::Dead:
        break;
    }
}

void SkeletonWarriorActor::TakeDamage(int damage)
{
    if (state == State::Dead || damage <= 0)
        return;

    hp = (std::max)(0, hp - damage);
    if (hp > 0)
        return;

    state = State::Dead;
    attackHitActive = false;
    ResetWeaponSweep();
    PlayBodyAnimation("Idle", true, true, 0.1f, true);
    if (sword) sword->SetIsVisible(false);
    if (shield) shield->SetIsVisible(false);
}

void SkeletonWarriorActor::BeginAttack(const DirectX::XMFLOAT3& directionToPlayer)
{
    if (rotationComponent)
        rotationComponent->SetDirection(directionToPlayer);

    state = State::Attacking;
    stateElapsed = 0.0f;
    attackHitActive = false;
    hasHitPlayerThisAttack = false;
    ResetWeaponSweep();
    PlayBodyAnimation("Attack", false, true, 0.08f, true);
}

void SkeletonWarriorActor::UpdateAttack(float elapsedTime, Player& player)
{
    stateElapsed += elapsedTime;
    const bool hitWindow = stateElapsed >= attackHitStartTime && stateElapsed <= attackHitEndTime;
    if (hitWindow)
    {
        if (!attackHitActive)
            ResetWeaponSweep();
        attackHitActive = true;
        UpdateWeaponSweep(player);
    }
    else if (attackHitActive)
    {
        attackHitActive = false;
        ResetWeaponSweep();
    }

    if (stateElapsed >= attackDuration)
    {
        state = State::Recovery;
        stateElapsed = 0.0f;
        attackHitActive = false;
        ResetWeaponSweep();
        PlayBodyAnimation("Idle", true, true, 0.1f, true);
    }
}

void SkeletonWarriorActor::UpdateRecovery(float elapsedTime)
{
    stateElapsed += elapsedTime;
    if (stateElapsed >= recoveryDuration)
    {
        state = State::Idle;
        stateElapsed = 0.0f;
    }
}

void SkeletonWarriorActor::UpdateWeaponSweep(Player& player)
{
    const DirectX::XMFLOAT3 root = GetWeaponRootPosition();
    const DirectX::XMFLOAT3 tip = GetWeaponTipPosition();
    if (!hasPreviousWeaponPoints)
    {
        previousWeaponRoot = root;
        previousWeaponTip = tip;
        hasPreviousWeaponPoints = true;
        return;
    }

    HitResultWithActor rootHit;
    HitResultWithActor tipHit;
    const uint32_t playerMask = CollisionHelper::ToBit(CollisionLayer::Player);
    const bool rootSucceeded = CollisionFunction::SphereRayCast(
        previousWeaponRoot, root, rootHit, weaponHitRadius, playerMask);
    const bool tipSucceeded = CollisionFunction::SphereRayCast(
        previousWeaponTip, tip, tipHit, weaponHitRadius, playerMask);

    previousWeaponRoot = root;
    previousWeaponTip = tip;

    if (hasHitPlayerThisAttack || (!rootSucceeded && !tipSucceeded))
        return;

    const HitResultWithActor& hit = rootSucceeded ? rootHit : tipHit;
    if (hit.actor == &player && player.TryTakeDamage(attackDamage, GetPosition()))
        hasHitPlayerThisAttack = true;
}

void SkeletonWarriorActor::ResetWeaponSweep()
{
    hasPreviousWeaponPoints = false;
}

DirectX::XMFLOAT3 SkeletonWarriorActor::GetWeaponRootPosition() const
{
    return weaponRootPoint ? weaponRootPoint->GetComponentLocation() : GetPosition();
}

DirectX::XMFLOAT3 SkeletonWarriorActor::GetWeaponTipPosition() const
{
    return weaponTipPoint ? weaponTipPoint->GetComponentLocation() : GetPosition();
}

