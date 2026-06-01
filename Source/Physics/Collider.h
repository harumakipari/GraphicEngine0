#pragma once
#include <DirectXMath.h>

//#include "Math/MathHelper.h"

struct Vector
{
    float x;
    float y;
    float z;

    Vector() = default;
    //Point から Vector への変換用コンストラクタ
    Vector(float x, float y, float z) :x(x), y(y), z(z) {}

    Vector operator*(float scalar) const
    {
        return Vector(x * scalar, y * scalar, z * scalar);
    }
};

struct Point
{
    float x;
    float y;
    float z;

    Point() = default;

    //XMFLOAT3から変換するためのコンストラクタ
    Point(const DirectX::XMFLOAT3& f) :x(f.x), y(f.y), z(f.z) {}
    Point(float x, float y, float z) :x(x), y(y), z(z) {}

    Point& operator=(const Point& other)
    {
        if (this != &other)
        {
            x = other.x;
            y = other.y;
            z = other.z;
        }
        return *this;
    }

    //Point - Point の結果を Vector で返す
    Vector operator-(const Point& org)const
    {
        return Vector(x - org.x, y - org.y, z - org.z);
    }

    Point operator+(const Vector& org)const
    {
        return Point(x + org.x, y + org.y, z + org.z);
    }

    // ToXMFLOAT3
    DirectX::XMFLOAT3 ToXMFLOAT3()const
    {
        return DirectX::XMFLOAT3(x, y, z);
    }

    operator DirectX::XMFLOAT3() const
    {
        return DirectX::XMFLOAT3(x, y, z);
    }
};


struct Sphere
{
    Point c; //Sphere Center
    float r; //Sphere radius
};

struct Capsule
{
    Point a;//Medial line segmeny start point
    Point b;//Medial line segmeny end point
    float r;//radius
};


struct Cone
{
    Point v;    //頂点位置
    Vector d; //方向ベクトル（軸）
    float theta;   //開き角
    float h;    //高さ
    float r;    //底面
};


struct AABB
{
    DirectX::XMFLOAT3 min;
    DirectX::XMFLOAT3 max;

    // AABB の8頂点を取得
    void GetCorners(DirectX::XMFLOAT3 outCorners[8]) const
    {
        outCorners[0] = { min.x, min.y, min.z };
        outCorners[1] = { max.x, min.y, min.z };
        outCorners[2] = { min.x, max.y, min.z };
        outCorners[3] = { max.x, max.y, min.z };
        outCorners[4] = { min.x, min.y, max.z };
        outCorners[5] = { max.x, min.y, max.z };
        outCorners[6] = { min.x, max.y, max.z };
        outCorners[7] = { max.x, max.y, max.z };
    }

    //bool operator==(const AABB& other) const
    //{
    //    return MathHelper::AlmostEqualRelative(min.x, other.min.x)
    //        && MathHelper::AlmostEqualRelative(min.y, other.min.y)
    //        && MathHelper::AlmostEqualRelative(min.z, other.min.z)
    //        && MathHelper::AlmostEqualRelative(max.x, other.max.x)
    //        && MathHelper::AlmostEqualRelative(max.y, other.max.y)
    //        && MathHelper::AlmostEqualRelative(max.z, other.max.z);
    //}
    bool operator==(const AABB& o) const noexcept 
    {
        return min.x == o.min.x && min.y == o.min.y && min.z == o.min.z
            && max.x == o.max.x && max.y == o.max.y && max.z == o.max.z;
    }
};

bool ColliderAABBVsAABB(AABB a, AABB b);

bool CollideSphereVsSphere(Sphere a, Sphere b);

bool CollideSphereVsAABB(Sphere a, AABB aabb);

bool CollideSphereVsCapsule(Sphere s, Capsule capsule);

bool CollideCapsuleVsCapsule(Capsule capsule1, Capsule capsule2);

bool CollideSphereVsSegment(Sphere s, Point a, Point b);

bool CollideCapsuleVsSegment(Capsule capsule, Point a, Point b);

// Returns the squared distance between point c and segment ab
float SqDistPointVsSegment(Point a, Point b, Point c);

//2本の線分（セグメント）間の最も近い2点 を計算し、その2点間の距離の2乗を返す
//また、その最も近い2点が線分上のどこにあるかを示すパラメータ s と t も計算
// Finction result is squared distance between S1(s) and S2(t)
float ClosestPtSegmentVsSegment(Point p1, Point q1, Point p2, Point q2, float& s, float& t, Point& c1, Point& c2);