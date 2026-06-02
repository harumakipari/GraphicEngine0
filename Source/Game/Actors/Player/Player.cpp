#include "pch.h"
#include "Player.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "../External/imgui/imgui.h"
#endif

#include "Graphics/Core/Graphics.h"
#include "Physics/Physics.h"
#include "Core/ActorManager.h"

#include "Components/Render/PointLightComponent.h"

#include "PlayerStateDerived.h"
#include "Components/Audio/CoreAudioSourceComponent.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Camera/Camera.h"
#include "Game/Actors/Enemy/Enemy.h"
#include "Game/Actors/Stage/Stage.h"
#include "Game/DarkGame/Interactable.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Physics/CollisionFunction.h"

void Player::Initialize(const Transform& transform)
{
    std::string parentName = "skeletalComponent";
    // 描画用コンポーネントを追加
    {
        PROFILE_SCOPE("Create PlayerModel");

        skeletalMeshComponent = this->AddComponent<SkeletalMeshComponent>(parentName);
        skeletalMeshComponent->SetModel("./Data/Models/Characters/Player/player.gltf", false, true);
        skeletalMeshComponent->plusAlphaCBuffer->data.objectType = ObjectType::Player;   // オブジェクトの種類を Player に設定
        skeletalMeshComponent->plusAlphaCBuffer->data.emissionPower = 20.9f;   // 自己発光の強さを設定
#if 1
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
#endif // 0
    }
    {
        PROFILE_SCOPE("Create PlayerAnimationController");

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
    }

    {
        PROFILE_SCOPE("Create PlayerStateMachine");
        // ステートマシンを作成
        stateMachine_ = std::make_shared<StateMachine>();
        stateMachine_->RegisterState(std::make_unique<PlayerIdleState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerRunningState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerAttackingState>(this));
        stateMachine_->RegisterState(std::make_unique<PlayerDodgeState>(this));

        // ステートマシンを character に追加
        //this->SetStateMachine(stateMachine);
        // 初期ステートを設定
        stateMachine_->ChangeState("Idle");
    }

    {
        PROFILE_SCOPE("Create PlayerCollision");

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
    }

#if 1
    // ポイントライトコンポーネントを追加
    auto pointLightComponent = this->AddComponent<PointLightComponent>("pointLightComponent", parentName);
    pointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f, 1.0f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    pointLightComponent->SetSharedLightName("PlayerPointLight");

    // ポイントライトコンポーネントを追加
    auto backPointLightComponent = this->AddComponent<PointLightComponent>("PlayerBackPointLight", parentName);
    backPointLightComponent->SetRelativeLocationDirect({ 0.0f, 1.5f,-1.0f });
    // ライトの名前からライトマネージャーの共有ライトを取得して設定
    backPointLightComponent->SetSharedLightName("PlayerBackPointLight");



    AddHitCallback([&](std::pair<CollisionComponent*, CollisionComponent*> hitPair)
        {
#if 0
            CollisionComponent* own = hitPair.first;
            CollisionComponent* other = hitPair.second;

            // 自分が武器じゃなければ無視
            if (!(own->GetCollisionLayer() & CollisionHelper::ToBit(CollisionLayer::PlayerWeapon)))
                return;

            // 攻撃中じゃなければ無視
            if (stateMachine_->GetStateName() != "Attack")
                return;

            // すでに当たってたら無視
            if (hitTargets.contains(other))
                return;

            hitTargets.insert(other);

            uint32_t layer = other->GetCollisionLayer();

            if (layer & CollisionHelper::ToBit(CollisionLayer::WorldStatic) ||
                layer & CollisionHelper::ToBit(CollisionLayer::WorldProps))
            {
                auto hitPos = swordPointComp->GetComponentLocation();
                SpawnSpark(hitPos);
            }
#endif // 0

        }
    );
#endif // 0

    {
        PROFILE_SCOPE("Create PlayerComponent");

        // 入力用のコンポーネントを追加
        inputComponent = this->AddComponent<class InputComponent>("inputComponent", parentName);

        // 移動用コンポーネントを追加
        characterMovementComponent = this->AddComponent<CharacterMovementComponent>("movementComponent", parentName);
        //characterMovementComponent->SetUseGravity(false);

        // 回転用コンポーネントを追加
        rotationComponent = this->AddComponent<class RotationComponent>("rotationComponent", parentName);
    }

    // 剣に当たり判定のコンポーネントを追加
    swordCollisionComp = AddComponent<CapsuleComponent>("SwordCollision", parentName);
    DirectX::XMFLOAT3 size = { 0.1f,1.2f,1.0f };
    swordCollisionComp->AttachToComponent(skeletalMeshComponent, 181); // "VB root_weapon"
    swordCollisionComp->SetRadiusAndHeight(size.x, size.y);
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

#if 0
    auto swordMeshComponent = this->AddComponent<SkeletalMeshComponent>("Sword", parentName);
    swordMeshComponent->SetModel("./Data/Models/Weapons/PlayerSword/Sword.gltf", false, true);
    swordMeshComponent->AttachToComponent(skeletalMeshComponent, 181); // "VB root_weapon"

    auto bowMeshComponent = this->AddComponent<SkeletalMeshComponent>("Bow", parentName);
    bowMeshComponent->SetModel("./Data/Models/Weapons/PlayerBow/AnimationBow.gltf", false, true);

    // 武器アニメーションコントローラーを作成
    auto weaponController = std::make_shared<AnimationController>(bowMeshComponent.get());
    weaponController->AddAnimation("Bow", 0);
    AddAnimationController("Weapon", weaponController);
    PlayAnimation("Weapon", "Bow");


#endif // 0
    //bowMeshComponent->AttachToComponent(skeletalMeshComponent, 23); // "weapon"

    swordPointComp = AddComponent<CapsuleComponent>("SwordPointComponent", "SwordCollision");
    swordPointComp->SetRelativeLocationDirect({ 0.0f,0.0f,0.6f });

    // 火花エフェクト用のコンポーネントを追加
    sparkComponent = this->AddComponent<class ParticleComponent>("particleComponent", parentName);
    sparkComponent->Load("./Data/Effect/Files/DarkStageSparkEffect.json");

}


void Player::Update(float elapsedTime)
{
    using namespace DirectX;

    // ヒットストップ処理
    if (hitStopTimer > 0.0f)
    {
        hitStopTimer -= Time::UnscaledDeltaTime();

        if (hitStopTimer <= 0.0f)
        {
            Time::timeScale = 1.0f; // 元に戻す
        }
    }

    // これは絶対入れる　アニメーションの更新をしているから
    Character::Update(elapsedTime);


    // アニメーション時間から攻撃有効フラグ更新
    auto anim = GetBodyAnimationController();
    float time = anim->GetCurrentAnimationTime(); // ← 秒
    if (stateMachine_->GetStateName() == "Attack" && time >= 0.1f && time <= 0.4f)
    {
        isAttackActive = true;
    }
    else
    {
        isAttackActive = false;
    }


    //skeletalMeshComponent->UpdateCloth(elapsedTime);

    //skeletalMeshComponent->UpdateGlobalTransforms();

    if (InputSystem::GetInputState("RB", InputStateMask::Trigger))
    {
        Logger::Log("RBが押された");
    }
    if (InputSystem::GetInputState("LockOn", InputStateMask::Trigger))
    {
        Logger::Log("LockOnが押された");
    }
    if (InputSystem::GetInputState("RT", InputStateMask::Trigger))
    {
        Logger::Log("RTが押された");
    }
    if (InputSystem::GetInputState("ok", InputStateMask::Trigger))
    {
        if (IInteractable* interactable = FindInteractable())
        {
            interactable->Interact();
        }
    }

    if (swordPointComp)
    {

    auto currentTip = swordPointComp->GetComponentLocation();

    if (hasPrevSwordTip)
    {
        CheckSwordLineHit(prevSwordTip, currentTip);
    }

    prevSwordTip = currentTip;
    hasPrevSwordTip = true;

    DebugRender::DrawSphere(swordPointComp->GetComponentLocation(), 0.1f, { 1,1,0,1 }, 0.0f, true);

    // 剣先取得
    XMFLOAT3 tip = swordPointComp->GetComponentLocation();

    // トレイル追加（毎フレーム）
    trailPoints.push_back({ tip, 0.3f }); // ←長さ調整

    // 更新
    for (auto& p : trailPoints)
    {
        p.life -= elapsedTime;
    }

    // 削除
    trailPoints.erase(
        std::remove_if(trailPoints.begin(), trailPoints.end(),
            [](const TrailPoint& p) { return p.life <= 0; }),
        trailPoints.end());
    }

#if 1
    auto intent = inputComponent->GetIntent();
    //characterMovementComponent->SetMoveDirection({ 1,0,0 });
    DirectX::XMFLOAT3 moveDir = { 0,0,0 };

    if (auto camera = dynamic_cast<MainCamera*>(GetOwnerScene()->GetActiveCamera()))
    {
        auto camForward = camera->CameraForwardXZ();
        auto camRight = camera->CameraRightXZ();

        // 左スティック入力
        float stickX = intent.leftMove.x;
        float stickZ = intent.leftMove.z;

        // カメラ基準の移動方向
        moveDir.x = camForward.x * stickZ + camRight.x * stickX;
        moveDir.z = camForward.z * stickZ + camRight.z * stickX;
    }

    characterMovementComponent->SetMoveDirection(moveDir);
    rotationComponent->SetDirection(moveDir);



#endif // 0

}

void Player::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    Character::DrawImGuiDetails();
#endif

}

// 火花エフェクトの生成
void Player::SpawnSpark(DirectX::XMFLOAT3 pos)
{
    DebugRender::DrawSphere(pos, 0.2f, { 1, 0.5f, 0, 1 }, 0.3f, true);
    if (sparkComponent)
    {
        sparkComponent->SetWorldLocationDirect(pos);
        sparkComponent->Play();
    }
}

// 剣の攻撃判定
void Player::CheckSwordLineHit(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end)
{
    if (stateMachine_->GetStateName() != "Attack")
        return;

    // 1フレーム1回制御
    if (hasSpawnedThisAttack)
        return;

    if (!isAttackActive)
        return;

    XMVECTOR s = XMLoadFloat3(&start);
    XMVECTOR e = XMLoadFloat3(&end);

    XMVECTOR diff = e - s;
    float length = XMVectorGetX(XMVector3Length(diff));

    int steps = std::max<int>(1, (int)(length / 0.05f));

    for (int i = 0; i < steps; i++)
    {
        float t0 = (float)i / steps;
        float t1 = (float)(i + 1) / steps;

        XMVECTOR p0 = XMVectorLerp(s, e, t0);
        XMVECTOR p1 = XMVectorLerp(s, e, t1);

        XMFLOAT3 segStart, segEnd;
        XMStoreFloat3(&segStart, p0);
        XMStoreFloat3(&segEnd, p1);

        HitResultWithActor hit;

        if (CollisionFunction::SphereRayCast(
            segStart,
            segEnd,
            hit,
            0.1f,
            CollisionHelper::ToBit(CollisionLayer::WorldStatic)))
        {
            // 床無視
            if (hit.normal.y > 0.7f)
                continue;

            // 押し出し
            XMVECTOR pos = XMLoadFloat3(&hit.hitPoint);
            XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(&hit.normal));
            pos += normal * 0.03f;

            XMFLOAT3 finalPos;
            XMStoreFloat3(&finalPos, pos);

            SpawnSpark(finalPos);

            hasSpawnedThisAttack = true; // ←これが本質

            return;
        }
    }

    DebugRender::DrawLine(start, end, { 1,0,0,1 });
}




// 遅延更新処理
void Player::LateUpdate(float elapsedTime)
{

}

void Player::Turn(float elapsedTime)
{
    using namespace DirectX;
#if 0
    //pad.Acquire();

//入力情報を取得
    float thumbStateLx = pad.ThumbStateLx();
    float thumbStateLy = pad.ThumbStateLy();


    DirectX::XMVECTOR PAD = XMVectorSet(thumbStateLx, 0.0f, thumbStateLy, 0.0f);
    PAD = DirectX::XMVector3Normalize(PAD);
    // 入力がほぼゼロなら回転しない
    if (std::abs(thumbStateLx) < 0.1f && std::abs(thumbStateLy) < 0.1f)
    {
        return;
    }

    //プレイヤーが向いている前方向ベクトルを求める
    front = GetForward();
    DirectX::XMVECTOR FRONT = DirectX::XMLoadFloat3(&front);
    FRONT = DirectX::XMVector3Normalize(FRONT);

    //プレイヤーの３軸を求める
    DirectX::XMVECTOR PX, PY, PZ;
    PY = DirectX::XMVectorSet(0, 1, 0, 0);
    PZ = FRONT;
    PX = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(PY, PZ));

    ////カメラの注視点と位置を取得する
    //Camera& camera = Camera::Instance();
    //DirectX::XMFLOAT4 cameraPosition = camera.GetCameraPosition();
    //DirectX::XMFLOAT4 cameraFocus = camera.GetFocus();
    //XMVECTOR CP = XMLoadFloat4(&cameraPosition);
    //XMVECTOR CF = XMLoadFloat4(&cameraFocus);
    ////カメラの座標軸を求める
    //XMVECTOR CX, CY, CZ;
    //CY = XMVectorSet(0, 1, 0, 0);
    //CZ = XMVector3Normalize(CF - CP);
    //CX = XMVector3Normalize(XMVector3Cross(CY, CZ));
    //CY = XMVector3Normalize(XMVector3Cross(CZ, CX));

    //const float inputDeadZone = 0.0001f;

    ////スティックの入力をカメラの座標軸に変換して進行方向のベクトルを求める
    ////カメラ基準の移動方向
    //DirectX::XMVECTOR CameraBasedMoveDirection = DirectX::XMVector3Normalize(thumbStateLx * CX + thumbStateLy * CZ);

    ////進行方向ベクトルとplayerの今の向いている方向の X 軸右ベクトルで内積する
    //float turningSpeedFactor = DirectX::XMVectorGetX(DirectX::XMVector3Dot(CameraBasedMoveDirection, PX));

    //if (std::fabsf(turningSpeedFactor) > inputDeadZone)
    //{
    //    turningSpeed = maxTurningSpeed * turningSpeedFactor;
    //}
    // 


    //float cross = DirectX::XMVectorGetX(DirectX::XMVector3Cross(CameraBasedMoveDirection, PZ));
    //if (cross > 0.0f)
    //{//左に回転する
    //    rotation.y -= rot;
    //}
    //else
    //{//右に回転する
    //    rotation.y += rot;
    //}

#if 0
    XMVECTOR Q = XMVectorSet(rotation.x, rotation.y, rotation.z, 0.0f);
    XMMATRIX R = XMMatrixRotationQuaternion(Q); //クォータニオンから回転行列を作成
    XMVECTOR Y = R.r[1];//モデルのローカルなY軸を取り出す
    Q = XMQuaternionNormalize(XMQuaternionMultiply(Q, XMQuaternionRotationAxis(Y, +DirectX::XMConvertToRadians(turningSpeed) * elapsedTime)));
    XMStoreFloat3(&rotation, Q);
#else
    //rotation.y += DirectX::XMConvertToRadians(turningSpeed) * elapsedTime;
#endif

#endif // 0
    return;

    float vx = inputComponent->GetThumbStateRx();
    float vz = inputComponent->GetThumbStateRy();
    float speed = DirectX::XMConvertToRadians(currentTurnSpeed) * elapsedTime;

    // 進行ベクトルがゼロベクトルの場合は処理する必要なし
    float length = sqrtf(vx * vx + vz * vz);
    if (length <= 0.001f)
    {
        return;
    }
    // 進行ベクトルを単位ベクトル化
    vx /= length;
    vz /= length;

    // 自身の回転値から前方向を求める
    float frontX = sinf(angle.y);
    float frontZ = cosf(angle.y);

    // 回転角を求めるため、2つの単位ベクトルの内積を計算する
    float dot = frontX * vx + frontZ * vz;

    // 内積値は-1.0～1.0で表現されており、2つの単位ベクトルの角度が
    // 小さいほど1.0に近づくという性質を利用して回転速度を調整する
    float rot = 1.0f - dot;

    if (rot > speed)
    {
        rot = speed;
    }

    // 左右判定を行うために２つの単位ベクトルの外積を計算する
    float cross = (frontX * vz) - (frontZ * vx);

    // 2Dの外積値が生の場合か負の場合かによって左右判定が行える
    // 左右判定を行うことによって左右回転を選択する
    if (cross < 0.0f)
    {
        angle.y += rot;
    }
    else
    {
        angle.y -= rot;
    }

    DirectX::XMFLOAT4 quaternion;
    DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(angle.x, angle.y, angle.z);
    DirectX::XMStoreFloat4(&quaternion, q);

    SetQuaternionRotation(quaternion);



#if 0
    float thumbStateRx = inputComponent->GetThumbStateRx();
    float thumbStateRy = inputComponent->GetThumbStateRy();

    DirectX::XMVECTOR PAD = XMVectorSet(thumbStateRx, 0.0f, thumbStateRy, 0.0f);
    PAD = DirectX::XMVector3Normalize(PAD);
    // 入力がほぼゼロなら回転しない
    if (std::abs(thumbStateRx) < 0.1f && std::abs(thumbStateRy) < 0.1f)
    {
        return;
    }

    //プレイヤーが向いている前方向ベクトルを求める
    front = GetForward();
    DirectX::XMVECTOR FRONT = DirectX::XMLoadFloat3(&front);
    FRONT = DirectX::XMVector3Normalize(FRONT);

    //プレイヤーの３軸を求める
    DirectX::XMVECTOR PX, PY, PZ;
    PY = DirectX::XMVectorSet(0, 1, 0, 0);
    PZ = FRONT;
    PX = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(PY, PZ));

    ////カメラの注視点と位置を取得する
    //Camera& camera = Camera::Instance();
    //DirectX::XMFLOAT4 cameraPosition = camera.GetCameraPosition();
    //DirectX::XMFLOAT4 cameraFocus = camera.GetFocus();
    //XMVECTOR CP = XMLoadFloat4(&cameraPosition);
    //XMVECTOR CF = XMLoadFloat4(&cameraFocus);
    ////カメラの座標軸を求める
    //XMVECTOR CX, CY, CZ;
    //CY = XMVectorSet(0, 1, 0, 0);
    //CZ = XMVector3Normalize(CF - CP);
    //CX = XMVector3Normalize(XMVector3Cross(CY, CZ));
    //CY = XMVector3Normalize(XMVector3Cross(CZ, CX));

    const float inputDeadZone = 0.0001f;

    ////スティックの入力をカメラの座標軸に変換して進行方向のベクトルを求める
    ////カメラ基準の移動方向
    //DirectX::XMVECTOR CameraBasedMoveDirection = DirectX::XMVector3Normalize(thumbStateLx * CX + thumbStateLy * CZ);

    DirectX::XMFLOAT3 moveDir = inputComponent->GetMoveInput();
    DirectX::XMVECTOR MoveDirection = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&moveDir));
    //float lenSq = moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z;
    //if (lenSq > 0.0001f)
    //{
    //    rotationComponent->SetDirection(moveDir);
    //}

    ////進行方向ベクトルとplayerの今の向いている方向の X 軸右ベクトルで内積する
    float turningSpeedFactor = DirectX::XMVectorGetX(DirectX::XMVector3Dot(MoveDirection, PX));

    if (std::fabsf(turningSpeedFactor) > inputDeadZone)
    {
        turningSpeed = maxTurningSpeed * turningSpeedFactor;
    }



    float cross = DirectX::XMVectorGetX(DirectX::XMVector3Cross(MoveDirection, PZ));
    if (cross > 0.0f)
    {//左に回転する
        angle.y -= rot;
    }
    else
    {//右に回転する
        angle.y += rot;
    }

#if 0
    XMVECTOR Q = XMVectorSet(rotation.x, rotation.y, rotation.z, 0.0f);
    XMMATRIX R = XMMatrixRotationQuaternion(Q); //クォータニオンから回転行列を作成
    XMVECTOR Y = R.r[1];//モデルのローカルなY軸を取り出す
    Q = XMQuaternionNormalize(XMQuaternionMultiply(Q, XMQuaternionRotationAxis(Y, +DirectX::XMConvertToRadians(turningSpeed) * elapsedTime)));
    XMStoreFloat3(&rotation, Q);
#else
    angle.y += DirectX::XMConvertToRadians(turningSpeed) * elapsedTime;
#endif
    DirectX::XMFLOAT4 quaternion;
    DirectX::XMVECTOR Quat = DirectX::XMQuaternionRotationRollPitchYaw(angle.x, angle.y, angle.z);
    DirectX::XMStoreFloat4(&quaternion, Quat);
    SetQuaternionRotation(quaternion);

#endif // 0
}

void Player::Move(float elapsedTime)
{
    //pad.Acquire();

    DirectX::XMFLOAT3 pos = GetPosition();
    //進行方向のベクトル取得
    DirectX::XMFLOAT3 moveVec = GetMoveVec();
    velocity.x = moveVec.x;
    velocity.z = moveVec.z;
    float moveSpeed = 5.0f * elapsedTime;
    pos.x += moveVec.x * moveSpeed;
    pos.z += moveVec.z * moveSpeed;
    SetPosition(pos);
    //position.x += velocity.x * moveSpeed;
    //position.z += velocity.z * moveSpeed;
}

//当たった時の処理
void Player::TakeDamage(int damage)
{
    if (invincible)
    {// 無敵状態ならダメージを受けない
        Logger::Log(U8("攻撃を回避した"));
        return;
    }
    hp -= damage;
    Logger::Log(U8("プレイヤーにダメージ！ HP:") + std::to_string(hp));
    if (sparkComponent)
    {
        sparkComponent->Play();
    }
}

// 攻撃ヒット時の処理
void Player::DoAttackHit()
{
    auto enemies = GetOwnerScene()->GetActorManager()->GetActorsOfType<Character>();

    for (auto& actor : enemies)
    {
        auto enemy = std::dynamic_pointer_cast<GruxEnemy>(actor);
        if (!enemy) continue;

        auto p = GetPosition();
        auto e = enemy->GetPosition();

        // 敵へのベクトル
        float dx = e.x - p.x;
        float dz = e.z - p.z;

        float distSq = dx * dx + dz * dz;
        float attackRange = 2.5f;

        if (distSq > attackRange * attackRange)
            return;

        // 正規化
        float len = sqrtf(dx * dx + dz * dz);
        dx /= len;
        dz /= len;

        // プレイヤーの前方向（Z+方向）
        DirectX::XMFLOAT3 forward = GetForward();

        float dot = dx * forward.x + dz * forward.z;

        float angleCos = cosf(DirectX::XMConvertToRadians(60.0f)); // 60度

        if (dot > angleCos)
        {
            enemy->TakeDamage(10);
            //　ヒットストップ発動
            Time::timeScale = 0.1f;
            hitStopTimer = 0.35f;
        }
    }
}


// インタラクト対象検索
IInteractable* Player::FindInteractable()
{
    float bestDist = 2.0f;
    IInteractable* best = nullptr;

    DirectX::XMFLOAT3 forward = GetForward(); // プレイヤー前方向

    for (auto& actor : GetOwnerScene()->GetActorManager()->GetAllActors())
    {
        auto interactable = dynamic_cast<IInteractable*>(actor.get());
        if (!interactable) continue;

        DirectX::XMFLOAT3 dir = MathHelper::Normalize(
            MathHelper::Subtract(actor->GetPosition(), GetPosition())
        );

        float dot = MathHelper::Dot(forward, dir);

        // 前方60度以内
        if (dot < 0.5f) continue;

        float dist = MathHelper::Distance(GetPosition(), actor->GetPosition());

        if (dist < bestDist)
        {
            bestDist = dist;
            best = interactable;
        }
    }

    return best;
}



//スティックの入力値から移動ベクトルを取得 
DirectX::XMFLOAT3 Player::GetMoveVec()
{
    //pad.Acquire();
#if 0
    //入力情報を取得
    float ax = pad.ThumbStateLx();
    float ay = pad.ThumbStateLy();

    //カメラ方向とスティックの入力値によって進行方向を計算する
    Camera& camera = Camera::Instance();
    const DirectX::XMFLOAT3& cameraRight = camera.GetRight();
    const DirectX::XMFLOAT3& cameraFront = camera.GetFront();

    //カメラ右方向ベクトル[X軸]をXZ平面での単位ベクトルに変換
    float cameraRightX = cameraRight.x;
    float cameraRightZ = cameraRight.z;
    float cameraRightLength = std::sqrtf(cameraRightX * cameraRightX + cameraRightZ * cameraRightZ);
    if (cameraRightLength > 0.0f)
    {
        //単位ベクトル化
        cameraRightX /= cameraRightLength;
        cameraRightZ /= cameraRightLength;
    }

    //カメラの前方向のベクトル[Z軸]をXZ単位ベクトルに変換
    float cameraFrontX = cameraFront.x;
    float cameraFrontZ = cameraFront.z;
    float cameraFrontLength = std::sqrtf(cameraFrontX * cameraFrontX + cameraFrontZ * cameraFrontZ);
    if (cameraFrontLength > 0.0f)
    {
        //単位ベクトル化
        cameraFrontX /= cameraFrontLength;
        cameraFrontZ /= cameraFrontLength;
    }

    //進行ベクトルを計算する
    DirectX::XMFLOAT3 vec;
    vec.x = (cameraRightX * ax) + (cameraFrontX * ay);
    vec.z = (cameraRightZ * ax) + (cameraFrontZ * ay);
    //Y軸方向には移動しない
    vec.y = 0.0f;

    return vec;
#endif
    return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
}

