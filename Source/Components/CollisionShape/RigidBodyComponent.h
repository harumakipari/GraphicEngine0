#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <DirectXMath.h>
#include <PxPhysicsAPI.h>
#include <PxShape.h>

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

#include "Physics/Physics.h"
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

    void AddToScene(physx::PxScene* scene) override;

    void SetTransform(const Transform& worldTransform) override;

    void SetCollisionFilter(const uint32_t layer, const uint32_t mask) override;

    void SetGravity(const bool isUseGravity)override;

    void SetInitialVelocity(const DirectX::XMFLOAT3& velocity) const;

    void SetInitialAngularVelocity(const DirectX::XMFLOAT3& angularVelocity) const;

    void SetLinearDamping(const float damping) const;

    void SetAngularDamping(const float damping) const;

    void SetSleepThreshold(const float threshold) const;

    void SetMass(const float newMass);

    void DisableCollision() const;

    void EnableCollision() const;

    void ResizeSphere(float newRadius);

    void ResizeCapsule(float newRadius, float newHeight) const;

    void ResizeBox(float newExtentX, float newExtentY, float newExtentZ) const;

    void SetKinematic(const bool isKinematic)override;

    void SetTrigger(const bool isTrigger) { this->isTrigger = isTrigger; }

    void Destroy() override;

    void AddImpulse(const DirectX::XMFLOAT3& impulse) const;

    void SetMaterial(const PhysicsMaterialType type) const;

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

    void AddToScene(physx::PxScene* scene) override;

    void SetTransform(const Transform& worldTransform) override {}

    void SetCollisionFilter(const uint32_t layer, const uint32_t mask) override
    {
        this->layer = layer;
        this->mask = mask;
    }

    const std::vector<InterleavedGltfModel::Node>& GetAnimatedNodes() const{return animatedNodes;}

    void SetKinematic(const bool isKinematic) override;

    void DisableCollision() const;

    void Destroy() override;

private:
    bool IsAddedToScene() const { return isAddedToScene; }

    bool IsKinematic(const size_t index) const;

    void ComputeGlobalTransformRecursive(std::vector<InterleavedGltfModel::Node>& nodes, const int nodeIndex, const DirectX::XMFLOAT4X4& parentGlobal);

    void ComputeGlobalTransforms(std::vector<InterleavedGltfModel::Node>& nodes);

    // mesh 一つから vertices を作る
    std::vector<DirectX::XMFLOAT3> ReturnPhysxVertices(const InterleavedGltfModel::Mesh& mesh);

    // Vertices の Data から ConvexMesh を作成する
    physx::PxConvexMesh* ToPxConvexMesh(physx::PxPhysics* physics, const std::vector<DirectX::XMFLOAT3>& vertices);

    // rigidBodies の接続
    void CreateFixedJoints(const MeshComponent* meshComponent) const;

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

    void Destroy()override;

    void AddToScene(physx::PxScene* scene)override;

    void SetTransform(const Transform& worldTransform) override;

    void SetCollisionFilter(const uint32_t layer, const uint32_t mask) override;

private:
    void DisableCollision() const;

    void EnableCollision() const;
private:
    MeshComponent* meshComponent = nullptr;
    physx::PxRigidStatic* pxActor = nullptr;
    physx::PxShape* pxShape = nullptr;
    physx::PxMaterial* material = nullptr;
};

