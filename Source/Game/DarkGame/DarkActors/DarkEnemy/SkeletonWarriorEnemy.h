#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Core/Actor.h"
#include "Game/Actors/Base/Character.h"


class SwordActor : public Actor
{
public:
    void Initialize(const Transform& transform) override
    {
    }
};

class ShieldActor : public Actor
{
public:
    void Initialize(const Transform& transform) override
    {
    }
};

class SkeletonWarriorActor :public Character
{
public:
    explicit SkeletonWarriorActor(const std::string& actorName) :Character(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<SkeletalMeshComponent> sword;
    std::shared_ptr<SkeletalMeshComponent> shield;
    std::shared_ptr<RotationComponent> rotationComponent;
    std::vector<DirectX::XMFLOAT3> waypoints;
    int currentWaypoint = 0;

    float moveSpeed = 2.0f;
};




