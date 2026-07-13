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


    // pressButtonのUIコンポーネント
    std::shared_ptr<UIImageComponent> pressButtonUiComponent;
    std::shared_ptr<Sprite> controlButton;
    std::shared_ptr<Sprite> keyboardButton;
};
