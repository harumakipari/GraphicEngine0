#pragma once

#include <ranges>
#include <unordered_map>

#include "PostEffectBase.h"


class PostEffectManager
{
public:
    void Initialize(ID3D11Device* device, uint32_t width, uint32_t height)
    {
        for (auto& effect : effects | std::views::values)
        {
            effect->Initialize(device, width, height);
        }
    }

    void ApplyAll(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* colorSRV)
    {
        ID3D11ShaderResourceView* current = colorSRV;
        for (auto& effect : effects | std::views::values)
        {
            effect->Apply(immediateContext, current);
            current = effect->GetOutputSRV();
        }
        lastOutput = current;
    }

    void AddEffect(std::unique_ptr<PostEffectBase> effect)
    {
        const std::string name = effect->GetName();
        effects[name] = std::move(effect);
    }

    ID3D11ShaderResourceView* GetOutput(const std::string& name) const
    {
        auto it = effects.find(name);
        if (it != effects.end())
            return it->second->GetOutputSRV();
        return nullptr;
    }

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
    std::unordered_map<std::string, std::unique_ptr<PostEffectBase>> effects;
    ID3D11ShaderResourceView* lastOutput = nullptr;
};