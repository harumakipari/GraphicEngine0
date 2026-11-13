#include "pch.h"
#include "ShapeComponent.h"
#include "Core/Actor.h"

void ShapeComponent::Initialize()
{
    rigidBody_ = std::make_unique<SingleRigidBodyComponent>(name + "singleRigidBody", owner.lock(), this);
    rigidBody_->SetCollisionFilter(GetCollisionLayer(), GetCollisionMask());
    rigidBody_->Initialize(Physics::Instance().GetPhysics());
    rigidBody_->AddToScene(Physics::Instance().GetScene());
}

void ShapeComponent::Tick(const float deltaTime) 
{
    if (rigidBody_)
    {
        rigidBody_->Tick(deltaTime);
    }
}

// ƒ‚ƒfƒ‹‚©‚ç ConvexMesh ‚ğì¬‚·‚é
void ConvexCollisionComponent::CreateConvexMeshFromModel(MeshComponent* meshComponent)
{
    meshComponent_ = meshComponent;
    rigidBody_ = std::make_unique<MultiRigidBodyComponent>(name + "singleRigidBody", owner.lock(), meshComponent, this);
    rigidBody_->SetCollisionFilter(collisionLayer, collisionMask);
    rigidBody_->Initialize(Physics::Instance().GetPhysics());
}

// ƒ‚ƒfƒ‹‚©‚ç TriangleMesh ‚ğì¬‚·‚é
void TriangleMeshCollisionComponent::CreateConvexMeshFromModel(MeshComponent* meshComponent)
{
    meshComponent_ = meshComponent;
    rigidBody_ = std::make_unique<TriangleMeshRigidBodyComponent>(name + "singleRigidBody", owner.lock(), meshComponent);
    uint32_t triangleMeshLayer = CollisionHelper::ToBit(CollisionLayer::WorldStatic);
    uint32_t triangleMeshMask = CollisionHelper::MakeMask({ CollisionLayer::Convex,CollisionLayer::Player,CollisionLayer::Enemy });
    rigidBody_->SetCollisionFilter(triangleMeshLayer, triangleMeshMask);
    rigidBody_->Initialize(Physics::Instance().GetPhysics());
    rigidBody_->AddToScene(Physics::Instance().GetScene());
}
