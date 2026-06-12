#include "pch.h"
#include "SceneRenderer.h"

#include "Engine/Scene/Scene.h"
#include "Game/Actors/Stage/Cloth.h"
#include "Components/Elastic/ElasticComponent.h"

UINT SizeofComponent(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8_UINT: return 1;
    case DXGI_FORMAT_R16_UINT: return 2;
    case DXGI_FORMAT_R32_UINT: return 4;
    case DXGI_FORMAT_R32G32_FLOAT: return 8;
    case DXGI_FORMAT_R32G32B32_FLOAT: return 12;
    case DXGI_FORMAT_R8G8B8A8_UINT: return 3;
    case DXGI_FORMAT_R16G16B16A16_UINT: return 8;
    case DXGI_FORMAT_R32G32B32A32_UINT: return 16;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
    }
    _ASSERT_EXPR(FALSE, L"Not supported");
    return 0;
}

void SceneRenderer::RenderOpaque(ID3D11DeviceContext* immediateContext, const std::vector<MeshComponent*>& items) const
{
    std::vector<MeshComponent*> sorted = items;

    std::sort(sorted.begin(), sorted.end(),
        [](auto* a, auto* b)
        {
            return a->GetPriority() < b->GetPriority();
        });

    for (auto* meshComponent : sorted)
    {
        auto worldMat = meshComponent->GetComponentWorldTransform().ToWorldTransform();
        auto previousWorldMat = meshComponent->GetPreviousComponentWorldTransform().ToWorldTransform();

        meshComponent->UpdateConstantBuffer(immediateContext);
        meshComponent->UpdatePlusAlphaConstants(immediateContext);

        // --- ConvexCollisionComponent があって、MeshComponent が一致する場合のみノードを置き換える ---
        std::vector<InterleavedGltfModel::Node> animatedNodes = meshComponent->GetNodes();
        if (auto* convex = meshComponent->GetOwner()->GetComponent<ConvexCollisionComponent>())
        {
            if (convex->GetMeshComponent() == meshComponent) // ←これで MeshComponent が対象か確認
            {
                animatedNodes = convex->GetAnimatedNodes();
                // animationNodesがworld空間のためworldMatはモデル座標系にあった単位行列に変更すること
                worldMat =
                {
                    -1,0,0,0,
                     0,1,0,0,
                     0,0,1,0,
                     0,0,0,1,
                };
            }
        }

        if (meshComponent->model->mode == ModelTypes::ModelMode::SkeletalMesh)
        {
            Draw(immediateContext,
                meshComponent,
                worldMat, previousWorldMat,
                animatedNodes,
                InterleavedGltfModel::RenderPass::Opaque);
        }
        else
        {
            DrawWithStaticBatching(immediateContext,
                meshComponent,
                worldMat, previousWorldMat,
                animatedNodes,
                InterleavedGltfModel::RenderPass::Opaque);
        }
    }
}

void SceneRenderer::RenderMask(ID3D11DeviceContext* immediateContext, const std::vector<MeshComponent*>& items) const
{
    std::vector<MeshComponent*> sorted = items;

    std::sort(sorted.begin(), sorted.end(),
        [](auto* a, auto* b)
        {
            return a->GetPriority() < b->GetPriority();
        });

    for (auto* meshComponent : sorted)
    {
        const auto& worldMat = meshComponent->GetComponentWorldTransform().ToWorldTransform();
        auto previousWorldMat = meshComponent->GetPreviousComponentWorldTransform().ToWorldTransform();


        meshComponent->UpdateConstantBuffer(immediateContext);
        meshComponent->UpdatePlusAlphaConstants(immediateContext);

        if (meshComponent->model->mode ==
            ModelTypes::ModelMode::SkeletalMesh)
        {
            Draw(immediateContext,
                meshComponent,
                worldMat, previousWorldMat,
                meshComponent->GetNodes(),
                InterleavedGltfModel::RenderPass::Mask);
        }
        else
        {
            DrawWithStaticBatching(immediateContext,
                meshComponent,
                worldMat, previousWorldMat,
                meshComponent->GetNodes(),
                InterleavedGltfModel::RenderPass::Mask);
        }
    }
}

void SceneRenderer::RenderBlend(ID3D11DeviceContext* immediateContext, const std::vector<MeshComponent*>& items) const
{
    std::vector<MeshComponent*> sorted = items;

    std::sort(sorted.begin(), sorted.end(),
        [](auto* a, auto* b)
        {
            return a->GetPriority() < b->GetPriority();
        });

    for (auto* meshComponent : sorted)
    {
        const auto& worldMat = meshComponent->GetComponentWorldTransform().ToWorldTransform();
        auto previousWorldMat = meshComponent->GetPreviousComponentWorldTransform().ToWorldTransform();

        meshComponent->UpdateConstantBuffer(immediateContext);
        meshComponent->UpdatePlusAlphaConstants(immediateContext);

        if (meshComponent->model->mode ==
            ModelTypes::ModelMode::SkeletalMesh)
        {
            Draw(immediateContext,
                meshComponent,
                worldMat, previousWorldMat,
                meshComponent->GetNodes(),
                InterleavedGltfModel::RenderPass::Blend);
        }
        else
        {
            DrawWithStaticBatching(immediateContext,
                meshComponent,
                worldMat, previousWorldMat,
                meshComponent->GetNodes(),
                InterleavedGltfModel::RenderPass::Blend);
        }
    }
}

void SceneRenderer::RenderInstanced(ID3D11DeviceContext* immediateContext, const std::vector<InstanceBatch>& batches)
{
    instanceData.clear();
    for (const auto& batch : batches)
    {
        for (auto* instance : batch.instances)
        {
            InterleavedGltfModel::InstanceData data{};
            data.world = instance->GetComponentWorldTransform().ToWorldTransform();
            data.color = instance->plusAlphaCBuffer->data.cpuColor;
            data.emissiveColor = { instance->plusAlphaCBuffer->data.emissionPower,0.0f,0.0f,0.0f };
            instanceData.push_back(data);
        }

        HRESULT hr{ S_OK };
        D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
        hr = immediateContext->Map(batch.model->instanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
            0, &mapped_subresource);
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        memcpy_s(mapped_subresource.pData, sizeof(InterleavedGltfModel::InstanceData) * 1000, instanceData.data(), sizeof(InterleavedGltfModel::InstanceData) * instanceData.size());
        batch.model->instanceCount_ = static_cast<int>(instanceData.size());
        immediateContext->Unmap(batch.model->instanceBuffer.Get(), 0);
        batch.model->InstancedStaticBatchRender(immediateContext, InterleavedGltfModel::RenderPass::All, pipeLineState);
    }
}

void SceneRenderer::CastShadowRender(ID3D11DeviceContext* immediateContext, const std::vector<MeshComponent*>& items)
{
    for (auto* meshComponent : items)
    {
        const auto& worldMat =
            meshComponent->GetComponentWorldTransform()
            .ToWorldTransform();

        meshComponent->UpdateConstantBuffer(immediateContext);

        //meshComponent->CastShadow(immediateContext, worldMat);

#if 1
        if (meshComponent->model->mode ==
            ModelTypes::ModelMode::SkeletalMesh)
        {
            CastShadow(immediateContext,
                meshComponent,
                worldMat,
                meshComponent->GetNodes(),
                InterleavedGltfModel::RenderPass::All);
        }
        else
        {
            CastShadowWithStaticBatching(
                immediateContext,
                meshComponent,
                worldMat,
                meshComponent->GetNodes());
        }

#endif // 0
    }


}


void SceneRenderer::Draw(ID3D11DeviceContext* immediateContext, const MeshComponent* meshComponent, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& previousWorld, const std::vector<InterleavedGltfModel::Node>& animatedNodes, InterleavedGltfModel::RenderPass pass) const
{
    // 各 MeshComponent の model を取り出す
    const InterleavedGltfModel* model = meshComponent->model.get();
    immediateContext->PSSetShaderResources(0, 1, model->materialResourceView.GetAddressOf());
    std::function<void(int)> traverse = [&](int nodeIndex)->void
        {
            if (nodeIndex < 0 || nodeIndex >= static_cast<int>(animatedNodes.size()))
            {
                assert(false && "nodeIndex out of animatedNodes range");
                return;
            }
            const InterleavedGltfModel::Node& node = animatedNodes.at(nodeIndex);
            if (node.skin > -1)
            {
                const InterleavedGltfModel::Skin& skin = model->skins.at(node.skin);
                _ASSERT_EXPR(skin.joints.size() <= PRIMITIVE_MAX_JOINTS, L"The size of the joint array is insufficient, please expand it.");
                for (size_t jointIndex = 0; jointIndex < skin.joints.size(); ++jointIndex)
                {
                    DirectX::XMStoreFloat4x4(&primitiveJointCBuffer->data.matrices[jointIndex],
                        DirectX::XMLoadFloat4x4(&skin.inverseBindMatrices.at(jointIndex)) *
                        DirectX::XMLoadFloat4x4(&animatedNodes.at(skin.joints.at(jointIndex)).globalTransform) *
                        DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&node.globalTransform))
                    );
                }
                // 2番に定数バッファを送る
                primitiveJointCBuffer->Activate(immediateContext, 2);
            }
            if (node.mesh > -1)
            {
                const InterleavedGltfModel::Mesh& mesh = model->meshes.at(node.mesh);
                for (const InterleavedGltfModel::Mesh::Primitive& primitive : mesh.primitives)
                {
                    // INTERLEAVED_GLTF_MODEL
                    UINT stride = sizeof(InterleavedGltfModel::Mesh::Vertex);
                    UINT offset = 0;

                    {
                        immediateContext->IASetVertexBuffers(0, 1, model->buffers.at(primitive.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);
                    }

                    primitiveCBuffer->data.material = primitive.material;
                    primitiveCBuffer->data.hasTangent = primitive.has("TANGENT");
                    primitiveCBuffer->data.skin = node.skin;

                    XMMATRIX W = XMLoadFloat4x4(&node.globalTransform) * XMLoadFloat4x4(&world);
                    XMMATRIX PrevW = XMLoadFloat4x4(&node.globalTransform) * XMLoadFloat4x4(&previousWorld);

                    DirectX::XMStoreFloat4x4(&primitiveCBuffer->data.world, W);
                    DirectX::XMStoreFloat4x4(&primitiveCBuffer->data.previousWorld, PrevW);
                    DirectX::XMStoreFloat4x4(&primitiveCBuffer->data.inverseTransposeWorld, XMMatrixTranspose(XMMatrixInverse(nullptr, W)));



                    const InterleavedGltfModel::Material& material = model->materials.at(primitive.material);
                    // マテリアルの種類をシェーダーに伝える
                    primitiveCBuffer->data.materialType = static_cast<int>(material.materialType);
                    // 0番に定数バッファを送る
                    primitiveCBuffer->Activate(immediateContext, 0);

                    std::string pipelineName;
                    if (material.overridePipelineName.has_value())
                    {
                        pipelineName = *material.overridePipelineName;
                    }
                    else if (meshComponent->overrideDeferredPipelineName.has_value())
                    {
                        if (pass == InterleavedGltfModel::RenderPass::Blend)
                        {
                            if (meshComponent->overrideForwardPipelineName.has_value())
                            {
                                pipelineName = *meshComponent->overrideForwardPipelineName;
                            }
                            else
                            {
                                pipelineName = *meshComponent->overrideDeferredPipelineName;
                            }
                        }
                        else
                        {
                            pipelineName = *meshComponent->overrideDeferredPipelineName;
                        }
                    }
                    else
                    {
                        pipelineName = GetPipelineName(currentRenderPath, static_cast<MaterialAlphaMode>(material.data.alphaMode), static_cast<ModelMode>(model->mode));
                    }
                    pipeLineStateSet->BindPipeLineState(immediateContext, pipelineName);

                    bool passed = false;
                    switch (pass)
                    {
                    case InterleavedGltfModel::RenderPass::Opaque:
                        if (material.data.alphaMode == 0/*OPAQUE*/)
                        {
                            RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE);
                            passed = true;
                        }
                        break;
                    case InterleavedGltfModel::RenderPass::Mask:
                        if (material.data.alphaMode == 1/*MASK*/)
                        {
                            RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE);
                            passed = true;
                        }
                        break;
                    case InterleavedGltfModel::RenderPass::Blend:
                        if (material.data.alphaMode == 2/*BLEND*/)
                        {
                            RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA);
                            passed = true;
                        }
                        break;
                    case InterleavedGltfModel::RenderPass::All:
                        passed = true;
                        break;
                    }
                    if (!passed)
                    {
                        continue;
                    }

                    const int textureIndices[] =
                    {
                        material.data.pbrMetallicRoughness.basecolorTexture.index,
                        material.data.pbrMetallicRoughness.metallicRoughnessTexture.index,
                        material.data.normalTexture.index,
                        material.data.emissiveTexture.index,
                        material.data.occlusionTexture.index,
                    };

                    ID3D11ShaderResourceView* nullShaderResourceView = {};
                    std::vector<ID3D11ShaderResourceView*> shaderResourceViews(_countof(textureIndices));
                    for (int textureIndex = 0; textureIndex < shaderResourceViews.size(); ++textureIndex)
                    {
                        shaderResourceViews.at(textureIndex) = textureIndices[textureIndex] > -1 ? model->textureResourceViews.at(model->textures.at(textureIndices[textureIndex]).source).Get() : nullShaderResourceView;
                    }
                    immediateContext->PSSetShaderResources(1, static_cast<UINT>(shaderResourceViews.size()), shaderResourceViews.data());


                    if (primitive.indexBufferView.buffer > -1)
                    {
                        // INTERLEAVED_GLTF_MODEL
                        immediateContext->IASetIndexBuffer(model->buffers.at(primitive.indexBufferView.buffer).Get(), primitive.indexBufferView.format, 0);
                        immediateContext->DrawIndexed(primitive.indexBufferView.sizeInBytes / SizeofComponent(primitive.indexBufferView.format), 0, 0);
                    }
                    else
                    {
                        // INTERLEAVED_GLTF_MODEL
                        immediateContext->Draw(primitive.vertexBufferView.sizeInBytes / primitive.vertexBufferView.strideInBytes, 0);
                    }

                }
            }
            for (std::vector<int>::value_type childIndex : node.children)
            {
                traverse(childIndex);
            }
        };
    for (std::vector<int>::value_type nodeIndex : model->scenes.at(model->defaultScene).nodes)
    {
        traverse(nodeIndex);
    }

}

void SceneRenderer::DrawWithStaticBatching(ID3D11DeviceContext* immediateContext, const MeshComponent* meshComponent, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& previousWorld, const std::vector<InterleavedGltfModel::Node>& animatedNodes, InterleavedGltfModel::RenderPass pass) const
{
#if 1
    _ASSERT_EXPR(meshComponent->model->mode == ModelTypes::ModelMode::StaticMesh, L"This function only works with static_batching data.");
    // 各 MeshComponent の model を取り出す
    const InterleavedGltfModel* model = meshComponent->model.get();
    immediateContext->PSSetShaderResources(0, 1, model->materialResourceView.GetAddressOf());

    for (const InterleavedGltfModel::BatchMesh& batchMesh : model->batchMeshes)
    {
        if (batchMesh.vertexBufferView.buffer < 0)
            continue;

        UINT stride = sizeof(InterleavedGltfModel::BatchMesh::Vertex);
        UINT offset = 0;
        immediateContext->IASetVertexBuffers(0, 1, model->buffers.at(batchMesh.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);

        //PrimitiveConstants primitiveData = {};
        primitiveCBuffer->data.material = batchMesh.material;
        primitiveCBuffer->data.hasTangent = batchMesh.has("TANGENT");
        primitiveCBuffer->data.skin = -1;
        const DirectX::XMFLOAT4X4 coordinateSystemTransforms[]
        {
            {//RHS Y-UP
                -1,0,0,0,
                 0,1,0,0,
                 0,0,1,0,
                 0,0,0,1,
            },
            {//LHS Y-UP
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1,
            },
            {//RHS Z-UP
                -1,0, 0,0,
                 0,0,-1,0,
                 0,1, 0,0,
                 0,0, 0,1,
            },
            {//LHS Z-UP
                1,0,0,0,
                0,0,1,0,
                0,1,0,0,
                0,0,0,1,
            },
        };

        float scaleFactor;

        if (model->isModelInMeters)
        {
            scaleFactor = 1.0f;//メートル単位の時
        }
        else
        {
            scaleFactor = 0.01f;//㎝単位の時
        }

        DirectX::XMMATRIX C{ DirectX::XMLoadFloat4x4(&coordinateSystemTransforms[static_cast<int>(model->GetCoordinateSystem())]) * DirectX::XMMatrixScaling(scaleFactor,scaleFactor,scaleFactor) };

        XMMATRIX W = C * XMLoadFloat4x4(&world);
        XMMATRIX PrevW = XMLoadFloat4x4(&previousWorld);

        DirectX::XMStoreFloat4x4(&primitiveCBuffer->data.world, W);
        DirectX::XMStoreFloat4x4(&primitiveCBuffer->data.previousWorld, PrevW);
        DirectX::XMStoreFloat4x4(&primitiveCBuffer->data.inverseTransposeWorld, XMMatrixTranspose(XMMatrixInverse(nullptr, W)));

        const InterleavedGltfModel::Material& material = model->materials.at(batchMesh.material);

        // マテリアルの種類をシェーダーに伝える
        primitiveCBuffer->data.materialType = static_cast<int>(material.materialType);
        // 0番に定数バッファを送る
        primitiveCBuffer->Activate(immediateContext, 0);


        std::string pipelineName;
        if (material.overridePipelineName.has_value())
        {
            pipelineName = *material.overridePipelineName;
        }
        else if (meshComponent->overrideDeferredPipelineName.has_value())
        {
            if (pass == InterleavedGltfModel::RenderPass::Blend)
            {
                if (meshComponent->overrideForwardPipelineName.has_value())
                {
                    pipelineName = *meshComponent->overrideForwardPipelineName;
                }
                else
                {
                    pipelineName = *meshComponent->overrideDeferredPipelineName;
                }
            }
            else
            {
                pipelineName = *meshComponent->overrideDeferredPipelineName;
            }
        }
        else
        {
            pipelineName = GetPipelineName(currentRenderPath, static_cast<MaterialAlphaMode>(material.data.alphaMode), static_cast<ModelMode>(model->mode));
        }
        pipeLineStateSet->BindPipeLineState(immediateContext, pipelineName);


        bool passed = false;
        switch (pass)
        {
        case InterleavedGltfModel::RenderPass::Opaque:
            if (material.data.alphaMode == 0/*OPAQUE*/)
            {
                RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE);
                passed = true;
            }
            break;
        case InterleavedGltfModel::RenderPass::Mask:
            if (material.data.alphaMode == 1/*MASK*/)
            {
                RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE);
                passed = true;
            }
            break;
        case InterleavedGltfModel::RenderPass::Blend:
            if (material.data.alphaMode == 2/*BLEND*/)
            {
                //RenderState::BindDepthStencilState(immediateContext, DEPTH_STATE::ZT_ON_ZW_OFF);
                RenderState::BindBlendState(immediateContext, BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA);
                passed = true;
            }
            break;
        case InterleavedGltfModel::RenderPass::All:
            passed = true;
            break;
        }
        if (!passed)
        {
            continue;
        }

        const int textureIndices[]
        {
            material.data.pbrMetallicRoughness.basecolorTexture.index,
            material.data.pbrMetallicRoughness.metallicRoughnessTexture.index,
            material.data.normalTexture.index,
            material.data.emissiveTexture.index,
            material.data.occlusionTexture.index,
        };
        //RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_NONE);
        //RenderState::BindRasterizerState(immediateContext, RASTERIZE_STATE::SOLID_CULL_BACK);

        ID3D11ShaderResourceView* nullShaderResourceView{};
        std::vector<ID3D11ShaderResourceView*> shaderResourceViews(_countof(textureIndices));
        for (int textureIndex = 0; textureIndex < shaderResourceViews.size(); ++textureIndex)
        {
            shaderResourceViews.at(textureIndex) = textureIndices[textureIndex] > -1 ? model->textureResourceViews.at(model->textures.at(textureIndices[textureIndex]).source).Get() : nullShaderResourceView;
        }
        immediateContext->PSSetShaderResources(1, static_cast<UINT>(shaderResourceViews.size()), shaderResourceViews.data());

        if (batchMesh.indexBufferView.buffer > -1)
        {
            immediateContext->IASetIndexBuffer(model->buffers.at(batchMesh.indexBufferView.buffer).Get(), batchMesh.indexBufferView.format, 0);
            immediateContext->DrawIndexed(batchMesh.indexBufferView.sizeInBytes / SizeofComponent(batchMesh.indexBufferView.format), 0, 0);
        }
        else
        {
            immediateContext->Draw(batchMesh.vertexBufferView.sizeInBytes / batchMesh.vertexBufferView.strideInBytes, 0);
        }
    }

#endif // 0
}

void SceneRenderer::CastShadow(ID3D11DeviceContext* immediateContext, const MeshComponent* meshComponent, const DirectX::XMFLOAT4X4& world, const std::vector<InterleavedGltfModel::Node>& animatedNodes, InterleavedGltfModel::RenderPass pass)
{
    const InterleavedGltfModel* model = meshComponent->model.get();
    _ASSERT_EXPR(model != nullptr, L"meshComponent->model is null!");
    const std::vector<InterleavedGltfModel::Node>& nodes{ animatedNodes.size() > 0 ? animatedNodes : meshComponent->GetNodes() };
    immediateContext->PSSetShaderResources(0, 1, model->materialResourceView.GetAddressOf());
    //CASCADED_SHADOW_MAPS

#if 0
    immediateContext->VSSetShader(model->vertexShaderCSM.Get(), nullptr, 0);
    immediateContext->GSSetShader(model->geometryShaderCSM.Get(), nullptr, 0);

    Microsoft::WRL::ComPtr <ID3D11PixelShader> nullPixelShader{ NULL };
    immediateContext->PSSetShader(nullPixelShader.Get()/*SHADOW*/, nullptr, 0);

    immediateContext->IASetInputLayout(model->inputLayout.Get());
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

#endif // 0

    std::function<void(int)> traverse = [&](int nodeIndex)->void {
        const InterleavedGltfModel::Node& node = nodes.at(nodeIndex);
        if (node.skin > -1)
        {
            const InterleavedGltfModel::Skin& skin = model->skins.at(node.skin);
            _ASSERT_EXPR(skin.joints.size() <= PRIMITIVE_MAX_JOINTS, L"The size of the joint array is insufficient, please expand it.");
            for (size_t jointIndex = 0; jointIndex < skin.joints.size(); ++jointIndex)
            {
                DirectX::XMStoreFloat4x4(&primitiveJointCBuffer->data.matrices[jointIndex],
                    DirectX::XMLoadFloat4x4(&skin.inverseBindMatrices.at(jointIndex)) *
                    //DirectX::XMLoadFloat4x4(&animatedNodes.at(skin.joints.at(jointIndex)).globalTransform) *
                    DirectX::XMLoadFloat4x4(&nodes.at(skin.joints.at(jointIndex)).globalTransform) *
                    DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&node.globalTransform))
                );
            }
            // 2番に定数バッファを送る
            primitiveJointCBuffer->Activate(immediateContext, 2);
        }
        if (node.mesh > -1)
        {
            const InterleavedGltfModel::Mesh& mesh = model->meshes.at(node.mesh);
            for (const InterleavedGltfModel::Mesh::Primitive& primitive : mesh.primitives)
            {
                UINT stride = sizeof(InterleavedGltfModel::Mesh::Vertex);
                UINT offset = 0;
                immediateContext->IASetVertexBuffers(0, 1, model->buffers.at(primitive.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);

                primitiveCBuffer->data.material = primitive.material;
                primitiveCBuffer->data.hasTangent = primitive.has("TANGENT");
                primitiveCBuffer->data.skin = node.skin;

                //座標系の変換を行う
                const DirectX::XMFLOAT4X4 coordinateSystemTransforms[]
                {
                    {//RHS Y-UP
                        -1,0,0,0,
                         0,1,0,0,
                         0,0,1,0,
                         0,0,0,1,
                    },
                    {//LHS Y-UP
                        1,0,0,0,
                        0,1,0,0,
                        0,0,1,0,
                        0,0,0,1,
                    },
                    {//RHS Z-UP
                        -1,0, 0,0,
                         0,0,-1,0,
                         0,1, 0,0,
                         0,0, 0,1,
                    },
                    {//LHS Z-UP
                        1,0,0,0,
                        0,0,1,0,
                        0,1,0,0,
                        0,0,0,1,
                    },
                };

                float scaleFactor;

                if (model->isModelInMeters)
                {
                    scaleFactor = 1.0f;//メートル単位の時
                }
                else
                {
                    scaleFactor = 0.01f;//㎝単位の時
                }
                DirectX::XMMATRIX C{ DirectX::XMLoadFloat4x4(&coordinateSystemTransforms[static_cast<int>(model->GetCoordinateSystem())]) * DirectX::XMMatrixScaling(scaleFactor,scaleFactor,scaleFactor) };
                DirectX::XMStoreFloat4x4(&primitiveCBuffer->data.world, DirectX::XMLoadFloat4x4(&node.globalTransform) * C * DirectX::XMLoadFloat4x4(&world));
                DirectX::XMStoreFloat4x4(&primitiveCBuffer->data.inverseTransposeWorld, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&node.globalTransform) * DirectX::XMLoadFloat4x4(&world))));

                const InterleavedGltfModel::Material& material = model->materials.at(primitive.material);
                const int textureIndices[] =
                {
                    material.data.pbrMetallicRoughness.basecolorTexture.index,
                    material.data.pbrMetallicRoughness.metallicRoughnessTexture.index,
                    material.data.normalTexture.index,
                    material.data.emissiveTexture.index,
                    material.data.occlusionTexture.index,
                };

                // マテリアルの種類をシェーダーに伝える
                primitiveCBuffer->data.materialType = static_cast<int>(material.materialType);
                // 0番に定数バッファを送る
                primitiveCBuffer->Activate(immediateContext, 0);


#if 1 //CASCADED_SHADOW_MAPS
                std::string pipelineName;
                if (meshComponent->overrideCascadeShadowPipelineName.has_value())
                {
                    pipelineName = *meshComponent->overrideCascadeShadowPipelineName;
                }
                else
                {
                    pipelineName = GetPipelineName(currentRenderPath, static_cast<MaterialAlphaMode>(material.data.alphaMode), static_cast<ModelMode>(model->mode));
                }
                pipeLineStateSet->BindPipeLineState(immediateContext, pipelineName);
                // ピクセルシェーダを確実に解除
                immediateContext->PSSetShader(nullptr, nullptr, 0);
#endif // 0
                ID3D11ShaderResourceView* nullShaderResourceView = {};
                std::vector<ID3D11ShaderResourceView*> shaderResourceViews(_countof(textureIndices));
                for (int textureIndex = 0; textureIndex < shaderResourceViews.size(); ++textureIndex)
                {
                    shaderResourceViews.at(textureIndex) = textureIndices[textureIndex] > -1 ? model->textureResourceViews.at(model->textures.at(textureIndices[textureIndex]).source).Get() : nullShaderResourceView;
                }
                immediateContext->PSSetShaderResources(1, static_cast<UINT>(shaderResourceViews.size()), shaderResourceViews.data());

                if (primitive.indexBufferView.buffer > -1)
                {
                    // INTERLEAVED_GLTF_MODEL
                    immediateContext->IASetIndexBuffer(model->buffers.at(primitive.indexBufferView.buffer).Get(), primitive.indexBufferView.format, 0);
                    immediateContext->DrawIndexedInstanced(primitive.indexBufferView.sizeInBytes / SizeofComponent(primitive.indexBufferView.format), 4, 0, 0, 0);
                }
                else
                {
                    // INTERLEAVED_GLTF_MODEL
                    //immediateContext->Draw(primitive.vertexBufferView.sizeInBytes / primitive.vertexBufferView.strideInBytes, 0);
                    immediateContext->DrawIndexedInstanced(primitive.vertexBufferView.sizeInBytes / primitive.vertexBufferView.strideInBytes, 4, 0, 0, 0);
                }
            }
        }
        for (std::vector<int>::value_type childIndex : node.children)
        {
            traverse(childIndex);
        }
        };
    for (std::vector<int>::value_type nodeIndex : model->scenes.at(model->defaultScene).nodes)
    {
        traverse(nodeIndex);
    }

    immediateContext->VSSetShader(nullptr, nullptr, 0);
    immediateContext->GSSetShader(nullptr, nullptr, 0);
    immediateContext->PSSetShader(nullptr, nullptr, 0);
}

void SceneRenderer::CastShadowWithStaticBatching(ID3D11DeviceContext* immediateContext, const MeshComponent* meshComponent, const DirectX::XMFLOAT4X4& world, const std::vector<InterleavedGltfModel::Node>& animatedNodes) const
{
    const InterleavedGltfModel* model = meshComponent->model.get();
    _ASSERT_EXPR(model->mode == ModelTypes::ModelMode::StaticMesh, L"This function only works with static_batching data.");

    immediateContext->PSSetShaderResources(0, 1, model->materialResourceView.GetAddressOf());

    immediateContext->VSSetShader(model->vertexShaderCSM.Get(), nullptr, 0);
    immediateContext->GSSetShader(model->geometryShaderCSM.Get(), nullptr, 0);

    Microsoft::WRL::ComPtr <ID3D11PixelShader> nullPixelShader{ nullptr };
    immediateContext->PSSetShader(nullPixelShader.Get()/*SHADOW*/, nullptr, 0);

    //immediate_context->PSSetShader(pixelShader.Get(), nullptr, 0);
    immediateContext->IASetInputLayout(model->inputLayout.Get());
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (const InterleavedGltfModel::BatchMesh& batchMesh : model->batchMeshes)
    {
        if (batchMesh.vertexBufferView.buffer < 0)
            continue;

        UINT stride = sizeof(InterleavedGltfModel::BatchMesh::Vertex);
        UINT offset = 0;
        immediateContext->IASetVertexBuffers(0, 1, model->buffers.at(batchMesh.vertexBufferView.buffer).GetAddressOf(), &stride, &offset);

        primitiveCBuffer->data.material = batchMesh.material;
        primitiveCBuffer->data.hasTangent = batchMesh.has("TANGENT");
        primitiveCBuffer->data.skin = -1;
        const DirectX::XMFLOAT4X4 coordinateSystemTransforms[]
        {
            {//RHS Y-UP
                -1,0,0,0,
                 0,1,0,0,
                 0,0,1,0,
                 0,0,0,1,
            },
            {//LHS Y-UP
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1,
            },
            {//RHS Z-UP
                -1,0, 0,0,
                 0,0,-1,0,
                 0,1, 0,0,
                 0,0, 0,1,
            },
            {//LHS Z-UP
                1,0,0,0,
                0,0,1,0,
                0,1,0,0,
                0,0,0,1,
            },
        };

        float scaleFactor;

        if (model->isModelInMeters)
        {
            scaleFactor = 1.0f;//メートル単位の時
        }
        else
        {
            scaleFactor = 0.01f;//㎝単位の時
        }

        DirectX::XMMATRIX C{ DirectX::XMLoadFloat4x4(&coordinateSystemTransforms[static_cast<int>(model->GetCoordinateSystem())]) * DirectX::XMMatrixScaling(scaleFactor,scaleFactor,scaleFactor) };

        DirectX::XMStoreFloat4x4(&primitiveCBuffer->data.world, C * DirectX::XMLoadFloat4x4(&world));
        DirectX::XMStoreFloat4x4(&primitiveCBuffer->data.inverseTransposeWorld, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&world))));


        const InterleavedGltfModel::Material& material = model->materials.at(batchMesh.material);
        const int textureIndices[]
        {
            material.data.pbrMetallicRoughness.basecolorTexture.index,
            material.data.pbrMetallicRoughness.metallicRoughnessTexture.index,
            material.data.normalTexture.index,
            material.data.emissiveTexture.index,
            material.data.occlusionTexture.index,
        };

        // マテリアルの種類をシェーダーに伝える
        primitiveCBuffer->data.materialType = static_cast<int>(material.materialType);
        // 0番に定数バッファを送る
        primitiveCBuffer->Activate(immediateContext, 0);


#if 1 //CASCADED_SHADOW_MAPS
        std::string pipelineName;
        if (meshComponent->overrideCascadeShadowPipelineName.has_value())
        {
            pipelineName = *meshComponent->overrideCascadeShadowPipelineName;
        }
        else
        {
            pipelineName = GetPipelineName(currentRenderPath, static_cast<MaterialAlphaMode>(material.data.alphaMode), static_cast<ModelMode>(model->mode));
        }
        pipeLineStateSet->BindPipeLineState(immediateContext, pipelineName);
        immediateContext->PSSetShader(nullptr, nullptr, 0);
#endif // 0


        ID3D11ShaderResourceView* nullShaderResourceView{};
        std::vector<ID3D11ShaderResourceView*> shaderResourceViews(_countof(textureIndices));
        for (int textureIndex = 0; textureIndex < shaderResourceViews.size(); ++textureIndex)
        {
            shaderResourceViews.at(textureIndex) = textureIndices[textureIndex] > -1 ? model->textureResourceViews.at(model->textures.at(textureIndices[textureIndex]).source).Get() : nullShaderResourceView;
        }
        immediateContext->PSSetShaderResources(1, static_cast<UINT>(shaderResourceViews.size()), shaderResourceViews.data());

        if (batchMesh.indexBufferView.buffer > -1)
        {
            immediateContext->IASetIndexBuffer(model->buffers.at(batchMesh.indexBufferView.buffer).Get(), batchMesh.indexBufferView.format, 0);
            immediateContext->DrawIndexedInstanced(batchMesh.indexBufferView.sizeInBytes / SizeofComponent(batchMesh.indexBufferView.format), 4, 0, 0, 0);
        }
        else
        {
            immediateContext->DrawIndexedInstanced(batchMesh.vertexBufferView.sizeInBytes / batchMesh.vertexBufferView.strideInBytes, 4, 0, 0, 0);
        }
    }
    immediateContext->VSSetShader(nullptr, nullptr, 0);
    immediateContext->GSSetShader(nullptr, nullptr, 0);
    immediateContext->PSSetShader(nullptr, nullptr, 0);
}

// メッシュを収集して振り分ける
RenderQueues SceneRenderer::BuildRenderQueues()
{
    RenderQueues queues;

    Scene* scene = Scene::GetCurrentScene();
    if (!scene) return queues;

    auto& actors = scene->GetActorManager()->GetAllActors();

    for (auto actor : actors)
    {
        if (!actor || !actor->IsActive() || !actor->GetRootComponent())
            continue;

        // MeshComponentを収集
        std::vector<MeshComponent*> meshes;
        actor->GetComponents<MeshComponent>(meshes);

        for (auto* mesh : meshes)
        {
            if (!mesh)
                continue;

            if (auto* instanceMesh = dynamic_cast<InstanceMeshComponent*>(mesh))
            {
                if (instanceMesh->IsVisible())
                {
                    // モデル単位でまとめるためのキー
                    auto model = instanceMesh->model;

                    bool found = false;
                    for (auto& batch : queues.instanceBatches)
                    {
                        if (batch.model == model.get())
                        {
                            batch.instances.push_back(instanceMesh);
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {// 同一のモデルがなかったら、
                        InstanceBatch batch;
                        batch.model = model.get();
                        batch.instances.push_back(instanceMesh);
                        queues.instanceBatches.push_back(std::move(batch));
                    }
                }
                continue; // 通常描画には流さない
            }

            const bool visible = mesh->IsVisible();
            const bool castShadow = mesh->IsCastShadow() && visible;

            // ===== 通常描画 =====
            if (visible)
            {
                queues.meshes.push_back(mesh);
            }

            // ===== 影パス =====
            if (castShadow)
            {
                queues.shadowCasters.push_back(mesh);
            }
        }


    }

    return queues;
}
