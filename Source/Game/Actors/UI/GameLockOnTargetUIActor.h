#pragma once
#include "Core/Actor.h"

class UIImageComponent;
class DarkCameraActor;

class GameLockOnTargetUIActor : public Actor
{
public:
    explicit GameLockOnTargetUIActor(const std::string& name) : Actor(name) {}
    void Initialize(const Transform& transform) override;
    void Finalize() override;
    void HideMarker();
    void UpdateMarkerPresentation(const DarkCameraActor& camera, const std::shared_ptr<SceneComponent>& target,
        const DirectX::XMFLOAT3& worldPosition, bool isGrux, float deltaTime);
    void DrawImGuiDetails() override;

private:
    enum class LockOnTargetUIAnimationPhase : uint8_t
    {
        Hidden,
        Gathering,
        Hold,
        Rotating,
    };

    std::shared_ptr<UIImageComponent> lockOnTargetCenterImageComponent;
    std::shared_ptr<UIImageComponent> lockOnTargetTopImageComponent;
    std::shared_ptr<UIImageComponent> lockOnTargetBottomImageComponent;
    std::shared_ptr<UIImageComponent> lockOnTargetLeftImageComponent;
    std::shared_ptr<UIImageComponent> lockOnTargetRightImageComponent;
    std::weak_ptr<SceneComponent> lockOnTargetComponent;
    LockOnTargetUIAnimationPhase lockOnTargetUIAnimationPhase = LockOnTargetUIAnimationPhase::Hidden;
    float lockOnTargetUIAnimationElapsed = 0.0f;
    float lockOnTargetUIRotationDegree = 0.0f;
    float lockOnTargetUIStartOffset = 200.0f;
    float lockOnTargetUIGatherDuration = 0.3f;
    float lockOnTargetUIHoldDuration = 0.08f;
    float lockOnTargetUIRotationSpeedDegree = 72.0f;
    float lockOnTargetUISize = 150.0f;
    float skeletonLockOnTargetUIOverallScale = 0.15f;
    DirectX::XMFLOAT2 skeletonLockOnTargetUIOverallOffset = { 0.0f, 0.0f };
    float gruxLockOnTargetUIOverallScale = 0.15f;
    DirectX::XMFLOAT2 gruxLockOnTargetUIOverallOffset = { 0.0f, 70.5f };
    DirectX::XMFLOAT2 lockOnTargetUITopFinalOffset = { 0.0f, -97.0f };
    DirectX::XMFLOAT2 lockOnTargetUIBottomFinalOffset = { 0.0f, 96.5f };
    DirectX::XMFLOAT2 lockOnTargetUILeftFinalOffset = { -101.0f, 0.0f };
    DirectX::XMFLOAT2 lockOnTargetUIRightFinalOffset = { 102.5f, 0.0f };
};