// 2次ベジェ曲線の計算
float3 QuadricBezier(float3 p0, float3 p1, float3 p2, float t)
{
    float u = 1.0 - t;
    return u * u * p0 + 2 * u * t * p1 + t * t * p2;
}

// 2次ベジェ曲線の接線
float3 QuadricBezierTangent(float3 p0, float3 p1, float3 p2, float t)
{
    return 2 * ((1 - t) * (p1 - p0) + t * (p2 - p1));
}

// ロドリゲスの回転公式
float3 RotateVector(float3 v, float3 axis, float angle)
{
    float cosA = cos(angle);
    float sinA = sin(angle);
    return cosA * v + (1 - cosA) * dot(axis, v) * axis + sinA * cross(axis, v);
}

float3 RodriguezRotate(float3 v, float3 axis, float cosA, float sinA)
{
    return cosA * v + sinA * cross(axis, v) + (1.0 - cosA) * dot(axis, v) * axis;
}

cbuffer ELASTIC_CONSTANT_BUFFER : register(b6)
{
    float4 p1; // 始点
    float4 p2; // 制御点
    float4 p3; // 終点
    float maxAngleDegree; // 0.0 ~ 1.0  t
    float buildHeight; // ビルの高さ
}

float3 SafeNormalize(float3 v, float3 fallback)
{
    float len = length(v);
    if (len < 1e-5)
    {
        return fallback;
    }
    return v / len;
}
