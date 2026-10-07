#include "pch.h"
#include "InteractableActor.h"

#include "Engine/Scene/Scene.h"
#include "Physics/CollisionFunction.h"

void InteractableActor::Initialize(const Transform& transform)
{
    auto uiManager = GetOwnerScene()->GetUIManager();
    // �C���^���N�g�\��UI��ǉ�����
    interactUiComponent = std::make_unique<UIImageComponent>("./Data/Textures/UI/button_a.png", "interactUI");
    interactUiComponent->SetWorldPosition({ 0.0f, 0.0f });
    interactUiComponent->SetSize({ 140.0f, 140.0f });
    interactUiComponent->SetPivot({ 0.5f, 0.5f }); // ���̍�����v���C���[�̈ʒu�ɍ��킹��
    interactUiComponent->SetVisible(false);
    interactUiComponent->SetScale({ 0.6f,0.6f });
    uiManager->Add(interactUiComponent);

    controlButton = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/UI/button_a.png");
    keyboardButton = std::make_shared<Sprite>(Graphics::GetDevice(), L"./Data/Textures/UI/button_enter.png");
}


void InteractableActor::Update(float deltaTime)
{
    if (InputSystem::IsGamepadConnected())
    {//�@�R���g���[���[�Ή�
        interactUiComponent->SetTexture(controlButton);
        interactUiComponent->SetScale({ 0.6f,0.6f });
    }
    else
    {
        interactUiComponent->SetTexture(keyboardButton);
        interactUiComponent->SetScale({ 1.2f,1.2f });
    }
    interactUiComponent->SetVisible(canInteract && !interacted);
    interactUiComponent->SetWorldPosition(interactUiWorldPos);
}

void InteractableActor::Interact()
{
    interacted = true;
}

void InteractableActor::CompleteInteraction()
{
    canInteract = false;
    interacted = true;
    if (interactUiComponent) interactUiComponent->SetVisible(false);
}
