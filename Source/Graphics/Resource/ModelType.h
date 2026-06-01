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
};

enum class ObjectType :int
{
    Default = 0,
    Player,
    Enemy,
    Stage,
    NotSSR, // SSRとかをつけたくないステージのオブジェクト
    Door, // メタリックを下げて、ラフネスを上げるため
};