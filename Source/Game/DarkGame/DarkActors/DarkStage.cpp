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
#include "Engine/Scene/Scene.h"

void DarkStage::Initialize(const Transform& transform)
{
    // //影用のスタティックメッシュコンポーネントを追加
    //std::shared_ptr<StaticMeshComponent> castStaticMeshComponent = this->AddComponent<class StaticMeshComponent>("castShadowModel", parentName);
    //castStaticMeshComponent->SetModel("./Data/Models/DarkStageShadow/DarkStageShadow.gltf");
    //castStaticMeshComponent->SetIsVisible(false);

#if 1
    {
        PROFILE_SCOPE("Create StageCollision");
        auto stageCollisionModel = this->AddComponent<StaticMeshComponent>("collisionModel", parentName);
        stageCollisionModel->SetModel("./Data/Models/DarkStage_Collision/DarkStage_Collision1.glb", true, true);
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

        // 床の当たり判定用のボックスコリジョンコンポーネント
        std::shared_ptr<BoxComponent> boxComponent = this->AddComponent<class BoxComponent>("boxComponent", parentName);
        boxComponent->SetHalfBoxExtent(DirectX::XMFLOAT3(80.0f, 0.2f, 80.0f));
        boxComponent->SetRelativeLocationDirect({ 0.0f,-0.2f,0.0f });
        //boxComponent->SetCollisionOffsetY(-4.5f);
        boxComponent->SetStatic(true);
        boxComponent->SetLayer(CollisionLayer::Floor);
        boxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        boxComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
        boxComponent->Initialize();
    }





}

void DarkStage::Update(float elapsedTime)
{


}


void DarkStage::SetModel(std::shared_ptr<StageAsset> stageAsset, std::shared_ptr<StageAsset> stageCandelabraAsset, std::shared_ptr<StageAsset> stageBrazierAsset, std::shared_ptr<StageAsset> stageGroundBrazierAsset, std::shared_ptr<StageAsset> stageMeltedWaxAsset, std::shared_ptr<StageAsset> stageStandingBrazierAsset)
{
    auto scene = GetOwnerScene();

    std::shared_ptr<StaticMeshComponent> staticMeshComponent;
    {
        PROFILE_SCOPE("Create StageModel");
        staticMeshComponent = this->AddComponent<class StaticMeshComponent>("model", parentName);
        staticMeshComponent->model = stageAsset->model;
        //staticMeshComponent->SetModel("./Data/Models/DarkStage0302/DarkStage.gltf", true);

        //staticMeshComponent->SetModel("./Data/Models/DarkStage0223_3/DarkStage.gltf", true);
        //staticMeshComponent->SetIsCastShadow(false);
        //staticMeshComponent->model->modelCoordinateSystem = InterleavedGltfModel::CoordinateSystem::LH_Y_UP;
    }
    auto lightsData = staticMeshComponent->model->GetPointLights();

    //for (auto& material : staticMeshComponent->model->materials)
    //{
    //    if (material.name == "M_Aurora_Hair_Blonde_FrozenHearth")
    //    {// 床だったら

    //    }
    //}

    // ポイントライトコンポーネントを追加
    for (int i = 0; i < static_cast<int>(lightsData.size()); ++i)
    {
        const auto& light = lightsData[i];
        //continue; // とりあえずポイントライトは無効化
#if 0
        std::string compName = "pointLightComponent_" + std::to_string(i);
        auto pointLightComponent =
            this->AddComponent<PointLightComponent>(compName, parentName);

        DirectX::XMFLOAT3 pos = convertRHtoLh(light.worldPosition);
        pointLightComponent->SetRelativeLocationDirect(pos);
        pointLightComponent->SetColor(light.color);
        pointLightComponent->SetRange(light.range);
        pointLightComponent->SetIntensity(light.intensity);
#else

#endif // 0

    }


#if 1
    {
        PROFILE_SCOPE("Create StageActor");


        for (auto point : stageAsset->spawnPoints)
        {
            if (point.name == "Spawn_Particle_Steam")
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
            else if (point.name.rfind("Spawn_Chandelier", 0) == 0)
            {// 名前が "Spawn_Chandelier" で始まる場合、シャンデリアを配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);

                Transform chandelierTr{ pos,
                    point.worldRotation,
                    point.worldScale
                };

                auto chandelier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageChandelierActor>("chandelier", chandelierTr);

            }
            else if (point.name.rfind("Spawn_TorchSconce", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
                Transform candelabraTr{
            pos,
                    {0.0f,180.0f,0.0f},
                    point.worldScale
                };

                auto candelabra = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageTorchSconceActor>("TorchSconce", candelabraTr);
            }
            //else if (point.name.rfind("Spawn_FireBowl", 0) == 0)
            //{
            //    DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
            //    Transform candelabraTr{
            //pos,
            //        point.worldRotation,
            //        {1.0f,1.0f,1.0f}
            //    };

            //    auto candelabra = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageFireBowlActor>("FireBowl", candelabraTr);
            //}
            else if (point.name.rfind("Spawn_Candelabra", 0) == 0)
            {// 名前が "Spawn_Candelabra" で始まる場合、燭台を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
#if 1
                Transform candelabraTr{
    pos,
    point.worldRotation,
    point.worldScale
                };

                auto candelabra = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageCandelabraActor>("candelabra", candelabraTr);
                candelabra->SetModel(stageCandelabraAsset);
#else
                std::shared_ptr<StaticMeshComponent> door = AddComponent<StaticMeshComponent>("Candelabra", parentName);
                door->model = stageCandelabraAsset->model;
                door->SetRelativeLocationDirect(pos);
                door->SetRelativeRotationDirect(point.worldRotation);
                door->SetRelativeScaleDirect(point.worldScale);

                stageCandelabraAsset->model->
#endif // 0
            }
            else if (point.name.rfind("Spawn_Brazier", 0) == 0)
            {// 名前が "Spawn_Brazier" で始まる場合、火鉢を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);

#if 1
                Transform brazierTr{
    pos,
    point.worldRotation,
    point.worldScale
                };

                auto brazier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageBrazierActor>("brazier", brazierTr);
                brazier->SetModel(stageBrazierAsset);
#else
                std::shared_ptr<StaticMeshComponent> door = AddComponent<StaticMeshComponent>("Brazier", parentName);
                door->model = stageBrazierAsset->model;
                door->SetRelativeLocationDirect(pos);
                door->SetRelativeRotationDirect(point.worldRotation);
                door->SetRelativeScaleDirect(point.worldScale);
#endif // 0
            }
            else if (point.name.rfind("Spawn_GroundBrazier", 0) == 0)
            {// 名前が "Spawn_GroundBrazier" で始まる場合、地面の火鉢を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
#if 1
                Transform candelabraTr{
                    pos,
                    point.worldRotation,
                    point.worldScale
                };

                auto groundBrazier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageGroundBrazierActor>("GroundBrazier", candelabraTr);
                groundBrazier->SetModel(stageGroundBrazierAsset);
#else
                std::shared_ptr<StaticMeshComponent> door = AddComponent<StaticMeshComponent>("GroundBrazier", parentName);
                door->model = stageGroundBrazierAsset->model;
                door->SetRelativeLocationDirect(pos);
                door->SetRelativeRotationDirect(point.worldRotation);
                door->SetRelativeScaleDirect(point.worldScale);

#endif // 0
            }
#if 0
            else if (point.name.rfind("Spawn_Melted_Wax", 0) == 0)
            {// 名前が "Spawn_Melted_Wax" で始まる場合、溶けた蝋を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
#if 1
                Transform candelabraTr{
    pos,
    {0,0,0,1},
    /*      point.worldRotation,*/
point.worldScale
                };

                auto meltedWax = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageMeltedWaxActor>("MeltedWax", candelabraTr);
                meltedWax->SetModel(stageMeltedWaxAsset);
#else
                std::shared_ptr<StaticMeshComponent> door = AddComponent<StaticMeshComponent>("MeltedWax", parentName);
                door->model = stageMeltedWaxAsset->model;
                door->SetRelativeLocationDirect(pos);
                door->SetRelativeRotationDirect(point.worldRotation);
                door->SetRelativeScaleDirect(point.worldScale);
#endif // 0
                }
#endif // 0

            else if (point.name.rfind("Spawn_Standing_Brazier", 0) == 0)
            {// 名前が "Spawn_Standing_Brazier" で始まる場合、スタンド式火鉢を配置
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);
#if 1
                Transform candelabraTr{
                    pos,
                    {0,0,0,1},
                    point.worldScale
                };

                auto standingBrazier = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageStandingBrazierActor>("StandingBrazier", candelabraTr);
                standingBrazier->SetModel(stageStandingBrazierAsset);
#else
                std::shared_ptr<StaticMeshComponent> door = AddComponent<StaticMeshComponent>("StandingBrazier", parentName);
                door->model = stageStandingBrazierAsset->model;
                door->SetRelativeLocationDirect(pos);
                door->SetRelativeRotationDirect(point.worldRotation);
                door->SetRelativeScaleDirect(point.worldScale);

#endif // 0
            }
            else if (point.name.rfind("Spawn_Barrel", 0) == 0)
            {
                DirectX::XMFLOAT3 pos = MathHelper::ConvertRHtoLh(point.worldPosition);

                Transform barrelTr{
                    pos,
                    point.worldRotation,
                    point.worldScale
                };

                auto barrel = scene->GetActorManager()->CreateAndRegisterActorWithTransform<DarkStageBarrelActor>("barrel", barrelTr);
            }
        }


    }
#endif // 1
}



