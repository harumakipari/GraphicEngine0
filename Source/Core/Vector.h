#pragma once
#include <DirectXMath.h>
/**
 * @brief 2 次元ベクトルの軽量構造体。
 * @details `XMFLOAT2` との相互変換や基本演算子を提供します。
 */
struct Vector2
{
	float x, y;
	Vector2(const Vector2&) = default;
	Vector2& operator=(const Vector2&) = default;

	Vector2(Vector2&&) = default;
	Vector2& operator=(Vector2&&) = default;

	constexpr Vector2(float x = 0.f, float y = 0.f) noexcept : x(x), y(y) {}
	explicit constexpr Vector2(const DirectX::XMFLOAT2& v) : x(v.x), y(v.y) {}

	explicit Vector2(_In_reads_(2) const float* pArray) noexcept : x(pArray[0]), y(pArray[1]) {}

	operator DirectX::XMFLOAT2() const { return DirectX::XMFLOAT2(x, y); }

	Vector2& operator+=(const Vector2& a) { x += a.x, y += a.y; return *this; }
	Vector2& operator-=(const Vector2& a) { x -= a.x, y -= a.y; return *this; }
	Vector2& operator*=(const Vector2& a) { x *= a.x, y *= a.y; return *this; }
	Vector2& operator/=(const Vector2& a) { x /= a.x, y /= a.y; return *this; }
	Vector2& operator*=(float s) { x *= s, y *= s; return *this; }
	Vector2& operator/=(float s) { x /= s, y /= s; return *this; }

	Vector2 operator+(const Vector2& a) const { return Vector2(this->x + a.x, this->y + a.y); }
	Vector2 operator-(const Vector2& a) const { return Vector2(this->x - a.x, this->y - a.y); }
	Vector2 operator*(const Vector2& a) const { return Vector2(this->x * a.x, this->y * a.y); }
	Vector2 operator/(const Vector2& a) const { return Vector2(this->x / a.x, this->y / a.y); }
	Vector2 operator*(float a) const { return Vector2(this->x * a, this->y * a); }
	Vector2 operator/(float a) const { return Vector2(this->x / a, this->y / a); }
	bool operator==(const Vector2& a) const { return (this->x == a.x && this->y == a.y); }

};

/**
 * @brief 3 次元ベクトルの軽量構造体。
 * @details 代表的な向きの定数や、`XMFLOAT3` との相互変換、基本演算子を提供します。
 */
struct Vector3
{
	static constexpr DirectX::XMFLOAT3 up{ 0,1,0 };
	static constexpr DirectX::XMFLOAT3 down{ 0,-1,0 };
	static constexpr DirectX::XMFLOAT3 right{ 1,0,0 };
	static constexpr DirectX::XMFLOAT3 left{ -1,0,0 };
	static constexpr DirectX::XMFLOAT3 forward{ 0,0,1 };
	static constexpr DirectX::XMFLOAT3 back{ 0,0,-1 };
	static constexpr DirectX::XMFLOAT3 zero{ 0,0,0 };

	/** @brief 2 つの XMFLOAT3 が等しいかを比較します。*/
	static bool Equal(const DirectX::XMFLOAT3& v0, const DirectX::XMFLOAT3& v1) { return (v0.x == v1.x && v0.y == v1.y && v0.z == v1.z); }

#if 1
	float x, y, z;
	Vector3(const Vector3&) = default;
	Vector3& operator=(const Vector3&) = default;

	Vector3(Vector3&&) = default;
	Vector3& operator=(Vector3&&) = default;

	constexpr Vector3(float x = 0.f, float y = 0.f, float z = 0.f) noexcept : x(x), y(y), z(z) {}
	explicit constexpr Vector3(const DirectX::XMFLOAT3& v) : x(v.x), y(v.y), z(v.z) {}

	explicit Vector3(_In_reads_(3) const float* pArray) noexcept : x(pArray[0]), y(pArray[1]), z(pArray[2]) {}

	operator DirectX::XMFLOAT3() const { return DirectX::XMFLOAT3(x, y, z); }

	Vector3& operator+=(const Vector3& a) { x += a.x, y += a.y, z += a.z; return *this; }
	Vector3& operator-=(const Vector3& a) { x -= a.x, y -= a.y, z -= a.z; return *this; }
	Vector3& operator*=(const Vector3& a) { x *= a.x, y *= a.y, z *= a.z; return *this; }
	Vector3& operator/=(const Vector3& a) { x /= a.x, y /= a.y, z /= a.z; return *this; }
	Vector3& operator*=(float s) { x *= s, y *= s, z *= s; return *this; }
	Vector3& operator/=(float s) { x /= s, y /= s, z /= s; return *this; }

	Vector3 operator+(const Vector3& a) const { return Vector3(this->x + a.x, this->y + a.y, this->z + a.z); }
	Vector3 operator-(const Vector3& a) const { return Vector3(this->x - a.x, this->y - a.y, this->z - a.z); }
	Vector3 operator*(const Vector3& a) const { return Vector3(this->x * a.x, this->y * a.y, this->z * a.z); }
	Vector3 operator/(const Vector3& a) const { return Vector3(this->x / a.x, this->y / a.y, this->z / a.z); }
	Vector3 operator*(float a) const { return Vector3(this->x * a, this->y * a, this->z * a); }
	Vector3 operator/(float a) const { return Vector3(this->x / a, this->y / a, this->z / a); }
	bool operator==(const Vector3& a) const { return (this->x == a.x && this->y == a.y && this->z == a.z); }

#endif
	/** @brief ベクトルの長さを返す。*/
	float Length() const;
	/** @brief ベクトルの長さの二乗を返す。*/
	float LengthSq() const;
	/** @brief 正規化したベクトルを返す。*/
	Vector3 Normalize() const;
	/** @brief 内積を計算する。*/
	float Dot(const Vector3& v) const;
	/** @brief 外積を計算する。*/
	Vector3 Cross(const Vector3& v) const;
};

