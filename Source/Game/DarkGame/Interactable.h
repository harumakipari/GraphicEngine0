#pragma once

class Player;


class IInteractable
{
public:
    virtual ~IInteractable() = default;

    virtual void Interact() = 0;

    // ƒIƒvƒVƒ‡ƒ“
    virtual bool CanInteract() const { return true; }

    virtual std::string GetInteractText() const
    {
        return "Interact";
    }
};
