#pragma once
#include "Core/Actor.h"
#include "Game/DarkGame/Interactable.h"
#include "UI/Widgets/Widget.h"

class InteractableActor :public Actor, public IInteractable
{
public:
    InteractableActor(const std::string& actorName) :Actor(actorName) {}

    void Initialize(const Transform& transform) override;

    void Update(float deltaTime) override;

    virtual void Interact() override;
    void CompleteInteraction();

    float GetInteractRange() const
    {
        return interactRange;
    }

    void SetInteractRange(const float newRange)
    {
        interactRange = newRange;
    }

    DirectX::XMFLOAT3 GetInteractOffset() const
    {
        return interactOffset;
    }

    void SetInteractOffset(const DirectX::XMFLOAT3& newOffset)
    {
        interactOffset = newOffset;
    }

    void SetInteractDegree(const float degree)
    {
        interactDegree = degree;
    }

    float GetInteractRadian() const
    {
        return DirectX::XMConvertToRadians(interactDegree);
    }

    void DrawImGuiDetails() override
    {
#ifdef USE_IMGUI
        ImGui::DragFloat(U8("�C���^���N�g����������͈�"), &interactRange, 0.1f, 0.0f, 5.0f);
        ImGui::DragFloat(U8("�C���^���N�g����������p�x"), &interactDegree, 1.0f, 0.0f, 180.0f);
        ImGui::DragFloat3(U8("�C���^���N�g�͈͂̃I�t�Z�b�g"), &interactOffset.x, 0.1f);
        ImGui::DragFloat2(U8("�C���^���N�g��UI�̍��W"), &interactUiWorldPos.x, 1.0f);
#endif
    }

protected:
    float interactRange = 2.0f;
    float interactDegree = 0.0f;
    DirectX::XMFLOAT3 interactOffset = { 0.0f, 0.0f, 0.0f }; 

    DirectX::XMFLOAT2 interactUiWorldPos = { 0.0f,0.0f };

    std::shared_ptr<UIImageComponent> interactUiComponent;  
    std::shared_ptr<Sprite> controlButton;
    std::shared_ptr<Sprite> keyboardButton;

};