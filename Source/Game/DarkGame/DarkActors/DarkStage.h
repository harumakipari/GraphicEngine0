#pragma once
#include <array>
#include "DarkStageAsset.h"
#include "Core/Actor.h"
#include "Components/Render/MeshComponent.h"
#include "Components/CollisionShape/ShapeComponent.h"
#include "Components/Render/PointLightComponent.h"

class ParticleComponent;

class DarkStage :public Actor
{
public:
    enum class StageArea
    {
        MainRoom,
        Transition,
        BossRoom,
    };

    explicit DarkStage(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    void DrawImGuiDetails() override;

    // ボスの部屋に入った時の処理
    void StartBossRoomLightSequence();

    void SetModel(std::shared_ptr<StageAsset> mainRoomAsset, std::shared_ptr<StageAsset> transitionAreaAsset, std::shared_ptr<StageAsset> bossRoomAsset, std::shared_ptr<StageAsset> stageCandelabraAsset, std::shared_ptr<StageAsset> stageBrazierAsset, std::shared_ptr<StageAsset> stageGroundBrazierAsset, std::shared_ptr<StageAsset> stageMeltedWaxAsset, std::shared_ptr<StageAsset> stageStandingBrazierAsset, std::shared_ptr<StageAsset> stageCandleStandAsset);

    void SetStageArea(StageArea area);
    StageArea GetStageArea() const { return currentStageArea; }
    
private:
    void ApplyStageVisibility();
    void ApplyStageLightEnable();
    void UpdateAutomaticStageState();
    bool IsStageAreaVisible(StageArea area) const;
    void RegisterStageLight(StageArea area, const std::shared_ptr<PointLightComponent>& light);
    void RegisterActorStageLights(StageArea area, const std::shared_ptr<Actor>& actor);

    std::string parentName = "RootComponent";
    std::shared_ptr<StaticMeshComponent> mainRoomMeshComponent;
    std::shared_ptr<StaticMeshComponent> transitionAreaMeshComponent;
    std::shared_ptr<StaticMeshComponent> bossRoomMeshComponent;
    StageArea currentStageArea = StageArea::MainRoom;
    std::array<std::vector<std::weak_ptr<PointLightComponent>>, 3> stageLightsByArea;
    bool isMoviePlaying = false;
    bool isBossRoomEntered = false;
    bool automaticStageStateInitialized = false;

    // ボス部屋のライト
    std::vector<PointLightComponent*> bossRoomLightsLeft;
    bool bossRoomSequencePlaying = false;
    float bossRoomSequenceTime = 0.0f;
};




