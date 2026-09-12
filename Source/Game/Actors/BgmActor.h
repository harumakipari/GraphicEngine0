#pragma once
#include "Components/Audio/AudioSourceComponent.h"
#include "Core/Actor.h"


class BgmActor :public Actor
{
public:
    BgmActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override {}

    void SetSource(const std::wstring& filePath) const
    {
        audioComponent->SetSource(filePath);
    }

    void SetLoop(const bool loop)const { audioComponent->SetLoop(loop); }

    void SetBgm(const bool isBgm) const { audioComponent->SetBgm(isBgm); }

    void SetVolume(const float volume) const { audioComponent->SetVolume(volume); }
    float GetVolume() const { return audioComponent ? audioComponent->GetVolume() : 0.0f; }

    void Play() const { audioComponent->Play(); }

    void Stop(const bool playTails = true) const { if (audioComponent) audioComponent->Stop(playTails); }
private:
    std::shared_ptr<AudioSourceComponent> audioComponent;
};
