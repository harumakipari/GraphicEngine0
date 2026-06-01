#pragma once

#include <vector>
#include <set>
#include <map>
#include <DirectXMath.h>
#include <memory>

#include "Graphics/Renderer/ShapeRenderer.h"
struct HitResult
{
    DirectX::XMFLOAT3	position;
    DirectX::XMFLOAT3	normal;
    float				distance;
};


class Actor;
class ShapeComponent;
struct HitResultWithActor
{
    Actor* actor = nullptr;     // 衝突した相手
    ShapeComponent* component = nullptr;    // 衝突したコンポーネント
    float distance = 0.0f;      // 距離
    DirectX::XMFLOAT3 hitPoint = { 0,0,0 };    // ヒット距離
    DirectX::XMFLOAT3 normal = { 0,1,0 };    // 法線
};


// マテリアルのタイプを作成する
enum class PhysicsMaterialType : uint8_t
{
    Default,
    Wall,
    Food,
    NoFriction,
};

// フィジクス
class Physics
    : public physx::PxQueryFilterCallback		// NOTE:③フィルタリングインターフェースの継承
    , public physx::PxSimulationEventCallback	// NOTE:⑦衝突イベントインターフェース継承
{
private:
    Physics() = default;
    ~Physics() = default;

public:
    // インスタンス取得
    static Physics& Instance()
    {
        static Physics instance;
        return instance;
    }

    // 初期化
    void Initialize();

    // 終了化
    void Finalize();

    // 更新処理
    void Update(float elapsedTime);

    // 描画処理
    void Render(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, const DirectX::XMFLOAT3& lightDirection);

    // フィジクス取得
    physx::PxPhysics* GetPhysics() const { return pxPhysics; }

    // シーン取得
    physx::PxScene* GetScene() const { return pxScene; }

    // コントローラーマネージャー取得
    physx::PxControllerManager* GetControllerManager() const { return pxControllerManager; }

    // マテリアル取得
    physx::PxMaterial* GetMaterial(PhysicsMaterialType type)
    {
        auto it = materials_.find(type);
        if (it != materials_.end())
        {
            return it->second;
        }
        return materials_[PhysicsMaterialType::Default];
    }

    // デフォルトのマテリアル取得
    physx::PxMaterial* GetDefaultMaterial() const
    {
        physx::PxMaterial* defaultMaterial_ = pxPhysics->createMaterial(0.5f, 0.5f, 0.6f); /* static friction, dynamic friction, restitution*/
        return defaultMaterial_;
    }


    // レイキャスト
    bool RayCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float distance, HitResult& result, uint32_t wantToHitLayer = 0xFFFFFF/*なにと当たりたいか、ここの数字に入れたら、この数値と同じレイヤーに当たる*/);

    // スフィアキャスト
    bool SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float distance, float radius, HitResult& result, uint32_t wantToHitLayer = 0xFFFFFF/*なにと当たりたいか、ここの数字に入れたら、この数値と同じレイヤーに当たる*/);
    bool SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float distance, float radius, HitResultWithActor& result, uint32_t wantToHitLayer = 0xFFFFFF/*なにと当たりたいか、ここの数字に入れたら、この数値と同じレイヤーに当たる*/);


protected:
    //--------------------------
    // NOTE:③フィルタリングインターフェース関数
    //--------------------------
    physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData& filterData, const physx::PxShape* shape, const physx::PxRigidActor* actor, physx::PxHitFlags& queryFlags) override;
    physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData, const physx::PxQueryHit& hit, const physx::PxShape* shape, const physx::PxRigidActor* actor) override;

    //--------------------------
    // NOTE:⑦衝突イベントインターフェース関数
    //--------------------------
    void onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count) override {};
    void onWake(physx::PxActor** actors, physx::PxU32 count) override {};
    void onSleep(physx::PxActor** actors, physx::PxU32 count) override {};
    void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count) override;
    void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs) override;
    void onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) override {};

private:
    //--------------------------
    // NOTE:⑧衝突検出フィルタリング
    //--------------------------
    static physx::PxFilterFlags SimulationFilterShader(
        physx::PxFilterObjectAttributes	attributes0, physx::PxFilterData filterData0,
        physx::PxFilterObjectAttributes	attributes1, physx::PxFilterData	filterData1,
        physx::PxPairFlags& pairFlags,
        const void* constantBlock, physx::PxU32 constantBlockSize);

private:

    physx::PxDefaultAllocator			pxAllocator;
    physx::PxDefaultErrorCallback		pxErrorCallback;
    physx::PxFoundation* pxFoundation = nullptr;
    physx::PxPhysics* pxPhysics = nullptr;
    physx::PxDefaultCpuDispatcher* pxDispatcher = nullptr;
    physx::PxScene* pxScene = nullptr;
    physx::PxControllerManager* pxControllerManager = nullptr;

    //physx::PxMaterial* pxMaterial = nullptr;

    // 複数のマテリアルを所持する
    std::map<PhysicsMaterialType, physx::PxMaterial*> materials_;

    physx::PxPvd* pxPvd = nullptr;

    struct ContactData
    {
        physx::PxVec3					normal;
        physx::PxF32					depth;
    };

    struct Line
    {
        DirectX::XMFLOAT3	start;
        DirectX::XMFLOAT3	end;
        DirectX::XMFLOAT4	color;
    };

    struct Capsule
    {
        DirectX::XMFLOAT4X4	transform;
        float				radius;
        float				height;
        DirectX::XMFLOAT4	color;
    };
    std::vector<Line>		lines;
    std::vector<Capsule>	capsules;


    std::vector<physx::PxRigidDynamic*> gravityEnableList_;

};
