#pragma once
#include <map>
#include <memory>
#include <cassert>
#include "Actor.h"

#include "Graphics/Renderer/ShapeRenderer.h"
#include "Graphics/Core/ConstantBuffer.h"

#include "Components/CollisionShape/ShapeComponent.h"
#include "Components/Render/MeshComponent.h"

#include "Engine/Camera/CameraConstants.h"

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
#if 0
    template <class T>
    std::shared_ptr<T> CreateAndRegisterActor(const std::string& actorName, bool autoInitialize = true)
    {
#if 0 // 同名の時に警告する
        auto findByName = [&actorName](const std::shared_ptr<Actor>& actor)
            {
                return actor->GetName() == actorName;
            };
        // 名前が一致するアクターを探す
        std::vector<std::shared_ptr<Actor>>::iterator it = std::find_if(allActors_.begin(), allActors_.end(), findByName);

        // 同名のアクターがすでに存在していたら警告
        _ASSERT_EXPR(it == allActors_.end(), L"An actor with this name has already been registered.");
        std::shared_ptr<T> newActor = std::make_shared<T>(actorName);

#else // 同名の時にユニークな名前をつける
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
        std::shared_ptr<T> newActor = std::make_shared<T>(finalName);
        // Sceneを渡す
        newActor->SetOwnerScene(ownerScene_);
        allActors_.push_back(newActor);
#endif
        if (autoInitialize)
        {
            newActor->Initialize();
            newActor->PostInitialize();
        }
        return newActor;
    }
#endif // 0



    // アクターを名前付きで作成・登録する（同名アクターが存在する場合は"_1","_2"とつけてユニークな名前にする） 二つ目の引数は初期化をautoでするかどうかを決定する
    template <class T, class... Args>
    std::shared_ptr<T> CreateAndRegisterActorWithTransform(const std::string& actorName, const Transform& transform = { DirectX::XMFLOAT3 {0,0,0},DirectX::XMFLOAT4{1,1,1,0},DirectX::XMFLOAT3{1,1,1} },Args&&... args)
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
    std::shared_ptr<Actor> GetActorByName(const std::string& actorName)
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
    void ClearAll()
    {
        for (const std::shared_ptr<Actor>& actor : allActors_)
        {
            //for (std::shared_ptr<SceneComponent>& component : actor->ownedSceneComponents_)
            //{
            //    component->OnUnregister();
            //}
            //for (std::shared_ptr<Component>& component : actor->ownedLogicComponents_)
            //{
            //    component->OnUnregister();
            //}
            //actor->Finalize();
            actor->DestroyActor();
        }
        allActors_.clear();
        actorCacheByName_.clear();
    }

    // Actorのポインタを一括で生ポインタ形式で取得する（描画やシーン用などに）
    void ConvineActor(std::vector<Actor*>& outActorPointers) const
    {
        outActorPointers.clear();
        for (const std::shared_ptr<Actor>& actor : allActors_)
        {
            outActorPointers.push_back(actor.get());
        }
    }

    // 全アクターのUpdate処理を呼び出す（RootComponentとOwnedComponent）
    void Update(float deltaTime)
    {
        // allActors_ のコピーを作る（弱参照ならshared_ptrもコピーされる）
        auto updateActors = allActors_;

        for (auto it = updateActors.begin(); it != updateActors.end(); ++it)
        {
            auto& actor = *it;
            if (!actor /*|| !actor->isActive*/) continue;

            //char buf[256];
            //sprintf_s(buf, "Update Loop: actor=%s, isValid=%d, isActive=%d\n", actor->GetName().c_str(), actor->isValid, actor->isActive);
            //OutputDebugStringA(buf);

            //if (!actor->isValid)
            if (actor->IsPendingKill())
            {
                //char buf[256];
                //sprintf_s(buf, "actor=%s, isValid=%d, isActive=%d\n → Destroy() を呼ぶ！\n", actor->GetName().c_str(), actor->isValid, actor->isActive);
                //OutputDebugStringA(buf);

                //OutputDebugStringA(" → Destroy() を呼ぶ！\n");
                actor->DestroyActor();
                continue;
            }

            for (auto& component : actor->GetComponents())
            {
                component->Tick(deltaTime);
            }

            //for (auto& component : actor->ownedLogicComponents_)
            //{
            //    component->Tick(deltaTime);
            //}

            if (actor->GetRootComponent())
            {
                actor->GetRootComponent()->UpdateComponentToWorld();
            }
            actor->Update(deltaTime);

            //if (!actor->isValid)
            //{
            //    actor->Destroy();
            //    continue;
            //}

            actor->PostDestroyComponents();
        }

        // isValid == false のアクターだけを削除
        allActors_.erase(
            std::remove_if(allActors_.begin(), allActors_.end(),
                [](const std::shared_ptr<Actor>& a) { return !a || !a->IsAlive(); }),
            allActors_.end());
    }

    void DrawImGuiAllActors() const
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
};

class Renderer
{
private:
    std::unique_ptr<ConstantBuffer<ViewConstants>> viewBuffer;

public:
    Renderer()
    {
        ID3D11Device* device = Graphics::GetDevice();
        itemModel = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/Items/PickUpEnergyCore/pick_up_item.gltf", ModelTypes::ModelMode::InstancedStaticMesh);

        viewBuffer = std::make_unique<ConstantBuffer<ViewConstants>>(device);
    }

    virtual ~Renderer() = default;

    // View関連の定数バッファを更新する
    void UpdateViewConstants(ID3D11DeviceContext* immediateContext, const ViewConstants& data) const
    {
        viewBuffer->data = data;
        viewBuffer->Activate(immediateContext, 4);
    }

    void RenderParticle(ID3D11DeviceContext* immediateContext);

    void RenderOpaque(ID3D11DeviceContext* immediateContext);

    void RenderMask(ID3D11DeviceContext* immediateContext);

    void RenderBlend(ID3D11DeviceContext* immediateContext);

    void RenderInstanced(ID3D11DeviceContext* immediateContext);

    std::vector<DirectX::XMFLOAT4X4> instanceDatas;

private:
    std::shared_ptr<InterleavedGltfModel> itemModel;
    PipeLineStateDesc pipeLineState_ = {};
public:
    void CastShadowRender(ID3D11DeviceContext* immediateContext);
};




