#include "pch.h"
#include "Actor.h"

void Actor::DestroyActor()
{
    // ヒットコールバックをクリア
    RemoveAllHitCallBacks();

    // 全コンポーネントの破棄
    for (auto& comp : ownedComponents_)
    {
        if (comp)
        {
            comp->Destroy();          // PhysXからの除去など内部的なクリーンアップ
            comp->OnUnregister();     // Sceneなどからの登録解除
        }
    }

    Finalize();

    ownedComponents_.clear();
    componentsByName_.clear();
    rootComponent_ = nullptr;

    isAlive = false;
}


std::shared_ptr<Component> Actor::FindComponentByName(const std::string& name)
{
    if (name.empty())
    {
        return rootComponent_;
    }

    decltype(componentsByName_)::const_iterator nameToComponent = componentsByName_.find(name);
    if (nameToComponent != componentsByName_.end())
    {
        return nameToComponent->second;
    }

    decltype(ownedComponents_)::const_iterator component = std::find(ownedComponents_.begin(), ownedComponents_.end(), name);
    if (component != ownedComponents_.end())
    {
        componentsByName_.emplace(name, *component);
        return *component;
    }


    return nullptr;
}

void Actor::DestroyComponentByName(const std::string& name)
{
    if (name.empty())
    {
        Logger::Warning(Logger::LogCategory::System, u8"この名前のコンポーネントは存在しないため削除できません。");
        _ASSERT(L"この名前のコンポーネントは存在しないため削除できません。");
        return;
    }

    if (rootComponent_ && rootComponent_->GetName() == name)
    {
        // rootComponentの削除は禁止
        Logger::Warning(Logger::LogCategory::System, u8"rootComponentは削除できません。");
        _ASSERT(L"rootComponentは削除できません。");
        return;
    }

    // キャッシュからも探す
    auto itNameSceneComp = componentsByName_.find(name);
    if (itNameSceneComp != componentsByName_.end())
    {
        componentsByName_.erase(itNameSceneComp);
    }

    auto it = std::remove_if(ownedComponents_.begin(), ownedComponents_.end(),
        [&](const std::shared_ptr<Component>& comp)
        {
            if (comp->GetName() == name) {
                comp->Destroy(); // 上で定義した Destroy 呼ぶ
                return true;     // erase 対象にする
            }
            return false;
        });

    ownedComponents_.erase(it, ownedComponents_.end());
    componentsByName_.erase(name);
}

