#pragma once
#include <json.hpp>

using json = nlohmann::json;

namespace DirectX
{
    inline void to_json(nlohmann::json& j, const XMFLOAT4& v)
    {
        j = { v.x, v.y, v.z, v.w };
    }

    inline void from_json(const nlohmann::json& j, XMFLOAT4& v)
    {
        v = XMFLOAT4(j[0], j[1], j[2], j[3]);
    }
    inline void to_json(nlohmann::json& j, const XMFLOAT3& v)
    {
        j = { v.x, v.y, v.z };
    }

    inline void from_json(const nlohmann::json& j, XMFLOAT3& v)
    {
        v = XMFLOAT3(j[0], j[1], j[2]);
    }
    inline void to_json(nlohmann::json& j, const XMFLOAT2& v)
    {
        j = { v.x, v.y };
    }
    inline void from_json(const nlohmann::json& j, XMFLOAT2& v)
    {
        v = XMFLOAT2(j[0], j[1]);
    }
}