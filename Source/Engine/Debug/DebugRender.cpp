#include "pch.h"
#include "DebugRender.h"

#include "Graphics/Renderer/ShapeRenderer.h"

void DebugRender::DrawSphere(
    const DirectX::XMFLOAT3& pos,
    float radius,
    const DirectX::XMFLOAT4& color,
    float life, bool wired)
{
    DebugDrawCommand command{};
    command.type = DebugDrawType::Sphere;
    command.position = pos;
    command.size = DirectX::XMFLOAT3{ radius, radius, radius };
    command.color = color;
    command.lifetime = life;
    if (wired)
    {
        wiredCommands_.push_back(command);
    }
    else
    {
        commands_.push_back(command);
    }
}

void DebugRender::DrawBox(
    const DirectX::XMFLOAT3& pos,
    const DirectX::XMFLOAT3& size,
    const DirectX::XMFLOAT4& color,
    float life, bool wired)
{
    DebugDrawCommand command{};
    command.type = DebugDrawType::Box;
    command.position = pos;
    command.size = size;
    command.color = color;
    command.lifetime = life;
    if (wired)
    {
        wiredCommands_.push_back(command);
    }
    else
    {
        commands_.push_back(command);
    }
}

void DebugRender::DrawCapsule(
    const DirectX::XMFLOAT3& startPos,
    const DirectX::XMFLOAT3& endPos,
    float radius,
    const DirectX::XMFLOAT4& color,
    float life, bool wired)
{
    DebugDrawCommand command{};
    command.type = DebugDrawType::Capsule;
    // カプセルの中心位置を計算
    command.position = startPos;
    command.endPosition = endPos;
    // size.x に半径、size.y に高さを格納
    DirectX::XMVECTOR startVec = DirectX::XMLoadFloat3(&startPos);
    DirectX::XMVECTOR endVec = DirectX::XMLoadFloat3(&endPos);
    DirectX::XMVECTOR heightVec = DirectX::XMVectorSubtract(endVec, startVec);
    float height = DirectX::XMVectorGetX(DirectX::XMVector3Length(heightVec));
    command.size = DirectX::XMFLOAT3{ radius, height, 0.0f };
    command.color = color;
    command.lifetime = life;
    if (wired)
    {
        wiredCommands_.push_back(command);
    }
    else
    {
        commands_.push_back(command);
    }
}

void DebugRender::DrawLine(
    const DirectX::XMFLOAT3& startPos,
    const DirectX::XMFLOAT3& endPos,
    const DirectX::XMFLOAT4& color,
    float life, bool wired)
{
    DebugDrawCommand command{};
    command.type = DebugDrawType::Line;
    // 線の終点位置を position に格納
    command.position = startPos;
    command.endPosition = endPos;
    // size は未使用だが初期化しておく
    command.size = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
    command.color = color;
    command.lifetime = life;
    if (wired)
    {
        wiredCommands_.push_back(command);
    }
    else
    {
        commands_.push_back(command);
    }
}

void DebugRender::DrawLightIcon(DirectX::XMFLOAT3 pos, const DirectX::XMFLOAT4 color)
{
    float s = 0.2f;

    DrawLine({ pos.x - s, pos.y, pos.z }, { pos.x + s, pos.y, pos.z }, color);
    DrawLine({ pos.x, pos.y - s, pos.z }, { pos.x, pos.y + s, pos.z }, color);
    DrawLine({ pos.x, pos.y, pos.z - s }, { pos.x, pos.y, pos.z + s }, color);
    DrawLine(pos, { pos.x, pos.y + 0.3f, pos.z }, color);
}

void DebugRender::DrawCylinder(
    const DirectX::XMFLOAT3& pos,
    float radius,
    float height,
    const DirectX::XMFLOAT4& color,
    float life, bool wired)
{
    DebugDrawCommand command{};
    command.type = DebugDrawType::Cylinder;
    command.position = pos;
    command.size = DirectX::XMFLOAT3{ radius, height, radius };
    command.color = color;
    command.lifetime = life;
    if (wired)
    {
        wiredCommands_.push_back(command);
    }
    else
    {
        commands_.push_back(command);
    }
}

void DebugRender::Tick(float deltaTime)
{
#if 0 // ライフタイムいる時に使用する
    for (auto it = commands_.begin(); it != commands_.end(); )
    {
        if (it->lifetime > 0.0f)
        {
            it->lifetime -= deltaTime;
            if (it->lifetime <= 0.0f)
            {
                it = commands_.erase(it);
                continue;
            }
        }
        ++it;
    }

#endif // 0 // ライフタイムいる時に使用する
    wiredCommands_.clear();
    commands_.clear();

}

void DebugRender::Render(ID3D11DeviceContext* immediateContext)
{
    for (auto& cmd : commands_)
    {
        switch (cmd.type)
        {
        case DebugDrawType::Sphere:
            ShapeRenderer::DrawSphere(
                immediateContext,
                cmd.position,
                cmd.size.x,
                cmd.color);
            break;
        case DebugDrawType::Box:
            ShapeRenderer::DrawBoxCenter(
                immediateContext,
                cmd.position,
                DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f },
                cmd.size,
                cmd.color);
            break;
        case DebugDrawType::Line:
            ShapeRenderer::DrawLineSegment(
                immediateContext,
                cmd.position,
                cmd.endPosition, cmd.color);
            break;
        case DebugDrawType::Capsule:
            ShapeRenderer::DrawCapsule(
                immediateContext,
                cmd.position,
                cmd.size.x,
                cmd.size.y,
                cmd.color);
            break;
        case DebugDrawType::Cylinder:
            ShapeRenderer::DrawCylinder(
                immediateContext,
                cmd.position,
                cmd.size.x,
                cmd.size.y,
                cmd.color);
            break;
        default:
            break;
        }
    }
}

void DebugRender::WiredRender(ID3D11DeviceContext* immediateContext)
{
    for (auto& cmd : wiredCommands_)
    {
        switch (cmd.type)
        {
        case DebugDrawType::Sphere:
            ShapeRenderer::DrawDebugSphere(
                immediateContext,
                cmd.position,
                cmd.size.x,
                cmd.color,32);
            break;
        case DebugDrawType::Box:
            ShapeRenderer::DrawBoxCenter(
                immediateContext,
                cmd.position,
                DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f },
                cmd.size,
                cmd.color);
            break;
        case DebugDrawType::Line:
            ShapeRenderer::DrawLineSegment(
                immediateContext,
                cmd.position,
                cmd.endPosition, cmd.color);
            break;
        case DebugDrawType::Capsule:
            ShapeRenderer::DrawCapsule(
                immediateContext,
                cmd.position,
                cmd.size.x,
                cmd.size.y,
                cmd.color);
            break;
        case DebugDrawType::Cylinder:
            ShapeRenderer::DrawCylinder(
                immediateContext,
                cmd.position,
                cmd.size.x,
                cmd.size.y,
                cmd.color);
            break;
        default:
            break;
        }
    }
}