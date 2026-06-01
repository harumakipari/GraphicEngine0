#pragma once
#include "Game/DarkGame/DarkActors/InteractableActor.h"



class DoorLargeActor : public InteractableActor
{
public:
    explicit DoorLargeActor(const std::string& actorName) :InteractableActor(actorName) {}

    void Initialize(const Transform& transform) override;
    void Update(float dt) override;

    void Interact() override;

    void DrawImGuiDetails() override;
private:

    std::shared_ptr<SceneComponent> root;

    std::shared_ptr<SceneComponent> leftHinge;
    std::shared_ptr<SceneComponent> rightHinge;

    std::shared_ptr<SkeletalMeshComponent> leftDoorMesh;
    std::shared_ptr<SkeletalMeshComponent> rightDoorMesh;

    float openAngle = 0.0f;
    float openSpeed = 60.0f;

    enum class DoorState
    {
        Closed,
        Opening,
        Open,
        Closing
    };

    DoorState doorState = DoorState::Closed;
};


class DoorSmallActor : public InteractableActor
{
public:
    explicit DoorSmallActor(const std::string& actorName) :InteractableActor(actorName) {}
    void Initialize(const Transform& transform) override;
    void Update(float dt) override;
    void Interact() override;
private:
    std::shared_ptr<SceneComponent> root;
    std::shared_ptr<SceneComponent> hinge;
    std::shared_ptr<SkeletalMeshComponent> doorMesh;
    float openAngle = 0.0f;
    float openSpeed = 60.0f;
    enum class DoorState
    {
        Closed,
        Opening,
        Open,
        Closing
    };
    DoorState doorState = DoorState::Closed;
};