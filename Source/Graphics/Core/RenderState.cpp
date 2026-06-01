#include "pch.h"
#include "RenderState.h"

#include "Engine/Utility/Win32Utils.h"
#include "Graphics/Core/Graphics.h"


void RenderState::Initialize()
{
    HRESULT hr{ S_OK };
    ID3D11Device* device = Graphics::GetDevice();

    // POINT_WRAP サンプラー（テクスチャの拡大縮小に最近傍補間を使用し、テクスチャ座標が0.0から1.0の範囲を超える場合は繰り返す）
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;// U座標（X軸方向）のラッピング（繰り返し）
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;// V座標（Y軸方向）のラッピング（繰り返し）
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;// W座標のラッピング（繰り返し）
        samplerDesc.MipLODBias = 0.0f;// ミップマップのバイアス設定
        samplerDesc.MaxAnisotropy = 1;// 最大異方性サンプリング数（画質を向上させる）
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;// 比較関数
        samplerDesc.BorderColor[0] = 0;// 境界色（透明）
        samplerDesc.BorderColor[1] = 0;
        samplerDesc.BorderColor[2] = 0;
        samplerDesc.BorderColor[3] = 0;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::POINT)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // LINEAR_WRAP サンプラー（テクスチャの拡大縮小に線形補間を使用し、テクスチャ座標が0.0から1.0の範囲を超える場合は繰り返す）
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;// U座標（X軸方向）のラッピング（繰り返し）
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;// V座標（Y軸方向）のラッピング（繰り返し）
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;// W座標のラッピング（繰り返し）
        samplerDesc.MipLODBias = 0.0f;// ミップマップのバイアス設定
        samplerDesc.MaxAnisotropy = 1;// 最大異方性サンプリング数（画質を向上させる）
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;// 比較関数
        samplerDesc.BorderColor[0] = 0;// 境界色（透明）
        samplerDesc.BorderColor[1] = 0;
        samplerDesc.BorderColor[2] = 0;
        samplerDesc.BorderColor[3] = 0;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // ANISOTROPIC_WRAP サンプラー（テクスチャの拡大縮小に異方性フィルタリングを使用し、テクスチャ座標が0.0から1.0の範囲を超える場合は繰り返す）
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;// U座標（X軸方向）のラッピング（繰り返し）
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;// V座標（Y軸方向）のラッピング（繰り返し）
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;// W座標のラッピング（繰り返し）
        samplerDesc.MipLODBias = 0.0f;// ミップマップのバイアス設定
        samplerDesc.MaxAnisotropy = 8;// 最大異方性サンプリング数（画質を向上させる）
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;// 比較関数
        samplerDesc.BorderColor[0] = 0;// 境界色（透明）
        samplerDesc.BorderColor[1] = 0;
        samplerDesc.BorderColor[2] = 0;
        samplerDesc.BorderColor[3] = 0;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::ANISOTROPIC)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // LINEAR_BORDER_BLACK サンプラー（テクスチャの拡大縮小に線形補間を使用し、テクスチャ座標が0.0から1.0の範囲を超える場合は境界色（黒）で塗りつぶす）
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.MipLODBias = 0.0f;// ミップマップのバイアス設定
        samplerDesc.MaxAnisotropy = 1;// 最大異方性サンプリング数（画質を向上させる）
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;// 比較関数
        samplerDesc.BorderColor[0] = 0;
        samplerDesc.BorderColor[1] = 0;
        samplerDesc.BorderColor[2] = 0;
        samplerDesc.BorderColor[3] = 1;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_BLACK)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // LINEAR_BORDER_WHITE サンプラー（テクスチャの拡大縮小に線形補間を使用し、テクスチャ座標が0.0から1.0の範囲を超える場合は境界色（白）で塗りつぶす）
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.MipLODBias = 0.0f;// ミップマップのバイアス設定
        samplerDesc.MaxAnisotropy = 1;// 最大異方性サンプリング数（画質を向上させる）
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;// 比較関数
        samplerDesc.BorderColor[0] = 1;
        samplerDesc.BorderColor[1] = 1;
        samplerDesc.BorderColor[2] = 1;
        samplerDesc.BorderColor[3] = 1;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_WHITE)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // LINEAR_CLAMP サンプラー（テクスチャの拡大縮小に線形補間を使用し、テクスチャ座標が0.0から1.0の範囲を超える場合は最も近い有効なテクスチャ座標でサンプリング）
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MipLODBias = 0.0f;// ミップマップのバイアス設定
        samplerDesc.MaxAnisotropy = 1;// 最大異方性サンプリング数（画質を向上させる）
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;// 比較関数
        samplerDesc.BorderColor[0] = 0;
        samplerDesc.BorderColor[1] = 0;
        samplerDesc.BorderColor[2] = 0;
        samplerDesc.BorderColor[3] = 0;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_CLAMP)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // LINEAR_MIRROR サンプラー（テクスチャの拡大縮小に線形補間を使用し、テクスチャ座標が0.0から1.0の範囲を超える場合は鏡面反射のように繰り返す）
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
        samplerDesc.MipLODBias = 0;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.BorderColor[0] = 0;
        samplerDesc.BorderColor[1] = 0;
        samplerDesc.BorderColor[2] = 0;
        samplerDesc.BorderColor[3] = 0;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_MIRROR)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // COMPARISON_LINEAR_BORDER_WHITE サンプラー（テクスチャの拡大縮小に線形補間を使用し、テクスチャ座標が0.0から1.0の範囲を超える場合は境界色（白）で塗りつぶし、比較関数を使用してシャドウマッピングなどに利用）
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.MipLODBias = 0;
        samplerDesc.MaxAnisotropy = 16;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        samplerDesc.BorderColor[0] = 1;
        samplerDesc.BorderColor[1] = 1;
        samplerDesc.BorderColor[2] = 1;
        samplerDesc.BorderColor[3] = 1;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::COMPARISON_LINEAR_BORDER_WHITE)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    //	モーションブラー用サンプラーを設定
    {
        D3D11_SAMPLER_DESC sampler_desc{};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MipLODBias = 0;
        sampler_desc.MaxAnisotropy = 16;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sampler_desc.BorderColor[0] = FLT_MAX;
        sampler_desc.BorderColor[1] = FLT_MAX;
        sampler_desc.BorderColor[2] = FLT_MAX;
        sampler_desc.BorderColor[3] = FLT_MAX;
        sampler_desc.MinLOD = 0;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&sampler_desc, samplerStates[static_cast<size_t>(SAMPLER_STATE::MOTION_BLUR)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // 深度テストやステンシルバッファの設定を行う（画面の奥行きを扱う）
    // 深度テストON、深度書き込みON ZT_ON_ZW_ON
    {
        D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
        depthStencilDesc.DepthEnable = TRUE;// 深度テストを有効にする
        depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; // 深度を書き込む
        depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; // 深度比較方法（近いものから描画）
        hr = device->CreateDepthStencilState(&depthStencilDesc, depthStencilStates[static_cast<size_t>(DEPTH_STATE::ZT_ON_ZW_ON)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    // 深度テストON、深度書き込みOFF ZT_ON_ZW_OFF
    {
        D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
        depthStencilDesc.DepthEnable = TRUE;// 深度テストを有効にする
        depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;// 深度書き込みを無効にする
        depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        hr = device->CreateDepthStencilState(&depthStencilDesc, depthStencilStates[static_cast<size_t>(DEPTH_STATE::ZT_ON_ZW_OFF)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    // 深度テストOFF、深度書き込みON ZT_OFF_ZW_ON
    {
        D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
        depthStencilDesc.DepthEnable = FALSE;// 深度テストを無効にする
        depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; // 深度書き込みを有効にする
        depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        hr = device->CreateDepthStencilState(&depthStencilDesc, depthStencilStates[static_cast<size_t>(DEPTH_STATE::ZT_OFF_ZW_ON)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    // 深度テストOFF、深度書き込みOFF ZT_OFF_ZW_OFF
    {
        D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
        depthStencilDesc.DepthEnable = FALSE; // 深度テストを無効にする
        depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;// 深度書き込みを無効にする
        depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        hr = device->CreateDepthStencilState(&depthStencilDesc, depthStencilStates[static_cast<size_t>(DEPTH_STATE::ZT_OFF_ZW_OFF)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    // デカール用深度バッファ 制限用深度
    {
        D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
        depthStencilDesc.DepthEnable = TRUE;
        //	深度比較は行うが書き込まない
        depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

        depthStencilDesc.StencilEnable = TRUE;
        depthStencilDesc.StencilReadMask = 0xFF;
        depthStencilDesc.StencilWriteMask = 0xFF;

        //	ステンシルの設定
        //	裏面描画時はステンシルを書き込み
        depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
        depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
        depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
        //	表面描画時はステンシルを比較
        depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
        depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
        depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
        depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
        hr = device->CreateDepthStencilState(&depthStencilDesc, depthStencilStates[static_cast<size_t>(DEPTH_STATE::DECAL)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // ブレンディングステートを作成する処理
    // BLEND_STATE::NONE ブレンドなしの設定
    {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE; // AlphaToCoverageを無効にする
        blendDesc.IndependentBlendEnable = FALSE; // 複数のターゲットを使わない
        blendDesc.RenderTarget[0].BlendEnable = FALSE; // ブレンドを無効にする
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; // ブレンド演算は加算
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; // アルファのソースは常に1
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; // アルファの宛先は0
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; // アルファ演算は加算
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; // 色全体を書き込む
        hr = device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::NONE)].GetAddressOf()); // ブレンド設定の作成
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr)); // 作成に成功したか確認
    }

    // BLEND_STATE::ALPHA アルファブレンド設定 (透過処理)
    {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE; // AlphaToCoverageを無効にする（透明度でマスクをかけない）
        blendDesc.IndependentBlendEnable = FALSE; // 複数のレンダターターゲットを使わない
        blendDesc.RenderTarget[0].BlendEnable = TRUE; // 透過を有効にする
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; // ソースのアルファ値を使ってブレンド
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; // 宛先のアルファ値を反転させてブレンド
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; // ブレンド演算：加算
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; // アルファのソースブレンドは常に1
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; // 宛先のアルファ値を反転させてブレンド
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; // アルファ用の演算は加算
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; // 色のすべてのチャネルを書き込む
        hr = device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::ALPHA)].GetAddressOf()); // 上記設定を使ってブレンディングステートを作成
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    // BLEND_STATE::ADD 加算ブレンド設定
    {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE; // AlphaToCoverageを無効にする
        blendDesc.IndependentBlendEnable = FALSE; // 複数ターゲットを使わない
        blendDesc.RenderTarget[0].BlendEnable = TRUE; // ブレンドを有効にする
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE; // 宛先ブレンドはゼロ（色を加算する）
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; // ブレンド演算は加算
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO; // アルファのソースは常に1
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE; // アルファの宛先はゼロ
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; // アルファ演算は加算
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; // 色全体を書き込む
        hr = device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::ADD)].GetAddressOf()); // 加算ブレンド設定を作成
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    // BLEND_STATE::MULTIPLY 乗算ブレンド設定
    {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::MULTIPLY)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA 乗算ブレンド設定（アルファチャンネルを考慮して複数のレンダターゲットに対応）
    {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = TRUE;
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        for (int i = 1; i < 8; ++i)
        {
            blendDesc.RenderTarget[i].BlendEnable = FALSE;
            blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }
        hr = device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::MULTIPLY_RENDER_TARGET_ALPHA)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE 無効なブレンド設定（ブレンドなし）
    {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = TRUE;
        blendDesc.RenderTarget[0].BlendEnable = FALSE; // ブレンドを無効にする
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; // ブレンド演算は加算
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; // アルファのソースは常に1
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; // アルファの宛先は0
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; // アルファ演算は加算
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; // 色全体を書き込む
        for (int i = 1; i < 8; ++i)
        {
            blendDesc.RenderTarget[i].BlendEnable = FALSE;
            blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }
        hr = device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::MULTIPLY_RENDER_TARGET_NONE)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // ラスタライザーステートの作成
    // RASTERIZE_STATE::SOLID_CULL_BACK 後面カリングのラスタライザーステート設定
    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {}; // ラスタライザの設定
        rasterizerDesc.FillMode = D3D11_FILL_SOLID; // 塗りつぶしモード
        rasterizerDesc.CullMode = D3D11_CULL_BACK; // 背面カリング（裏面を描画しない）
        rasterizerDesc.FrontCounterClockwise = TRUE; // 順方向は時計回り
        rasterizerDesc.DepthBias = 0; // 深度バイアス（今回はなし）
        rasterizerDesc.DepthBiasClamp = 0; // 深度バイアスの制限
        rasterizerDesc.SlopeScaledDepthBias = 0; // 傾斜スケールの深度バイアス
        rasterizerDesc.DepthClipEnable = TRUE; // 深度クリップを有効にする
        rasterizerDesc.ScissorEnable = FALSE; // スクリーンの切り抜きを無効にする
        rasterizerDesc.MultisampleEnable = TRUE; 
        rasterizerDesc.AntialiasedLineEnable = FALSE; // ラインアンチエイリアスを無効にする
        hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerState[static_cast<size_t>(RASTERIZE_STATE::SOLID_CULL_BACK)].GetAddressOf()); // ラスタライザーステート作成
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // RASTERIZE_STATE::WIREFRAME_CULL_NONE  ワイヤーフレーム描画（カリングなし）のラスタライザーステート設定
    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {}; // ラスタライザの設定
        rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME; // ワイヤーフレーム描画に変更
        rasterizerDesc.CullMode = D3D11_CULL_NONE; // カリングなし
        rasterizerDesc.FrontCounterClockwise = TRUE; // 順方向は時計回り
        rasterizerDesc.DepthBias = 0; // 深度バイアス（今回はなし）
        rasterizerDesc.DepthBiasClamp = 0; // 深度バイアスの制限
        rasterizerDesc.SlopeScaledDepthBias = 0; // 傾斜スケールの深度バイアス
        rasterizerDesc.DepthClipEnable = TRUE; // 深度クリップを有効にする
        rasterizerDesc.ScissorEnable = FALSE; // スクリーンの切り抜きを無効にする
        rasterizerDesc.MultisampleEnable = TRUE;
        rasterizerDesc.AntialiasedLineEnable = TRUE; // アンチエイリアス線を有効にする
        hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerState[static_cast<size_t>(RASTERIZE_STATE::WIREFRAME_CULL_NONE)].GetAddressOf()); // カリングなしのワイヤーフレーム設定
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr)); // 作成成功を確認
    }

    // RASTERIZE_STATE::SOLID_CULL_FRONT  前面カリングのラスタライザーステート設定
    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {}; // ラスタライザの設定
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_FRONT;
        rasterizerDesc.AntialiasedLineEnable = FALSE;
        rasterizerDesc.FrontCounterClockwise = TRUE; // 順方向は時計回り
        rasterizerDesc.DepthBias = 0; // 深度バイアス（今回はなし）
        rasterizerDesc.DepthBiasClamp = 0; // 深度バイアスの制限
        rasterizerDesc.SlopeScaledDepthBias = 0; // 傾斜スケールの深度バイアス
        rasterizerDesc.DepthClipEnable = TRUE; // 深度クリップを有効にする
        rasterizerDesc.ScissorEnable = FALSE; // スクリーンの切り抜きを無効にする
        rasterizerDesc.MultisampleEnable = TRUE;
        hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerState[static_cast<size_t>(RASTERIZE_STATE::SOLID_CULL_FRONT)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

    // RASTERIZE_STATE::SOLID_CULL_NONE カリングなしのラスタライザーステート設定
    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {}; // ラスタライザの設定
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_NONE;
        rasterizerDesc.FrontCounterClockwise = TRUE; // 順方向は時計回り
        rasterizerDesc.DepthBias = 0; // 深度バイアス（今回はなし）
        rasterizerDesc.DepthBiasClamp = 0; // 深度バイアスの制限
        rasterizerDesc.SlopeScaledDepthBias = 0; // 傾斜スケールの深度バイアス
        rasterizerDesc.DepthClipEnable = TRUE; // 深度クリップを有効にする
        rasterizerDesc.ScissorEnable = FALSE; // スクリーンの切り抜きを無効にする
        rasterizerDesc.MultisampleEnable = TRUE;
        rasterizerDesc.AntialiasedLineEnable = FALSE; 
        hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerState[static_cast<size_t>(RASTERIZE_STATE::SOLID_CULL_NONE)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr)); // 作成成功を確認
    }

    // RASTERIZE_STATE::USE_SCISSOR_RECTS スクリーン切り抜き有効のラスタライザーステート設定
    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {}; // ラスタライザの設定
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_BACK;
        rasterizerDesc.ScissorEnable = TRUE;
        rasterizerDesc.FrontCounterClockwise = TRUE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;
        rasterizerDesc.FrontCounterClockwise = TRUE; // 順方向は時計回り
        rasterizerDesc.DepthBias = 0; // 深度バイアス（今回はなし）
        rasterizerDesc.DepthBiasClamp = 0; // 深度バイアスの制限
        rasterizerDesc.SlopeScaledDepthBias = 0; // 傾斜スケールの深度バイアス
        rasterizerDesc.DepthClipEnable = TRUE; // 深度クリップを有効にする
        rasterizerDesc.MultisampleEnable = TRUE;
        hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerState[static_cast<int>(RASTERIZE_STATE::USE_SCISSOR_RECTS)].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }

}

// サンプラステート設定
void RenderState::SetSamplerState(ID3D11DeviceContext* immediateContext)
{
    //サンプラーステートオブジェクトをバインドする
    // ピクセルシェーダーで使用するサンプラー（テクスチャの取得方法）を設定
    immediateContext->PSSetSamplers(0, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::POINT)].GetAddressOf());
    immediateContext->PSSetSamplers(1, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR)].GetAddressOf());
    immediateContext->PSSetSamplers(2, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::ANISOTROPIC)].GetAddressOf());
    immediateContext->PSSetSamplers(3, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_BLACK)].GetAddressOf());
    immediateContext->PSSetSamplers(4, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_WHITE)].GetAddressOf());
    immediateContext->PSSetSamplers(5, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_CLAMP)].GetAddressOf());
    immediateContext->PSSetSamplers(6, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_MIRROR)].GetAddressOf());
    immediateContext->PSSetSamplers(7, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::MOTION_BLUR)].GetAddressOf());
    immediateContext->PSSetSamplers(8, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::COMPARISON_LINEAR_BORDER_WHITE)].GetAddressOf());

    // コンピュートシェーダー
    immediateContext->CSSetSamplers(0, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::POINT)].GetAddressOf());
    immediateContext->CSSetSamplers(1, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR)].GetAddressOf());
    immediateContext->CSSetSamplers(2, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::ANISOTROPIC)].GetAddressOf());
    immediateContext->CSSetSamplers(3, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_BLACK)].GetAddressOf());
    immediateContext->CSSetSamplers(4, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_WHITE)].GetAddressOf());
    immediateContext->CSSetSamplers(5, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_CLAMP)].GetAddressOf());
    immediateContext->CSSetSamplers(6, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_MIRROR)].GetAddressOf());
    immediateContext->CSSetSamplers(7, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::MOTION_BLUR)].GetAddressOf());
    immediateContext->CSSetSamplers(8, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::COMPARISON_LINEAR_BORDER_WHITE)].GetAddressOf());

    // ジオメトリックシェーダー
    immediateContext->GSSetSamplers(0, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::POINT)].GetAddressOf());
    immediateContext->GSSetSamplers(1, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR)].GetAddressOf());
    immediateContext->GSSetSamplers(2, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::ANISOTROPIC)].GetAddressOf());
    immediateContext->GSSetSamplers(3, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_BLACK)].GetAddressOf());
    immediateContext->GSSetSamplers(4, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_WHITE)].GetAddressOf());
    immediateContext->GSSetSamplers(5, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_CLAMP)].GetAddressOf());
    immediateContext->GSSetSamplers(6, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_MIRROR)].GetAddressOf());
    immediateContext->GSSetSamplers(7, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::MOTION_BLUR)].GetAddressOf());
    immediateContext->GSSetSamplers(8, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::COMPARISON_LINEAR_BORDER_WHITE)].GetAddressOf());

}