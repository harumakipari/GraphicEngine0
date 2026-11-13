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
protected:
    PhysicsControlMode physicsMode = PhysicsControlMode::Kinematic; // physics Mode を有効にする

    CapsuleAxis capsuleAxis = CapsuleAxis::y;   // デフォルトは Y 軸方向

    std::unique_ptr<SingleRigidBodyComponent> rigidBody;// 物理ボディを内包

    // 物理形状情報を返すもの
    struct PhysicsShapeInfo
    {
        physx::PxGeometryHolder geometry;   // 全ての形状を保持できるユーティリティ
        physx::PxTransform transform;       //ワールド変換
    };
public:
    ShapeComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :CollisionComponent(name, owner) {}

    void Initialize()override;

    bool IsKinematic()const
    {
        return physicsMode == PhysicsControlMode::Kinematic;
    }

    // キネマティックにするかどうか　
    void SetKinematic(const bool isKinematic)override
    {
        physicsMode = isKinematic ? PhysicsControlMode::Kinematic : PhysicsControlMode::Dynamic;
        if (rigidBody)
        {
            rigidBody->SetKinematic(isKinematic);
        }
    }
    // OnTrigger か OnContact どっちに入るか設定する
    //　これは Initialize の前に設定する　デフォルトは false
    void SetTrigger(const bool isTrigger)override
    {
        if (rigidBody)
        {
            rigidBody->SetTrigger(isTrigger);
        }
    }

    void SetGravity(const bool isUseGravity) const
    {
        if (rigidBody)
        {
            rigidBody->SetGravity(isUseGravity);
        }
    }

    void Tick(const float deltaTime) override;

    // ゲーム内でカプセルがどの軸に長いか取得
    const CapsuleAxis& GetCapsuleAxis() const { return capsuleAxis; }
    // ゲーム内でカプセルがどの軸に長いか設定
    void SetCapsuleAxis(const CapsuleAxis& axis) { this->capsuleAxis = axis; }

    // 初速を設定する
    void SetInitialVelocity(const DirectX::XMFLOAT3& velocity) const
    {
        rigidBody->SetIntialVelocity(velocity);
    }

    void SetInitialAngularVelocity(const DirectX::XMFLOAT3& angularVelocity) const
    {
        rigidBody->SetInitialAngularVelocity(angularVelocity);
    }

    void SetPhysicsWorldTransform(const Transform& worldTransform) const
    {
        if (rigidBody)
            rigidBody->SetTransform(worldTransform);
    }

    // 当たり判定レイヤーを途中で追加する
    void AddCollisionFilter(const CollisionLayer otherLayer, const CollisionResponse response)
    {
        SetResponseToLayer(otherLayer, response);
        rigidBody->SetCollisionFilter(GetCollisionLayer(), GetCollisionMask());
    }

    void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        SceneComponent::DrawImGuiInspector();
        if (ImGui::TreeNode((name + "  debugRender").c_str()))
        {
            ImGui::Checkbox("isVisibleDebugShape", &isVisibleDebugShape);
            ImGui::TreePop();
        }
#endif
    }

    virtual AABB GetAABB() const = 0;

    const float& GetRadius() const { return radius; }

    void SetCollisionOffsetX(const float offsetX) { collisionOffsetX = offsetX; }
    void SetCollisionOffsetY(const float offsetY) { collisionOffsetY = offsetY; }
    void SetCollisionOffsetZ(const float offsetZ) { collisionOffsetZ = offsetZ; }

    const float& GetCollisionOffsetX() const { return collisionOffsetX; }
    const float& GetCollisionOffsetY() const { return collisionOffsetY; }
    const float& GetCollisionOffsetZ() const { return collisionOffsetZ; }

    DirectX::XMFLOAT3 GetSizeFromAABB(const AABB& aabb)
    {
        return
        {
            aabb.max.x - aabb.min.x,
            aabb.max.y - aabb.min.y,
            aabb.max.z - aabb.min.z
        };
    }

    void SetLinearDamping(float damping) const
    {
        if (rigidBody) rigidBody->SetLinearDamping(damping);
    }

    void SetAngularDamping(float damping) const
    {
        if (rigidBody) rigidBody->SetAngularDamping(damping);
    }

    void SetSleepThreshold(float threshold) const
    {
        if (rigidBody) rigidBody->SetSleepThreshold(threshold);
    }

    // 当たった時に衝撃を与える
    void AddImpulse(const DirectX::XMFLOAT3& impulse)override
    {
        if (rigidBody)
        {
            rigidBody->AddImpulse(impulse);
        }
    }

    virtual PhysicsShapeInfo GetPhysicsShapeInfo()const = 0;

    virtual void SetMass(const float m) { this->mass = m; }

    virtual float GetMass() { return this->mass; }

    void SetStatic(const bool isStatic)
    {
        this->isStatic = isStatic;
        if (isStatic)
        {
            physicsMode = PhysicsControlMode::Kinematic;
            mass = 0.0f;
            if (rigidBody)
                rigidBody->SetKinematic(true);
        }
    }

    bool IsStatic() const { return isStatic; }

    // 当たり判定を無効にする
    void DisableCollision() override
    {
        isCollide = false;
        if (rigidBody)
        {
            rigidBody->DisableCollision();
        }
    };

    // 当たり判定を有効にする
    void EnableCollision() override
    {
        isCollide = true;
        if (rigidBody)
        {
            rigidBody->EnableCollision();
        }
    };

    void OnUnregister()override
    {
        if (rigidBody)
        {
            rigidBody->Destroy();
            rigidBody = nullptr;
        }
    }

    // デバックの形状を描画するか
    bool IsVisibleDebugShape() const { return isVisibleDebugShape; }

    void SetIsVisibleDebugShape(const bool isVisibleDebugShape) { this->isVisibleDebugShape = isVisibleDebugShape; }

    const std::string& GetCollisionType()const { return collisionTypeName; }

    void SetPhysicsMaterial(PhysicsMaterialType type)
    {
        materialType = type;
        if (rigidBody)
        {
            rigidBody->SetMaterial(type);
        }
    }

    PhysicsMaterialType GetPhysicsMaterialType() const
    {
        return materialType;
    }

    // 初速度を与える
    void InitialVelocity(DirectX::XMFLOAT3 initialVelocity) const
    {
        if (rigidBody)
        {
            rigidBody->SetIntialVelocity(initialVelocity);
        }
    }


protected:
    bool isStatic = false;

    bool isVisibleDebugShape = true;

    float radius = 0.0f;
    float collisionOffsetY = 0.0f; // physxの当たり判定の原点が埋まらないようにするために使用
    float collisionOffsetX = 0.0f; // physxの当たり判定の原点が埋まらないようにするために使用
    float collisionOffsetZ = 0.0f; // physxの当たり判定の原点が埋まらないようにするために使用

    float mass = 1.0f; // defaultで 1.0kg にしておく

    std::string collisionTypeName = "";
    PhysicsMaterialType materialType = PhysicsMaterialType::Default;
};


class BoxComponent :public ShapeComponent
{
    DirectX::XMFLOAT3 boxExtent_ = { 1.0f,1.0f,1.0f };

public:
    BoxComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :ShapeComponent(name, owner)
    {
        collisionTypeName = "Box";
    }

    void SetHalfBoxExtent(const DirectX::XMFLOAT3& extent)//boxの半径
    {
        boxExtent_ = extent;
    }

    void SetBoxExtent(const DirectX::XMFLOAT3& extent)
    {
        boxExtent_ = { extent.x * 0.5f,extent.y * 0.5f,extent.z * 0.5f };
    }

    AABB GetAABB()const override
    {
        AABB aabb{};
        DirectX::XMFLOAT3 center = GetComponentLocation();
        aabb.min = { center.x - boxExtent_.x,center.y - boxExtent_.y,center.z - boxExtent_.z };
        aabb.max = { center.x + boxExtent_.x,center.y + boxExtent_.y,center.z + boxExtent_.z };
        return aabb;
    }

    void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        ShapeComponent::DrawImGuiInspector();
        if (ImGui::TreeNode((name + "  box").c_str()))
        {
            ImGui::DragFloat3("boxExtent", &boxExtent_.x, 0.1f);
            ResizeBox(boxExtent_.x, boxExtent_.y, boxExtent_.z);
            ImGui::TreePop();
        }
#endif
    }

    // 動的にボックスの判定を大きくする関数
    void ResizeBox(const float newExtentX, const float newExtentY, const float newExtentZ)
    {
        rigidBody->ResizeBox(newExtentX, newExtentY, newExtentZ);
        boxExtent_.x = newExtentX;
        boxExtent_.y = newExtentY;
        boxExtent_.z = newExtentZ;
    }

    PhysicsShapeInfo GetPhysicsShapeInfo()const override
    {
        PhysicsShapeInfo info;
        info.geometry = physx::PxBoxGeometry(boxExtent_.x, boxExtent_.y, boxExtent_.z);
        const Transform& worldT = GetComponentWorldTransform();
        DirectX::XMFLOAT3 pos = worldT.GetLocation();
        DirectX::XMFLOAT4 rot = worldT.GetRotation();

        DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rot);
        q = DirectX::XMQuaternionNormalize(q);
        DirectX::XMStoreFloat4(&rot, q);

        info.transform = physx::PxTransform{
            physx::PxVec3(pos.x,pos.y,pos.z),
            physx::PxQuat(rot.x,rot.y,rot.z,rot.w)
        };

        return info;
    }
};

class SphereComponent :public ShapeComponent
{
public:
    SphereComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :ShapeComponent(name, owner)
    {
        collisionTypeName = "Sphere";
    }

    void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI

        ShapeComponent::DrawImGuiInspector();
        if (ImGui::TreeNode((name + "  sphere").c_str()))
        {
            ImGui::DragFloat("radius", &radius, 0.1f);
            ImGui::TreePop();
        }
#endif
    }

    Sphere ToSphere()const
    {
        Sphere s{};
        s.c = Point(GetComponentWorldTransform().GetLocation());
        s.r = radius;
        return s;
    }

    PhysicsShapeInfo GetPhysicsShapeInfo()const override
    {
        PhysicsShapeInfo info;
        info.geometry = physx::PxSphereGeometry(radius);
        DirectX::XMFLOAT3 pos = GetComponentWorldTransform().GetLocation();
        info.transform = physx::PxTransform{ {pos.x,pos.y,pos.z} };
        return info;
    }

    void SetRadius(const float radius)
    {
        this->radius = radius;
    }

    _NODISCARD AABB GetAABB()const override
    {
        DirectX::XMFLOAT3 pos = GetComponentWorldTransform().GetLocation();
        AABB aabb;
        aabb.min = { pos.x - radius,pos.y - radius,pos.z - radius };
        aabb.max = { pos.x + radius,pos.y + radius,pos.z + radius };
        return aabb;
    }

    // 動的に球の当たり判定の半径を変更する関数
    void ResizeSphere(const float radius)
    {
        this->radius = radius;
        rigidBody->ResizeSphere(radius);
    }

    float GetRadius() const { return radius; }
private:
};

class CapsuleComponent :public ShapeComponent
{
    float height = 0.0f;
public:
    CapsuleComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :ShapeComponent(name, owner)
    {
        collisionTypeName = "Capsule";
    }

    void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        ShapeComponent::DrawImGuiInspector();

        if (ImGui::TreeNode((name + "  capsule").c_str()))
        {
            ImGui::DragFloat("radius", &radius, 0.1f);
            ImGui::DragFloat("height", &height, 0.1f);
            ResizeCapsule(radius,height);
            ImGui::TreePop();
        }
#endif
    }


    Capsule ToCapsule()const
    {
        Capsule c{};
        c.a = Point(GetComponentWorldTransform().GetLocation().x, GetComponentWorldTransform().GetLocation().y + height - radius, GetComponentWorldTransform().GetLocation().z);
        c.b = DirectX::XMFLOAT3(GetComponentWorldTransform().GetLocation().x, GetComponentWorldTransform().GetLocation().y + radius, GetComponentWorldTransform().GetLocation().z);
        c.r = radius;
        return c;
    }

    AABB GetAABB() const override
    {
        auto pos = GetComponentWorldTransform().GetLocation();
        DirectX::XMFLOAT3 a = { pos.x, pos.y + height , pos.z };
        DirectX::XMFLOAT3 b = { pos.x, pos.y + radius, pos.z };

        AABB box;
        box.min.x = std::min<float>(a.x, b.x) - radius;
        box.min.y = std::min<float>(a.y, b.y) - radius;
        box.min.z = std::min<float>(a.z, b.z) - radius;

        box.max.x = std::max<float>(a.x, b.x) + radius;
        box.max.y = std::max<float>(a.y, b.y) + radius;
        box.max.z = std::max<float>(a.z, b.z) + radius;

        return box;
    }

    PhysicsShapeInfo GetPhysicsShapeInfo()const override
    {
        PhysicsShapeInfo info;
        // 見た目の全長（頭～足）
        float totalHeight = height;

        // PhysX 用 halfHeight（球を除いた円柱部分）
        float halfHeight = (totalHeight * 0.5f) - radius;
        halfHeight = std::max<float>(halfHeight, 0.0f);

        info.geometry = physx::PxCapsuleGeometry(radius, halfHeight);
        const Transform& worldT = GetComponentWorldTransform();
        DirectX::XMFLOAT3 pos = worldT.GetLocation();
        DirectX::XMFLOAT4 rot = worldT.GetRotation();

        DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rot);
        q = DirectX::XMQuaternionNormalize(q);
        DirectX::XMStoreFloat4(&rot, q);

        // World 回転
        physx::PxQuat worldQ(rot.x, rot.y, rot.z, rot.w);
        physx::PxQuat capsuleAxisQ = { physx::PxPiDivTwo, physx::PxVec3(0.0f, 0.0f, 1.0f) };
        // Capsule を X → Y に立てる回転
        switch (capsuleAxis)
        {
        case CapsuleAxis::x:
            capsuleAxisQ = { physx::PxPiDivTwo, physx::PxVec3(1.0f, 0.0f, 0.0f) };
            break;
        case ShapeComponent::CapsuleAxis::y:
            capsuleAxisQ = { physx::PxPiDivTwo, physx::PxVec3(0.0f, 0.0f, 1.0f) };
            break;
        case ShapeComponent::CapsuleAxis::z:
            capsuleAxisQ = { physx::PxPiDivTwo, physx::PxVec3(0.0f, 1.0f, 0.0f) };
            break;
        };

        info.transform = physx::PxTransform(
            physx::PxVec3(pos.x, pos.y, pos.z),
            worldQ * capsuleAxisQ
        );
        return info;
    }

    void SetRadiusAndHeight(const float radius, const float height) { this->radius = radius; this->height = height; }

    float GetRadius() const { return radius; }
    float GetHeight() const { return height; }


    // 動的にカプセルの判定を大きくする関数
    void ResizeCapsule(const float newRadius, const float newHeight)
    {
        rigidBody->ResizeCapsule(newRadius, newHeight);
        radius = newRadius;
        height = newHeight;
    }

};

class ConvexCollisionComponent :public CollisionComponent
{
public:
    ConvexCollisionComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :CollisionComponent(name, owner) {}

    void Initialize()override
    {
    }

    void Tick(const float deltaTime)override
    {
        if (rigidBody_)
        {
            rigidBody_->Tick(deltaTime);
        }
    }

    // モデルから ConvexMesh を作成する
    void CreateConvexMeshFromModel(MeshComponent* meshComponent);

    void SetKinematic(const bool isKinematic)override
    {
        rigidBody_->SetKinematic(isKinematic);
    }

    void AddToScene()override
    {
        rigidBody_->AddToScene(Physics::Instance().GetScene());
    }

    const std::vector<InterleavedGltfModel::Node>& GetAnimatedNodes() const
    {
        return rigidBody_->GetAnimatedNodes();
    }

    void DisableCollision()override
    {
        rigidBody_->DisableCollision();
    }

    void OnUnregister()override
    {
        if (rigidBody_)
        {
            rigidBody_->Destroy();
            rigidBody_ = nullptr;
        }
    }

    MeshComponent* GetMeshComponent()const { return meshComponent_; }
private:
    std::shared_ptr<MultiRigidBodyComponent> rigidBody_;
    MeshComponent* meshComponent_ = nullptr;
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
