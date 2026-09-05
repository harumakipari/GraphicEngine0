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
    enum class DeathPresentationCue : uint8_t
    {
        GameplayHudFade,
        OverlayFade,
        DefeatedFade,
        BattleTimeFade,
        ButtonsFade,
    };

    void SetDeathPresentationCueCallback(std::function<void(DeathPresentationCue)> callback)
    {
        deathPresentationCueCallback = std::move(callback);
    }

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
    // 死亡時のリザルトUIを作成する
    void CreateDeathResultUI();
    void SetDeathResultVisible(bool visible);
    void SelectDeathResult(int index);
    void ExecuteDeathResult(int index);
    void UpdateDeathResultBattleTimeValue();
    void UpdateDeathResultBattleTimeLayout();
    void UpdateDeathResultUILayout();
    void UpdateDeathResultMenu();
    void UpdateDeathResultPresentation();
    void ResetDeathPresentationCues();
    void FireDeathPresentationCue(DeathPresentationCue cue, bool& fired);

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
    float deathHudFadeDuration = 0.60f;
    float deathOverlayStartTime = 4.6f;
    float deathOverlayFadeDuration = 0.4f;
    float deathOverlayMaxAlpha = 0.25f;
    float deathDefeatedStartTime = 5.0f;
    float deathDefeatedFadeDuration = 0.40f;
    float deathBattleTimeStartTime = 5.1f;
    float deathBattleTimeFadeDuration = 0.30f;
    float deathButtonsStartTime = 5.2f;
    float deathButtonsFadeDuration = 0.30f;
    float deathSelectLineStartTime = 4.40f;
    float deathResultInputEnableTime = 5.2f;
    bool deathResultVisible = false;
    bool deathResultInputEnabled = false;
    int deathResultSelection = 0;
    std::shared_ptr<UIImageComponent> deathResultDarkOverlay;
    std::shared_ptr<UIImageComponent> deathResultDefeated;
    std::shared_ptr<UIImageComponent> deathResultBattleTime;
    std::array<std::shared_ptr<UIButtonComponent>, 3> deathResultButtons{};
    DirectX::XMFLOAT2 deathResultDefeatedPosition{ 960.0f, 530.0f };
    DirectX::XMFLOAT2 deathResultDefeatedScale{ 1.0f, 1.0f };
    DirectX::XMFLOAT2 deathResultBattleTimePosition{ 770.0f, 594.0f };
    DirectX::XMFLOAT2 deathResultBattleTimeScale{ 0.5f, 0.5f };
    DirectX::XMFLOAT2 deathResultButtonStartPosition{ 1011.0f, 734.0f };
    float deathResultButtonHorizontalSpacing = 100.0f;
    DirectX::XMFLOAT2 deathResultButtonScale{ 0.4f, 0.4f };
    std::array<std::shared_ptr<UIImageComponent>, 8> deathResultTimeDigits{};
    std::array<int, 8> deathResultTimeValues{};
    DirectX::XMFLOAT2 deathResultTimePosition{ 1160.0f, 588.0f };
    DirectX::XMFLOAT2 deathResultTimeNumberScale{ 0.35f, 0.35f };
    DirectX::XMFLOAT2 deathResultColonOffset{ -8.0f, 0.0f };
    DirectX::XMFLOAT2 deathResultColonScale{ 3.5f, 2.2f };
    DirectX::XMFLOAT2 deathResultDotOffset{ -15.0f, 0.0f };
    DirectX::XMFLOAT2 deathResultDotScale{ 3.5f, 2.2f };
    float deathResultTimeNumberSpacing = 30.0f;
    float deathResultMinuteSecondSpacing = -18.0f;
    float deathResultSecondCentisecondSpacing = -26.0f;
    std::shared_ptr<UIImageComponent> deathResultSelectLineLeft;
    std::shared_ptr<UIImageComponent> deathResultSelectLineRight;
    std::array<float, 3> deathResultSelectLineDistances = { -21.0f,-39.0f,-39.0f };
    DirectX::XMFLOAT2 deathResultSelectLineScale{ 0.12f, 0.4f };
    float deathResultSelectLineAnimDuration = 0.15f;
    float deathResultSelectLineAnimProgress = 0.0f;
    float deathResultSelectedScale = 1.2f;
    CoreColor deathResultSelectedColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    CoreColor deathResultUnselectedColor{ 0.5f, 0.5f, 0.5f, 1.0f };
    float deathResultMoveSeVolume = 1.0f;
    float deathResultConfirmSeVolume = 1.0f;
    bool deathHudFadeCueFired = false;
    bool deathOverlayCueFired = false;
    bool deathDefeatedCueFired = false;
    bool deathBattleTimeCueFired = false;
    bool deathButtonsCueFired = false;
    std::function<void(DeathPresentationCue)> deathPresentationCueCallback;
    static constexpr float continueWaitDelay = 0.25f;
    float numberTexWidth = 198.f;
    float numberTexHeight = 300.f;


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
