#include "pch.h"

#include "BehaviorTree.h"
#include "ActionBase.h"
#include "NodeBase.h"
#include "JudgementBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "BehaviorData.h"

// デストラクタ
BehaviorTree::~BehaviorTree()
{
}

void BehaviorTree::AddNode(std::string parentName,
	std::string entryName,
	int priority,
	SelectRule selectRule,
	std::unique_ptr<JudgementBase> judgment,
	std::unique_ptr<ActionBase> action)
{
	if (!parentName.empty())
	{
		std::weak_ptr<NodeBase> parentNode = root->SearchNode(parentName);

		if (auto parentShared = parentNode.lock())
		{
			std::shared_ptr<NodeBase> addNode = std::make_shared<NodeBase>(
				entryName,
				parentShared,
				priority,
				selectRule,
				std::move(judgment),
				std::move(action)
			);
			parentShared->AddChild(addNode);
		}
	}
	else
	{
		if (!root)
		{
			root = std::make_shared<NodeBase>(
				entryName,
				std::weak_ptr<NodeBase>{},  // 最上位の親がいない場合は空のweak_ptrを指定
				priority,
				selectRule,
				std::move(judgment),
				std::move(action)
			);
		}
	}
}


// 次に実行するノードを推論
NodeBase* BehaviorTree::ActiveNodeInference(BehaviorData* data)
{
	// データをリセットして開始
	data->Init();
	return root->Inference(data);
}

// シーケンスノードからの推論開始
NodeBase* BehaviorTree::SequenceBack(NodeBase* sequenceNode, BehaviorData* data)
{
	return sequenceNode->Inference(data);
}

// ノード実行
NodeBase* BehaviorTree::Run(NodeBase* actionNode, BehaviorData* data, float elapsedTime)
{
	// ノード実行
	ActionBase::State state = actionNode->Run(elapsedTime);

	// 正常終了
	if (state == ActionBase::State::Complete)
	{
		// シーケンスの途中かを判断
		NodeBase* sequenceNode = data->PopSequenceNode();
		// シーケンスノードがあればシーケンスノードから推論開始
		// シーケンスノードがなければnullptrを返す
		// シーケンスノードの子にシーケンスノードがあるときを考慮してループを使用する
		while (sequenceNode != nullptr)
		{
			NodeBase* result = SequenceBack(sequenceNode, data);
			if (result != nullptr)
			{
				return result;
			}
			sequenceNode = data->PopSequenceNode();
		}
		return nullptr;
	}
	else if (state == ActionBase::State::Failed) {
		// 失敗は終了
		return nullptr;
	}

	// 現状維持
	return actionNode;
}


