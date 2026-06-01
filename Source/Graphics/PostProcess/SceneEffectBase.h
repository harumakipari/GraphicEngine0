#pragma once
#include <d3d11.h>
#include <memory>
#include <string>

class SceneEffectBase
{
public:
    explicit SceneEffectBase(const std::string& className) :name(className) {}
    virtual ~SceneEffectBase()
    {
    }

    virtual void Initialize(ID3D11Device* device, uint32_t width, uint32_t height) = 0;
    virtual void Apply(ID3D11DeviceContext* immediateContext, ID3D11ShaderResourceView* gBufferColor, ID3D11ShaderResourceView* gBufferNormal,
        ID3D11ShaderResourceView* gBufferDepth, ID3D11ShaderResourceView* gBufferPosition, ID3D11ShaderResourceView* gBufferPbrValue, ID3D11ShaderResourceView* shadowMap) = 0;

    virtual ID3D11ShaderResourceView* GetOutputSRV() const = 0;

    virtual const std::string& GetName() const { return name; }

    // UI Т▓Ро (ImGui)
    virtual void DrawDebugUI() {}

    bool IsEnabled() const { return enabled; }
protected:
    bool enabled = true;
private:
    std::string name;
};

