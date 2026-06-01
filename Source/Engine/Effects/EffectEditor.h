#pragma once
#include "Engine/Effects/EffectManager.h"
#include "Engine/Effects/CoreComputeParticleSystem.h"

#include "Core/Vector.h"
#include "Core/CoreColor.h"

class EffectEditor
{
public:
	EffectEditor() = default;
	~EffectEditor() = default;

	//初期化
	static void Initialize();

	//エディタ表示
	static void Show();
	//エディタが開いているか
	static bool IsOpen();

	//エディタGUI描画
	static void DrawGUI();

private:
#ifdef USE_IMGUI
	// 範囲指定スライダー描画 ( int 用 )
	static bool DrawRangeInt(const char* label, Range<int>& range, int speed = 1, int min = 0, int max = 0);
	// 範囲指定スライダー描画 ( unsigned int 用 )
	static bool DrawRangeUInt(const char* label, Range<unsigned int>& range, unsigned int speed = 1, unsigned int min = 0, unsigned int max = 0);
	// 範囲指定スライダー描画 ( float 用 )
	static bool DrawRangeFloat(const char* label, Range<float>& range, float speed = 0.1f, float min = 0.0f, float max = 0.0f);
	// 範囲指定スライダー描画 ( Vector2 用 )
	static bool DrawRangeVector2(const char* label, Range<Vector2>& range, float speed = 0.1f, float min = 0.0f, float max = 0.0f);
	// 範囲指定スライダー描画 ( Vector3 用 )
	static bool DrawRangeVector3(const char* label, Range<Vector3>& range, float speed = 0.1f, float min = 0.0f, float max = 0.0f);
	// 範囲指定スライダー描画 ( Color 用 )
	static bool DrawRangeColor(const char* label, Range<CoreColor>& range);

	// スライダー描画 ( float 用 )
	static bool DrawFloat(const char* label, float& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f);
	// ベクトルスライダー描画 ( Vector3 用 )
	static bool DrawVector3(const char* label, Vector3& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f);

	static void DrawCurve(const char* label, FloatCurve& curve, bool& dirty);

#endif // USE_IMGUI

private:
	//エディタが開いているか
	//static inline bool isOpen = false;
	static inline bool isOpen = true;

	static inline EffectHandle currentEffectHandle = -1; // 現在編集中のエフェクトハンドル
};