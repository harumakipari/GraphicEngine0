#pragma once

enum class DebugDrawType :uint8_t
{
    Sphere,
    Box,
    Line,
    Capsule,
    Cylinder,
};

struct DebugDrawCommand
{
    DebugDrawType type;
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 endPosition;
    DirectX::XMFLOAT3 size;
    DirectX::XMFLOAT4 color;
    float lifetime = 0.0f;
};

class DebugRender
{
public:
    static void DrawSphere(
        const DirectX::XMFLOAT3& pos,
        float radius,
        const DirectX::XMFLOAT4& color,
        float life = 0.0f,bool wired=false);

    static void DrawBox(
        const DirectX::XMFLOAT3& pos,
        const DirectX::XMFLOAT3& size,
        const DirectX::XMFLOAT4& color,
        float life = 0.0f, bool wired = false);

    static void DrawCapsule(
        const DirectX::XMFLOAT3& startPos,// ˆê”Ô‰º‚Ì“_
        const DirectX::XMFLOAT3& endPos,
        float radius,
        const DirectX::XMFLOAT4& color,
        float life = 0.0f, bool wired = false);

    static void DrawLine(
        const DirectX::XMFLOAT3& startPos,
        const DirectX::XMFLOAT3& endPos,
        const DirectX::XMFLOAT4& color,
        float life = 0.0f, bool wired = false);

    static void DrawCylinder(
        const DirectX::XMFLOAT3& pos,// ˆê”Ô‰º‚Ì“_
        float radius,
        float height,
        const DirectX::XMFLOAT4& color,
        float life = 0.0f, bool wired = false);

    static void DrawLightIcon(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 color);

    static void Tick(float deltaTime);
    static void Render(ID3D11DeviceContext* immediateContext);
    static void WiredRender(ID3D11DeviceContext* immediateContext);

    
private:
    static inline std::vector<DebugDrawCommand> commands_;
    static inline std::vector<DebugDrawCommand> wiredCommands_;
};
