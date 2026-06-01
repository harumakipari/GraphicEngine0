#pragma once
#include "Cherry.h"
#include "Core/Actor.h"

#include "Components/Effect/ParticleComponent.h"
#include "Components/Elastic/ElasticComponent.h"
#include "Engine/Audio/CoreAudio.h"
#include "Engine/Input/InputSystem.h"
#include "Physics/CollisionFunction.h"

class Pudding : public Actor
{
public:
    Pudding(const std::string& modelName) :Actor(modelName)
    {
    }
    std::shared_ptr<ElasticMeshComponent> elasticBuilding;
    std::shared_ptr<SkeletalMeshComponent> cherry;
    std::shared_ptr<SkeletalMeshComponent> whip;
    std::shared_ptr<ParticleComponent> particleComponent;
    std::shared_ptr<CoreAudioSourceComponent> audioSourceComponent;

    void Initialize(const Transform& transform)override
    {
        // 描画用コンポーネントを追加
        elasticBuilding = this->AddComponent<ElasticMeshComponent>("elasticBuilding");
        //elasticBuilding->SetModel("./Data/Models/pink_pudding/scene.gltf");
        //elasticBuilding->SetModel("./Data/Models/cherry_pudding/scene.gltf");
        elasticBuilding->SetModel("./Data/TeamModels/Enemy/YarnBigEnemy.glb");
        //elasticBuilding->SetModel("./Data/Models/cherry_pudding/pudding.glb");
        //elasticBuilding->SetModel("./Data/Models/Pudding/pudding_noCherry.glb");

        std::shared_ptr<BoxComponent> boxComponent = this->AddComponent<class BoxComponent>("boxComponent", "elasticBuilding");
        DirectX::XMFLOAT3 size = elasticBuilding->GetModelSize();
        boxComponent->SetBoxExtent({ size.x * 0.5f,size.y * 0.5f,size.z * 0.5f });
        boxComponent->SetMass(40.0f);
        boxComponent->SetLayer(CollisionLayer::Enemy);
        //boxComponent->SetCollisionOffsetY(size.y * 0.5f);
        boxComponent->SetResponseToLayer(CollisionLayer::Player, CollisionComponent::CollisionResponse::Block);
        boxComponent->SetResponseToLayer(CollisionLayer::WorldStatic, CollisionComponent::CollisionResponse::Block);
        boxComponent->Initialize();

#if 0
        whip = this->AddComponent<SkeletalMeshComponent>("whip", "elasticBuilding");
        //whip->SetModel("./Data/Models/Pudding/whip.glb");
        whip->SetModel("./Data/Models/cherry_pudding/whip.glb");
        whip->SetRelativeLocationDirect({ 0.0f,size.y,0.0f });

        cherry = this->AddComponent<SkeletalMeshComponent>("cherry", "whip");
        //cherry->SetModel("./Data/Models/Pudding/cherry.glb");
        cherry->SetModel("./Data/Models/cherry_pudding/cherry.glb");

#endif // 0


        particleComponent = this->AddComponent<class ParticleComponent>("particleComponent", "elasticBuilding");
        particleComponent->Load("./Data/Effect/Files/heartTestEffect.json");
        //particleComponent->Load("./Data/Effect/Files/testEffect.json");

        SetPosition(transform.GetLocation());
        SetQuaternionRotation(transform.GetRotation());
        SetScale(transform.GetScale());

        elasticBuilding->Initialize();
        {
            arrowRoot = std::make_shared<UIImageComponent>("./Data/Textures/UI/triangle.png", "arrowGauge");
            arrowRoot->SetWorldPosition({ 50, 300 });
            arrowRoot->SetPivot({ 0.5f,0.5f });
            arrowRoot->SetSize({ 50, 50 });
            GetOwnerScene()->GetUIManager()->Add(arrowRoot);

#if 1
            for (int i = 1; i <= 6; ++i)
            {
                auto piece = std::make_shared<UIImageComponent>(
                    "./Data/Textures/UI/triangle.png", "ArrowPiece_" + std::to_string(i));

                piece->SetLocalPosition({ i * 40.0f ,0.0f });
                piece->SetPivot({ 0.5f,0.5f });
                piece->SetSize({ 50, 50 });
                piece->color = CoreColor::White;

                piece->SetParent(arrowRoot.get());
                GetOwnerScene()->GetUIManager()->Add(piece);
            }
#endif // 0

        }

        audioSourceComponent = AddComponent<CoreAudioSourceComponent>("audioComponent", "elasticBuilding");
        audioSourceComponent->SetSource(L"./Data/Sound/SE1/charge.wav");

    }
    void Update(float deltaTime)override
    {
        // ポーズ中はゲーム入力を一切受け付けない
        if (GetOwnerScene()->IsPaused())
        {
            return;
        }

        // UIがマウスを使っているならゲーム操作しない
        if (GetOwnerScene()->GetUIManager()->IsMouseCaptured())
        {
            return;
        }



        const auto& pull = elasticBuilding->GetPullInfo();

        if (InputSystem::GetInputState("MouseLeft"))
        {
#if 0
            audioSourceComponent->Play();
            if (audioSourceComponent && !audioSourceComponent->IsPlaying())
            {
                audioSourceComponent->Play();
            }
#else
            //CoreAudio::PlayOneShot(L"./Data/Sound/SE/pudding.wav");



#endif // 0
        }
        if (InputSystem::GetInputState("MouseLeft", InputStateMask::Release))
        {
            if (particleComponent)
            {
                particleComponent->Play();
                //CoreAudio::PlayOneShot(L"./Data/Sound/SE/pudding.wav");
            }
        }
        XMFLOAT3 surfacePos, tangent;
        elasticBuilding->GetSurfacePositionTangent(surfacePos, tangent);
        //whip->SetWorldLocationDirect(surfacePos);

        // ===== 回転 =====
        XMVECTOR yAxis = XMVector3Normalize(XMLoadFloat3(&tangent));

        // fallback（真上を向いてるとき用）
        XMVECTOR worldForward = XMVectorSet(0, 0, 1, 0);
        if (fabsf(XMVectorGetX(XMVector3Dot(yAxis, worldForward))) > 0.99f)
        {
            worldForward = XMVectorSet(0, 0, 1, 0);
        }

        // X軸 = forward × Y
        XMVECTOR xAxis = XMVector3Normalize(XMVector3Cross(yAxis, worldForward));

        // Z軸 = Y × X
        XMVECTOR zAxis = XMVector3Cross(xAxis, yAxis);

        // 回転行列
        XMMATRIX rotMatrix =
        {
            xAxis,
            yAxis,
            zAxis,
            XMVectorSet(0, 0, 0, 1)
        };

        XMVECTOR rotQuat = XMQuaternionRotationMatrix(rotMatrix);
        DirectX::XMFLOAT4 rot;
        XMStoreFloat4(&rot, rotQuat);
        //whip->SetRelativeRotationDirect(rot);

        // 逆方向
        XMVECTOR dir = -XMVector3Normalize(XMLoadFloat3(&tangent));

        // 上方向を少し足す（放物線っぽさ）
        XMVECTOR launchDir =
            XMVector3Normalize(dir + XMVectorSet(0, 0.5f, 0, 0));

        // ===== 引っ張り量 =====
        float pullAmount = pull.amount;
        //Logger::Log("cherry pull Amount" + std::to_string(pullAmount));


        // ちょい溜めると一気に強くなる
        float power = std::lerp(minPower, maxPower, pullAmount);

        XMVECTOR velocity = launchDir * power;

        XMFLOAT3 vel;
        XMStoreFloat3(&vel, velocity);

        if (InputSystem::GetInputState("MouseLeft", InputStateMask::Release))
        {
            if (cherry)
            {
                cherry->SetIsVisible(false);
                XMFLOAT3 pos = cherry->GetComponentLocation();
                Transform cherryTr{ pos,{0.0f,0.0f,0.0f},{1.0f,1.0f,1.0f} };
                auto flyingCherry = GetOwnerScene()->GetActorManager()->CreateAndRegisterActorWithTransform<Cherry>("cherry", cherryTr);
                if (flyingCherry.get())
                {
                    flyingCherry->Launch(surfacePos, vel);
                }
            }
        }

        XMFLOAT3 launch;
        XMStoreFloat3(&launch, launchDir);

        // XZ 平面に落とす
        XMFLOAT2 dir2D = { launch.x, launch.z };

        // normalize
        float len = sqrtf(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
        if (len > 0.0001f)
        {
            dir2D.x /= len;
            dir2D.y /= len;
        }

        // UI角度（右=0°, 下=+）
        float angleRad = atan2f(-dir2D.y, dir2D.x);
        float angleDeg = DirectX::XMConvertToDegrees(angleRad);


        if (pull.active)
        {
            arrowRoot->SetVisible(true);

            // === ワールド → スクリーン ===
            XMFLOAT3 worldTop = surfacePos;

            XMFLOAT2 uiPos = WorldToUI(worldTop);

            arrowRoot->SetWorldPosition({ uiPos.x, uiPos.y });

            arrowRoot->SetWorldAngleDegree(angleDeg);
            //arrowGauge->value = pull.amount;
        }
        else
        {
            arrowRoot->SetVisible(false);
        }

    }
    void DrawImGuiDetails()
    {
#ifdef USE_IMGUI
        ImGui::DragFloat(U8("サクランボの最小速度"), &minPower, 0.02f);
        ImGui::DragFloat(U8("サクランボの最大速度"), &maxPower, 0.02f);
        if (ImGui::Button(U8("サクランボが乗った時")))
        {
            elasticBuilding->AddImpulse({ 0.0f,-0.5f,0.0f });
        }
#endif
    };

private:
    float minPower = 2.0f;
    float maxPower = 10.0f;

    std::shared_ptr<UIImageComponent> arrowRoot;
};
