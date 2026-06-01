#pragma once
#include <DirectXMath.h>
using namespace DirectX;
//カラー（XMFLOAT4と互換性あり）
struct CoreColor
{
	float r;
	float g;
	float b;
	float a;

	CoreColor(const CoreColor&) = default;
	CoreColor& operator=(const CoreColor&) = default;

	CoreColor(CoreColor&&) = default;
	CoreColor& operator=(CoreColor&&) = default;

	constexpr CoreColor(float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f) noexcept : r(r), g(g), b(b), a(a) {}
	explicit constexpr CoreColor(const XMFLOAT4& color) : r(color.x), g(color.y), b(color.z), a(color.w) {}

	explicit CoreColor(_In_reads_(4) const float* pArray) noexcept : r(pArray[0]), g(pArray[1]), b(pArray[2]), a(pArray[3]) {}

	operator XMFLOAT4() const { return XMFLOAT4(r, g, b, a); }

	//Color operator=(const XMFLOAT4& color) { return { color.x,color.y,color.z,color.w }; }
	CoreColor& operator+=(const CoreColor& c) { r += c.r, g += c.g, b += c.b, a += c.a; return *this; }
	CoreColor& operator-=(const CoreColor& c) { r -= c.r, g -= c.g, b -= c.b, a -= c.a; return *this; }
	CoreColor& operator*=(const CoreColor& c) { r *= c.r, g *= c.g, b *= c.b, a *= c.a; return *this; }
	CoreColor& operator/=(const CoreColor& c) { r /= c.r, g /= c.g, b /= c.b, a /= c.a; return *this; }
	CoreColor& operator*=(float scale) { r *= scale, g *= scale, b *= scale, a *= scale; return *this; }
	CoreColor& operator/=(float scale) { r /= scale, g /= scale, b /= scale, a /= scale; return *this; }

	CoreColor operator+(const CoreColor& c) const { return { this->r + c.r, this->g + c.g, this->b + c.b, this->a + c.a }; }
	CoreColor operator-(const CoreColor& c) const { return { this->r - c.r, this->g - c.g, this->b - c.b, this->a - c.a }; }
	CoreColor operator*(const CoreColor& c) const { return { this->r * c.r, this->g * c.g, this->b * c.b, this->a * c.a }; }
	CoreColor operator/(const CoreColor& c) const { return { this->r / c.r, this->g / c.g, this->b / c.b, this->a / c.a }; }
	CoreColor operator*(float scale) const { return { this->r * scale, this->g * scale, this->b * scale, this->a * scale }; }
	CoreColor operator/(float scale) const { return { this->r / scale, this->g / scale, this->b / scale, this->a / scale }; }
	bool operator==(CoreColor& c) { return (r == c.r && g == c.g && b == c.b && a == c.a); }
	bool operator!=(CoreColor& c) { return (r != c.r || g != c.g || b != c.b || a != c.a); }
	bool operator==(const CoreColor& c) const { return (r == c.r && g == c.g && b == c.b && a == c.a); }
	bool operator!=(const CoreColor& c) const { return (r != c.r || g != c.g || b != c.b || a != c.a); }

	static const CoreColor Red;
	static const CoreColor Green;
	static const CoreColor Blue;
	static const CoreColor Yellow;
	static const CoreColor Orange;
	static const CoreColor Purple;
	static const CoreColor White;
	static const CoreColor Black;

	static void ConvertToPastelColors(CoreColor& color);
	/**
	 * @brief 16進数カラーコード（0xRRGGBB）をRGB（0～1）に変換する
	 * @param hexColor 0xRRGGBB形式のカラーコード
	 * @param color 変換後のColor構造体（r, g, b, a）
	 * @param alpha アルファ値（省略時は1.0）
	 */
	static void HexToRGB(uint32_t hexColor, CoreColor& color, float alpha = 1.f);
	/**
	* @brief 色を少し暗くする
	* @param color 対象の色（0～1の範囲）
	* @param factor 暗くする割合（0.0～1.0、デフォルトは0.8で20%暗くする）
	*/
	static void DarkenColor(CoreColor& color, float factor = 0.8f);
};

namespace ColorHelper
{
	static inline DirectX::XMFLOAT4 HSVtoRGB(float h, float s, float v, float a)
	{
		h = fmodf(h, 360.0f);
		if (h < 0.0f) h += 360.0f;

		s = std::clamp(s, 0.0f, 1.0f);
		v = std::clamp(v, 0.0f, 1.0f);
		a = std::clamp(a, 0.0f, 1.0f);

		float c = v * s;                     // chroma
		float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
		float m = v - c;

		float r = 0, g = 0, b = 0;

		if (h < 60.0f) { r = c; g = x; b = 0; }
		else if (h < 120.0f) { r = x; g = c; b = 0; }
		else if (h < 180.0f) { r = 0; g = c; b = x; }
		else if (h < 240.0f) { r = 0; g = x; b = c; }
		else if (h < 300.0f) { r = x; g = 0; b = c; }
		else { r = c; g = 0; b = x; }

		return {
			r + m,
			g + m,
			b + m,
			a
		};
	}
}