#pragma once
#include "json.hpp"
using namespace nlohmann;
#include "Core/Vector.h"
#include "Core/CoreColor.h"
#include "Engine/Effects/EffectManager.h"

template<typename T>
void from_json(const json& j, Range<T>& r)
{
	if (j.is_array() && j.size() == 2) {
		r.min = j.at(0).get<T>();
		r.max = j.at(1).get<T>();
	}
}

template<typename T>
void to_json(json& j, const Range<T>& r)
{
	j = json::array({ r.min, r.max });
}

void from_json(const json& j, Vector2& v)
{
	if (j.is_array() && j.size() == 2) {
		j[0].get_to(v.x);
		j[1].get_to(v.y);
	}
}

void to_json(json& j, const Vector2& v)
{
	j = json::array({ v.x, v.y });
}

void from_json(const json& j, Vector3& v)
{
	if (j.is_array() && j.size() == 3) {
		j[0].get_to(v.x);
		j[1].get_to(v.y);
		j[2].get_to(v.z);
	}
}

void to_json(json& j, const Vector3& v)
{
	j = json::array({ v.x, v.y, v.z });
}

void from_json(const json& j, CoreColor& c)
{
	if (j.is_array() && j.size() == 4) {
		j[0].get_to(c.r);
		j[1].get_to(c.g);
		j[2].get_to(c.b);
		j[3].get_to(c.a);
	}
}

void to_json(json& j, const CoreColor& c)
{
	j = json::array({ c.r, c.g, c.b, c.a });
}

