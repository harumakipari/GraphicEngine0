#include "pch.h"
#include "ParticleComponent.h"
#include "Core/Vector.h"
#include "Core/Actor.h"
#include "Engine/Scene/Scene.h"

//REGISTER_COMPONENT(ParticleComponent, "Effects");

ParticleComponent::~ParticleComponent()
{
    Stop();
    CancelPendingEffectLoad();
}

void ParticleComponent::OnUnregister()
{
    Stop();
    CancelPendingEffectLoad();
    SceneComponent::OnUnregister();
}

void ParticleComponent::TrackPlayback(EffectPlaybackId id)
{
    std::erase_if(playbackIds, [](EffectPlaybackId oldId) { return !EffectManager::IsPlaying(oldId); });
    if (id != InvalidEffectPlaybackId) playbackIds.push_back(id);
}

void ParticleComponent::Load(const std::string& filePath)
{
    Stop();
    CancelPendingEffectLoad();
    effectHandle = -1;

    if (Scene::IsAsyncPreloadThread())
    {
        effectLoadTicket = EffectManager::LoadEffectDataAsync(filePath);
        Scene::RegisterPreloadEffectTicket(effectLoadTicket);
        return;
    }

    effectHandle = EffectManager::LoadEffectData(filePath);
}

void ParticleComponent::CancelPendingEffectLoad()
{
    if (effectLoadTicket)
    {
        effectLoadTicket->Cancel();
        effectLoadTicket.reset();
    }
}

void ParticleComponent::Play()
{
    Stop();
    if (GetEffectHandle() == -1)
    {
        if (effectLoadTicket && effectLoadTicket->GetState() == EffectLoadState::Pending)
        {
            pendingPlayRequest = true;
            pendingPlayAttachedRequest = false;
        }
        return;
    }

    BeginPlay();
}

void ParticleComponent::BeginPlay()
{
    const EffectHandle handle = GetEffectHandle();
    if (handle == -1) return;

    if (settings.onPreEmit)
    {
        settings.onPreEmit();
    }

    UpdateComponentToWorld();
    XMFLOAT3 position = GetComponentLocation();
    XMFLOAT3 rotation = GetComponentEulerRotation();
    elapsedTimeSincePlay = 0.0f;
    duration = CalculateDuration();
    emitTimer = 0.0f;
    isPlaying = true;
    elapsedDelayTime = 0.0f;
    TrackPlayback(EffectManager::Play(handle, position, rotation));
}
void ParticleComponent::PlayAttached()
{
    Stop();
    if (GetEffectHandle() == -1)
    {
        if (effectLoadTicket && effectLoadTicket->GetState() == EffectLoadState::Pending)
        {
            pendingPlayRequest = true;
            pendingPlayAttachedRequest = true;
        }
        return;
    }

    BeginPlayAttached();
}

void ParticleComponent::BeginPlayAttached()
{
    const EffectHandle handle = GetEffectHandle();
    if (handle == -1) return;

    TrackPlayback(EffectManager::PlayAttached(handle, shared_from_this()));
    isPlaying = true;
}
void ParticleComponent::Stop()
{
    for (auto id : playbackIds) EffectManager::Stop(id);
    playbackIds.clear();
    pendingPlayRequest = false;
    pendingPlayAttachedRequest = false;
    isPlaying = false;
}

void ParticleComponent::Tick(float deltaTime)
{
    if (effectLoadTicket && pendingPlayRequest)
    {
        const EffectLoadState state = effectLoadTicket->GetState();
        if (state == EffectLoadState::Pending)
        {
            return;
        }

        const bool playAttached = pendingPlayAttachedRequest;
        pendingPlayRequest = false;
        pendingPlayAttachedRequest = false;
        if (state == EffectLoadState::Ready)
        {
            if (playAttached)
            {
                BeginPlayAttached();
            }
            else
            {
                BeginPlay();
            }
        }
    }

    const EffectHandle handle = GetEffectHandle();
    if (handle == -1 || !IsPlaying())
    {
        return;
    }

    if (elapsedDelayTime < settings.startDelay)
    {
        elapsedDelayTime += deltaTime;
        if (elapsedDelayTime < settings.startDelay)
        {
            return;
        }
    }

    if (settings.onPreEmit)
    {
        settings.onPreEmit();
    }
    XMFLOAT3 position = GetComponentLocation();
    XMFLOAT3 rotation = GetComponentEulerRotation();

    elapsedTimeSincePlay += deltaTime;

    if (!settings.loop && elapsedTimeSincePlay >= duration)
    {
        isPlaying = false;
    }

    emitTimer += deltaTime;

    if (settings.loop)
    {
        float safeInterval = std::max<float>(emitInterval, 0.001f);

        while (emitTimer >= safeInterval)
        {
            emitTimer -= safeInterval;
            TrackPlayback(EffectManager::Play(handle, position, rotation));
        }
    }
    else if (elapsedTimeSincePlay >= duration)
    {
        isPlaying = false;
    }
}
float ParticleComponent::CalculateDuration() const
{
    const EffectHandle handle = GetEffectHandle();
    if (handle == -1)
    {
        return 0.0f;
    }

    float maxDuration = 0.0f;
    auto& effect = EffectManager::GetEffectData(handle);

    for (auto& emitter : effect.emitters)
    {
        float d =
            emitter.emitData.initialDelay.max +
            emitter.emitData.emitInterval.max * (emitter.emitData.emitCount.max - 1) +
            emitter.motionData.lifeTime.max;

        maxDuration = std::max<float>(maxDuration, d);
    }

    return maxDuration;
}
void ParticleComponent::DrawImGuiInspector()
{
#ifdef USE_IMGUI
    ImGui::PushID(this);
    SceneComponent::DrawImGuiInspector();


    if (ImGui::Button("Load Effect"))
    {
        // ダイアログを開いてエフェクトデータを読み込む
        Stop();
        CancelPendingEffectLoad();
        effectHandle = EffectManager::LoadEffectDataWithDialog();
    }
    ImGui::SameLine();
    if (ImGui::Button("Play Effect"))
    {
        Play();
    }

    if (GetEffectHandle() != -1)
    {
        ImGui::Text("Effect Handle: %d", GetEffectHandle());
    }
    else
    {
        ImGui::Text("No Effect Loaded");
    }

    ImGui::PopID();

#endif // USE_IMGUI

}