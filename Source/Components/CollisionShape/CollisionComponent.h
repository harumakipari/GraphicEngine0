#ifndef COLLISION_COMPONENT_H
#define COLLISION_COMPONENT_H

// C++ 標準ライブラリ
#include <cstdint>
#include <string>
#include <unordered_map>

// 他ライブラリ

// プロジェクトの他のヘッダ
#include "Components/Base/SceneComponent.h"
#include "Physics/CollisionHelper.h"
#include "Physics/Collider.h"

class CollisionComponent :public SceneComponent
{
public:
    enum class CollisionResponse
    {
        None,    // 完全に無視
        Trigger, // 衝突通知だけ（押し出ししない）
        Block    // 通知＋押し出し
    };

    CollisionComponent(const std::string& name, const std::shared_ptr<Actor>& owner) : SceneComponent(name, owner) {}

    // 現在の設定を取得
    uint32_t GetCollisionLayer() const { return collisionLayer_; }

    uint32_t GetCollisionMask() const { return collisionMask_; }

    void OnRegister()override;


    // 所属レイヤーを設定（例：Player、Enemy、Convexなど）
    void SetLayer(CollisionLayer layer)
    {
        collisionLayer_ = CollisionHelper::ToBit(layer);
    }

    // 相手に対する反応を登録 (引数：相手、反応)
    void SetResponseToLayer(CollisionLayer otherLayer, CollisionResponse response)
    {
        uint32_t bit = CollisionHelper::ToBit(otherLayer);
        // 相手と反応を登録
        responseTable_[bit] = response;

        if (response != CollisionResponse::None)
        {
            collisionMask_ |= bit;  // 衝突対象として登録
        }
        else
        {
            collisionMask_ &= ~bit; // 対象から除外
        }
    }

    // 引数の相手とはどんな反応をするかを取得する関数
    CollisionResponse GetResponseTo(const CollisionComponent* other)const
    {
        uint32_t layer = other->GetCollisionLayer();

        auto it = responseTable_.find(layer);
        if (it != responseTable_.end())
        {
            return it->second;
        }

        return CollisionResponse::None;
    }

    // 衝突判定対象か？
    bool ShouldCollideWith(const CollisionComponent* other)const
    {
        return GetResponseTo(other) != CollisionResponse::None;
    }

    // 押し出し処理をする対象か？
    bool ShouldPushOnCollisionWith(const CollisionComponent* other)const
    {
        return GetResponseTo(other) == CollisionResponse::Block;
    }

    // 衝突イベント
    virtual void OnHit(CollisionComponent* self, CollisionComponent* other);

    // 他のCollisionComponentとの衝突通知
    virtual void OnCollisionEnter(CollisionComponent* other, const DirectX::XMFLOAT3& hitPos, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT3& impulse) {}

    // 当たった時に衝撃を与える
    virtual void AddImpulse(const DirectX::XMFLOAT3& impulse) {}

    // コールバック関数を設定するための関数
    void SetOnHitCallback(std::function<void(CollisionComponent*, CollisionComponent*)> callback)
    {
        onHitCallback_ = callback;
    }

    //------- physics で遅延処理するための仮想関数-------//
    // シーンに物理処理を追加する
    virtual void AddToScene() {};
    virtual void SetKinematic(bool isKinematic) {};
    // 当たり判定を無効にする
    virtual void DisableCollision() {};
    // 当たり判定を有効にする
    virtual void EnableCollision() {};
    // 当たり判定が有効かどうかを取得する
    bool IsCollide() { return this->isCollide_; }
    // OnTrigger か OnContact どっちに入るか設定する
    virtual void SetTrigger(bool isTrigger) {};
    //virtual void Destroy()override {};
protected:
    uint32_t collisionLayer_ = 0;
    uint32_t collisionMask_ = 0;
    std::unordered_map<uint32_t, CollisionResponse> responseTable_;// 相手、反応

    bool isCollide_ = true;

    std::function<void(CollisionComponent*, CollisionComponent*)> onHitCallback_;
};


#endif