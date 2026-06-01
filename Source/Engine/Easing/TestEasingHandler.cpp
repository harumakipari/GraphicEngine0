#include "pch.h"
#include "TestEasingHandler.h"

void TestEasingHandler::AddEasing(TestEaseType type, float start, float end, float duration, float back)
{
	//処理内容を設定
	EaseItem item{};
	ToEasingFunction(type, item.function, item.backFunction);
	item.easeData.timer = 0.0f;
	item.easeData.totalTime = duration;
	item.easeData.startValue = start;
	item.easeData.endValue = end;

	if (item.backFunction) {
		item.easeData.backValue = back;
	}

	//シーケンスに追加
	sequence.emplace_back(item);

	isCompleted = false;
}

void TestEasingHandler::AddEasing(const EaseItem& item)
{
	//シーケンスに追加
	sequence.emplace_back(item);

	isCompleted = false;
}

void TestEasingHandler::AddWait(float waitTime)
{
	//処理内容を設定
	EaseItem item{};
	item.function = nullptr;
	item.easeData.timer = 0.0f;
	item.easeData.totalTime = waitTime;

	//シーケンスに追加
	sequence.emplace_back(item);

	isCompleted = false;
}

void TestEasingHandler::Update(float& value, float deltaTime)
{
	if (sequence.empty()) return;

	auto& item = sequence.front();

	//先頭のイージング処理を実行する
	{
		item.easeData.timer += deltaTime;

		//イージング関数
		if (item.function != nullptr)
			value = item.function(item.easeData.timer, item.easeData.totalTime, item.easeData.endValue, item.easeData.startValue);
		else if (item.backFunction != nullptr) {
			value = item.backFunction(item.easeData.timer, item.easeData.totalTime, item.easeData.backValue, item.easeData.endValue, item.easeData.startValue);
		}
		progress = item.easeData.timer / item.easeData.totalTime;
		easedProgress = value / (item.easeData.endValue - item.easeData.startValue);

		if (item.easeData.timer >= item.easeData.totalTime)
		{
			if (item.function != nullptr || item.backFunction != nullptr)
				value = item.easeData.endValue;
			sequence.erase(sequence.begin());
		}
	}
	//全ての補完処理が完了したら完了フラグを立てる
	if (sequence.empty() && !isCompleted)
	{
		isCompleted = true;
		sequence.clear();
		ExecuteCompletedFunction();
		return;
	}
}

void TestEasingHandler::Clear()
{
	sequence.clear();
	isCompleted = false;
	completeFunction = nullptr;
}

void TestEasingHandler::ToEasingFunction(TestEaseType type, std::function<float(float, float, float, float)>& function, std::function<float(float, float, float, float, float)>& backFunction)
{
	function = nullptr;
	backFunction = nullptr;

	switch (type)
	{
	case TestEaseType::InQuad: function = Easing::InQuad<float>; break;
	case TestEaseType::OutQuad: function = Easing::OutQuad<float>; break;
	case TestEaseType::InOutQuad:function = Easing::InOutQuad<float>; break;
	case TestEaseType::InCubic:function = Easing::InCubic<float>; break;
	case TestEaseType::OutCubic:function = Easing::OutCubic<float>; break;
	case TestEaseType::InOutCubic:function = Easing::InOutCubic<float>; break;
	case TestEaseType::InQuart:function = Easing::InQuart<float>; break;
	case TestEaseType::OutQuart:function = Easing::OutQuart<float>; break;
	case TestEaseType::InOutQuart:function = Easing::InOutQuart<float>; break;
	case TestEaseType::InQuint:function = Easing::InQuint<float>; break;
	case TestEaseType::OutQuint:function = Easing::OutQuint<float>; break;
	case TestEaseType::InOutQuint:function = Easing::InOutQuint<float>; break;
	case TestEaseType::InSine:function = Easing::InSine<float>; break;
	case TestEaseType::OutSine:function = Easing::OutSine<float>; break;
	case TestEaseType::InOutSine:function = Easing::InOutSine<float>; break;
	case TestEaseType::InExp:function = Easing::InExp<float>; break;
	case TestEaseType::OutExp:function = Easing::OutExp<float>; break;
	case TestEaseType::InOutExp:function = Easing::InOutExp<float>; break;
	case TestEaseType::InCirc:function = Easing::InCirc<float>; break;
	case TestEaseType::OutCirc:function = Easing::OutCirc<float>; break;
	case TestEaseType::InOutCirc:function = Easing::InOutCirc<float>; break;
	case TestEaseType::InBounce:function = Easing::InBounce<float>; break;
	case TestEaseType::OutBounce:function = Easing::OutBounce<float>; break;
	case TestEaseType::InOutBounce:function = Easing::InOutBounce<float>; break;
	case TestEaseType::Linear:function = Easing::Linear<float>; break;
	case TestEaseType::OutElastic:function = Easing::OutElastic<float>; break;
	case TestEaseType::InBack:backFunction = Easing::InBack<float>; break;
	case TestEaseType::OutBack:backFunction = Easing::OutBack<float>; break;
	case TestEaseType::InOutBack:backFunction = Easing::InOutBack<float>; break;
	default:
		break;
	}
}