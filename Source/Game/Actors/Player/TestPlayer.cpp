#include "pch.h"
#include "TestPlayer.h"

void TestPlayer::Initialize(const Transform& transform)
{
	std::string parentName = "skeletalComponent";
#if 0
	skeletalMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
	skeletalMeshComponent->SetModel("./Data/Models/Characters/RPG-Character.glb", false, true);

	int rootNodeIndex = 3;
	// アニメーションコントローラーを作成
	auto controller = std::make_shared<AnimationController>(skeletalMeshComponent.get(), rootNodeIndex);
	controller->AddAnimation("Evade_B", 0);
	controller->AddAnimation("Evade_F", 1);
	controller->AddAnimation("Evade_L", 2);
	controller->AddAnimation("Evade_R", 3);
	controller->AddAnimation("Idle", 4);
	controller->AddAnimation("Walk_B", 5);
	controller->AddAnimation("Walk_F", 6);
	controller->AddAnimation("Walk_L", 7);
	controller->AddAnimation("Walk_R", 8);
	controller->AddAnimation("Slash", 9);

	AddBodyAnimationController(controller);
	GetBodyAnimationController()->ResetRootMotion("Idle");
#else
	skeletalMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
	//skeletalMeshComponent->SetModel("./Data/Models/Characters/PlayerNoWeapon1/player.gltf", false, true);
	skeletalMeshComponent->SetModel("./Data/Models/Characters/Player/player.gltf", false, true);

	for (auto& material : skeletalMeshComponent->model->materials)
	{
		if (material.name == "MI_Aurora_Sword_FrozenHearth")
		{// 髪の毛だったら
			material.overridePipelineName = "DarkStagePlayerWeaponPS";
		}
	}

	// ルートノードを設定する
	int rootNodeIndex = skeletalMeshComponent->FindIndexByName("root");

	// アニメーションコントローラーを作成
	auto controller = std::make_shared<AnimationController>(skeletalMeshComponent.get(), rootNodeIndex);
	controller->AddAnimation("Idle", 0);
	controller->AddAnimation("Jog_Fwd", 1);
	controller->AddAnimation("Roll_front_0", 2);
	controller->AddAnimation("Roll_back_0", 3);
	controller->AddAnimation("Roll_left_0", 4);
	controller->AddAnimation("Roll_right_0", 5);
	controller->AddAnimation("Primary_Attack_Fast_A", 6);
	controller->AddAnimation("Primary_Attack_Fast_B", 7);
	controller->AddAnimation("Primary_Attack_Fast_C", 8);
	controller->AddAnimation("Primary_Attack_Fast_D", 9);

	// アニメーションコントローラーを character に追加
	this->AddBodyAnimationController(controller);
	PlayBodyAnimation("Idle");

#endif // 1
	// 敵からの攻撃を受ける当たり判定用のコンポーネントを追加
	std::shared_ptr<CapsuleComponent> capsuleComponent = this->AddComponent<class CapsuleComponent>("capsuleComponent", parentName);
	DirectX::XMFLOAT3 size = skeletalMeshComponent->GetModelSize();
	height = size.y;
	radius = size.x * 0.5f;
	capsuleComponent->SetRadiusAndHeight(radius, height);
	capsuleComponent->SetMass(mass);
	capsuleComponent->SetCapsuleAxis(ShapeComponent::CapsuleAxis::y);
	capsuleComponent->SetLayer(CollisionLayer::Player);
	capsuleComponent->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Block);
	capsuleComponent->SetResponseToLayer(CollisionLayer::Floor, CollisionComponent::CollisionResponse::Block);
	capsuleComponent->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Block);
	capsuleComponent->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Block);
	capsuleComponent->SetResponseToLayer(CollisionLayer::Convex, CollisionComponent::CollisionResponse::Block);
	capsuleComponent->SetCollisionOffsetY(height * 0.5f);
	capsuleComponent->SetIsVisibleDebugBox(false);
	capsuleComponent->Initialize();

	int weaponSocketNode = skeletalMeshComponent->FindIndexByName("VB root_weapon");
	//int weaponSocketNode = skeletalMeshComponent->FindIndexByName("ik_hand_gun");


	// 剣に当たり判定のコンポーネントを追加
	auto swordCollisionComp = AddComponent<CapsuleComponent>("SwordCollision", parentName);
	DirectX::XMFLOAT3 weaponSize = { 0.1f,1.2f,1.0f };
	swordCollisionComp->AttachToComponent(skeletalMeshComponent, weaponSocketNode); // "VB root_weapon"
	swordCollisionComp->SetRadiusAndHeight(weaponSize.x, weaponSize.y);
	swordCollisionComp->SetMass(mass);
	swordCollisionComp->SetCapsuleAxis(ShapeComponent::CapsuleAxis::z);
	swordCollisionComp->SetLayer(CollisionLayer::PlayerWeapon);
	swordCollisionComp->SetResponseToLayer(CollisionLayer::Enemy, CollisionComponent::CollisionResponse::Trigger);
	swordCollisionComp->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Trigger);
	swordCollisionComp->SetResponseToLayer(CollisionLayer::WorldProps, CollisionComponent::CollisionResponse::Trigger);
	swordCollisionComp->SetCollisionOffsetY(height * 0.5f);
	swordCollisionComp->SetIsVisibleDebugBox(false);
	swordCollisionComp->SetRelativeLocationDirect({ -0.f, -0.f, 0.8f });
	swordCollisionComp->Initialize();



	//auto swordMeshComponent = this->AddComponent<SkeletalMeshComponent>("Sword", "SwordCollision");
	//swordMeshComponent->SetModel("./Data/Models/Weapons/PlayerSword/Sword.gltf", false, true);
	//swordMeshComponent->AttachToComponent(skeletalMeshComponent, weaponSocketNode); // "VB root_weapon"

}

void TestPlayer::Update(float elapsedTime)
{
    Character::Update(elapsedTime);
	clip_index = GetBodyAnimationController()->GetAnimationClip();
	std::string currentAnimationName = GetBodyAnimationController()->GetCurrentAnimationName();
#if 0
	bool pressing_shift = (GetKeyState(VK_SHIFT) & 0x8000);
	if (GetKeyState(VK_DOWN) & 0x8000)
	{
		if (pressing_shift && clip_index != EVADE_B)
		{
			GetBodyAnimationController()->ResetRootMotion("Evade_B");
		}
		else if (!pressing_shift && clip_index != WALK_B)
		{
			GetBodyAnimationController()->ResetRootMotion("Walk_B");
		}
	}
	else if (GetKeyState(VK_UP) & 0x8000)
	{
		if (pressing_shift && clip_index != EVADE_F)
		{
			GetBodyAnimationController()->ResetRootMotion("Evade_F");
		}
		else if (!pressing_shift && clip_index != WALK_F)
		{
			GetBodyAnimationController()->ResetRootMotion("Walk_F");
		}
	}
	else if (GetKeyState(VK_RIGHT) & 0x8000)
	{
		if (pressing_shift && clip_index != EVADE_R)
		{
			GetBodyAnimationController()->ResetRootMotion("Evade_R");
		}
		else if (!pressing_shift && clip_index != WALK_R)
		{
			GetBodyAnimationController()->ResetRootMotion("Walk_R");
		}
	}
	else if (GetKeyState(VK_LEFT) & 0x8000)
	{
		if (pressing_shift && clip_index != EVADE_L)
		{
			GetBodyAnimationController()->ResetRootMotion("Evade_L");
		}
		else if (!pressing_shift && clip_index != WALK_L)
		{
			GetBodyAnimationController()->ResetRootMotion("Walk_L");
		}
	}
	else
	{
		if (clip_index != IDLE)
		{
			GetBodyAnimationController()->ResetRootMotion("Idle");
		}
	}
#else
	bool pressing_shift = (GetKeyState(VK_SHIFT) & 0x8000);
	if (GetKeyState(VK_DOWN) & 0x8000)
	{
		if (pressing_shift && currentAnimationName != "Roll_back_0")
		{
			GetBodyAnimationController()->ResetRootMotion("Roll_back_0");
		}
		else if (!pressing_shift && currentAnimationName != "Jog_Fwd")
		{
			GetBodyAnimationController()->ResetRootMotion("Jog_Fwd");
		}
	}
	else if (GetKeyState(VK_UP) & 0x8000)
	{
		if (pressing_shift && currentAnimationName != "Roll_front_0")
		{
			GetBodyAnimationController()->ResetRootMotion("Roll_front_0");
		}
		else if (!pressing_shift && currentAnimationName != "Jog_Fwd")
		{
			GetBodyAnimationController()->ResetRootMotion("Jog_Fwd");
		}
	}
	else if (GetKeyState(VK_RIGHT) & 0x8000)
	{
		if (pressing_shift && currentAnimationName != "Roll_right_0")
		{
			GetBodyAnimationController()->ResetRootMotion("Roll_right_0");
		}
		else if (!pressing_shift && currentAnimationName != "Jog_Fwd")
		{
			GetBodyAnimationController()->ResetRootMotion("Jog_Fwd");
		}
	}
	else if (GetKeyState(VK_LEFT) & 0x8000)
	{
		if (pressing_shift && currentAnimationName != "Roll_left_0")
		{
			GetBodyAnimationController()->ResetRootMotion("Roll_left_0");
		}
		else if (!pressing_shift && currentAnimationName != "Jog_Fwd")
		{
			GetBodyAnimationController()->ResetRootMotion("Jog_Fwd");
		}
	}
	//else
	//{
	//	if (currentAnimationName != "Idle")
	//	{
	//		GetBodyAnimationController()->ResetRootMotion("Idle");
	//	}
	//}
#endif // 0

}


void TestPlayer::DrawImGuiDetails()
{
	Character::DrawImGuiDetails();
}