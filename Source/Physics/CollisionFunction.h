#pragma once
#include <DirectXMath.h>

#include "Physics.h"
#include "Engine/Camera/CameraConstants.h"
#include "Engine/Camera/CameraManager.h"
#include "Engine/Scene/Scene.h"
#include "Game/Actors/Camera/Camera.h"
#include "Graphics/Core/Graphics.h"

namespace CollisionFunction
{
    inline bool SphereRayCast(
        const DirectX::XMFLOAT3& rayStart,
        const DirectX::XMFLOAT3& rayEnd,
        HitResultWithActor& result,
        float radius = 0.3f,
        uint32_t collisionLayer = 0xFFFFFF)
    {
        using namespace DirectX;

        XMVECTOR start = XMLoadFloat3(&rayStart);
        XMVECTOR end = XMLoadFloat3(&rayEnd);

        XMVECTOR dirVec = XMVectorSubtract(end, start);

        float distance = XMVectorGetX(XMVector3Length(dirVec));

        if (distance <= 0.0001f)
            return false;

        dirVec = XMVector3Normalize(dirVec);

        XMFLOAT3 rayDir;
        XMStoreFloat3(&rayDir, dirVec);

        return Physics::Instance().SphereCast(
            rayStart,
            rayDir,
            distance,
            radius,
            result,
            collisionLayer);
    }

    inline bool RaycastFromMouse(const DirectX::XMFLOAT2& mouseCursor, HitResultWithActor& result, uint32_t collisionLayer = 0xFFFFFF)
    {
        float screenWidth, screenHeight, viewportX, viewportY;
        Graphics::GetViewport(viewportX, viewportY, screenWidth, screenHeight);

        // スクリーン座標の設定
        DirectX::XMVECTOR ScreenPosition, WorldPosition;
        DirectX::XMFLOAT3 screenPosition;
        screenPosition.x = static_cast<float>(mouseCursor.x);
        screenPosition.y = static_cast<float>(mouseCursor.y);
        screenPosition.z = 0.0f;
        ScreenPosition = DirectX::XMLoadFloat3(&screenPosition);

        auto scene = Scene::GetCurrentScene();
        if (!scene)
        {
            Logger::Warning(Logger::LogCategory::System, U8("RaycastFromMouse で　scene が null です！"));
            return false;
        }
        auto camera = scene->GetCameraManager()->GetRenderCamera(scene);
        ViewConstants data;
        

        if (camera)
        {
            data = camera->GetViewConstants();
            //各行列を取得
            DirectX::XMMATRIX View = DirectX::XMLoadFloat4x4(&data.view);
            DirectX::XMMATRIX Projection = DirectX::XMLoadFloat4x4(&data.projection);
            DirectX::XMMATRIX World = DirectX::XMMatrixIdentity();
            // スクリーン座標をワールド座標に変換し、レイの始点を求める
            WorldPosition = DirectX::XMVector3Unproject(ScreenPosition, 0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f, Projection, View, World);

            DirectX::XMFLOAT3 rayStart;
            DirectX::XMStoreFloat3(&rayStart, WorldPosition);

            // スクリーン座標をワールド座標に変換し、レイの終点を求める
            screenPosition.x = static_cast<float>(mouseCursor.x);
            screenPosition.y = static_cast<float>(mouseCursor.y);
            screenPosition.z = 1.0f;
            ScreenPosition = DirectX::XMLoadFloat3(&screenPosition);
            WorldPosition = DirectX::XMVector3Unproject(
                ScreenPosition, 0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f, Projection, View, World
            );
            DirectX::XMFLOAT3 rayEnd;
            DirectX::XMStoreFloat3(&rayEnd, WorldPosition);
            DirectX::XMVECTOR RayDir = DirectX::XMVectorSubtract(XMLoadFloat3(&rayEnd), XMLoadFloat3(&rayStart));
            float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(RayDir));
            RayDir = DirectX::XMVector3Normalize(RayDir);
            DirectX::XMFLOAT3 rayDir;
            DirectX::XMStoreFloat3(&rayDir, RayDir);

            if (Physics::Instance().SphereCast(rayStart, rayDir, FLT_MAX, 0.001f, result, collisionLayer))
            {
                return true;
            }
            else
            {
                return false;
            }

        }
        return false;
    }

    inline XMFLOAT2 GetScreenPositionFromWorldPosition(const XMFLOAT3& worldPosition)
    {
        float screenWidth, screenHeight, viewportX, viewportY;
        Graphics::GetViewport(viewportX, viewportY, screenWidth, screenHeight);
        DirectX::XMVECTOR WorldPosition = DirectX::XMLoadFloat3(&worldPosition);
        auto scene = Scene::GetCurrentScene();
        if (!scene)
        {
            Logger::Warning(Logger::LogCategory::System, U8("GetScreenPositionFromWorldPosition で　scene が null です！"));
            return { 0.0f,0.0f };
        }
        auto camera = scene->GetCameraManager()->GetRenderCamera(scene);

        //auto camera = CameraManager::GetRenderCamera(dynamic_cast<SceneBase*>(Scene::GetCurrentScene()));

        ViewConstants data;
        if (camera)
            data= camera->GetViewConstants();
        //各行列を取得
        DirectX::XMMATRIX View = DirectX::XMLoadFloat4x4(&data.view);
        DirectX::XMMATRIX Projection = DirectX::XMLoadFloat4x4(&data.projection);
        DirectX::XMMATRIX World = DirectX::XMMatrixIdentity();
        // ワールド座標をスクリーン座標に変換
        DirectX::XMVECTOR ScreenPosition = DirectX::XMVector3Project(
            WorldPosition, viewportX, viewportY, screenWidth, screenHeight, 0.0f, 1.0f, Projection, View, World
        );
        DirectX::XMFLOAT3 screenPos;
        DirectX::XMStoreFloat3(&screenPos, ScreenPosition);
        return XMFLOAT2(screenPos.x, screenPos.y);
    }

}
inline XMFLOAT2 ConvertScreenToUI(const XMFLOAT2& screen)
{
    float vx, vy, vw, vh;
    Graphics::GetViewport(vx, vy, vw, vh);

    constexpr float DESIGN_W = 1920.0f;
    constexpr float DESIGN_H = 1080.0f;

    float scaleX = vw / DESIGN_W;
    float scaleY = vh / DESIGN_H;
    float scale = std::min<float>(scaleX, scaleY);

    float offsetX = (vw - DESIGN_W * scale) * 0.5f;
    float offsetY = (vh - DESIGN_H * scale) * 0.5f;

    XMFLOAT2 ui;
    ui.x = (screen.x - vx - offsetX) / scale;
    ui.y = (screen.y - vy - offsetY) / scale;
    return ui;
}
inline XMFLOAT2 WorldToUI(const XMFLOAT3& worldPos)
{
    XMFLOAT2 screen =
        CollisionFunction::GetScreenPositionFromWorldPosition(worldPos);

    return ConvertScreenToUI(screen);
}

