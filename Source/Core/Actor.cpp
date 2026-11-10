#include "pch.h"
#include "Actor.h"

#include <unordered_set>

void Actor::DestroyActor()
{
    // ヒットコールバックをクリア
    RemoveAllHitCallBacks();

    // 全コンポーネントの破棄
    std::vector<std::shared_ptr<Component>> destroyTargets;
    std::unordered_set<const Component*> covered;
    std::vector<std::shared_ptr<SceneComponent>> work;

    // Select every direct target before Destroy() changes attachment relationships.
    for (const auto& comp : ownedComponents_)
    {
        if (!comp || !covered.insert(comp.get()).second)
        {
            continue;
        }

        destroyTargets.push_back(comp);
        if (auto sceneComponent = std::dynamic_pointer_cast<SceneComponent>(comp))
        {
            work.push_back(sceneComponent);
            while (!work.empty())
            {
                auto current = work.back();
                work.pop_back();
                for (const auto& child : current->GetAttachChildren())
                {
                    if (child && covered.insert(child.get()).second)
                    {
                        work.push_back(child);
                    }
                }
            }
        }
    }

    for (const auto& comp : destroyTargets)
    {
        comp->Destroy();
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

