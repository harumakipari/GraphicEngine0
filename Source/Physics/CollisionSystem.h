#ifndef COLLISION_SYSTEM_H
#define COLLISION_SYSTEM_H

// C++ 標準ライブラリ
#include <vector>

// 他ライブラリ
#include <PxPhysicsAPI.h>

// プロジェクトの他のヘッダ
#include "Core/Actor.h"
#include "Components/CollisionShape/CollisionComponent.h"
#include "Components/CollisionShape/ShapeComponent.h"


// キネマティック同士専用のコリジョンシステム
class CollisionSystem
{
public:
    static DirectX::XMFLOAT3 GetPushVectorForActor(Actor* actor)
    {
        auto it = actorPushMap_.find(actor);
        if (it != actorPushMap_.end())
        {
            return it->second;
        }
        return { 0.0f, 0.0f, 0.0f };
    }

    static void DetectAndResolveCollisions()
    {
        using namespace physx;

        actorPushMap_.clear(); // 初期化

        // expired な　weak_ptr　
        allComponents_.erase(
            std::remove_if(
                allComponents_.begin(), allComponents_.end(),
                [](const std::pair<std::weak_ptr<Actor>, std::weak_ptr<CollisionComponent>>& p)->bool
                {
                    return p.first.expired() || p.second.expired();
                }),
            allComponents_.end());


        // 衝突判定＆押し出し
        for (size_t i = 0; i < allComponents_.size(); ++i)
        {
            for (size_t j = i + 1; j < allComponents_.size(); ++j)
            {
                auto aActorWeak = allComponents_[i].first;
                auto bActorWeak = allComponents_[j].first;

                auto aComponentWeak = allComponents_[i].second;
                auto bComponentWeak = allComponents_[j].second;

                auto aActorShared = aActorWeak.lock();
                auto bActorShared = bActorWeak.lock();

                auto aComponentShared = aComponentWeak.lock();
                auto bComponentShared = bComponentWeak.lock();

                if (!aActorShared || !bActorShared || !aComponentShared || !bComponentShared)
                {
                    // どれか有効じゃない場合はスキップ（もしくは後でallComponents_から削除する）
                    continue;
                }

                Actor* aActor = aActorShared.get();
                Actor* bActor = bActorShared.get();
                CollisionComponent* aComponent = aComponentShared.get();
                CollisionComponent* bComponent = bComponentShared.get();

                if (!aActor->IsActive() || !bActor->IsActive())
                {
                    continue;
                }

                // どちらかの当たり判定のフラグが無効ならスキップ
                if (!aComponent->IsCollide() || !bComponent->IsCollide())
                {
                    continue;
                }

                // 同一アクターならスキップ
                if (aComponent->GetOwner() == bComponent->GetOwner())
                {
                    continue;
                }
                CollisionComponent::CollisionResponse aToB = aComponent->GetResponseTo(bComponent);
                CollisionComponent::CollisionResponse bToA = bComponent->GetResponseTo(aComponent);

                if (aToB == CollisionComponent::CollisionResponse::None && bToA == CollisionComponent::CollisionResponse::None)
                {
                    continue;
                }

                // 物理ベースで交差チェック
                auto aShape = dynamic_cast<ShapeComponent*>(aComponent);
                auto bShape = dynamic_cast<ShapeComponent*>(bComponent);
                if (!aShape || !bShape)
                {// 両者が shape でないと押し出しできない
                    continue;
                }

                //　両者が　kinematic　でないと押し出しできない
                if (!aShape->IsKinematic() && !bShape->IsKinematic())
                {
                    continue; // PhysX 同士は押し出さない
                }



                // PxGeometry 取得
                auto aInfo = aShape->GetPhysicsShapeInfo();
                auto bInfo = bShape->GetPhysicsShapeInfo();

                PxVec3 dir;
                float depth = 0.0f;
                bool hit = PxGeometryQuery::computePenetration(
                    dir, depth,
                    aInfo.geometry.any(), aInfo.transform,
                    bInfo.geometry.any(), bInfo.transform);

                if (!hit || depth <= 0.0001f) // 微小な衝突は無視
                    continue;

                std::pair<CollisionComponent*, CollisionComponent*> hitPairA = { aComponent, bComponent };
                std::pair<CollisionComponent*, CollisionComponent*> hitPairB = { bComponent, aComponent };

                // 衝突通知 (Trigger or Block ) どっちにも
                aComponent->OnHit(aComponent, bComponent);
                bComponent->OnHit(bComponent, aComponent);


                // 衝突イベント(Actor 通知)
                aActor->DispatchHitEvent(hitPairA);
                bActor->DispatchHitEvent(hitPairB);

                // 衝突イベント(Actor 通知)
                aActor->OnHit(hitPairA);
                bActor->OnHit(hitPairB);



                // Block でなければ押し出さない
                //if (aToB != CollisionComponent::CollisionResponse::Block && bToA != CollisionComponent::CollisionResponse::Block)
                if (aToB != CollisionComponent::CollisionResponse::Block || bToA != CollisionComponent::CollisionResponse::Block)
                {
                    //char msg[256];
                    //sprintf_s(msg, "skipPush: aToB=%d bToA=%d\n", (int)aToB, (int)bToA);
                    //OutputDebugStringA(msg);
                    continue;
                }

                //if (aComponent->IsStatic() && bComponent->IsStatic())
                //{
                //    continue; // 両方静的なら無視
                //}

                // 質量から押し出し率を計算
                float massA = aShape->GetMass();
                float massB = bShape->GetMass();

                float rateA = 0.5f;
                float rateB = 0.5f;

                if (massA == 0.0f)
                {
                    rateA = 0.0f;
                    rateB = 1.0f;
                }
                else if (massB == 0.0f)
                {
                    rateA = 1.0f;
                    rateB = 0.0f;
                }
                else
                {
                    rateA = massB / (massA + massB);
                    rateB = 1.0f - rateA;
                }

                // 押し出しベクトル

                float pushDistance = std::min<float>(depth, 1.0f); // 最大深さ制限
                dir = dir.getNormalized();


                // Actor単位に押し出し量を合算
                actorPushMap_[aActor].x += dir.x * pushDistance * rateA;
                //actorPushMap_[aActor].y += dir.y * pushDistance * rateA;
                actorPushMap_[aActor].z += dir.z * pushDistance * rateA;

                actorPushMap_[bActor].x -= dir.x * pushDistance * rateB;
                //actorPushMap_[bActor].y -= dir.y * pushDistance * rateB;
                actorPushMap_[bActor].z -= dir.z * pushDistance * rateB;

            }
        }
    }

    // 押出
    static void ApplyPushAll()
    {
        for (auto& [actor, pushVec] : actorPushMap_)
        {
            if (!actor) continue;

            auto rootComp = actor->GetRootComponent();
            if (rootComp)
            {
                rootComp->AddWorldOffset(pushVec);
            }
        }
    }

    static void RegisterCollisionComponent(std::shared_ptr<CollisionComponent> collisionComponent)
    {
        //allComponents_.emplace_back(collisionComponent->GetOwner(), collisionComponent);
        allComponents_.emplace_back(collisionComponent->GetActor(), collisionComponent);
    }

    static void UnRegisterCollisionComponent(CollisionComponent* collisionComponent)
    {
        allComponents_.erase(
            std::remove_if(
                allComponents_.begin(), allComponents_.end(),
                [collisionComponent](const std::pair<std::weak_ptr<Actor>, std::weak_ptr<CollisionComponent>>& p) {
                    return p.second.lock().get() == collisionComponent;
                }),
            allComponents_.end());
    }

    static void ClearAll()
    {
        allComponents_.clear();
    }

    static inline std::vector<std::pair<std::weak_ptr<Actor>, std::weak_ptr<CollisionComponent>>> allComponents_;
    //static inline std::vector<std::pair<Actor*, CollisionComponent*>> allComponents_;
private:
    // Actor 毎の押出量を計算して合成する用の Map 
    static inline std::unordered_map<Actor*, DirectX::XMFLOAT3> actorPushMap_;
};
#endif //COLLISION_SYSTEM_H