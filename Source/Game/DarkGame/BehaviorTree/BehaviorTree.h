#pragma once
#include "ActionBase.h"


class ActionBase;
class JudgementBase;
class NodeBase;
class BehaviorData;
class GruxEnemy;

class BehaviorTree
{
public:
	// 選択ルール
	enum class SelectRule
	{
		Non,				// 無い末端ノード用
		Priority,			// 優先順位
		Sequence,			// シーケンス
		SequentialLooping,	// シーケンシャルルーピング
		Random,				// ランダム
	};

public:
	BehaviorTree() :root(nullptr), owner(nullptr) {}
	BehaviorTree(GruxEnemy* enemy) :root(nullptr), owner(enemy) {}
	~BehaviorTree();

	// ルートノードから実行ノードを推論する
	NodeBase* ActiveNodeInference(BehaviorData* data);

	// シーケンスノード、シーケンシャルルーピングノードで次に実行するノードを推論する
	NodeBase* SequenceBack(NodeBase* sequenceNode, BehaviorData* data);

	// ビヘイビアツリーにノード追加
	void AddNode(std::string parentName, std::string entryName, int priority, SelectRule selectRule, std::unique_ptr<JudgementBase> judgment, std::unique_ptr<ActionBase> action);

	// ビヘイビアツリーを実行する関数
	NodeBase* Run(NodeBase* actionNode, BehaviorData* data, float elapsedTime);
    ActionBase::State GetLastRunResult() const { return lastRunResult; }

	// 登録されているノードを削除する関数
	void NodeAllClear();

private:
	// ルートノード
	std::shared_ptr<NodeBase> root;
	GruxEnemy* owner;
    ActionBase::State lastRunResult = ActionBase::State::Run;
};