#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include <DirectXMath.h>
#include <PxPhysicsAPI.h>
#include <PxShape.h>
#include <geometry/PxConvexMesh.h>

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

#include "Physics/Physics.h"
#include "Physics/PhysicsHelper.h"
#include "Components/Transform/Transform.h"
#include "Graphics/Resource/InterleavedGltfModel.h"
#include "Components/Render/MeshComponent.h"
#include "Components/Base/Component.h"

class CollisionComponent;
class ShapeComponent;
class Actor;

// XMFLOAT3 用ハッシュ＆比較
struct XMFLOAT3Hasher
{
    size_t operator()(const DirectX::XMFLOAT3& v)const
    {
        // ハッシュ関数
        size_t hx = std::hash<float>()(v.x);
        size_t hy = std::hash<float>()(v.y);
        size_t hz = std::hash<float>()(v.z);
        return hx ^ (hy << 1) ^ (hz << 2);
    }
};

struct XMFLOAT3Equal
{
    bool operator()(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) const {
        constexpr float EPS = 1e-6f;
        return
            fabs(a.x - b.x) < EPS &&
            fabs(a.y - b.y) < EPS &&
            fabs(a.z - b.z) < EPS;
    }
};

// ShapeComponent に アタッチする用の component
class RigidBodyComponent : public Component
{
public:
    RigidBodyComponent(const std::string& name, const std::shared_ptr<Actor>& owner) : Component(name, owner) {}

    ~RigidBodyComponent() override = default;

    void Initialize() override {};

    void UpdateComponentToWorld(UpdateTransformFlags update_transform_flags = UpdateTransformFlags::None) override {}

    virtual void Initialize(physx::PxPhysics* physics) = 0;

    void Tick(float deltaTime) override = 0;

    virtual void AddToScene(physx::PxScene* scene) = 0;

    virtual void SetTransform(const Transform& worldTransform) = 0;

    virtual void SetCollisionFilter(uint32_t layer, uint32_t mask) = 0;

    virtual void SetGravity(bool useGravity) {}

    virtual void SetKinematic(bool isKinematic) {}

    void Destroy() override = 0;

protected:
    uint32_t layer = 0xFFFFFFFF;
    uint32_t mask = 0xFFFFFFFF;
};


class SingleRigidBodyComponent :public RigidBodyComponent
{
public:
    SingleRigidBodyComponent(const std::string& name, const std::shared_ptr<Actor>& owner, ShapeComponent* shape)
        : RigidBodyComponent(name, owner), shapeComponent(shape), material(Physics::Instance().GetMaterial(PhysicsMaterialType::Default)) {}

    void Initialize(physx::PxPhysics* physics) override;

    void Tick(float deltaTime) override;

    void AddToScene(physx::PxScene* scene) override
    {
        if (pxActor)
        {
            scene->addActor(*pxActor);
            isInScene = true;
        }
    }

    void SetTransform(const Transform& worldTransform) override
    {
        if (pxActor)
        {
            pxActor->setGlobalPose(PhysicsHelper::ToPxTransform(worldTransform));
        }
    }

    void SetCollisionFilter(uint32_t layer, uint32_t mask) override
    {
        layer = layer;
        mask = mask;
        if (pxShape)
        {
            physx::PxFilterData filterData(layer, mask, 0, 0);
            pxShape->setSimulationFilterData(filterData);
            pxShape->setQueryFilterData(filterData);
        }
    }

    void SetGravity(bool isUseGravity)override
    {
        if (pxActor)
        {
            pxActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !isUseGravity);
        }
    }

    void SetIntialVelocity(const DirectX::XMFLOAT3& velocity) const
    {
        if (pxActor)
        {
            pxActor->setLinearVelocity(physx::PxVec3(velocity.x, velocity.y, velocity.z));
        }
    }

    void SetInitialAngularVelocity(const DirectX::XMFLOAT3& angularVelocity) const
    {
        if (pxActor)
            pxActor->setAngularVelocity(physx::PxVec3(angularVelocity.x, angularVelocity.y, angularVelocity.z));
    }

    void SetLinearDamping(float damping) const
    {
        if (pxActor)
            pxActor->setLinearDamping((std::max)(0.0f, damping));
    }

    void SetAngularDamping(float damping) const
    {
        if (pxActor)
            pxActor->setAngularDamping((std::max)(0.0f, damping));
    }

    void SetSleepThreshold(float threshold) const
    {
        if (pxActor)
            pxActor->setSleepThreshold((std::max)(0.0f, threshold));
    }

    void SetMass(float newMass)
    {
        if (pxActor)
            physx::PxRigidBodyExt::updateMassAndInertia(*pxActor, newMass);
        mass = newMass;
    }

    // 当たり判定を無効にする関数
    void DisableCollision() const
    {
        if (!pxShape)
        {
            return;
        }
        pxShape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        pxShape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, false);
    }

    // 当たり判定を有効にする関数
    void EnableCollision() const
    {
        if (!pxShape)
        {
            return;
        }
        pxShape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
        pxShape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, true);
    }

    // 動的に球の当たり判定の半径を大きくする関数
    void ResizeSphere(float newRadius);

    // 動的にカプセルの判定を大きくする関数
    void ResizeCapsule(float newRadius, float newHeight);

    // 動的にボックスの判定を大きくする関数
    void ResizeBox(float newExtentX, float newExtentY, float newExtentZ);

    void SetKinematic(bool isKinematic)override
    {
        if (pxActor)
        {
            pxActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, isKinematic);
        }
        isKinematic = isKinematic;
    }

    void SetTrigger(bool isTrigger)
    {
        isTrigger = isTrigger;
    }

    void Destroy() override
    {
        if (pxActor && isInScene)
        {
            Physics::Instance().GetScene()->removeActor(*pxActor);
            isInScene = false;
        }

        isDestroyed = true;
    }

    void AddImpulse(const DirectX::XMFLOAT3& impulse) const
    {
        using namespace physx;
        if (!isKinematic)
        {// ダイナミックの時
            if (pxActor)
            {
                PxVec3 impulsePx(impulse.x, impulse.y, impulse.z);

                // 瞬間的な速度変化を与える
                pxActor->addForce(impulsePx, PxForceMode::eIMPULSE);
            }
        }
    }

    void SetMaterial(PhysicsMaterialType type) const
    {
        if (!pxShape) return;

        physx::PxMaterial* mat =
            Physics::Instance().GetMaterial(type);

        pxShape->setMaterials(&mat, 1);
    }


private:
    physx::PxRigidDynamic* pxActor = nullptr;
    physx::PxShape* pxShape = nullptr;
    ShapeComponent* shapeComponent = nullptr;
    physx::PxMaterial* material = nullptr;

    float mass = 1.0f;
    bool isDestroyed = false;
    bool isInScene = false;
    bool isKinematic = true;
    bool isTrigger = false;

    // 動的に半径を変更した時に position を足すため
    float currentRadius = 0.0f;
};


// node 毎に rigidBody (ConvexMesh)　を作成する時 
class MultiRigidBodyComponent :public RigidBodyComponent
{
public:
    MultiRigidBodyComponent(const std::string& name, const std::shared_ptr<Actor>& owner, MeshComponent* meshComponent, CollisionComponent* collisionComponent)
        : RigidBodyComponent(name, owner), meshComponent(meshComponent), collisionComponent(collisionComponent) {}

    void Initialize(physx::PxPhysics* physics)override;

    void Tick(float deltaTime)override;

    void AddToScene(physx::PxScene* scene) override
    {
        if (isAddedToScene)
        {
            return;
        }
        for (auto* pxBody : rigidBodies)
        {
            scene->addActor(*pxBody);
        }
        isAddedToScene = true;
    }

    void SetTransform(const Transform& worldTransform) override {}

    void SetCollisionFilter(uint32_t layer, uint32_t mask) override
    {
        this->layer = layer;
        this->mask = mask;
    }

    const std::vector<InterleavedGltfModel::Node>& GetAnimatedNodes() const
    {
        return animatedNodes;
    }

    void SetKinematic(bool isKinematic) override
    {
        for (physx::PxRigidActor* actor : rigidBodies)
        {
            // RigidDynamic のみが Kinematic を設定可能
            if (physx::PxRigidDynamic* dynActor = actor->is<physx::PxRigidDynamic>())
            {
                dynActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, isKinematic);
                if (isKinematic)
                {
                    dynActor->setLinearVelocity({ 0, 0, 0 });
                    dynActor->setAngularVelocity({ 0, 0, 0 });
                }
                else
                {
                    dynActor->wakeUp();
                }
            }
        }
    }

    bool IsKinematic(size_t index) const
    {
        if (index >= rigidBodies.size())
        {
            return false;
        }
        auto* dyn = rigidBodies[index]->is<physx::PxRigidDynamic>();
        return dyn ? dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC : false;
    }

    // 当たり判定を無効にする関数
    void DisableCollision() const
    {
        for (physx::PxShape* pxShape : pxShapes)
        {
            if (pxShape)
            {
                pxShape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
                pxShape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, false);
            }
        }
    }


    void Destroy() override;

private:
    void ComputeGlobalTransformRecursive(std::vector<InterleavedGltfModel::Node>& nodes, int nodeIndex, const DirectX::XMFLOAT4X4& parentGlobal)
    {
        InterleavedGltfModel::Node& node = nodes[nodeIndex];

        // ローカル行列 SRT
        DirectX::XMMATRIX S = DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f);
        DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
        DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z);
        DirectX::XMMATRIX localMat = S * R * T;

        // 親行列 * ローカル行列 で自分のグローバル行列
        DirectX::XMMATRIX parentMat = DirectX::XMLoadFloat4x4(&parentGlobal);
        DirectX::XMMATRIX globalMat = parentMat * localMat;
        DirectX::XMStoreFloat4x4(&node.globalTransform, globalMat);

        // 子ノードも同様に処理
        for (int childIdx : node.children)
        {
            ComputeGlobalTransformRecursive(nodes, childIdx, node.globalTransform);
        }
    }

    void ComputeGlobalTransforms(std::vector<InterleavedGltfModel::Node>& nodes)
    {
        // ルートを探す
        std::vector<bool> hasParent(nodes.size(), false);
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            for (int c : nodes[i].children)
            {
                hasParent[c] = true;
            }
        }
        // Identity 行列を親として、各ルートから再帰
        DirectX::XMFLOAT4X4 identity;
        DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            if (!hasParent[i])
            {
                ComputeGlobalTransformRecursive(nodes, int(i), identity);
            }
        }
    }

    // mesh 一つから vertices を作る
    std::vector<DirectX::XMFLOAT3> ReturnPhysxVertices(const InterleavedGltfModel::Mesh& mesh)
    {
        std::unordered_set<DirectX::XMFLOAT3, XMFLOAT3Hasher, XMFLOAT3Equal> uniquePositions;

        for (auto& primitive : mesh.primitives)
        {
            for (auto& vertex : primitive.cachedVertices)
            {
                DirectX::XMFLOAT3 scaledPos =
                {
                    vertex.position.x,
                    vertex.position.y,
                    vertex.position.z,
                };

                uniquePositions.insert(scaledPos);
            }
        }

        // vector にコピーして返す
        return std::vector<DirectX::XMFLOAT3>(uniquePositions.begin(), uniquePositions.end());

    }

    // Vertices の Data から ConvexMesh を作成する
    physx::PxConvexMesh* ToPxConvexMesh(physx::PxPhysics* physics, const std::vector<DirectX::XMFLOAT3>& vertices)
    {
        using namespace physx;

        // 頂点数チェック（明示的に）
        if (vertices.size() < 4 || vertices.size() > 256)
        {
            Logger::Warning("ConvexMesh cooking skipped: 頂点数が 4 未満 or 256 超え\n");
            return nullptr;
        }

        physx::PxTolerancesScale tolerancesScale;
        physx::PxCookingParams cookingParams(tolerancesScale);
        cookingParams.convexMeshCookingType = physx::PxConvexMeshCookingType::Enum::eQUICKHULL;
        cookingParams.gaussMapLimit = 256;
        physx::PxConvexMeshDesc pxMeshDesc;

        pxMeshDesc.points.count = static_cast<physx::PxU32>(vertices.size());
        pxMeshDesc.points.stride = sizeof(DirectX::XMFLOAT3);
        pxMeshDesc.points.data = vertices.data();
        pxMeshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
        physx::PxConvexMesh* convexMesh = nullptr;
        physx::PxDefaultMemoryOutputStream writeBuffer;
        if (!PxCookConvexMesh(cookingParams, pxMeshDesc, writeBuffer))
        {
            _ASSERT(L" PxCookConvexMesh が失敗しました！");
        }
        physx::PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
        convexMesh = physics->createConvexMesh(readBuffer);
        return convexMesh;
    }

    // rigidBodies の接続
    void CreateFixedJoints(const MeshComponent* meshComponent) const
    {
        using namespace physx;
        PxPhysics* pxPhysics = Physics::Instance().GetPhysics();
        auto& model = meshComponent->model;

        for (size_t parentIndex = 0; parentIndex < rigidBodies.size(); ++parentIndex)
        {
            PxRigidActor* parentBody = rigidBodies[parentIndex];
            if (!parentBody)
            {// 空ならスキップ
                continue;
            }

            auto nodes = model->GetNodes();
            auto node = nodes[parentIndex];
            for (int childIndex : node.children)
            {
                if (childIndex < 0 || static_cast<size_t>(childIndex) >= rigidBodies.size())
                {
                    continue;
                }

                PxRigidActor* childBody = rigidBodies[childIndex];
                if (!childBody)
                {
                    continue;
                }

                PxFixedJoint* joint = PxFixedJointCreate(*pxPhysics, parentBody, PxTransform(PxIdentity), childBody, PxTransform(PxIdentity));
                joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, true);
            }
        }
    }

    bool IsAddedToScene() const
    {
        return isAddedToScene;
    }


private:
    std::vector<physx::PxShape*> pxShapes;
    std::vector<physx::PxRigidActor*> rigidBodies;
    // 更新時に nodeIndex と rigidBody を紐づけておくため
    std::unordered_map<size_t, physx::PxRigidActor*> nodeIndexToRigidBody;
    // 描画時に必要な nodes
    std::vector<InterleavedGltfModel::Node> animatedNodes;
    MeshComponent* meshComponent = nullptr;
    // shape の userData に登録する
    CollisionComponent* collisionComponent = nullptr;
    bool isAddedToScene = false;
};

// 静的なステージ用 RigidBody
class TriangleMeshRigidBodyComponent :public RigidBodyComponent
{
public:
    TriangleMeshRigidBodyComponent(const std::string& name, const std::shared_ptr<Actor>& owner, MeshComponent* meshComponent)
        : RigidBodyComponent(name, owner), meshComponent(meshComponent), material(Physics::Instance().GetMaterial(PhysicsMaterialType::Default)) {}

    void Initialize(physx::PxPhysics* physics) override;

    void Tick(float deltaTime) override {}

    void AddToScene(physx::PxScene* scene)override
    {
        if (pxActor)
        {
            scene->addActor(*pxActor);
        }
    }

    void SetTransform(const Transform& worldTransform) override
    {
        if (pxActor)
        {
            pxActor->setGlobalPose(PhysicsHelper::ToPxTransform(worldTransform));
        }
    }

    void SetCollisionFilter(uint32_t layer, uint32_t mask) override
    {
        this->layer = layer;
        this->mask = mask;
        if (pxShape)
        {
            physx::PxFilterData filterData(layer, mask, 0, 0);
            pxShape->setSimulationFilterData(filterData);
            pxShape->setQueryFilterData(filterData);
        }
    }

    void DisableCollision() const
    {
        if (!pxActor) return;

        pxActor->setActorFlag(
            physx::PxActorFlag::eDISABLE_SIMULATION,
            false
        );
        if (!pxShape)
        {
            return;
        }
        pxShape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        pxShape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, false);
    }

    // 当たり判定を有効にする関数
    void EnableCollision() const
    {
        if (!pxActor) return;

        pxActor->setActorFlag(
            physx::PxActorFlag::eDISABLE_SIMULATION,
            true
        );

        if (!pxShape)
        {
            return;
        }
        pxShape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
        pxShape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, true);
    }


    void Destroy()override
    {
        if (pxActor)
        {
            pxActor->release();
            pxActor = nullptr;
        }

        pxShape = nullptr; // PxShape はアクターが所有しているので release 不要
    }
private:
    MeshComponent* meshComponent = nullptr;
    physx::PxRigidStatic* pxActor = nullptr;
    physx::PxShape* pxShape = nullptr;
    physx::PxMaterial* material = nullptr;
};

