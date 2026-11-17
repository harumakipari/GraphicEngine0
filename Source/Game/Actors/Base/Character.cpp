#include "pch.h"
#include "Character.h"

#include "Engine/Input/InputSystem.h"

void Character::HandleCommonAnimationNotifyEvent(const AnimationNotifyEvent& event)
{
    if (event.type != AnimationNotifyEvent::Type::ControllerRumble)
        return;

    InputSystem::RequestRumble(
        event.leftMotorStrength,
        event.rightMotorStrength,
        event.duration);
}

void Character::UpdateDirectionVectors()
{
    using namespace DirectX;

    XMVECTOR q = XMLoadFloat4(&GetQuaternionRotation());

    XMStoreFloat3(
        &front,
        XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q));

    XMStoreFloat3(
        &right,
        XMVector3Rotate(XMVectorSet(1, 0, 0, 0), q));

    XMStoreFloat3(
        &up,
        XMVector3Rotate(XMVectorSet(0, 1, 0, 0), q));
}


void Character::Update(const float deltaTime)
{
    // front,up,rightを更新する
    UpdateDirectionVectors();

    const bool editorPreviewActive = IsAnimationEditorPreviewActive();
    if (stateMachine && !editorPreviewActive)
    {
        stateMachine->Update(deltaTime);
    }

    // Preview対象ActorだけLocomotionによるAnimation要求も停止する。
    if (!editorPreviewActive)
        UpdateLocomotionAnimation();

    for (auto& controller : animationControllers | std::views::values)
    {
        controller->OnUpdate(deltaTime);
    }

};

bool Character::IsAnimationEditorPreviewActive() const
{
    for (const auto& controller : animationControllers | std::views::values)
    {
        if (controller && controller->IsEditorPreviewActive())
            return true;
    }
    return false;
}


// ステートマシンをセットする
void Character::SetStateMachine(const std::shared_ptr<StateMachine>& stateMachine)
{
    this->stateMachine = stateMachine;
}

// ステートマシンを取得する
std::shared_ptr<StateMachine> Character::GetStateMachine()
{
    return stateMachine;
}

// 使用
void Character::PlayBodyAnimation(const std::string& name, const bool loop , const bool blend , const float blendTime, const bool ignoreRootMotion) const
{
    if (const auto controller = GetAnimationController(animationBodyControllerName))
    {
        controller->ResetRootMotion(name, loop, blend, blendTime);
        controller->SetIgnoreRootMotion(ignoreRootMotion);
    }
}
