#include "pch.h"
#include "ActorManager.h"
#include <tracy/Tracy.hpp>



// 名前からアクターを取得（キャッシュ付き検索）
std::shared_ptr<Actor> ActorManager::GetActorByName(const std::string& actorName)
{
    // キャッシュにあればそれを返す
    auto cached = actorCacheByName_.find(actorName);
    if (cached != actorCacheByName_.end())
    {
        if (!cached->second.expired())
        {
            return cached->second.lock();
        }
    }

    // なければ全アクターから探す
    auto found = std::find_if(allActors_.begin(), allActors_.end(),
        [&actorName](const std::shared_ptr<Actor>& actor) {
            return actor->GetName() == actorName;
        });

    // 見つかった場合はキャッシュして返す
    if (found != allActors_.end()) {
        actorCacheByName_[actorName] = *found;
        return *found;
    }

    // 見つからなかった
    return nullptr;
}

// 登録済みアクターとキャッシュをすべてクリアする
void ActorManager::ClearAll()
{
    for (const std::shared_ptr<Actor>& actor : allActors_)
    {
        actor->DestroyActor();
    }
    allActors_.clear();
    actorCacheByName_.clear();
}

// 全アクターのUpdate処理を呼び出す（RootComponentとOwnedComponent）
void ActorManager::Update(float deltaTime)
{
    ZoneScopedN("Actor Update");

    // 更新中に Actor が追加されても走査を継続できるよう、shared_ptr の一覧をコピーする。
    // コピーが更新対象の寿命を保持し、追加された Actor の更新は次回に回る。
    auto updateActors = allActors_;

    for (auto it = updateActors.begin(); it != updateActors.end(); ++it)
    {
        auto& actor = *it;
        if (!actor /*|| !actor->isActive*/) continue;

        const float timeScale = actor->GetTimeScale();

        float actorDeltaTime = deltaTime * timeScale;

        if (actor->IsPendingKill())
        {
            actor->DestroyActor();
            continue;
        }

        for (auto& component : actor->GetComponents())
        {
            component->Tick(actorDeltaTime);
        }


        if (actor->GetRootComponent())
        {
            actor->GetRootComponent()->UpdateComponentToWorld();
        }

        actor->Update(actorDeltaTime);

        actor->PostDestroyComponents();
    }

    // 破棄済みの Actor と null の要素を所有一覧から除く。
    std::erase_if(allActors_,
        [](const std::shared_ptr<Actor>& a) { return !a || !a->IsAlive(); });
}
