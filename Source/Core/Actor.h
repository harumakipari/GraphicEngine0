#pragma once

// C++ 標準ライブラリ
#include <string>
#include <DirectXMath.h>
#include <memory>
#include<assert.h>

#include "Components/Base/Component.h"
#include "Components/Base/SceneComponent.h"
#include "Components/CollisionShape/ShapeComponent.h"
#include "Components/Transform/Transform.h"
#include "Engine/Debug/Assert.h"
#include "Math/MathHelper.h"

class Scene;


/**
 * @class Actor
 * @brief Scene 上に存在するゲームオブジェクト
 *
 * Actor は Component を所有し、
 * Transform・ライフサイクル・イベントを管理する。
 */
class Actor :public std::enable_shared_from_this <Actor>
{
public:
    //==================================================
    // Type Definitions
    //==================================================
    using HitCallBack = std::function<void(std::pair<CollisionComponent*, CollisionComponent*>)>;

    //==================================================
    // Constructor / Destructor
    //==================================================
    Actor() = default;
    Actor(const std::string& actorName) :actorName(actorName) {}
    virtual ~Actor() = default;

    Actor(const Actor&) = delete;
    Actor& operator=(const Actor&) = delete;
    Actor(Actor&&) noexcept = delete;
    Actor& operator=(Actor&&) noexcept = delete;


    //==================================================
    // Lifecycle
    //==================================================
    virtual void Initialize(const Transform& transform) {}

    virtual void Update(float deltaTime) {}

    virtual void Finalize() {}

    /**
    * @brief Actor を完全に破棄する
    *
    * 全 Component を Destroy し、Scene からも削除される。
    * 通常は MarkPendingKill() 経由で呼ばれる。
    */
    void DestroyActor();

    /**
    * @brief Actor を削除予約状態にする
    *
    * この関数を呼ぶと、次の Update フレームで
    * DestroyActor() が実行される。
    */
    void MarkPendingKill() { isPendingKill = true; }

    //==================================================
    // Component Management
    //==================================================
    template <class T>
    std::shared_ptr<T> AddComponent(const std::string& name, const std::string& parentName = "")
    {
        // 自分自身が shared_ptr で管理されている前提で、それを渡す
        std::shared_ptr<Actor> sharedThis = shared_from_this(); // Actorは std::enable_shared_from_this 継承が必要
        // Debugチェック1: 自分自身の確認
        _ASSERT_EXPR(sharedThis != nullptr, "shared_from_this() returned nullptr!");


#if 1
        //------------------------------------------------------
        // ① ユニーク名生成
        //------------------------------------------------------
        std::string finalName = name;
        int suffix = 1;

        auto nameExists = [&](const std::string& checkName)
            {
                return std::any_of(
                    ownedComponents_.begin(),
                    ownedComponents_.end(),
                    [&](const std::shared_ptr<Component>& comp)
                    {
                        return comp->GetName() == checkName;
                    });
            };

        while (nameExists(finalName))
        {
            finalName = name + "_" + std::to_string(suffix++);
        }

#endif // 0

        std::shared_ptr<T> newComponent = std::make_shared<T>(finalName, sharedThis);
        _ASSERT_EXPR(newComponent != nullptr, "Failed to create new SceneComponent!");
        //std::shared_ptr<T> newComponent = std::make_shared<T>(name, this);

        if constexpr ((std::is_base_of<SceneComponent, T>::value))
        {
            std::shared_ptr<SceneComponent> sceneComponent = std::dynamic_pointer_cast<SceneComponent>(newComponent);
            _ASSERT_EXPR(sceneComponent != nullptr, "Dynamic cast to SceneComponent failed!");
            if (parentName.empty())
            {
                if (rootComponent_)
                {
                    sceneComponent->AttachTo(rootComponent_);
                }
                else
                {
                    SetRootComponent(sceneComponent);
                }
            }
            else
            {
                std::shared_ptr<SceneComponent> parent = std::dynamic_pointer_cast<SceneComponent>(FindComponentByName(parentName));
                sceneComponent->AttachTo(parent);
            }
        }

        _ASSERT_EXPR(reinterpret_cast<void*>(&ownedComponents_) != nullptr, "ownedSceneComponents_ is nullptr!");

        //_ASSERT_EXPR(newComponent.use_count() >= 2, "newComponent use_count is invalid!"); 

        //OutputDebugStringA(("Before push_back size: " + std::to_string(ownedComponents_.size()) + "\n").c_str());
        //OutputDebugStringA(("Before push_back capacity: " + std::to_string(ownedComponents_.capacity()) + "\n").c_str());

        //ownedSceneComponents_.push_back(newComponent);
        ownedComponents_.push_back(std::static_pointer_cast<Component>(newComponent));


        // push_back後も同様に確認
        //OutputDebugStringA(("After push_back size: " + std::to_string(ownedComponents_.size()) + "\n").c_str());
        //OutputDebugStringA(("After push_back capacity: " + std::to_string(ownedComponents_.capacity()) + "\n").c_str());

        // 初期化する
        //newComponent->Initialize();
        newComponent->OnRegister();

        return newComponent;
    }


    /**
    * @brief 名前から Component を取得する
    * @return 見つからない場合 nullptr
    */
    std::shared_ptr<Component> FindComponentByName(const std::string& name);

    // 名前からcomponentを削除する
    void DestroyComponentByName(const std::string& name);

    template<typename T>
    T* GetComponent()
    {
        // SceneComponent から探す
        //for (const std::shared_ptr<SceneComponent>& compent : ownedSceneComponents_)
        for (const std::shared_ptr<Component>& compent : ownedComponents_)
        {
            T* casted = dynamic_cast<T*>(compent.get());
            if (casted != nullptr)
            {
                return casted;
            }
        }

        //Logger::Warning(Logger::LogCategory::System,u8"Actor の GetComponent が nullptr を返しています。");
        _ASSERT(L"Actor の GetComponent が nullptr を返しています。");
        return nullptr;
    }


    template<class T>       //引数にvectorを入れると、コンポーネントを
    void GetComponents(std::vector<T*>& components)
    {
        components.clear();
        for (auto component : ownedComponents_)
        {
            if (T* downCastComponent = dynamic_cast<T*>(component.get()))
            {
                components.push_back(downCastComponent);
            }
        }

    }

    /**
    * @brief Actor が所有する全 Component を取得する（読み取り専用）
    */
    const std::vector<std::shared_ptr<Component>>& GetComponents() const
    {
        return ownedComponents_;
    }

    /**
    * @brief Component を削除予約する
    *
    * PhysX simulate 中でも安全に削除できる。
    */
    void RequestDestroyComponent(const std::string& name)
    {
        pendingDestroyComponentNames_.push_back(name);
    }

    // physx の後でやりたい処理
    void PostDestroyComponents()
    {
        for (const auto& name : pendingDestroyComponentNames_)
        {
            DestroyComponentByName(name);
        }
        pendingDestroyComponentNames_.clear();
    }

    //==================================================
    // Transform
    //==================================================
    void SetRootComponent(const std::shared_ptr<SceneComponent>& root)
    {
        rootComponent_ = root;
    }

    std::shared_ptr<SceneComponent> GetRootComponent()
    {
        return rootComponent_;
    }


    DirectX::XMFLOAT4X4 GetWorldTransform() const
    {
        DirectX::XMFLOAT4X4 worldTransform;
        DirectX::XMMATRIX M = rootComponent_ ? rootComponent_->GetFinalWorldTransform().ToMatrix() : DirectX::XMMatrixIdentity();
        DirectX::XMStoreFloat4x4(&worldTransform, M);
        return worldTransform;
    }

    DirectX::XMFLOAT3 GetPosition() const
    {
        if (rootComponent_)
        {
            return rootComponent_->GetRelativeLocation();
        }
        return { 0.0f,0.0f,0.0f };
    }

    void SetPosition(const DirectX::XMFLOAT3& position) const
    {
        this->rootComponent_->SetRelativeLocationDirect(position);
        UpdateAllComponentTransforms();
    }

    const DirectX::XMFLOAT4& GetQuaternionRotation() const { return rootComponent_->GetRelativeRotation(); }

    void SetQuaternionRotation(const DirectX::XMFLOAT4& rotation) const
    {
        // 明示的に検査を追加
        _ASSERT_EXPR(MathHelper::IsValidQuaternion(rotation), L"SetQuaternionRotation: Invalid quaternion");
        this->rootComponent_->SetRelativeRotationDirect(rotation);
    }

    // degrees で返す
    DirectX::XMFLOAT3 GetEulerRotation() const
    {
        return rootComponent_->GetRelativeEulerRotation();
    }

    void SetEulerRotation(const DirectX::XMFLOAT3& eulerRotation) const
    {
        this->rootComponent_->SetRelativeEulerRotationDirect(eulerRotation);
    }

    DirectX::XMFLOAT3 GetScale() const { return rootComponent_->GetRelativeScale(); }

    void SetScale(const DirectX::XMFLOAT3& scale) const
    {
        this->rootComponent_->SetRelativeScaleDirect(scale);
    }

    //進行方向の単位ベクトルを取得する
    DirectX::XMFLOAT3 GetForward() const
    {
        DirectX::XMFLOAT3  front;
        // Z軸方向の単位方向ベクトル　デフォルト
        DirectX::XMVECTOR DefaultForward = DirectX::XMVectorSet(0, 0, 1, 0);
        //playerの回転値によって作られる回転行列
        DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&GetQuaternionRotation()));
        //DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(GetQuaternionRotation().x, GetQuaternionRotation().y, GetQuaternionRotation().z);
        // デフォルトのベクトルに回転行列を適応する
        DirectX::XMVECTOR TransformedForward = DirectX::XMVector3TransformNormal(DefaultForward, RotationMatrix);
        //正規化
        TransformedForward = DirectX::XMVector3Normalize(TransformedForward);

        DirectX::XMStoreFloat3(&front, TransformedForward);
        return front;
    }


    //==================================================
    // State
    //==================================================
    void SetActive(bool isActive)
    {
        this->isActive = isActive;
    }

    bool IsActive() const
    {
        return isActive;
    }

    bool IsAlive() const
    {
        return this->isAlive;
    }

    bool IsPendingKill() const
    {
        return this->isPendingKill;
    }

    //==================================================
    // Collision / Event
    //==================================================

    //　collisionComponent　が Dynamic の物と当たった時に通る
    virtual void NotifyHit(/*std::shared_ptr<Component> selfComp, std::shared_ptr<Component> otherComp,*//* std::shared_ptr<Actor> otherActor*/Actor* otherActor, const DirectX::XMFLOAT3& hitPos, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT3& impulse) {}
    //　collisionComponent　が Dynamic の物と当たった時に通る
    virtual void NotifyHit(CollisionComponent* selfComp, CollisionComponent* otherComp, Actor* otherActor, const DirectX::XMFLOAT3& hitPos, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT3& impulse) {}


    /**
    * @brief 衝突イベントを全コールバックへ通知する
    *
    * @param hitPair first が自身、second が相手の CollisionComponent
    */
    void DispatchHitEvent(std::pair<CollisionComponent*, CollisionComponent*> hitPair) const
    {
        for (auto& callback : hitCallbacks_)
        {
            callback(hitPair);
        }
    }

    /**
    * @brief キネマティック同士の衝突時に呼ばれる仮想関数
    *
    * 派生クラスでオーバーライドして使用する。
    */
    virtual void OnHit(std::pair<CollisionComponent*, CollisionComponent*> hitPair) {}

    void AddHitCallback(const HitCallBack& callback)
    {
        hitCallbacks_.push_back(callback);
    }

    void RemoveAllHitCallBacks()
    {
        hitCallbacks_.clear();
    }

    //==================================================
    // Debug
    //==================================================
    void DrawImGuiInspector()
    {
#ifdef USE_IMGUI

        if (ImGui::TreeNodeEx(actorName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            // isValid チェックボックスを追加
            ImGui::Checkbox("isValid", &isActive);

            if (rootComponent_)
            {
                ImGui::Text("RootComponent: %s", typeid(*rootComponent_).name());
                rootComponent_->DrawImGuiInspector();
            }

            for (auto& comp : ownedComponents_)
            {
                if (comp != rootComponent_) {
                    ImGui::Text("%s", typeid(*comp).name());
                    comp->DrawImGuiInspector();
                }
            }


            DrawImGuiDetails();

            ImGui::TreePop();
        }
#endif
    }

    // 継承押したサブクラスの専用GUI
    virtual void DrawImGuiDetails() {};

    //==================================================
    // Scene
    //==================================================
    void SetOwnerScene(Scene* scene) { ownerScene_ = scene; }
    Scene* GetOwnerScene() const { return ownerScene_; }



    // 

    void MakeRootComponent()
    {
        rootComponent_ = AddComponent<SceneComponent>("RootComponent");
    }


    std::string& GetName() { return actorName; }


    // Initialize の後に呼ばれるべき処理 Transformを更新する　キャッシュしているため一フレーム後のTransformをが呼ばれる可能性を防ぐため
    void UpdateAllComponentTransforms() const
    {
        if (rootComponent_)
        {
            rootComponent_->UpdateComponentToWorld();
        }

        for (const auto& comp : ownedComponents_)
        {
            comp->UpdateComponentToWorld();
        }
    }

    // タグを追加する
    void AddTag(const std::string& tag)
    {
        tags_.push_back(tag);
    }
    // タグを持っているか
    bool HasTag(const std::string& tag) const
    {
        return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
    }

protected:
    Scene* ownerScene_ = nullptr;   // 自分が属しているScene

private:
    //==================================================
    // Internal Data
    //==================================================
    std::string actorName;

    bool isActive = true;

    // アクターが存在しているかどうか
    bool isAlive = true;

    // アクターの削除予約
    bool isPendingKill = false;

    std::vector<HitCallBack> hitCallbacks_;

    // rootComponent (Transform) 系
    std::shared_ptr<SceneComponent> rootComponent_;
    // Component
    std::vector<std::shared_ptr<Component>> ownedComponents_;

    // 名前とコンポーネントをキャッシュしておく
    std::unordered_map<std::string, std::shared_ptr<Component>> componentsByName_;

    // 削除予約用リスト　
    // physx の simulate 途中に pxShape が付いた shapeComponent を削除するのを防ぐため
    std::vector<std::string> pendingDestroyComponentNames_;

protected:
    std::vector<std::string> tags_;
};

static inline bool operator==(const std::shared_ptr<Actor>& actor, const std::string& name)
{
    return actor->GetName() == name;
}

static inline bool operator==(const std::shared_ptr<SceneComponent>& component, const std::string& name)
{
    return component->GetName() == name;
}

static inline bool operator==(const std::shared_ptr<Component>& component, const std::string& name)
{
    return component->GetName() == name;
}
