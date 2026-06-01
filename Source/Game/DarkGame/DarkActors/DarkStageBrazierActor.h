#pragma once
#include "DarkStageAsset.h"
#include "Core/Actor.h"

class DarkStageBrazierActor :public Actor
{
public:
    DarkStageBrazierActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStageBrazierActor() = default;
    void Initialize(const Transform& transform)override {}
    void SetModel(const std::shared_ptr<StageAsset>& stageAsset);

private:
    // 火鉢のモデル
    std::shared_ptr<SkeletalMeshComponent> brazierMeshComponent;
};

// 溶けた蝋のモデル
class DarkStageMeltedWaxActor :public Actor
{
public:
    DarkStageMeltedWaxActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStageMeltedWaxActor() = default;
    void Initialize(const Transform& transform)override {}
    void SetModel(const std::shared_ptr<StageAsset>& stageAsset);

private:
    // 溶けた蝋のモデル
    std::shared_ptr<SkeletalMeshComponent> metedWaxMeshComponent;
};

// スタンド式火鉢のモデル
class DarkStageStandingBrazierActor :public Actor
{
public:
    DarkStageStandingBrazierActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStageStandingBrazierActor() = default;
    void Initialize(const Transform& transform)override {}
    void SetModel(const std::shared_ptr<StageAsset>& stageAsset);

private:
    // スタンド式火鉢のモデル
    std::shared_ptr<SkeletalMeshComponent> standingBrazierMeshComponent;
};

