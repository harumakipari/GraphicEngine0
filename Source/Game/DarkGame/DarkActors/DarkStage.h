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
    void SetBossRoomWallCollisionEnabled(bool enabled);
    void UpdateBossRoomWallProbeSettings();
    void DrawBossRoomWallCollisionDebug() const;    bool IsStageAreaVisible(StageArea area) const;
    void RegisterStageLight(StageArea area, const std::shared_ptr<PointLightComponent>& light);
    void RegisterActorStageLights(StageArea area, const std::shared_ptr<Actor>& actor);

    std::string parentName = "RootComponent";
    std::shared_ptr<StaticMeshComponent> mainRoomMeshComponent;
    std::shared_ptr<StaticMeshComponent> transitionAreaMeshComponent;
    std::shared_ptr<StaticMeshComponent> bossRoomMeshComponent;
    std::shared_ptr<StaticMeshComponent> bossRoomCollisionModelComponent;
    std::vector<std::shared_ptr<BoxComponent>> bossRoomWallCollisionComponents;
    std::vector<DirectX::XMFLOAT3> bossRoomWallCollisionHalfExtents;
    StageArea currentStageArea = StageArea::MainRoom;
    std::array<std::vector<std::weak_ptr<PointLightComponent>>, 3> stageLightsByArea;
    bool isMoviePlaying = false;
    bool isBossRoomEntered = false;
    bool automaticStageStateInitialized = false;

    // ボス部屋のライト
    bool bossRoomWallCollisionEnabled = false;
    bool showBossRoomWallCollisionDebug = false;
    float bossRoomPlayerWallProbeRadiusScale = 1.0f;
    float bossRoomPlayerWallVisualClearance = 0.10f;
    float bossRoomGruxWallProbeRadiusScale = 0.5f;
    float bossRoomGruxWallVisualClearance = 0.15f;
    float activeBossRoomPlayerWallProbeRadius = 0.4f;
    float activeBossRoomGruxWallProbeRadius = 0.4f;
    std::vector<PointLightComponent*> bossRoomLightsLeft;
    bool bossRoomSequencePlaying = false;
    float bossRoomSequenceTime = 0.0f;
};




