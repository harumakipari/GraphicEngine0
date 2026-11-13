#pragma once

#include <memory>
#include <string>
#include <type_traits>

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

#include "Math/MathHelper.h"
#include "Physics/Physics.h"

class Actor;

// Transform を更新する際のフラグ
enum class UpdateTransformFlags :int
{
    None = 0x0,  // 通常通り更新
    SkipPhysicsUpdate = 0x1,    // 物理エンジンには通知しない
    PropagateFromParent = 0x2,  // 親から伝播してきた更新だと示す
    OnlyUpdateIfUsingSocket = 0x4,  // Socket（ソケット）を使っている子コンポーネントだけを更新対象にする
};

inline UpdateTransformFlags operator|(UpdateTransformFlags lhs, UpdateTransformFlags rhs)
{
    using T = std::underlying_type_t<UpdateTransformFlags>;
    return static_cast<UpdateTransformFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
}
inline UpdateTransformFlags operator&(UpdateTransformFlags lhs, UpdateTransformFlags rhs)
{
    using T = std::underlying_type_t<UpdateTransformFlags>;
    return static_cast<UpdateTransformFlags>(static_cast<T>(lhs) & static_cast<T>(rhs));
}
inline UpdateTransformFlags operator^(UpdateTransformFlags lhs, UpdateTransformFlags rhs)
{
    using T = std::underlying_type_t<UpdateTransformFlags>;
    return static_cast<UpdateTransformFlags>(static_cast<T>(lhs) ^ static_cast<T>(rhs));
}
inline UpdateTransformFlags operator~(UpdateTransformFlags flag)
{
    using T = std::underlying_type_t<UpdateTransformFlags>;
    return static_cast<UpdateTransformFlags>(~static_cast<T>(flag));
}


class Component
{
public:
    Component(const std::string& name, const std::shared_ptr<Actor>& owner) : owner(owner), name(name), active(true) {}
    virtual ~Component() = default;

    Component(const Component& rhs) = delete;
    Component& operator=(const Component& rhs) = delete;
    Component(Component&&) noexcept = delete;
    Component& operator=(Component&&) noexcept = delete;

    virtual void Initialize() {};

    virtual void Tick(float deltaTime) {}

    virtual void Destroy()
    {
        OnUnregister();
        SetActive(false);
    }

    virtual void OnRegister() {} 
    virtual void OnUnregister() {}


    const std::string& GetName() const
    {
        return name;
    }

    Actor* GetOwner() const { return owner.lock().get(); }

    std::shared_ptr<Actor> GetActor() const { return owner.lock(); }

    void SetActive(bool active) { this->active = active; }
    bool IsActive() const { return active; }

    virtual void DrawImGuiInspector() {}
    virtual void UpdateComponentToWorld(UpdateTransformFlags updateTransformFlags = UpdateTransformFlags::None) {}

protected:
    std::weak_ptr<Actor> owner;
    std::string name;
    bool active;
};


