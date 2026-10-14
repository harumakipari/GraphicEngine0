#pragma once


#include <memory>
#include <vector>
#include <functional>

#include "Components/Easing/CoreEasingComponent.h"
#include "Core/CoreColor.h"
#include "Core/Vector.h"
#include "Engine/Input/InputSystem.h"
#include "Graphics/Core/Graphics.h"
#include "Graphics/Renderer/SpriteRenderer.h"
#include "Graphics/Sprite/Sprite.h"
#include "UI/Font.h"
#include "UI/FontManager.h"

class UICoreComponent
{
public:
    UICoreComponent(const std::string& name)
    {
        this->name = name;
    }
    virtual ~UICoreComponent() = default;
    virtual void Update(float dt) {}
    virtual void Draw(ID3D11DeviceContext* immediateContext) {}
    virtual void DrawTexts(ID3D11DeviceContext* immediateContext) {}
    virtual void DrawSceneChangeSprite(ID3D11DeviceContext* immediateContext) {}
    virtual void OnMouseEnter() {}
    virtual void OnMouseLeave() {}
    virtual void OnMouseDown() {}
    virtual void OnMouseUp() {}
    virtual void OnClick() {}
    virtual void DrawImGui();

    void UpdateTransform();

    void SetParent(UICoreComponent* parent);
    const std::vector<UICoreComponent*>& GetChildren() const { return children; }

    void SetWorldPosition(const XMFLOAT2 worldPos) { this->worldPosition = worldPos; }

    void SetSize(const XMFLOAT2 size) { this->size = size; }

    void SetPivot(const XMFLOAT2 pivot) { this->pivot = pivot; }

    void SetScale(const XMFLOAT2 scale) { this->scale = scale; }

    void SetWorldAngleDegree(float angle) { this->worldAngle = angle; }

    bool IsVisible() const { return visible; }

    bool IsEnabled() const { return enabled; }

    void SetVisible(const bool visible) { this->visible = visible; }

    void SetEnable(const bool enabled) { this->enabled = enabled; }

    void SetLocalPosition(const XMFLOAT2 localPos) { this->localPosition = localPos; }

    void MarkPendingKill() { isPendingKill = true; }

    bool IsPendingKill() const { return isPendingKill; }

    XMFLOAT2 GetWorldPosition() const { return this->worldPosition; }

    XMFLOAT2 GetSize() const { return this->size; }

    bool IsDirty() const { return dirty; }
    void ClearDirty() { dirty = false; }

public:
    int zOrder = 0; 

    bool dirty = true;

protected:
    SpriteUV uv{ 0,0,100,100 };
    XMFLOAT2 scale = { 1.0f,1.0f };
    XMFLOAT2 worldPosition = { 0.0f,0.0f };
    XMFLOAT2 size = { 1.0f,1.0f };
    XMFLOAT2 pivot = { 0.0f,0.0f };
    bool visible = true;
    bool enabled = true;
    float worldAngle = 0.0f;

    XMFLOAT2 localPosition = { 0.0f,0.0f };
    float localAngle = 0.0f;

    std::string name = "UICoreComponent";
    UICoreComponent* parent = nullptr;
    std::vector<UICoreComponent*> children;

    bool isPendingKill = false;

};


class UIImageComponent : public UICoreComponent
{
public:
    UIImageComponent(const std::string& filename, const std::string& name) :UICoreComponent(name)
    {
        texture = std::make_shared<Sprite>(Graphics::GetDevice(), std::wstring(filename.begin(), filename.end()).c_str());
        uv.w = texture->GetTextureSize().x;
        uv.h = texture->GetTextureSize().y;
    }

    UIImageComponent(const std::string& name) :UICoreComponent(name)
    {
        texture = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/square.png");
        uv.w = texture->GetTextureSize().x;
        uv.h = texture->GetTextureSize().y;
    }

    UIImageComponent() = default;

    CoreColor color = CoreColor::White;

    void SetColor(const CoreColor color) { this->color = color; }
    void SetColor(const XMFLOAT4 color)
    {
        this->color.r = color.x;
        this->color.g = color.y;
        this->color.b = color.z;
        this->color.a = color.w;
    }



    void SetTexture(const std::shared_ptr<Sprite>& sprite)
    {
        texture = sprite;
        uv.w = texture->GetTextureSize().x;
        uv.h = texture->GetTextureSize().y;
    }

    void Draw(ID3D11DeviceContext* immediateContext) override
    {
        SpriteRenderer::Draw(
            texture.get(),
            worldPosition,
            size,
            color,
            uv,
            worldAngle,
            pivot,
            scale
        );
    }

    void SetUV(const SpriteUV& inUV) { uv = inUV; }

protected:
    std::shared_ptr<Sprite>  texture;

};




enum class UIButtonState :uint8_t
{
    Normal,
    Hovered,
    Pressed,
    Selected,
};

class UIButtonComponent : public UIImageComponent
{
public:
    UIButtonComponent(const std::string& filename, const std::string& name) :UIImageComponent(filename, name)
    {
        hoverScale = 1.1f;
        pressScale = 1.05f;
    }

    UIButtonComponent(const std::string& name) :UIImageComponent(name) {}

    UIButtonState state = UIButtonState::Normal;

    std::function<void()> onClick;

    void Update(float dt) override;

    void OnClick() override
    {
        if (onClick) onClick();
    }

    void SetUseHoverScale(bool useHoverScale) { this->useHoverScale = useHoverScale; }

    void SetVisualColors(const CoreColor& normal, const CoreColor& hovered, const CoreColor& pressed, const CoreColor& selected) { normalColor = normal; hoveredColor = hovered; pressedColor = pressed; selectedColor = selected; }

private:
    bool IsInside(const DirectX::XMFLOAT2& p) const
    {
        float left = worldPosition.x - size.x * pivot.x;
        float top = worldPosition.y - size.y * pivot.y;

        float right = left + size.x;
        float bottom = top + size.y;

        return p.x >= left && p.x <= right &&
            p.y >= top && p.y <= bottom;
    }

    void UpdateVisual()
    {
        switch (state)
        {
        case UIButtonState::Normal:
            color = normalColor;
            targetScale = normalScale;
            break;
        case UIButtonState::Hovered:
            color = hoveredColor;
            targetScale = hoverScale;
            break;
        case UIButtonState::Pressed:
            targetScale = pressScale;
            color = CoreColor(0.8f, 0.8f, 0.8f, 1);
            break;
        case UIButtonState::Selected:
            color = hoveredColor;
            targetScale = hoverScale;
            break;
        }
    }

    void UpdateScale(float deltaTime);
public:
    float normalScale = 1.0f;
    float hoverScale = 1.1f;
    float pressScale = 1.05f;
    float scaleSpeed = 10.0f;

private:
    float currentScale = 1.0f;
    float targetScale = 1.0f;
    bool useHoverScale = false;

    bool isSelected = false;
    CoreColor normalColor = CoreColor::White;
    CoreColor hoveredColor = CoreColor(0.8f, 0.8f, 0.8f, 1);
    CoreColor pressedColor = CoreColor(0.8f, 0.8f, 0.8f, 1);
    CoreColor selectedColor = CoreColor(0.8f, 0.8f, 0.8f, 1);
};


class UIGaugeFillComponent : public UIImageComponent
{
public:
    UIGaugeFillComponent(const std::string& filename, const std::string& name)
        : UIImageComponent(filename, name)
    {
    }

    UIGaugeFillComponent(const std::string& name)
        : UIImageComponent(name)
    {
    }

    void Draw(ID3D11DeviceContext* immediateContext) override
    {
        if (value <= 0.0f)
            return;

        XMFLOAT2 drawSize = size;
        SpriteUV drawUv = uv;

        if (horizontal)
        {
            drawSize.x *= value;
            drawUv.w *= value;
        }
        else
        {
            drawSize.y *= value;
            drawUv.h *= value;
        }

        SpriteRenderer::Draw(
            texture.get(),
            worldPosition,
            drawSize,
            color,
            drawUv,
            worldAngle,
            pivot,
            scale
        );
    }

    void SetValue(const float normalizedValue)
    {
        value = std::clamp(normalizedValue, 0.0f, 1.0f);
    }

    void SetValue(const float current, const float max)
    {
        SetValue(max > 0.0f ? current / max : 0.0f);
    }

    void SetHorizontal(const bool horizontal)
    {
        this->horizontal = horizontal;
    }

private:
    float value = 1.0f;
    bool horizontal = true;
};


class UIGaugeComponent : public UIImageComponent
{
public:
    UIGaugeComponent(const std::string& frameFilename, const std::string& fillFilename, const std::string& name) :UIImageComponent(fillFilename, name)
    {
        frameTexture = std::make_unique<Sprite>(Graphics::GetDevice(), std::wstring(frameFilename.begin(), frameFilename.end()).c_str());
    }

    UIGaugeComponent(const std::string& name) :UIImageComponent(name)
    {
        frameTexture = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/square.png");
    }

    void SetGaugeFillColor(const CoreColor color)
    {
        gaugeFillColor = color;
    }

    void Draw(ID3D11DeviceContext* immediateContext) override
    {
        XMFLOAT2 drawSize = gaugeFillSize;

        if (horizontal)
            drawSize.x *= value;
        else
            drawSize.y *= value;

        SpriteRenderer::Draw(
            texture.get(),
            { worldPosition.x + gaugeOffset.x,worldPosition.y + gaugeOffset.y },
            drawSize,
            gaugeFillColor,
            uv,
            worldAngle,
            pivot,
            scale
        );

        SpriteRenderer::Draw(
            frameTexture.get(),
            worldPosition,
            size,
            gaugeFrameColor,
            { uv.x,uv.y,size.x,size.y },
            worldAngle,
            pivot,
            scale
        );


    }


    void SetValue(const float current, const float max)
    {
        value = std::clamp(current / max, 0.0f, 1.0f);
    }

    void SetGaugeFrameColor(const CoreColor color)
    {
        this->gaugeFrameColor = color;
    }

    void SetGaugeOffset(const XMFLOAT2 offset)
    {
        this->gaugeOffset = offset;
    }

    void SetGaugeFillSize(DirectX::XMFLOAT2 gaugeFillSize) { this->gaugeFillSize = gaugeFillSize; }

    bool horizontal = true;

private:
    float value = 1.0f;  // 0.0f ~ 1.0f
    std::shared_ptr<Sprite>  frameTexture;  
    XMFLOAT2 gaugeOffset = { 0.0f,0.0f };
    CoreColor gaugeFrameColor = CoreColor::White;
    CoreColor gaugeFillColor = CoreColor::White;
    XMFLOAT2 gaugeFillSize ={0.0f,0.0f};    
};

class UISceneChangeComponent : public UICoreComponent
{
public:
    UISceneChangeComponent(const std::string& filename, const std::string& name) :UICoreComponent(name)
    {
        texture = std::make_shared<Sprite>(Graphics::GetDevice(), std::wstring(filename.begin(), filename.end()).c_str());
        uv.w = texture->GetTextureSize().x;
        uv.h = texture->GetTextureSize().y;
    }

    UISceneChangeComponent(const std::string& name) :UICoreComponent(name)
    {
        texture = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/square.png");
        uv.w = texture->GetTextureSize().x;
        uv.h = texture->GetTextureSize().y;
    }

    UISceneChangeComponent() = default;

    CoreColor color = CoreColor::White;

    void SetColor(const CoreColor color) { this->color = color; }

    void SetTexture(const std::shared_ptr<Sprite>& sprite)
    {
        texture = sprite;
        uv.w = texture->GetTextureSize().x;
        uv.h = texture->GetTextureSize().y;
    }



    void DrawSceneChangeSprite(ID3D11DeviceContext* immediateContext) override
    {
        SpriteRenderer::Draw(
            texture.get(),
            worldPosition,
            size,
            color,
            uv,
            worldAngle,
            pivot,
            scale
        );
    }

private:
    std::shared_ptr<Sprite>  texture;
};

