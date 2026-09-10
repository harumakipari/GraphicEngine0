#include "pch.h"
#include "ActionDerived.h"

ActionBase::State StartJumpAttack::Run(float elapsedTime)
{
    // 

    return ActionBase::State::Complete;
}

ActionBase::State ExecuteJumpAttack::Run(float elapsedTime)
{
    // ジャスト回避されたら
    return ActionBase::State::Failed;

    // 攻撃成功したら
    return ActionBase::State::Complete;

    // 実行中を返す
    return ActionBase::State::Run;
}
