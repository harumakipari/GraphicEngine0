#include "pch.h"
#include "CollisionComponent.h"


#include "Physics/CollisionSystem.h"
#include "Core/Actor.h"

void CollisionComponent::OnRegister()
{
    std::shared_ptr<CollisionComponent> sharedThis =
        std::static_pointer_cast<CollisionComponent>(shared_from_this());
    CollisionSystem::RegisterCollisionComponent(sharedThis);
}


// è’ìÀÉCÉxÉìÉg
void CollisionComponent::OnHit(CollisionComponent* self, CollisionComponent* other)
{
    if (onHitCallback_)
    {
        onHitCallback_(self, other);
    }
}
