#include "pch.h"
#include "ActorManager.h"
#include "Engine/Scene/Scene.h"


void Renderer::RenderParticle(ID3D11DeviceContext* immediateContext)
{
    Scene* currentScene = Scene::GetCurrentScene();  // 現在のシーン取得
    if (!currentScene) return;
    auto& allActors = currentScene->GetActorManager()->GetAllActors();
    for (auto& actor : allActors)
    {
        if (!actor->GetRootComponent())
        {
            continue;
        }

        if (!actor->IsActive())
        {// actorが存在していなかったらスキップ
            continue;
        }

    }
}

void Renderer::RenderOpaque(ID3D11DeviceContext* immediateContext)
{
    Scene* currentScene = Scene::GetCurrentScene();  // 現在のシーン取得
    if (!currentScene) return;
    auto& allActors = currentScene->GetActorManager()->GetAllActors();
    for (auto& actor : allActors)
    {
        if (!actor->GetRootComponent() || !actor->IsActive())
        {
            continue;
        }

        std::vector<MeshComponent*> meshComponents;
        actor->GetComponents<MeshComponent>(meshComponents);

        for (const MeshComponent* meshComponent : meshComponents)
        {
            //  各 MeshComponent 自身の最新ワールド行列を取り出す
            const auto& worldMat = meshComponent->GetComponentWorldTransform().ToWorldTransform();

            if (meshComponent->IsVisible())
            {
                //  描画呼び出しも meshComponent ベースの行列を渡す
                meshComponent->Render(immediateContext, worldMat, InterleavedGltfModel::RenderPass::Opaque);
            }
        }
    }
}

void Renderer::RenderMask(ID3D11DeviceContext* immediateContext)
{
    Scene* currentScene = Scene::GetCurrentScene();  // 現在のシーン取得
    if (!currentScene) return;
    auto& allActors = currentScene->GetActorManager()->GetAllActors();
    for (auto& actor : allActors)
    {
        if (!actor->GetRootComponent())
        {
            continue;
        }
        if (!actor->IsActive())
        {// actorが存在していなかったらスキップ
            continue;
        }

        std::vector<MeshComponent*> meshComponents;
        actor->GetComponents<MeshComponent>(meshComponents);

        for (const MeshComponent* meshComponent : meshComponents)
        {
            //  各 MeshComponent 自身の最新ワールド行列を取り出す
            const auto& worldMat = meshComponent->GetComponentWorldTransform().ToWorldTransform();
            bool rendered = false;

            // ConvexCollisionComponent が使用できるならそちらを優先する
            if (auto* convexComponent = actor->GetComponent<ConvexCollisionComponent>())
            {
                if (convexComponent->GetActive())
                {
                    convexComponent = dynamic_cast<ConvexCollisionComponent*>(convexComponent);
                    DirectX::XMFLOAT4X4 world;
                    DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());
                    convexComponent->GetMeshComponent()->model->Render(immediateContext, world, convexComponent->GetAnimatedNodes(), InterleavedGltfModel::RenderPass::Mask);
                    //meshComponent->model->Render(immediateContext, world, convexComponent->GetAnimatedNodes(), InterleavedGltfModel::RenderPass::Mask);
                    rendered = true;
                }
            }

            if (!rendered && meshComponent->IsVisible())
            {
                //  描画呼び出しも meshComponent ベースの行列を渡す
                meshComponent->Render(immediateContext, worldMat, InterleavedGltfModel::RenderPass::Mask);
            }
        }
    }
}

void Renderer::RenderBlend(ID3D11DeviceContext* immediateContext)
{
    Scene* currentScene = Scene::GetCurrentScene();  // 現在のシーン取得
    if (!currentScene) return;
    auto& allActors = currentScene->GetActorManager()->GetAllActors();
    for (auto& actor : allActors)
    {
        if (!actor->GetRootComponent())
        {
            continue;
        }

        if (!actor->IsActive())
        {// actorが存在していなかったらスキップ
            continue;
        }


        std::vector<MeshComponent*> meshComponents;
        actor->GetComponents<MeshComponent>(meshComponents);

        for (const MeshComponent* meshComponent : meshComponents)
        {
            //  各 MeshComponent 自身の最新ワールド行列を取り出す
            const auto& worldMat = meshComponent->GetComponentWorldTransform().ToWorldTransform();
            bool rendered = false;

            // ConvexCollisionComponent が使用できるならそちらを優先する
            if (auto* convexComponent = actor->GetComponent<ConvexCollisionComponent>())
            {
                if (convexComponent->GetActive())
                {
                    convexComponent = dynamic_cast<ConvexCollisionComponent*>(convexComponent);
                    DirectX::XMFLOAT4X4 world;
                    DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());
                    convexComponent->GetMeshComponent()->model->Render(immediateContext, world, convexComponent->GetAnimatedNodes(), InterleavedGltfModel::RenderPass::Blend);
                    //meshComponent->model->Render(immediateContext, world, convexComponent->GetAnimatedNodes(), InterleavedGltfModel::RenderPass::Blend);
                    rendered = true;
                }
            }

            if (!rendered && meshComponent->IsVisible())
            {
                //  描画呼び出しも meshComponent ベースの行列を渡す
                meshComponent->Render(immediateContext, worldMat,InterleavedGltfModel::RenderPass::Blend);
            }
        }
    }
}

void Renderer::RenderInstanced(ID3D11DeviceContext* immediateContext)
{
    instanceDatas.clear();
    Scene* currentScene = Scene::GetCurrentScene();  // 現在のシーン取得
    if (!currentScene) return;
    auto& allActors = currentScene->GetActorManager()->GetAllActors();
    for (auto& actor : allActors)
    {
        if (!actor->GetRootComponent())
        {
            continue;
        }
        if (!actor->IsActive())
        {// actorが存在していなかったらスキップ
            continue;
        }

    }
    HRESULT hr{ S_OK };
    D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
    hr = immediateContext->Map(itemModel->instanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
        0, &mapped_subresource);
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


    //DirectX::XMFLOAT4X4* p = reinterpret_cast<DirectX::XMFLOAT4X4*>(mapped_subresource.pData);

    memcpy_s(mapped_subresource.pData, sizeof(DirectX::XMFLOAT4X4) * 1000, instanceDatas.data(), sizeof(DirectX::XMFLOAT4X4) * instanceDatas.size());
    itemModel->instanceCount_ = static_cast<int>(instanceDatas.size());
    immediateContext->Unmap(itemModel->instanceBuffer.Get(), 0);
    itemModel->InstancedStaticBatchRender(immediateContext, InterleavedGltfModel::RenderPass::All, pipeLineState_);
    //char buf[256];
    //sprintf_s(buf, "instanceSize:%d\n", static_cast<int>(instanceDatas.size()));
    //OutputDebugStringA(buf);
}

void Renderer::CastShadowRender(ID3D11DeviceContext* immediateContext)
{
    Scene* currentScene = Scene::GetCurrentScene();  // 現在のシーン取得
    if (!currentScene) return;
    auto& allActors = currentScene->GetActorManager()->GetAllActors();
    for (auto& actor : allActors)
    {
        if (!actor->GetRootComponent())
        {
            continue;
        }

        if (!actor->IsActive())
        {// actorが存在していなかったらスキップ
            continue;
        }

        std::vector<MeshComponent*> meshComponents;
        actor->GetComponents<MeshComponent>(meshComponents);
        for (const MeshComponent* meshComponent : meshComponents)
        {
            //  各 MeshComponent 自身の最新ワールド行列を取り出す
            const auto& worldMat = meshComponent->GetComponentWorldTransform().ToWorldTransform();
            //bool rendered = false;

            // ConvexCollisionComponent が使用できるならそちらを優先する
            if (auto* convexComponent = actor->GetComponent<ConvexCollisionComponent>())
            {
                if (convexComponent->GetActive())
                {
                    convexComponent = dynamic_cast<ConvexCollisionComponent*>(convexComponent);
                    DirectX::XMFLOAT4X4 world;
                    DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());
                    convexComponent->GetMeshComponent()->model->CastShadow(immediateContext, world, convexComponent->GetAnimatedNodes());

                    //meshComponent->model->CastShadow(immediateContext, world, convexComponent->GetAnimatedNodes());
                    //rendered = true;
                }
            }

            if (/*!rendered &&*/ meshComponent->IsVisible())
            {
                if (meshComponent->IsCastShadow())
                {
                    //  描画呼び出しも meshComponent ベースの行列を渡す
                    meshComponent->CastShadow(immediateContext, worldMat);
                }
            }

        }
    }
}


