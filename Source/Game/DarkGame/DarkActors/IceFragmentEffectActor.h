#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Core/Actor.h"
#include "Components/Render/PointLightComponent.h"

class IceFragmentEmitterActor :public Actor
{
private:
    struct Fragment
    {
        std::shared_ptr<InstanceMeshComponent> meshComponent;

        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 velocity;

        DirectX::XMFLOAT3 rotation;
        DirectX::XMFLOAT3 rotationSpeed;

        float scale;
        float life;
    };

public:
    explicit IceFragmentEmitterActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    void DrawImGuiDetails() override;

    // 飛ぶ方向を決定する
    void SetDirection(DirectX::XMFLOAT3 hitNormal, DirectX::XMFLOAT3 enemyCenter, DirectX::XMFLOAT3 playerPos);

private:
    std::string parentName = "RootComponent";

    std::shared_ptr<RotationComponent> rotationComponent;

    std::vector<Fragment> fragments;


    // 調整値
    float spreadAngle = 20.0f;      // 広がり角度
    float speedMin = 5.0f;
    float speedMax = 12.0f;
    float gravity = 3.0f;
    float lifeTime = 0.7f;
    int fragmentCount = 15;
    float spawnOffset = 0.8f;  // 生成位置を敵の中心側へ少し押し戻す数値
};