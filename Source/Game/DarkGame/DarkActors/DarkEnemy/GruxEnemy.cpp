#include "pch.h"

#include "GruxEnemy.h"

#include "Components/Render/PointLightComponent.h"
#include "Engine/Scene/SceneBase.h"
#include "Game/Actors/Player/Player.h"

void GruxEnemy::Initialize(const Transform& transform)
{
    int maxHp = 100;
    hp = maxHp;

    std::string parentName = "SkeletonWarriorMeshComponent";
    Character::Initialize(transform);
    skeletalMeshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    skeletalMeshComponent->SetModel("./Data/Models/Characters/GruxQilin/animations.gltf", false, true);
    //skeletalMeshComponent->SetModel("./Data/Models/Characters/Grux/animations.gltf", false, true);
    //skeletalMeshComponent->SetModel("./Data/Models/Characters/StoneGolem/StoneGolem.gltf", false, true);
    skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Enemy;   // オブジェクトの種類を Enemy に設定
    skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 6.6f;   // 目玉の自己発光の強さを設定
    skeletalMeshComponent->plusAlphaCBuffer->data.cpuColor = { 0.9f,0.08f,0.08f,1.0f };   // 目玉の色を赤にしてみる
    for (auto& material : skeletalMeshComponent->model->materials)
    {
        if (material.name == "MI_Grux_Eye")
        {// 目だったら、
            material.materialType = MaterialType::Eye;
        }
    }

    // アニメーションコントローラーを作成
    auto controller = std::make_shared<AnimationController>(skeletalMeshComponent.get());
    controller->AddAnimation("Idle", 0);
    controller->AddAnimation("PrimaryAttack_RA", 1);
    controller->AddAnimation("PrimaryAttack_LA", 2);
    controller->AddAnimation("PrimaryAttack_LB", 3);
    controller->AddAnimation("PrimaryAttack_RB", 4);
    controller->AddAnimation("Respawn", 5);
    controller->AddAnimation("LaunchPad", 6);
    controller->AddAnimation("LevelStart", 7);
    controller->AddAnimation("Death", 8);

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

void GruxEnemy::Update(float deltaTime)
{
    Character::Update(deltaTime);

    return;
    stateTimer += deltaTime;

    auto player = GetOwnerScene()->GetActorManager()->GetActorByName("player");
    DirectX::XMFLOAT3 playerPos = player->GetPosition();
    switch (state)
    {
    case BossState::Idle:
    {
        // プレイヤーとの距離を見る
        if (GetDistanceToPlayer() < 5.0f)
        {
            state = BossState::Attack;
            stateTimer = 0.0f;
            attackPlayed = false;
        }
        break;
    }

    case BossState::Attack:
    {
        if (!attackPlayed)
        {
            PlayBodyAnimation("PrimaryAttack_RA", false, true, 0.1f);
            attackPlayed = true;
            damageDone = false;
            stateTimer = 0.0f;
        }

        // ここで遅れて当たる
        if (!damageDone && stateTimer > attackHitTime)
        {
            DoAttackHit(); // ←さっきの距離判定関数
            damageDone = true;
        }

        if (stateTimer > 1.2f)
        {
            state = BossState::Cooldown;
            stateTimer = 0.0f;
        }
        break;
    }

    case BossState::Cooldown:
    {
        if (stateTimer > 1.5f)
        {
            state = BossState::Idle;
            stateTimer = 0.0f;
        }
        break;
    }
    }
}

//当たった時の処理
void GruxEnemy::TakeDamage(int damage)
{
    hp -= damage;
    Logger::Log(U8("エネミーにダメージ！ HP:") + std::to_string(hp));
}

// 攻撃が当たるタイミングで呼ばれる関数
void GruxEnemy::DoAttackHit()
{
    auto playerActor = GetOwnerScene()->GetActorManager()->GetActorByName("player");
    auto player = std::dynamic_pointer_cast<Player>(playerActor);
    if (!player)
    {// プレイヤーがいない場合は攻撃しない
        return;
    }
    DirectX::XMFLOAT3 bossPos = GetPosition();
    DirectX::XMFLOAT3 playerPos = player->GetPosition();

    // ▼プレイヤーへの方向ベクトル
    float dx = playerPos.x - bossPos.x;
    float dz = playerPos.z - bossPos.z;

    float distSq = dx * dx + dz * dz;
    float attackRange = 3.0f;

    if (distSq > attackRange * attackRange) return;

    // 正規化
    float len = sqrtf(dx * dx + dz * dz);
    dx /= len;
    dz /= len;

    // ボスの前方向（Z+方向）
    DirectX::XMFLOAT3 forward = GetForward();

    float dot = dx * forward.x + dz * forward.z;

    float angleCos = cosf(DirectX::XMConvertToRadians(60.0f)); // 60度

    if (dot > angleCos)
    {
        player->TakeDamage(10);
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

void KnightActor::Initialize(const Transform& transform)
{
    std::string parentName = "SkeletonWarriorMeshComponent";
    Character::Initialize(transform);
    skeletalMeshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    skeletalMeshComponent->SetModel("./Data/Models/Characters/Greystone/Idle.gltf", false, true);

    // アニメーションコントローラーを作成
    auto controller = std::make_shared<AnimationController>(skeletalMeshComponent.get());
    controller->AddAnimation("Idle", 0);

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
    auto controller = std::make_shared<AnimationController>(skeletalMeshComponent.get());
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
    auto controller = std::make_shared<AnimationController>(skeletalMeshComponent.get());
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