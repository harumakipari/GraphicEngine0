#pragma once
#include <DirectXMath.h>
#include <vector>
#include "PBDParticleData.h"

namespace PBD
{
    struct DistanceConstraint
    {
        int i0;
        int i1;

        float restLength;
        float stiffness;    // 0.0f~1.0f 1.0fÇÃÇŸÇ§Ç™çSë©Ç™å≈Ç¢

        DistanceConstraint(int i1, int i2, float length, float k = 1.0f) : i0(i1), i1(i2), restLength(length), stiffness(k) {}

        void Solve(std::vector<Particle>& particles, int iterationCount, float disStiffness) const
        {
            auto& pA = particles[i0];
            auto& pB = particles[i1];

            if (pA.IsStatic() && pB.IsStatic()) return;

            // p4 Figure2
            XMFLOAT3 dir =
            {
                pA.expectedPosition.x - pB.expectedPosition.x,
                pA.expectedPosition.y - pB.expectedPosition.y,
                pA.expectedPosition.z - pB.expectedPosition.z,
            };

            float dist = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (dist < 1e-6f)return;

            // Constraint
            float C = dist - restLength;

            float w1 = pA.invMass;
            float w2 = pB.invMass;

            float invSum = w1 + w2;

            // ê≥ãKâªï˚å¸
            XMFLOAT3 n = { dir.x / dist,dir.y / dist,dir.z / dist };

            // stiffnessÇï‚ê≥Ç∑ÇÈÅ@ò_ï∂Ç…èëÇ¢ÇƒÇΩÅAÅA
            //float kPrime = 1.0f - powf(1.0f - stiffness, 1.0f / iterationCount);
            float kPrime = 1.0f - powf(1.0f - disStiffness, 1.0f / iterationCount);
            //float kPrime = stiffness;


            XMFLOAT3 deltaPA =
            {
                 (-w1 / invSum) * C * n.x,
                 (-w1 / invSum) * C * n.y,
                 (-w1 / invSum) * C * n.z
            };

            XMFLOAT3 deltaPB =
            {
                 (w2 / invSum) * C * n.x,
                 (w2 / invSum) * C * n.y,
                 (w2 / invSum) * C * n.z
            };

            pA.expectedPosition.x += deltaPA.x * kPrime;
            pA.expectedPosition.y += deltaPA.y * kPrime;
            pA.expectedPosition.z += deltaPA.z * kPrime;

            pB.expectedPosition.x += deltaPB.x * kPrime;
            pB.expectedPosition.y += deltaPB.y * kPrime;
            pB.expectedPosition.z += deltaPB.z * kPrime;
            char buf[256];
            float deltaLen = sqrtf(deltaPA.x * deltaPA.x + deltaPA.y * deltaPA.y + deltaPA.z * deltaPA.z);
            sprintf_s(buf, "C=%.6f k'=%f deltaLen=%.6f invSum=%f\n", C, kPrime, deltaLen, invSum);
            //OutputDebugStringA(buf);
        }
    };


    struct BendingConstraint
    {
        int p1, p2, p3, p4;
        float restAngle;
        float stiffness;

        // p1 p2 Ç™ìØÇ∂ï”Çã§óLÇ∑ÇÈéOäpå`
        BendingConstraint(int _p1, int _p2, int _p3, int _p4, float _stiffness = 0.5f) : p1(_p1), p2(_p2), p3(_p3), p4(_p4), stiffness(_stiffness)
        {
            restAngle = 0.0f;
        }

        void Initialize(const std::vector<Particle>& particles)
        {
            using namespace DirectX;

            // èâä˙äpìx É”0 ÇãÅÇﬂÇÈ
            auto& pa = particles[p1];
            auto& pb = particles[p2];
            auto& pc = particles[p3];
            auto& pd = particles[p4];

            XMFLOAT3 n1 = MathHelper::ComputeTriangleNormal(pa.position, pb.position, pc.position);
            XMFLOAT3 n2 = MathHelper::ComputeTriangleNormal(pa.position, pb.position, pd.position);

            float dot = n1.x * n2.x + n1.y * n2.y + n1.z * n2.z;
            dot = std::clamp(dot, -1.0f, 1.0f);

            restAngle = acosf(dot);
        }

        void Solve(std::vector<Particle>& particles, int iterationCount, float bendStiffness) const
        {
            using namespace DirectX;

            auto& pa = particles[p1];
            auto& pb = particles[p2];
            auto& pc = particles[p3];
            auto& pd = particles[p4];

            // ç°ÇÃó\ë™àÍ
            XMVECTOR p1v = XMLoadFloat3(&pa.expectedPosition);
            XMVECTOR p2v = XMLoadFloat3(&pb.expectedPosition);
            XMVECTOR p3v = XMLoadFloat3(&pc.expectedPosition);
            XMVECTOR p4v = XMLoadFloat3(&pd.expectedPosition);

            // Åip2-p1, p3-p1, p4-p1Åj
            XMVECTOR e1 = XMVectorSubtract(p2v, p1v);   // p2-p1
            XMVECTOR e2 = XMVectorSubtract(p3v, p1v);   // p3-p1
            XMVECTOR e3 = XMVectorSubtract(p4v, p1v);   // p4-p1

            // ê∂ÇÃñ@ê¸Åiñ¢ê≥ãKâªÅjÇ∆ê≥ãKâªñ@ê¸
            XMVECTOR cross1 = XMVector3Cross(e1, e2); // (p2-p1) x (p3-p1)
            XMVECTOR cross2 = XMVector3Cross(e1, e3); // (p2-p1) x (p4-p1)

            float normCross1 = XMVectorGetX(XMVector3Length(cross1));
            float normCross2 = XMVectorGetX(XMVector3Length(cross2));
            if (normCross1 < 1e-8f || normCross2 < 1e-8f) return;

            XMVECTOR n1 = XMVectorScale(cross1, 1.0f / normCross1);
            XMVECTOR n2 = XMVectorScale(cross2, 1.0f / normCross2);

            // d = dot(n1,n2)   ç°ÇÃñ@ê¸Ç…ÇÊÇÈäpìx
            float d = XMVectorGetX(XMVector3Dot(n1, n2));
            d = std::clamp(d, -1.0f, 1.0f);

            // sinPhi = |cross(n1,n2)|, phi = atan2(sinPhi, d)
            XMVECTOR nCross = XMVector3Cross(n1, n2);
            float sinPhi = XMVectorGetX(XMVector3Length(nCross));
            if (sinPhi < 1e-7f) return; // ÇŸÇ⁄ïΩíRÇ‹ÇΩÇÕêîílìIÇ…äÎåØ

            float phi = atan2f(sinPhi, d);      // arccos(d)Ç∆ìØã`
            float C = phi - restAngle;
            if (fabsf(C) < 1e-6f) return;

            // --- q ÇÃï™éqÅiò_ï∂ÇÃéÆÇÃì‡ë§Åj ---
            // note: we must divide by |p2 x p3| and |p2 x p4| as in the paper
            // cross1, cross2 are p2Å~p3 and p2Å~p4 (wrt p1), and normCross1/normCross2 are their norms.

            // compute auxiliary cross products used in q's numerator
            XMVECTOR termA3 = XMVector3Cross(e1, n2);                  // p2 x n2
            XMVECTOR termB3 = XMVector3Cross(n1, e1);                  // n1 x p2  (since e1 = p2-p1)

            XMVECTOR termA4 = XMVector3Cross(e1, n1);                  // p2 x n1
            XMVECTOR termB4 = XMVector3Cross(n2, e1);                  // n2 x p2

            // q3 = (p2 Å~ n2 + (n1 Å~ p2) * d) / |p2 Å~ p3|
            XMVECTOR q3 = XMVectorScale(XMVectorAdd(termA3, XMVectorScale(termB3, d)), 1.0f / normCross1);

            // q4 = (p2 Å~ n1 + (n2 Å~ p2) * d) / |p2 Å~ p4|
            XMVECTOR q4 = XMVectorScale(XMVectorAdd(termA4, XMVectorScale(termB4, d)), 1.0f / normCross2);

            // q2 = - ( (p3 Å~ n2 + (n1 Å~ p3) * d) / |p2Å~p3| ) - ( (p4 Å~ n1 + (n2 Å~ p4) * d) / |p2Å~p4| )
            XMVECTOR termA2a = XMVector3Cross(e2, n2); // p3 Å~ n2 (but careful: e2 = p3-p1)
            XMVECTOR termB2a = XMVector3Cross(n1, e2); // n1 Å~ p3
            XMVECTOR q2_part1 = XMVectorScale(XMVectorAdd(termA2a, XMVectorScale(termB2a, d)), 1.0f / normCross1);

            XMVECTOR termA2b = XMVector3Cross(e3, n1); // p4 Å~ n1 (e3 = p4-p1)
            XMVECTOR termB2b = XMVector3Cross(n2, e3); // n2 Å~ p4
            XMVECTOR q2_part2 = XMVectorScale(XMVectorAdd(termA2b, XMVectorScale(termB2b, d)), 1.0f / normCross2);

            // É}ÉCÉiÉXÇÇ¬ÇØÇÈ
            XMVECTOR q2 = XMVectorNegate(XMVectorAdd(q2_part1, q2_part2));

            // q1 = -q2 - q3 - q4
            XMVECTOR q1 = XMVectorNegate(XMVectorAdd(XMVectorAdd(q2, q3), q4));

            // masses
            float w1 = pa.invMass, w2 = pb.invMass, w3 = pc.invMass, w4 = pd.invMass;

            // denom = sum wj * |qj|^2
            float lenq1sq = XMVectorGetX(XMVector3LengthSq(q1));
            float lenq2sq = XMVectorGetX(XMVector3LengthSq(q2));
            float lenq3sq = XMVectorGetX(XMVector3LengthSq(q3));
            float lenq4sq = XMVectorGetX(XMVector3LengthSq(q4));

            float denom = w1 * lenq1sq + w2 * lenq2sq + w3 * lenq3sq + w4 * lenq4sq;
            if (denom < 1e-8f) return;

            float lambda = -C / (sinPhi * denom);
            XMVECTOR dp1 = XMVectorScale(q1, lambda * w1);
            XMVECTOR dp2 = XMVectorScale(q2, lambda * w2);
            XMVECTOR dp3 = XMVectorScale(q3, lambda * w3);
            XMVECTOR dp4 = XMVectorScale(q4, lambda * w4);
#if 0
            // optional: clamp per-particle movement (avoid huge jumps)
            auto clampMove = [](XMVECTOR v, float maxLen) {
                float len = XMVectorGetX(XMVector3Length(v));
                if (len > maxLen) return XMVectorScale(v, maxLen / len);
                return v;
                };

            const float maxMove = 0.5f; // tune: maximum m per iteration
            dp1 = clampMove(dp1, maxMove);
            dp2 = clampMove(dp2, maxMove);
            dp3 = clampMove(dp3, maxMove);
            dp4 = clampMove(dp4, maxMove);

#endif // 0
            //float k = stiffness;
            // stiffnessÇï‚ê≥Ç∑ÇÈÅ@ò_ï∂Ç…èëÇ¢ÇƒÇΩÅAÅA
            float kPrime = 1.0f - powf(1.0f - stiffness, 1.0f / iterationCount);

            // apply
            p1v = XMVectorAdd(p1v, dp1 * kPrime);    // expectedpos+ deltaP1
            p2v = XMVectorAdd(p2v, dp2 * kPrime);
            p3v = XMVectorAdd(p3v, dp3 * kPrime);
            p4v = XMVectorAdd(p4v, dp4 * kPrime);

            XMStoreFloat3(&pa.expectedPosition, p1v);
            XMStoreFloat3(&pb.expectedPosition, p2v);
            XMStoreFloat3(&pc.expectedPosition, p3v);
            XMStoreFloat3(&pd.expectedPosition, p4v);
        }
    };

    // ëÃêœçSë©
    struct VolumeConstraint
    {
        std::vector<int> vertexIndices;
        std::vector<Triangle> triangles;
        float restVolume = 0.0f;    // èâä˙ëÃêœ
        float pressure = 1.0f;

        VolumeConstraint() = default;
        VolumeConstraint(const std::vector<int>& vertices, const std::vector<Triangle>& tris, const std::vector<Particle>& particles, float pressure)
            : vertexIndices(vertices), triangles(tris), pressure(pressure)
        {
            restVolume = ComputeVolume(particles);
            int a = 0;
        }

        float ComputeVolume(const std::vector<Particle>& particles) const
        {
            float volume = 0.0f;
            for (auto& tri : triangles)
            {
                XMVECTOR p1 = XMLoadFloat3(&particles[tri.i1].expectedPosition);
                XMVECTOR p2 = XMLoadFloat3(&particles[tri.i2].expectedPosition);
                XMVECTOR p3 = XMLoadFloat3(&particles[tri.i3].expectedPosition);
                volume += XMVectorGetX(XMVector3Dot(XMVector3Cross(p1, p2), p3));
            }
            return fabs(volume / 6.0f);
        }

        void Solve(std::vector<Particle>& particles, float stiffness) const
        {
            const int N = (int)particles.size();

            // åªç›ÇÃëÃêœ
            float currentVolume = ComputeVolume(particles);
            // (14) çSë©íl
            float C = currentVolume - pressure * restVolume;

            // äeí∏ì_ÇÃå˘îz  (15)
            std::vector<XMFLOAT3> grad(N, { 0,0,0 });
            for (auto& tri : triangles)
            {
                XMVECTOR p1 = XMLoadFloat3(&particles[tri.i1].expectedPosition);
                XMVECTOR p2 = XMLoadFloat3(&particles[tri.i2].expectedPosition);
                XMVECTOR p3 = XMLoadFloat3(&particles[tri.i3].expectedPosition);

                XMVECTOR g1 = XMVector3Cross(p2, p3);
                XMVECTOR g2 = XMVector3Cross(p3, p1);
                XMVECTOR g3 = XMVector3Cross(p1, p2);

                XMStoreFloat3(&grad[tri.i1], XMLoadFloat3(&grad[tri.i1]) + g1);
                XMStoreFloat3(&grad[tri.i2], XMLoadFloat3(&grad[tri.i2]) + g2);
                XMStoreFloat3(&grad[tri.i3], XMLoadFloat3(&grad[tri.i3]) + g3);
            }

            // (8) ÇÃÅ@s ÇÃï™ïÍ
            float denom = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                float w = particles[i].invMass;
                if (w == 0.0f) continue;
                XMVECTOR g = XMLoadFloat3(&grad[i]);
                float len2 = XMVectorGetX(XMVector3LengthSq(g));
                denom += w * len2;
            }
            if (fabs(denom) < 1e-8f) return;
            // (8)
            float s = C / denom;

            // í∏ì_à íuèCê≥
            // (9)
            for (int i = 0; i < N; ++i)
            {
                float w = particles[i].invMass;
                if (w == 0.0f) continue;
                XMVECTOR g = XMLoadFloat3(&grad[i]);
                XMVECTOR delta = -stiffness * s * w * g;    // (9)
                XMVECTOR pos = XMLoadFloat3(&particles[i].expectedPosition);
                XMStoreFloat3(&particles[i].expectedPosition, pos + delta);
            }
#ifdef USE_IMGUI
            ImGui::Begin("imgui");
            ImGui::Text("Rest volume: %.4f", restVolume);
            ImGui::Text("Current volume: %.4f", currentVolume);
            ImGui::Text("C (constraint value): %.4f", C);
            ImGui::Text("Ratio: %.2f %%", (currentVolume / restVolume) * 100.0f);
            ImGui::End();
#endif
        }
    };

    struct CollisionConstraint
    {
        XMFLOAT3 planeNormal;   // ïΩñ ñ@ê¸
        float planeOffset;      // ïΩñ ï˚íˆéÆ nÅEx + d = 0 ÇÃ d Ç…ëäìñ
        float restitution;      // îΩî≠åWêîÅi0Å`1Åj

        CollisionConstraint(const XMFLOAT3& normal, float offset, float restitution = 0.0f)
            : planeNormal(normal), planeOffset(offset), restitution(restitution)
        {
        }

        void Solve(std::vector<Particle>& particles)
        {
            using namespace DirectX;
            XMVECTOR n = XMLoadFloat3(&planeNormal);

            for (auto& p : particles)
            {
                if (p.IsStatic()) continue;

                XMVECTOR pos = XMLoadFloat3(&p.expectedPosition);
                float dist = XMVectorGetX(XMVector3Dot(pos, n)) - planeOffset;

                if (dist < 0.0f)
                {
                    // ïΩñ Çâ∫âÒÇ¡ÇƒÇ¢ÇΩÇÁâüÇµñﬂÇ∑
                    pos -= n * dist;
                    XMStoreFloat3(&p.expectedPosition, pos);

                    // îΩî≠ÇìKópÅiè≠Çµã≠í≤Åj
                    XMVECTOR v = XMLoadFloat3(&p.velocity);
                    float vn = XMVectorGetX(XMVector3Dot(v, n));

                    if (vn < 0.0f) // ì‡å¸Ç´ê¨ï™ÇÃÇ›îΩéÀ
                    {
                        v -= (1.0f + restitution) * vn * n;
                        XMStoreFloat3(&p.velocity, v);
                    }
                }
            }
        }
    };


    struct SelfCollisionConstraint
    {
        float radius; // äeó±éqÇÃîºåaÅiãﬂê⁄îªíËÇ…égópÅj

        SelfCollisionConstraint(float r = 0.05f)
            : radius(r)
        {
        }

        void Solve(std::vector<Particle>& particles)
        {
            using namespace DirectX;

            for (size_t i = 0; i < particles.size(); ++i)
            {
                for (size_t j = i + 1; j < particles.size(); ++j)
                {
                    auto& p1 = particles[i];
                    auto& p2 = particles[j];
                    if (p1.IsStatic() && p2.IsStatic()) continue;

                    XMVECTOR x1 = XMLoadFloat3(&p1.expectedPosition);
                    XMVECTOR x2 = XMLoadFloat3(&p2.expectedPosition);
                    XMVECTOR diff = x1 - x2;
                    float len = XMVectorGetX(XMVector3Length(diff));

                    if (len < 1e-6f) continue;

                    float penetration = (radius * 2.0f) - len;
                    if (penetration > 0.0f)
                    {
                        XMVECTOR dir = diff / len;
                        float w1 = p1.invMass;
                        float w2 = p2.invMass;
                        float wSum = w1 + w2;
                        if (wSum == 0.0f) continue;

                        float ratio1 = w1 / wSum;
                        float ratio2 = w2 / wSum;

                        // âüÇµèoÇµó 
                        XMVECTOR correction = dir * (penetration * 0.5f);

                        if (!p1.IsStatic())
                        {
                            XMVECTOR newPos1 = x1 + correction * ratio1;
                            XMStoreFloat3(&p1.expectedPosition, newPos1);
                        }

                        if (!p2.IsStatic())
                        {
                            XMVECTOR newPos2 = x2 - correction * ratio2;
                            XMStoreFloat3(&p2.expectedPosition, newPos2);
                        }
                    }
                }
            }
        }
    };
}



