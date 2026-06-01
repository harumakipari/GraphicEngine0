#include "pch.h"
#include "Collider.h"
#include <cfloat>

float Dot(const Point& a, const Point& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Dot(const Vector& a, const Vector& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

//Clamp n to lie within the range [min,max]
float Clamp(float n, float min, float max)
{
    if (n < min) return min;
    if (n > max) return max;
    return n;
}

bool CollideSphereVsSphere(Sphere a, Sphere b)
{
    //Calculate squared distance between centers
    Vector d = a.c - b.c;
    float dist2 = Dot(d, d);
    //Spheres intersect if squared distance is less than squared sum of radii
    float radiusSum = a.r + b.r;
    return dist2 <= radiusSum * radiusSum;
}

bool ColliderAABBVsAABB(AABB a, AABB b)
{
# if 0
    int r;
    r = a.r[0] + b.r[0];
    if ((unsigned int)(a.c.x - b.c.x + r) > r + r)
    {
        return false;
    }
    r = a.r[1] + b.r[1];
    if ((unsigned int)(a.c.y - b.c.y + r) > r + r)
    {
        return false;
    }
    r = a.r[2] + b.r[2];
    if ((unsigned int)(a.c.z - b.c.z + r) > r + r)
    {
        return false;
    }
    return true;
#else
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
        (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
        (a.min.z <= b.max.z && a.max.z >= b.min.z);

#endif
}

// Return true if sphere s intersects AABB b, false otherwise
bool CollideSphereVsAABB(Sphere a, AABB aabb)
{
    // Compute squard distance between sphere center and AABB
    return 1;
}


bool CollideSphereVsCapsule(Sphere s, Capsule capsule)
{
    // Compute (squared) distance between sphere center and capsule line segment
    float dist2 = SqDistPointVsSegment(capsule.a, capsule.b, s.c);
    // if (squared) distance smaller than (squared) sum of radii, they collide
    float radius = s.r + capsule.r;
    return dist2 <= radius + radius;
}

bool CollideCapsuleVsCapsule(Capsule capsule1, Capsule capsule2)
{
    // Compute (squared) distance between the inner structures of the capsules
    float s, t;
    Point c1, c2;
    float dist2 = ClosestPtSegmentVsSegment(capsule1.a, capsule1.b, capsule2.a, capsule2.b, s, t, c1, c2);
    // If (squared) distance smaller than (squared) sum of radii, they collide
    float radius = capsule1.r + capsule2.r;
    return dist2 <= radius * radius;
}

bool CollideSphereVsSegment(Sphere s, Point a, Point b)
{
    float dist2 = SqDistPointVsSegment(a, b, s.c);
    float radius2 = s.r * s.r;
    return dist2 <= radius2;
}

bool CollideCapsuleVsSegment(Capsule capsule, Point a, Point b)
{
    float s, t;
    Point c1, c2;
    float dist2 = ClosestPtSegmentVsSegment(a, b, capsule.a, capsule.b, s, t, c1, c2);

    float radius2 = capsule.r * capsule.r;
    return dist2 <= radius2;
}

// Returns the squared distance between point c and segment ab
float SqDistPointVsSegment(Point a, Point b, Point c)
{
    Vector ab = b - a, ac = c - a, bc = c - b;
    float e = Dot(ac, ab);
    // Handle cases where c projects outside ab
    // 内積の結果で二つの矢印の向きがわかるからそれを利用して　マイナスなら c は a の後ろ側にあるから最近点は a になる
    if (e <= 0.0f) return Dot(ac, ac);

    float f = Dot(ab, ab);
    if (e >= f) return Dot(bc, bc);
    // Handle cases where c projects onto ab
    return Dot(ac, ac) - e * e / f;
}

//2本の線分（セグメント）間の最も近い2点 を計算し、その2点間の距離の2乗を返す
//また、その最も近い2点が線分上のどこにあるかを示すパラメータ s と t も計算
// Computes closest points C1 and C2 of S1(s)=P1*s*(Q1-P1) and S2(t)=P2+t*(Q2-P2),returning s and t. 
// Finction result is squared distance between S1(s) and S2(t)
float ClosestPtSegmentVsSegment(Point p1, Point q1, Point p2, Point q2, float& s, float& t, Point& c1, Point& c2)
{
    Vector d1 = q1 - p1;// Direction vector of segmeny S1
    Vector d2 = q2 - p2;// Direction vector of segmeny S2
    Vector r = p1 - p2;
    float a = Dot(d1, d1);// Squared length of segment S1, always nonnegative
    float e = Dot(d2, d2);// Squared length of segment S2, always nonnegative
    float f = Dot(d2, r);
    //Check if either or both segments degenerate into points
    if (a <= FLT_EPSILON && e <= FLT_EPSILON)
    {// Both segments degenerate into points    両方点
        s = t = 0.0f;
        c1 = p1;
        c2 = p2;
        return Dot(c1 - c2, c1 - c2);
    }
    if (a <= FLT_EPSILON)
    {// First segment degenerates into a point  S1が点
        s = 0.0f;
        t = f / e; // s = 0 => t = (b*s + f) / e = f / e
        t = Clamp(t, 0.0f, 1.0f);
    }
    else
    {
        float c = Dot(d1, r);
        if (e <= FLT_EPSILON)
        {// Second segment degenerates into a point  S2が点
            t = 0.0f;
            s = Clamp(-c / a, 0.0f, 1.0f); // t = 0 => s = (b*t - c) / a = -c / a
        }
        else
        {// The general nondegenerate case starts here
            float b = Dot(d1, d2);
            float denom = a * e - b * b; //Always nonnegative

            // If segments not parallel, compute closest point on L1 to L2 and clamp to segment S1.
            // Else pick arbitrary s (here 0)
            if (denom != 0.0f)
            {//平行じゃないとき　最近点を求める
                s = Clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            }
            else s = 0.0f;
            // Compute point on L2 closest to S1(s) using 
            // t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
            t = (b * s + f) / e;

            //If t in [0,1] done. Else clamp t, recompute s for the new value of t using 
            // s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a and clamp s to [0, 1]
            if (t < 0.0f)
            {
                t = 0.0f;
                s = Clamp(-c / a, 0.0f, 1.0f);
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = Clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
    return Dot(c1 - c2, c1 - c2);
};