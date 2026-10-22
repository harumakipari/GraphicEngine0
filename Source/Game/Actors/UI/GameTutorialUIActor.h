#pragma once
#include "Core/Actor.h"

class UIImageComponent;
class StaticMeshComponent;
class DarkCameraActor;
class SkeletonWarriorActor;

class GameTutorialUIActor : public Actor
{
public:
    explicit GameTutorialUIActor(const std::string& name) : Actor(name) {}
    void Initialize(const Transform& transform) override;
    void Finalize() override;
    void DrawImGuiDetails()override;


    void UpdateExplanationLayout();
    void SetExplanationVisible(bool visible);
    void HidePrompts();
    void UpdatePrompts(const DarkCameraActor& camera, const std::shared_ptr<SkeletonWarriorActor>& passive,
        const std::shared_ptr<SkeletonWarriorActor>& dodge, bool showLt, bool showY, bool showX);
    void HideRing();
    void UpdateRing(const DirectX::XMFLOAT3& position, float deltaTime);

private:
    std::shared_ptr<UIImageComponent> ltPrompt, yPrompt, xPrompt;
    std::shared_ptr<UIImageComponent> explanation, closeImage;
    std::shared_ptr<StaticMeshComponent> ringMesh;
    DirectX::XMFLOAT2 explanationPosition{ 620.0f, 280.0f };
    DirectX::XMFLOAT2 explanationScale{ 0.40f, 0.40f };
    DirectX::XMFLOAT2 closePosition{ 973.0f, 325.0f };
    DirectX::XMFLOAT2 closeScale{ 0.44f, 0.44f };
    DirectX::XMFLOAT2 promptOffset{ -1.0f, -63.0f };
    float promptScale = 0.36f;
    float ringPulseElapsed = 0.0f;
    DirectX::XMFLOAT3 ringPositionOffset{ 0.0f, 0.39f, 0.13f };
    DirectX::XMFLOAT3 ringScale{ 6.04f, 1.0f, 2.95f };
    DirectX::XMFLOAT3 ringColor{ 1.0f, 0.78f, 0.42f };
    float ringEmission = 7.8f;
    float ringPulseSpeed = 0.8f;
    float ringPulseMin = 0.11f, ringPulseMax = 1.00f;
};
