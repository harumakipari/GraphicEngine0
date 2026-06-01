#include "pch.h"
#include "DoorActor.h"

void DoorLargeActor::Initialize(const Transform& transform)
{
    root = AddComponent<SceneComponent>("DoorRoot");

    leftHinge = AddComponent<SceneComponent>("LeftHinge", "DoorRoot");
    rightHinge = AddComponent<SceneComponent>("RightHinge", "DoorRoot");

    leftDoorMesh = AddComponent<SkeletalMeshComponent>("LeftDoor", "LeftHinge");
    rightDoorMesh = AddComponent<SkeletalMeshComponent>("RightDoor", "RightHinge");

    // ドアのメッシュコンポーネントを追加
    leftDoorMesh->SetModel("./Data/Models/DarkStageAssets/Door_Large/SM_Door_Large_01.gltf", false, true);
    //leftDoorMesh->plusAlphaCBuffer->data.objectType = ObjectType::Door;   // オブジェクトの種類を Door に設定
    rightDoorMesh->SetModel("./Data/Models/DarkStageAssets/Door_Large/SM_Door_Large_01.gltf", false, true);
    //rightDoorMesh->plusAlphaCBuffer->data.objectType = ObjectType::Door;   // オブジェクトの種類を Door に設定

    // ドアのサイズを取得
    DirectX::XMFLOAT3 leftSize = leftDoorMesh->GetModelSize();
    // ドアの当たり判定用のコリジョンコンポーネントを追加
    std::shared_ptr<BoxComponent> leftBoxComponent = AddComponent<BoxComponent>("DoorLeftCollision", "LeftHinge");
    leftBoxComponent->SetBoxExtent(leftSize);
    leftBoxComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-1.1f });
    leftBoxComponent->SetCollisionOffsetY(leftSize.y * 0.5f);
    leftBoxComponent->SetCollisionOffsetX(-leftSize.x * 0.5f);
    leftBoxComponent->SetCollisionOffsetZ(-leftSize.z * 0.5f);
    leftBoxComponent->SetStatic(true);
    leftBoxComponent->SetLayer(CollisionLayer::WorldProps);
    leftBoxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
    leftBoxComponent->Initialize();

    // ドアのサイズを取得
    DirectX::XMFLOAT3 rightSize = rightDoorMesh->GetModelSize();
    // ドアの当たり判定用のコリジョンコンポーネントを追加
    std::shared_ptr<BoxComponent> rightBoxComponent = AddComponent<BoxComponent>("DoorRightCollision", "RightHinge");
    rightBoxComponent->SetBoxExtent(rightSize);
    rightBoxComponent->SetBoxExtent(rightSize);
    rightBoxComponent->SetRelativeLocationDirect({ 0.0f,0.0f,-1.1f });
    rightBoxComponent->SetCollisionOffsetY(rightSize.y * 0.5f);
    rightBoxComponent->SetCollisionOffsetX(-rightSize.x * 0.5f);
    rightBoxComponent->SetCollisionOffsetZ(-rightSize.z * 0.5f);
    rightBoxComponent->SetStatic(true);
    rightBoxComponent->SetLayer(CollisionLayer::WorldProps);
    rightBoxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
    rightBoxComponent->Initialize();

    // ヒンジ位置調整
    leftHinge->SetRelativeLocationDirect({ 0.0f,0,-2.0f });
    leftHinge->SetRelativeEulerRotationDirect({ 0.0f,180.0f,0.0f });
    rightHinge->SetRelativeLocationDirect({ 0.0f,0,2.0f });
}

void DoorLargeActor::Update(float deltaTime)
{
    float delta = openSpeed * deltaTime;

    switch (doorState)
    {

    case DoorState::Opening:

        openAngle += delta;

        if (openAngle >= 90)
        {
            delta -= (openAngle - 90);
            doorState = DoorState::Open;
        }

        leftHinge->AddLocalRotation({ 0,-delta,0 });
        rightHinge->AddLocalRotation({ 0, delta,0 });

        break;


    case DoorState::Closing:

        openAngle -= delta;

        if (openAngle <= 0)
        {
            delta -= (0 - openAngle);
            doorState = DoorState::Closed;
        }

        leftHinge->AddLocalRotation({ 0, delta,0 });
        rightHinge->AddLocalRotation({ 0,-delta,0 });

        break;

    default:
        break;
    }
}

void DoorLargeActor::Interact()
{
    if (doorState == DoorState::Closed || doorState == DoorState::Closing)
    {
        doorState = DoorState::Opening;
    }
    else if (doorState == DoorState::Open || doorState == DoorState::Opening)
    {
        doorState = DoorState::Closing;
    }
}

void DoorLargeActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
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
}


void DoorSmallActor::Update(float deltaTime)
{
    float delta = openSpeed * deltaTime;
    switch (doorState)
    {
    case DoorState::Opening:
        openAngle += delta;
        if (openAngle >= 90)
        {
            delta -= (openAngle - 90);
            doorState = DoorState::Open;
        }
        hinge->AddLocalRotation({ 0,-delta,0 });
        break;
    case DoorState::Closing:
        openAngle -= delta;
        if (openAngle <= 0)
        {
            delta -= (0 - openAngle);
            doorState = DoorState::Closed;
        }
        hinge->AddLocalRotation({ 0, delta,0 });
        break;
    default:
        break;
    }
}

void DoorSmallActor::Interact()
{
    if (doorState == DoorState::Closed || doorState == DoorState::Closing)
    {
        doorState = DoorState::Opening;
    }
    else if (doorState == DoorState::Open || doorState == DoorState::Opening)
    {
        doorState = DoorState::Closing;
    }
}
