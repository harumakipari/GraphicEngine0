#pragma once

#include <string>
#include <unordered_map>

#include "Graphics/Core/PipelineState.h"
#include "Graphics/Core/Shader.h"
#include "Engine/Utility/Win32Utils.h"

enum RenderPath :uint8_t
{
    Forward,
    Deferred,
    Shadow,
};

enum MaterialAlphaMode :uint8_t
{
    MaterialOpaque,
    MaterialMask,
    MaterialBlend,
};

enum ModelMode :uint8_t
{
    SkeletalComponent,
    StaticComponent,
    InstanceComponent,
};

constexpr const char* const SkeletalMesh_ForwardNames[] =
{
    "forwardOpaqueSkeletalMesh",
    "forwardMaskSkeletalMesh",
    "forwardBlendSkeletalMesh"
};

constexpr const char* const SkeletalMesh_DeferredNames[] =
{
    "deferredOpaqueSkeletalMesh",
    "deferredMaskSkeletalMesh",
    "deferredBlendSkeletalMesh"
};

constexpr const char* const SkeletalMesh_ShadowNames[] =
{
    "CascadeShadowMapSkeletalMesh"
};

constexpr const char* const* const SkeletalMesh_PipelineNames[] =
{
    SkeletalMesh_ForwardNames,
    SkeletalMesh_DeferredNames,
    SkeletalMesh_ShadowNames
};

// RenderPath と materialAlphaMode からパイプライン名を作成する関数
inline std::string GetPipelineName(const RenderPath renderPath, const MaterialAlphaMode alphaMode, const ModelMode modelMode)
{
    switch (modelMode)
    {
    case ModelMode::SkeletalComponent:
        switch (renderPath)
        {
        case RenderPath::Forward:
            switch (alphaMode)
            {
            case MaterialOpaque:
                return "forwardOpaqueSkeletalMesh";
                break;
            case MaterialMask:
                return "forwardMaskSkeletalMesh";
                break;
            case MaterialBlend:
                return "forwardBlendSkeletalMesh";
                break;
            default:
                break;
            }
            break;
        case RenderPath::Deferred:
            switch (alphaMode)
            {
            case MaterialOpaque:
                return "deferredOpaqueSkeletalMesh";
                break;
            case MaterialMask:
                return "deferredMaskSkeletalMesh";
                break;
            case MaterialBlend:
                return "deferredBlendSkeletalMesh";
                break;
            default:
                break;
            }
            break;
        case RenderPath::Shadow:
            return "CascadeShadowMapSkeletalMesh";
            break;
        default:
            break;
        }

        break;
    case ModelMode::StaticComponent:
        switch (renderPath)
        {
        case RenderPath::Forward:
            switch (alphaMode)
            {
            case MaterialOpaque:
                return "forwardOpaqueStaticMesh";
                break;
            case MaterialMask:
                return "forwardMaskStaticMesh";
                break;
            case MaterialBlend:
                return "forwardBlendStaticMesh";
                break;
            default:
                break;
            }
            break;
        case RenderPath::Deferred:
            switch (alphaMode)
            {
            case MaterialOpaque:
                return "deferredOpaqueStaticMesh";
                break;
            case MaterialMask:
                return "deferredMaskStaticMesh";
                break;
            case MaterialBlend:
                return "deferredBlendStaticMesh";
                break;
            default:
                break;
            }
            break;
        case RenderPath::Shadow:
            return "CascadeShadowMapStaticMesh";
            break;
        default:
            break;
        }

        break;
    default:
        break;
    }
    Logger::Error(U8("当てはまるシェーダーがありません"));
    return "";
}

class PipeLineStateSet
{
public:
    PipeLineStateSet() = default;
    virtual ~PipeLineStateSet() {};

    // StaticMesh のパイプラインの設定する関数
    void InitStaticMesh(ID3D11Device* device)
    {
        HRESULT hr = S_OK;
        PipeLineStateDesc desc;

        // StaticMesh
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        desc.primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        desc.rasterState = RASTERIZE_STATE::SOLID_CULL_BACK;
        desc.depthState = DEPTH_STATE::ZT_ON_ZW_ON;
        hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelStaticBatchingVS.cso", desc.vertexShader.ReleaseAndGetAddressOf(), desc.inputLayout.ReleaseAndGetAddressOf(), inputElementDesc, _countof(inputElementDesc));

        // StaticMesh forward Opaque 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelForwardTransparencyPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("forwardOpaqueStaticMesh", desc);
        }

        // StaticMesh deferred Opaque 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelDeferredPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("deferredOpaqueStaticMesh", desc);
        }

        // StaticMesh deferred Stage Blend 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelDeferredPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA;
            AddPipeLineState("deferredBlendStaticMesh", desc);
        }

        // StaticMesh forward Mask 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelForwardTransparencyPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("forwardMaskStaticMesh", desc);
        }

        // StaticMesh deferred Mask 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelDeferredPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("deferredMaskStaticMesh", desc);
        }

        // StaticMesh forward Blend 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelForwardTransparencyPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA;
            AddPipeLineState("forwardBlendStaticMesh", desc);
        }

        // StaticMesh deferred Blend 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelDeferredPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA;
            AddPipeLineState("deferredBlendStaticMesh", desc);
        }

        //// StaticMesh deferred stage 用
        //{
        //    hr = CreatePsFromCSO(device, "./Shader/GltfModelFightStagePS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
        //    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

        //    AddPipeLineState("deferredFightStage", desc);
        //}

        // StaticMesh Cascade ShadowMap 用
        {
            desc.pixelShader = nullptr;
            hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelStaticBatchingCsmVS.cso", desc.vertexShader.ReleaseAndGetAddressOf(), NULL, NULL, 0);
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            hr = CreateGsFromCSO(device, "./Data/Shaders/GltfModelCsmGS.cso", desc.geometryShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            desc.blendState = BLEND_STATE::NONE;
            AddPipeLineState("CascadeShadowMapStaticMesh", desc);
        }
    }

    void InitInstanceMesh(ID3D11Device* device)
    {
        HRESULT hr = S_OK;
        PipeLineStateDesc desc;
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "JOINTS", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "JOINTS", 1, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "WEIGHTS", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        desc.primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelVS.cso", desc.vertexShader.ReleaseAndGetAddressOf(), desc.inputLayout.ReleaseAndGetAddressOf(), inputElementDesc, _countof(inputElementDesc));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        desc.rasterState = RASTERIZE_STATE::SOLID_CULL_NONE;
        desc.depthState = DEPTH_STATE::ZT_ON_ZW_ON;
    }

    // SkeletalMesh のパイプラインの設定する関数
    void InitSkeletalMesh(ID3D11Device* device)
    {
        HRESULT hr = S_OK;
        PipeLineStateDesc desc;
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "JOINTS", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "JOINTS", 1, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "WEIGHTS", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        desc.primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelVS.cso", desc.vertexShader.ReleaseAndGetAddressOf(), desc.inputLayout.ReleaseAndGetAddressOf(), inputElementDesc, _countof(inputElementDesc));
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        desc.rasterState = RASTERIZE_STATE::SOLID_CULL_NONE;
        desc.depthState = DEPTH_STATE::ZT_ON_ZW_ON;

        // SkeletalMesh forward Opaque 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("forwardOpaqueSkeletalMesh", desc);
        }

        // SkeletalMesh forward Opaque 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/DarkStagePlayerWeaponPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("DarkStagePlayerWeaponPS", desc);
        }

        // SkeletalMesh deferred Opaque 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelDeferredPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("deferredOpaqueSkeletalMesh", desc);
        }

        // StaticMesh deferred stage 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelFightStagePS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            AddPipeLineState("deferredFightStage", desc);
        }

        // SkeletalMesh forward Mask 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("forwardMaskSkeletalMesh", desc);
        }

        // SkeletalMesh deferred Mask 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelDeferredPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("deferredMaskSkeletalMesh", desc);
        }

        // SkeletalMesh forward Blend 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelForwardTransparencyPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA;
            AddPipeLineState("forwardBlendSkeletalMesh", desc);
        }


        // SkeletalMesh deferred Blend 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelDeferredPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA;
            AddPipeLineState("deferredBlendSkeletalMesh", desc);
        }

        // ポイントライト 用　
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/PointLightModelPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("pointLightSkeletalMesh", desc);
        }

        // deferred キャラクターの髪の毛とかファー 用
        {
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelHairOrFurDeferredPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            //desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            AddPipeLineState("characterFurAndHairSkeletalMesh", desc);
        }

        // elasticBuilding forward Blend 用
        {
            hr = CreateVsFromCSO(device, "./Data/Shaders/ElasticBuildsVS.cso", desc.vertexShader.ReleaseAndGetAddressOf(), desc.inputLayout.ReleaseAndGetAddressOf(), inputElementDesc, _countof(inputElementDesc));
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelForwardTransparencyPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA;
            AddPipeLineState("elasticBuildingForward", desc);
        }

        // elasticBuilding deferred Blend 用
        {
            hr = CreateVsFromCSO(device, "./Data/Shaders/ElasticBuildsVS.cso", desc.vertexShader.ReleaseAndGetAddressOf(), desc.inputLayout.ReleaseAndGetAddressOf(), inputElementDesc, _countof(inputElementDesc));
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            hr = CreatePsFromCSO(device, "./Data/Shaders/GltfModelDeferredPS.cso", desc.pixelShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            desc.blendState = BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE;
            desc.rasterState = RASTERIZE_STATE::SOLID_CULL_NONE;
            AddPipeLineState("elasticBuildingDeferred", desc);
        }

        // SkeletalMesh Cascade ShadowMap 用
        {
            desc.pixelShader = nullptr;
            hr = CreateVsFromCSO(device, "./Data/Shaders/GltfModelCsmVS.cso", desc.vertexShader.ReleaseAndGetAddressOf(), NULL, NULL, 0);
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            hr = CreateGsFromCSO(device, "./Data/Shaders/GltfModelCsmGS.cso", desc.geometryShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            AddPipeLineState("CascadeShadowMapSkeletalMesh", desc);
        }

        // ElasticBuilding Cascade ShadowMap 用
        {
            desc.pixelShader = nullptr;
            hr = CreateVsFromCSO(device, "./Data/Shaders/ElasticBuildingCsmVS.cso", desc.vertexShader.ReleaseAndGetAddressOf(), NULL, NULL, 0);
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            hr = CreateGsFromCSO(device, "./Data/Shaders/GltfModelCsmGS.cso", desc.geometryShader.ReleaseAndGetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
            AddPipeLineState("CascadeShadowMapElasticBuilding", desc);
        }


    }

    const PipeLineStateDesc& Get(const std::string& name)const
    {
        auto it = sets_.find(name);
        if (it == sets_.end())
        {// 見つからなかったら
            _ASSERT(L"このパイプラインステートは設定されていません。");
        }
        return it->second;
    }

    void AddPipeLineState(const std::string& name, const PipeLineStateDesc& state)
    {
        sets_[name] = state;
    }

    void BindPipeLineState(ID3D11DeviceContext* immediateContext, const std::string& name)
    {
        auto it = sets_.find(name);
        if (it == sets_.end())
        {// 見つからなかったら
            _ASSERT(L"このパイプラインステートは設定されていません。");
        }

        immediateContext->IASetInputLayout(sets_[name].inputLayout.Get());
        immediateContext->IASetPrimitiveTopology(sets_[name].primitiveTopology);
        immediateContext->VSSetShader(sets_[name].vertexShader.Get(), nullptr, 0);
        immediateContext->PSSetShader(sets_[name].pixelShader.Get(), nullptr, 0);
        immediateContext->DSSetShader(sets_[name].domainShader.Get(), nullptr, 0);
        immediateContext->GSSetShader(sets_[name].geometryShader.Get(), nullptr, 0);
        immediateContext->HSSetShader(sets_[name].hullShader.Get(), nullptr, 0);

        RenderState::BindDepthStencilState(immediateContext, sets_[name].depthState);
        RenderState::BindBlendState(immediateContext, sets_[name].blendState);
        //RenderState::BindRasterizerState(immediateContext, sets_[name].rasterState);
    }


private:
    std::unordered_map<std::string, PipeLineStateDesc> sets_;
};


