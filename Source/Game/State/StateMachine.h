#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "StateBase.h"

class StateMachine
{
public:
    StateMachine();
    virtual ~StateMachine() = default;

    // コピー禁止（オブジェクトの重複を防ぐ）
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator =(const StateMachine&) = delete;
    StateMachine(StateMachine&&) noexcept = delete;
    StateMachine& operator =(StateMachine&&) noexcept = delete;

    // 更新処理
    void Update(float deltaTime);

    // ステート変更
    void ChangeState(const std::string name);

    // ステート登録
    void RegisterState(std::unique_ptr<State> state);

    // ImGui描画
    void DrawImGui();

    // ステート名取得
    const char* GetStateName() const { return currentState == nullptr ? "" : currentState->GetName(); }

    // 現在のステートを取得
    State* GetCurrentState() const { return currentState; }

private:
    // 現在のステート
    State* currentState = nullptr;

    // 各ステートを保持する配列
    std::unordered_map<std::string, std::unique_ptr<State>> statePool;
};