#pragma once
#include <d3d11.h>
#include <unordered_map>
#include <memory>
#include <string>
#include "Graphics/Renderer/SceneRenderer.h"
#include "Core/Actor.h"
class CollisionSystem;


class World
{
public:
    World() = default;
    virtual ~World() {};

    // 更新処理
    void Tick(float deltaTime);

    // 描画処理
    void Render(ID3D11DeviceContext* immediateContext);

    // 影の描画処理
    void CastShadowRender(ID3D11DeviceContext* immediateContext);

    // ImGuiを描画する
    void DrawGUI();
    
    void Clear();

    // アクターを名前付きで作成・登録する（同名アクターが存在する場合は警告する）
    template<class T>
    std::shared_ptr<T> SpawnActor(const std::string& actorName)
    {
        auto findByName = [&actorName](const std::shared_ptr<Actor>& actor)
            {
                return actor->GetName() == actorName;
            };
        // 名前が一致するアクターを探す
        std::vector<std::shared_ptr<Actor>>::iterator it = std::find_if(allActors_.begin(), allActors_.end(), findByName);

        // 同名のアクターがすでに存在していたら警告
        _ASSERT_EXPR(it == allActors_.end(), L"この actor の名前は既に使用されています。");
        std::shared_ptr<T> newActor = std::make_shared<T>(actorName);
        allActors_.push_back(newActor);

        newActor->Initialize();
        return newActor;
    }


    // 名前からアクターを取得（キャッシュ付き検索）
    std::shared_ptr<Actor> FindActorByName(const std::string& actorName)
    {
        // キャッシュにあればそれを返す
        auto cached = actorCacheByName_.find(actorName);
        if (cached != actorCacheByName_.end())
        {
            return cached->second;
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

private:
    // アクター名からアクターへのポインタを高速に取得するためのキャッシュ。
    // 名前が見つからない場合はアクターリストを検索し、結果をこのキャッシュに保存する。
    std::unordered_map<std::string, std::shared_ptr<Actor>> actorCacheByName_;
    // 現在存在しているすべてのアクター
    std::vector<std::shared_ptr<Actor>> allActors_;
    // レンダラー
    //SceneRenderer renderer;
};