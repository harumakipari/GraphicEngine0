#pragma once

#include <wrl.h>
#include <d3d11.h>
#include <vector>
// サンプラステート
enum class SAMPLER_STATE :uint8_t
{
    POINT,
    LINEAR,
    ANISOTROPIC,
    LINEAR_BORDER_BLACK,
    LINEAR_BORDER_WHITE,
    LINEAR_CLAMP,
    LINEAR_MIRROR,
    MOTION_BLUR,//モーションブラー用サンプラー
    COMPARISON_LINEAR_BORDER_WHITE,
    ENUM_COUNT,
};

//デプスステート
enum class DEPTH_STATE :uint8_t
{
    ZT_ON_ZW_ON,    //深度テストあり　　深度書き込みあり
    ZT_ON_ZW_OFF,
    ZT_OFF_ZW_ON,
    ZT_OFF_ZW_OFF,
    DECAL, // 深度比較を行うが書き込まない
    ENUM_COUNT,
};

//ブレンディングステート
enum class BLEND_STATE :uint8_t
{
    NONE,
    ALPHA,
    ADD,
    MULTIPLY,
    MULTIPLY_RENDER_TARGET_ALPHA,
    MULTIPLY_RENDER_TARGET_NONE,
    ENUM_COUNT,
};

//ラスタライザステート
enum class RASTERIZE_STATE :uint8_t
{
    SOLID_CULL_BACK,
    SOLID_CULL_NONE,
    WIREFRAME_CULL_NONE,
    USE_SCISSOR_RECTS,
    SOLID_CULL_FRONT,
    ENUM_COUNT,
};

// レンダーステート
class RenderState
{
private:
    RenderState() = default;
    virtual ~RenderState() = default;
public:
    // インスタンス取得
    //static RenderState& Instance()
    //{
    //    static RenderState instance;
    //    return instance;
    //}
    // 初期化
    static void Initialize();

    // サンプラステート設定
    static void SetSamplerState(ID3D11DeviceContext* immediateContext);

    // サンプラステート取得
    static ID3D11SamplerState* GetSamplerState(SAMPLER_STATE state)
    {
        return samplerStates[static_cast<int>(state)].Get();
    }

    // デプスステート取得
    static ID3D11DepthStencilState* GetDepthStencilState(DEPTH_STATE state)
    {
        return depthStencilStates[static_cast<int>(state)].Get();
    }

    // ブレンドステート取得
    static ID3D11BlendState* GetBlendState(BLEND_STATE state)
    {
        return blendStates[static_cast<int>(state)].Get();
    }

    // ラスタライザーステート取得
    static ID3D11RasterizerState* GetRasterizerState(RASTERIZE_STATE state)
    {
        return rasterizerState[static_cast<int>(state)].Get();
    }

    static void BindDepthStencilState(ID3D11DeviceContext* immediate_context, DEPTH_STATE depth_stencil_state, UINT stencil_ref = 0)
    {
        immediate_context->OMSetDepthStencilState(depthStencilStates[static_cast<size_t>(depth_stencil_state)].Get(), stencil_ref);
    }

    //全てのサンプラーステートを PS GS CS それぞれに設定
    static void BindSamplerStates(ID3D11DeviceContext* immediate_context)
    {
        std::vector<ID3D11SamplerState*> samplers;
        for (const Microsoft::WRL::ComPtr<ID3D11SamplerState>& sampler : samplerStates)
        {
            samplers.emplace_back(sampler.Get());
        }
        immediate_context->PSSetSamplers(0, static_cast<UINT>(samplers.size()), samplers.data());
        immediate_context->GSSetSamplers(0, static_cast<UINT>(samplers.size()), samplers.data());
        immediate_context->CSSetSamplers(0, static_cast<UINT>(samplers.size()), samplers.data());
    }

    static void BindBlendState(ID3D11DeviceContext* immediate_context, BLEND_STATE blend_state)
    {
        immediate_context->OMSetBlendState(blendStates[static_cast<size_t>(blend_state)].Get(), NULL, 0xFFFFFFFF);
    }

    static void BindRasterizerState(ID3D11DeviceContext* immediate_context, RASTERIZE_STATE rasterizer_state)
    {
        immediate_context->RSSetState(rasterizerState[static_cast<size_t>(rasterizer_state)].Get());
    }

private:
    static inline Microsoft::WRL::ComPtr<ID3D11SamplerState>		samplerStates[static_cast<int>(SAMPLER_STATE::ENUM_COUNT)];
    static inline Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilStates[static_cast<int>(DEPTH_STATE::ENUM_COUNT)];
    //画像の背景色を透過させる
    static inline Microsoft::WRL::ComPtr<ID3D11BlendState> blendStates[static_cast<int>(BLEND_STATE::ENUM_COUNT)];
    //ワイヤーフレームを描画するためのラスタライザ
    static inline Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState[static_cast<int>(RASTERIZE_STATE::ENUM_COUNT)];
};