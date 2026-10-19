#pragma once
#include "Core/Actor.h"


class GameTimerUIActor :public Actor
{
public:
    explicit GameTimerUIActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override{}

    void Update(float elapsedTime)override{}

private:
};
