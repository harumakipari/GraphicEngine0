#pragma once

#include <vector>

#include "PBDConstaraintData.h"
#include "PBDParticleData.h"

namespace PBD
{
    // どこかにグローバルで置く
    struct PBDParams
    {
        int iterationCount = 5;
        float distanceStiffness = 0.5f;
        float bendingStiffness = 0.5f;
        float volumeStiffness = 1.0f;
        float damping = 0.58f;
        float friction = 0.1f;
        XMFLOAT3 externalForce = { 0.0f, -1.0f, 0.0f }; // 重力
    };


    class System
    {
    public:
        // これVertexの3.1(2)initialize
        void AddParticle(const DirectX::XMFLOAT3& pos, float mass)
        {
            Particle p;
            p.position = pos;
            p.expectedPosition = pos;
            p.invMass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
            particles.push_back(p);
            restPositions.push_back(pos);
        }

        void AddBendingConstraint(int i1, int i2, int i3, int i4, float stiffness)
        {
            BendingConstraint c(i1, i2, i3, i4, stiffness);
            c.Initialize(particles); // 初期角度を計算する
            bendingConstraints.push_back(c);
        }

        void Update(float deltaTime)
        {
            AddForceToVelocity(deltaTime);

            DampVelocities( gPbdParams.damping);

            ExpectedPosition(deltaTime);

            //ShapeMatching(0.1f);

            GenerateCollisionConstraints();

            for (int i = 0; i < gPbdParams.iterationCount; ++i)
            {
                ProjectConstraints();
            }

            CalculateVelocities(deltaTime);

            DampRigidModesPostSolve();

            UpdateVelocity(collisionConstraints);
        }

        void AddDistanceConstraints(int i1, int i2, float stiffness)
        {
            XMFLOAT3 diff =
            {
                particles[i1].position.x - particles[i2].position.x,
                particles[i1].position.y - particles[i2].position.y,
                particles[i1].position.z - particles[i2].position.z,
            };

            float restLength = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
            distanceConstraints.emplace_back(i1, i2, restLength, stiffness);
        }

        void AddVolumeConstraint(const std::vector<int>& vertexIndices, const std::vector<Triangle>& tris, float pressure)
        {
            VolumeConstraint c(vertexIndices, tris, particles, pressure);
            volumeConstraints.push_back(c);
        }

        void AddCollisionPlane(const XMFLOAT3& normal, float offset, float restitution = 0.0f)
        {
            collisionConstraints.emplace_back(normal, offset, restitution);
        }

        void EnableSelfCollision(float radius)
        {
            selfCollision = SelfCollisionConstraint(radius);
            enableSelfCollision = true;
        }

        const std::vector<Particle>& GetParticles() const { return particles; }
        std::vector<Particle>& GetParticles() { return particles; }


        void DebugRender(ID3D11DeviceContext* immediateContext)
        {
            const auto& p = GetParticles();

            // 粒子を描く
            for (const auto& particle : p)
                ShapeRenderer::DrawPoint(immediateContext, particle.position, { 1,0,1,1 });

            // 拘束を描く
            for (const auto& c : distanceConstraints)
            {
                ShapeRenderer::DrawLineSegment(immediateContext, p[c.i0].position, p[c.i1].position, { 1,1,0,1 });
            }

            for (const auto& b : bendingConstraints)
            {
                ShapeRenderer::DrawLineSegment(immediateContext, p[b.p1].position, p[b.p2].position, { 0,1,1,1 });
                ShapeRenderer::DrawLineSegment(immediateContext, p[b.p1].position, p[b.p3].position, { 0,1,1,1 });
                ShapeRenderer::DrawLineSegment(immediateContext, p[b.p1].position, p[b.p4].position, { 0,1,1,1 });
                ShapeRenderer::DrawLineSegment(immediateContext, p[b.p2].position, p[b.p3].position, { 0,1,1,1 });
            }
        }

        void DrawGui()
        {
#ifdef USE_IMGUI
            ImGui::Begin("pbd parameter");
            ImGui::SliderInt("Iterations", &gPbdParams.iterationCount, 1, 20);
            ImGui::SliderFloat("Distance Stiffness", &gPbdParams.distanceStiffness, 0.0f, 1.0f);
            ImGui::SliderFloat("Bending Stiffness", &gPbdParams.bendingStiffness, 0.0f, 1.0f);
            ImGui::SliderFloat("Volume Stiffness", &gPbdParams.volumeStiffness, 0.0f, 5.0f);
            ImGui::SliderFloat("Damping", &gPbdParams.damping, 0.0f, 1.0f);
            ImGui::SliderFloat("Friction", &gPbdParams.friction, 0.0f, 1.0f);
            ImGui::DragFloat3("External Force", &gPbdParams.externalForce.x, 0.001f);
            ImGui::End();
#endif
        }
    private:

        void ShapeMatching(float stiffness)
        {
            using namespace DirectX;

            if (particles.empty()) return;

            // --- (1) 重心を求める ---
            XMVECTOR xcm = XMVectorZero(); // 現在
            XMVECTOR qcm = XMVectorZero(); // 初期
            float totalMass = 0.0f;

            for (size_t i = 0; i < particles.size(); ++i)
            {
                float invM = particles[i].invMass;
                if (invM == 0.0f) continue;
                float m = 1.0f / invM;
                totalMass += m;

                XMVECTOR xi = XMLoadFloat3(&particles[i].expectedPosition);
                XMVECTOR qi = XMLoadFloat3(&restPositions[i]);
                xcm += xi * m;
                qcm += qi * m;
            }

            xcm /= totalMass;
            qcm /= totalMass;

            // --- (2) A_pq 行列を求める ---
            XMFLOAT3X3 Apq = {}; // 3x3 行列
            for (size_t i = 0; i < particles.size(); ++i)
            {
                float invM = particles[i].invMass;
                if (invM == 0.0f) continue;
                float m = 1.0f / invM;

                XMVECTOR xi = XMLoadFloat3(&particles[i].expectedPosition);
                XMVECTOR qi = XMLoadFloat3(&restPositions[i]);

                XMVECTOR ri = xi - xcm;
                XMVECTOR qiRel = qi - qcm;

                XMFLOAT3 r, q;
                XMStoreFloat3(&r, ri);
                XMStoreFloat3(&q, qiRel);

                // outer product r * q^T
                Apq._11 += m * r.x * q.x; Apq._12 += m * r.x * q.y; Apq._13 += m * r.x * q.z;
                Apq._21 += m * r.y * q.x; Apq._22 += m * r.y * q.y; Apq._23 += m * r.y * q.z;
                Apq._31 += m * r.z * q.x; Apq._32 += m * r.z * q.y; Apq._33 += m * r.z * q.z;
            }

            // --- (3) 回転行列 R を求める（極分解 / SVD）
            XMMATRIX R = XMMatrixIdentity();

            // ここでは簡易的に正交化（Gram-Schmidt法）を使用
            {
                XMVECTOR col0 = XMVectorSet(Apq._11, Apq._21, Apq._31, 0.0f);
                XMVECTOR col1 = XMVectorSet(Apq._12, Apq._22, Apq._32, 0.0f);
                XMVECTOR col2 = XMVectorSet(Apq._13, Apq._23, Apq._33, 0.0f);

                col0 = XMVector3Normalize(col0);
                col1 = XMVector3Normalize(col1 - XMVector3Dot(col0, col1) * col0);
                col2 = XMVector3Cross(col0, col1);

                R = XMMATRIX(col0, col1, col2, XMVectorSet(0, 0, 0, 1));
            }

            // --- (4) 新しい目標位置 ---
            for (size_t i = 0; i < particles.size(); ++i)
            {
                if (particles[i].invMass == 0.0f) continue;

                XMVECTOR qi = XMLoadFloat3(&restPositions[i]);
                XMVECTOR qiRel = qi - qcm;
                XMVECTOR goal = XMVector3TransformNormal(qiRel, R) + xcm;

                XMVECTOR xi = XMLoadFloat3(&particles[i].expectedPosition);
                XMVECTOR corrected = XMVectorLerp(xi, goal, stiffness);
                XMStoreFloat3(&particles[i].expectedPosition, corrected);
            }
        }

        // 速度に外力を加える
        void AddForceToVelocity(float deltaTime)
        {
            XMFLOAT3 force = gPbdParams.externalForce;
            for (auto& p : particles)
            {
                if (p.IsStatic()) continue;

                p.AddForce(force);
                p.velocity.x += deltaTime * p.force.x * p.invMass;
                p.velocity.y += deltaTime * p.force.y * p.invMass;
                p.velocity.z += deltaTime * p.force.z * p.invMass;
            }
        }

        //位置予測する前に速度を減衰させる 3.5Damping
        void DampVelocities(float damping)
        {
            using namespace DirectX;

            if (particles.empty()) return;

            float totalMass = 0.0f;
            XMVECTOR xcm = XMVectorZero();
            XMVECTOR vcm = XMVectorZero();

            // (1) 重心位置を求める
            // (2) 重心速度の計算
            for (auto& p : particles)
            {
                if (p.invMass == 0.0f) continue;

                float m = 1.0f / p.invMass;
                totalMass += m;
                XMVECTOR pos = XMLoadFloat3(&p.position);
                XMVECTOR vel = XMLoadFloat3(&p.velocity);
                xcm = XMVectorAdd(xcm, XMVectorScale(pos, m));
                vcm = XMVectorAdd(vcm, XMVectorScale(vel, m));
            }

            xcm = XMVectorScale(xcm, 1.0f / totalMass);
            vcm = XMVectorScale(vcm, 1.0f / totalMass);

            // (3)　L　全体の角運動量　　L = cross(r,p) 位置ベクトル、運動ベクトル
            XMVECTOR L = XMVectorZero();

            // (4) 慣性テンソルを求める　x,y,zどの軸に対しても回転のしにくさを表したいとき
            XMFLOAT3X3 I = {}; // 慣性テンソル
            float I_[3][3] = { 0 };

            for (auto& p : particles)
            {
                if (p.invMass == 0.0f) continue;

                float m = 1.0f / p.invMass;
                XMVECTOR xi = XMLoadFloat3(&p.position);
                XMVECTOR vi = XMLoadFloat3(&p.velocity);
                XMVECTOR ri = XMVectorSubtract(xi, xcm);//ri 点がどれくらい重心と離れているか、

                L = L + XMVector3Cross(ri, XMVectorScale(vi, m)); // 全体の角運動量

                // 慣性テンソル
                XMFLOAT3 r;
                XMStoreFloat3(&r, ri);
                float r2 = r.x * r.x + r.y * r.y + r.z * r.z;

                I_[0][0] += m * (r2 - r.x * r.x);
                I_[1][1] += m * (r2 - r.y * r.y);
                I_[2][2] += m * (r2 - r.z * r.z);
                I_[0][1] -= m * (r.x * r.y);
                I_[0][2] -= m * (r.x * r.z);
                I_[1][0] -= m * (r.y * r.x);
                I_[1][2] -= m * (r.y * r.z);
                I_[2][0] -= m * (r.z * r.x);
                I_[2][1] -= m * (r.z * r.y);

            }

            // (5) 角速度　L=Iwより　w=I^-1*L
            XMMATRIX I_mat = XMLoadFloat3x3(reinterpret_cast<XMFLOAT3X3*>(I_));
            XMVECTOR det;
            XMMATRIX I_inv = XMMatrixInverse(&det, I_mat); // I^-1 逆行列
            if (fabsf(XMVectorGetX(det)) < 1e-6f) return; // 不正な行列回避

            XMVECTOR w = XMVector3TransformNormal(L, I_inv);


            // (6) ~ (9)
            for (auto& p : particles)
            {
                if (p.IsStatic())
                {
                    continue;
                }

                XMVECTOR xi = XMLoadFloat3(&p.position);
                XMVECTOR vi = XMLoadFloat3(&p.velocity);
                XMVECTOR ri = XMVectorSubtract(xi, xcm);//ri 点がどれくらい重心と離れているか

                XMVECTOR deltaVi = vcm + XMVector3Cross(w, ri) - vi; //(7)
                vi = XMVectorAdd(vi, XMVectorScale(deltaVi, damping));
                XMStoreFloat3(&p.velocity, vi); // (8)
            }
        }

        // 明示的オイラー積分で新しい位置を見積もる
        void ExpectedPosition(float deltaTime)
        {
            for (auto& p : particles)
            {
                p.expectedPosition.x = p.position.x + deltaTime * p.velocity.x;
                p.expectedPosition.y = p.position.y + deltaTime * p.velocity.y;
                p.expectedPosition.z = p.position.z + deltaTime * p.velocity.z;

                p.ClearForce();
            }
        }

        // 衝突拘束関数を生成
        void GenerateCollisionConstraints()
        {
            SolveFloorConstraint();
        }

        // 拘束を反映する関数　・・これをsolverIteration分回す
        void ProjectConstraints()
        {

            for (auto& c : bendingConstraints)
            {
                c.Solve(particles, gPbdParams.iterationCount, gPbdParams.bendingStiffness);
            }

            for (auto& v : volumeConstraints)
            {
                v.Solve(particles, gPbdParams.volumeStiffness);
            }

            for (auto& c : distanceConstraints)
            {
                c.Solve(particles, gPbdParams.iterationCount, gPbdParams.distanceStiffness);
            }
            // 地面・平面衝突
            for (auto& c : collisionConstraints)
                c.Solve(particles);

            // 自己衝突
            if (enableSelfCollision)
                selfCollision.Solve(particles);
        }

        // 速度を計算し直す
        void CalculateVelocities(float deltaTime)
        {
            for (auto& p : particles)
            {
                p.velocity.x = (p.expectedPosition.x - p.position.x) / deltaTime;
                p.velocity.y = (p.expectedPosition.y - p.position.y) / deltaTime;
                p.velocity.z = (p.expectedPosition.z - p.position.z) / deltaTime;

                p.position = p.expectedPosition;
            }
        }

        void DampRigidModesPostSolve( float rigidDamping = 0.5f)
        {
            using namespace DirectX;
            const size_t N = particles.size();
            if (N == 0) return;

            // 1) 総質量と重心（固定点を除く）
            float totalMass = 0.0;
            XMVECTOR com = XMVectorZero(); // 
            for (auto& p : particles) {
                if (p.invMass <= 0.0f) continue;
                double m = 1.0 / p.invMass;
                com = XMVectorAdd(com, XMVectorScale(XMLoadFloat3(&p.position), (float)m));
                totalMass += (float)m;
            }
            if (totalMass <= 0.0) return;
            com = XMVectorScale(com, (float)(1.0 / totalMass));

            // 2) 線運動量と角運動量
            XMVECTOR P = XMVectorZero();
            XMVECTOR L = XMVectorZero();
            for (auto& p : particles) {
                if (p.invMass <= 0.0f) continue;
                float m = 1.0f / p.invMass;
                XMVECTOR x = XMLoadFloat3(&p.position);
                XMVECTOR v = XMLoadFloat3(&p.velocity);
                P = XMVectorAdd(P, XMVectorScale(v, m));
                L = XMVectorAdd(L, XMVectorScale(XMVector3Cross(XMVectorSubtract(x, com), v), m));
            }
            XMVECTOR v_cm = XMVectorScale(P, (float)(1.0 / totalMass));

            // 3) 慣性テンソル（3x3）を double で蓄積して float に落とす（正則化）
            double I_[3][3] = { 0 };
            for (auto& p : particles) {
                if (p.invMass <= 0.0f) continue;
                double m = 1.0 / p.invMass;
                XMVECTOR rvec = XMVectorSubtract(XMLoadFloat3(&p.position), com);
                XMFLOAT3 r; XMStoreFloat3(&r, rvec);
                double rx = r.x, ry = r.y, rz = r.z, r2 = rx * rx + ry * ry + rz * rz;
                I_[0][0] += m * (r2 - rx * rx); I_[1][1] += m * (r2 - ry * ry); I_[2][2] += m * (r2 - rz * rz);
                I_[0][1] -= m * (rx * ry); I_[0][2] -= m * (rx * rz);
                I_[1][0] -= m * (ry * rx); I_[1][2] -= m * (ry * rz);
                I_[2][0] -= m * (rz * rx); I_[2][1] -= m * (rz * ry);
            }
            XMFLOAT3X3 I_f;
            I_f._11 = (float)I_[0][0]; I_f._12 = (float)I_[0][1]; I_f._13 = (float)I_[0][2];
            I_f._21 = (float)I_[1][0]; I_f._22 = (float)I_[1][1]; I_f._23 = (float)I_[1][2];
            I_f._31 = (float)I_[2][0]; I_f._32 = (float)I_[2][1]; I_f._33 = (float)I_[2][2];
            XMMATRIX I_mat = XMLoadFloat3x3(&I_f);

            // 正則化（det が小さい or NaN になったら）
            XMVECTOR det = XMMatrixDeterminant(I_mat);
            float detv = XMVectorGetX(det);
            if (!isfinite(detv) || fabsf(detv) < 1e-8f) {
                // diag に小さな値を足して安定化
                const float REG = 1e-3f;
                I_f._11 += REG; I_f._22 += REG; I_f._33 += REG;
                I_mat = XMLoadFloat3x3(&I_f);
                det = XMMatrixDeterminant(I_mat);
                detv = XMVectorGetX(det);
                if (!isfinite(detv) || fabsf(detv) < 1e-12f) {
                    // 完全に特殊なら剛体系抑制をスキップ
                    return;
                }
            }

            XMMATRIX invI = XMMatrixInverse(nullptr, I_mat);
            XMVECTOR omega = XMVector3TransformNormal(L, invI);

            // clamp omega (安定化)
            const float MAX_OMEGA = 30.0f;
            float omLen = XMVectorGetX(XMVector3Length(omega));
            if (omLen > MAX_OMEGA) omega = XMVectorScale(omega, MAX_OMEGA / omLen);

            // 4) 剛体成分 v_rigid = v_cm + omega x r を計算し
            //    各粒子の速度を v <- v + k*(v_rigid - v) で収束させる（k in [0,1]）
            for (auto& p : particles) {
                if (p.invMass <= 0.0f) continue;
                XMVECTOR x = XMLoadFloat3(&p.position);
                XMVECTOR r = XMVectorSubtract(x, com);
                XMVECTOR v_rigid = XMVectorAdd(v_cm, XMVector3Cross(omega, r));
                XMVECTOR v = XMLoadFloat3(&p.velocity);
                XMVECTOR dv = XMVectorSubtract(v_rigid, v);
                v = XMVectorAdd(v, XMVectorScale(dv, rigidDamping)); // rigidDamping: 0..1
                XMStoreFloat3(&p.velocity, v);
            }
        }



        // friction restitution とかに応じて変更
        void UpdateVelocity( const std::vector<CollisionConstraint>& constraints, float friction = 0.5f)
        {
            using namespace DirectX;

            for (auto& p : particles)
            {
                if (p.IsStatic()) continue;

                XMVECTOR v = XMLoadFloat3(&p.velocity);
                XMVECTOR pos = XMLoadFloat3(&p.position);

                // 衝突ごとに速度を修正
                for (const auto& c : constraints)
                {
                    XMVECTOR n = XMLoadFloat3(&c.planeNormal);
                    float d = c.planeOffset;

                    float dist = XMVectorGetX(XMVector3Dot(pos, n)) - d;

                    if (dist < 0.0f)
                    {
                        // 法線方向の速度成分
                        float vn = XMVectorGetX(XMVector3Dot(v, n));

                        if (vn < 0.0f)
                        {
                            // 反発
                            v -= (1.0f + c.restitution) * vn * n;

                            // 接線成分の摩擦
                            XMVECTOR vt = v - XMVector3Dot(v, n) * n;
                            vt = XMVectorScale(vt, 1.0f - gPbdParams.friction); // 摩擦係数で減衰
                            v = XMVector3Dot(v, n) * n + vt;        // 法線+接線
                        }
                    }
                }

                XMStoreFloat3(&p.velocity, v);
            }
        }

    private:
        void SolveFloorConstraint()
        {
            float floorY = 0.0f;
            for (auto& p : particles)
            {
                if (p.expectedPosition.y < floorY)
                {
                    p.expectedPosition.y = floorY;
                }
            }
        }

        std::vector<Particle> particles;
        std::vector<DistanceConstraint> distanceConstraints;
        std::vector<BendingConstraint> bendingConstraints;
        std::vector<VolumeConstraint> volumeConstraints;
        std::vector<CollisionConstraint> collisionConstraints;


        std::vector<XMFLOAT3> restPositions; // 初期の相対位置
        SelfCollisionConstraint selfCollision;
        bool enableSelfCollision = false;
        PBDParams gPbdParams;
        //XMFLOAT3 gravity = { 0.0f,-9.8f,0.0f };
        //int solveIterationCount = 3; // 3 ~ 20
        //float damping = 0.05f;

    };

}
