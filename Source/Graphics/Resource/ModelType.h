#pragma once

namespace ModelTypes
{
    enum class ModelMode : uint8_t
    {
        SkeletalMesh,
        StaticMesh,
        InstancedStaticMesh
    };
}

enum class MaterialType :int
{
    Default = 0,
    Hair,
    Fur,
    Skin,
    Eye,
    Metallic,   // メタリックにする場所(現在は敵の黄色の部分）
    Cloth,  // 服マテリアル
};

enum class ObjectType :int
{
    Default = 0,
    Player,
    Enemy,
    Stage,
    NotSSR, // SSRとかをつけたくないステージのオブジェクト
    Door, // メタリックを下げて、ラフネスを上げるため
    Furniture,
    EnemyEye, // 敵の目の表現用 暗い中で光る用
    NoLighting, // ライティング無し用
};