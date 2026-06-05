#include "pch.h"
#include "StateMachine.h"

#include "Engine/Debug/Assert.h"


StateMachine::StateMachine()
{
    statePool.clear();
}

// 更新処理
void StateMachine::Update(float deltaTime)
{
    // 現在のステートを実行
    if (!currentState)
        return;
    currentState->Execute(deltaTime);
}

// ステート変更
void StateMachine::ChangeState(const std::string name)
{
    if (currentState)
    {
        currentState->Exit();
    }

    // ステートの中から指定されたステートがあるか検索
    auto it = statePool.find(name);
    if (it != statePool.end())
    {
        // 登録されていれば指定されたステートに更新
        currentState = it->second.get();
        // また指定されたステートのEnter関数の実行
        currentState->Enter();
    }
    else
    {
        // 指定されたステートが存在しない
        _ASSERT_EXPR_A(it != statePool.end(), "Not Found State.");
    }
}

// ステート登録
void StateMachine::RegisterState(std::unique_ptr<State> state)
{
    std::string name = state->GetName();
    statePool[name] = std::move(state);
}


// ImGui描画
void StateMachine::DrawImGui()
{
    ImGui::SeparatorText("StateMachine");
    ImGui::Text("Current State: %s", currentState ? GetStateName() : "None");
}