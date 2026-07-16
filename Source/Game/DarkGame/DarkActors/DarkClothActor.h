#pragma once
#include "Core/Actor.h"
#include "Game/Actors/Stage/ClothSimulate.h"

class DarkClothActor :public Actor
{
public:
    explicit DarkClothActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void DrawImGuiDetails() override;

    // 布を描画する
    void RenderCloth(ID3D11DeviceContext* immediateContext);

private:
    std::string parentName = "RootComponent";
    std::shared_ptr<SkeletalMeshComponent> meshComponent;  // ポールモデル
    std::shared_ptr<SceneComponent> clothPoint; // 布の位置
    std::unique_ptr<ClothSimulate> clothSimulate;  // クロスシミュレーション
};
