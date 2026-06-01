#pragma once

#include "Engine/Scene/Scene.h"

#include <d3d11.h>
#include <wrl.h>
#include <memory>

#include "Core/ActorManager.h"
#include "Engine/Scene/SceneBase.h"


#include "Graphics/Renderer/SceneRenderer.h"

#include "Game/Actors/Camera/LoadingCamera.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Actors/Stage/ClothSimulate.h"
#include "Game/Actors/Stage/Stage.h"
#include "Game/DarkGame/DarkActors/DarkStageAsset.h"

#include "UI/Widgets/Widget.h"

#include "PBD/PBDSystem.h"

class GameScene : public SceneBase
{
public:
    bool Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override;

    void Start() override;

    void Update(float deltaTime) override;

    // 定数バッファの更新処理をシーンごとにカスタマイズできるようにするための仮想関数
    void UpdateConstants(ID3D11DeviceContext* immediateContext, float deltaTime)override;

    bool Uninitialize(ID3D11Device* device) override;

    void DrawGui() override;

    void SetUpActors()override;

    //シーンの自動登録
    static inline Scene::Autoenrollment<GameScene> _autoenrollment;

private:
    struct SkyShaderConstants
    {
        DirectX::XMFLOAT3 topColor= { 0.0f, 0.0f, 0.0f };
        float scrollSpeed = 0.03f;

        DirectX::XMFLOAT3 bottomColor= { 0.025f,0.016f,0.73f };
        float cloudIntensity= 0.6f;

        DirectX::XMFLOAT3 sunColor = { 1.0f, 0.95f, 0.75f };
        float sunSize = 0.98f;

        DirectX::XMFLOAT3 cloudColor = { 1.0f, 1.0f, 1.0f };
        float cloudThreshold= 0.5f;

        float starScale= 1.3f;
        DirectX::XMFLOAT2 starOffset= { 0.0f,0.0f };
        float starIntensity= 4.4f;

        DirectX::XMFLOAT3 moonColor= { 0.98f,1.0f,0.0f };
        float moonRadius= 0.09f;

        DirectX::XMFLOAT2 moonPos= { 0.1f,0.1f };
        DirectX::XMFLOAT2 moonOffset= { 1.00/*5*/,1.0f };

        DirectX::XMFLOAT3 startAuroraColor= { 0.0f,0.0f,0.0f };
        float value=15.5f;

        DirectX::XMFLOAT3 endAuroraColor= { 0.0f,0.0f,0.0f };
        float value1= 1.2f;
    };
    std::unique_ptr<ConstantBuffer<SkyShaderConstants>> skyShaderConstantsBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> darkStageSkyPS;

    std::shared_ptr<StageAsset> stageAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageCandelabraAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageGroundBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageMeltedWaxAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageStandingBrazierAsset = std::make_shared<StageAsset>();

    std::thread loadStageThread;
    std::thread loadStageAssetsThread;

    std::unique_ptr<PBD::System> pbd;


    std::shared_ptr<InterleavedGltfModel> model;

    std::shared_ptr<Player> player;
    TPSCameraComponent* mainCameraComponent = nullptr;

    std::unique_ptr<ClothSimulate> clothSimulate;

};
