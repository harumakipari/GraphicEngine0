#include "pch.h"
#include "GameLockOnTargetUIActor.h"
#include "Engine/Scene/Scene.h"
#include "Game/Actors/Camera/DarkGameCamera.h"
#include "Physics/CollisionFunction.h"
#include "UI/UIManager.h"

void GameLockOnTargetUIActor::Initialize(const Transform&)
{
    const auto createImage = [this](const char* texturePath, const char* name)
        {
            auto image = std::make_shared<UIImageComponent>(texturePath, name);
            image->SetPivot({ 0.5f, 0.5f });
            image->SetSize({ lockOnTargetUISize, lockOnTargetUISize });
            image->SetVisible(false);
            GetOwnerScene()->GetUIManager()->Add(image);
            return image;
        };

    lockOnTargetCenterImageComponent = createImage("./Data/Textures/UI/LockOn/lock_on_center.png", "LockOnTargetCenter");
    lockOnTargetTopImageComponent = createImage("./Data/Textures/UI/LockOn/lock_on_top.png", "LockOnTargetTop");
    lockOnTargetBottomImageComponent = createImage("./Data/Textures/UI/LockOn/lock_on_bottom.png", "LockOnTargetBottom");
    lockOnTargetLeftImageComponent = createImage("./Data/Textures/UI/LockOn/lock_on_left.png", "LockOnTargetLeft");
    lockOnTargetRightImageComponent = createImage("./Data/Textures/UI/LockOn/lock_on_right.png", "LockOnTargetRight");
}
void GameLockOnTargetUIActor::Finalize()
{
    HideMarker();
    for (const auto& image : { lockOnTargetCenterImageComponent, lockOnTargetTopImageComponent,
        lockOnTargetBottomImageComponent, lockOnTargetLeftImageComponent, lockOnTargetRightImageComponent })
        if (image) image->MarkPendingKill();
    lockOnTargetCenterImageComponent.reset();
    lockOnTargetTopImageComponent.reset();
    lockOnTargetBottomImageComponent.reset();
    lockOnTargetLeftImageComponent.reset();
    lockOnTargetRightImageComponent.reset();
}

void GameLockOnTargetUIActor::HideMarker()
{
    const std::array<std::shared_ptr<UIImageComponent>, 5> images =
    {
        lockOnTargetCenterImageComponent, lockOnTargetTopImageComponent,
        lockOnTargetBottomImageComponent, lockOnTargetLeftImageComponent,
        lockOnTargetRightImageComponent
    };
    for (const auto& image : images)
    {
        if (image)
            image->SetVisible(false);
    }
    lockOnTargetComponent.reset();
    lockOnTargetUIAnimationPhase = LockOnTargetUIAnimationPhase::Hidden;
    lockOnTargetUIAnimationElapsed = 0.0f;
    lockOnTargetUIRotationDegree = 0.0f;
}
void GameLockOnTargetUIActor::UpdateMarkerPresentation(const DarkCameraActor& camera,
    const std::shared_ptr<SceneComponent>& target, const DirectX::XMFLOAT3& worldPosition,
    bool isGrux, float deltaTime)
{
    if (lockOnTargetComponent.lock() != target)
    {
        lockOnTargetComponent = target;
        lockOnTargetUIAnimationPhase = LockOnTargetUIAnimationPhase::Gathering;
        lockOnTargetUIAnimationElapsed = 0.0f;
        lockOnTargetUIRotationDegree = 0.0f;
    }

    const float safeDeltaTime = (std::max)(deltaTime, 0.0f);
    switch (lockOnTargetUIAnimationPhase)
    {
    case LockOnTargetUIAnimationPhase::Gathering:
        lockOnTargetUIAnimationElapsed += safeDeltaTime;
        if (lockOnTargetUIAnimationElapsed >= (std::max)(lockOnTargetUIGatherDuration, 0.0f))
        {
            lockOnTargetUIAnimationPhase = LockOnTargetUIAnimationPhase::Hold;
            lockOnTargetUIAnimationElapsed = 0.0f;
        }
        break;
    case LockOnTargetUIAnimationPhase::Hold:
        lockOnTargetUIAnimationElapsed += safeDeltaTime;
        if (lockOnTargetUIAnimationElapsed >= (std::max)(lockOnTargetUIHoldDuration, 0.0f))
        {
            lockOnTargetUIAnimationPhase = LockOnTargetUIAnimationPhase::Rotating;
            lockOnTargetUIAnimationElapsed = 0.0f;
        }
        break;
    case LockOnTargetUIAnimationPhase::Rotating:
        lockOnTargetUIRotationDegree += lockOnTargetUIRotationSpeedDegree * safeDeltaTime;
        if (lockOnTargetUIRotationDegree >= 360.0f || lockOnTargetUIRotationDegree <= -360.0f)
            lockOnTargetUIRotationDegree = std::fmod(lockOnTargetUIRotationDegree, 360.0f);
        break;
    default:
        break;
    }

    const auto projection = camera.ProjectWorldPositionForUI(
        worldPosition);
    if (!projection.valid || !projection.inFront || !projection.insideViewport)
    {
        const std::array<std::shared_ptr<UIImageComponent>, 5> images =
        {
            lockOnTargetCenterImageComponent, lockOnTargetTopImageComponent,
            lockOnTargetBottomImageComponent, lockOnTargetLeftImageComponent,
            lockOnTargetRightImageComponent
        };
        for (const auto& image : images)
        {
            if (image)
                image->SetVisible(false);
        }
        return;
    }

    float targetOverallScale = skeletonLockOnTargetUIOverallScale;
    DirectX::XMFLOAT2 targetOverallOffset = skeletonLockOnTargetUIOverallOffset;
    if (isGrux)
    {
        targetOverallScale = gruxLockOnTargetUIOverallScale;
        targetOverallOffset = gruxLockOnTargetUIOverallOffset;
    }
    const float overallScale = std::clamp(targetOverallScale, 0.01f, 2.0f);
    const float scaledImageSize = lockOnTargetUISize * overallScale;
    DirectX::XMFLOAT2 center = ConvertScreenToUI(projection.screenPosition);
    center.x += targetOverallOffset.x;
    center.y += targetOverallOffset.y;
    float gatherRemaining = 0.0f;
    if (lockOnTargetUIAnimationPhase == LockOnTargetUIAnimationPhase::Gathering)
    {
        const float duration = (std::max)(lockOnTargetUIGatherDuration, 0.0001f);
        const float t = std::clamp(lockOnTargetUIAnimationElapsed / duration, 0.0f, 1.0f);
        const float easeOutCubic = 1.0f - std::pow(1.0f - t, 3.0f);
        gatherRemaining = lockOnTargetUIStartOffset * (1.0f - easeOutCubic);
    }
    const float ringAngle = lockOnTargetUIAnimationPhase == LockOnTargetUIAnimationPhase::Rotating
        ? lockOnTargetUIRotationDegree : 0.0f;

    lockOnTargetCenterImageComponent->SetSize({ scaledImageSize, scaledImageSize });
    lockOnTargetCenterImageComponent->SetWorldPosition(center);
    lockOnTargetCenterImageComponent->SetWorldAngleDegree(0.0f);
    lockOnTargetCenterImageComponent->SetVisible(true);

    const std::array<std::shared_ptr<UIImageComponent>, 4> rings =
    {
        lockOnTargetTopImageComponent, lockOnTargetBottomImageComponent,
        lockOnTargetLeftImageComponent, lockOnTargetRightImageComponent
    };
    const std::array<DirectX::XMFLOAT2, 4> directions =
    {
        DirectX::XMFLOAT2{ 0.0f, -1.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f },
        DirectX::XMFLOAT2{ -1.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f }
    };
    const std::array<DirectX::XMFLOAT2, 4> finalOffsets =
    {
        lockOnTargetUITopFinalOffset, lockOnTargetUIBottomFinalOffset,
        lockOnTargetUILeftFinalOffset, lockOnTargetUIRightFinalOffset
    };
    const float radians = DirectX::XMConvertToRadians(ringAngle);
    const float cosAngle = std::cos(radians);
    const float sinAngle = std::sin(radians);
    for (size_t i = 0; i < rings.size(); ++i)
    {
        const auto& ring = rings[i];
        if (!ring)
            continue;

        ring->SetSize({ scaledImageSize, scaledImageSize });
        DirectX::XMFLOAT2 offset =
        {
            (finalOffsets[i].x + directions[i].x * gatherRemaining) * overallScale,
            (finalOffsets[i].y + directions[i].y * gatherRemaining) * overallScale
        };
        if (lockOnTargetUIAnimationPhase == LockOnTargetUIAnimationPhase::Rotating)
        {
            // UI Y grows downward, so positive degrees rotate clockwise visually.
            offset =
            {
                offset.x * cosAngle - offset.y * sinAngle,
                offset.x * sinAngle + offset.y * cosAngle
            };
        }
        ring->SetWorldPosition({ center.x + offset.x, center.y + offset.y });
        ring->SetWorldAngleDegree(ringAngle);
        ring->SetVisible(true);
    }
}
void GameLockOnTargetUIActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::SeparatorText("LockOn Target UI");
    ImGui::DragFloat("LockOn Gather Start Offset", &lockOnTargetUIStartOffset, 0.5f, 0.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("LockOn Gather Duration", &lockOnTargetUIGatherDuration, 0.01f, 0.0f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("LockOn Hold Duration", &lockOnTargetUIHoldDuration, 0.01f, 0.0f, 3.0f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("LockOn Ring Rotation Speed", &lockOnTargetUIRotationSpeedDegree, 1.0f, -360.0f, 360.0f, "%.1f deg/sec", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SeparatorText("LockOn Target Overall Transform");
    ImGui::DragFloat("Skeleton LockOn Overall Scale", &skeletonLockOnTargetUIOverallScale, 0.01f, 0.01f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Skeleton LockOn Overall Offset X / Y", &skeletonLockOnTargetUIOverallOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Grux LockOn Overall Scale", &gruxLockOnTargetUIOverallScale, 0.01f, 0.01f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Grux LockOn Overall Offset X / Y", &gruxLockOnTargetUIOverallOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextDisabled("Final Offset: +Y is down in UI screen coordinates.");
    ImGui::DragFloat2("Top Final Offset X / Y", &lockOnTargetUITopFinalOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Bottom Final Offset X / Y", &lockOnTargetUIBottomFinalOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Left Final Offset X / Y", &lockOnTargetUILeftFinalOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Right Final Offset X / Y", &lockOnTargetUIRightFinalOffset.x, 0.5f, -200.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
#endif
}