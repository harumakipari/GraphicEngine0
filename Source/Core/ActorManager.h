#pragma once
#include <map>
#include <memory>
#include <cassert>
#include <tracy/Tracy.hpp>
#include "Actor.h"

#include "Components/CollisionShape/ShapeComponent.h"
#include "Components/Render/MeshComponent.h"


class Scene;

class ActorManager
{
protected:
    Scene* ownerScene_ = nullptr;
public:
    void SetOwnerScene(Scene* scene) { ownerScene_ = scene; }
    Scene* GetOwnerScene() const { return ownerScene_; }
    // アクター名からアクターへのポインタを高速に取得するためのキャッシュ。
    // 名前が見つからない場合はアクターリストを検索し、結果をこのキャッシュに保存する。
    std::unordered_map<std::string, std::weak_ptr<Actor>> actorCacheByName_;
    // 現在存在しているすべてのアクター
    std::vector<std::shared_ptr<Actor>> allActors_;

    // アクターを名前付きで作成・登録する（同名アクターが存在する場合は"_1","_2"とつけてユニークな名前にする） 二つ目の引数は初期化をautoでするかどうかを決定する
    template <class T, class... Args>
    std::shared_ptr<T> CreateAndRegisterActorWithTransform(const std::string& actorName, const Transform& transform = { DirectX::XMFLOAT3 {0,0,0},DirectX::XMFLOAT4{1,1,1,0},DirectX::XMFLOAT3{1,1,1} }, Args&&... args)
    {
        // 同名の時にユニークな名前をつける
        // 同名があれば "_1", "_2", ... をつけてユニークな名前にする
        std::string finalName = actorName;
        int suffix = 1;
        auto nameExists = [&](const std::string& name)
            {
                return std::any_of(allActors_.begin(), allActors_.end(), [&](const std::shared_ptr<Actor>& actor)
                    {
                        return actor->GetName() == name;
                    });
            };

        while (nameExists(finalName))
        {
            finalName = actorName + "_" + std::to_string(suffix++);
        }

        //std::shared_ptr<T> newActor = std::make_shared<T>(finalName);
        std::shared_ptr<T> newActor = std::make_shared<T>(finalName, std::forward<Args>(args)...);
        // Sceneを渡す
        newActor->SetOwnerScene(ownerScene_);
        allActors_.push_back(newActor);

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
        return allActors_;
    }

    // 名前からアクターを取得（キャッシュ付き検索）
    std::shared_ptr<Actor> GetActorByName(const std::string& actorName);

    // 型からアクターを取得（最初に見つかったものを返す）
    template<class T>
    std::shared_ptr<T> GetActorOfType()
    {
        for (auto& actor : allActors_)
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

        for (auto& actor : allActors_)
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

        for (auto& actor : allActors_)
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

    // 登録済みアクターとキャッシュをすべてクリアする
    void ClearAll();

    // 全アクターのUpdate処理を呼び出す（RootComponentとOwnedComponent）
    void Update(float deltaTime);

    void DrawImGuiAllActors() const;

};





