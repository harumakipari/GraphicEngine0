#include "pch.h"
#include "ParticleComponent.h"
#include "Core/Vector.h"
#include "Core/Actor.h"

//REGISTER_COMPONENT(ParticleComponent, "Effects");

void ParticleComponent::Load(const std::string& filePath)
{
    effectHandle = EffectManager::LoadEffectData(filePath);
}

void ParticleComponent::Play()
{
    // エフェクト再生
    if (effectHandle != -1)
    {
        // エフェクト発生前コールバック実行
        if (settings.onPreEmit)
        {
            settings.onPreEmit();
        }

        // ゲームオブジェクトの位置と回転を取得
        //XMFLOAT3 position = owner_.lock()->GetPosition();
        //XMFLOAT3 rotation = owner_.lock()->GetEulerRotation();
        UpdateComponentToWorld(); // これ入れないと最初に呼ばれる時に位置がずれる
        XMFLOAT3 position = GetComponentLocation();
        XMFLOAT3 rotation = GetComponentEulerRotation();
        elapsedTimeSincePlay = 0.0f;
        duration = CalculateDuration();
        emitTimer = 0.0f;
        isPlaying = true;
        // 再生開始遅延経過時間リセット
        elapsedDelayTime = 0.0f;
        // ★最初の1発
        EffectManager::Play(effectHandle, position, rotation);

    }
}

// エフェクトをアタッチ先に再生
void ParticleComponent::PlayAttached()
{
    if (effectHandle == -1) return;

    EffectManager::PlayAttached(effectHandle, shared_from_this());
    isPlaying = true;
}

void ParticleComponent::Stop()
{
    // エフェクト停止
    isPlaying = false;
}

void ParticleComponent::Tick(float deltaTime)
{
    // ループ再生でない場合は何もしない
    if (effectHandle == -1 || !IsPlaying() /*|| !settings.loop*/)
    {
        return;
    }


    // 再生開始遅延時間の処理
    if (elapsedDelayTime < settings.startDelay)
    {
        elapsedDelayTime += deltaTime;
        if (elapsedDelayTime < settings.startDelay)
        {
            // まだ遅延時間内なので再生しない
            return;
        }
    }

    // エフェクト発生前コールバック実行
    if (settings.onPreEmit)
    {
        settings.onPreEmit();
    }
    XMFLOAT3 position = GetComponentLocation();;
    XMFLOAT3 rotation = GetComponentEulerRotation();

    // 再生してからの経過時間更新
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
            EffectManager::Play(effectHandle, position, rotation);
        }
    }
    else
    {
        if (elapsedTimeSincePlay >= duration)
        {
            isPlaying = false;
        }
    }
}

float ParticleComponent::CalculateDuration() const
{
    float maxDuration = 0.0f;

    auto& effect = EffectManager::GetEffectData(effectHandle);

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
        effectHandle = EffectManager::LoadEffectDataWithDialog();
    }
    ImGui::SameLine();
    if (ImGui::Button("Play Effect"))
    {
        Play();
    }

    if (effectHandle != -1)
    {
        ImGui::Text("Effect Handle: %d", effectHandle);
    }
    else
    {
        ImGui::Text("No Effect Loaded");
    }

    ImGui::PopID();

#endif // USE_IMGUI

}