#include "pch.h"
#include "DarkTitleStage.h"

#include "DarkStageBarrelActor.h"
#include "DarkStageBrazierActor.h"
#include "DarkStageCandelabraActor.h"
#include "DarkStageChandelierActor.h"
#include "DarkStageGroundBrazierActor.h"
#include "DarkStagePointLightActor.h"
#include "DoorActor.h"
#include "Components/Effect/ParticleComponent.h"
#include "Engine/Scene/Scene.h"

void DarkTitleStage::Initialize(const Transform& transform)
{
    //影用のスタティックメッシュコンポーネントを追加
    std::shared_ptr<StaticMeshComponent> castStaticMeshComponent = this->AddComponent<class StaticMeshComponent>("castShadowModel", parentName);
    castStaticMeshComponent->SetModel("./Data/Models/DarkStageShadowModel/DarkStageShadowModel.glb", false, true);
    castStaticMeshComponent->SetIsOnlyShadow(true);
    castStaticMeshComponent->SetIsVisible(false);

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

void DarkTitleStage::Update(float deltaTime)
{
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

}

void DarkTitleStage::DrawImGuiDetails()
{
}

void DarkTitleStage::SetModel(std::shared_ptr<StageAsset> stageAsset, std::shared_ptr<StageAsset> stageCandelabraAsset, std::shared_ptr<StageAsset> stageBrazierAsset, std::shared_ptr<StageAsset> stageGroundBrazierAsset, std::shared_ptr<StageAsset> stageMeltedWaxAsset, std::shared_ptr<StageAsset> stageStandingBrazierAsset, std::shared_ptr<StageAsset> stageCandleStandAsset)
{
    auto scene = GetOwnerScene();

    std::shared_ptr<StaticMeshComponent> staticMeshComponent;
    {
        PROFILE_SCOPE("Create StageModel");
        staticMeshComponent = this->AddComponent<class StaticMeshComponent>("model", parentName);
        staticMeshComponent->model = stageAsset->model;
        staticMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Stage;
        staticMeshComponent->SetIsCastShadow(false);
    }

    {
        PROFILE_SCOPE("Create StageActor");

        for (auto point : stageAsset->spawnPoints)
        {
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
            }
            else if (point.name.rfind("Spawn_BossRoomChandelier", 0) == 0)
            {// bossの部屋のシャンデリアを生成する
                Transform chandelierTr{ {17.221f,13.996f,11.082f},point.worldRotation,{3.71f,3.51f,5.1f} };
                auto chandelier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageChandelierActor>("bossRoomChandelier", chandelierTr);
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
            }
            else if (point.name.rfind("Spawn_MainChandelier", 0) == 0)
            {// メインの部屋のシャンデリアを生成する
                Transform chandelierTr{ {-13.28f,13.266f,11.182f},point.worldRotation,{2.5f,2.5f,2.5f} };
                auto chandelier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageChandelierActor>("MainChandelier", chandelierTr);
            }
            else if (point.name.rfind("Spawn_TorchSconce", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{ pos,point.worldRotation,point.worldScale };
                auto candelabra = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageTorchSconceActor>("TorchSconce", candelabraTr);
            }
            else if (point.name.rfind("Spawn_CandleStand", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candleStandTr{ pos,point.worldRotation,{1.0f,1.0f,1.0f} };
                auto candleStand = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageCandleStandActor>("candleStand", candleStandTr);
                candleStand->SetModel(stageCandleStandAsset);
            }
            else if (point.name.rfind("Spawn_Candelabra", 0) == 0)
            {// 名前が "Spawn_Candelabra" で始まる場合、燭台を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{ pos,point.worldRotation,point.worldScale };
                auto candelabra = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageCandelabraActor>("candelabra", candelabraTr);
                candelabra->SetModel(stageCandelabraAsset);
            }
            else if (point.name.rfind("Spawn_Brazier", 0) == 0)
            {// 名前が "Spawn_Brazier" で始まる場合、火鉢を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform brazierTr{ pos,point.worldRotation,point.worldScale };
                auto brazier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageBrazierActor>("brazier", brazierTr);
                brazier->SetModel(stageBrazierAsset);
            }
            else if (point.name.rfind("Spawn_GroundBrazier", 0) == 0)
            {// 名前が "Spawn_GroundBrazier" で始まる場合、地面の火鉢を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{ pos,point.worldRotation,point.worldScale };
                auto groundBrazier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageGroundBrazierActor>("GroundBrazier", candelabraTr);
                groundBrazier->SetModel(stageGroundBrazierAsset);
            }
            else if (point.name.rfind("Spawn_Melted_Wax", 0) == 0)
            {// 名前が "Spawn_Melted_Wax" で始まる場合、溶けた蝋を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{ pos,point.worldRotation,point.worldScale };
                auto meltedWax = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageMeltedWaxActor>("MeltedWax", candelabraTr);
                meltedWax->SetModel(stageMeltedWaxAsset);
            }
            else if (point.name.rfind("Spawn_Standing_Brazier", 0) == 0)
            {// 名前が "Spawn_Standing_Brazier" で始まる場合、スタンド式火鉢を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{ pos,{0,0,0,1},point.worldScale };
                auto standingBrazier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageStandingBrazierActor>("StandingBrazier", candelabraTr);
                standingBrazier->SetModel(stageStandingBrazierAsset);
            }
            else if (point.name.rfind("Spawn_JailDoor", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                pos.z = 12.0f;
                Transform doorJailTr{ pos,point.worldRotation,point.worldScale };
                auto doorJailActor = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DoorJailActor>("DoorJailActor", doorJailTr);

            }
#if 0
            else if (point.name.rfind("Spawn_Barrel", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform barrelTr{ pos,point.worldRotation,point.worldScale };
                auto barrel = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageBarrelActor>("barrel", barrelTr);
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
            }
            else if (point.name.rfind("Spawn_MainRoomLight", 0) == 0)
            {// メインの部屋のPointLightを生成する
                static int i = 0;
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                std::string compName = "MainRoomLight" + std::to_string(i);
                auto pointLightComponent = this->AddComponent<PointLightComponent>(compName, parentName);
                pointLightComponent->SetRelativeLocationDirect(pos);
                pointLightComponent->SetSharedLightName("MainRoomPointLight");
            }

        }
    }



}

