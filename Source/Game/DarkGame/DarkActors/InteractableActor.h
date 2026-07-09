#pragma once
#include "Core/Actor.h"
#include "Game/DarkGame/Interactable.h"

class InteractableActor :public Actor, public IInteractable
{
public:
    InteractableActor(const std::string& actorName) :Actor(actorName) {}

    virtual void Interact() override {}

    // インタラクト可能な範囲を取得する
    float GetInteractRange() const
    {
        return interactRange;
    }

    // インタラクト可能な範囲を設定する
    void SetInteractRange(const float newRange)
    {
        interactRange = newRange;
    }

    // インタラクト可能な範囲のオフセットを取得する
    DirectX::XMFLOAT3 GetInteractOffset() const
    {
        return interactOffset;
    }

    // インタラクト可能な範囲のオフセットを設定する
    void SetInteractOffset(const DirectX::XMFLOAT3& newOffset)
    {
        interactOffset = newOffset;
    }

    // インタラクト可能な角度(度)を設定する
    void SetInteractDegree(const float degree)
    {
        interactDegree = degree;
    }

    // インタラクト可能な角度(ラジアン)を取得する
    float GetInteractRadian() const
    {
        return DirectX::XMConvertToRadians(interactDegree);
    }

    void DrawImGuiDetails() override
    {
#ifdef USE_IMGUI
        ImGui::DragFloat(U8("インタラクトが反応する範囲"), &interactRange, 0.1f, 0.0f, 5.0f);
        ImGui::DragFloat(U8("インタラクトが反応する角度"), &interactDegree, 1.0f, 0.0f, 180.0f);
        ImGui::DragFloat3(U8("インタラクト範囲のオフセット"), &interactOffset.x, 0.1f);
#endif
    }

protected:
    float interactRange = 2.0f;
    float interactDegree = 0.0f;
    DirectX::XMFLOAT3 interactOffset = { 0.0f, 0.0f, 0.0f }; // インタラクト可能な範囲のオフセット
};