#pragma once
#include "DarkStageAsset.h"
#include "Components/Easing/CoreEasingComponent.h"
#include "Core/Actor.h"
#include "Components/Render/PointLightComponent.h"

class DarkStageCandelabraActor :public Actor
{
    struct FlameSettings
    {
        float baseEmission = 200.0f;

        float flickerSpeed1 = 7.3f;
        float flickerSpeed2 = 13.7f;
        float flickerAmp1 = 0.05f;
        float flickerAmp2 = 0.02f;

        float baseScale = 0.006f;
        float scaleAmp = 0.0002f;
        float heightMultiplier = 3.1f;

        float posAmpX = 0.001f;
        float posAmpY = 0.00f;
        float posAmpZ = 0.001f;

        float colorBaseG = 0.3f;
        float colorAmpG = 0.15f;
    };
    FlameSettings flameSettings = {};
public:
    DarkStageCandelabraActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStageCandelabraActor() = default;
    void Initialize(const Transform& transform)override;
    void Update(float deltaTime) override;
    void SetModel(const std::shared_ptr<StageAsset>& stageAsset);
    void DrawImGuiDetails() override;
private:
    // 燭台のモデル
    std::shared_ptr<MeshComponent> candelabraMeshComponent;
    std::vector<MeshComponent*> flameComponents; // 炎のモデル
    std::vector<DirectX::XMFLOAT3> flameBasePositions;// 炎の初期位置
};

// 蝋燭台
class DarkStageCandleStandActor :public Actor
{
    struct FlameSettings
    {
        float baseEmission = 200.0f;

        float flickerSpeed1 = 7.3f;
        float flickerSpeed2 = 13.7f;
        float flickerAmp1 = 0.05f;
        float flickerAmp2 = 0.02f;

        float baseScale = 0.006f;
        float scaleAmp = 0.0002f;
        float heightMultiplier = 3.1f;

        float posAmpX = 0.001f;
        float posAmpY = 0.00f;
        float posAmpZ = 0.001f;

        float colorBaseG = 0.3f;
        float colorAmpG = 0.15f;
    };
    FlameSettings flameSettings = {};
public:
    DarkStageCandleStandActor(const std::string& actorName) :Actor(actorName) {}
    virtual ~DarkStageCandleStandActor() = default;
    void Initialize(const Transform& transform)override {}
    void SetModel(const std::shared_ptr<StageAsset>& stageAsset);
    void Update(float deltaTime) override;
    void DrawImGuiDetails() override {}

    // 炎の光のスケールを変更する関数
    void SetFireLightScale(DirectX::XMFLOAT3 fireScale);

    // 炎の光のスケールを戻す関数
    void ResetFireLightScale();
private:
    // スタンド式火鉢のモデル
    std::shared_ptr<SkeletalMeshComponent> meshComponent;
    std::vector<MeshComponent*> flameComponents; // 炎のモデル
    std::vector<DirectX::XMFLOAT3> flameBasePositions;// 炎の初期位置
    // シャンデリアの炎の球体のモデル
    std::vector <std::shared_ptr<InstanceMeshComponent>> sphereMeshComponents;
    DirectX::XMFLOAT3 initFireScale = { 0.01f,0.02f,0.01f };// 炎の初期スケール

    std::unique_ptr<EasingRunner> easingRunner;

    float elapsedTime = 0.0f;
    bool isSetScale = false;    // スケールを外から設定する
    DirectX::XMFLOAT3 fireScale = { 0.0f,0.0f,0.0f };
};
