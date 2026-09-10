#include "pch.h"
#include "BehaviorData.h"
#include "NodeBase.h"

// シーケンスノードのポップ
NodeBase* BehaviorData::PopSequenceNode()
{
	// 空ならNULL
	if (sequenceStack.empty() != 0)
	{
		return nullptr;
	}
	NodeBase* node = sequenceStack.top();
	if (node != nullptr)
	{
		// 取り出したデータを削除
		sequenceStack.pop();
	}
	return node;
}

// シーケンスステップのゲッター
int BehaviorData::GetSequenceStep(std::string name)
{
	if (runSequenceStepMap.count(name) == 0)
	{
		runSequenceStepMap.insert(std::make_pair(name, 0));
	}

	return runSequenceStepMap.at(name);
}

// シーケンスステップのセッター
void BehaviorData::SetSequenceStep(std::string name, int step)
{
	runSequenceStepMap.at(name) = step;
}

// 初期化
void BehaviorData::Init()
{
	runSequenceStepMap.clear();
	while (sequenceStack.size() > 0)
	{
		sequenceStack.pop();
	}
}