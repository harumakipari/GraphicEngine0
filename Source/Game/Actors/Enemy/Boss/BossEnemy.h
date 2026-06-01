#pragma once
#include "Game/Actors/Enemy/Enemy.h"

class RotationComponent;
class CharacterMovementComponent;

class BossEnemy :public Enemy
{
public:
    BossEnemy() = default;
    ~BossEnemy() override {}

    BossEnemy(const std::string& modelName) :Enemy(modelName) {}

    //コピーコンストラクタとコピー代入演算子を禁止にする
    BossEnemy(const BossEnemy&) = delete;
    BossEnemy& operator=(const BossEnemy&) = delete;

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime) override;

private:
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;
    std::shared_ptr<RotationComponent> rotationComponent;
};
