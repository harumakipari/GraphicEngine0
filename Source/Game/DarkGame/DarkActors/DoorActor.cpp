#include "pch.h"
#include "DoorActor.h"

#include "Engine/Audio/Audio.h"
#include "Engine/Camera/MovieCameraManagerActor.h"
#include "Engine/Scene/Scene.h"

void DoorLargeActor::Initialize(const Transform& transform)
{
    // インタラクトの初期化
    InteractableActor::Initialize(transform);

    root = AddComponent<SceneComponent>("DoorRoot");

    leftHinge = AddComponent<SceneComponent>("LeftHinge", "DoorRoot");
    rightHinge = AddComponent<SceneComponent>("RightHinge", "DoorRoot");

    leftDoorMesh = AddComponent<SkeletalMeshComponent>("LeftDoor", "LeftHinge");
    rightDoorMesh = AddComponent<SkeletalMeshComponent>("RightDoor", "RightHinge");

    // ドアのメッシュコンポーネントを追加
    leftDoorMesh->SetModel("./Data/Models/DarkStageAssets/Door_Large/SM_Door_Large_01.gltf", false, true);
    leftDoorMesh->plusAlphaCBuffer->data.objectType = ObjectType::Door;   // オブジェクトの種類を Door に設定
    rightDoorMesh->SetModel("./Data/Models/DarkStageAssets/Door_Large/SM_Door_Large_01.gltf", false, true);
    rightDoorMesh->plusAlphaCBuffer->data.objectType = ObjectType::Door;   // オブジェクトの種類を Door に設定

    // ドアのサイズを取得
    DirectX::XMFLOAT3 leftSize = leftDoorMesh->GetModelSize();
    DirectX::XMFLOAT3 size = leftSize;
    //size.x = 1.6f;
    // ドアの当たり判定用のコリジョンコンポーネントを追加
    std::shared_ptr<BoxComponent> leftBoxComponent = AddComponent<BoxComponent>("DoorLeftCollision", "LeftHinge");
    leftBoxComponent->SetBoxExtent(size);
    leftBoxComponent->SetRelativeLocationDirect({ 0.0f,0.0f,0.0f });
    //leftBoxComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-1.1f });
    leftBoxComponent->SetCollisionOffsetY(leftSize.y * 0.5f);
    leftBoxComponent->SetCollisionOffsetX(-leftSize.x * 0.5f);
    leftBoxComponent->SetCollisionOffsetZ(-leftSize.z * 0.5f);
    leftBoxComponent->SetStatic(true);
    leftBoxComponent->SetLayer(CollisionLayer::WorldPropsNoRaycast);
    leftBoxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
    leftBoxComponent->Initialize();

    // ドアのサイズを取得
    DirectX::XMFLOAT3 rightSize = rightDoorMesh->GetModelSize();
    // ドアの当たり判定用のコリジョンコンポーネントを追加
    std::shared_ptr<BoxComponent> rightBoxComponent = AddComponent<BoxComponent>("DoorRightCollision", "RightHinge");
    rightBoxComponent->SetBoxExtent(size);
    rightBoxComponent->SetRelativeLocationDirect({ 0.2f,0.0f,0.0f });
    //rightBoxComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-1.1f });
    rightBoxComponent->SetCollisionOffsetY(rightSize.y * 0.5f);
    rightBoxComponent->SetCollisionOffsetX(-rightSize.x * 0.5f);
    rightBoxComponent->SetCollisionOffsetZ(-rightSize.z * 0.5f);
    rightBoxComponent->SetStatic(true);
    rightBoxComponent->SetLayer(CollisionLayer::WorldPropsNoRaycast);
    rightBoxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
    rightBoxComponent->Initialize();

    // ヒンジ位置調整
    leftHinge->SetRelativeLocationDirect({ 0.0f,0,-2.0f });
    leftHinge->SetRelativeEulerRotationDirect({ 0.0f,closedAngleLeft,0.0f });
    rightHinge->SetRelativeLocationDirect({ 0.0f,0,2.0f });
    rightHinge->SetRelativeEulerRotationDirect({ 0.0f,closedAngleRight,0.0f });

    // インタラクト角度を設定する
    interactDegree = 0.0f;

    // インタラクトUIの座標を設定する
    interactUiWorldPos = { 1200.0f,500.0f };
}

void DoorLargeActor::Update(float deltaTime)
{
    // インタラクトの更新
    InteractableActor::Update(deltaTime);

    switch (doorState)
    {
    case DoorState::Opening:
        openAlpha += deltaTime / openTime;
        openAlpha = std::clamp(openAlpha, 0.0f, 1.0f);
        if (openAlpha >= 1.0f)
            doorState = DoorState::Open;
        break;
    case DoorState::Closing:
        openAlpha -= deltaTime / closeTime;
        openAlpha = std::clamp(openAlpha, 0.0f, 1.0f);
        if (openAlpha <= 0.0f)
            doorState = DoorState::Closed;
        break;
    case DoorState::Closed:
        openAlpha = 0.0f;
        break;
    case DoorState::Open:
        openAlpha = 1.0f;
        break;
    }

    float leftAngle = std::lerp(closedAngleLeft, openedAngleLeft, openAlpha);
    float rightAngle = std::lerp(closedAngleRight, openedAngleRight, openAlpha);

    leftHinge->SetRelativeEulerRotationDirect({ 0,leftAngle,0 });
    rightHinge->SetRelativeEulerRotationDirect({ 0,rightAngle,0 });
}

void DoorLargeActor::Interact()
{
    InteractableActor::Interact();

    if (doorState == DoorState::Closed || doorState == DoorState::Closing)
    {
        if (auto movieManager = GetOwnerScene()->GetActorManager()->GetActorOfType<MovieCameraManagerActor>())
        {
            movieManager->PlayDoorMovie();
        }
    }
    else if (doorState == DoorState::Open || doorState == DoorState::Opening)
    {
        doorState = DoorState::Closing;
        CoreAudio::PlayOneShot("./Data/Sound/SE/big_door_close.wav");
    }
}

// ドアを開く
void DoorLargeActor::Open()
{
    if (doorState == DoorState::Closed || doorState == DoorState::Closing)
    {
        doorState = DoorState::Opening;
        CoreAudio::PlayOneShot("./Data/Sound/SE/big_door_open.wav");
    }
}

// ドアを閉める
void DoorLargeActor::Closed()
{
    leftHinge->SetRelativeEulerRotationDirect({ 0,closedAngleLeft,0 });
    rightHinge->SetRelativeEulerRotationDirect({ 0,closedAngleRight,0 });

    doorState = DoorState::Closed;
}

void DoorLargeActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    InteractableActor::DrawImGuiDetails();
    ImGui::DragFloat(U8("ドアを開ける速度"), &openTime, 0.1f);
    ImGui::DragFloat(U8("ドアを閉める速度"), &closeTime, 0.1f);
    if (ImGui::Button(U8("ドア空ける")))
    {
        if (doorState == DoorState::Closed || doorState == DoorState::Closing)
        {
            doorState = DoorState::Opening;
        }
    }
    if (ImGui::Button(U8("ドア閉める")))
    {
        if (doorState == DoorState::Open || doorState == DoorState::Opening)
        {
            doorState = DoorState::Closing;
        }
    }

#endif
}


void DoorSmallActor::Initialize(const Transform& transform)
{
    // インタラクトの初期化
    InteractableActor::Initialize(transform);

    root = AddComponent<SceneComponent>("DoorRoot");
    hinge = AddComponent<SceneComponent>("Hinge", "DoorRoot");
    doorMesh = AddComponent<SkeletalMeshComponent>("Door", "Hinge");
    // ドアのメッシュコンポーネントを追加
    doorMesh->SetModel("./Data/Models/DarkStageAssets/Door_Small/SmallDoor.gltf", false, true);

    // ドアのサイズを取得
    DirectX::XMFLOAT3 size = doorMesh->GetModelSize();
    // ドアの当たり判定用のコリジョンコンポーネントを追加
    std::shared_ptr<BoxComponent> boxComponent = AddComponent<BoxComponent>("DoorCollision", "Hinge");
    boxComponent->SetBoxExtent(size);
    boxComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-1.1f });
    boxComponent->SetCollisionOffsetY(size.y * 0.5f);
    boxComponent->SetCollisionOffsetX(-size.x * 0.5f);
    boxComponent->SetCollisionOffsetZ(-size.z * 0.5f);
    boxComponent->SetStatic(true);
    boxComponent->SetLayer(CollisionLayer::WorldProps);
    boxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
    boxComponent->Initialize();

    // インタラクトUIの座標を設定する
    interactUiWorldPos = { 1160.0f,500.0f };
}


void DoorSmallActor::Update(float deltaTime)
{
    // インタラクトの更新
    InteractableActor::Update(deltaTime);

    switch (doorState)
    {
    case DoorState::Opening:
        openAlpha += deltaTime / openTime;
        openAlpha = std::clamp(openAlpha, 0.0f, 1.0f);
        if (openAlpha >= 1.0f)
            doorState = DoorState::Open;
        break;
    case DoorState::Closing:
        openAlpha -= deltaTime / closeTime;
        openAlpha = std::clamp(openAlpha, 0.0f, 1.0f);
        if (openAlpha <= 0.0f)
            doorState = DoorState::Closed;
        break;
    case DoorState::Closed:
        openAlpha = 0.0f;
        break;
    case DoorState::Open:
        openAlpha = 1.0f;
        break;
    }

    float angle = std::lerp(closedAngle, openedAngle, openAlpha);

    hinge->SetRelativeEulerRotationDirect({ 0,angle,0 });
}

void DoorSmallActor::Interact()
{
    InteractableActor::Interact();

    if (doorState == DoorState::Closed || doorState == DoorState::Closing)
    {
        doorState = DoorState::Opening;
    }
    else if (doorState == DoorState::Open || doorState == DoorState::Opening)
    {
        doorState = DoorState::Closing;
    }
}

void DoorSmallActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    InteractableActor::DrawImGuiDetails();
    ImGui::DragFloat(U8("ドアを開ける速度"), &openTime, 0.1f);
    ImGui::DragFloat(U8("ドアを閉める速度"), &closeTime, 0.1f);
#endif
}


void DoorJailActor::Initialize(const Transform& transform)
{
    // インタラクトの初期化
    InteractableActor::Initialize(transform);

    root = AddComponent<SceneComponent>("DoorRoot");
    hinge = AddComponent<SceneComponent>("Hinge", "DoorRoot");
    doorMesh = AddComponent<SkeletalMeshComponent>("Door", "Hinge");
    // ドアのメッシュコンポーネントを追加
    doorMesh->SetModel("./Data/Models/DarkStageAssets/Door_Jail/Door_Jail.gltf", false, true);

    // ドアのサイズを取得
    DirectX::XMFLOAT3 size = doorMesh->GetModelSize();
    // ドアの当たり判定用のコリジョンコンポーネントを追加
    std::shared_ptr<BoxComponent> boxComponent = AddComponent<BoxComponent>("DoorCollision", "Hinge");
    boxComponent->SetBoxExtent({ 0.005f,0.945f,1.655f });
    boxComponent->SetRelativeLocationDirect({ 0.0f,0.0f,0.0f });
    boxComponent->SetCollisionOffsetY(size.y * 0.5f);
    boxComponent->SetCollisionOffsetX(-size.x * 0.5f);
    boxComponent->SetCollisionOffsetZ(-size.z * 0.5f);
    boxComponent->SetStatic(true);
    boxComponent->SetLayer(CollisionLayer::WorldPropsNoRaycast);
    boxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
    boxComponent->Initialize();

    // インタラクトの角度を設定する
    interactDegree = 0.0f;

    // インタラクトUIの座標を設定する
    interactUiWorldPos = { 1120.0f,490.0f };
}


void DoorJailActor::Update(float deltaTime)
{
    // インタラクトの更新
    InteractableActor::Update(deltaTime);

    if (InputSystem::GetInputState("2", InputStateMask::Trigger))
    {
        CoreAudio::PlayOneShot("./Data/Sound/SE/jail_door_open.wav");
        doorState = DoorState::Opening;
    }

    switch (doorState)
    {
    case DoorState::Opening:
        openAlpha += deltaTime / openTime;
        openAlpha = std::clamp(openAlpha, 0.0f, 1.0f);
        if (openAlpha >= 1.0f)
            doorState = DoorState::Open;
        break;
    case DoorState::Closing:
        openAlpha -= deltaTime / closeTime;
        openAlpha = std::clamp(openAlpha, 0.0f, 1.0f);
        if (openAlpha <= 0.0f)
            doorState = DoorState::Closed;
        break;
    case DoorState::Closed:
        openAlpha = 0.0f;
        break;
    case DoorState::Open:
        openAlpha = 1.0f;
        break;
    }

    float angle = std::lerp(closedAngle, openedAngle, openAlpha);

    hinge->SetRelativeEulerRotationDirect({ 0,angle,0 });
}

void DoorJailActor::Interact()
{
    InteractableActor::Interact();

    if (doorState == DoorState::Closed || doorState == DoorState::Closing)
    {
        CoreAudio::PlayOneShot("./Data/Sound/SE/jail_door_open.wav");
        doorState = DoorState::Opening;
    }
    else if (doorState == DoorState::Open || doorState == DoorState::Opening)
    {
        CoreAudio::PlayOneShot("./Data/Sound/SE/jail_door_close.wav");
        doorState = DoorState::Closing;
    }
}
void DoorJailActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    InteractableActor::DrawImGuiDetails();
    ImGui::DragFloat(U8("ドアを開ける速度"), &openTime, 0.1f);
    ImGui::DragFloat(U8("ドアを閉める速度"), &closeTime, 0.1f);
#endif
}


