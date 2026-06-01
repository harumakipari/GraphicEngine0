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

    // テキスト描画コンポーネントに使用している　テキストが更新された時に呼ばれる
    bool IsDirty() const { return dirty; }
    void ClearDirty() { dirty = false; }

public:
    int zOrder = 0; // 値が大きいほど手前に描画される

    bool dirty = true;// テキスト描画コンポーネントに使用している

protected:
    // スクリーン座標
    SpriteUV uv{ 0,0,100,100 };
    XMFLOAT2 scale = { 1.0f,1.0f };
    XMFLOAT2 worldPosition = { 0.0f,0.0f };
    XMFLOAT2 size = { 1.0f,1.0f };
    XMFLOAT2 pivot = { 0.0f,0.0f };
    bool visible = true;
    bool enabled = true;
    float worldAngle = 0.0f;

    // ローカル（親基準）
    XMFLOAT2 localPosition = { 0.0f,0.0f };
    float localAngle = 0.0f;



    std::string name = "UICoreComponent";
    UICoreComponent* parent = nullptr;
    std::vector<UICoreComponent*> children;

    // アクターの削除予約
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
        // ダミーテクスチャを設定
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

    // UV設定
    void SetUV(const SpriteUV& inUV) { uv = inUV; }

protected:
    std::shared_ptr<Sprite>  texture;

};


class UIArrowComponent : public UIImageComponent
{
public:
    UIArrowComponent(const std::string& filename, const std::string& name) ;

    UIArrowComponent() = default;

    void SetStart(DirectX::XMFLOAT3 startPos)
    {
        this->startPos = startPos;
    }

    void SetEnd(DirectX::XMFLOAT3 endPos);
   

private:
    std::vector<std::shared_ptr<UIImageComponent>> bodyRects;
    std::shared_ptr<UIImageComponent> head;

    DirectX::XMFLOAT3 startPos = { 0,0,0 };
    DirectX::XMFLOAT3 endPos = { 0,0,0 };

};

class UIRingEffect : public UIImageComponent
{
public:
    UIRingEffect(const std::string& texPath, XMFLOAT4 endColor)
        : UIImageComponent(texPath, "RingEffect")
    {
        lifeTime = 0.5f;
        elapsed = 0.0f;

        startSize = { 20.0f, 20.0f };
        endSize = { 400.0f, 400.0f };
        //endSize = { 300.0f, 300.0f };

        SetSize(startSize);
        SetPivot({ 0.5f, 0.5f }); // 中心基準

        this->endColor = endColor;

    }

    void SetEndSize(DirectX::XMFLOAT2 endSize) { this->endSize = endSize; }

    void Update(float dt) override
    {
        elapsed += dt;

        float t = elapsed / lifeTime;
        if (t > 1.0f)
        {
            MarkPendingKill();
            return;
        }

        // サイズ補間
        XMFLOAT2 newSize;
        newSize.x = startSize.x + (endSize.x - startSize.x) * t;
        newSize.y = startSize.y + (endSize.y - startSize.y) * t;
        SetSize(newSize);

        // 透明度
         // 色補間（白 → 黄色）
        XMFLOAT4 startColor = { 1,1,1,1 };


        XMFLOAT4 color;
        color.x = startColor.x + (endColor.x - startColor.x) * t;
        color.y = startColor.y + (endColor.y - startColor.y) * t;
        color.z = startColor.z + (endColor.z - startColor.z) * t;
        color.w = startColor.w + (endColor.w - startColor.w) * t;

        SetColor(color);
    }

private:
    float lifeTime;
    float elapsed;

    XMFLOAT2 startSize;
    XMFLOAT2 endSize;

    XMFLOAT4 endColor;
};

class UICoreFlashEffect : public UIImageComponent
{
public:
    UICoreFlashEffect(const std::string& texPath)
        : UIImageComponent(texPath, "CoreFlash")
    {
        lifeTime = 0.25f;
        elapsed = 0.0f;

        startSize = { 30.0f, 30.0f };
        endSize = { 200.0f, 200.0f };

        SetSize(startSize);
        SetPivot({ 0.5f, 0.5f });
    }

    void Update(float dt) override
    {
        elapsed += dt;

        float t = elapsed / lifeTime;
        if (t > 1.0f)
        {
            MarkPendingKill();
            return;
        }

        // サイズ拡大
        XMFLOAT2 size;
        size.x = startSize.x + (endSize.x - startSize.x) * t;
        size.y = startSize.y + (endSize.y - startSize.y) * t;
        SetSize(size);

        // 明るさ → 消える
        float alpha = 1.0f - t;
        SetColor(DirectX::XMFLOAT4{ 1.0f, 0.8f, 0.2f, alpha });
    }

private:
    float lifeTime;
    float elapsed;

    XMFLOAT2 startSize;
    XMFLOAT2 endSize;
};


class UILineEffect : public UIImageComponent
{
public:
    UILineEffect(const std::string& texPath, XMFLOAT2 center)
        : UIImageComponent(texPath, "LineEffect")
    {
        lifeTime = 1.0f;
        elapsed = 0.0f;

        pos = center;

        // ランダム方向
        float angle = MathHelper::RandomRange(0.0f, 360.0f);
        angle = DirectX::XMConvertToRadians(angle);

        float speed = MathHelper::RandomRange(200.0f, 500.0f);
        velocity = { cosf(angle) * speed, sinf(angle) * speed };

        // 初期サイズランダム
        float s = MathHelper::RandomRange(30.0f, 80.0f);
        startSize = { s, s };
        endSize = { s * 0.2f, s * 0.2f }; // 小さくなる

        SetSize(startSize);
        SetPivot({ 0.5f, 0.5f });

        // 色ランダム
        baseColor = RandomStarColor();
        SetColor(baseColor);
    }

    void Update(float dt) override
    {
        elapsed += dt;

        float t = elapsed / lifeTime;
        if (t > 1.0f)
        {
            MarkPendingKill();
            return;
        }


        SetWorldAngleDegree(this->worldAngle + dt * 180.0f);
        // 移動
        pos.x += velocity.x * dt;
        pos.y += velocity.y * dt;
        SetWorldPosition(pos);

        //  サイズ縮小
        XMFLOAT2 size;
        size.x = startSize.x + (endSize.x - startSize.x) * t;
        size.y = startSize.y + (endSize.y - startSize.y) * t;
        SetSize(size);

        //  フェード
        float alpha = 1.0f - t;
        SetColor(DirectX::XMFLOAT4{ baseColor.x, baseColor.y, baseColor.z, alpha });
    }


    DirectX::XMFLOAT4 RandomStarColor()
    {
        int r = MathHelper::RandomRange(0, 3);

        switch (r)
        {
        case 0: return { 1.0f, 0.6f, 0.8f, 1.0f }; // ピンク
        case 1: return { 1.0f, 1.0f, 0.3f, 1.0f }; // 黄色
        case 2: return { 0.7f, 0.5f, 1.0f, 1.0f }; // 紫
        default:return { 0.5f, 1.0f, 1.0f, 1.0f }; // 水色
        }
    }
private:
    float lifeTime;
    float elapsed;

    XMFLOAT2 pos;
    XMFLOAT2 velocity;

    XMFLOAT2 startSize;
    XMFLOAT2 endSize;

    XMFLOAT4 baseColor;
};


class UIStarTrailEffect : public UIImageComponent
{
public:
    UIStarTrailEffect(const std::string& file, const std::string& name)
        : UIImageComponent(file, name)
    {
    }

    void Update(float dt) override
    {
        elapsedTime += dt;

        float t = elapsedTime / lifeTime;

        // ===== サイズ変化 =====
        float scaleValue = 1.0f;

        if (t < 0.4f)
        {
            // ポンポンする
            float pulse = sinf(t * 20.0f) * 0.2f; // ←ここ調整
            scaleValue = 1.0f + pulse;
        }
        else
        {
            // 最後は縮小
            float shrinkT = (t - 0.4f) / 0.6f;
            scaleValue = 1.0f - shrinkT;
        }

        scale = { scaleValue, scaleValue };

        // ===== 発光っぽいカラー =====
        float glow = 1.0f + (1.0f - t); // 2 → 1 に落ちる
        SetColor(XMFLOAT4{ glow, glow, glow, 1.0f });

        // ===== 消滅 =====
        if (t >= 1.0f)
        {
            MarkPendingKill();
        }
    }

public:
    float lifeTime = 0.6f;

private:
    float elapsedTime = 0.0f;
};

class UIStarBurstEffect : public UIImageComponent
{
public:
    UIStarBurstEffect(const std::string& file, XMFLOAT2 center)
        : UIImageComponent(file, "burst"), center(center)
    {
        pos = center;
        pivot = { 0.5f,0.5f };
    }

    void Update(float dt) override
    {
        elapsedTime += dt;
        float t = elapsedTime / lifeTime;

        // 放射移動
        pos.x += velocity.x * dt;
        pos.y += velocity.y * dt;
        velocity.x *= 0.97f;
        velocity.y *= 0.97f;
        SetWorldPosition(pos);

        // サイズ縮小
        float scaleValue = 1.0f - (t * t);
        scale = { scaleValue, scaleValue };

        // フェード
        SetColor(DirectX::XMFLOAT4{ 2.0f, 2.0f, 2.0f, 1.0f - t });

        if (t >= 1.0f)
        {
            MarkPendingKill();
        }
    }
    void SetVelocity(XMFLOAT2 v)
    {
        velocity = v;
    }
private:
    XMFLOAT2 center;
    XMFLOAT2 pos;
    XMFLOAT2 velocity;
    float elapsedTime = 0.0f;
    float lifeTime = 0.5f;
};

class UIStarEffect : public UIImageComponent
{
public:
    UIStarEffect(const std::string& texPath, XMFLOAT2 center)
        : UIImageComponent(texPath, "UIStarEffect")
    {
        lifeTime = 1.0f;
        elapsed = 0.0f;

        pos = center;

        // 初期サイズランダム
        float s = MathHelper::RandomRange(80.0f, 80.0f);
        startSize = { s, s };
        endSize = { s * 0.2f, s * 0.2f }; // 小さくなる

        SetSize(startSize);
        SetPivot({ 0.5f, 0.5f });

        baseColor = { 1.0f, 1.0f, 0.3f, 1.0f }; // 黄色
        SetColor(baseColor);
    }

    void Update(float dt) override
    {
        elapsed += dt;

        float t = elapsed / lifeTime;
        if (t > 1.0f)
        {
            MarkPendingKill();
            return;
        }


        SetWorldAngleDegree(this->worldAngle + dt * 180.0f);
        // 移動
        pos.x += velocity.x * dt;
        pos.y = followPos.y + velocity.y * dt * 0.2f;
        SetWorldPosition(pos);

        //  サイズ縮小
        XMFLOAT2 size;
        size.x = startSize.x + (endSize.x - startSize.x) * t;
        size.y = startSize.y + (endSize.y - startSize.y) * t;
        SetSize(size);

        //  フェード
        float alpha = 1.0f - t;
        SetColor(DirectX::XMFLOAT4{ baseColor.x, baseColor.y, baseColor.z, alpha });
    }

    void SetVelocity(XMFLOAT2 v)
    {
        velocity = v;
    }

    void SetFollowPos(XMFLOAT2 followPos)
    {
        this->followPos = followPos;
    }

    DirectX::XMFLOAT4 RandomStarColor()
    {
        int r = MathHelper::RandomRange(0, 3);

        switch (r)
        {
        case 0: return { 1.0f, 0.6f, 0.8f, 1.0f }; // ピンク
        case 1: return { 1.0f, 1.0f, 0.3f, 1.0f }; // 黄色
        case 2: return { 0.7f, 0.5f, 1.0f, 1.0f }; // 紫
        default:return { 0.5f, 1.0f, 1.0f, 1.0f }; // 水色
        }
    }
private:
    float lifeTime;
    float elapsed;

    XMFLOAT2 pos;
    XMFLOAT2 velocity;

    XMFLOAT2 startSize;
    XMFLOAT2 endSize;

    XMFLOAT4 baseColor;

    DirectX::XMFLOAT2 followPos = { 0.0f,0.0f };
};




class UIDashEffect : public UIImageComponent
{
public:
    UIDashEffect(const std::string& texPath, XMFLOAT2 center, XMFLOAT2 dir)
        : UIImageComponent(texPath, "DashEffect")
    {
        lifeTime = 1.0f;
        elapsed = 0.0f;

        pos = center;

        float spread = MathHelper::RandomRange(-0.3f, 0.3f);

        XMFLOAT2 side = { dir.y, dir.x }; // 横方向

        velocity = {
            dir.x * 300.0f + side.x * spread * 100.0f,
            dir.y * 300.0f + side.y * spread * 100.0f
        };

        // 初期サイズランダム
        float s = MathHelper::RandomRange(30.0f, 80.0f);
        startSize = { s, s };
        endSize = { s * 0.7f, s * 0.7f };

        SetSize(startSize);
        SetPivot({ 0.5f, 0.5f });

        // 色ランダム
        baseColor = { 0.6f, 0.9f, 1.0f, 1.0f };
        SetColor(baseColor);
    }

    void Update(float dt) override
    {
        elapsed += dt;

        float t = elapsed / lifeTime;
        if (t > 1.0f)
        {
            MarkPendingKill();
            return;
        }


        SetWorldAngleDegree(this->worldAngle + dt * 30.0f);
        // 移動
        pos.x += velocity.x * dt;
        pos.y += velocity.y * dt;
        SetWorldPosition(pos);

        //  サイズ縮小
        XMFLOAT2 size;
        size.x = startSize.x + (endSize.x - startSize.x) * t;
        size.y = startSize.y + (endSize.y - startSize.y) * t;
        SetSize(size);

        //  フェード
        float alpha = powf(1.0f - t, 2.0f);
        SetColor(DirectX::XMFLOAT4{ baseColor.x, baseColor.y, baseColor.z, alpha });
    }


    DirectX::XMFLOAT4 RandomStarColor()
    {
        int r = MathHelper::RandomRange(0, 3);

        switch (r)
        {
        case 0: return { 1.0f, 0.6f, 0.8f, 1.0f }; // ピンク
        case 1: return { 1.0f, 1.0f, 0.3f, 1.0f }; // 黄色
        case 2: return { 0.7f, 0.5f, 1.0f, 1.0f }; // 紫
        default:return { 0.5f, 1.0f, 1.0f, 1.0f }; // 水色
        }
    }
private:
    float lifeTime;
    float elapsed;

    XMFLOAT2 pos;
    XMFLOAT2 velocity;

    XMFLOAT2 startSize;
    XMFLOAT2 endSize;

    XMFLOAT4 baseColor;
};


class UISpikeEffect : public UIImageComponent
{
public:
    UISpikeEffect(const std::string& texPath)
        : UIImageComponent(texPath, "SpikeEffect")
    {
        lifeTime = 0.12f;
        elapsed = 0.0f;

        SetSize({ 250.0f, 250.0f });
        SetPivot({ 0.5f, 0.5f });

        // ランダム回転
        float angle = MathHelper::RandomRange(0.0f, 360.0f);
        SetWorldAngleDegree(angle);
    }

    void Update(float dt) override
    {
        elapsed += dt;

        float t = elapsed / lifeTime;
        if (t > 1.0f)
        {
            MarkPendingKill();
            return;
        }

        // 少しだけ拡大
        float scale = 1.0f + t * 0.5f;
        SetScale({ scale, scale });

        // すぐ消える
        float alpha = 1.0f - t;
        SetColor(DirectX::XMFLOAT4{ 1.0f, 0.3f, 0.1f, alpha });
    }

private:
    float lifeTime;
    float elapsed;
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

    // かざしたときにスケールで大きくするかどうか
    void SetUseHoverScale(bool useHoverScale) { this->useHoverScale = useHoverScale; }

private:
    bool IsInside(const DirectX::XMFLOAT2& p) const
    {
        // pivot を考慮した左上座標
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
            color = CoreColor::White;
            targetScale = normalScale;
            break;
        case UIButtonState::Hovered:
            color = CoreColor(0.8f, 0.8f, 0.8f, 1);
            targetScale = hoverScale;
            break;
        case UIButtonState::Pressed:
            targetScale = pressScale;
            color = CoreColor(0.8f, 0.8f, 0.8f, 1);
            break;
        case UIButtonState::Selected:
            color = CoreColor(0.8f, 0.8f, 0.8f, 1);
            targetScale = hoverScale;
            break;
        }
    }

    void UpdateScale(float deltaTime);
public:
    float normalScale = 1.0f;
    float hoverScale = 1.1f;
    float pressScale = 1.05f;
    float scaleSpeed = 10.0f; // 補間速度

private:
    float currentScale = 1.0f;
    float targetScale = 1.0f;
    bool useHoverScale = false; // かざしたときにスケールで大きくするかどうか

    bool isSelected = false;
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

    // ゲージの中身の色を設定する
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

        // ゲージの中身
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


        // 枠の描画
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
    std::shared_ptr<Sprite>  frameTexture;  //　枠のテクスチャ
    XMFLOAT2 gaugeOffset = { 0.0f,0.0f }; // ゲージの中身のオフセット
    CoreColor gaugeFrameColor = CoreColor::White; // ゲージのフレームの色
    CoreColor gaugeFillColor = CoreColor::White;
    XMFLOAT2 gaugeFillSize ={0.0f,0.0f};    // ゲージの中身のサイズ
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
        // ダミーテクスチャを設定
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

class UITextComponent : public UICoreComponent
{
public:
    explicit UITextComponent(const std::string& name) : UICoreComponent(name)
    {
        zOrder = 10;
    }

    void SetText(const std::wstring& t)
    {
        //if (text == t)
        //    return;
        this->text = t;
        //dirty = true;
    }

    void DrawTexts(ID3D11DeviceContext* immediateContext) override
    {
        if (!visible) return;

        FontManager::GetUIFont()->Draw(worldPosition.x, worldPosition.y, text.c_str(), color, scale.x, pivot.x, pivot.y);
    }


    void SetColor(const CoreColor color) { this->color = color; }

protected:
    std::wstring text;
    CoreColor color = CoreColor::White;
};

class UITextPopup :public UITextComponent
{
public:
    explicit UITextPopup(const std::string& name) : UITextComponent(name)
    {
        easingRunner = std::make_unique<EasingRunner>();
    }

    void Update(float dt) override;

    void Play(const std::wstring& text/*, const DirectX::XMFLOAT2& startPos*/)
    {
        SetText(text);
        //worldPosition = startPos;

        TestEasingHandler handler;
        handler.AddEasing(TestEaseType::OutExp, 0.0f, 1.0f, 1.3f);


        handler.SetCompletedFunction([this]()
            {
                MarkPendingKill();
            });

        PropertyAccessor<float> accessor;
        accessor.getter = [this]() { return 1.0f; };
        accessor.setter = [this](float v)
            {
                float startPos = worldPosition.y;
                // 位置を動かす
                float endPos = worldPosition.y + 10.0f;
                worldPosition.y = std::lerp(startPos, endPos, v);

                // フェードアウト
                color.a = 1.0f - v;

                // スケール
                scale.x = std::lerp(0.8f, 1.2f, v);
            };

        easingRunner->StartHandler(handler, accessor);

    }

private:
    std::shared_ptr<EasingRunner> easingRunner;
};