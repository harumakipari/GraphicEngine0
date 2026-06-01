#pragma once
#include "Cherry.h"
#include "Components/Audio/CoreAudioSourceComponent.h"
#include "Core/Actor.h"

#include "Components/Effect/ParticleComponent.h"
#include "Components/Elastic/ElasticComponent.h"
#include "Engine/Audio/CoreAudio.h"
#include "Engine/Input/InputSystem.h"
#include "Physics/CollisionFunction.h"

class TargetPudding : public Actor
{
public:
    TargetPudding(const std::string& modelName) :Actor(modelName)
    {
    }
    std::shared_ptr<ElasticMeshComponent> elasticComponent;
    std::shared_ptr<SkeletalMeshComponent> whip;
    std::shared_ptr<ParticleComponent> particleComponent;

    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override
    {


    }
    void DrawImGuiDetails() override
    {
#ifdef USE_IMGUI
#endif
    };

};
