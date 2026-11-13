#pragma once

#include <memory>
#include <string>
#include <DirectXMath.h>
#include <PxPhysicsAPI.h>

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "imgui.h"
#endif

#include "Physics/Collider.h"
#include "Physics/Physics.h"
#include "Physics/CollisionHelper.h"
#include "Components/Base/SceneComponent.h"
#include "RigidBodyComponent.h"
#include "Graphics/Resource/InterleavedGltfModel.h"
#include "Components/Render/MeshComponent.h"
#include "CollisionComponent.h"

class ShapeComponent :public CollisionComponent
{
public:
    // 物理演算を有効にするかどうか
    enum class PhysicsControlMode :uint8_t
    {
        Kinematic,     // SetWorldLocationでTransform制御
        Dynamic,        // 物理演算がTransformを制御（PxRigidDynamicの座標がSceneComponentに反映される）
    };

    // ゲーム内でカプセルがどの軸に長いか
    enum class CapsuleAxis :uint8_t
    {
        x, y, z
    };

    // 物理形状情報を返すもの
    struct PhysicsShapeInfo
    {
        physx::PxGeometryHolder geometry;   // 全ての形状を保持できるユーティリティ
        physx::PxTransform transform;       //ワールド変換
    };

public:
    ShapeComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :CollisionComponent(name, owner) {}

    void Initialize()override;

    void Tick(const float deltaTime) override;

    bool IsKinematic()const { return physicsMode == PhysicsControlMode::Kinematic; }

    void SetKinematic(const bool isKinematic)override;

    // OnTrigger か OnContact どっちに入るか設定する
    void SetTrigger(const bool isTrigger)override;

    void SetGravity(const bool isUseGravity) const;

    void SetLinearDamping(const float damping) const;

    void SetAngularDamping(const float damping) const;

    void SetSleepThreshold(const float threshold) const;

    // 当たった時に衝撃を与える
    void AddImpulse(const DirectX::XMFLOAT3& impulse)override;

    virtual PhysicsShapeInfo GetPhysicsShapeInfo()const = 0;

    virtual void SetMass(const float m) { this->mass = m; }

    virtual float GetMass() { return this->mass; }

    void SetStatic(const bool isStatic);

    bool IsStatic() const { return isStatic; }

    // 当たり判定を無効にする
    void DisableCollision() override;

    // 当たり判定を有効にする
    void EnableCollision() override;

    void OnUnregister()override;

    // デバックの形状を描画するか
    bool IsVisibleDebugShape() const { return isVisibleDebugShape; }

    void SetIsVisibleDebugShape(const bool isVisibleDebugShape) { this->isVisibleDebugShape = isVisibleDebugShape; }

    const std::string& GetCollisionType()const { return collisionTypeName; }

    void SetPhysicsMaterial(const PhysicsMaterialType type);

    PhysicsMaterialType GetPhysicsMaterialType() const { return materialType; }

    // ゲーム内でカプセルがどの軸に長いか取得
    const CapsuleAxis& GetCapsuleAxis() const { return capsuleAxis; }
    // ゲーム内でカプセルがどの軸に長いか設定
    void SetCapsuleAxis(const CapsuleAxis& axis) { this->capsuleAxis = axis; }

    // 初速を設定する
    void SetInitialVelocity(const DirectX::XMFLOAT3& velocity) const;

    void SetInitialAngularVelocity(const DirectX::XMFLOAT3& angularVelocity) const;

    void SetPhysicsWorldTransform(const Transform& worldTransform) const;

    // 当たり判定レイヤーを途中で追加する
    void AddCollisionFilter(const CollisionLayer otherLayer, const CollisionResponse response);

    void DrawImGuiInspector() override;

    virtual AABB GetAABB() const = 0;

    virtual const float& GetRadius() const { return radius; }

    void SetCollisionOffsetX(const float offsetX) { collisionOffsetX = offsetX; }
    void SetCollisionOffsetY(const float offsetY) { collisionOffsetY = offsetY; }
    void SetCollisionOffsetZ(const float offsetZ) { collisionOffsetZ = offsetZ; }

    const float& GetCollisionOffsetX() const { return collisionOffsetX; }
    const float& GetCollisionOffsetY() const { return collisionOffsetY; }
    const float& GetCollisionOffsetZ() const { return collisionOffsetZ; }

protected:
    std::string collisionTypeName = "";
    CapsuleAxis capsuleAxis = CapsuleAxis::y;   // デフォルトは Y 軸方向
    std::unique_ptr<SingleRigidBodyComponent> rigidBody;// 物理ボディを内包
    float radius = 0.0f;

private:
    PhysicsControlMode physicsMode = PhysicsControlMode::Kinematic; // physics Mode を有効にする
    PhysicsMaterialType materialType = PhysicsMaterialType::Default;
    bool isStatic = false;
    bool isVisibleDebugShape = true;
    float collisionOffsetY = 0.0f; // physxの当たり判定の原点が埋まらないようにするために使用
    float collisionOffsetX = 0.0f; // physxの当たり判定の原点が埋まらないようにするために使用
    float collisionOffsetZ = 0.0f; // physxの当たり判定の原点が埋まらないようにするために使用
    float mass = 1.0f; // defaultで 1.0kg にしておく
};


class BoxComponent :public ShapeComponent
{
public:
    BoxComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :ShapeComponent(name, owner)
    {
        collisionTypeName = "Box";
    }

    void SetHalfBoxExtent(const DirectX::XMFLOAT3& extent)//boxの半径
    {
        boxExtent = extent;
    }

    void SetBoxExtent(const DirectX::XMFLOAT3& extent)
    {
        boxExtent = { extent.x * 0.5f,extent.y * 0.5f,extent.z * 0.5f };
    }

    PhysicsShapeInfo GetPhysicsShapeInfo()const override;

    AABB GetAABB()const override;

    void DrawImGuiInspector() override;

private:
    void ResizeBox(const float newExtentX, const float newExtentY, const float newExtentZ);
private:
    DirectX::XMFLOAT3 boxExtent = { 1.0f,1.0f,1.0f };

};

class SphereComponent :public ShapeComponent
{
public:
    SphereComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :ShapeComponent(name, owner)
    {
        collisionTypeName = "Sphere";
    }

    void DrawImGuiInspector() override;

    PhysicsShapeInfo GetPhysicsShapeInfo()const override;

    void SetRadius(const float radius) { this->radius = radius; }

    _NODISCARD AABB GetAABB()const override
    {
        DirectX::XMFLOAT3 pos = GetComponentWorldTransform().GetLocation();
        AABB aabb;
        aabb.min = { pos.x - radius,pos.y - radius,pos.z - radius };
        aabb.max = { pos.x + radius,pos.y + radius,pos.z + radius };
        return aabb;
    }

private:
    Sphere ToSphere()const;

    void ResizeSphere(const float radius)
    {
        this->radius = radius;
        rigidBody->ResizeSphere(radius);
    }
};

class CapsuleComponent :public ShapeComponent
{
public:
    CapsuleComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :ShapeComponent(name, owner)
    {
        collisionTypeName = "Capsule";
    }

    void DrawImGuiInspector() override;

    AABB GetAABB() const override;

    PhysicsShapeInfo GetPhysicsShapeInfo()const override;

    void SetRadiusAndHeight(const float radius, const float height) { this->radius = radius; this->height = height; }

    float GetHeight() const { return height; }

private:
    void ResizeCapsule(const float newRadius, const float newHeight)
    {
        rigidBody->ResizeCapsule(newRadius, newHeight);
        radius = newRadius;
        height = newHeight;
    }
    Capsule ToCapsule()const;

private:
    float height = 0.0f;
};

class ConvexCollisionComponent :public CollisionComponent
{
public:
    ConvexCollisionComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :CollisionComponent(name, owner) {}

    void Initialize()override {}

    void Tick(const float deltaTime)override
    {
        if (rigidBody)
        {
            rigidBody->Tick(deltaTime);
        }
    }

    // モデルから ConvexMesh を作成する
    void CreateConvexMeshFromModel(MeshComponent* meshComponent);

    void SetKinematic(const bool isKinematic)override
    {
        rigidBody->SetKinematic(isKinematic);
    }

    void AddToScene()override
    {
        rigidBody->AddToScene(Physics::Instance().GetScene());
    }

    const std::vector<InterleavedGltfModel::Node>& GetAnimatedNodes() const
    {
        return rigidBody->GetAnimatedNodes();
    }

    void DisableCollision()override
    {
        rigidBody->DisableCollision();
    }

    void OnUnregister()override
    {
        if (rigidBody)
        {
            rigidBody->Destroy();
            rigidBody = nullptr;
        }
    }

    MeshComponent* GetMeshComponent()const { return meshComponent; }
private:
    std::shared_ptr<MultiRigidBodyComponent> rigidBody;
    MeshComponent* meshComponent = nullptr;
};

// staticBatchingModel にのみ使用可能
class TriangleMeshCollisionComponent :public CollisionComponent
{
public:
    TriangleMeshCollisionComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :CollisionComponent(name, owner) {}

    void Initialize()override {}

    void Tick(const float deltaTime)override
    {
        if (rigidBody)
        {
            rigidBody->Tick(deltaTime);
        }
    }
    void AddToScene()override
    {
        rigidBody->AddToScene(Physics::Instance().GetScene());
    }

    // モデルから TriangleMesh を作成する
    void CreateConvexMeshFromModel(MeshComponent* meshComponent);

private:
    std::shared_ptr<TriangleMeshRigidBodyComponent> rigidBody;
    MeshComponent* meshComponent = nullptr;
};
