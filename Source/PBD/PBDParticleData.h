#pragma once

#include <DirectXMath.h>


namespace PBD
{
    using namespace DirectX;

    struct Particle
    {
        XMFLOAT3 position;
        XMFLOAT3 expectedPosition;
        XMFLOAT3 velocity;
        float invMass;
        XMFLOAT3 force;

        Particle() : position(0, 0, 0), expectedPosition(0, 0, 0), velocity(0, 0, 0), invMass(1.0f), force(0, 0, 0)
        {
        }

        void AddForce(const XMFLOAT3& f)
        {
            force.x += f.x;
            force.y += f.y;
            force.z += f.z;
        }

        void ClearForce()
        {
            force = { 0, 0, 0 };
        }

        bool IsStatic() const { return invMass == 0.0f; }
    };

    struct Triangle
    {
        int i1, i2, i3;
    };


}
