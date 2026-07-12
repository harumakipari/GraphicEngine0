#pragma once
#include "DarkStageAsset.h"
#include "Core/Actor.h"
#include "Components/Render/PointLightComponent.h"


class DarkTitleStage :public Actor
{
public:
    explicit DarkTitleStage(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    void DrawImGuiDetails() override;

    void SetModel(std::shared_ptr<StageAsset> stageAsset, std::shared_ptr<StageAsset> stageCandelabraAsset, std::shared_ptr<StageAsset> stageBrazierAsset, std::shared_ptr<StageAsset> stageGroundBrazierAsset, std::shared_ptr<StageAsset> stageMeltedWaxAsset, std::shared_ptr<StageAsset> stageStandingBrazierAsset, std::shared_ptr<StageAsset> stageCandleStandAsset);

private:
    std::string parentName = "RootComponent";

    // ボス部屋のライト
    std::vector<PointLightComponent*> bossRoomLightsLeft;
    bool bossRoomSequencePlaying = false;
    float bossRoomSequenceTime = 0.0f;
};