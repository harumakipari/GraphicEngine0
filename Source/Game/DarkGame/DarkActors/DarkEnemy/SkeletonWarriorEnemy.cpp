#include "pch.h"
#include "SkeletonWarriorEnemy.h"

#include "Components/Render/PointLightComponent.h"
#include "Engine/Debug/DebugRender.h"
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
    // Notify assets are authored under Data/Animation/Skeleton, not the scene actor name.
    controller->SetOwnerName("Skeleton");
    // 全てのNotifyAssetsをロードする
    controller->LoadAllNotifyAssets("Skeleton");


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

    // Reuse the established Player-vs-Grux impact assets, owned by this
    // normal enemy so their positions follow the skeleton rather than the boss.
    hitSwordEffectComponent = AddComponent<ParticleComponent>("SkeletonHitEffect", parentName);
    hitSwordEffectComponent->Load("./Data/Effect/Files/NormalAttackHitEffect.json");
    rushHitRingEffectComponent = AddComponent<ParticleComponent>("SkeletonRushHitRing", parentName);
    rushHitRingEffectComponent->Load("./Data/Effect/Files/RushHitRingEffect.json");
    rushHitSparkEffectComponent = AddComponent<ParticleComponent>("SkeletonRushHitSpark", parentName);
    rushHitSparkEffectComponent->Load("./Data/Effect/Files/RushCoreEffect.json");

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

    DrawDangerAreaDebug();
}

bool SkeletonWarriorActor::TakeDamageFromPlayer(int damage)
{
    if (state == State::Dead || damage <= 0)
        return false;

    hp = (std::max)(0, hp - damage);
    CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_damage.wav", 0.25f);
    if (hp > 0)
        return true;

    state = State::Dead;
    attackHitActive = false;
    isDangerWindow = false;
    activeDangerNotifyState = nullptr;
    ResetWeaponSweep();
    PlayBodyAnimation("Idle", true, true, 0.1f, true);
    if (sword) sword->SetIsVisible(false);
    if (shield) shield->SetIsVisible(false);
    if (const auto capsule = std::dynamic_pointer_cast<CapsuleComponent>(
        FindComponentByName("capsuleComponent")))
    {
        capsule->DisableCollision();
    }
    return true;
}

void SkeletonWarriorActor::SpawnPlayerHitEffect(const DirectX::XMFLOAT3& hitPosition,
    const DirectX::XMFLOAT3& hitNormal, const DirectX::XMFLOAT3& playerPosition)
{
    (void)hitNormal;
    (void)playerPosition;
    if (!hitSwordEffectComponent)
        return;

    hitSwordEffectComponent->SetWorldLocationDirect(hitPosition);
    hitSwordEffectComponent->UpdateComponentToWorld();
    EffectManager::EmitParticle(hitSwordEffectComponent->GetEffectHandle(),
        hitSwordEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });
}

void SkeletonWarriorActor::SpawnPlayerRushHitEffect(const DirectX::XMFLOAT3& hitPosition,
    const DirectX::XMFLOAT3& hitNormal, const DirectX::XMFLOAT3& playerPosition,
    const bool hasHitPosition, const bool hasHitNormal)
{
    DirectX::XMFLOAT3 surfaceNormal = hitNormal;
    const float normalLengthSq = surfaceNormal.x * surfaceNormal.x + surfaceNormal.y * surfaceNormal.y + surfaceNormal.z * surfaceNormal.z;
    const bool useSurfacePosition = hasHitPosition && hasHitNormal && normalLengthSq > 0.000001f;
    DirectX::XMFLOAT3 effectPosition{};
    if (useSurfacePosition)
    {
        surfaceNormal = MathHelper::Multiply(surfaceNormal, 1.0f / std::sqrt(normalLengthSq));
        effectPosition = MathHelper::Add(hitPosition, MathHelper::Multiply(surfaceNormal, 0.05f));
    }
    else
    {
        effectPosition = GetPosition();
        effectPosition.y += 1.1f;
        DirectX::XMFLOAT3 direction = MathHelper::Subtract(playerPosition, effectPosition);
        direction.y = 0.0f;
        if (MathHelper::Length(direction) > FLT_EPSILON)
            effectPosition = MathHelper::Add(effectPosition, MathHelper::Multiply(MathHelper::Normalize(direction), 0.5f));
    }

    if (rushHitSparkEffectComponent)
    {
        rushHitSparkEffectComponent->SetWorldLocationDirect(effectPosition);
        rushHitSparkEffectComponent->UpdateComponentToWorld();
        EffectManager::EmitParticle(rushHitSparkEffectComponent->GetEffectHandle(),
            rushHitSparkEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });
    }
    if (rushHitRingEffectComponent)
    {
        rushHitRingEffectComponent->SetWorldLocationDirect(effectPosition);
        rushHitRingEffectComponent->UpdateComponentToWorld();
        EffectManager::EmitParticle(rushHitRingEffectComponent->GetEffectHandle(),
            rushHitRingEffectComponent->GetComponentLocation(),
            rushHitRingEffectComponent->GetComponentEulerRotation());
    }
}

void SkeletonWarriorActor::BeginAttack(const DirectX::XMFLOAT3& directionToPlayer)
{
    if (rotationComponent)
        rotationComponent->SetDirection(directionToPlayer);

    // The just-dodge area must stay aligned with the committed swing, rather
    // than rotating after a player who moves during the animation.
    lockedAttackForward = directionToPlayer;
    lockedAttackUp = { 0.0f, 1.0f, 0.0f };
    lockedAttackRight = { directionToPlayer.z, 0.0f, -directionToPlayer.x };

    state = State::Attacking;
    stateElapsed = 0.0f;
    attackHitActive = false;
    isDangerWindow = false;
    activeDangerNotifyState = nullptr;
    hasHitPlayerThisAttack = false;
    hasJustDodgedPlayerThisAttack = false;
    ResetWeaponSweep();
    PlayBodyAnimation("Attack", false, true, 0.08f, true);
}

void SkeletonWarriorActor::UpdateAttack(float elapsedTime, Player& player)
{
    stateElapsed += elapsedTime;
    RefreshDangerAreaFromNotify();
    if (isDangerWindow)
    {
        UpdateDangerWindow(player);
    }
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
        isDangerWindow = false;
        activeDangerNotifyState = nullptr;
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

    if (hasHitPlayerThisAttack || hasJustDodgedPlayerThisAttack ||
        (!rootSucceeded && !tipSucceeded))
        return;

    const HitResultWithActor& hit = rootSucceeded ? rootHit : tipHit;
    if (hit.actor != &player)
        return;

    if (player.TryTakeDamage(attackDamage, GetPosition()))
        hasHitPlayerThisAttack = true;
}

bool SkeletonWarriorActor::TryStartJustDodgeSuccess(Player& player)
{
    if (hasJustDodgedPlayerThisAttack || !isDangerWindow ||
        !player.GetJustDodgeWindow() || !player.CanJustDodgeAgainst(GetPosition()))
    {
        return false;
    }

    const auto self = std::dynamic_pointer_cast<Enemy>(shared_from_this());
    if (!self)
        return false;

    hasJustDodgedPlayerThisAttack = true;
    player.StartJustDodgeSuccess(self);
    return true;
}

void SkeletonWarriorActor::UpdateDangerWindow(Player& player)
{
    if (hasJustDodgedPlayerThisAttack || !IsPlayerInsideDangerArea(player) ||
        !player.GetJustDodgeWindow())
    {
        return;
    }

    TryStartJustDodgeSuccess(player);
}

void SkeletonWarriorActor::RefreshDangerAreaFromNotify()
{
    const AnimationNotifyState* notify = activeDangerNotifyState;
    if (!notify)
        notify = GetAttackDangerNotifyState();
    if (!notify)
        return;

    dangerArea = BuildDangerArea(GetPosition(), lockedAttackRight, lockedAttackUp,
        lockedAttackForward, notify->justDodgeAreaOffset, notify->justDodgeAreaSize);
}

bool SkeletonWarriorActor::IsPlayerInsideDangerArea(Player& player) const
{
    DirectX::XMFLOAT3 capsuleCenter = player.GetPosition();
    float capsuleRadius = 0.0f;
    float capsuleHeight = 0.0f;
    if (const auto capsule = std::dynamic_pointer_cast<CapsuleComponent>(
        player.FindComponentByName("capsuleComponent")))
    {
        capsuleCenter = capsule->GetComponentLocation();
        capsuleRadius = capsule->GetRadius();
        capsuleHeight = capsule->GetHeight();
    }
    return dangerArea.IntersectsPlayerCapsule(capsuleCenter, capsuleRadius, capsuleHeight).overlap;
}

AnimationNotifyState* SkeletonWarriorActor::GetAttackDangerNotifyState()
{
    const auto controller = GetBodyAnimationController();
    if (!controller)
        return nullptr;
    auto* asset = controller->GetNotifyAssetForRuntimeTuning(1);
    if (!asset)
        return nullptr;
    for (auto& notify : asset->notifyTrack.states)
    {
        if (notify.type == AnimationNotifyState::Type::DangerWindow)
            return &notify;
    }
    return nullptr;
}

void SkeletonWarriorActor::DrawDangerAreaDebug() const
{
    if (!dangerAreaDebug || state != State::Attacking)
        return;

    const DirectX::XMFLOAT4 color = hasJustDodgedPlayerThisAttack
        ? DirectX::XMFLOAT4{ 0.15f, 1.0f, 0.25f, 1.0f }
        : isDangerWindow ? DirectX::XMFLOAT4{ 1.0f, 0.2f, 0.1f, 1.0f }
        : DirectX::XMFLOAT4{ 1.0f, 0.75f, 0.15f, 1.0f };
    DebugRender::DrawBox(dangerArea.WorldTransform(), dangerArea.size, color, 0.0f, true);
    DebugRender::DrawSphere(dangerArea.center, 0.08f, color, 0.0f, true);
}

void SkeletonWarriorActor::OnAnimationNotifyBegin(const AnimationNotifyState& notify)
{
    Enemy::OnAnimationNotifyBegin(notify);
    if (state != State::Attacking || notify.type != AnimationNotifyState::Type::DangerWindow)
        return;

    activeDangerNotifyState = &notify;
    isDangerWindow = true;
    RefreshDangerAreaFromNotify();
}

void SkeletonWarriorActor::OnAnimationNotifyEnd(const AnimationNotifyState& notify)
{
    Enemy::OnAnimationNotifyEnd(notify);
    if (notify.type != AnimationNotifyState::Type::DangerWindow ||
        activeDangerNotifyState != &notify)
    {
        return;
    }

    isDangerWindow = false;
    activeDangerNotifyState = nullptr;
}

void SkeletonWarriorActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    Character::DrawImGuiDetails();

    ImGui::SeparatorText("Tutorial Skeleton Danger Area");
    ImGui::Checkbox("Danger Area Debug", &dangerAreaDebug);
    AnimationNotifyState* notify = GetAttackDangerNotifyState();
    const auto controller = GetBodyAnimationController();
    if (!notify || !controller)
    {
        ImGui::TextDisabled("Attack DangerWindow notify data is not loaded.");
        return;
    }

    const float animationLength = controller->GetAnimationLength("Attack");
    bool changed = false;
    changed |= ImGui::DragFloat("DangerWindow Start (sec)", &notify->startTime,
        0.01f, 0.0f, animationLength, "%.3f");
    changed |= ImGui::DragFloat("DangerWindow End (sec)", &notify->endTime,
        0.01f, 0.0f, animationLength, "%.3f");
    notify->startTime = (std::clamp)(notify->startTime, 0.0f, animationLength);
    notify->endTime = (std::clamp)(notify->endTime, notify->startTime, animationLength);
    ImGui::TextDisabled("Center Offset (local: X=Right, Y=Up, Z=Forward)");
    changed |= ImGui::DragFloat3("Danger Area Center Offset", &notify->justDodgeAreaOffset.x,
        0.05f, -20.0f, 20.0f, "%.2f");
    ImGui::TextDisabled("Full Size (Width, Height, Depth)");
    changed |= ImGui::DragFloat3("Danger Area Size", &notify->justDodgeAreaSize.x,
        0.05f, 0.0f, 20.0f, "%.2f");
    notify->justDodgeAreaSize.x = (std::max)(0.0f, notify->justDodgeAreaSize.x);
    notify->justDodgeAreaSize.y = (std::max)(0.0f, notify->justDodgeAreaSize.y);
    notify->justDodgeAreaSize.z = (std::max)(0.0f, notify->justDodgeAreaSize.z);
    if (changed && isDangerWindow)
        RefreshDangerAreaFromNotify();

    ImGui::Text("Animation Length: %.3f sec", animationLength);
    ImGui::Text("DangerWindow Active: %s", isDangerWindow ? "YES" : "NO");
    ImGui::Text("Just Dodge Succeeded: %s", hasJustDodgedPlayerThisAttack ? "YES" : "NO");

    auto* asset = controller->GetNotifyAssetForRuntimeTuning(1);
    const size_t stateIndex = static_cast<size_t>(notify - asset->notifyTrack.states.data());
    if (ImGui::Button("Save Attack Danger Area"))
    {
        std::string savePath;
        const auto result = controller->SaveDangerObbForRuntimeTuning(1, stateIndex, *notify, savePath);
        dangerAreaSaveStatus = result == AnimationController::RuntimeDangerObbSaveResult::Saved
            ? "Saved: " + savePath : "Save failed";
    }
    if (!dangerAreaSaveStatus.empty())
        ImGui::TextUnformatted(dangerAreaSaveStatus.c_str());
#endif
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
