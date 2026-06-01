#ifndef MATH_HELPER_H
#define MATH_HELPER_H

// C++ 標準ライブラリ
#include <cmath>
#include <cfloat>
#include <random>

// 他ライブラリ
#include <DirectXMath.h>

namespace MathHelper
{
    static DirectX::XMFLOAT3 ConvertRHtoLh(DirectX::XMFLOAT3 v)
    {
        v.x *= -1.0f;
        return v;
    };

    static bool VectorContainsNanOrInfinite(DirectX::FXMVECTOR v)
    {
        DirectX::XMVECTOR isInvalid = DirectX::XMVectorOrInt(DirectX::XMVectorIsNaN(v), DirectX::XMVectorIsInfinite(v));
        return DirectX::XMVector4EqualInt(isInvalid, DirectX::XMVectorTrueInt());
    }

    static bool IsValidQuaternion(const DirectX::XMFLOAT4& q)
    {
        return std::isfinite(q.x) && std::isfinite(q.y) &&
            std::isfinite(q.z) && std::isfinite(q.w);
    }

    inline float ClampAngle(float angle)
    {
        const float PI = 3.14159265f;
        const float TWO_PI = PI * 2.0f;

        angle = std::fmod(angle, TWO_PI);
        if (angle > PI)
        {
            angle -= TWO_PI;
        }
        else if (angle < -PI)
        {
            angle += TWO_PI;
        }
        return angle;
    }

    inline float ClampEulerAngle(float eulerAngle)
    {
        float angle = DirectX::XMConvertToRadians(eulerAngle);
        const float PI = 3.14159265f;
        const float TWO_PI = PI * 2.0f;

        angle = std::fmod(angle, TWO_PI);
        if (angle > PI)
        {
            angle -= TWO_PI;
        }
        else if (angle < -PI)
        {
            angle += TWO_PI;
        }

        return DirectX::XMConvertToDegrees(angle);
    }

    inline float RandomRange(float min, float max)
    {
        if (min >= max)
        {
            std::swap(min, max);
        }

        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(min, max);
        return dist(gen);
    }

    inline float RandomRange(std::mt19937& gen, float min, float max)
    {
        if (min >= max) std::swap(min, max);
        std::uniform_real_distribution<float> dist(min, max);
        return dist(gen);
    }

    inline int RandomRange(int min, int max)
    {
        if (min >= max)
        {
            std::swap(min, max);
        }

        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(min, max);
        return dist(gen);
    }

    enum class RotationSequence
    {
        zyx, zxy, xyz, xzy, yxz, yzx
    };

    // クォータニオンから角度に変更する
    static DirectX::XMFLOAT3 QuaternionToEuler(DirectX::XMFLOAT4 quaternion, RotationSequence rotationSequenceToUse = RotationSequence::yxz)
    {
        int i = 0;
        int j = 0;
        int k = 0;
        // The first action in the function is to reverse the sequence if the rotation is intrinsic. As we only do intrinsic rotations, we have to make sure i,j and k are reversed
        // So if e.g. the sequence is ZXY, i should be 1, j should be 0 and k should be 2 as the sequence to use is yxz. 
        switch (rotationSequenceToUse)
        {
        case RotationSequence::zyx:
            i = 0;
            j = 1;
            k = 2;
            break;
        case RotationSequence::zxy:
            i = 1;
            j = 0;
            k = 2;
            break;
        case RotationSequence::xyz:
            i = 2;
            j = 1;
            k = 0;
            break;
        case RotationSequence::xzy:
            i = 1;
            j = 2;
            k = 0;
            break;
        case RotationSequence::yxz:
            i = 2;
            j = 0;
            k = 1;
            break;
        case RotationSequence::yzx:
            i = 0;
            j = 2;
            k = 1;
            break;
        }

        float sign = (float)static_cast<int>((i - j) * (j - k) * (k - i) / 2);

        float angles[3] = { 0.0f };

        float quat[4] = { quaternion.x, quaternion.y, quaternion.z, quaternion.w };
        float a = quat[3] - quat[j];
        float b = quat[i] + quat[k] * sign;
        float c = quat[j] + quat[3];
        float d = quat[k] * sign - quat[i];

        float n2 = a * a + b * b + c * c + d * d;

        // always not proper as we only support Tait-Bryan angles/rotations

        angles[1] = std::acos((2.0f * (a * a + b * b) / n2) - 1.0f);
        bool safe1 = abs(angles[1]) >= FLT_EPSILON;
        bool safe2 = abs(angles[1] - DirectX::XM_PI) >= FLT_EPSILON;
        if (safe1 && safe2)
        {
            float half_sum = std::atan2(b, a);
            float half_diff = std::atan2(-d, c);

            angles[0] = half_sum + half_diff;
            angles[2] = half_sum - half_diff;
        }
        else
        {
            // always intrinsic as we rotate a camera
            angles[0] = 0.0f;

            if (!safe1)
            {
                float half_sum = std::atan2(b, a);
                angles[2] = 2.0f * half_sum;
            }
            if (!safe2)
            {
                float half_diff = std::atan2(-d, c);
                angles[2] = -2.0f * half_diff;
            }
        }

        for (int index = 0; index < 3; index++)
        {
            if (angles[index] < -DirectX::XM_PI)
            {
                angles[index] += DirectX::XM_2PI;
            }
            else
            {
                if (angles[index] > DirectX::XM_PI)
                {
                    angles[index] -= DirectX::XM_2PI;
                }
            }
        }
        // always not proper as we only support Tait-Bryan angles/rotations
        angles[2] *= sign;
        angles[1] -= DirectX::XM_PIDIV2;
        // reversal, always intrinsic
        float tmp = angles[0];
        angles[0] = angles[2];
        angles[2] = tmp;

        // angle 1 is pitch, angle 0 is yaw and angle 2 is roll... 
        return { ClampAngle(angles[1]), ClampAngle(angles[0]), ClampAngle(angles[2]) };
    }


    // 各成分が 0 以上なら +1.0f、負なら -1.0f となるベクトルを返す関数
    // ビット演算を用いて高速に符号を判定する。
    static DirectX::XMVECTOR VectorSign(DirectX::FXMVECTOR v)
    {
        // 全成分が +1.0f のベクトルを用意する
        DirectX::XMVECTOR one = DirectX::XMVectorSplatOne();

        // 各成分の符号ビットだけを取り出すためのマスク（0x80000000）
        DirectX::XMVECTOR signMask = DirectX::XMVectorSplatSignMask();

        // v の各成分から符号ビットだけを抽出（AND演算でマスク適用）
        DirectX::XMVECTOR signBits = DirectX::XMVectorAndInt(v, signMask);

        // +1.0f（= 0x3F800000）に符号ビットを OR 演算で合成：
        // → 0xBF800000（= -1.0f）になる可能性がある
        // 結果として、v の各成分が負なら -1.0f、正なら +1.0f になる
        return DirectX::XMVectorOrInt(one, signBits);
    }

    // 三点から法線計算
    inline DirectX::XMFLOAT3 ComputeTriangleNormal(const DirectX::XMFLOAT3& p1, const DirectX::XMFLOAT3& p2, const DirectX::XMFLOAT3& p3)
    {
        using namespace DirectX;
        XMVECTOR a = XMLoadFloat3(&p1);
        XMVECTOR b = XMLoadFloat3(&p2);
        XMVECTOR c = XMLoadFloat3(&p3);

        XMVECTOR ab = XMVectorSubtract(b, a);
        XMVECTOR ac = XMVectorSubtract(c, a);

        XMVECTOR n = XMVector3Normalize(XMVector3Cross(ab, ac));

        XMFLOAT3 out;
        XMStoreFloat3(&out, n);
        return out;
    }

    inline DirectX::XMVECTOR QuaternionLookAt(const DirectX::XMVECTOR& Original, const DirectX::XMVECTOR& Target)
    {
        DirectX::XMVECTOR Forward = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(Target, Original));
        DirectX::XMVECTOR Up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        DirectX::XMVECTOR Right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Up, Forward));
        Up = DirectX::XMVector3Cross(Forward, Right);
        DirectX::XMMATRIX Rotation = DirectX::XMMatrixIdentity();
        Rotation.r[0] = Right;
        Rotation.r[1] = Up; Rotation.r[2] = Forward;
        DirectX::XMVECTOR Quaternion = DirectX::XMQuaternionRotationMatrix(Rotation);
        return Quaternion;
    }

    // 線形補間（Vector3）
    inline DirectX::XMFLOAT3 Lerp(
        const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b,
        float t
    )
    {
        // 念のため clamp
        t = std::clamp(t, 0.0f, 1.0f);

        return DirectX::XMFLOAT3(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        );
    }

    // 距離計算（Vector3）
    inline float Distance(
        const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b
    )
    {
        DirectX::XMFLOAT3 va = a;
        DirectX::XMVECTOR Va = DirectX::XMLoadFloat3(&va);

        DirectX::XMFLOAT3 vb = b;
        DirectX::XMVECTOR Vb = DirectX::XMLoadFloat3(&vb);

        float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(Va, Vb)));
        return distance;
    }

    // 距離計算（Vector3）
    inline float DistanceSq(
        const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b
    )
    {
        DirectX::XMFLOAT3 va = a;
        DirectX::XMVECTOR Va = DirectX::XMLoadFloat3(&va);

        DirectX::XMFLOAT3 vb = b;
        DirectX::XMVECTOR Vb = DirectX::XMLoadFloat3(&vb);

        float distanceSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(Va, Vb)));
        return distanceSq;
    }


    // 距離計算（Vector2） a - b
    inline float DistanceFloat2(
        const DirectX::XMFLOAT2& a,
        const DirectX::XMFLOAT2& b
    )
    {
        DirectX::XMVECTOR vecA = DirectX::XMLoadFloat2(&a);
        DirectX::XMVECTOR vecB = DirectX::XMLoadFloat2(&b);
        return DirectX::XMVectorGetX(DirectX::XMVector2Length(DirectX::XMVectorSubtract(vecA, vecB)));
    }

    // 長さ計算（Vector3）
    inline float Length(
        const DirectX::XMFLOAT3& a)
    {
        return DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&a)));
    }

    // 正規化（Vector3）
    inline DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& v)
    {
        using namespace DirectX;

        XMVECTOR vec = XMLoadFloat3(&v);
        vec = XMVector3Normalize(vec);

        XMFLOAT3 out;
        XMStoreFloat3(&out, vec);
        return out;
    }

    // 内積（Vector3）
    inline float Dot(
        const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b
    )
    {
        using namespace DirectX;

        XMVECTOR va = XMLoadFloat3(&a);
        XMVECTOR vb = XMLoadFloat3(&b);

        return XMVectorGetX(XMVector3Dot(va, vb));
    }

    // 引き算 a-b
    inline DirectX::XMFLOAT3 Subtract(
        const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    // 引き算 a-b
    inline DirectX::XMFLOAT2 SubtractFloat2(
        const DirectX::XMFLOAT2& a,
        const DirectX::XMFLOAT2& b)
    {
        DirectX::XMVECTOR va = XMLoadFloat2(&a);
        DirectX::XMVECTOR vb = XMLoadFloat2(&b);

        DirectX::XMVECTOR vec = DirectX::XMVectorSubtract(va, vb);
        DirectX::XMFLOAT2 ans;
        DirectX::XMStoreFloat2(&ans, vec);
        return ans;
    }

    // 掛け算
    inline DirectX::XMFLOAT3 Multiply(
        const DirectX::XMFLOAT3& a,
        const float& scale)
    {
        DirectX::XMVECTOR va = XMLoadFloat3(&a);
        va = DirectX::XMVectorScale(va, scale);
        DirectX::XMFLOAT3 ans;
        DirectX::XMStoreFloat3(&ans, va);
        return ans;
    }

    // 掛け算
    inline DirectX::XMFLOAT3 MultiplyF3XF3(
        const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b)
    {
        DirectX::XMFLOAT3 ans = { 0.0f,0.0f,0.0f };
        ans.x = a.x * b.x;
        ans.y = a.y * b.y;
        ans.z = a.z * b.z;
        return ans;
    }



    // 加算 a+b
    inline DirectX::XMFLOAT3 Add(
        const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b)
    {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }


    inline DirectX::XMVECTOR Load(const DirectX::XMFLOAT3& v)
    {
        return DirectX::XMLoadFloat3(&v);
    }

    inline DirectX::XMFLOAT3 StoreFloat3(DirectX::XMVECTOR v)
    {
        DirectX::XMFLOAT3 out;
        DirectX::XMStoreFloat3(&out, v);
        return out;
    }

    inline DirectX::XMFLOAT4 StoreFloat4(DirectX::XMVECTOR v)
    {
        DirectX::XMFLOAT4 out;
        DirectX::XMStoreFloat4(&out, v);
        return out;
    }

    inline DirectX::XMFLOAT3 ConvertQuaternion4X4ToEuler(const DirectX::XMFLOAT4X4& rotation)
    {
        //	ZXY回転
        DirectX::XMFLOAT3 euler;
        if (1.0f - fabs(rotation.m[2][1]) < 1.0e-6f)
        {
            euler.x = rotation.m[2][1] < 0 ? DirectX::XM_PIDIV2 : -DirectX::XM_PIDIV2;
            euler.y = atan2f(rotation.m[1][0], rotation.m[0][0]);
            euler.z = 0;
        }
        else
        {
            euler.x = asinf(-rotation.m[2][1]);
            euler.y = atan2f(rotation.m[2][0], rotation.m[2][2]);
            euler.z = atan2f(rotation.m[0][1], rotation.m[1][1]);
        }
        return euler;
    }

    inline DirectX::XMFLOAT3 ConvertQuaternionToEuler(const DirectX::XMFLOAT4 quaternion)
    {
        DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&quaternion));
        DirectX::XMFLOAT4X4 rotation;
        DirectX::XMStoreFloat4x4(&rotation, Rotation);
        return ConvertQuaternion4X4ToEuler(rotation);
    }

}

#endif //MATH_HELPER_H
