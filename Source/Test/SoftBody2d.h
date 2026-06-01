#pragma once

#include <DirectXMath.h>
#include <vector>
#include <limits>
#include <cmath>
#include <memory>
#include "Graphics/Sprite/Sprite.h"
#include "Graphics/Core/Graphics.h"
#include "Math/MathHelper.h"
struct PointMass2d
{
    PointMass2d() : position{ 0.0f, 0.0f }, velocity{ 0.0f, 0.0f }
    {
        sprite = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/circle.png");
    }
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 velocity;
    std::shared_ptr<Sprite> sprite;
    float radius = 15.0f;
    DirectX::XMFLOAT4 color = { 1.0f,1.0f,1.0f,1.0f };
};

struct Planet
{
    DirectX::XMFLOAT2 center;
    float radius;
};

struct Wall
{
    DirectX::XMFLOAT2 normal;
    DirectX::XMFLOAT2 position;
};

// ãóó£êßñÒ
struct DistConstraint
{
    uint32_t index0, index1;
    float distance;
};

struct ConstraintVisual
{
    DirectX::XMFLOAT2 center;
    float angle;
    float size; 
};

struct SoftBody
{
    struct Vertex
    {
        uint32_t index;
        DirectX::XMFLOAT2 position; // èâä˙èÛë‘Ç≈ÇÃíÜêSÇ©ÇÁÇÃëäëŒà íu
    };
    std::vector<Vertex> vertices;
};

struct Engine
{
    std::vector<DistConstraint> constraints;
    std::vector<PointMass2d> points;
    std::vector<Wall> walls;
    DirectX::XMFLOAT2 gravity = { 0.0f, 98.0f };
    int iterations = 4;
    float speed = 500.0f;
    std::shared_ptr<Sprite> sprite;
    float radius = 15.0f;

    std::vector<ConstraintVisual> constraintVisuals;

    Engine()
    {
        for (int i = 0; i < iterations; i++)
        {
            PointMass2d point;
            point.position = { 1280.0f * 0.5f,720.0f * 0.5f };
            point.velocity = { MathHelper::RandomRange(-speed,speed),MathHelper::RandomRange(-speed,speed) };
            points.push_back(point);
        }

        Wall right{ { -1.0f, 0.0f }, { 1280.0f - radius, 0.0f } };
        walls.push_back(right);
        Wall left{ { 1.0f, 0.0f }, { radius, 0.0f } };
        walls.push_back(left);
        Wall top{ { 0.0f, 1.0f }, { radius, 0.0f } };
        walls.push_back(top);
        Wall bottom{ { 0.0f, -1.0f }, { 0.0f, 720.0f - radius} };
        walls.push_back(bottom);

        // êßñÒí«â¡
        DistConstraint constrain{ 0,1,400.0f };
        constraints.push_back(constrain);
        DistConstraint constrain1{ 0,2,300.0f };
        constraints.push_back(constrain1);
        DistConstraint constrain2{ 1,2,500.0f };
        constraints.push_back(constrain2);
        DistConstraint constrain3{ 0,3,500.0f };
        constraints.push_back(constrain3);
        DistConstraint constrain4{ 1,3,300.0f };
        constraints.push_back(constrain4);
        DistConstraint constrain5{ 2,3,400.0f };
        constraints.push_back(constrain5);

        SoftBody softBody;
        softBody.vertices.push_back({ 0,{ -200.0f,150.0f} });
        softBody.vertices.push_back({ 1,{ 200.0f,150.0f} });
        softBody.vertices.push_back({ 2,{ -200.0f,-150.0f} });
        softBody.vertices.push_back({ 3,{ 200.0f,-150.0f} });
        softBodies.push_back(softBody);

        sprite = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/square.png");
    }

    void Update(float deltaTime);

    void Render(ID3D11DeviceContext* immediateContext)
    {
#if 1
        for (const auto& visual : constraintVisuals)
        {
            sprite->Render(immediateContext, visual.center.x, visual.center.y, radius, visual.size,
                { 1.0f,1.0f,1.0f,1.0f }, DirectX::XMConvertToDegrees(visual.angle), 0.0f, 0.0f, 100.0f, 100.0f, 1.0f, 1.0f, 0.5f, 0.5f);
        }
        //sprite->Render(immediateContext, squareCenter.x, squareCenter.y, radius, squareSize,
        //    { 1.0f,1.0f,1.0f,1.0f }, DirectX::XMConvertToDegrees(squareAngle), 0.0f, 0.0f, 100.0f, 100.0f, 1.0f, 1.0f, 0.5f, 0.5f);

        for (const auto& point : points)
        {
            if (point.sprite)
            {
                point.sprite->Render(immediateContext, point.position.x, point.position.y, point.radius * 2.0f, point.radius * 2.0f, { 0.5f,0.5f },
                    point.color, 0.0f, 0.0f, 100.0f, 100.0f);
                //point.sprite->Render(immediateContext, point.position.x, point.position.y, point.radius * 2.0f, point.radius * 2.0f,
                //    point.color, 0.0f, 0.0f, 0.0f, 100.0f, 100.0f, 1.0f, 1.0f, point.radius, point.radius);
            }
        }
//#else
        sprite->SoftRender(immediateContext, { points[0].position }, points[1].position, points[2].position, points[3].position, 0.0f, 0.0f, 400.0f, 300.0f, 0.0f, 1.0f, 0.0f, 0.5f);
#endif // 1
    }

private:
    void HardConstraints(float deltaTime);
    void SoftConstraints(float deltaTime);
    void ForceBasedConstraints(float deltaTime);   

    float squareAngle;
    DirectX::XMFLOAT2 squareCenter;
    float squareSize = 400.0f;

    std::vector<SoftBody> softBodies;
};


struct Collision
{
    DirectX::XMFLOAT2 normal;
    float depth = -std::numeric_limits<float>::infinity();  // float å^Ç…Ç®ÇØÇÈç≈è¨íl

};

Collision FindCollision(const DirectX::XMFLOAT2& position);


Collision FindCollision(const DirectX::XMFLOAT2& position, const Wall& wall);

Collision FindCollision(const DirectX::XMFLOAT2& position, const std::vector<Wall>& walls);

Collision FindCollision(const DirectX::XMFLOAT2& position, const Planet& planet);

Collision FindCollision(const DirectX::XMFLOAT2& position, const std::vector<Planet>& plannets);