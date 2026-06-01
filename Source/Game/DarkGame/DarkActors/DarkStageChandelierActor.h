#pragma once
#include "Core/Actor.h"
#include "Components/Render/PointLightComponent.h"

class DarkStageChandelierActor :public Actor
{
public:
    DarkStageChandelierActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStageChandelierActor() = default;
    void Initialize(const Transform& transform)override;
    void Update(float deltaTime) override;
private:
    // シャンデリアのモデル
    std::shared_ptr<SkeletalMeshComponent> chandelierMeshComponent;

    float swingTime = 0.0f;
    float swingSpeed = 1.5f;     // 揺れる速さ
    float swingAngle = 10.0f;    // 最大角度（度）
};

class DarkStageFireBowlActor:public Actor
{
public:
    DarkStageFireBowlActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStageFireBowlActor() = default;
    void Initialize(const Transform& transform)override;
private:
    // モデル
    std::shared_ptr<SkeletalMeshComponent> meshComponent;
};

class DarkStageTorchSconceActor:public Actor
{
public:
    DarkStageTorchSconceActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStageTorchSconceActor() = default;
    void Initialize(const Transform& transform)override;
private:
    // モデル
    std::shared_ptr<SkeletalMeshComponent> meshComponent;
};


// 絵画
class DarkStagePaintingActor :public Actor
{
public:
    DarkStagePaintingActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStagePaintingActor() = default;
    void Initialize(const Transform& transform)override;
private:
    // モデル
    std::shared_ptr<SkeletalMeshComponent> meshComponent;
};
