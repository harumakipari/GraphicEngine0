#include "pch.h"
#include "ActorManager.h"
#include "Engine/Scene/Scene.h"



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

    // allActors_ のコピーを作る（弱参照ならshared_ptrもコピーされる）
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

    // isValid == false のアクターだけを削除
    std::erase_if(allActors_,
        [](const std::shared_ptr<Actor>& a) { return !a || !a->IsAlive(); });
}

void ActorManager::DrawImGuiAllActors() const
{
#ifdef USE_IMGUI
    // 画面サイズを取得
    ImGuiIO& io = ImGui::GetIO();
    float windowWidth = io.DisplaySize.x * 0.25f;
    float windowHeight = io.DisplaySize.y;

    // 次のウィンドウの位置とサイズを指定
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - windowWidth, 0));
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));

    // フラグをつけて固定表示に（サイズ変更などを禁止したい場合）
    ImGui::Begin("Actor Inspector", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse
    );

    for (const auto& actor : allActors_)
    {
        actor->DrawImGuiInspector();
    }

    ImGui::End();
#endif
}
