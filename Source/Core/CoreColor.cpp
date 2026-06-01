#include "pch.h"
#include "CoreColor.h"
// 定数の定義
const CoreColor CoreColor::Red = CoreColor(1.0f, 0.0f, 0.0f);
const CoreColor CoreColor::Green = CoreColor(0.0f, 1.0f, 0.0f);
const CoreColor CoreColor::Blue = CoreColor(0.0f, 0.0f, 1.0f);
const CoreColor CoreColor::Yellow = CoreColor(1.0f, 1.0f, 0.0f);
const CoreColor CoreColor::Orange = CoreColor(1.0f, 0.5f, 0.0f);
const CoreColor CoreColor::Purple = CoreColor(1.0f, 0.0f, 1.0f);
const CoreColor CoreColor::White = CoreColor(1.0f, 1.0f, 1.0f);
const CoreColor CoreColor::Black = CoreColor(0.0f, 0.0f, 0.0f);

void CoreColor::ConvertToPastelColors(CoreColor& color) {

	color.r = (color.r + 1) * 0.5f;
	color.g = (color.g + 1) * 0.5f;
	color.b = (color.b + 1) * 0.5f;
}
void CoreColor::DarkenColor(CoreColor& color, float factor)
{
	// 各成分を指定した割合で暗くする
	color.r *= factor;
	color.g *= factor;
	color.b *= factor;
	// アルファ値は変更しない
}
void CoreColor::HexToRGB(uint32_t hexColor, CoreColor& color, float alpha)
{
	// 赤成分を抽出し、0～1の範囲に変換
	color.r = static_cast<float>((hexColor >> 16) & 0xFF) / 255.0f;
	// 緑成分を抽出し、0～1の範囲に変換
	color.g = static_cast<float>((hexColor >> 8) & 0xFF) / 255.0f;
	// 青成分を抽出し、0～1の範囲に変換
	color.b = static_cast<float>(hexColor & 0xFF) / 255.0f;
	// アルファ値を設定
	color.a = alpha;
}