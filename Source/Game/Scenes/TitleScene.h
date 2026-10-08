#pragma once

#include "Engine/Scene/Scene.h"

#include <d3d11.h>
#include <wrl.h>
#include <memory>

#include "Core/ActorManager.h"
#include "Engine/Scene/SceneBase.h"
#include "Game/Actors/BgmActor.h"



#include "Game/Actors/Camera/LoadingCamera.h"
#include "Game/Actors/Player/TitlePlayer.h"
#include "Game/DarkGame/DarkActors/DarkStageAsset.h"
#include "UI/Widgets/Widget.h"



class TitleScene : public SceneBase
{
public:
    bool Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override;

    void Start() override;

    void Update(float deltaTime) override;

    bool Uninitialize(ID3D11Device* device) override;

    void DrawGui() override;

    void SetUpActors()override;

    //シーンの自動登録
    static inline Scene::Autoenrollment<TitleScene> _autoenrollment;

private:
    enum class TitleMenuSelection : uint8_t { StartGame, QuitGame };
    void HandleTitleMenuInput(); void SetTitleMenuSelection(TitleMenuSelection, bool); void UpdateTitleMenuAnimations(float); void UpdateTitleSelectionLines();
    std::thread loadStageThread;
    std::thread loadStageAssetsThread;

    std::shared_ptr<StageAsset> stageAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageCandelabraAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageGroundBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageMeltedWaxAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageStandingBrazierAsset = std::make_shared<StageAsset>();
    std::shared_ptr<StageAsset> stageCandleStandAsset = std::make_shared<StageAsset>();

    // ゲームBGMアクター
    std::shared_ptr<BgmActor> gameBgmActor;

    std::shared_ptr<TitlePlayer> player;
    // カメラ
    TPSCameraComponent* mainCameraComponent = nullptr;
    std::shared_ptr<MainCamera> mainCameraActor;
    // タイトル固定用カメラ
    std::shared_ptr<CinemaCamera> cinemaCameraActor;

    std::shared_ptr<UIButtonComponent> startGameButton, quitGameButton;
    std::shared_ptr<UIImageComponent> selectionLineLeft, selectionLineRight;
    TitleMenuSelection titleMenuSelection = TitleMenuSelection::StartGame; bool titleMenuStickArmed = true;

    DirectX::XMFLOAT2 startGamePosition{ 1552.0f, 888.0f };
    DirectX::XMFLOAT2 quitGamePosition{ 1552.0f, 967.0f };
    float startGameBaseScale = 0.65f;
    float quitGameBaseScale = 0.65f;
    float selectedScale = 1.05f;
    float unselectedScale = 0.90f;
    float scaleAnimationDuration = 0.15f;
    float lineAnimationDuration = 0.25f;
    DirectX::XMFLOAT2 lineOffset{ 15.0f, 0.0f };
    DirectX::XMFLOAT2 lineBaseScale{ 0.5f, 0.5f };

    float startGameSelectionScale = 1.05f;
    float quitGameSelectionScale = 0.90f;
    float startGameScaleAnimationStart = 1.05f;
    float quitGameScaleAnimationStart = 0.90f;
    float titleMenuScaleAnimationElapsed = 0.15f;
    float titleMenuLineAnimationElapsed = 0.20f;
};
