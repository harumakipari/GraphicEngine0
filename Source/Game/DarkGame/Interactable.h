#pragma once

class Player;


class IInteractable
{
public:
    virtual ~IInteractable() = default;

    virtual void Interact() = 0;

    // インタラクトできるか
    bool CanInteract() const { return canInteract; }

    // インタラクトできるかを設定する
    void SetCanInteract(const bool interact) { canInteract = interact; }

    virtual std::string GetInteractText() const
    {
        return "Interact";
    }
protected:
    // インタラクトできるか
    bool canInteract = false;
    // インタラクトされたかどうか
    bool interacted = false;
};
