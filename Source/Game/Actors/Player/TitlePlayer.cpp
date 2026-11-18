#include "pch.h"
#include "TitlePlayer.h"

#include "Components/Render/PointLightComponent.h"

void TitlePlayer::Initialize(const Transform& transform)
{
    std::string parentName = "skeletalComponent";
    // 描画用コンポーネントを追加
    {
        PROFILE_SCOPE("Create PlayerModel");

        skeletalMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
        skeletalMeshComponent->SetModel("./Data/Models/Characters/PlayerNoWeapon/titlePlayer.gltf", false, true);
        skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Player;   // オブジェクトの種類を Player に設定
        skeletalMeshComponent->plusAlphaCBuffer->data.cpuColor.w = 1.0f;
        skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 20.9f;   // 自己発光の強さを設定
        // 服の色のための色相変更
        skeletalMeshComponent->plusAlphaCBuffer->data.hueShift = -1.0f;
        skeletalMeshComponent->plusAlphaCBuffer->data.saturation = 0.442f;
        skeletalMeshComponent->plusAlphaCBuffer->data.brightness = 0.352f;
        skeletalMeshComponent->plusAlphaCBuffer->data.contrast = 0.8f;
        skeletalMeshComponent->overrideDeferredPipelineName = "GltfModelPlayerDeferredPS";
        skeletalMeshComponent->SetIsCastShadow(false);
        skeletalMeshComponent->SetIsShadowMap(true);
        for (auto& material : skeletalMeshComponent->model->materials)
        {
            if (material.name == "M_Aurora_Hair_Blonde_FrozenHearth")
            {// 髪の毛だったら
                material.materialType = MaterialType::Hair;
            }
            else if (material.name == "M_Aurora_Fur_FrozenHearth")
            {// 毛皮材質なら
                material.overridePipelineName = "characterFurAndHairSkeletalMesh";
                material.materialType = MaterialType::Fur;
            }
            else if (material.name == "M_Aurora_Dress_Skirt_FrozenHearth" ||
                material.name == "M_Aurora_Dress_FrozenHearth" ||
                material.name == "M_Aurora_Body_Metals_FrozenHearth" ||
                material.name == "M_Aurora_Fur_FrozenHearth")
            {// 服部分だったら、
                material.materialType = MaterialType::Cloth;
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

        // アニメーションコントローラーを character に追加
        this->AddBodyAnimationController(controller);
    }

    // ポイントライトコンポーネントを追加
    auto pointLightComponent = this->AddComponent<PointLightComponent>("pointLightComponent", parentName);
    pointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.8f, 0.5f });
    pointLightComponent->SetSharedLightName("PlayerPointLight");

    auto backPointLightComponent = this->AddComponent<PointLightComponent>("PlayerBackPointLight", parentName);
    backPointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f,-2.7f });
    backPointLightComponent->SetSharedLightName("PlayerBackPointLight");

    auto playerPlusPointLight = this->AddComponent<PointLightComponent>("PlayerPlusPointLight", parentName);
    playerPlusPointLight->SetRelativeLocationDirect({ 0.0f, 1.6f, 0.5f });
    playerPlusPointLight->SetSharedLightName("PlayerPointLight");

    int weaponSocketNode = skeletalMeshComponent->FindIndexByName("weapon");

    // 剣のメッシュコンポーネントを追加
    swordMeshComponent = this->AddComponent<SkeletalMeshComponent>("Sword", parentName);
    swordMeshComponent->SetModel("./Data/Models/Weapons/PlayerSwordGhost/Sword.gltf", false, true);
    swordMeshComponent->AttachToComponent(skeletalMeshComponent, weaponSocketNode); // "VB root_weapon"
    swordMeshComponent->plusAlphaCBuffer->data.cpuColor = { 0.0f,0.8f,1.0f ,0.0f };
    swordMeshComponent->plusAlphaCBuffer->data.flashValue = 9.5f;
    swordMeshComponent->overrideDeferredPipelineName = "GltfModelPlayerWeaponForwardPS";
    swordMeshComponent->overrideForwardPipelineName = "GltfModelPlayerWeaponForwardPS";


}

void TitlePlayer::Update(float deltaTime)
{
    using namespace DirectX;

    // Character::Updateでアニメーションを更新するため必要。
    Character::Update(deltaTime);
    DirectX::XMFLOAT3 playerPos = { -25.302f,0.004f,12.205f };
    SetPosition(playerPos);

}


void TitlePlayer::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    Character::DrawImGuiDetails();
#endif
}

