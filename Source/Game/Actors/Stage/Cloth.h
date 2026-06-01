#pragma once

#include <wrl.h>
#include <d3d11.h>
#include <directxmath.h>

#include "Core/Actor.h"
#include "Graphics/Core/ConstantBuffer.h"
#include "Components/CollisionShape/ShapeComponent.h"
#include "Components/Render/MeshComponent.h"

#include "Engine/Utility/Win32Utils.h"
#include "Engine/Input/InputSystem.h"

struct ClothVertex
{
    DirectX::XMFLOAT4 position;
    DirectX::XMFLOAT4 normal;
    DirectX::XMFLOAT4 tangent;
    DirectX::XMFLOAT4 texcoord;

    DirectX::XMFLOAT4 oldVelocity;
    DirectX::XMFLOAT4 velocity;
    DirectX::XMFLOAT4 newPosition;
};
struct ClothEdge
{
    uint32_t id = 0xffffff;
    float restLength = 0.0f;
    DirectX::XMFLOAT3 delta;
};


class SphereTest :public Actor
{
public:
    SphereTest(const std::string& modelName) :Actor(modelName)
    {
    }
    std::shared_ptr<SkeletalMeshComponent> skeltalMeshComponent;
    void Initialize(const Transform& transform)override
    {
        // 描画用コンポーネントを追加
        skeltalMeshComponent = this->AddComponent<class SkeletalMeshComponent>("skeltalComponent");
        skeltalMeshComponent->SetModel("./Data/Models/Primitives/sphere.glb", false);

        SetPosition(transform.GetLocation());
        SetQuaternionRotation(transform.GetRotation());
        SetScale(transform.GetScale());

        sphereCBuffer = std::make_unique<ConstantBuffer<SphereCBuffer>>(Graphics::GetDevice());

    }

    void Update(float deltaTime)override
    {
        using namespace DirectX;

        XMFLOAT3 pos = GetPosition();

        if (flag)
        {
            pos.y += deltaTime * -9.8f*0.1f;
        }
        else
        {
            pos.y = 10.0f;
        }
        //if (pos.y < 3.5f)
        //{
        //    pos.y = 3.5f;
        //}

        SetPosition(pos);

        if (onFloor)
        {
            pos.y = 1.0f;
            SetPosition(pos);
        }

        // flag がtrueのときだけ動く


        sphereCBuffer->data.radius = 1.1f;
        sphereCBuffer->data.worldPos = GetPosition();
        DirectX::XMFLOAT4X4 world = GetWorldTransform();
        XMMATRIX World = XMLoadFloat4x4(&world);
        XMMATRIX invWorldM = XMMatrixInverse(nullptr, World);
        DirectX::XMStoreFloat4x4(&sphereCBuffer->data.invWorldTransform, invWorldM);
        DirectX::XMStoreFloat4x4(&sphereCBuffer->data.worldTransform, World);
        sphereCBuffer->Activate(Graphics::GetDeviceContext(), 7);

        //clothMesh->Simulate(Graphics::GetDeviceContext());
    }

    struct SphereCBuffer
    {
        DirectX::XMFLOAT3 worldPos;
        float radius = 2.0f;
        DirectX::XMFLOAT4X4 worldTransform;
        DirectX::XMFLOAT4X4 invWorldTransform;
    };

    void DrawImGuiDetails()
    {
#ifdef USE_IMGUI
        ImGui::Checkbox("flag", &flag);
        ImGui::Checkbox("onFloor", &onFloor);
#endif
    };
    bool flag = false;
    bool onFloor = true;
private:
    std::unique_ptr<ConstantBuffer<SphereCBuffer>> sphereCBuffer;

};


class PlaneTest :public Actor
{
public:
    PlaneTest(const std::string& modelName) :Actor(modelName)
    {
    }
    std::shared_ptr<SkeletalMeshComponent> skeltalMeshComponent;
    std::shared_ptr<SkeletalMeshComponent> skeltalMeshComponent1;
    void Initialize(const Transform& transform)override
    {
        // 描画用コンポーネントを追加
        skeltalMeshComponent = this->AddComponent<class SkeletalMeshComponent>("skeltalComponent");
        skeltalMeshComponent->SetModel("./Data/Models/TestCloth/cloth1.gltf", false);

        skeltalMeshComponent1 = this->AddComponent<class SkeletalMeshComponent>("skeltalComponent");
        skeltalMeshComponent1->SetModel("./Data/Models/TestCloth/cloth1.gltf", false);

        SetPosition(transform.GetLocation());
        SetQuaternionRotation(transform.GetRotation());
        SetScale(transform.GetScale());
        planeCBuffer = std::make_unique<ConstantBuffer<PlaneCBuffer>>(Graphics::GetDevice());

    }

    void Update(float deltaTime)override
    {
        using namespace DirectX;

        Plane plane1;
        plane1.normal = { 0.0f,1.0f,0.0f };
        plane1.d = 1.0f;

        Plane plane2;
        plane2.normal = { 0.0f,1.0f,0.0f };
        plane2.d = 0.0f;
        planeCBuffer->data.plane[1] = plane2;

        float angle = XMConvertToRadians(-30.0f);
        // Z軸回転行列
        XMMATRIX rotMatrix = XMMatrixRotationZ(angle);
        XMVECTOR n = XMLoadFloat3(&plane1.normal);
        n = XMVector3TransformNormal(n, rotMatrix);
        XMStoreFloat3(&plane1.normal, n);
        planeCBuffer->data.plane[0] = plane1;


        XMFLOAT3 pos1 = { plane1.normal.x * plane1.d, plane1.normal.y * plane1.d, plane1.normal.z * plane1.d };


        XMVECTOR base = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR normal = XMLoadFloat3(&plane1.normal);
        normal = XMVector3Normalize(normal);

        XMVECTOR axis = XMVector3Cross(base, normal);
        float dot = XMVectorGetX(XMVector3Dot(base, normal));
        float normalLength = XMVectorGetX(XMVector3Length(axis));

        XMVECTOR quat;
        if (normalLength < 1e-6f)
        {
            quat = (dot < 0.0f) ? XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), XM_PI) : XMQuaternionIdentity();
        }
        else
        {
            float angle2 = acosf(dot);
            quat = XMQuaternionRotationAxis(axis, angle2);
        }
        XMFLOAT4 quatF;
        XMStoreFloat4(&quatF, quat);
        skeltalMeshComponent->SetWorldLocationDirect(pos1);
        skeltalMeshComponent->SetWorldRotationDirect(quatF);
        planeCBuffer->Activate(Graphics::GetDeviceContext(), 6);
    }

    struct Plane
    {
        DirectX::XMFLOAT3 normal;
        float d;
    };

    struct PlaneCBuffer
    {
        Plane plane[4];
    };

    void DrawImGuiDetails()
    {
#ifdef USE_IMGUI
        ImGui::Checkbox("flag", &flag);
        ImGui::Checkbox("onFloor", &onFloor);
#endif
    };
    bool flag = false;
    bool onFloor = true;
private:
    std::unique_ptr<ConstantBuffer<PlaneCBuffer>> planeCBuffer;
};


