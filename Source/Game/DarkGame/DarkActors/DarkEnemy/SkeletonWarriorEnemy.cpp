#include "pch.h"
#include "SkeletonWarriorEnemy.h"

#include "Components/Render/PointLightComponent.h"
#include "Engine/Debug/DebugRender.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Utility/Time.h"
#include "Engine/Effects/EffectManager.h"
#include "Game/Actors/Camera/DarkGameCamera.h"
#include "Game/Actors/Player/Player.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/SkeletonBodyPartActor.h"
#include "Physics/CollisionFunction.h"
#include <tracy/Tracy.hpp>


namespace
{
}

class SkeletonHpBarUI
{
public:
    std::shared_ptr<UIImageComponent> background;
    std::shared_ptr<UIGaugeFillComponent> delayedFill;
    std::shared_ptr<UIGaugeFillComponent> currentFill;
    std::shared_ptr<UIImageComponent> frame;
    float delayedHp = 0.0f;
    float delayedHpDelayTimer = 0.0f;
    int observedHp = 0;
    bool delayedHpInitialized = false;

    void Create(UIManager* uiManager, const std::string& prefix);
    void Update(const SkeletonWarriorActor& owner, const DarkCameraActor& camera,
        const DirectX::XMFLOAT3& playerPosition);
    void Hide();
    ~SkeletonHpBarUI();

private:
    float tutorialSkeletonHpDisplayDistance = 10.0f;
    float tutorialSkeletonHpNearDistance = 4.0f;
    float tutorialSkeletonHpFarDistance = 10.0f;
    float tutorialSkeletonHpNearScaleMultiplier = 1.0f;
    float tutorialSkeletonHpFarScaleMultiplier = 0.75f;
    const DirectX::XMFLOAT3 tutorialSkeletonHpBarWorldOffset = { 0.0f, 1.1f, 0.4f };
    float tutorialSkeletonHpBarScale = 0.65f;

};

void SkeletonHpBarUI::Create(UIManager* uiManager, const std::string& prefix)
{
    const auto createImage = [uiManager](const char* texturePath, const char* name,
        const DirectX::XMFLOAT2& size, const int zOrder)
        {
            auto image = std::make_shared<UIImageComponent>(texturePath, name);
            image->SetSize(size);
            image->SetPivot({ 0.0f, 0.5f });
            image->SetVisible(false);
            image->zOrder = zOrder;
            uiManager->Add(image);
            return image;
        };

    const auto createHpBar = [&createImage, uiManager](SkeletonHpBarUI& hpBar, const std::string& prefix)
        {
            hpBar.background = createImage("./Data/Textures/UI/HpBar/skeleton_hp_background.png", (std::string(prefix) + "Background").c_str(), { 234.0f, 17.0f }, 40);
            hpBar.delayedFill = std::make_shared<UIGaugeFillComponent>(
                "./Data/Textures/UI/HpBar/skeleton_hp_fill.png", (prefix)+"DelayedFill");
            hpBar.delayedFill->SetSize({ 233.0f, 16.0f });
            hpBar.delayedFill->SetPivot({ 0.0f, 0.5f });
            hpBar.delayedFill->SetColor(CoreColor{ 0.95f, 0.72f, 0.38f, 1.0f });
            hpBar.delayedFill->SetVisible(false);
            hpBar.delayedFill->zOrder = 41;
            uiManager->Add(hpBar.delayedFill);
            hpBar.currentFill = std::make_shared<UIGaugeFillComponent>(
                "./Data/Textures/UI/HpBar/skeleton_hp_fill.png", (prefix)+"CurrentFill");
            hpBar.currentFill->SetSize({ 233.0f, 16.0f });
            hpBar.currentFill->SetPivot({ 0.0f, 0.5f });
            hpBar.currentFill->SetColor(CoreColor{ 0.55f, 0.08f, 0.06f, 1.0f });
            hpBar.currentFill->SetVisible(false);
            hpBar.currentFill->zOrder = 42;
            uiManager->Add(hpBar.currentFill);
            hpBar.frame = createImage("./Data/Textures/UI/HpBar/skeleton_hp_frame.png",
                (std::string(prefix) + "Frame").c_str(), { 239.0f, 21.0f }, 43);
        };

    createHpBar(*this, prefix);
}

void SkeletonHpBarUI::Hide()
{
    if (background) background->SetVisible(false);
    if (delayedFill) delayedFill->SetVisible(false);
    if (currentFill) currentFill->SetVisible(false);
    if (frame) frame->SetVisible(false);
}

SkeletonHpBarUI::~SkeletonHpBarUI()
{
    Hide();
    if (background) background->MarkPendingKill();
    if (delayedFill) delayedFill->MarkPendingKill();
    if (currentFill) currentFill->MarkPendingKill();
    if (frame) frame->MarkPendingKill();
}

void SkeletonHpBarUI::Update(const SkeletonWarriorActor& owner, const DarkCameraActor& camera,
    const DirectX::XMFLOAT3& playerPosition)
{
    const auto getPlayerSkeletonDistanceXZ = [&](const SkeletonWarriorActor* skeleton)
        {
            const DirectX::XMFLOAT3 delta = MathHelper::Subtract(skeleton->GetPosition(), playerPosition);
            return std::sqrt(delta.x * delta.x + delta.z * delta.z);
        };
    const auto isWithinHpDisplayDistance = [&](const SkeletonWarriorActor* skeleton)
        {
            return getPlayerSkeletonDistanceXZ(skeleton) <=
                (std::max)(tutorialSkeletonHpDisplayDistance, 0.0f);
        };
    const auto getHpDistanceScaleMultiplier = [&](const SkeletonWarriorActor* skeleton)
        {
            const float nearDistance = (std::max)(tutorialSkeletonHpNearDistance, 0.0f);
            const float farDistance = (std::max)(tutorialSkeletonHpFarDistance, nearDistance);
            const float distanceRange = farDistance - nearDistance;
            const float interpolation = distanceRange > FLT_EPSILON
                ? std::clamp((getPlayerSkeletonDistanceXZ(skeleton) - nearDistance) / distanceRange, 0.0f, 1.0f)
                : 1.0f;
            return std::lerp(tutorialSkeletonHpNearScaleMultiplier,
                tutorialSkeletonHpFarScaleMultiplier, interpolation);
        };
    const auto updateHpBar = [&](const SkeletonWarriorActor* skeleton,
        SkeletonHpBarUI& hpBar)
        {
            const auto hide = [&hpBar]()
                {
                    if (hpBar.background) hpBar.background->SetVisible(false);
                    if (hpBar.delayedFill) hpBar.delayedFill->SetVisible(false);
                    if (hpBar.currentFill) hpBar.currentFill->SetVisible(false);
                    if (hpBar.frame) hpBar.frame->SetVisible(false);
                };
            if (!skeleton || skeleton->IsDead() || skeleton->IsPendingKill() || !isWithinHpDisplayDistance(skeleton))
            {
                hide();
                return;
            }

            const auto target = skeleton->GetCameraTargetComponent();
            if (!target)
            {
                hide();
                return;
            }
            // Keep the HP bar above the Skeleton in world space so its position
            // remains correct as the camera angle and Skeleton distance change.
            const DirectX::XMFLOAT3 skeletonWorldPosition = target->GetComponentLocation();
            const DirectX::XMFLOAT3 hpBarWorldPosition = {
                skeletonWorldPosition.x + tutorialSkeletonHpBarWorldOffset.x,
                skeletonWorldPosition.y + tutorialSkeletonHpBarWorldOffset.y,
                skeletonWorldPosition.z + tutorialSkeletonHpBarWorldOffset.z
            };
            const auto projection = camera.ProjectWorldPositionForUI(hpBarWorldPosition);
            if (!projection.valid || !projection.inFront || !projection.insideViewport)
            {
                hide();
                return;
            }

            DirectX::XMFLOAT2 position = ConvertScreenToUI(projection.screenPosition);
            const float currentHp = static_cast<float>((std::max)(skeleton->GetHp(), 0));
            const float maximumHp = static_cast<float>((std::max)(skeleton->GetMaxHp(), 1));
            if (!hpBar.delayedHpInitialized)
            {
                hpBar.delayedHp = currentHp;
                hpBar.observedHp = static_cast<int>(currentHp);
                hpBar.delayedHpInitialized = true;
            }
            else if (currentHp < hpBar.observedHp)
            {
                // Keep the delayed fill monotonically decreasing during Rush hits.
                hpBar.delayedHp = (std::min)(hpBar.delayedHp, static_cast<float>(hpBar.observedHp));
                hpBar.delayedHpDelayTimer = 0.25f;
            }
            else if (currentHp > hpBar.observedHp)
            {
                // Match Grux: HP recovery synchronizes the delayed display immediately.
                hpBar.delayedHp = currentHp;
                hpBar.delayedHpDelayTimer = 0.0f;
            }
            hpBar.observedHp = static_cast<int>(currentHp);

            if (hpBar.delayedHp > currentHp)
            {
                const float uiDeltaTime = Time::UnscaledDeltaTime();
                if (hpBar.delayedHpDelayTimer > 0.0f)
                {
                    hpBar.delayedHpDelayTimer = (std::max)(0.0f,
                        hpBar.delayedHpDelayTimer - uiDeltaTime);
                }
                else
                {
                    hpBar.delayedHp = (std::max)(currentHp,
                        hpBar.delayedHp - 8.95f * uiDeltaTime);
                }
            }

            const float distanceScaleMultiplier = getHpDistanceScaleMultiplier(skeleton);
            const float finalScale = tutorialSkeletonHpBarScale * distanceScaleMultiplier;
            const DirectX::XMFLOAT2 scale = { finalScale, finalScale };
            if (hpBar.background)
            {
                hpBar.background->SetWorldPosition(position);
                hpBar.background->SetScale(scale);
                hpBar.background->SetVisible(true);
            }
            if (hpBar.delayedFill)
            {
                hpBar.delayedFill->SetWorldPosition(position);
                hpBar.delayedFill->SetScale(scale);
                hpBar.delayedFill->SetValue(hpBar.delayedHp, maximumHp);
                hpBar.delayedFill->SetVisible(true);
            }
            if (hpBar.currentFill)
            {
                hpBar.currentFill->SetWorldPosition(position);
                hpBar.currentFill->SetScale(scale);
                hpBar.currentFill->SetValue(currentHp, maximumHp);
                hpBar.currentFill->SetVisible(true);
            }
            if (hpBar.frame)
            {
                hpBar.frame->SetWorldPosition(position);
                hpBar.frame->SetScale(scale);
                hpBar.frame->SetVisible(true);
            }
        };

    updateHpBar(&owner, *this);
}

void SkeletonWarriorActor::CreateHpBarUI(UIManager* uiManager, const std::string& prefix)
{
    if (!uiManager || hpBarUI) return;
    hpBarUI = std::make_shared<SkeletonHpBarUI>();
    hpBarUI->Create(uiManager, prefix);
}

void SkeletonWarriorActor::UpdateHpBar(const DarkCameraActor& camera,
    const DirectX::XMFLOAT3& playerPosition)
{
    if (hpBarUI) hpBarUI->Update(*this, camera, playerPosition);
}

void SkeletonWarriorActor::HideHpBar()
{
    if (hpBarUI) hpBarUI->Hide();
}

void SkeletonWarriorActor::Finalize()
{
    hpBarUI.reset();
    Enemy::Finalize();
}

void SkeletonWarriorActor::Initialize(const Transform& transform)
{
    std::string parentName = "SkeletonWarriorMeshComponent";
    Enemy::Initialize(transform);
    skeletalMeshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    skeletalMeshComponent->SetModel("./Data/Models/Characters/Skeleton/Skeleton.gltf", false, true);
    ApplyRimLight();
    skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Enemy;   // オブジェクトの種類を Enemy に設定
    skeletalMeshComponent->SetRelativeLocationDirect({ 0.0f,-0.f,0.0f });

    // アニメーションコントローラーを作成
    int rootIndex = skeletalMeshComponent->FindIndexByName("root");
    auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootIndex);
    controller->AddAnimation("Walk", 0);
    controller->AddAnimation("Attack", 1);
    controller->AddAnimation("Idle", 2);
    controller->AddAnimation("Death", 3);
    controller->AddAnimation("Lean_Wall", 4);
    controller->AddAnimation("Stand_Idle", 5);
    // アニメーションコントローラーを character に追加
    this->AddBodyAnimationController(controller);
    // アニメーションコントローラーのオーナーの名前を設定する
    controller->SetOwnerName("Skeleton");
    // 全てのNotifyAssetsをロードする
    controller->LoadAllNotifyAssets("Skeleton");

    PlayBodyAnimation("Idle");


    int handRightSocketNode = skeletalMeshComponent->FindIndexByName("Hand_r_end");

    // 剣
    sword = AddComponent<SkeletalMeshComponent>("SwordMesh", parentName);
    sword->SetModel("./Data/Models/Weapons/Sword/Sword.gltf");
    sword->AttachToComponent(skeletalMeshComponent, handRightSocketNode); // "Hand_r_end"
    sword->SetRelativeLocationDirect({ -0.f, -0.1f, -0.0f });
    sword->SetRelativeEulerRotationDirect({ 0.0f, 90.f, 0.0f });
    sword->SetRelativeScaleDirect({ 1.37f,1.37f,1.37f });

    weaponRootPoint = AddComponent<SceneComponent>("SwordHitRoot", "SwordMesh");
    weaponRootPoint->SetRelativeLocationDirect(weaponRootOffset);
    weaponMiddlePoint = AddComponent<SceneComponent>("SwordHitMiddle", "SwordHitRoot");
    weaponMiddlePoint->SetRelativeLocationDirect({ 0.0f, 0.6f, 0.0f });
    weaponTipPoint = AddComponent<SceneComponent>("SwordHitTip", "SwordHitRoot");
    weaponTipPoint->SetRelativeLocationDirect({ 0.0f, 1.2f, 0.0f });

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
    rotationComponent->SyncCurrentRotation();

    cameraTargetComponent = AddComponent<SceneComponent>("cameraTargetComponent", parentName);
    cameraTargetComponent->SetRelativeLocationDirect({ 0.0f, 1.15f, 0.0f });
    hp = maxHp;

    deathSeAudioBuffer = CoreAudio::CoreAudioBuffer::GetResource(
        L"./Data/Sound/SE/skeleton_death.wav");
    PreCreateDeathBodyParts();

    // ポイントライトコンポーネントを追加
    auto pointLightComponent = this->AddComponent<PointLightComponent>("pointLightComponent", parentName);
    pointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f, 1.0f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    pointLightComponent->SetSharedLightName("PlayerPointLight");

    // HPバーを生成
    const auto uiManager = GetOwnerScene()->GetUIManager();
    const std::string actorName = GetName();
    CreateHpBarUI(uiManager, actorName);
}

void SkeletonWarriorActor::ApplyRimLight()
{
    if (skeletalMeshComponent)
    {
        skeletalMeshComponent->SetRuntimeMaterialRimLight(
            "M_Skeleton_01", rimLightColor, rimLightPower);
    }
}

void SkeletonWarriorActor::ApplyTutorialAttackHighlight(const float intensityMultiplier)
{
    if (!tutorialAttackHighlightEnabled)
        return;

    tutorialAttackHighlightActive = true;
    tutorialAttackHighlightMultiplier = (std::max)(0.0f, intensityMultiplier);
    if (skeletalMeshComponent)
    {
        skeletalMeshComponent->SetRuntimeMaterialRimLight("M_Skeleton_01", tutorialAttackBodyRimColor, tutorialAttackBodyRimIntensity * tutorialAttackHighlightMultiplier);
    }
    if (sword)
    {
        sword->SetRuntimeMaterialRimLight("M_Sword_01", tutorialAttackSwordRimColor, tutorialAttackSwordRimIntensity * tutorialAttackHighlightMultiplier);
    }
}

void SkeletonWarriorActor::ResetTutorialAttackHighlight()
{
    if (!tutorialAttackHighlightActive)
        return;

    tutorialAttackHighlightActive = false;
    tutorialAttackHighlightMultiplier = 1.0f;
    ApplyRimLight();
    if (sword)
        sword->ClearRuntimeMaterialRimLight("M_Sword_01");
}
bool SkeletonWarriorActor::PreCreateDeathBodyParts()
{
    ZoneScopedN("Skeleton BodyParts Precreate");
    if (!skeletalMeshComponent || !skeletalMeshComponent->model)
        return false;
    auto* actorManager = GetOwnerScene()->GetActorManager();
    if (!actorManager)
        return false;

    const DirectX::XMFLOAT4X4 meshWorld = skeletalMeshComponent->GetComponentWorldTransform().ToWorldTransform();
    const auto getBoneTransform = [this, &meshWorld](const char* boneName)
        {
            const DirectX::XMFLOAT4X4 matrix = skeletalMeshComponent->model->GetJointMatrix(
                boneName, skeletalMeshComponent->GetNodes(), meshWorld);
            return Transform(DirectX::XMLoadFloat4x4(&matrix));
        };
    const Transform transforms[] = {
        getBoneTransform("Head"), getBoneTransform("SpineHigh"), getBoneTransform("Root"),
        getBoneTransform("Shoulder_l"), getBoneTransform("Shoulder_r"),
        getBoneTransform("Thigh_l"), getBoneTransform("Thigh_r") };
    const SkeletonBodyPartActor::PartType types[] = {
        SkeletonBodyPartActor::PartType::Skull, SkeletonBodyPartActor::PartType::Ribs,
        SkeletonBodyPartActor::PartType::Spine, SkeletonBodyPartActor::PartType::Arm,
        SkeletonBodyPartActor::PartType::Arm, SkeletonBodyPartActor::PartType::Leg,
        SkeletonBodyPartActor::PartType::Leg };
    for (size_t i = 0; i < 7; ++i)
    {
        SkeletonBodyPartActor::SpawnParams params{};
        params.partType = types[i];
        params.lifeTime = bodyPartsLifetime;
        params.debugCollisionShape = bodyPartsDebug;
        params.linearDamping = bodyPartsLinearDamping;
        params.angularDamping = bodyPartsAngularDamping;
        params.sleepThreshold = bodyPartsSleepThreshold;
        const auto bodyPart = actorManager->CreateAndRegisterActorWithTransform<SkeletonBodyPartActor>(
            "SkeletonBodyPart", transforms[i], params);
        spawnedBodyParts.push_back(bodyPart);
    }
    return true;
}

bool SkeletonWarriorActor::ActivateDeathBodyParts()
{
    ZoneScopedN("Skeleton BodyParts Activate");
    if (!skeletalMeshComponent || !skeletalMeshComponent->model || spawnedBodyParts.size() != 7)
        return false;

    const DirectX::XMFLOAT4X4 meshWorld = skeletalMeshComponent->GetComponentWorldTransform().ToWorldTransform();
    const auto getBoneTransform = [this, &meshWorld](const char* boneName)
        {
            const DirectX::XMFLOAT4X4 matrix = skeletalMeshComponent->model->GetJointMatrix(
                boneName, skeletalMeshComponent->GetNodes(), meshWorld);
            return Transform(DirectX::XMLoadFloat4x4(&matrix));
        };
    const Transform transforms[] = {
        getBoneTransform("Head"), getBoneTransform("SpineHigh"), getBoneTransform("Root"),
        getBoneTransform("Shoulder_l"), getBoneTransform("Shoulder_r"),
        getBoneTransform("Thigh_l"), getBoneTransform("Thigh_r") };
    deathHeadBonePosition = transforms[0].GetLocation();
    deathSpineHighBonePosition = transforms[1].GetLocation();
    deathThighLeftBonePosition = transforms[5].GetLocation();
    hasDeathBodyPartBonePositions = true;

    DirectX::XMFLOAT3 awayFromPlayer = GetForward();
    if (const auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>())
    {
        awayFromPlayer = MathHelper::Subtract(GetPosition(), player->GetPosition());
        awayFromPlayer.y = 0.0f;
        awayFromPlayer = MathHelper::Length(awayFromPlayer) > 0.0001f
            ? MathHelper::Normalize(awayFromPlayer) : GetForward();
    }
    for (size_t i = 0; i < spawnedBodyParts.size(); ++i)
    {
        const auto bodyPart = spawnedBodyParts[i].lock();
        if (!bodyPart || bodyPart->IsPendingKill())
            return false;
        const float speed = bodyPartsInitialSpeed * MathHelper::RandomRange(0.85f, 1.15f);
        bodyPart->ActivateFromDeathPose(transforms[i], {
            awayFromPlayer.x * speed + MathHelper::RandomRange(-0.18f, 0.18f),
            bodyPartsUpwardSpeed + MathHelper::RandomRange(-0.10f, 0.16f),
            awayFromPlayer.z * speed + MathHelper::RandomRange(-0.18f, 0.18f) }, {
            MathHelper::RandomRange(-180.0f, 180.0f), MathHelper::RandomRange(-180.0f, 180.0f),
            MathHelper::RandomRange(-180.0f, 180.0f) });
    }
    return true;
}
void SkeletonWarriorActor::DestroySpawnedBodyParts()
{
    for (const auto& weakBodyPart : spawnedBodyParts)
    {
        if (const auto bodyPart = weakBodyPart.lock(); bodyPart && !bodyPart->IsPendingKill())
            bodyPart->MarkPendingKill();
    }

    spawnedBodyParts.clear();
}

// 剣を描画するか
void SkeletonWarriorActor::SetSwordVisible(const bool visible) const
{
    if (sword)
    {
        sword->SetIsVisible(visible);
    }
}

void SkeletonWarriorActor::SyncSpawnedBodyPartPhysicsSettings()
{
    std::erase_if(spawnedBodyParts, [this](const std::weak_ptr<SkeletonBodyPartActor>& weakBodyPart)
        {
            const auto bodyPart = weakBodyPart.lock();
            if (!bodyPart || bodyPart->IsPendingKill())
                return true;

            bodyPart->SetPhysicsDamping(bodyPartsLinearDamping,
                bodyPartsAngularDamping, bodyPartsSleepThreshold);
            return false;
        });
}
void SkeletonWarriorActor::DrawBodyPartBoneDebug() const
{
    if (!bodyPartsDebug || !hasDeathBodyPartBonePositions)
        return;

    DebugRender::DrawSphere(deathHeadBonePosition, 0.08f, { 0.2f, 0.8f, 1.0f, 1.0f }, 0.0f, true);
    DebugRender::DrawSphere(deathSpineHighBonePosition, 0.08f, { 0.3f, 1.0f, 0.3f, 1.0f }, 0.0f, true);
    DebugRender::DrawSphere(deathThighLeftBonePosition, 0.08f, { 1.0f, 0.75f, 0.2f, 1.0f }, 0.0f, true);
}
void SkeletonWarriorActor::Update(float elapsedTime)
{
    Enemy::Update(elapsedTime);
    SyncSpawnedBodyPartPhysicsSettings();
    DrawBodyPartBoneDebug();

    if (IsAnimationEditorPreviewActive())
        return;
    ResetAnimationEditorPreviewWeaponSweep();
    if (state == State::Dead)
        return;

    if (!tutorialPassive)
    {
        auto player = GetOwnerScene()->GetActorManager()->GetActorOfType<Player>();
        if (player && !player->IsPendingKill() && player->GetHp() > 0)
        {
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
                if (IsPlayerWithinAttackRange(*player))
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
    }

    DrawDangerAreaDebug();
}

void SkeletonWarriorActor::SetTutorialPassive(bool enabled)
{
    if (tutorialPassive == enabled)
        return;

    tutorialPassive = enabled;
    if (!tutorialPassive || state == State::Dead)
        return;

    ResetTutorialAttackHighlight();

    const bool wasInCombatState = state == State::Attacking || state == State::Recovery;
    state = State::Idle;
    stateElapsed = 0.0f;
    attackHitActive = false;
    isDangerWindow = false;
    activeDangerNotifyState = nullptr;
    hasHitPlayerThisAttack = false;
    hasJustDodgedPlayerThisAttack = false;
    ResetWeaponSweep();
    if (tutorialPassiveUseStandIdle)
        PlayTutorialPassiveStandIdle();
    else if (wasInCombatState)
        PlayBodyAnimation("Idle", true, true, 0.1f, true);
}

void SkeletonWarriorActor::SetTutorialAttackHighlightEnabled(const bool enabled)
{
    if (tutorialAttackHighlightEnabled == enabled)
        return;

    tutorialAttackHighlightEnabled = enabled;
    if (!tutorialAttackHighlightEnabled)
        ResetTutorialAttackHighlight();
}
void SkeletonWarriorActor::EnableTutorialPassiveStandIdle()
{
    tutorialPassiveUseStandIdle = true;
    if (!tutorialPassive || state == State::Dead)
        return;

    PlayTutorialPassiveStandIdle();
}

void SkeletonWarriorActor::PlayTutorialPassiveStandIdle()
{
    const auto controller = GetBodyAnimationController();
    if (!controller)
        return;

    controller->SetRemoveRootTranslationFromPose("Stand_Idle", true);
    PlayBodyAnimation("Stand_Idle", true, true, 0.1f, true);
}

void SkeletonWarriorActor::SetMaxHp(const int value)
{
    maxHp = (std::max)(1, value);
    hp = maxHp;
}

bool SkeletonWarriorActor::TakeDamageFromPlayer(int damage)
{
    if (state == State::Dead || damage <= 0)
        return false;

    hp = (std::max)(0, hp - damage);

    // コントローラー振動
    InputSystem::SetVibration(0.8f, 0.1f);
    // カメラシェイク
    if (auto camera = GetOwnerScene()->GetActorManager()->GetActorOfType<DarkCameraActor>())
    {
        std::string presetName = "BossRoar";
        camera->PlayCameraShakePreset(presetName);
    }

    CoreAudio::PlayOneShot("./Data/Sound/SE/enemy_damage.wav", 0.3f);


    if (hp > 0)
        return true;

    // Mark death before side effects so repeated damage cannot replay the SE.
    state = State::Dead;
    ResetTutorialAttackHighlight();
    CoreAudio::PlayOneShot("./Data/Sound/SE/skeleton_death.wav", 5.0f);
    const bool spawnedBodyParts = ActivateDeathBodyParts();
    attackHitActive = false;
    isDangerWindow = false;
    activeDangerNotifyState = nullptr;
    ResetWeaponSweep();
    if (spawnedBodyParts && skeletalMeshComponent)
        skeletalMeshComponent->SetIsVisible(false);

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
        EffectManager::EmitParticle(rushHitSparkEffectComponent->GetEffectHandle(), rushHitSparkEffectComponent->GetComponentLocation(), { 0.0f, 0.0f, 0.0f });
    }
    if (rushHitRingEffectComponent)
    {
        rushHitRingEffectComponent->SetWorldLocationDirect(effectPosition);
        rushHitRingEffectComponent->UpdateComponentToWorld();
        EffectManager::EmitParticle(rushHitRingEffectComponent->GetEffectHandle(), rushHitRingEffectComponent->GetComponentLocation(), rushHitRingEffectComponent->GetComponentEulerRotation());
    }
}

bool SkeletonWarriorActor::IsPlayerWithinAttackRange(const Player& player) const
{
    DirectX::XMFLOAT3 toPlayer = MathHelper::Subtract(player.GetPosition(), GetPosition());
    toPlayer.y = 0.0f;
    return MathHelper::Length(toPlayer) <= attackRange;
}

void SkeletonWarriorActor::BeginAttack(const DirectX::XMFLOAT3& directionToPlayer)
{
    if (tutorialPassive)
        return;
    if (rotationComponent)
        rotationComponent->SetDirection(directionToPlayer);

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

void SkeletonWarriorActor::UpdateAttack(float /*elapsedTime*/, Player& player)
{
    RefreshDangerAreaFromNotify();
    if (isDangerWindow)
    {
        UpdateDangerWindow(player);
    }

    if (attackHitActive)
        UpdateWeaponSweep(player);

    const auto controller = GetBodyAnimationController();
    const bool isAttackAnimation = controller && controller->GetCurrentAnimationName() == "Attack";
    const bool naturalAttackEnd = isAttackAnimation && !controller->IsPlayAnimation();
    const bool attackInterrupted = controller && !isAttackAnimation;
    if (naturalAttackEnd || attackInterrupted)
    {
        ResetTutorialAttackHighlight();
        state = State::Recovery;
        stateElapsed = 0.0f;
        attackHitActive = false;
        isDangerWindow = false;
        activeDangerNotifyState = nullptr;
        ResetWeaponSweep();

        if (naturalAttackEnd)
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
    const DirectX::XMFLOAT3 root = GetWeaponHitPoint(weaponRootPoint, activeWeaponHitOffset);
    const DirectX::XMFLOAT3 middle = GetWeaponHitPoint(weaponMiddlePoint, activeWeaponHitOffset);
    const DirectX::XMFLOAT3 tip = GetWeaponHitPoint(weaponTipPoint, activeWeaponHitOffset);
    if (!hasPreviousWeaponRoot)
    {
        previousWeaponRoot = root;
        previousWeaponMiddle = middle;
        previousWeaponTip = tip;
        hasPreviousWeaponRoot = true;
        return;
    }

    HitResultWithActor rootHit;
    HitResultWithActor middleHit;
    HitResultWithActor tipHit;
    const uint32_t playerMask = CollisionHelper::ToBit(CollisionLayer::Player);
    const bool rootSucceeded = CollisionFunction::SphereRayCast(
        previousWeaponRoot, root, rootHit, activeWeaponHitRadius, playerMask);
    const bool middleSucceeded = CollisionFunction::SphereRayCast(
        previousWeaponMiddle, middle, middleHit, activeWeaponHitRadius, playerMask);
    const bool tipSucceeded = CollisionFunction::SphereRayCast(
        previousWeaponTip, tip, tipHit, activeWeaponHitRadius, playerMask);

    if (weaponHitDebug)
        DrawWeaponHitDebug();

    previousWeaponRoot = root;
    previousWeaponMiddle = middle;
    previousWeaponTip = tip;

    if (hasHitPlayerThisAttack || hasJustDodgedPlayerThisAttack)
        return;

    const HitResultWithActor* hit = rootSucceeded ? &rootHit :
        middleSucceeded ? &middleHit : tipSucceeded ? &tipHit : nullptr;
    if (!hit || hit->actor != &player)
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
    if (!dangerAreaDebug || state != State::Attacking || !isDangerWindow)
        return;

    constexpr DirectX::XMFLOAT4 dangerColor{ 0.65f, 0.25f, 1.0f, 1.0f };
    DebugRender::DrawBox(dangerArea.WorldTransform(), dangerArea.size, dangerColor, 0.0f, true);
    DebugRender::DrawSphere(dangerArea.center, 0.08f, dangerColor, 0.0f, true);
}

void SkeletonWarriorActor::OnAnimationNotifyBegin(const AnimationNotifyState& notify)
{
    Enemy::OnAnimationNotifyBegin(notify);
    if (tutorialPassive || state != State::Attacking)
        return;

    switch (notify.type)
    {
    case AnimationNotifyState::Type::ShowEmissive:
        ApplyTutorialAttackHighlight(notify.value);
        break;
    case AnimationNotifyState::Type::HitBox:
        activeWeaponHitRadius = (std::max)(0.01f, notify.hitBoxRadius);
        activeWeaponHitOffset = notify.hitBoxOffset;
        attackHitActive = true;
        ResetWeaponSweep();
        break;
    case AnimationNotifyState::Type::DangerWindow:
        activeDangerNotifyState = &notify;
        isDangerWindow = true;
        RefreshDangerAreaFromNotify();
        break;
    default:
        break;
    }
}

void SkeletonWarriorActor::OnAnimationNotifyEnd(const AnimationNotifyState& notify)
{
    Enemy::OnAnimationNotifyEnd(notify);
    switch (notify.type)
    {
    case AnimationNotifyState::Type::ShowEmissive:
        ResetTutorialAttackHighlight();
        break;
    case AnimationNotifyState::Type::HitBox:
        attackHitActive = false;
        ResetWeaponSweep();
        break;
    case AnimationNotifyState::Type::DangerWindow:
        if (activeDangerNotifyState == &notify)
        {
            isDangerWindow = false;
            activeDangerNotifyState = nullptr;
        }
        break;
    default:
        break;
    }
}

void SkeletonWarriorActor::OnAnimationNotifyEvent(const AnimationNotifyEvent& event)
{
    HandleCommonAnimationNotifyEvent(event);

    if (state == State::Dead || IsPendingKill() ||
        event.type != AnimationNotifyEvent::Type::PlaySE || event.parameter.empty())
    {
        return;
    }

    const std::string audioPath = "./Data/Sound/SE/" + event.parameter + ".wav";
    CoreAudio::PlayOneShot(audioPath, event.value);
}
void SkeletonWarriorActor::OnAnimationChanged()
{
    ResetTutorialAttackHighlight();
}
void SkeletonWarriorActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    Character::DrawImGuiDetails();

    bool passive = tutorialPassive;
    if (ImGui::Checkbox("Tutorial Passive", &passive))
        SetTutorialPassive(passive);
    ImGui::TextDisabled("Passive disables only this Skeleton's AI attack and facing behavior.");

    ImGui::SeparatorText("Skeleton Rim Light");
    bool rimLightChanged = ImGui::ColorEdit3("Rim Color", &rimLightColor.x);
    rimLightChanged |= ImGui::DragFloat("Rim Power", &rimLightPower,
        0.01f, 0.0f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (rimLightChanged)
    {
        if (tutorialAttackHighlightActive)
            ApplyTutorialAttackHighlight(tutorialAttackHighlightMultiplier);
        else
            ApplyRimLight();
    }

    if (tutorialAttackHighlightEnabled)
    {
        ImGui::SeparatorText("Tutorial Attack Highlight");
        bool tutorialHighlightChanged = ImGui::ColorEdit3("Body Rim Color", &tutorialAttackBodyRimColor.x);
        tutorialHighlightChanged |= ImGui::DragFloat("Body Rim Intensity", &tutorialAttackBodyRimIntensity,
            0.01f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        tutorialHighlightChanged |= ImGui::ColorEdit3("Sword Rim Color", &tutorialAttackSwordRimColor.x);
        tutorialHighlightChanged |= ImGui::DragFloat("Sword Rim Intensity", &tutorialAttackSwordRimIntensity,
            0.01f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        if (tutorialHighlightChanged && tutorialAttackHighlightActive)
            ApplyTutorialAttackHighlight(tutorialAttackHighlightMultiplier);
    }

    ImGui::SeparatorText("Skeleton BodyParts Debug");
    ImGui::Checkbox("Skeleton BodyParts Debug", &bodyPartsDebug);
    ImGui::DragFloat("BodyParts Initial Speed", &bodyPartsInitialSpeed, 0.05f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("BodyParts Upward Speed", &bodyPartsUpwardSpeed, 0.05f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("BodyParts Lifetime", &bodyPartsLifetime, 0.05f, 0.0f, 15.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (bodyPartsDebug)
    {
        ImGui::DragFloat("Linear Damping", &bodyPartsLinearDamping, 0.05f, 0.0f, 20.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Angular Damping", &bodyPartsAngularDamping, 0.05f, 0.0f, 20.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Sleep Threshold", &bodyPartsSleepThreshold, 0.0005f, 0.0f, 1.0f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
    }
    ImGui::TextDisabled("Bone markers: Head=blue, SpineHigh=green, Thigh_l=orange.");

    ImGui::SeparatorText("Tutorial Skeleton Danger Area");
    ImGui::Checkbox("Danger Area Debug", &dangerAreaDebug);
    ImGui::Checkbox("Weapon Hit Debug", &weaponHitDebug);
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

    ImGui::DragFloat("attackRange", &attackRange, 0.1f, 0.0f, 7.0f);

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

void SkeletonWarriorActor::DrawAnimationEditorPreviewState(const AnimationNotifyState& state)
{
    constexpr DirectX::XMFLOAT4 dangerPreviewColor{ 0.65f, 0.25f, 1.0f, 1.0f };
    if (state.type == AnimationNotifyState::Type::DangerWindow)
    {
        const DangerArea previewArea = BuildDangerArea(
            GetPosition(), GetRight(), GetUp(), GetForward(),
            state.justDodgeAreaOffset, state.justDodgeAreaSize);
        DebugRender::DrawBox(previewArea.WorldTransform(), previewArea.size,
            dangerPreviewColor, 0.0f, true);
        DebugRender::DrawSphere(previewArea.center, 0.08f, dangerPreviewColor, 0.0f, true);
        return;
    }

    if (state.type != AnimationNotifyState::Type::HitBox ||
        !weaponRootPoint || !weaponMiddlePoint || !weaponTipPoint)
    {
        return;
    }

    constexpr DirectX::XMFLOAT4 previewColor{ 0.25f, 1.0f, 0.45f, 1.0f };
    const auto controller = GetBodyAnimationController();
    const bool drawSweep = controller && controller->IsEditorPreviewPlaying() &&
        controller->IsEditorPreviewHitBoxSweepVisible();
    const float currentTime = controller ? controller->GetCurrentSampledAnimationTime() : 0.0f;
    const float radius = (std::max)(0.01f, state.hitBoxRadius);
    const DirectX::XMFLOAT3 root = GetWeaponHitPoint(weaponRootPoint, state.hitBoxOffset);
    const DirectX::XMFLOAT3 middle = GetWeaponHitPoint(weaponMiddlePoint, state.hitBoxOffset);
    const DirectX::XMFLOAT3 tip = GetWeaponHitPoint(weaponTipPoint, state.hitBoxOffset);
    const bool contiguous = editorPreviewHasPreviousWeaponRoot &&
        editorPreviewWeaponHitBoxState == &state && currentTime >= editorPreviewWeaponHitBoxTime &&
        currentTime - editorPreviewWeaponHitBoxTime <= 0.1f;

    if (!drawSweep || !contiguous)
        ResetAnimationEditorPreviewWeaponSweep();
    else
    {
        DebugRender::DrawLine(editorPreviewPreviousWeaponRoot, root, previewColor, 0.0f, true);
        DebugRender::DrawLine(editorPreviewPreviousWeaponMiddle, middle, previewColor, 0.0f, true);
        DebugRender::DrawLine(editorPreviewPreviousWeaponTip, tip, previewColor, 0.0f, true);
        DebugRender::DrawSphere(editorPreviewPreviousWeaponRoot, radius, previewColor, 0.0f, true);
        DebugRender::DrawSphere(editorPreviewPreviousWeaponMiddle, radius, previewColor, 0.0f, true);
        DebugRender::DrawSphere(editorPreviewPreviousWeaponTip, radius, previewColor, 0.0f, true);
    }

    DebugRender::DrawSphere(root, radius, previewColor, 0.0f, true);
    DebugRender::DrawSphere(middle, radius, previewColor, 0.0f, true);
    DebugRender::DrawSphere(tip, radius, previewColor, 0.0f, true);
    DebugRender::DrawLine(root, middle, previewColor, 0.0f, true);
    DebugRender::DrawLine(middle, tip, previewColor, 0.0f, true);

    editorPreviewHasPreviousWeaponRoot = true;
    editorPreviewPreviousWeaponRoot = root;
    editorPreviewPreviousWeaponMiddle = middle;
    editorPreviewPreviousWeaponTip = tip;
    editorPreviewWeaponHitBoxState = &state;
    editorPreviewWeaponHitBoxTime = currentTime;
}

void SkeletonWarriorActor::DrawWeaponHitDebug() const
{
    if (!weaponHitDebug || !attackHitActive || !weaponRootPoint || !weaponMiddlePoint || !weaponTipPoint)
        return;

    constexpr DirectX::XMFLOAT4 hitColor{ 1.0f, 0.75f, 0.15f, 1.0f };
    const DirectX::XMFLOAT3 root = GetWeaponHitPoint(weaponRootPoint, activeWeaponHitOffset);
    const DirectX::XMFLOAT3 middle = GetWeaponHitPoint(weaponMiddlePoint, activeWeaponHitOffset);
    const DirectX::XMFLOAT3 tip = GetWeaponHitPoint(weaponTipPoint, activeWeaponHitOffset);
    DebugRender::DrawSphere(root, activeWeaponHitRadius, hitColor, 0.0f, true);
    DebugRender::DrawSphere(middle, activeWeaponHitRadius, hitColor, 0.0f, true);
    DebugRender::DrawSphere(tip, activeWeaponHitRadius, hitColor, 0.0f, true);
    DebugRender::DrawLine(root, middle, hitColor, 0.0f, true);
    DebugRender::DrawLine(middle, tip, hitColor, 0.0f, true);
    if (hasPreviousWeaponRoot)
    {
        DebugRender::DrawLine(previousWeaponRoot, root, hitColor, 0.0f, true);
        DebugRender::DrawLine(previousWeaponMiddle, middle, hitColor, 0.0f, true);
        DebugRender::DrawLine(previousWeaponTip, tip, hitColor, 0.0f, true);
    }
}

void SkeletonWarriorActor::ResetWeaponSweep()
{
    hasPreviousWeaponRoot = false;
}

void SkeletonWarriorActor::ResetAnimationEditorPreviewWeaponSweep()
{
    editorPreviewHasPreviousWeaponRoot = false;
    editorPreviewWeaponHitBoxState = nullptr;
    editorPreviewWeaponHitBoxTime = -1.0f;
}

DirectX::XMFLOAT3 SkeletonWarriorActor::GetWeaponHitPoint(
    const std::shared_ptr<SceneComponent>& point, const DirectX::XMFLOAT3& localOffset) const
{
    if (!point)
        return GetPosition();

    DirectX::XMVECTOR offset = DirectX::XMLoadFloat3(&localOffset);
    const DirectX::XMFLOAT4 rotation = point->GetComponentWorldTransform().GetRotation();
    offset = DirectX::XMVector3Rotate(offset, DirectX::XMLoadFloat4(&rotation));
    DirectX::XMFLOAT3 worldOffset{};
    DirectX::XMStoreFloat3(&worldOffset, offset);
    const DirectX::XMFLOAT3 base = point->GetComponentLocation();
    return { base.x + worldOffset.x, base.y + worldOffset.y, base.z + worldOffset.z };
}
