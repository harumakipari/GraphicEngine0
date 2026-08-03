#pragma once

#include "Engine/Scene/Scene.h"

#include <d3d11.h>
#include <wrl.h>
#include <memory>

#include "Core/ActorManager.h"
#include "Engine/Scene/SceneBase.h"
#include "Game/Actors/BgmActor.h"
#include "Game/Actors/Camera/DarkGameCamera.h"


#include "Graphics/Renderer/SceneRenderer.h"

#include "Game/Actors/Camera/LoadingCamera.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Actors/Stage/ClothSimulate.h"
#include "Game/Actors/Stage/Stage.h"
#include "Game/DarkGame/DarkActors/DarkClothActor.h"
#include "Game/DarkGame/DarkActors/DarkStageAsset.h"

#include "UI/Widgets/Widget.h"

#include "PBD/PBDSystem.h"

class GruxEnemy;

class GameScene : public SceneBase
{
public:
    bool Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override;

    void Start() override;

    void Update(float deltaTime) override;

    // 定数バッファの更新処理をシーンごとにカスタマイズできるようにするための仮想関数
    void UpdateConstants(ID3D11DeviceContext* immediateContext, float deltaTime)override;

    bool Uninitialize(ID3D11Device* device) override;

    void DrawGuiPlusAlpha() override;

    void SetUpActors()override;

    //シーンの自動登録
    static inline Scene::Autoenrollment<GameScene> _autoenrollment;

    // ボスの部屋の色ラープ値を設定する
    void SetBossRoomLerpFactor(float lerpFactor);

    // ボスの部屋の色のラープを開始する関数
    void StartBossRoomLerp(float startFactor, float endFactor, float duration, std::function<void()> finished = nullptr);

    // ボスの目のみBloomをつける
    void SetEyeBloom(bool enable);

    // カメラのモードを変更する
    void ChangeCameraMode(TPSCameraController::CameraMode cameraMode);
private:

    std::shared_ptr<StageAsset> stageAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageCandelabraAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageGroundBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageMeltedWaxAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageStandingBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageCandleStandAsset = std::make_shared<StageAsset>();

    std::thread loadStageThread;
    std::thread loadStageAssetsThread;

    std::unique_ptr<PBD::System> pbd;


    std::shared_ptr<InterleavedGltfModel> model;

    std::shared_ptr<Player> player;

    std::shared_ptr<GruxEnemy> gruxEnemyActor;

    std::unique_ptr<ClothSimulate> clothSimulate;

    // カメラ
    TPSCameraComponent* mainCameraComponent = nullptr;
    std::shared_ptr<MainCamera> mainCameraActor;
    std::shared_ptr<CinemaCamera> cinemaCameraActor;

    std::shared_ptr<DarkCameraActor> darkCameraActor;

    // ボスの部屋のラープのための変数
    std::unique_ptr<EasingRunner> bossLerpEasing;
    float bossLerpEasingFactor = 0.0f;
    bool startBossRoomLerp = false;
    std::function<void()> onFinished;
    float startBossRoomLerpFactor = 0.0f;
    float endBossRoomLerpFactor = 1.0f;

    // ゲームBGMアクター
    std::shared_ptr<BgmActor> gameBgmActor;
    // ボスBGMアクター
    std::shared_ptr<BgmActor> bossBgmActor;

    // 布アクター
    std::shared_ptr<DarkClothActor> darkClothActor;
};
