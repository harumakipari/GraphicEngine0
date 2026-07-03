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

    float openAlpha = 0.0f;  // 0=閉 1=開
    float openTime = 1.5f; // 1秒でドアが開く
    float closeTime = 1.8f; // 1秒でドアが閉じる
    float openedAngleLeft = -90.0f;  // 開いた角度
    float openedAngleRight = -90.0f;  // 開いた角度
    float closedAngleLeft = -180.0f;   // 閉じた角度
    float closedAngleRight = 0.0f;   // 閉じた角度

    enum class DoorState :uint8_t
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
    void DrawImGuiDetails() override;
private:
    std::shared_ptr<SceneComponent> root;
    std::shared_ptr<SceneComponent> hinge;
    std::shared_ptr<SkeletalMeshComponent> doorMesh;

    float openAlpha = 0.0f;  // 0=閉 1=開
    float openTime = 1.5f; // 1秒でドアが開く
    float closeTime = 1.8f; // 1秒でドアが閉じる
    float openedAngle = -90.0f;  // 開いた角度
    float closedAngle = -180.0f;   // 閉じた角度

    enum class DoorState :uint8_t
    {
        Closed,
        Opening,
        Open,
        Closing
    };
    DoorState doorState = DoorState::Closed;
};

class DoorJailActor :public InteractableActor
{
public:
    explicit DoorJailActor(const std::string& actorName) :InteractableActor(actorName) {}
    void Initialize(const Transform& transform) override;
    void Update(float deltaTime) override;
    void Interact() override;
    void DrawImGuiDetails() override;

private:
    std::shared_ptr<SceneComponent> root;
    std::shared_ptr<SceneComponent> hinge;
    std::shared_ptr<SkeletalMeshComponent> doorMesh;

    float openAlpha = 0.0f;  // 0=閉 1=開
    float openTime = 1.5f; // 1秒でドアが開く
    float closeTime = 1.2f; // 1秒でドアが閉じる
    float openedAngle = 90.0f;  // 開いた角度
    float closedAngle = 180.0f;   // 閉じた角度

    enum class DoorState:uint8_t
    {
        Closed,
        Opening,
        Open,
        Closing
    };
    DoorState doorState = DoorState::Closed;
};
