#include "pch.h"
#include "TitleScene.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "imgui.h"
#endif

#include "Components/Audio/AudioSourceComponent.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Framework/Framework.h"
#include "Graphics/Core/RenderState.h"
#include "Engine/Input/InputSystem.h"
#include "Core/ActorManager.h"
#include "Engine/Debug/SceneEditor.h"
#include "Engine/Utility/Time.h"

#include "Game/Actors/Camera/LoadingCamera.h"
#include "Game/Actors/Player/Player.h"
#include "Game/Actors/Player/TitlePlayer.h"
#include "Game/Actors/Stage/ElasticBuilding.h"
#include "Game/Actors/Stage/Cloth.h"


#include "Physics/Physics.h"
#include "Game/DarkGame/DarkActors/DarkStage.h"
#include "Game/DarkGame/DarkActors/DarkStageCandelabraActor.h"
#include "Game/DarkGame/DarkActors/DarkStageChandelierActor.h"
#include "Game/DarkGame/DarkActors/DarkTitleStage.h"
#include "Game/DarkGame/DarkActors/DoorActor.h"
#include "Game/DarkGame/DarkActors/LanternActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

#include "Game/DarkGame/DarkActors/DarkEnemy/SkeletonWarriorEnemy.h"

#include "Physics/CollisionSystem.h"
#include "UI/UIManager.h"
#include "UI/Game/Pause.h"
namespace { constexpr const char* Push="./Data/Sound/SE/button_push.wav"; constexpr const char* Move="./Data/Sound/SE/button_select_move.wav"; }

bool TitleScene::Initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
{
    loadStageThread = std::thread([&]()
        {
            PROFILE_SCOPE("Load StageModel");
            stageAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/TitleStage0712/DarkStage.gltf",
                ModelTypes::ModelMode::StaticMesh, false, true);
            stageAsset->spawnPoints = stageAsset->model->spawnPoints;
        });
    loadStageAssetsThread = std::thread([&]()
        {
            PROFILE_SCOPE("Load StageAssetModel");
            //stageCandelabraAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Candelabra/Candelabra.gltf", ModelTypes::ModelMode::StaticMesh, false, true);
            stageCandelabraAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Candelabra/Candelabra.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageCandelabraAsset->spawnPoints = stageCandelabraAsset->model->spawnPoints;

            stageBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/Brazier/Brazier.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageBrazierAsset->spawnPoints = stageBrazierAsset->model->spawnPoints;

            stageGroundBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/GroundBrazier/groundBrazier.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageGroundBrazierAsset->spawnPoints = stageGroundBrazierAsset->model->spawnPoints;

            stageMeltedWaxAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/MeltedWax/MeltedWax.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageMeltedWaxAsset->spawnPoints = stageMeltedWaxAsset->model->spawnPoints;

            stageStandingBrazierAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/StandingBrazier/StandingBrazier.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageStandingBrazierAsset->spawnPoints = stageStandingBrazierAsset->model->spawnPoints;

            stageCandleStandAsset->model = std::make_shared<InterleavedGltfModel>(device, "./Data/Models/DarkStageAssets/CandleStand/CandleStand.gltf", ModelTypes::ModelMode::InstancedStaticMesh, false, true);
            stageCandleStandAsset->spawnPoints = stageCandleStandAsset->model->spawnPoints;

        });

    // ライトの方向と色を設定
    lightDirection = { 0.722f, -0.38f, -0.0211f, 0.9f };   // 上の窓からの光
    lightColor = { 1.0f, 0.8f, 1.0f, 2.6f };
    {
        //PROFILE_SCOPE("SceneBase Init");
        SceneBase::Initialize(device, width, height, props);
    }
    {
        //PROFILE_SCOPE("Physics Init");
        Physics::Instance().Initialize();
    }
    {
        PROFILE_SCOPE("SetUpActors Init");
        //アクターをセット
        SetUpActors();
    }

    // ここで軌跡を描画する
    RegisterRenderHook(RenderPass::ForwardBlend, [&](ID3D11DeviceContext* immediateContext)
        {
            RenderState::BindBlendState(immediateContext, BLEND_STATE::ADD);
            player->RenderTrail(immediateContext);
        });

    return true;
}

void TitleScene::Start()
{
    // ゲームBGM
    gameBgmActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<BgmActor>("GameBgmActor");
    gameBgmActor->SetSource(L"./Data/Sound/BGM/title_bgm.wav");
    gameBgmActor->SetLoop(true);
    gameBgmActor->SetBgm(true);
    gameBgmActor->Play();
    gameBgmActor->SetVolume(0.25f);

    std::string parentName = gameBgmActor->GetRootComponentName();
    auto seComponent=gameBgmActor->AddComponent<AudioSourceComponent>("bonfireSeComponent", parentName);
    seComponent->SetSource(L"./Data/Sound/SE/bonfire_se.wav");
    seComponent->SetLoop(true);
    seComponent->SetBgm(true);
    seComponent->SetVolume(1.5f);
    seComponent->Play();


    cameraManager->ToggleCinematicCamera(this);
    SceneEditor::LoadPresetList(); // 更新
    std::string file = "Title.json";
    static SceneState savedState;
    SceneEditor::LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
    savedState.Apply(Scene::GetCurrentScene());
    // カメラを固定する
    cinemaCameraActor->SetUseDebugMode(false);

    // タイトルの画像を作成
    std::shared_ptr<UIImageComponent> title = std::make_shared<UIImageComponent>("./Data/Textures/UI/title_logo1.png", "title");
    title->SetWorldPosition({ 790, 120 });
    //title->SetWorldPosition({ 835, 180 });
    title->SetScale({ 0.8f,0.8f });
    title->SetSize({ 1300, 700 });
    uiManager->Add(title);

    startGameButton=std::make_shared<UIButtonComponent>("./Data/Textures/UI/start_game.png","TitleStartGame");
    startGameButton->SetWorldPosition(startGamePosition); startGameButton->SetSize({514,73}); startGameButton->SetScale({startGameBaseScale,startGameBaseScale}); startGameButton->SetPivot({.5f,.5f}); startGameButton->zOrder=10;
    startGameButton->SetVisualColors(CoreColor(.8f,.8f,.8f,1), CoreColor::White, CoreColor::White, CoreColor::White);
    startGameButton->onClick=[this](){CoreAudio::PlayOneShot(Push);SceneTransitionManager::Instance().RequestTransition("LoadingScene", { std::make_pair("preload", "GameScene") }, TransitionStyle::Fade);}; uiManager->Add(startGameButton);uiManager->AddButton(startGameButton);
    quitGameButton=std::make_shared<UIButtonComponent>("./Data/Textures/UI/Pause/quit_game.png","TitleQuitGame");
    quitGameButton->SetWorldPosition(quitGamePosition); quitGameButton->SetSize({472,92}); quitGameButton->SetScale({quitGameBaseScale,quitGameBaseScale}); quitGameButton->SetPivot({.5f,.5f}); quitGameButton->zOrder=10;
    quitGameButton->SetVisualColors(CoreColor(.8f,.8f,.8f,1), CoreColor::White, CoreColor::White, CoreColor::White);
    quitGameButton->onClick=[](){CoreAudio::PlayOneShot(Push);Framework::RequestExit();};uiManager->Add(quitGameButton);uiManager->AddButton(quitGameButton);
    selectionLineLeft=std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/select_button_line.png","TitleSelectLineLeft"); selectionLineRight=std::make_shared<UIImageComponent>("./Data/Textures/UI/Result/select_button_line.png","TitleSelectLineRight");
    for(const auto& l:{selectionLineLeft,selectionLineRight}){l->SetSize({113,5});l->SetEnable(false);l->zOrder=11;uiManager->Add(l);}selectionLineLeft->SetPivot({1.0f,.5f});selectionLineRight->SetPivot({0.0f,.5f});uiManager->SetNavigationEnabled(false);SetTitleMenuSelection(TitleMenuSelection::StartGame,false);
    // シーンが切り替わった時に
    SceneTransitionManager::Instance().NotifySceneChanged();
}

void TitleScene::Update(float deltaTime)
{
    using namespace DirectX;

    // ライトのビューの焦点をプレイヤー位置に設定する (シャドウマップ用)
    if (player)
    {
        SetLightViewFocus(player->GetPosition());
    }

    SceneBase::Update(deltaTime);

    Physics::Instance().Update(Time::UnscaledDeltaTime());
    CollisionSystem::DetectAndResolveCollisions();
    CollisionSystem::ApplyPushAll();

    HandleTitleMenuInput();
    UpdateTitleMenuAnimations(deltaTime);
    UpdateTitleSelectionLines();
}

void TitleScene::HandleTitleMenuInput()
{
    const bool up = InputSystem::GetInputState("UIUp", InputStateMask::Trigger);
    const bool down = InputSystem::GetInputState("UIDown", InputStateMask::Trigger);
    const float stickY = InputSystem::GetLeftStick().y;
    if (std::abs(stickY) < .4f) titleMenuStickArmed = true;
    if (up || down || (titleMenuStickArmed && (stickY > .6f || stickY < -.6f)))
    {
        if (!up && !down) titleMenuStickArmed = false;
        SetTitleMenuSelection(titleMenuSelection == TitleMenuSelection::StartGame
            ? TitleMenuSelection::QuitGame : TitleMenuSelection::StartGame, true);
    }
    if (InputSystem::GetInputState("GamePadA", InputStateMask::Trigger))
    {
        const auto& button = titleMenuSelection == TitleMenuSelection::StartGame ? startGameButton : quitGameButton;
        if (button) button->OnClick();
    }
}

void TitleScene::SetTitleMenuSelection(const TitleMenuSelection selection, const bool playSound)
{
    if (titleMenuSelection == selection && uiManager->GetSelectedButton()) return;

    const bool initialSelection = uiManager->GetSelectedButton() == nullptr;
    startGameScaleAnimationStart = startGameSelectionScale;
    quitGameScaleAnimationStart = quitGameSelectionScale;
    titleMenuSelection = selection;
    const float startTarget = selection == TitleMenuSelection::StartGame ? selectedScale : unselectedScale;
    const float quitTarget = selection == TitleMenuSelection::QuitGame ? selectedScale : unselectedScale;
    if (initialSelection)
    {
        startGameSelectionScale = startTarget;
        quitGameSelectionScale = quitTarget;
        titleMenuScaleAnimationElapsed = scaleAnimationDuration;
    }
    else
    {
        titleMenuScaleAnimationElapsed = 0.0f;
    }
    titleMenuLineAnimationElapsed = 0.0f;
    uiManager->SetSelected((selection == TitleMenuSelection::StartGame ? startGameButton : quitGameButton).get());
    if (playSound) CoreAudio::PlayOneShot(Move);
}

void TitleScene::UpdateTitleMenuAnimations(const float deltaTime)
{
    const float scaleDuration = (std::max)(scaleAnimationDuration, FLT_EPSILON);
    titleMenuScaleAnimationElapsed = (std::min)(scaleDuration, titleMenuScaleAnimationElapsed + (std::max)(0.0f, deltaTime));
    const float scaleT = titleMenuScaleAnimationElapsed / scaleDuration;
    const float scaleEase = 1.0f - std::pow(1.0f - scaleT, 3.0f);
    const float startTarget = titleMenuSelection == TitleMenuSelection::StartGame ? selectedScale : unselectedScale;
    const float quitTarget = titleMenuSelection == TitleMenuSelection::QuitGame ? selectedScale : unselectedScale;
    startGameSelectionScale = std::lerp(startGameScaleAnimationStart, startTarget, scaleEase);
    quitGameSelectionScale = std::lerp(quitGameScaleAnimationStart, quitTarget, scaleEase);

    startGameButton->SetWorldPosition(startGamePosition);
    quitGameButton->SetWorldPosition(quitGamePosition);
    startGameButton->SetScale({ startGameBaseScale * startGameSelectionScale, startGameBaseScale * startGameSelectionScale });
    quitGameButton->SetScale({ quitGameBaseScale * quitGameSelectionScale, quitGameBaseScale * quitGameSelectionScale });
}

void TitleScene::UpdateTitleSelectionLines()
{
    const auto& button = titleMenuSelection == TitleMenuSelection::StartGame ? startGameButton : quitGameButton;
    const float buttonScale = titleMenuSelection == TitleMenuSelection::StartGame
        ? startGameBaseScale * startGameSelectionScale : quitGameBaseScale * quitGameSelectionScale;
    if (!button || !selectionLineLeft || !selectionLineRight) return;

    const float lineDuration = (std::max)(lineAnimationDuration, FLT_EPSILON);
    titleMenuLineAnimationElapsed = (std::min)(lineDuration, titleMenuLineAnimationElapsed + Time::UnscaledDeltaTime());
    const float t = titleMenuLineAnimationElapsed / lineDuration;
    const float ease = 1.0f - std::pow(1.0f - t, 3.0f);
    const auto position = button->GetWorldPosition();
    const float innerEdge = button->GetSize().x * buttonScale * .5f + lineOffset.x;
    selectionLineLeft->SetWorldPosition({ position.x - innerEdge, position.y + lineOffset.y });
    selectionLineRight->SetWorldPosition({ position.x + innerEdge, position.y + lineOffset.y });
    selectionLineLeft->SetScale({ lineBaseScale.x * ease, lineBaseScale.y });
    selectionLineRight->SetScale({ lineBaseScale.x * ease, lineBaseScale.y });
    selectionLineLeft->SetVisible(true);
    selectionLineRight->SetVisible(true);
}
void TitleScene::SetUpActors()
{
    Transform mainCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    mainCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<MainCamera>("mainCameraActor", mainCameraTr);
    mainCameraComponent = mainCameraActor->GetComponent<TPSCameraComponent>();
    {
        PROFILE_SCOPE("Create Player");
        Transform playerTr(DirectX::XMFLOAT3{ -15.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,126.0f,10.0f }, DirectX::XMFLOAT3{ 1.07f,1.07f,1.07f });
        player = this->GetActorManager()->CreateAndRegisterActorWithTransform<TitlePlayer>("Player", playerTr);
        mainCameraActor->SetLookTarget(player->GetRootComponent());
        mainCameraActor->SetEye(player->GetRootComponent());
    }
    mainCameraComponent->SetPitch(-20.0f);
    mainCameraComponent->distance = 5.35f;
    mainCameraActor->tpsController.useRaycast = false;
    SetActiveCamera(mainCameraActor);
    Logger::Log(U8("sampleシーンのカメラ設定される。"));

    Transform debugCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto debugCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DebugCamera>("debugCam", debugCameraTr);
    cameraManager->SetDebugCamera(debugCameraActor);

    Transform cinemaCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    cinemaCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<CinemaCamera>("cinemaCam", cinemaCameraTr);
    cameraManager->SetCinematicCamera(cinemaCameraActor);

    Transform movieCameraTr(DirectX::XMFLOAT3{ -0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto movieCameraActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<MovieCamera>("movieCam", movieCameraTr);
    cameraManager->SetMovieCamera(movieCameraActor);

    loadStageThread.join();
    loadStageAssetsThread.join();
    {
        PROFILE_SCOPE("Create Stage");
        Transform stageTr(DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT4{ 0.0f,0.0f,0.0f,1.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
        auto stage = this->GetActorManager()->CreateAndRegisterActorWithTransform<DarkTitleStage>("stage", stageTr);
        stage->SetModel(stageAsset, stageCandelabraAsset, stageBrazierAsset, stageGroundBrazierAsset, stageMeltedWaxAsset, stageStandingBrazierAsset, stageCandleStandAsset);
    }

    for (auto point : stageAsset->spawnPoints)
    {
        if (point.name.rfind("Spawn_SmallDoor", 0) == 0)
        {
            DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
            Transform smallDoorTr{ pos,point.worldRotation,point.worldScale };
            auto smallDoorActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<DoorSmallActor>("smallDoorActor", smallDoorTr);
        }
    }

    // ランタンを生成する
    Transform lanternTr(DirectX::XMFLOAT3{ -25.0f,0.0f,12.0f }, DirectX::XMFLOAT3{ 0.0f,0.0f,0.0f }, DirectX::XMFLOAT3{ 1.0f,1.0f,1.0f });
    auto lanternActor = this->GetActorManager()->CreateAndRegisterActorWithTransform<LanternActor>("LanternActor", lanternTr);

}

bool TitleScene::Uninitialize(ID3D11Device* device)
{
    uiManager->SetNavigationEnabled(true); uiManager->SetSelected(nullptr);
    SceneBase::Uninitialize(device);
    Physics::Instance().Finalize();
    return true;
}

void TitleScene::DrawGui()
{
    SceneBase::DrawGui();
#ifdef USE_IMGUI
#if 0
    if (ImGui::Begin("Title Menu UI"))
    {
        ImGui::SeparatorText("START GAME");
        ImGui::DragFloat("Position X##Start", &startGamePosition.x, 1.0f);
        ImGui::DragFloat("Position Y##Start", &startGamePosition.y, 1.0f);
        ImGui::DragFloat("Base Scale##Start", &startGameBaseScale, 0.01f, 0.1f, 3.0f);
        ImGui::SeparatorText("QUIT GAME");
        ImGui::DragFloat("Position X##Quit", &quitGamePosition.x, 1.0f);
        ImGui::DragFloat("Position Y##Quit", &quitGamePosition.y, 1.0f);
        ImGui::DragFloat("Base Scale##Quit", &quitGameBaseScale, 0.01f, 0.1f, 3.0f);
        ImGui::SeparatorText("Selection Animation");
        ImGui::DragFloat("Selected Scale", &selectedScale, 0.01f, 0.1f, 3.0f);
        ImGui::DragFloat("Unselected Scale", &unselectedScale, 0.01f, 0.1f, 3.0f);
        ImGui::DragFloat("Scale Animation Duration", &scaleAnimationDuration, 0.01f, 0.01f, 2.0f);
        ImGui::DragFloat("Line Animation Duration", &lineAnimationDuration, 0.01f, 0.01f, 2.0f);
        ImGui::SeparatorText("Selection Lines");
        ImGui::DragFloat("Line Offset X", &lineOffset.x, 1.0f);
        ImGui::DragFloat("Line Offset Y", &lineOffset.y, 1.0f);
        ImGui::DragFloat("Line Base Scale X", &lineBaseScale.x, 0.01f, 0.0f, 3.0f);
        ImGui::DragFloat("Line Base Scale Y", &lineBaseScale.y, 0.01f, 0.0f, 3.0f);
    }
    ImGui::End();

#endif // 0
#endif
}
