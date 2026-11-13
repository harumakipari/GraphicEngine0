#pragma once

#include <vector>
#include <map>

#include <DirectXMath.h>

#include "Graphics/Renderer/ShapeRenderer.h"

class Actor;
class ShapeComponent;
class CollisionComponent;

struct HitResult
{
    DirectX::XMFLOAT3 position = {};
    DirectX::XMFLOAT3 normal = {};
    float distance = 0.0f;
    bool initialOverlap = false;
    uint32_t layerMask = 0;
};

struct HitResultWithActor
{
    Actor* actor = nullptr;     // 衝突した相手
    ShapeComponent* component = nullptr;    // 衝突したコンポーネント
    float distance = 0.0f;      // 距離
    DirectX::XMFLOAT3 hitPoint = { 0,0,0 };    // ヒット距離
    DirectX::XMFLOAT3 normal = { 0,1,0 };    // 法線
    bool hasPosition = false;
    bool hasNormal = false;
    bool initialOverlap = false;
    float penetrationDepth = 0.0f;
};

struct PositioningSweepShape
{
    enum class Type { Sphere, Capsule };
    Type type = Type::Sphere;
    DirectX::XMFLOAT3 center{};
    DirectX::XMFLOAT3 point1{};
    DirectX::XMFLOAT3 point2{};
    float radius = 0.0f;
};

struct PositioningPathSafetyPolicy
{
    float floorNormalThreshold = 0.7f;
    float supportPenetrationTolerance = 0.30f;
};

struct PositioningPathSafetyHit
{
    bool hit = false;
    bool initialOverlap = false;
    bool hasNormal = false;
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 normal{};
    float distance = 0.0f;
    float penetrationDepth = 0.0f;
    Actor* actor = nullptr;
    CollisionComponent* component = nullptr;
};

struct PositioningPathSafetyResult
{
    bool pathBlocked = false;
    int ignoredSupportContactCount = 0;
    PositioningPathSafetyHit ignoredSupportContact{};
    PositioningPathSafetyHit blockingHit{};
};

enum class PhysicsMaterialType : uint8_t
{
    Default,
    NoFriction,
};

/// PhysXの管理クラス
class Physics : public physx::PxQueryFilterCallback, public physx::PxSimulationEventCallback	
{
private:
    Physics() = default;
    ~Physics() = default;

public:
    /// シングルトン
    static Physics& Instance()
    {
        static Physics instance;
        return instance;
    }

    void Initialize();

    void Finalize();

    void Update(float elapsedTime);

    void Render(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, const DirectX::XMFLOAT3& lightDirection);

    physx::PxPhysics* GetPhysics() const { return pxPhysics; }

    physx::PxScene* GetScene() const { return pxScene; }

    physx::PxControllerManager* GetControllerManager() const { return pxControllerManager; }

    physx::PxMaterial* GetMaterial(const PhysicsMaterialType type)
    {
        auto it = materials.find(type);
        if (it != materials.end())
        {
            return it->second;
        }
        return materials[PhysicsMaterialType::Default];
    }

    physx::PxMaterial* GetDefaultMaterial() const
    {
        return materials.at(PhysicsMaterialType::Default);
    }

    bool RayCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float distance, HitResult& result, uint32_t wantToHitLayer = 0xFFFFFF);

    // スフィアキャスト
    bool SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float distance, float radius, HitResult& result, uint32_t wantToHitLayer = 0xFFFFFF);
    bool SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float distance, float radius, HitResultWithActor& result, uint32_t wantToHitLayer = 0xFFFFFF, bool useMTD = false);
    bool SweepPositioningPath(const PositioningSweepShape& shape,const DirectX::XMFLOAT3& direction, float distance, uint32_t wantToHitLayer,const PositioningPathSafetyPolicy& policy, PositioningPathSafetyResult& result);
  
    // カプセルキャスト
    bool CapsuleCast(const DirectX::XMFLOAT3& point1, const DirectX::XMFLOAT3& point2, float radius, const DirectX::XMFLOAT3& direction, float distance, bool trigger, HitResult& result, uint32_t wantToHitLayer = 0xFFFFFF);

protected:
    // フィルタリングインターフェース関数
    physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData& filterData, const physx::PxShape* shape, const physx::PxRigidActor* actor, physx::PxHitFlags& queryFlags) override;
    physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData, const physx::PxQueryHit& hit, const physx::PxShape* shape, const physx::PxRigidActor* actor) override;

    // 衝突イベントインターフェース関数
    void onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count) override {};
    void onWake(physx::PxActor** actors, physx::PxU32 count) override {};
    void onSleep(physx::PxActor** actors, physx::PxU32 count) override {};
    void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count) override;
    void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs) override;
    void onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) override {};

private:
    // 衝突検出フィルタリング
    static physx::PxFilterFlags SimulationFilterShader(
        physx::PxFilterObjectAttributes	attributes0, physx::PxFilterData filterData0,
        physx::PxFilterObjectAttributes	attributes1, physx::PxFilterData	filterData1,
        physx::PxPairFlags& pairFlags,
        const void* constantBlock, physx::PxU32 constantBlockSize);

private:
    struct ContactData
    {
        physx::PxVec3					normal;
        physx::PxF32					depth;
    };

    struct DebugLine
    {
        DirectX::XMFLOAT3	start;
        DirectX::XMFLOAT3	end;
        DirectX::XMFLOAT4	color;
    };

    struct DebugCapsule
    {
        DirectX::XMFLOAT4X4	transform;
        float				radius;
        float				height;
        DirectX::XMFLOAT4	color;
    };
private:
    physx::PxDefaultAllocator			pxAllocator;
    physx::PxDefaultErrorCallback		pxErrorCallback;
    physx::PxFoundation* pxFoundation = nullptr;
    physx::PxPhysics* pxPhysics = nullptr;
    physx::PxDefaultCpuDispatcher* pxDispatcher = nullptr;
    physx::PxScene* pxScene = nullptr;
    physx::PxControllerManager* pxControllerManager = nullptr;
    physx::PxPvd* pxPvd = nullptr;

    // 複数のマテリアルを所持する
    std::map<PhysicsMaterialType, physx::PxMaterial*> materials;

    std::vector<DebugLine>		lines;
    std::vector<DebugCapsule>	capsules;
};
