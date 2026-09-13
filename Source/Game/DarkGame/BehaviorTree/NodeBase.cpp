#include "pch.h"
#include "NodeBase.h"

#include "JudgementBase.h"
#include "BehaviorData.h"
#include "ActionBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

// デストラクタ
NodeBase::~NodeBase()
{
}

// ノード検索
std::weak_ptr<NodeBase> NodeBase::SearchNode(const std::string& searchName)
{
    // 自分の名前が一致
    if (name == searchName)
    {
        return shared_from_this();
    }

    // 子ノードを検索
    for (const auto& child : children)
    {
        std::weak_ptr<NodeBase> result = child->SearchNode(searchName);
        if (!result.expired())
        {
            return result;
        }
    }

    return std::weak_ptr<NodeBase>();
}

// ノード推論
NodeBase* NodeBase::Inference(BehaviorData* data)
{
    std::vector<std::shared_ptr<NodeBase>> list;
    NodeBase* result = nullptr;

    // childrenの数だけループを行う。
    for (int i = 0; i < children.size(); i++)
    {
        // children.at(i)->judgmentがnullptrでなければ
        if (children.at(i)->judgment != nullptr)
        {
            // tureであればlistにchildren.at(i)を追加していく
            if (children.at(i)->judgment->Judgment())
            {
                list.push_back(children.at(i));
            }
        }
        else 
        {
            list.push_back(children.at(i));
        }
    }

    // 選択ルールでノード決め
    switch (selectRule)
    {
        // 優先順位
    case BehaviorTree::SelectRule::Priority:
        result = SelectPriority(&list);
        break;
        // ランダム
    case BehaviorTree::SelectRule::Random:
        result = SelectRandom(&list);
        break;
    case BehaviorTree::SelectRule::AttackRandom:
        result = SelectAttackRandom(&list);
        break;
        // シーケンス
    case BehaviorTree::SelectRule::Sequence:
    case BehaviorTree::SelectRule::SequentialLooping:
        result = SelectSequence(&list, data);
        break;
    }

    if (result != nullptr)
    {
        // 行動があれば終了
        if (result->HasAction() == true)
        {
            return result;
        }
        else 
        {
            // 決まったノードで推論開始
            result = result->Inference(data);
        }
    }

    return result;
}

// 優先順位でノード選択
NodeBase* NodeBase::SelectPriority(std::vector<std::shared_ptr<NodeBase>>* list)
{
    NodeBase* selectNode = nullptr;
    int priority = INT_MAX;
    // 優先順位は数値が小さいほど高い
    for (int i = 0; i < list->size(); i++)
    {
        auto node = list->at(i);
        int nodePriority = node->GetPriority();
        if (nodePriority < priority)
        {
            priority = nodePriority;
            selectNode = node.get();
        }
    }
    return selectNode;
}


// ランダムでノード選択
NodeBase* NodeBase::SelectRandom(std::vector<std::shared_ptr<NodeBase>>* list)
{
    if (list->empty()) return nullptr;
    int selectNo = 0;
    // listのサイズで乱数を取得してselectNoに格納
    int random = rand() % list->size();
    selectNo = random;

    // listのselectNo番目の実態をリターン
    return list->at(selectNo).get();
}

NodeBase* NodeBase::SelectAttackRandom(std::vector<std::shared_ptr<NodeBase>>* list)
{
    if (!list || list->empty()) return nullptr;

    std::vector<NodeBase*> nonFast;
    NodeBase* fast = nullptr;
    for (const auto& node : *list)
    {
        if (node->GetName() == "FastComboPlan") fast = node.get();
        else nonFast.push_back(node.get());
    }

    const bool nearFront = owner && owner->IsNearFrontForAttackSelection();
    const float fastComboBaseProbability = nearFront && fast
        ? owner->GetNearFrontFastComboProbability() : 0.0f;
    const float fastComboPenalty = fast && owner
        ? owner->GetAttackRepeatPenaltyForPlan(fast->GetName()) : 1.0f;
    const float fastComboEffectiveProbability = std::clamp(
        fastComboBaseProbability * fastComboPenalty, 0.0f, 1.0f);
    const float roll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    NodeBase* selected = nullptr;
    const char* mode = nearFront && fast ? "NearFrontWeighted" : "Uniform";

    if (nearFront && fast && (nonFast.empty() || roll < fastComboEffectiveProbability))
    {
        selected = fast;
    }
    else if (!nonFast.empty())
    {
        float totalWeight = 0.0f;
        for (const NodeBase* node : nonFast)
            totalWeight += owner ? owner->GetAttackRepeatPenaltyForPlan(node->GetName()) : 1.0f;

        const float weightedRoll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * totalWeight;
        float accumulatedWeight = 0.0f;
        for (NodeBase* node : nonFast)
        {
            accumulatedWeight += owner ? owner->GetAttackRepeatPenaltyForPlan(node->GetName()) : 1.0f;
            if (weightedRoll <= accumulatedWeight)
            {
                selected = node;
                break;
            }
        }
        if (!selected)
            selected = nonFast.back();
    }
    else
    {
        selected = fast;
    }

    const NodeBase* repeatPenaltyTarget = nullptr;
    if (owner)
    {
        for (const auto& node : *list)
        {
            if (owner->GetAttackRepeatPenaltyForPlan(node->GetName()) < 1.0f)
            {
                repeatPenaltyTarget = node.get();
                break;
            }
        }
    }

    const float debugBaseWeight = selected ? 1.0f : 0.0f;
    const float debugEffectiveWeight = repeatPenaltyTarget && owner
        ? owner->GetAttackRepeatPenaltyForPlan(repeatPenaltyTarget->GetName())
        : debugBaseWeight;
    const bool repeatPenaltyApplied = repeatPenaltyTarget != nullptr;
    if (owner)
    {
        owner->RecordAttackSelectorDebug(mode, static_cast<int>(list->size()),
            fastComboBaseProbability, fastComboEffectiveProbability, roll,
            selected ? selected->GetName().c_str() : "None",
            repeatPenaltyTarget ? repeatPenaltyTarget->GetName().c_str() : "None",
            debugBaseWeight, debugEffectiveWeight, repeatPenaltyApplied);
    }
    return selected;
}
// シーケンス・シーケンシャルルーピングでノード選択
NodeBase* NodeBase::SelectSequence(std::vector<std::shared_ptr<NodeBase>>* list, BehaviorData* data)
{
    int step = 0;

    // 指定されている中間ノードのシーケンスがどこまで実行されたか取得する
    step = data->GetSequenceStep(name);

    // 中間ノードに登録されているノード数以上の場合、
    if (step >= children.size())
    {
        // ルールによって処理を切り替える
        // ルールがBehaviorTree::SelectRule::SequentialLoopingのときは最初から実行するため、stepに0を代入
        // ルールがBehaviorTree::SelectRule::Sequenceのときは次に実行できるノードがないため、nullptrをリターン
        if (selectRule == BehaviorTree::SelectRule::SequentialLooping)
        {
            step = 0;
        }
        else /*if( selectRule == BehaviorTree05::SelectRule::Sequence)*/
        {
            return nullptr;
        }
    }
    // 実行可能リストに登録されているデータの数だけループを行う
    for (auto itr = list->begin(); itr != list->end(); itr++)
    {
        // 子ノードが実行可能リストに含まれているか
        if (children.at(step)->GetName() == (*itr)->GetName())
        {
            // ①スタックにはdata->PushSequenceNode関数を使用する。保存するデータは実行中の中間ノード。
            // ②また、次に実行する中間ノードとステップ数を保存する
            // 　保存にはdata->SetSequenceStep関数を使用。
            // 　保存データは中間ノードの名前と次のステップ数です(step + 1) インクリメントすると次のステップになってしまうから
            // ③ステップ番号目の子ノードを実行ノードとしてリターンする
            data->PushSequenceNode(this);
            data->SetSequenceStep(name, step + 1);
            return children.at(step).get();
        }
    }
    // 指定された中間ノードに実行可能ノードがないのでnullptrをリターンする
    return nullptr;
}

// 判定
void NodeBase::ResetActionRuntimes()
{
    if (action) action->ResetRuntime();
    for (const auto& child : children) child->ResetActionRuntimes();
}

bool NodeBase::Judgment()
{
    // judgmentあればメンバ関数Judgment()実行した結果をリターン。
    if (judgment)
    {
        return judgment->Judgment();
    }
    return true;
}

// ノード実行
ActionBase::State NodeBase::Run(float elapsedTime)
{
    // actionあればメンバ関数Run()実行した結果をリターン。
    if (action)
    {
        return action->Run(elapsedTime);
    }

    return ActionBase::State::Failed;
}
