#include "pch.h"
#include "InteractableActor.h"

#include "Engine/Scene/Scene.h"
#include "Physics/CollisionFunction.h"

void InteractableActor::Initialize(const Transform& transform)
{
    auto uiManager = GetOwnerScene()->GetUIManager();
    // インタラクト可能なUIを追加する
    interactUiComponent = std::make_unique<UIImageComponent>("./Data/Textures/UI/button_a.png", "interactUI");
    interactUiComponent->SetWorldPosition({ 0.0f, 0.0f });
    interactUiComponent->SetSize({ 140.0f, 140.0f });
    interactUiComponent->SetPivot({ 0.5f, 0.5f }); // 矢印の根元をプレイヤーの位置に合わせる
    interactUiComponent->SetVisible(false);
    uiManager->Add(interactUiComponent);

    controlButton = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/UI/button_a.png");
    keyboardButton = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/UI/button_enter.png");
}


void InteractableActor::Update(float deltaTime)
{
    if (InputSystem::IsGamepadConnected())
    {//　コントローラー対応
        interactUiComponent->SetTexture(controlButton);
    }
    else
    {
        interactUiComponent->SetTexture(keyboardButton);
    }
    interactUiComponent->SetVisible(canInteract && !interacted);
    interactUiComponent->SetWorldPosition(interactUiWorldPos);
}

void InteractableActor::Interact()
{
    interacted = true;
}