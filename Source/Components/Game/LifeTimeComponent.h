#ifndef LIFE_TIME_COMPONENT_H
#define LIFE_TIME_COMPONENT_H

#include "Components/Base/Component.h"

class LifeTimeComponent :public Component
{
public:
    LifeTimeComponent(const std::string& name, std::shared_ptr<Actor> owner) :Component(name, owner) {};
    virtual ~LifeTimeComponent() = default;

    void Initialize()override {};

    // Žõ–½‚ðÝ’è‚·‚é
    void SetLifeTime(float seconds)
    {
        lifeTime_ = seconds;
        elapsedTime_ = 0.0f;
        isStartCountDown_ = true;
    }

    void Tick(float deltaTime) override;

    virtual void UpdateComponentToWorld(UpdateTransformFlags update_transform_flags = UpdateTransformFlags::None, TeleportType teleport = TeleportType::None)override
    {
        // LogicComponent‚É‚ÍTransform‚ª‚È‚¢‚Ì‚ÅA‰½‚à‚µ‚È‚¢
    }

    float GetLifeTime() const { return this->lifeTime_; }
    float GetElapsedTime() const { return elapsedTime_; }
    float GetRemainingTime() const { return lifeTime_ - elapsedTime_; }

private:
    float lifeTime_ = 0.0f; // Žõ–½
    float elapsedTime_ = 0.0f;  // Œo‰ßŽžŠÔ
    bool isStartCountDown_ = true;

};




#endif // !LIFE_TIME_COMPONENT_H
