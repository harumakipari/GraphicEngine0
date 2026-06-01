#pragma once
#include "Scene.h"
#include "SceneSetting.h"
#include "Components/Camera/CameraComponent.h"
#include "Game/Actors/Camera/Camera.h"

struct ActorTransformState
{
    std::string name;
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 rotation;
    DirectX::XMFLOAT3 scale;
};

struct CameraState
{
    DirectX::XMFLOAT3 position{ 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT4 rotation{ 0.0f,0.0f,0.0f,1.0f };
    float yaw = 0;
    float pitch = 0;
    float fov = 0;
};

struct SceneState
{
    SceneLightSaveData lightSaveData;
    SceneShaderConstants shader;
    CascadedShadowMapConstants cascadeShadow;
    FogConstants fog;
    SSRConstantBuffer ssr;
    SSAOConstantBuffer ssao;
    BloomConstantBuffer bloom;


    // カメラ情報
    CameraState camera;
    std::vector<CameraBookmark> cameraBookmarks;

    // Actor情報（Playerとか）
    std::vector<ActorTransformState> actorStates;

    void Capture(Scene* scene)
    {
        auto& s = scene->GetSceneSettings();

        lightSaveData = s.sceneLightSaveData;
        shader = s.sceneShaderConstants;
        fog = s.fogConstants;
        ssr = s.ssrConstantBuffer;
        ssao = s.ssaoConstantBuffer;
        bloom = s.bloomConstantBuffer;
        cascadeShadow = s.cascadedShadowMapConstants;

        // --- Player / Actor Transform ---
        actorStates.clear();
        for (auto& actor : scene->GetActorManager()->GetAllActors())
        {
            ActorTransformState ats;
            ats.name = actor->GetName();
            ats.position = actor->GetPosition();
            ats.rotation = actor->GetQuaternionRotation();
            ats.scale = actor->GetScale();
            actorStates.push_back(ats);
        }

        // --- Camera ---
        if (auto cam = scene->GetCameraManager()->GetRenderCamera(scene))
        {
            if (auto cinemaComp = dynamic_cast<CinematicCameraComponent*>(cam->GetCameraComponent()))
            {
                camera.position = cinemaComp->GetOwner()->GetPosition();
                camera.rotation = cinemaComp->GetOwner()->GetQuaternionRotation();
                camera.yaw = cinemaComp->GetYaw();
                camera.pitch = cinemaComp->GetPitch();

                camera.fov = cinemaComp->GetFov();

                // ブックマークも保存
                cameraBookmarks = cinemaComp->bookmarks;
            }
        }
    }

    void Apply(Scene* scene)
    {
        auto& s = scene->GetSceneSettings();

        // sceneConstantsは普通に上書き
        s.sceneLightSaveData.sceneConstants = lightSaveData.sceneConstants;

        auto backupShared = s.sceneLightSaveData.sharedLights;

        s.sceneLightSaveData = lightSaveData;

        // 空なら元を維持
        if (lightSaveData.sharedLights.empty())
        {
            s.sceneLightSaveData.sharedLights = backupShared;
        }
        s.sceneShaderConstants = shader;
        s.fogConstants = fog;
        s.ssrConstantBuffer = ssr;
        s.ssaoConstantBuffer = ssao;
        s.bloomConstantBuffer = bloom;
        s.cascadedShadowMapConstants = cascadeShadow;

        // --- Actor Transform ---
        for (auto& ats : actorStates)
        {
            auto actor = scene->GetActorManager()->GetActorByName(ats.name);
            if (actor)
            {
                actor->SetPosition(ats.position);
                actor->SetQuaternionRotation(ats.rotation);
                actor->SetScale(ats.scale);
            }
        }

        // --- Camera ---
        if (auto cam = scene->GetCameraManager()->GetRenderCamera(scene))
        {
            if (auto cinemaComp = dynamic_cast<CinematicCameraComponent*>(cam->GetCameraComponent()))
            {
                cinemaComp->GetOwner()->SetPosition(camera.position);
                cinemaComp->GetOwner()->SetQuaternionRotation(camera.rotation);
                cinemaComp->SetFov(camera.fov);
                // ブックマークも復元
                cinemaComp->SetYaw(camera.yaw);
                cinemaComp->SetPitch(camera.pitch);

                cinemaComp->UpdateRotationFromYawPitch();
                cinemaComp->bookmarks = cameraBookmarks;
            }
        }
    }
};

