#include "pch.h"
#include "SoftBody2d.h"
#include "Engine/Input/InputSystem.h"

float Cross(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b)
{
    return a.x * b.y - a.y * b.x;
}
float Dot(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b)
{
    return a.x * b.x + a.y * b.y;
}

void Engine::Update(float deltaTime)
{
    using namespace DirectX;

    // velocity intergration
    for (auto& point : points)
    {
        // Apply gravity
        point.velocity.x += gravity.x * deltaTime;
        point.velocity.y += gravity.y * deltaTime;
    }
    for (auto& point : points)
    {
        // Update position
        point.position.x += point.velocity.x * deltaTime;
        point.position.y += point.velocity.y * deltaTime;
    }

    // Collision resolution
    for (auto& p : points)
    {
        Collision c = FindCollision(p.position, walls);

        if (c.depth < 0.0f)
        {
            continue;
        }

        // 押出し
        p.position.x += c.normal.x * c.depth;
        p.position.y += c.normal.y * c.depth;

        // Reflect velocity
        float dot = p.velocity.x * c.normal.x + p.velocity.y * c.normal.y;

        // 法線方向の速度成分
        DirectX::XMFLOAT2 vn = { c.normal.x * dot, c.normal.y * dot };
        // 接線方向の速度成分    ベクトルの差で求める
        DirectX::XMFLOAT2 vt = { p.velocity.x - vn.x, p.velocity.y - vn.y };

        // 反発係数による法線方向の速度反転
        float elasticity = 0.5f;
        vn.x = -elasticity * vn.x;
        vn.y = -elasticity * vn.y;

        // 摩擦係数による接線方向の速度減衰
        float friction = 0.8f;
        vt.x *= std::exp(-friction * deltaTime);
        vt.y *= std::exp(-friction * deltaTime);

        // 合成して最終的な速度を求める
        p.velocity.x = vn.x + vt.x;
        p.velocity.y = vn.y + vt.y;
    }

    if (InputSystem::GetInputState("MouseLeft"))
    {
        DirectX::XMFLOAT2 cursor = InputSystem::GetMousePositionScreen();
        for (auto& point : points)
        {
            DirectX::XMFLOAT2 delta = { cursor.x - point.position.x, cursor.y - point.position.y };
            float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (distance < point.radius + 20.0f)
            {
#if 0
                float force = (100.0f - distance) * 10.0f;
                point.velocity.x += (delta.x / distance) * force * deltaTime;
                point.velocity.y += (delta.y / distance) * force * deltaTime;
#endif // 0
                point.position.x = cursor.x;
                point.position.y = cursor.y;
                point.color = { 1.0f,0.0f,0.0f,1.0f };
            }
        }
    }
    if (InputSystem::GetInputState("MouseLeft", InputStateMask::Release))
    {// マウスを離した瞬間
        DirectX::XMFLOAT2 cursor = InputSystem::GetMousePositionScreen();
        for (auto& point : points)
        {
            DirectX::XMFLOAT2 delta = { cursor.x - point.position.x, cursor.y - point.position.y };
            float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (distance < point.radius)
            {
                float force = (100.0f - distance) * 10.0f;
                point.velocity.x += (delta.x / distance) * force;
                point.velocity.y += (delta.y / distance) * force;
                point.color = { 1.0f,1.0f,1.0f,1.0f };
            }
        }
    }
    constraintVisuals.clear();
    //HardConstraints(deltaTime);
    //SoftConstraints(deltaTime);
    //ForceBasedConstraints(deltaTime);

    for (const auto& b : softBodies)
    {
        // compute center of mass
        XMFLOAT2 center = { 0.0f,0.0f };
        for (const auto& v : b.vertices)
        {
            center.x += points[v.index].position.x;
            center.y += points[v.index].position.y;
        }
        // 平均を取る
        center.x /= static_cast<float>(b.vertices.size());
        center.y /= static_cast<float>(b.vertices.size());

        // compute the shape of angle
        float A = 0.0f, B = 0.0f;
        for (const auto& v : b.vertices)
        {
            XMFLOAT2 r = { points[v.index].position.x - center.x, points[v.index].position.y - center.y };
            A += Dot(r, v.position);
            B += Cross(r, v.position);
        }
        float angle = -std::atan2(B, A);

        // apply spring force
        const float springForce = 200.0f;
        for (const auto& v : b.vertices)
        {
            XMFLOAT2 target;
            target.x = center.x + (v.position.x * std::cos(angle) - v.position.y * std::sin(angle));
            target.y = center.y + (v.position.x * std::sin(angle) + v.position.y * std::cos(angle));
            XMFLOAT2 delta = { target.x - points[v.index].position.x, target.y - points[v.index].position.y };
            points[v.index].velocity.x += delta.x * springForce * deltaTime;
            points[v.index].velocity.y += delta.y * springForce * deltaTime;
        }
    }
}

void Engine::HardConstraints(float deltaTime)
{
    using namespace DirectX;
    for (const auto& c : constraints)
    {
        auto& p0 = points[c.index0].position;
        auto& p1 = points[c.index1].position;

        XMFLOAT2 delta = { p1.x - p0.x, p1.y - p0.y };  // 2点間のベクトル
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);

        // 本来あるべき長さに調整
        XMFLOAT2 requiredDelta = { delta.x / distance * c.distance,delta.y / distance * c.distance }; // 単位ベクトル＊本来の距離
        XMFLOAT2 offset = { requiredDelta.x - delta.x,requiredDelta.y - delta.y };

        p0.x -= offset.x * 0.5f;
        p0.y -= offset.y * 0.5f;
        p1.x += offset.x * 0.5f;
        p1.y += offset.y * 0.5f;


        // 
        squareSize = c.distance;
        //squareAngle = atan2f(delta.y, delta.x);
        squareAngle = -atan2(delta.x, delta.y);
        squareCenter = { (p0.x + p1.x) * 0.5f,(p0.y + p1.y) * 0.5f };
    }

}

void Engine::SoftConstraints(float deltaTime)
{
    using namespace DirectX;
    for (const auto& c : constraints)
    {
        auto& p0 = points[c.index0].position;
        auto& p1 = points[c.index1].position;

        XMFLOAT2 delta = { p1.x - p0.x, p1.y - p0.y };  // 2点間のベクトル
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);

        XMFLOAT2 requiredDelta = { delta.x / distance * c.distance,delta.y / distance * c.distance }; // 単位ベクトル＊本来の距離
        const float constraintDamping = 5.0f;
        float dampingFactor = 1.0f - std::exp(-constraintDamping * deltaTime);
        XMFLOAT2 offset = { (requiredDelta.x - delta.x) * dampingFactor,(requiredDelta.y - delta.y) * dampingFactor };

        p0.x -= offset.x * 0.5f;
        p0.y -= offset.y * 0.5f;
        p1.x += offset.x * 0.5f;
        p1.y += offset.y * 0.5f;

        squareSize = distance;
        //squareAngle = atan2f(delta.y, delta.x);
        squareAngle = -atan2(delta.x, delta.y);
        squareCenter = { (p0.x + p1.x) * 0.5f,(p0.y + p1.y) * 0.5f };
    }
}
void Engine::ForceBasedConstraints(float deltaTime)
{
    using namespace DirectX;
    for (const auto& c : constraints)
    {
        auto& p0 = points[c.index0].position;
        auto& p1 = points[c.index1].position;

        auto& v0 = points[c.index0].velocity;
        auto& v1 = points[c.index1].velocity;

        XMFLOAT2 delta = { p1.x - p0.x, p1.y - p0.y };  // 2点間のベクトル
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);

        XMFLOAT2 requiredDelta = { delta.x / distance * c.distance,delta.y / distance * c.distance }; // 単位ベクトル＊本来の距離
        const float springForce = 50.0f;
        XMFLOAT2 force = { (requiredDelta.x - delta.x) * springForce,(requiredDelta.y - delta.y) * springForce };   // 理想のベクトルー今のベクトル

        v0.x -= force.x * deltaTime;
        v0.y -= force.y * deltaTime;
        v1.x += force.x * deltaTime;
        v1.y += force.y * deltaTime;
#if 1
        // 相対速度
        XMFLOAT2 relVel = { v1.x - v0.x, v1.y - v0.y };
        // 相対速度がばね方向に伸び縮みしている成分のみ取り出す
        float relVelDot = (relVel.x * delta.x + relVel.y * delta.y);
        // ダンピング力
        const float dampingForce = 10.0f;
        float dampingFactor = std::exp(-dampingForce * deltaTime);

        XMFLOAT2 newVRel = { relVel.x * dampingFactor, relVel.y * dampingFactor };
        XMFLOAT2 vRelDelta = { newVRel.x - relVel.x,newVRel.y - relVel.y };

        v0.x -= vRelDelta.x * 0.5f;
        v0.y -= vRelDelta.y * 0.5f;
        v1.x += vRelDelta.x * 0.5f;
        v1.y += vRelDelta.y * 0.5f;
#endif // 1

        squareSize = distance;
        squareAngle = -atan2(delta.x, delta.y);
        squareCenter = { (p0.x + p1.x) * 0.5f,(p0.y + p1.y) * 0.5f };

        ConstraintVisual visual;
        visual.center = squareCenter;
        visual.size = squareSize;
        visual.angle = squareAngle;
        constraintVisuals.push_back(visual);
    }
}


Collision FindCollision(const DirectX::XMFLOAT2& position)
{
    //return Collision{ DirectX::XMFLOAT2(0.0f, 1.0f), -position.y };
    return Collision{ DirectX::XMFLOAT2(0.0f, -1.0f), position.y - 700.0f };
}

Collision FindCollision(const DirectX::XMFLOAT2& position, const Planet& planet)
{
    DirectX::XMFLOAT2 delta = { position.x - planet.center.x, position.y - planet.center.y };
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    DirectX::XMFLOAT2 normal = { delta.x / distance, delta.y / distance };
    float depth = planet.radius - distance;
    if (depth > 0.0f)
    {
        return Collision{ normal, depth };
    }
    return Collision{ DirectX::XMFLOAT2(0.0f, 0.0f), -std::numeric_limits<float>::infinity() };
}

Collision FindCollision(const DirectX::XMFLOAT2& position, const Wall& wall)
{
    float depth = (position.x - wall.position.x) * wall.normal.x + (position.y - wall.position.y) * wall.normal.y;
    if (depth < 0.0f)
    {
        return Collision{ wall.normal, -depth };
    }
    return Collision{ DirectX::XMFLOAT2(0.0f, 0.0f), -std::numeric_limits<float>::infinity() };
}

Collision FindCollision(const DirectX::XMFLOAT2& position, const std::vector<Wall>& walls)
{
    Collision bestCollision{ DirectX::XMFLOAT2(0.0f, 0.0f), -std::numeric_limits<float>::infinity() };
    for (const auto& wall : walls)
    {
        Collision c = FindCollision(position, wall);
        if (c.depth > bestCollision.depth)
        {
            bestCollision = c;
        }
    }
    return bestCollision;
}

Collision FindCollision(const DirectX::XMFLOAT2& position, const std::vector<Planet>& plannets)
{
    Collision result{ DirectX::XMFLOAT2(0.0f, 0.0f), -std::numeric_limits<float>::infinity() };
    for (const auto& planet : plannets)
    {
        Collision collision = FindCollision(position, planet);
        if (collision.depth > result.depth)
        {
            result = collision;
        }
    }
    return result;
}


