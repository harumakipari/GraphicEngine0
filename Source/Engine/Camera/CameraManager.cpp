#include "pch.h"
#include "CameraManager.h"

#include "Engine/Scene/Scene.h"
#include "Game/Actors/Camera/Camera.h"
#include "Engine/Utility/Time.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace
{
    struct ShakePreset
    {
        float intensity, duration, frequency, position, target;
    };
    const ShakePreset* FindPreset(const std::string& n)
    {
        static const ShakePreset
            roar{ .35f,2.45f,7.f,.010f,.060f },
            wall{ 1.15f,.36f,10.f,.05f,.22f },
            rush{ .9f,.16f,18.f,.02f,.14f },
            weaponDrop{ 0.35f, 0.30f, 8.0f, 0.015f, 0.060f };

        if (n == "BossRoar")
            return &roar;
        if (n == "BossWallImpact")
            return &wall;
        if (n == "RushFinal")
            return &rush;
        if (n == "BossWeaponDrop")
            return &weaponDrop;
        return nullptr;
    }
}

Camera* CameraManager::GetRenderCamera(const Scene* scene) const
{
    if (useDebugCamera)
    {
        if (auto dbg = debugCamera.lock())
            return dbg.get();
    }

    if (useCinematicCamera)
    {
        if (auto cinema = cinematicCamera.lock())
            return cinema.get();
    }

    if (useMovieCamera)
    {
        if (auto movieCam = movieCamera.lock())
            return movieCam.get();
    }

    if (auto cam = scene->GetActiveCamera())
        return cam;

    Logger::Error(Logger::LogCategory::System, "Camera is nullptr");
    return nullptr;
}

void CameraManager::PlayCameraShake(float i, float d, float f, float p, float t, const std::string& n) { shakeIntensity = (std::max)(i, 0.f); shakeDuration = (std::max)(d, 0.f); shakeFrequency = (std::max)(f, 0.f); shakePositionAmount = (std::max)(p, 0.f); shakeTargetAmount = (std::max)(t, 0.f); shakeElapsed = 0.f; shakePositionSide = shakePositionVertical = shakeTargetYaw = shakeTargetPitch = 0.f; shakePresetName = n; shakeActive = shakeIntensity > 0.f && shakeDuration > FLT_EPSILON; }
void CameraManager::PlayCameraShakePreset(const std::string& n) { const auto* p = FindPreset(n); if (!p) { Logger::Warning(Logger::LogCategory::Gameplay, "[CameraShake] Unknown preset"); return; }PlayCameraShake(p->intensity, p->duration, p->frequency, p->position, p->target, n); }
void CameraManager::ClearCameraShake() { shakeActive = false; shakeElapsed = shakeDuration = 0.f; shakePositionSide = shakePositionVertical = shakeTargetYaw = shakeTargetPitch = 0.f; shakePresetName.clear(); }
void CameraManager::Update(float dt) { if (!shakeActive)return; if (shakeElapsed >= shakeDuration) { ClearCameraShake(); return; }const float n = std::clamp(shakeElapsed / shakeDuration, 0.f, 1.f), e = (1.f - n) * (1.f - n), ph = shakeElapsed * shakeFrequency * DirectX::XM_2PI; shakePositionVertical = ((sinf(ph) + .35f * sinf(ph * 2.17f + 1.1f)) / 1.35f) * shakePositionAmount * shakeIntensity * e; shakePositionSide = ((cosf(ph * 1.37f + .4f) + .25f * sinf(ph * .73f + 2.f)) / 1.25f) * shakePositionAmount * shakeIntensity * e * .35f; shakeTargetPitch = ((sinf(ph * .73f + 1.7f) + .4f * cosf(ph * 1.91f)) / 1.4f) * shakeTargetAmount * shakeIntensity * e; shakeTargetYaw = ((cosf(ph * 1.13f + .8f) + .3f * sinf(ph * 2.31f)) / 1.3f) * shakeTargetAmount * shakeIntensity * e * .45f; shakeElapsed = (std::min)(shakeElapsed + ((useCinematicCamera || useMovieCamera) ? (std::max)(Time::UnscaledDeltaTime(), 0.f) : (std::max)(dt, 0.f)), shakeDuration); }
ViewConstants CameraManager::GetRenderViewConstants(const Scene* s) { Camera* c = GetRenderCamera(s); if (!c)return{}; ViewConstants r = c->GetViewConstants(); if (!shakeActive)return r; auto* cc = c->GetCameraComponent(); if (!cc)return r; using namespace DirectX; XMFLOAT3 eye = cc->GetComponentLocation(), f = cc->GetForward(), up{ 0,1,0 }, right = cc->GetRight(); XMVECTOR fe = XMLoadFloat3(&eye) + XMLoadFloat3(&up) * shakePositionVertical + XMLoadFloat3(&right) * shakePositionSide; XMVECTOR tar = (cc->useLookTarget ? XMLoadFloat3(&cc->lookTarget) : XMLoadFloat3(&eye) + XMLoadFloat3(&f)) + XMLoadFloat3(&up) * shakeTargetPitch + XMLoadFloat3(&right) * shakeTargetYaw; XMMATRIX v = XMMatrixLookAtLH(fe, tar, XMLoadFloat3(&up)), p = XMLoadFloat4x4(&r.projection); XMStoreFloat4x4(&r.view, v); XMStoreFloat4x4(&r.viewProjection, v * p); XMStoreFloat4x4(&r.invView, XMMatrixInverse(nullptr, v)); XMStoreFloat4x4(&r.invViewProjection, XMMatrixInverse(nullptr, v * p)); XMStoreFloat4(&r.cameraPosition, fe); return r; }
#ifdef USE_IMGUI
void CameraManager::DrawImGuiDetails(const Scene* s) const { if (!ImGui::CollapsingHeader("Camera Shake Runtime"))return; auto* c = GetRenderCamera(s); ImGui::Text("Active: %s", shakeActive ? "true" : "false"); ImGui::Text("Preset: %s", shakePresetName.empty() ? "(custom)" : shakePresetName.c_str()); ImGui::Text("Elapsed / Duration: %.3f / %.3f", shakeElapsed, shakeDuration); ImGui::Text("Position Offset: (%.4f, %.4f)", shakePositionSide, shakePositionVertical); ImGui::Text("Target Offset: (%.4f, %.4f)", shakeTargetYaw, shakeTargetPitch); ImGui::Text("Render Camera: %s", c ? c->GetName().c_str() : "(none)"); ImGui::Text("Cinematic Camera Active: %s", useCinematicCamera ? "true" : "false"); }
#endif

void CameraManager::ToggleCamera(const Scene* scene)
{
    useDebugCamera = !useDebugCamera;
    if (useDebugCamera)
    {
        if (const auto dbg = debugCamera.lock())
        {
            if (auto cam = scene->GetActiveCamera())
            {
                dbg->SetPosition(cam->GetPosition());
                dbg->SetQuaternionRotation(cam->GetQuaternionRotation());
                auto cameraCom = cam->GetCameraComponent();
                float fov = cameraCom->GetFov();
                float pitch = cameraCom->GetPitch();
                float yaw = cameraCom->GetYaw();

                if (auto dbgComp = dynamic_cast <DebugCameraComponent*>(dbg->GetCameraComponent()))
                {
                    dbgComp->SetIsUseDebug(useDebugCamera);
                    dbgComp->SetFov(fov);
                    dbgComp->SetYawAndPitch(yaw, pitch);

                }
                //dbg->SetQuaternionRotation({0.0f,0.0f,0.0f,1.0f});
                //CameraState caneraState=
                //{
                //   cam->GetPosition(),
                //    {0.0f,0.0f,0.0f},
                //     DirectX::XMConvertToRadians(30.0f)
                //};
                //dbg->GetCameraComponent()->SetState(caneraState);
            }
        }
    }
}

void CameraManager::ToggleCinematicCamera(const Scene* scene)
{
    useCinematicCamera = !useCinematicCamera;
    if (useCinematicCamera)
    {
        if (const auto cinemaCamera = cinematicCamera.lock())
        {
            if (auto cam = scene->GetActiveCamera())
            {
                cinemaCamera->SetPosition(cam->GetPosition());
                cinemaCamera->SetQuaternionRotation(cam->GetQuaternionRotation());
                auto cameraCom = cam->GetCameraComponent();
                float fov = cameraCom->GetFov();
                float pitch = cameraCom->GetPitch();
                float yaw = cameraCom->GetYaw();

                if (auto cinemaComp = dynamic_cast <CinematicCameraComponent*>(cinemaCamera->GetCameraComponent()))
                {
                    cinemaComp->SetIsUseCinematic(useCinematicCamera);
                    cinemaComp->SetFov(fov);
                    cinemaComp->SetYawAndPitch(yaw, pitch);

                }
            }
        }
    }
}


void CameraManager::ToggleMovieCamera(const Scene* scene)
{
    useMovieCamera = !useMovieCamera;
    if (useMovieCamera)
    {
        if (const auto movieCam = movieCamera.lock())
        {
            if (auto cam = scene->GetActiveCamera())
            {
                movieCam->SetPosition(cam->GetPosition());
                movieCam->SetQuaternionRotation(cam->GetQuaternionRotation());
                auto cameraCom = cam->GetCameraComponent();
                float fov = cameraCom->GetFov();
                float pitch = cameraCom->GetPitch();
                float yaw = cameraCom->GetYaw();

                if (auto movieComp = dynamic_cast <MovieCameraComponent*>(movieCam->GetCameraComponent()))
                {
                    movieComp->SetIsUseMovie(useMovieCamera);
                    movieComp->SetFov(fov);
                    movieComp->SetYawAndPitch(yaw, pitch);


                }
            }
        }
    }
}
