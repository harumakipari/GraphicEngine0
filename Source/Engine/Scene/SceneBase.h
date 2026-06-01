#pragma once
#include "Engine/Scene/Scene.h"
#include "Graphics/Core/ConstantBuffer.h"
#include "Graphics/PostProcess/FullScreenQuad.h"
#include "Graphics/Core/LightManager.h"
#include "Graphics/PostProcess/PostEffectManager.h"
#include "Graphics/PostProcess/SceneEffectManager.h"
#include "Graphics/Renderer/SceneRenderer.h"
#include "Graphics/Environment/SkyMap.h"
#include "Graphics/PostProcess/GBuffer.h"
#include "Graphics/PostProcess/MultipleRenderTargets.h"
#include "Graphics/Shadow/CascadeShadowMap.h"
#include "UI/Widgets/Widget.h"
#include "UI/Font.h"

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <unordered_map>

#include "Graphics/PostProcess/DepthOfFieldEffect.h"
#include "Graphics/Shadow/ShadowMap.h"
#include "UI/UIManager.h"




/// @brief 全シーン共通の基底クラス。描画・定数バッファ・ライト・ポストエフェクトを統一管理する。
class SceneBase : public Scene
{
public:
    enum class RenderPass :uint8_t
    {
        Sky,
        Opaque,
        Mask,
        ForwardBlend,
        Particle,
        Debug,
        UI,
    };

    using RenderHook = std::function<void(ID3D11DeviceContext*)>;

    virtual bool Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override;

    virtual void Update(float deltaTime) override;

    // 定数バッファの更新処理をシーンごとにカスタマイズできるようにするための仮想関数
    virtual void UpdateConstants(ID3D11DeviceContext* immediateContext, float deltaTime){}

    virtual void Render(ID3D11DeviceContext* immediateContext, float deltaTime) override;

    virtual bool Uninitialize(ID3D11Device* device) override;
    virtual bool OnSizeChanged(ID3D11Device* device, UINT64 width, UINT height) override;
    virtual void DrawGui() override;

    void RegisterRenderHook(const RenderPass pass, const RenderHook& hook)
    {
        renderHooks[pass].push_back(hook);
    }

    void ExecuteHooks(const RenderPass pass, ID3D11DeviceContext* immediateContext)
    {
        for (auto& hook : renderHooks[pass])
            hook(immediateContext);
    }

    // ライトマネージャーへのアクセス関数
    //LightManager* GetLightManager() const { return lightManager.get(); }

protected:
    // UI描画
    void Draw(ID3D11DeviceContext* immediateContext);
private:
    void UpdateConstantBuffer(ID3D11DeviceContext* immediateContext, float deltaTime);

    void ForwardRender(ID3D11DeviceContext* immediateContext);

    void DeferredRender(ID3D11DeviceContext* immediateContext, const ViewConstants& viewConstants);

    void DrawOutliner();

    void DrawShortcutInfo();

    void DrawSceneSettingsTab();

    void DrawInspector();

    void DrawPostEffectTab();

    void DrawDockSpace();

    void SetupImGuiStyle();

    void DrawGizmo();

protected:

    //==============================
    // 定数バッファ構造体
    //==============================
    struct FrameConstants
    {
        float elapsedTime = 0.0f;
        float deltaTime = 0.0f;
        DirectX::XMFLOAT2 iResolution = { 1280.0f,720.0f }; // 画面解像度

        float gravity = -9.8f;
    };

    //==============================
    // メンバー変数（描画関連）
    //==============================
    std::unique_ptr<ConstantBuffer<FrameConstants>>  sceneCBuffer;
    std::unique_ptr<ConstantBuffer<SceneShaderConstants>> shaderCBuffer;
    std::unique_ptr<FullScreenQuad> fullscreenQuad;

    std::unique_ptr<FrameBuffer> frameBuffer;
    std::unique_ptr<FrameBuffer> finalBuffer;
    std::unique_ptr<FrameBuffer> imGuiGizmoBuffer;

    std::unique_ptr<CascadedShadowMaps> cascadedShadowMaps;

    std::unique_ptr<MultipleRenderTargets> multipleRenderTargets;
    std::unique_ptr<GBuffer> gBufferRenderTarget;
    std::unique_ptr<SkyMap> skyMap;
    std::unique_ptr<LightManager> lightManager;
    //std::unique_ptr<PostEffectManager> postEffectManager;
    std::unique_ptr<SceneEffectManager> sceneEffectManager;
    std::unique_ptr<SceneRenderer> sceneRenderer_;

    std::unique_ptr<DepthOfFieldEffect> depthOfFieldEffect;

    Microsoft::WRL::ComPtr<ID3D11PixelShader> postEffectPs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> finalPs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> deferredPs;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> environmentTextures[4];

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> starTexture;   // 星のテクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> skyNoiseTexture;   // 空のノイズテクスチャ

    DirectX::XMFLOAT4 lightDirection{ -0.75f, -0.581f, -0.4f, 0.0f };
    DirectX::XMFLOAT4 lightColor{ 1.0f, 1.0f, 1.0f, 20.1f };

    bool useDeferredRendering = true;
    bool useDrawDebug = true;


    SIZE framebufferDimensions = {};

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noise3d;

    std::shared_ptr<Actor> selectedActor_;  // 選択中のアクターを保持

    SceneRenderer sceneRender;

    //==============================
    // メンバー関数
    //==============================
    virtual void DrawSceneGui() {}///< 各シーン固有のImGui描画フック

    std::unordered_map<RenderPass, std::vector<RenderHook>> renderHooks;





};
