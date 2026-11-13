#include "pch.h"
#include "ShapeComponent.h"
#include "Core/Actor.h"

void ShapeComponent::Initialize()
{
    rigidBody = std::make_unique<SingleRigidBodyComponent>(name + "singleRigidBody", owner.lock(), this);
    rigidBody->SetCollisionFilter(GetCollisionLayer(), GetCollisionMask());
    rigidBody->Initialize(Physics::Instance().GetPhysics());
    rigidBody->AddToScene(Physics::Instance().GetScene());
}

void ShapeComponent::Tick(const float deltaTime) 
{
    if (rigidBody)
    {
        rigidBody->Tick(deltaTime);
    }
}

void ShapeComponent::SetKinematic(const bool isKinematic)
{
    physicsMode = isKinematic ? PhysicsControlMode::Kinematic : PhysicsControlMode::Dynamic;
    if (rigidBody)
    {
        rigidBody->SetKinematic(isKinematic);
    }
}

// OnTrigger か OnContact どっちに入るか設定する
void ShapeComponent::SetTrigger(const bool isTrigger)
{
    if (rigidBody)
    {
        rigidBody->SetTrigger(isTrigger);
    }
}

void ShapeComponent::SetGravity(const bool isUseGravity) const
{
    if (rigidBody)
    {
        rigidBody->SetGravity(isUseGravity);
    }
}

void ShapeComponent::SetLinearDamping(const float damping) const
{
    if (rigidBody) rigidBody->SetLinearDamping(damping);
}

void ShapeComponent::SetAngularDamping(const float damping) const
{
    if (rigidBody) rigidBody->SetAngularDamping(damping);
}

void ShapeComponent::SetSleepThreshold(const float threshold) const
{
    if (rigidBody) rigidBody->SetSleepThreshold(threshold);
}

// 当たった時に衝撃を与える
void ShapeComponent::AddImpulse(const DirectX::XMFLOAT3& impulse)
{
    if (rigidBody)
    {
        rigidBody->AddImpulse(impulse);
    }
}

void ShapeComponent::SetStatic(const bool isStatic)
{
    this->isStatic = isStatic;
    if (isStatic)
    {
        physicsMode = PhysicsControlMode::Kinematic;
        mass = 0.0f;
        if (rigidBody)
            rigidBody->SetKinematic(true);
    }
}

// 当たり判定を無効にする
void ShapeComponent::DisableCollision() 
{
    isCollide = false;
    if (rigidBody)
    {
        rigidBody->DisableCollision();
    }
}

// 初速を設定する
void ShapeComponent::SetInitialVelocity(const DirectX::XMFLOAT3& velocity) const
{
    if (rigidBody)
    {
        rigidBody->SetInitialVelocity(velocity);
    }
}

void ShapeComponent::SetInitialAngularVelocity(const DirectX::XMFLOAT3& angularVelocity) const
{
    if (rigidBody)
    {
        rigidBody->SetInitialAngularVelocity(angularVelocity);
    }
}

void ShapeComponent::SetPhysicsWorldTransform(const Transform& worldTransform) const
{
    if (rigidBody)
    {
        rigidBody->SetTransform(worldTransform);
    }
}

// 当たり判定レイヤーを途中で追加する
void ShapeComponent::AddCollisionFilter(const CollisionLayer otherLayer, const CollisionResponse response)
{
    SetResponseToLayer(otherLayer, response);
    if (rigidBody)
    {
        rigidBody->SetCollisionFilter(GetCollisionLayer(), GetCollisionMask());
    }
};

// 当たり判定を有効にする
void ShapeComponent::EnableCollision() 
{
    isCollide = true;
    if (rigidBody)
    {
        rigidBody->EnableCollision();
    }
};

void ShapeComponent::OnUnregister()
{
    if (rigidBody)
    {
        rigidBody->Destroy();
        rigidBody = nullptr;
    }
}

void ShapeComponent::SetPhysicsMaterial(const PhysicsMaterialType type)
{
    materialType = type;
    if (rigidBody)
    {
        rigidBody->SetMaterial(type);
    }
}


void ShapeComponent::DrawImGuiInspector() 
{
#ifdef USE_IMGUI
    SceneComponent::DrawImGuiInspector();
    if (ImGui::TreeNode((name + "  debugRender").c_str()))
    {
        ImGui::Checkbox("isVisibleDebugShape", &isVisibleDebugShape);
        ImGui::TreePop();
    }
#endif
}


ShapeComponent::PhysicsShapeInfo BoxComponent::GetPhysicsShapeInfo()const 
{
    PhysicsShapeInfo info;
    info.geometry = physx::PxBoxGeometry(boxExtent.x, boxExtent.y, boxExtent.z);
    const Transform& worldT = GetComponentWorldTransform();
    DirectX::XMFLOAT3 pos = worldT.GetLocation();
    DirectX::XMFLOAT4 rot = worldT.GetRotation();

    DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rot);
    q = DirectX::XMQuaternionNormalize(q);
    DirectX::XMStoreFloat4(&rot, q);

    info.transform = physx::PxTransform{
        physx::PxVec3(pos.x,pos.y,pos.z),
        physx::PxQuat(rot.x,rot.y,rot.z,rot.w)
    };

    return info;
}

AABB BoxComponent::GetAABB()const 
{
    AABB aabb{};
    DirectX::XMFLOAT3 center = GetComponentLocation();
    aabb.min = { center.x - boxExtent.x,center.y - boxExtent.y,center.z - boxExtent.z };
    aabb.max = { center.x + boxExtent.x,center.y + boxExtent.y,center.z + boxExtent.z };
    return aabb;
}

void BoxComponent::DrawImGuiInspector() 
{
#ifdef USE_IMGUI
    ShapeComponent::DrawImGuiInspector();
    if (ImGui::TreeNode((name + "  box").c_str()))
    {
        ImGui::DragFloat3("boxExtent", &boxExtent.x, 0.1f);
        ResizeBox(boxExtent.x, boxExtent.y, boxExtent.z);
        ImGui::TreePop();
    }
#endif
}

void BoxComponent::ResizeBox(const float newExtentX, const float newExtentY, const float newExtentZ)
{
    rigidBody->ResizeBox(newExtentX, newExtentY, newExtentZ);
    boxExtent.x = newExtentX;
    boxExtent.y = newExtentY;
    boxExtent.z = newExtentZ;
}

void SphereComponent::DrawImGuiInspector() 
{
#ifdef USE_IMGUI

    ShapeComponent::DrawImGuiInspector();
    if (ImGui::TreeNode((name + "  sphere").c_str()))
    {
        ImGui::DragFloat("radius", &radius, 0.1f);
        ImGui::TreePop();
    }
#endif
}

ShapeComponent::PhysicsShapeInfo SphereComponent::GetPhysicsShapeInfo()const
{
    PhysicsShapeInfo info;
    info.geometry = physx::PxSphereGeometry(radius);
    DirectX::XMFLOAT3 pos = GetComponentWorldTransform().GetLocation();
    info.transform = physx::PxTransform{ {pos.x,pos.y,pos.z} };
    return info;
}

Sphere SphereComponent::ToSphere()const
{
    Sphere s{};
    s.c = Point(GetComponentWorldTransform().GetLocation());
    s.r = radius;
    return s;
}


void CapsuleComponent::DrawImGuiInspector() 
{
#ifdef USE_IMGUI
    ShapeComponent::DrawImGuiInspector();

    if (ImGui::TreeNode((name + "  capsule").c_str()))
    {
        ImGui::DragFloat("radius", &radius, 0.1f);
        ImGui::DragFloat("height", &height, 0.1f);
        ResizeCapsule(radius, height);
        ImGui::TreePop();
    }
#endif
}

AABB CapsuleComponent::GetAABB() const 
{
    auto pos = GetComponentWorldTransform().GetLocation();
    DirectX::XMFLOAT3 a = { pos.x, pos.y + height , pos.z };
    DirectX::XMFLOAT3 b = { pos.x, pos.y + radius, pos.z };

    AABB box;
    box.min.x = std::min<float>(a.x, b.x) - radius;
    box.min.y = std::min<float>(a.y, b.y) - radius;
    box.min.z = std::min<float>(a.z, b.z) - radius;

    box.max.x = std::max<float>(a.x, b.x) + radius;
    box.max.y = std::max<float>(a.y, b.y) + radius;
    box.max.z = std::max<float>(a.z, b.z) + radius;

    return box;
}


ShapeComponent::PhysicsShapeInfo CapsuleComponent::GetPhysicsShapeInfo()const 
{
    PhysicsShapeInfo info;
    // 見た目の全長（頭～足）
    float totalHeight = height;

    // PhysX 用 halfHeight（球を除いた円柱部分）
    float halfHeight = (totalHeight * 0.5f) - radius;
    halfHeight = std::max<float>(halfHeight, 0.0f);

    info.geometry = physx::PxCapsuleGeometry(radius, halfHeight);
    const Transform& worldT = GetComponentWorldTransform();
    DirectX::XMFLOAT3 pos = worldT.GetLocation();
    DirectX::XMFLOAT4 rot = worldT.GetRotation();

    DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rot);
    q = DirectX::XMQuaternionNormalize(q);
    DirectX::XMStoreFloat4(&rot, q);

    // World 回転
    physx::PxQuat worldQ(rot.x, rot.y, rot.z, rot.w);
    physx::PxQuat capsuleAxisQ = { physx::PxPiDivTwo, physx::PxVec3(0.0f, 0.0f, 1.0f) };
    // Capsule を X → Y に立てる回転
    switch (capsuleAxis)
    {
    case CapsuleAxis::x:
        capsuleAxisQ = { physx::PxPiDivTwo, physx::PxVec3(1.0f, 0.0f, 0.0f) };
        break;
    case CapsuleAxis::y:
        capsuleAxisQ = { physx::PxPiDivTwo, physx::PxVec3(0.0f, 0.0f, 1.0f) };
        break;
    case CapsuleAxis::z:
        capsuleAxisQ = { physx::PxPiDivTwo, physx::PxVec3(0.0f, 1.0f, 0.0f) };
        break;
    };

    info.transform = physx::PxTransform(
        physx::PxVec3(pos.x, pos.y, pos.z),
        worldQ * capsuleAxisQ
    );
    return info;
}

Capsule CapsuleComponent::ToCapsule()const
{
    Capsule c{};
    c.a = Point(GetComponentWorldTransform().GetLocation().x, GetComponentWorldTransform().GetLocation().y + height - radius, GetComponentWorldTransform().GetLocation().z);
    c.b = DirectX::XMFLOAT3(GetComponentWorldTransform().GetLocation().x, GetComponentWorldTransform().GetLocation().y + radius, GetComponentWorldTransform().GetLocation().z);
    c.r = radius;
    return c;
}


// モデルから ConvexMesh を作成する
void ConvexCollisionComponent::CreateConvexMeshFromModel(MeshComponent* meshComponent)
{
    meshComponent = meshComponent;
    rigidBody = std::make_unique<MultiRigidBodyComponent>(name + "singleRigidBody", owner.lock(), meshComponent, this);
    rigidBody->SetCollisionFilter(collisionLayer, collisionMask);
    rigidBody->Initialize(Physics::Instance().GetPhysics());
}

// モデルから TriangleMesh を作成する
void TriangleMeshCollisionComponent::CreateConvexMeshFromModel(MeshComponent* meshComponent)
{
    meshComponent = meshComponent;
    rigidBody = std::make_unique<TriangleMeshRigidBodyComponent>(name + "singleRigidBody", owner.lock(), meshComponent);
    uint32_t triangleMeshLayer = CollisionHelper::ToBit(CollisionLayer::WorldStatic);
    uint32_t triangleMeshMask = CollisionHelper::MakeMask({ CollisionLayer::Convex,CollisionLayer::Player,CollisionLayer::Enemy });
    rigidBody->SetCollisionFilter(triangleMeshLayer, triangleMeshMask);
    rigidBody->Initialize(Physics::Instance().GetPhysics());
    rigidBody->AddToScene(Physics::Instance().GetScene());
}
