#include "pch.h"
#include "ShapeComponent.h"
#include "Core/Actor.h"

void ShapeComponent::Initialize()
{
    rigidBody_ = std::make_unique<SingleRigidBodyComponent>(name + "singleRigidBody", owner.lock(), this);
    rigidBody_->SetCollisionFilter(GetCollisionLayer(), GetCollisionMask());
    rigidBody_->Initialize(Physics::Instance().GetPhysics());
    rigidBody_->AddToScene(Physics::Instance().GetScene());
    //InitializePhysics();
}

// ƒ‚ƒfƒ‹‚©‚ç ConvexMesh ‚ğì¬‚·‚é
void ConvexCollisionComponent::CreateConvexMeshFromModel(MeshComponent* meshComponent)
{
    meshComponent_ = meshComponent;
    rigidBody_ = std::make_unique<MultiRigidBodyComponent>(name + "singleRigidBody", owner.lock(), meshComponent, this);
    rigidBody_->SetCollisionFilter(collisionLayer_, collisionMask_);
    rigidBody_->Initialize(Physics::Instance().GetPhysics());
    //rigidBody_->AddToScene(Physics::Instance().GetScene());       // Add Scene ‚ÍŠù‚É‚µ‚Ä‚ ‚é‚½‚ß
}

// ƒ‚ƒfƒ‹‚©‚ç TriangleMesh ‚ğì¬‚·‚é
void TriangleMeshCollisionComponent::CreateConvexMeshFromModel(MeshComponent* meshComponent)
{
    meshComponent_ = meshComponent;
    rigidBody_ = std::make_unique<TriangleMeshRigidBodyComponent>(name + "singleRigidBody", owner.lock(), meshComponent);
    uint32_t triangleMeshLayer = CollisionHelper::ToBit(CollisionLayer::WorldStatic);
    uint32_t triangleMeshMask = CollisionHelper::MakeMask({ CollisionLayer::Convex,CollisionLayer::Player,CollisionLayer::Enemy });
    //uint32_t triangleMeshLayer = static_cast<uint32_t>(ShapeComponent::CollisionLayer::WorldStatic);
    //uint32_t triangleMeshMask = static_cast<uint32_t>(0xFFFFFFFF);
    rigidBody_->SetCollisionFilter(triangleMeshLayer, triangleMeshMask);
    rigidBody_->Initialize(Physics::Instance().GetPhysics());

    rigidBody_->AddToScene(Physics::Instance().GetScene());


}
