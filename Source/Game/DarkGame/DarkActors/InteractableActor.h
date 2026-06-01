#pragma once
#include "Core/Actor.h"
#include "Game/DarkGame/Interactable.h"

class InteractableActor :public Actor, public IInteractable
{
public:
    InteractableActor(const std::string& actorName) :Actor(actorName) {}

    virtual void Interact() override {}
};