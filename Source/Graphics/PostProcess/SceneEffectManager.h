#pragma once
#include <ranges>
#include <unordered_map>

#include "SceneEffectBase.h"

class SceneEffectManager
{
public:
    void Initialize(ID3D11Device* device, uint32_t width, uint32_t height)
    {
        for (auto& effect : effects | std::views::values)
        {
            effect->Initialize(device, width, height);
        }
    }

    void ApplyAll(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* sceneColor, ID3D11ShaderResourceView* gbufferNormal,
        ID3D11ShaderResourceView* gbufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap)
    {
        ID3D11ShaderResourceView* current = sceneColor;

        for (auto* effect : executionOrder)
        {
            if (!effect->IsEnabled())
                continue;
            effect->Apply(immediateContext, current, gbufferNormal, gbufferDepth, gBufferPosition, gBufferPbrValue, shadowMap);

        }

        //lastOutput = current;
    }

    void AddEffect(std::unique_ptr<SceneEffectBase> effect)
    {
        const std::string name = effect->GetName();

        executionOrder.push_back(effect.get()); // ŽÀs‡‚É’Ç‰Á
        effects[name] = std::move(effect);      // Š—LŒ ‚Ímap
    }

    ID3D11ShaderResourceView* GetOutput(const std::string& name) const
    {
        auto it = effects.find(name);
        if (it != effects.end())
            return it->second->GetOutputSRV();
        return nullptr;
    }

    ID3D11ShaderResourceView* GetFinalOutput() const { return lastOutput; }

    void DrawGui()
    {
#ifdef USE_IMGUI

        for (auto& [name, effect] : effects)
        {
            if (ImGui::TreeNode(name.c_str()))
            {
                effect->DrawDebugUI();
                ImGui::TreePop();
            }
        }
#endif
    }
private:
    std::vector<SceneEffectBase*> executionOrder;
    std::unordered_map<std::string, std::unique_ptr<SceneEffectBase>> effects;
    ID3D11ShaderResourceView* lastOutput = nullptr;

};