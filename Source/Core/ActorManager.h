#pragma once
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Actor.h"

class Scene;

class ActorManager
{
public:
    void SetOwnerScene(Scene* scene) { ownerScene = scene; }
    Scene* GetOwnerScene() const { return ownerScene; }

    // 同名時は "_1", "_2", ... を付与して一意な名前で生成・登録する。
    template <class T, class... Args>
    std::shared_ptr<T> CreateAndRegisterActorWithTransform(const std::string& actorName, const Transform& transform = { DirectX::XMFLOAT3 {0,0,0},DirectX::XMFLOAT4{1,1,1,0},DirectX::XMFLOAT3{1,1,1} }, Args&&... args)
    {
        std::string finalName = actorName;
        int suffix = 1;
        auto nameExists = [&](const std::string& name)
            {
                return std::any_of(allActors.begin(), allActors.end(), [&](const std::shared_ptr<Actor>& actor)
                    {
                        return actor->GetName() == name;
                    });
            };

        while (nameExists(finalName))
        {
            finalName = actorName + "_" + std::to_string(suffix++);
        }

        std::shared_ptr<T> newActor = std::make_shared<T>(finalName, std::forward<Args>(args)...);
        newActor->SetOwnerScene(ownerScene);
        allActors.push_back(newActor);

        newActor->MakeRootComponent();
        newActor->SetPosition(transform.GetLocation());
        newActor->SetQuaternionRotation(transform.GetRotation());
        newActor->SetScale(transform.GetScale());
        newActor->Initialize(transform);
        newActor->UpdateAllComponentTransforms();
        return newActor;
    }

    const std::vector<std::shared_ptr<Actor>>& GetAllActors() const
    {
        return allActors;
    }

    // 名前からアクターを取得（キャッシュ付き検索）
    std::shared_ptr<Actor> GetActorByName(const std::string& actorName);

    // 型からアクターを取得（最初に見つかったものを返す）
    template<class T>
    std::shared_ptr<T> GetActorOfType()
    {
        for (auto& actor : allActors)
        {
            if (auto casted = std::dynamic_pointer_cast<T>(actor))
            {
                return casted;
            }
        }
        return nullptr;
    }

    // 型からアクターをすべて取得
    template<class T>
    std::vector<std::shared_ptr<T>> GetActorsOfType()
    {
        std::vector<std::shared_ptr<T>> result;

        for (auto& actor : allActors)
        {
            if (auto casted = std::dynamic_pointer_cast<T>(actor))
            {
                result.push_back(casted);
            }
        }

        return result;
    }

    template<class T>
    std::vector<std::shared_ptr<T>> GetActorsByTag(const std::string& tag)
    {
        std::vector<std::shared_ptr<T>> result;

        for (auto& actor : allActors)
        {
            if (actor->HasTag(tag))
            {
                if (auto casted = std::dynamic_pointer_cast<T>(actor))
                {
                    result.push_back(casted);
                }
            }
        }

        return result;
    }

    // Actorを更新し、破棄済みのActorを管理一覧から除去する。
    void Update(float deltaTime);

    // 全Actorを破棄した後、管理一覧と名前キャッシュを空にする。
    void ClearAll();

protected:
    Scene* ownerScene = nullptr;

private:
    // 名前検索済みの Actor を保持する弱参照キャッシュ。
    std::unordered_map<std::string, std::weak_ptr<Actor>> actorCacheByName;

    std::vector<std::shared_ptr<Actor>> allActors;
};