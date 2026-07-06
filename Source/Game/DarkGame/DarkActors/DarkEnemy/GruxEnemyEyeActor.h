#pragma once
#include "Core/Actor.h"

class GruxEnemyEyeActor :public Actor
{
public:
    GruxEnemyEyeActor() = default;
    ~GruxEnemyEyeActor() override {}

    GruxEnemyEyeActor(const std::string& modelName) :Actor(modelName)
    {
    }

    //コピーコンストラクタとコピー代入演算子を禁止にする
    GruxEnemyEyeActor(const GruxEnemyEyeActor&) = delete;
    GruxEnemyEyeActor& operator=(const GruxEnemyEyeActor&) = delete;

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    void DrawImGuiDetails() override;

    void StartEyeFlash();
private:
    // 左目の描画用コンポーネントを追加　暗闇で光る目の表現用
    std::shared_ptr<SkeletalMeshComponent> leftEyeMeshComponent;
    // 左目の描画用コンポーネントを追加　横に光るフレアの表現用
    std::shared_ptr<SkeletalMeshComponent> leftEyeFlareMeshComponent;
    // 右目の描画用コンポーネントを追加　暗闇で光る目の表現用
    std::shared_ptr<SkeletalMeshComponent> rightEyeMeshComponent;
    // 右目の描画用コンポーネントを追加　横に光るフレアの表現用
    std::shared_ptr<SkeletalMeshComponent> rightEyeFlareMeshComponent;
    float emissionFactor = 10.0f;
    DirectX::XMFLOAT4 eyeColor = { 0.011f,0.034f,0.0f,1.0f };
    DirectX::XMFLOAT3 eyeFlareScale = { 9.6f,1.0f,3.95f };
    DirectX::XMFLOAT3 eyeFlareDegreeAngle = { 8.0f,90.0f,0.0f };
};
