#include "pch.h"
#include "TitlePlayer.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include <imgui.h>
#endif

#include "Physics/Physics.h"
#include "Core/ActorManager.h"

#include "Components/Render/PointLightComponent.h"

#include "PlayerStateDerived.h"
#include "Components/Audio/AudioSourceComponent.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Camera/Camera.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/Actors/Stage/Stage.h"
#include "Game/DarkGame/Interactable.h"
#include "Game/DarkGame/DarkActors/InteractableActor.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Physics/CollisionFunction.h"


void TitlePlayer::Initialize(const Transform& transform)
{
    std::string parentName = "skeletalComponent";
    // 描画用コンポーネントを追加
    {
        PROFILE_SCOPE("Create PlayerModel");

        skeletalMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
        skeletalMeshComponent->SetModel("./Data/Models/Characters/PlayerNoWeapon/titlePlayer.gltf", false, true);
        skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Player;   // オブジェクトの種類を Player に設定
        skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 20.9f;   // 自己発光の強さを設定

        skeletalMeshComponent->SetIsCastShadow(false);
        skeletalMeshComponent->SetIsShadowMap(true);
        for (auto& material : skeletalMeshComponent->model->materials)
        {
            if (material.name == "M_Aurora_Hair_Blonde_FrozenHearth")
            {// 髪の毛だったら
                //material.overridePipelineName = "characterFurAndHairSkeletalMesh";
                material.materialType = MaterialType::Hair;
            }
            else if (material.name == "M_Aurora_Fur_FrozenHearth")
            {// 髪の毛だったら
                material.overridePipelineName = "characterFurAndHairSkeletalMesh";
                material.materialType = MaterialType::Fur;
            }
        }
    }
    {
        PROFILE_SCOPE("Create PlayerAnimationController");

        // ルートノードを設定する
        int rootNodeIndex = skeletalMeshComponent->FindIndexByName("root");
        // アニメーションコントローラーを作成
        auto controller = std::make_shared<AnimationController>(this, skeletalMeshComponent.get(), rootNodeIndex);
        controller->AddAnimation("Idle", 0);
        controller->AddAnimation("Idle_Noise_A_0", 1);
        controller->AddAnimation("Idle_Noise_B_0", 2);
        controller->AddAnimation("Emote_Ice_Sculpture1_0", 3);
        controller->AddAnimation("Recall_0", 4);
        controller->AddAnimation("Level_Start_Cut", 5);

        // 剣を地面に突き刺す時のSE
        controller->AddNotifyEvent("Recall_0", 1.4f, AnimationNotifyEvent::Type::PlaySE, "player_recall");
        controller->AddNotifyEvent("Recall_0", 1.4f, AnimationNotifyEvent::Type::PlaySE, "player_recall_voice", 1.2f);

        // 剣を構えたときのSE
        controller->AddNotifyEvent("Level_Start_Cut", 0.48f, AnimationNotifyEvent::Type::PlaySE, "player_attack2");
        controller->AddNotifyEvent("Level_Start_Cut", 0.48f, AnimationNotifyEvent::Type::PlaySE, "player_level_voice");
        controller->AddNotifyEvent("Level_Start_Cut", 0.48f, AnimationNotifyEvent::Type::SwordEmissive);


        // アニメーションコントローラーを character に追加
        this->AddBodyAnimationController(controller);
    }

    // ポイントライトコンポーネントを追加
    auto pointLightComponent = this->AddComponent<PointLightComponent>("pointLightComponent", parentName);
    pointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.8f, 0.5f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    pointLightComponent->SetSharedLightName("PlayerPointLight");

    // ポイントライトコンポーネントを追加
    auto backPointLightComponent = this->AddComponent<PointLightComponent>("PlayerBackPointLight", parentName);
    backPointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f,-2.7f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    backPointLightComponent->SetSharedLightName("PlayerBackPointLight");

    // ポイントライトコンポーネントを追加
    auto playerPlusPointLight = this->AddComponent<PointLightComponent>("PlayerPlusPointLight", parentName);
    playerPlusPointLight->SetRelativeLocationDirect({ 0.0f, 1.6f, 0.5f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    playerPlusPointLight->SetSharedLightName("PlayerPointLight");


    {
        PROFILE_SCOPE("Create PlayerComponent");

        // 移動用コンポーネントを追加
        characterMovementComponent = this->AddComponent<CharacterMovementComponent>("movementComponent", parentName);
        characterMovementComponent->SetUseGravity(true);

    }

    int weaponSocketNode = skeletalMeshComponent->FindIndexByName("weapon");


    {
        // 剣の根本のコンポーネントを追加   
        swordRootComponent = AddComponent<SceneComponent>("swordRootComponent", parentName);
        swordRootComponent->SetRelativeLocationDirect({ 0.0f,0.0f,0.3f });
        swordRootComponent->AttachToComponent(skeletalMeshComponent, weaponSocketNode); // "VB root_weapon"

        // 剣の真ん中のコンポーネントを追加
        swordMiddleComponent = AddComponent<SceneComponent>("swordMiddleComponent", "swordRootComponent");
        swordMiddleComponent->SetRelativeLocationDirect({ 0.0f,0.0f,0.4f });

        // 剣の先端のコンポーネントを追加
        swordTipComponent = AddComponent<SceneComponent>("swordTipComponent", "swordRootComponent");
        swordTipComponent->SetRelativeLocationDirect({ 0.0f,0.0f,0.8f });
    }


    // 剣のメッシュコンポーネントを追加
    swordMeshComponent = this->AddComponent<SkeletalMeshComponent>("Sword", parentName);
    swordMeshComponent->SetModel("./Data/Models/Weapons/PlayerSwordGhost/Sword.gltf", false, true);
    swordMeshComponent->AttachToComponent(skeletalMeshComponent, weaponSocketNode); // "VB root_weapon"
    swordMeshComponent->plusAlphaCBuffer->data.cpuColor = { 0.0f,0.8f,1.0f ,0.0f };
    swordMeshComponent->plusAlphaCBuffer->data.flashValue = 9.5f;
    swordMeshComponent->overrideDeferredPipelineName = "GltfModelPlayerWeaponForwardPS";
    swordMeshComponent->overrideForwardPipelineName = "GltfModelPlayerWeaponForwardPS";

    // 剣の残像用の剣のメッシュコンポーネント
    for (auto& ghost : ghosts)
    {
        // マテリアル　ブレンド
        ghost.swordMeshComp = this->AddComponent<SkeletalMeshComponent>("Sword", parentName);
        ghost.swordMeshComp->SetModel("./Data/Models/Weapons/PlayerSwordGhost/Sword.gltf", false, true);
        ghost.swordMeshComp->SetIsVisible(false);
        ghost.swordMeshComp->overrideForwardPipelineName = "PlayerSwordGhostPS";
        ghost.swordMeshComp->overrideDeferredPipelineName = "PlayerSwordGhostPS";
    }

    // 軌跡初期化
    trail.Initialize();
}

void TitlePlayer::Update(float deltaTime)
{
    using namespace DirectX;

    DirectX::XMFLOAT3 swordRootPos = swordRootComponent->GetComponentLocation();
    DirectX::XMFLOAT3 swordMidPos = swordMiddleComponent->GetComponentLocation();
    DirectX::XMFLOAT3 swordTipPos = swordTipComponent->GetComponentLocation();

    prevSwordRootPos = swordRootPos;
    prevSwordMidPos = swordMidPos;
    prevSwordTipPos = swordTipPos;


    // 軌跡の更新処理
    trail.UpdateTrail(deltaTime);

    // 軌跡を追加
    trail.trailPoints.push_back({ swordTipPos,swordRootPos, trailRemainTime });

    //if (isAttackActive)
    //{
        XMFLOAT4X4 currentWorld = swordMeshComponent->GetComponentWorldTransform().ToWorldTransform();

        if (!isPrevSwordWorldValid)
        {
            prevSwordWorld = currentWorld;
            isPrevSwordWorldValid = true;
        }

        XMFLOAT3 prevPos =
        {
            prevSwordWorld._41,
            prevSwordWorld._42,
            prevSwordWorld._43
        };

        XMFLOAT3 currentPos =
        {
            currentWorld._41,
            currentWorld._42,
            currentWorld._43
        };

        swordGhostElapsedTime += deltaTime;
        while (swordGhostElapsedTime >= ghostInterval)
        {
            swordGhostElapsedTime -= ghostInterval;

            float t = 1.0f - swordGhostElapsedTime / deltaTime;

            XMFLOAT3 pos = MathHelper::Lerp(prevPos, currentPos, t);

            XMFLOAT4X4 world = currentWorld;
            world._41 = pos.x;
            world._42 = pos.y;
            world._43 = pos.z;

            ghosts[swordGhostIndex].world = world;
            ghosts[swordGhostIndex].alpha = 1.0f;
            ghosts[swordGhostIndex].isVisible = true;

            swordGhostIndex = (swordGhostIndex + 1) % ghosts.size();
        }
        // 前回の姿勢を保存する
        prevSwordWorld = currentWorld;
    //}

    // 剣の残像用の剣のメッシュコンポーネント
    for (auto& ghost : ghosts)
    {
        ghost.alpha -= deltaTime / ghostFadeTime;

        if (ghost.alpha <= 0)
        {
            ghost.alpha = 0;
            ghost.isVisible = false;
        }

        //ghost.swordMeshComp->SetIsVisible(ghost.isVisible);

        if (!ghost.isVisible)
            continue;
        if (ghost.swordMeshComp)
        {
            ghost.swordMeshComp->plusAlphaCBuffer->data.emissionPower = swordGhostEmissive;
            ghost.swordMeshComp->plusAlphaCBuffer->data.cpuColor = { swordGhostColor.x,swordGhostColor.y,swordGhostColor.z, ghost.alpha };
            ghost.swordMeshComp->plusAlphaCBuffer->data.effectParameters.edgeColor = { ghostEdgeColor.x,ghostEdgeColor.y,ghostEdgeColor.z,1.0f };
            ghost.swordMeshComp->plusAlphaCBuffer->data.effectParameters.innerColor = { ghostInnerColor.x,ghostInnerColor.y,ghostInnerColor.z,1.0f };
            ghost.swordMeshComp->plusAlphaCBuffer->data.effectParameters.edgeWidth = ghostEdgeWidth;
        }
    }

    // これは絶対入れる　アニメーションの更新をしているから
    Character::Update(deltaTime);

}

// 軌跡を描画する処理
void TitlePlayer::RenderTrail(ID3D11DeviceContext* immediateContext)
{
    trail.Render(immediateContext);
}

void TitlePlayer::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::DragFloat(U8("剣の軌跡が残る時間"), &trailRemainTime, 0.1f);
    ImGui::DragFloat(U8("剣の残像が残る時間"), &ghostFadeTime, 0.1f);
    ImGui::ColorEdit3(U8("剣の残像の色"), &swordGhostColor.x);
    ImGui::DragFloat(U8("残像のemissiveColor"), &swordGhostEmissive, 0.1f);
    ImGui::DragFloat(U8("残像を出す間隔"), &ghostInterval, 0.001f, 0.0f, 1.0f, "%.5f");
    ImGui::DragFloat(U8("剣の残像の輪郭"), &ghostEdgeWidth);
    ImGui::ColorEdit3(U8("剣の残像のエッジの色"), &ghostEdgeColor.x);
    ImGui::ColorEdit3(U8("剣の残像の内部の色"), &ghostInnerColor.x);
    Character::DrawImGuiDetails();
#endif
}

void TitlePlayer::OnAnimationNotifyBegin(const AnimationNotifyState& state)
{
}

void TitlePlayer::OnAnimationNotifyEnd(const AnimationNotifyState& state)
{
}

void TitlePlayer::OnAnimationNotifyEvent(const AnimationNotifyEvent& event)
{
    switch (event.type)
    {
    case AnimationNotifyEvent::Type::PlaySE:
    {
        std::string audioPath = "./Data/Sound/SE/" + event.parameter + ".wav";
        CoreAudio::PlayOneShot(audioPath, event.value);
    }
    break;
    case AnimationNotifyEvent::Type::SpawnEffect:
        break;
    case AnimationNotifyEvent::Type::SwordEmissive:
        if (swordMeshComponent)
        {// 剣にエミッシブを追加
            swordMeshComponent->plusAlphaCBuffer->data.emissionPower = 10.5f;
        }
        break;
    }
}

void TitlePlayer::OnAnimationChanged()
{
}

// アニメーションステート関連のフラグをリセットする
void TitlePlayer::ResetAnimationStateFlag()
{
}


