#pragma once
#include <functional>

#include "Easing.h"

// イージング種別。
enum class TestEaseType
{
	InQuad,
	OutQuad,
	InOutQuad,
	InCubic,
	OutCubic,
	InOutCubic,
	InQuart,
	OutQuart,
	InOutQuart,
	InQuint,
	OutQuint,
	InOutQuint,
	InSine,
	OutSine,
	InOutSine,
	InExp,
	OutExp,
	InOutExp,
	InCirc,
	OutCirc,
	InOutCirc,
	InBounce,
	OutBounce,
	InOutBounce,
	InBack,
	OutBack,
	InOutBack,
	Linear,
	OutElastic
};

class EasingHandler
{
public:
	// ステップ分のイージング要素。
	struct EaseItem
	{
		EaseData easeData{}; 
		std::function<float(float, float, float, float)> function; 
		std::function<float(float, float, float, float, float)> backFunction; 
	};

	EasingHandler() {}
	~EasingHandler() {}

	// イージング要素追加
	void AddEasing(TestEaseType type, float start, float end, float duration = 1.0f, float back = 1.70158f);
	// 既製のイージング要素を追加
	void AddEasing(const EaseItem& item);

	// 待機（ディレイ）を追加
	void AddWait(float waitTime);

	void Clear();

	void Update(float& value, float deltaTime);

	// 現在の進捗（0～1）
	float GetProgress() const { return progress; }

	//イージング済みの進捗（0～1）
	float GetEasedProgress() const { return easedProgress; }

	// 線形補間を行う
	template <class T>
	T Lerp(const T& a, const T& b, float t)
	{
		return a * (1.0f - t) + b * t;
	}
	// 任意型の補間値を返す
	template<typename T>
	T GetValue(const T& start, const T& end) const {
		return Lerp(start, end, easedProgress);
	}

	bool IsCompleted() const { return isCompleted; }

	// 完了時に実行する関数を設定
	void SetCompletedFunction(std::function<void()> function) { completeFunction = function; }

	// 現在のシーケンス長を返す
	size_t GetSequenceCount() const { return sequence.size(); }

	// 列挙型をイージング関数に変換
	static void ToEasingFunction(TestEaseType type, std::function<float(float, float, float, float)>& func, std::function<float(float, float, float, float, float)>& backFunc);

private:
	// 完了時コールバックがあれば実行
	void ExecuteCompletedFunction() { if (completeFunction != nullptr) completeFunction(); }

private:
	std::vector<EaseItem> sequence;
	bool isCompleted = false;
	float progress = 0.0f;
	float easedProgress = 0.0f;
	std::function<void()> completeFunction;
};