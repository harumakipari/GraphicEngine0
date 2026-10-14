#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Game/Actors/Base/Character.h"
#include "Graphics/Renderer/TrailRenderer.h"

class TitlePlayer :public Character
{
public:
    explicit TitlePlayer(const std::string& modelName) :Character(modelName){}

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void DrawImGuiDetails()override;

    void Finalize()override {}

private:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    // 剣の描画用メッシュコンポーネント
    std::shared_ptr<SkeletalMeshComponent> swordMeshComponent;
};
