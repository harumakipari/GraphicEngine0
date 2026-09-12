#pragma once

#include "BehaviorTree.h"
#include "ActionBase.h"
#include "JudgementBase.h"
class GruxEnemy;

class BehaviorData;

// ノード
class NodeBase : public std::enable_shared_from_this<NodeBase>
{
public:
	// コンストラクタ
	NodeBase(std::string name,
		std::weak_ptr<NodeBase> parent,
		int priority,
		BehaviorTree::SelectRule selectRule,
		std::unique_ptr<JudgementBase> judgment,
		std::unique_ptr<ActionBase> action, GruxEnemy* owner = nullptr)
		: name(std::move(name)),
		parent(parent),
		priority(priority),
		selectRule(selectRule),
		judgment(std::move(judgment)),
		action(std::move(action)), owner(owner)
	{
	}
	// デストラクタ
	~NodeBase();
	// 名前ゲッター
	std::string GetName() { return name; }
	// 優先順位ゲッター
	int GetPriority() { return priority; }
	// 子ノード追加
	void AddChild(std::shared_ptr<NodeBase> child) { children.push_back(child); }
	// 行動データを持っているか
	bool HasAction() { return action != nullptr ? true : false; }
	// 実行可否判定
	bool Judgment();
	// 優先順位選択
	NodeBase* SelectPriority(std::vector<std::shared_ptr<NodeBase>>* list);
	// ランダム選択
	NodeBase* SelectRandom(std::vector<std::shared_ptr<NodeBase>>* list);
	NodeBase* SelectAttackRandom(std::vector<std::shared_ptr<NodeBase>>* list);
	// シーケンス選択
	NodeBase* SelectSequence(std::vector<std::shared_ptr<NodeBase>>* list, BehaviorData* data);
	// ノード検索
	std::weak_ptr<NodeBase> SearchNode(const std::string& searchName);
	// ノード推論
	NodeBase* Inference(BehaviorData* data);
	// 実行
	ActionBase::State Run(float elapsedTime);
	void ResetActionRuntimes();
	std::vector<std::shared_ptr<NodeBase>>		children;		// 子ノード
protected:
	std::string						name;			// 名前
	BehaviorTree::SelectRule		selectRule;		// 選択ルール
	std::unique_ptr<JudgementBase>	judgment;		// 判定クラス
	std::unique_ptr<ActionBase>	action;			// 実行クラス
	unsigned int					priority;		// 優先順位
	std::weak_ptr<NodeBase>		parent;
	GruxEnemy* owner = nullptr;				// 親ノード
};