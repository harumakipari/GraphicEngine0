#pragma once

class Character;

class State
{
public:
    State(Character* actor) :owner(actor) {}
    virtual ~State() = default;

    // コピー禁止（オブジェクトの重複を防ぐ）
    State(const State&) = delete;
    State& operator =(const State&) = delete;
    State(State&&) noexcept = delete;
    State& operator =(State&&) noexcept = delete;

    // ステートに入った時のメソッド
    virtual void Enter() = 0;

    // ステートで実行するメソッド
    virtual void Execute(float deltaTime) = 0;

    // ステージから出ていくときのメソッド
    virtual void Exit() = 0;

    virtual const char* GetName() const = 0;


protected:
    Character* owner = nullptr;


};

