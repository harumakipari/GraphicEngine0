#include "pch.h"
#include  "DarkStage.h"

#include "DarkStageBarrelActor.h"
#include "DarkStageBrazierActor.h"
#include "DarkStageCandelabraActor.h"
#include "DarkStageChandelierActor.h"
#include "DarkStageGroundBrazierActor.h"
#include "DarkStagePointLightActor.h"
#include "DoorActor.h"
#include "Components/Effect/ParticleComponent.h"
#include "Game/Actors/Player/Player.h"
#include "Engine/Camera/CameraManager.h"
#include "Engine/Scene/Scene.h"

void DarkStage::Initialize(const Transform& transform)
{
    //影用のスタティックメッシュコンポーネントを追加
    std::shared_ptr<StaticMeshComponent> castStaticMeshComponent = this->AddComponent<class StaticMeshComponent>("castShadowModel", parentName);
    castStaticMeshComponent->SetModel("./Data/Models/DarkStageShadowModel/DarkStageShadowModel.glb", false, true);
    castStaticMeshComponent->SetIsOnlyShadow(true);
    castStaticMeshComponent->SetIsVisible(false);
    

#if 1
    {
        PROFILE_SCOPE("Create StageCollision");
        auto stageCollisionModel = this->AddComponent<StaticMeshComponent>("collisionModel", parentName);
        //stageCollisionModel->SetModel("./Data/Models/DarkStage_Collision/DarkStage_CollisionModel_0622.glb", true, false);
        stageCollisionModel->SetModel("./Data/Models/DarkStage_Collision/DarkStage_CollisionModel_0711.glb", true, false);
        //stageCollisionModel->SetModel("./Data/Models/DarkStage_Collision/DarkStage_Collision.glb", true, true);
        stageCollisionModel->SetIsCastShadow(false);
        stageCollisionModel->SetIsVisible(false);
        auto nodes = stageCollisionModel->model->GetNodes();
        for (auto node : nodes)
        {
            DirectX::XMVECTOR S, R, T;

            bool ok = DirectX::XMMatrixDecompose(
                &S,
                &R,
                &T,
                DirectX::XMLoadFloat4x4(&node.globalTransform)
            );

            DirectX::XMFLOAT3 worldScale;
            DirectX::XMFLOAT4 worldRotation;
            DirectX::XMFLOAT3 worldPosition;

            if (ok)
            {
                XMStoreFloat3(&worldScale, S);
                XMStoreFloat4(&worldRotation, R);
                XMStoreFloat3(&worldPosition, T);
            }
            auto box = AddComponent<BoxComponent>(node.name, parentName);

            DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(worldPosition);

            box->SetHalfBoxExtent(worldScale);
            box->SetRelativeLocationDirect(pos);
            box->SetRelativeRotationDirect(worldRotation);

            box->SetStatic(true);
            box->SetLayer(CollisionLayer::WorldStatic);
            box->SetResponseToLayer(
                CollisionLayer::Player,
                CollisionComponent::CollisionResponse::Block);

            box->Initialize();
        }
    }
#endif // 0
    {
        PROFILE_SCOPE("Create FloorCollision");
        std::shared_ptr<StaticMeshComponent> floorCollisionModel = this->AddComponent<StaticMeshComponent>("floorCollisionModel", parentName);
        floorCollisionModel->SetModel("./Data/Models/DarkStage_Collision/DarkStage_CollisionFloor.glb", true, true);
        floorCollisionModel->SetIsVisible(false);

        std::shared_ptr<TriangleMeshCollisionComponent> triangleMeshComponent = this->AddComponent<class TriangleMeshCollisionComponent>("triangleMeshComponent", "floorCollisionModel");
        triangleMeshComponent->CreateConvexMeshFromModel(floorCollisionModel.get());

#if 1
        // 床の当たり判定用のボックスコリジョンコンポーネント
        std::shared_ptr<BoxComponent> boxComponent = this->AddComponent<class BoxComponent>("boxComponent", parentName);
        boxComponent->SetHalfBoxExtent(DirectX::XMFLOAT3(80.0f, 0.2f, 80.0f));
        boxComponent->SetRelativeLocationDirect({ 0.0f,-0.3f,0.0f });
        //boxComponent->SetCollisionOffsetY(-4.5f);
        boxComponent->SetStatic(true);
        boxComponent->SetLayer(CollisionLayer::WorldStatic);
        boxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        boxComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
        boxComponent->Initialize();

#endif // 0
    }
}

void DarkStage::Update(float deltaTime)
{
    UpdateAutomaticStageState();

    if (!bossRoomSequencePlaying)
        return;
#if 0
    bossRoomSequenceTime += deltaTime;

    const float interval = 0.3f;

#endif // 0
    for (size_t i = 0; i < bossRoomLightsLeft.size(); i++)
    {
        //if (bossRoomSequenceTime > i * interval)
        {
            bossRoomLightsLeft[i]->SetSharedLightName("WallLight");
        }
    }

    //SetStageArea(StageArea::BossRoom);
    //SetStageArea(StageArea::MainRoom);

}

void DarkStage::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::SeparatorText("Stage Geometry Visibility");
    const char* currentStageState = isMoviePlaying
        ? "Movie: All Areas"
        : (currentStageArea == StageArea::BossRoom
            ? "Gameplay: Transition + BossRoom"
            : "Gameplay: MainRoom + Transition");
    ImGui::Text("Is Movie Playing: %s", isMoviePlaying ? "true" : "false");
    ImGui::Text("BossRoom Entered: %s", isBossRoomEntered ? "true" : "false");
    ImGui::Text("Current Stage State: %s", currentStageState);
    if (ImGui::Button("Show MainRoom"))
        SetStageArea(StageArea::MainRoom);
    ImGui::SameLine();
    if (ImGui::Button("Show BossRoom"))
        SetStageArea(StageArea::BossRoom);
    ImGui::Text("MainRoom: %s", mainRoomMeshComponent && mainRoomMeshComponent->IsVisible() ? "Visible" : "Hidden");
    ImGui::Text("TransitionArea: %s", transitionAreaMeshComponent && transitionAreaMeshComponent->IsVisible() ? "Visible" : "Hidden");
    ImGui::Text("BossRoom: %s", bossRoomMeshComponent && bossRoomMeshComponent->IsVisible() ? "Visible" : "Hidden");
    const auto liveLightCount = [this](StageArea area)
    {
        size_t count = 0;
        for (const auto& light : stageLightsByArea[static_cast<size_t>(area)])
            count += light.expired() ? 0 : 1;
        return count;
    };
    size_t enabledStageLightCount = 0;
    for (const auto& areaLights : stageLightsByArea)
        for (const auto& light : areaLights)
            if (const auto component = light.lock(); component && component->IsUsePointLight())
                ++enabledStageLightCount;
    ImGui::Text("MainRoom Light Count: %zu", liveLightCount(StageArea::MainRoom));
    ImGui::Text("Transition Light Count: %zu", liveLightCount(StageArea::Transition));
    ImGui::Text("BossRoom Light Count: %zu", liveLightCount(StageArea::BossRoom));
    ImGui::Text("Current Enabled Stage Light Count: %zu", enabledStageLightCount);
    ImGui::Separator();
    if (ImGui::Button(U8("ボスの部屋に入る")))
    {
        StartBossRoomLightSequence();
    }
#endif 
}

// ボスの部屋に入った時の処理
void DarkStage::StartBossRoomLightSequence()
{
    bossRoomSequencePlaying = true;
    bossRoomSequenceTime = 0.0f;
}

void DarkStage::SetStageArea(StageArea area)
{
    if (area == StageArea::Transition)
        return;

    currentStageArea = area;
    ApplyStageVisibility();
    ApplyStageLightEnable();
}

bool DarkStage::IsStageAreaVisible(StageArea area) const
{
    if (isMoviePlaying)
        return true;
    return area == StageArea::Transition || area == currentStageArea;
}

void DarkStage::UpdateAutomaticStageState()
{
    const auto scene = GetOwnerScene();
    if (!scene)
        return;

    const auto cameraManager = scene->GetCameraManager();
    const bool moviePlaying = cameraManager && cameraManager->IsUseMovie();

    bool bossRoomEntered = false;
    if (const auto player = scene->GetActorManager()->GetActorOfType<Player>())
        bossRoomEntered = player->IsBossBattle();

    if (automaticStageStateInitialized &&
        moviePlaying == isMoviePlaying &&
        bossRoomEntered == isBossRoomEntered)
    {
        return;
    }

    automaticStageStateInitialized = true;
    isMoviePlaying = moviePlaying;
    isBossRoomEntered = bossRoomEntered;
    if (!isMoviePlaying)
    {
        currentStageArea = isBossRoomEntered
            ? StageArea::BossRoom
            : StageArea::MainRoom;
    }

    ApplyStageVisibility();
    ApplyStageLightEnable();
}

void DarkStage::RegisterStageLight(
    StageArea area, const std::shared_ptr<PointLightComponent>& light)
{
    if (!light)
        return;
    stageLightsByArea[static_cast<size_t>(area)].push_back(light);
    light->SetUsePointLight(IsStageAreaVisible(area));
}

void DarkStage::RegisterActorStageLights(
    StageArea area, const std::shared_ptr<Actor>& actor)
{
    if (!actor)
        return;
    for (const auto& component : actor->GetComponents())
    {
        if (const auto light = std::dynamic_pointer_cast<PointLightComponent>(component))
            RegisterStageLight(area, light);
    }
}

void DarkStage::ApplyStageVisibility()
{
    if (mainRoomMeshComponent)
        mainRoomMeshComponent->SetIsVisible(IsStageAreaVisible(StageArea::MainRoom));
    if (transitionAreaMeshComponent)
        transitionAreaMeshComponent->SetIsVisible(IsStageAreaVisible(StageArea::Transition));
    if (bossRoomMeshComponent)
        bossRoomMeshComponent->SetIsVisible(IsStageAreaVisible(StageArea::BossRoom));

}

void DarkStage::ApplyStageLightEnable()
{
    for (size_t areaIndex = 0; areaIndex < stageLightsByArea.size(); ++areaIndex)
    {
        const StageArea area = static_cast<StageArea>(areaIndex);
        const bool enabled = IsStageAreaVisible(area);
        for (const auto& stageLight : stageLightsByArea[areaIndex])
        {
            if (const auto light = stageLight.lock())
                light->SetUsePointLight(enabled);
        }
    }
}

void DarkStage::SetModel(std::shared_ptr<StageAsset> mainRoomAsset, std::shared_ptr<StageAsset> transitionAreaAsset, std::shared_ptr<StageAsset> bossRoomAsset, std::shared_ptr<StageAsset> stageCandelabraAsset, std::shared_ptr<StageAsset> stageBrazierAsset, std::shared_ptr<StageAsset> stageGroundBrazierAsset, std::shared_ptr<StageAsset> stageMeltedWaxAsset, std::shared_ptr<StageAsset> stageStandingBrazierAsset, std::shared_ptr<StageAsset> stageCandleStandAsset)
{
    auto scene = GetOwnerScene();


    {
        PROFILE_SCOPE("Create StageModels");
        const auto createStageMesh = [this](
            const char* componentName, const std::shared_ptr<StageAsset>& asset)
        {
            auto meshComponent = this->AddComponent<class StaticMeshComponent>(
                componentName, parentName);
            meshComponent->model = asset->model;
            meshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Stage;
            meshComponent->SetIsCastShadow(false);
            return meshComponent;
        };

        mainRoomMeshComponent = createStageMesh("MainRoomModel", mainRoomAsset);
        transitionAreaMeshComponent = createStageMesh("TransitionAreaModel", transitionAreaAsset);
        bossRoomMeshComponent = createStageMesh("BossRoomModel", bossRoomAsset);
        ApplyStageVisibility();
    }

    {
        PROFILE_SCOPE("Create StageActor");

        struct StageSpawnSource
        {
            StageArea area;
            std::shared_ptr<StageAsset> asset;
        };
        const StageSpawnSource areaAssets[] =
        {
            { StageArea::MainRoom, mainRoomAsset },
            { StageArea::Transition, transitionAreaAsset },
            { StageArea::BossRoom, bossRoomAsset },
        };
        for (const auto& [spawnArea, areaAsset] : areaAssets)
        {
            for (const auto& point : areaAsset->spawnPoints)
        {
            const auto registerActorLights = [this, spawnArea](const std::shared_ptr<Actor>& actor)
            {
                RegisterActorStageLights(spawnArea, actor);
            };
            if (point.name.rfind("Spawn_Particle_Steam", 0) == 0)
            {
                // 湯気のエフェクト
                auto steamComponent = this->AddComponent<ParticleComponent>("steamComponent", parentName);
                steamComponent->Load("./Data/Effect/Files/Pot_SteamEffect.json");
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                steamComponent->SetRelativeLocationDirect(pos);
                steamComponent->Play();
            }
            else if (point.name.rfind("Spawn_FireEffect", 0) == 0)
            {
                // 炎のエフェクト
                auto frameEffect = this->AddComponent<ParticleComponent>("FireFrameEffect", parentName);
                frameEffect->Load("./Data/Effect/Files/DarkStageFrameEffect.json");
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                pos.y = 2.8f;
                frameEffect->SetRelativeLocationDirect(pos);
                frameEffect->Play();
                // ポイントライトも一緒に配置する
                auto pointLightComponent = this->AddComponent<PointLightComponent>("pointLightComponent", parentName);
                pointLightComponent->SetRelativeLocationDirect(pos);
                // ライトの名前からライトマネージャーの共有ライトを取得して設定
                pointLightComponent->SetSharedLightName("FireBowl");
                RegisterStageLight(spawnArea, pointLightComponent);
            }
            else if (point.name.rfind("Spawn_BossRoomChandelier", 0) == 0)
            {// bossの部屋のシャンデリアを生成する
                Transform chandelierTr{ {17.221f,13.996f,11.082f},point.worldRotation,{3.71f,3.51f,5.1f} };
                auto chandelier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageChandelierActor>("bossRoomChandelier", chandelierTr);
                registerActorLights(chandelier);
            }
            else if (point.name.rfind("Spawn_Chandelier", 0) == 0)
            {// 名前が "Spawn_Chandelier" で始まる場合、シャンデリアを配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                static int count = 0;
                Logger::Log("chandelier spawn count :" + std::to_string(count));
                count++;
                Transform chandelierTr{ pos,
                    point.worldRotation,
                    point.worldScale
                };
                auto chandelier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageChandelierActor>("chandelier", chandelierTr);
                registerActorLights(chandelier);
            }
            else if (point.name.rfind("Spawn_MainChandelier", 0) == 0)
            {// メインの部屋のシャンデリアを生成する
                Transform chandelierTr{ {-13.28f,13.266f,11.182f},point.worldRotation,{2.5f,2.5f,2.5f} };
                auto chandelier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageChandelierActor>("MainChandelier", chandelierTr);
                registerActorLights(chandelier);
            }
            else if (point.name.rfind("Spawn_TorchSconce", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{ pos,point.worldRotation,point.worldScale };
                auto candelabra = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageTorchSconceActor>("TorchSconce", candelabraTr);
                registerActorLights(candelabra);
            }
            else if (point.name.rfind("Spawn_CandleStand", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candleStandTr{ pos,point.worldRotation,{1.0f,1.0f,1.0f} };
                auto candleStand = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageCandleStandActor>("candleStand", candleStandTr);
                candleStand->SetModel(stageCandleStandAsset);
                registerActorLights(candleStand);
            }
            else if (point.name.rfind("Spawn_Candelabra", 0) == 0)
            {// 名前が "Spawn_Candelabra" で始まる場合、燭台を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{ pos,point.worldRotation,point.worldScale };
                auto candelabra = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageCandelabraActor>("candelabra", candelabraTr);
                candelabra->SetModel(stageCandelabraAsset);
                registerActorLights(candelabra);
            }
            else if (point.name.rfind("Spawn_Brazier", 0) == 0)
            {// 名前が "Spawn_Brazier" で始まる場合、火鉢を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform brazierTr{ pos,point.worldRotation,point.worldScale };
                auto brazier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageBrazierActor>("brazier", brazierTr);
                brazier->SetModel(stageBrazierAsset);
                registerActorLights(brazier);
            }
            else if (point.name.rfind("Spawn_GroundBrazier", 0) == 0)
            {// 名前が "Spawn_GroundBrazier" で始まる場合、地面の火鉢を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{ pos,point.worldRotation,point.worldScale };
                auto groundBrazier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageGroundBrazierActor>("GroundBrazier", candelabraTr);
                groundBrazier->SetModel(stageGroundBrazierAsset);
                registerActorLights(groundBrazier);
            }
            else if (point.name.rfind("Spawn_Melted_Wax", 0) == 0)
            {// 名前が "Spawn_Melted_Wax" で始まる場合、溶けた蝋を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{ pos,point.worldRotation,point.worldScale };
                auto meltedWax = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageMeltedWaxActor>("MeltedWax", candelabraTr);
                meltedWax->SetModel(stageMeltedWaxAsset);
                registerActorLights(meltedWax);
            }
            else if (point.name.rfind("Spawn_Standing_Brazier", 0) == 0)
            {// 名前が "Spawn_Standing_Brazier" で始まる場合、スタンド式火鉢を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{ pos,{0,0,0,1},point.worldScale };
                auto standingBrazier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageStandingBrazierActor>("StandingBrazier", candelabraTr);
                standingBrazier->SetModel(stageStandingBrazierAsset);
                registerActorLights(standingBrazier);
            }
            else if (point.name.rfind("Spawn_JailDoor", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                pos.z = 12.0f;
                Transform doorJailTr{ pos,point.worldRotation,point.worldScale };
                auto doorJailActor = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DoorJailActor>("DoorJailActor", doorJailTr);
                registerActorLights(doorJailActor);

            }
#if 0
            else if (point.name.rfind("Spawn_Barrel", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform barrelTr{ pos,point.worldRotation,point.worldScale };
                auto barrel = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageBarrelActor>("barrel", barrelTr);
                registerActorLights(barrel);
            }

#endif // 0
            else if (point.name.rfind("Spawn_TorchLight", 0) == 0)
            {// ボスの部屋のTorchLightを生成する
                static int i = 0;
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform barrelTr{ pos,point.worldRotation,point.worldScale };
                std::string compName = "TorchLight" + std::to_string(i);
                auto pointLightComponent = this->AddComponent<PointLightComponent>(compName, parentName);
                pointLightComponent->SetRelativeLocationDirect(pos);
                pointLightComponent->SetSharedLightName("TorchLight");
                RegisterStageLight(spawnArea, pointLightComponent);
            }
            else if (point.name.rfind("Spawn_BossRoomLight", 0) == 0)
            {// ボスの部屋のPointLightを生成する
                static int i = 0;
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                std::string compName = "BossRoomLight" + std::to_string(i);
                auto pointLightComponent = this->AddComponent<PointLightComponent>(compName, parentName);
                pointLightComponent->SetRelativeLocationDirect(pos);
                pointLightComponent->SetSharedLightName("BossRoomPointLight");
                //pointLightComponent->SetSharedLightName("ZeroLight");
                bossRoomLightsLeft.push_back(pointLightComponent.get());
                RegisterStageLight(spawnArea, pointLightComponent);
            }
            else if (point.name.rfind("Spawn_WallLight", 0) == 0)
            {// ボスの部屋の壁のPointLightを生成する
                static int i = 0;
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                std::string compName = "WallLight" + std::to_string(i);
                auto pointLightComponent = this->AddComponent<PointLightComponent>(compName, parentName);
                pointLightComponent->SetRelativeLocationDirect(pos);
                pointLightComponent->SetSharedLightName("WallLight");
                //pointLightComponent->SetSharedLightName("ZeroLight");
                bossRoomLightsLeft.push_back(pointLightComponent.get());
                RegisterStageLight(spawnArea, pointLightComponent);
            }
            else if (point.name.rfind("Spawn_MainRoomLight", 0) == 0)
            {// メインの部屋のPointLightを生成する
                static int i = 0;
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                std::string compName = "MainRoomLight" + std::to_string(i);
                auto pointLightComponent = this->AddComponent<PointLightComponent>(compName, parentName);
                pointLightComponent->SetRelativeLocationDirect(pos);
                pointLightComponent->SetSharedLightName("MainRoomPointLight");
                RegisterStageLight(spawnArea, pointLightComponent);
            }

            }
        }
    }


    ApplyStageLightEnable();
    StartBossRoomLightSequence();

}



