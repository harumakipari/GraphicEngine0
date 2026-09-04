#pragma once

#include "Engine/Scene/Scene.h"

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <array>

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
    enum class DeathStagingArea : uint8_t
    {
        Center, Front, Back, Left, Right, FrontLeft, FrontRight, BackLeft, BackRight
    };

    struct DeathStagingAreaSettings
    {
        float bossDistance = 2.5f;
    };
public:
    enum class BattleFlowState : uint8_t
    {
        Intro,
        Playing,
        PlayerDead,
        ContinueWait,
        ResetForContinue,
        BossDead,
        Victory,
    };

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

    // Called when the existing boss-introduction camera blend has completed.
    void StartBossBattle();
private:
    void UpdateBattleFlow();
    void SetBattleHudVisible(bool visible);
    void EnterPlayerDead();
    void StageDeathActors();
    DeathStagingArea DetermineDeathStagingArea(const DirectX::XMFLOAT3& originalPlayerPosition) const;
    void OnPlayerDeathCameraStart();
    void ResetBattleForContinue();
    void EnterBossDead();
    void CreateDeathResultUI();
    void SetDeathResultVisible(bool visible);
    void SelectDeathResult(int index);
    void ExecuteDeathResult(int index);

    std::shared_ptr<StageAsset> mainRoomAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> bossRoomAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> transitionAreaAsset = std::make_shared<StageAsset>();
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

    BattleFlowState battleFlowState = BattleFlowState::Intro;
    Transform playerBattleStartTransform{};
    Transform bossBattleStartTransform{};
    bool battleStartTransformsSaved = false;
    bool deathCameraStartRequested = false;
    float playerDeadElapsed = 0.0f;
    float battleElapsedTime = 0.0f;
    float deathAttemptTime = 0.0f;
    float deathPresentationElapsed = 0.0f;
    float deathResultDelay = 4.0f;
    float deathResultInputDelay = 0.3f;
    bool deathResultVisible = false;
    bool deathResultInputEnabled = false;
    int deathResultSelection = 0;
    std::shared_ptr<UIImageComponent> deathResultDefeated;
    std::shared_ptr<UIImageComponent> deathResultBattleTime;
    std::array<std::shared_ptr<UIButtonComponent>, 3> deathResultButtons{};
    DirectX::XMFLOAT2 deathResultDefeatedPosition{ 960.0f, 160.0f };
    DirectX::XMFLOAT2 deathResultBattleTimePosition{ 960.0f, 330.0f };
    DirectX::XMFLOAT2 deathResultButtonStartPosition{ 960.0f, 500.0f };
    float deathResultButtonSpacing = 130.0f;
    static constexpr float continueWaitDelay = 0.25f;

    // Death staging bounds in world XZ coordinates.
    float deathStagingMinPlayerX = 0.0f;
    float deathStagingMaxPlayerX = 17.425f;
    float deathStagingMinPlayerZ = 1.0f;
    float deathStagingMaxPlayerZ = 20.45f;
    float deathStagingCornerInsetX = 0.8f;
    float deathStagingCornerInsetZ = 0.8f;
    std::array<DeathStagingAreaSettings, 9> deathStagingAreaSettings{};

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
