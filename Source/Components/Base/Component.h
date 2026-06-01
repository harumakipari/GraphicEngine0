#ifndef COMPONENT_H
#define COMPONENT_H
// 1. 自分自身のヘッダが一番上に来る

// 2. C 標準ライブラリ（あれば）

// C++ 標準ライブラリ
#include <memory>
#include <string>

// 他ライブラリ


#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#endif

// プロジェクトの他のヘッダ
#include "Math/MathHelper.h"
#include "Physics/Physics.h"

class Actor;


// 物理ボディを移動（テレポート）させる方法を指定
enum class TeleportType :int
{
    None, //テレポートなし。通常の移動（速度と衝突計算あり）
    TeleportPhysics, // 瞬間移動するが速度は維持。衝突判定なし
    ResetPhysics, // テレポート後に物理状態を完全リセット
};

// Transform を更新する際のフラグ
enum class UpdateTransformFlags :int
{
    None = 0x0,  // 通常通り更新
    SkipPhysicsUpdate = 0x1,    // 物理エンジンには通知しない
    PropagateFromParent = 0x2,  // 親から伝播してきた更新だと示す
    OnlyUpdateIfUsingSocket = 0x4,  // Socket（ソケット）を使っている子コンポーネントだけを更新対象にする
};
// UpdateTransformFlagsをビット演算できるようにするオーバーロードの定義
inline UpdateTransformFlags operator|(UpdateTransformFlags lhs, UpdateTransformFlags rhs)
{
    // underlying_type_t -> enum class を整数として扱う時のキャスト
    using T = std::underlying_type_t<UpdateTransformFlags>;
    return static_cast<UpdateTransformFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
}
inline UpdateTransformFlags operator&(UpdateTransformFlags lhs, UpdateTransformFlags rhs)
{
    // underlying_type_t -> enum class を整数として扱う時のキャスト
    using T = std::underlying_type_t<UpdateTransformFlags>;
    return static_cast<UpdateTransformFlags>(static_cast<T>(lhs) & static_cast<T>(rhs));
}
inline UpdateTransformFlags operator^(UpdateTransformFlags lhs, UpdateTransformFlags rhs)
{
    // underlying_type_t -> enum class を整数として扱う時のキャスト
    using T = std::underlying_type_t<UpdateTransformFlags>;
    return static_cast<UpdateTransformFlags>(static_cast<T>(lhs) ^ static_cast<T>(rhs));
}
inline UpdateTransformFlags operator~(UpdateTransformFlags flag)
{
    // underlying_type_t -> enum class を整数として扱う時のキャスト
    using T = std::underlying_type_t<UpdateTransformFlags>;
    return static_cast<UpdateTransformFlags>(~static_cast<T>(flag));
}


//	コンポーネントのベースクラス
class Component
{
public:
    Component(const std::string& name, const std::shared_ptr<Actor>& owner) :name_(name), owner_(owner), active_(true) {};
    virtual ~Component() = default;

    Component(const Component& rhs) = delete;
    Component& operator=(const Component& rhs) = delete;
    Component(Component&&) noexcept = delete;
    Component& operator=(Component&&) noexcept = delete;

    virtual void Initialize() {};

    virtual void Tick(float deltaTime) {}

    virtual void OnRegister() {} // 派生クラスで override して登録処理を書く
    virtual void OnUnregister() {} // 派生クラスで override して解除処理を書く

    virtual void DrawImGuiInspector() {}

    const std::string& GetName() const
    {
        return name_;
    }

    Actor* GetOwner() { return owner_.lock().get(); }

    std::shared_ptr<Actor> GetActor() { return owner_.lock(); }

    void SetActive(bool active) { this->active_ = active; }

    bool GetActive() { return active_; }

    virtual void UpdateComponentToWorld(UpdateTransformFlags update_transform_flags = UpdateTransformFlags::None, TeleportType teleport = TeleportType::None) {}

    virtual void Destroy()
    {
        OnUnregister();
        SetActive(false);
    }
protected:
    //Actor* owner_ = nullptr;
    std::weak_ptr<Actor> owner_;
    std::string name_;
    bool active_;
    bool initialized_ = false; // 複数回の初期化するのを防ぐのに使う。。
};




#endif //COMPONENT_H