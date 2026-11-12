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

	// まずは ownedComponents_ の中から、まだ covered に入っていないコンポーネントを探す
    for (const auto& comp : ownedComponents)
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

    ownedComponents.clear();
    componentsByName.clear();
    rootComponent = nullptr;

    isAlive = false;
}


std::shared_ptr<Component> Actor::FindComponentByName(const std::string& name)
{
    if (name.empty())
    {
        return rootComponent;
    }

    decltype(componentsByName)::const_iterator nameToComponent = componentsByName.find(name);
    if (nameToComponent != componentsByName.end())
    {
        return nameToComponent->second;
    }

    decltype(ownedComponents)::const_iterator component = std::find(ownedComponents.begin(), ownedComponents.end(), name);
    if (component != ownedComponents.end())
    {
        componentsByName.emplace(name, *component);
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

    if (rootComponent && rootComponent->GetName() == name)
    {
        // rootComponentの削除は禁止
        Logger::Warning(Logger::LogCategory::System, u8"rootComponentは削除できません。");
        _ASSERT(L"rootComponentは削除できません。");
        return;
    }

    // キャッシュからも探す
    auto itNameSceneComp = componentsByName.find(name);
    if (itNameSceneComp != componentsByName.end())
    {
        componentsByName.erase(itNameSceneComp);
    }

    auto it = std::remove_if(ownedComponents.begin(), ownedComponents.end(),
        [&](const std::shared_ptr<Component>& comp)
        {
            if (comp->GetName() == name) {
                comp->Destroy(); // 上で定義した Destroy 呼ぶ
                return true;     // erase 対象にする
            }
            return false;
        });

    ownedComponents.erase(it, ownedComponents.end());
    componentsByName.erase(name);
}

void Actor::DrawImGuiInspector()
{
#ifdef USE_IMGUI

    if (ImGui::TreeNodeEx(actorName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        // isValid チェックボックスを追加
        ImGui::Checkbox("isActive", &isActive);

        if (rootComponent)
        {
            ImGui::Text("RootComponent: %s", typeid(*rootComponent).name());
            rootComponent->DrawImGuiInspector();
        }

        for (auto& comp : ownedComponents)
        {
            if (comp != rootComponent) {
                ImGui::Text("%s", typeid(*comp).name());
                comp->DrawImGuiInspector();
            }
        }

        ImGui::DragFloat(U8("ActorのTimeScale"), &timeScale, 0.05f);

        DrawImGuiDetails();

        ImGui::TreePop();
    }
#endif
}
