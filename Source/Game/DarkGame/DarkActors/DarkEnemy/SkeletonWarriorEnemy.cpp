#include "pch.h"
#include "SkeletonWarriorEnemy.h"

#include "Components/Render/PointLightComponent.h"
#include "Engine/Scene/SceneBase.h"

void SkeletonWarriorActor::Initialize(const Transform& transform)
{
    std::string parentName = "SkeletonWarriorMeshComponent";
    Character::Initialize(transform);
    skeletalMeshComponent = AddComponent<SkeletalMeshComponent>(parentName);
    //skeletalMeshComponent->SetModel("./Data/Models/Characters/Skeleton/Skeleton.gltf");
    skeletalMeshComponent->SetModel("./Data/Models/Characters/Skeleton/SK_Skeleton_01.gltf",false,true);
    skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Enemy;   // オブジェクトの種類を Enemy に設定

    // アニメーションコントローラーを作成
    auto controller = std::make_shared<AnimationController>(skeletalMeshComponent.get());
    controller->AddAnimation("Walk", 0);
    controller->AddAnimation("Attack", 1);
    controller->AddAnimation("Idle", 2);
    //controller->AddAnimation("Block", 2);
    //controller->AddAnimation("Death", 3);
    //controller->AddAnimation("BlockIdle", 4);
    //controller->AddAnimation("Run", 5);
    //controller->AddAnimation("Walk", 6);
    // アニメーションコントローラーを character に追加
    this->AddBodyAnimationController(controller);

    PlayBodyAnimation("Walk");

    waypoints =
    {
      {-28.0f,0.0f,-7.3f},  //1
      {-28.0f,0.0f,11.3f}, //2
      {-44.0f,0.0f,11.3f},//3
      {-28.0f,0.0f,11.3f}, //2
        {-11.0f,0.0f,11.3f},//4
      {-28.0f,0.0f,11.3f}, //2
    };

    // 盾
    shield = AddComponent<SkeletalMeshComponent>("ShieldMesh", parentName);
    shield->SetModel("./Data/Models/Weapons/Shield/Shield.gltf");
    shield->AttachToComponent(skeletalMeshComponent, 11); // "Hand_l_end"

    // 剣
    sword = AddComponent<SkeletalMeshComponent>("SwordMesh", parentName);
    sword->SetModel("./Data/Models/Weapons/Sword/Sword.gltf");
    sword->AttachToComponent(skeletalMeshComponent, 16); // "Hand_r_end"
    sword->SetRelativeLocationDirect({ -0.f, -0.1f, -0.0f });
    sword->SetRelativeEulerRotationDirect({ 0.0f, 90.f, 0.0f });
    sword->SetRelativeScaleDirect({ 0.8f,0.8f,0.8f });

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
    Character::Update(elapsedTime);


    return;

    if (waypoints.empty()) return;

    DirectX::XMFLOAT3 position = GetPosition();

    DirectX::XMFLOAT3 target = waypoints[currentWaypoint];

    DirectX::XMFLOAT3 dir =
    {
        target.x - position.x,
        target.y - position.y,
        target.z - position.z
    };

    float length = sqrt(dir.x * dir.x + dir.z * dir.z);

    if (length < 0.3f)
    {
        currentWaypoint = (currentWaypoint + 1) % waypoints.size();
        return;
    }

    dir.x /= length;
    dir.z /= length;

    position.x += dir.x * moveSpeed * elapsedTime;
    position.z += dir.z * moveSpeed * elapsedTime;

    rotationComponent->SetDirection(dir);
    SetPosition(position);
}