#pragma once
#include "Core/Actor.h"


class GameVictoryUIActor : public Actor
{
public:
    explicit GameVictoryUIActor(const std::string& actorName) : Actor(actorName) {}
    void Initialize(const Transform& transform) override{}

    void DrawImGuiDetails()override{}


private:

};
