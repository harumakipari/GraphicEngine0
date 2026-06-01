#ifndef COLLISION_HELPER_H
#define COLLISION_HELPER_H

#include <cmath>
#include <list>

enum class CollisionLayer :uint32_t
{
    None = 0,
    WorldStatic ,
    Player ,
    Enemy ,
    Boss,
    Convex ,
    WorldProps , // stage の object
    Camera ,   // カメラ
    Interactable,  // プレイヤーが近づいてインタラクトできるもの
    PlayerWeapon,// プレイヤーの武器
    Bobbin, //
    Floor,
    Wall, // 反射壁
    EnemyRedirect, // 反射敵
    Projectile, // 弾丸
    Bomb, // 爆弾
    Item, // アイテム
    Max,
};
constexpr uint32_t COLLISION_EVERYTHING = 0xFFFFFFFF;


namespace CollisionHelper
{
    // 単一レイヤーをビットに変換
    inline uint32_t ToBit(CollisionLayer layer)
    {
        uint32_t bit = (1u << static_cast<uint32_t>(layer));
        return bit;
    }

    // 複数レイヤーをまとめてマスクに変換
    inline uint32_t MakeMask(std::initializer_list<CollisionLayer> layers)
    {
        uint32_t mask = 0;
        for (auto layer : layers)
        {
            mask |= ToBit(layer);
        }
        return mask;
    }

    // マスクにレイヤーを追加
    inline void AddToMask(uint32_t& mask, CollisionLayer layer)
    {
        mask |= ToBit(layer);
    }

    // マスクからレイヤーを除去
    inline void RemoveFromMask(uint32_t& mask, CollisionLayer layer)
    {
        mask &= ~ToBit(layer);
    }

    // マスクに含まれているか確認
    inline bool HasLayer(uint32_t mask, CollisionLayer layer)
    {
        return (mask & ToBit(layer)) != 0;
    }
}



#endif //COLLISION_HELPER_H