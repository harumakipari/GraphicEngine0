#include "pch.h"
#include "GameTutorialUIActor.h"
#include "Components/Render/MeshComponent.h"
#include "Engine/Scene/Scene.h"
#include "Game/Actors/Camera/DarkGameCamera.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/SkeletonWarriorEnemy.h"
#include "Physics/CollisionFunction.h"
#include "UI/UIManager.h"

void GameTutorialUIActor::Initialize(const Transform&)
{
    const auto uiManager = GetOwnerScene()->GetUIManager();
    const auto createPrompt = [uiManager](const char* path, const char* name, DirectX::XMFLOAT2 size)
    {
        auto image = std::make_shared<UIImageComponent>(path, name);
        image->SetSize(size);
        image->SetPivot({ 0.5f, 0.5f });
        image->SetVisible(false);
        image->zOrder = 50;
        uiManager->Add(image);
        return image;
    };
    ltPrompt = createPrompt("./Data/Textures/UI/operate_LT.png", "TutorialPassiveSkeletonLtPrompt", { 345.0f, 270.0f });
    yPrompt = createPrompt("./Data/Textures/UI/operate_Y.png", "TutorialPassiveSkeletonYPrompt", { 184.0f, 279.0f });
    xPrompt = createPrompt("./Data/Textures/UI/operate_X.png", "TutorialDodgeSkeletonXPrompt", { 184.0f, 279.0f });
    explanation = std::make_shared<UIImageComponent>("./Data/Textures/UI/judge_dodge_operation.png", "JudgeDodgeOperationTutorial");
    explanation->SetSize({ 2226.0f, 375.0f });
    explanation->SetPivot({ 0.5f, 0.5f });
    explanation->SetScale({ 0.25f, 0.25f });
    explanation->zOrder = 120;
    explanation->SetVisible(false);
    uiManager->Add(explanation);
    closeImage = std::make_shared<UIImageComponent>("./Data/Textures/UI/close_a.png", "JudgeDodgeOperationTutorialClose");
    closeImage->SetSize({ 294.0f, 90.0f });
    closeImage->SetPivot({ 0.5f, 0.5f });
    closeImage->SetScale({ 0.25f, 0.25f });
    closeImage->zOrder = 121;
    closeImage->SetVisible(false);
    uiManager->Add(closeImage);
    ringMesh = AddComponent<StaticMeshComponent>("TutorialDodgeGuideRingMesh");
    ringMesh->SetModel("./Data/Models/EffectModel/JumpTeregraphPlane.glb");
    if (ringMesh->model) for (auto& material : ringMesh->model->materials) material.data.alphaMode = 2;
    ringMesh->overrideDeferredPipelineName = "chargeTelegraphUnlitForward";
    ringMesh->overrideForwardPipelineName = "chargeTelegraphUnlitForward";
    ringMesh->SetIsCastShadow(false);
    ringMesh->SetIsVisible(false);
    ringMesh->SetUsingAbsoluteLocation(true);
    ringMesh->SetUsingAbsoluteRotation(true);
    ringMesh->SetUsingAbsoluteScale(true);
    ringMesh->plusAlphaCBuffer->data.objectType = ObjectType::NoLighting;
}

void GameTutorialUIActor::Finalize()
{
    for (const auto& ui : { ltPrompt, yPrompt, xPrompt, explanation, closeImage })
        if (ui) { ui->SetVisible(false); ui->MarkPendingKill(); }
    ltPrompt.reset(); yPrompt.reset(); xPrompt.reset(); explanation.reset(); closeImage.reset();
    ringMesh.reset();
}

void GameTutorialUIActor::UpdateExplanationLayout()
{
    explanation->SetWorldPosition(explanationPosition);
    explanation->SetScale(explanationScale);
    closeImage->SetWorldPosition(closePosition);
    closeImage->SetScale(closeScale);
}

void GameTutorialUIActor::SetExplanationVisible(bool visible)
{
    explanation->SetVisible(visible);
    closeImage->SetVisible(visible);
}

void GameTutorialUIActor::HidePrompts()
{
    ltPrompt->SetVisible(false);
    yPrompt->SetVisible(false);
    xPrompt->SetVisible(false);
}

void GameTutorialUIActor::UpdatePrompts(const DarkCameraActor& camera,
    const std::shared_ptr<SkeletonWarriorActor>& passive, const std::shared_ptr<SkeletonWarriorActor>& dodge,
    bool showLt, bool showY, bool showX, bool fixedPosition)
{
    HidePrompts();
    const auto update = [&](const std::shared_ptr<SkeletonWarriorActor>& skeleton,
        const std::shared_ptr<UIImageComponent>& image, bool show, DirectX::XMFLOAT2 extra)
    {
        if (!show || !skeleton || skeleton->IsDead() || skeleton->IsPendingKill()) return;
        const auto target = skeleton->GetCameraTargetComponent();
        if (!target) return;
        const auto projection = camera.ProjectWorldPositionForUI(target->GetComponentLocation());
        if (!projection.valid || !projection.inFront || !projection.insideViewport) return;
        auto position = fixedPosition ? promptFixedPosition : ConvertScreenToUI(projection.screenPosition);
        if (!fixedPosition) { position.x += promptOffset.x; position.y += promptOffset.y; }
        position.x += extra.x; position.y += extra.y;
        image->SetWorldPosition(position);
        image->SetScale({ promptScale, promptScale });
        image->SetVisible(true);
    };
    update(passive, ltPrompt, showLt, { 0.0f, 18.0f });
    update(passive, yPrompt, showY, { 0.0f, -60.0f });
    update(dodge, xPrompt, showX, { 0.0f, 0.0f });
}

void GameTutorialUIActor::HideRing() { if (ringMesh) ringMesh->SetIsVisible(false); }

void GameTutorialUIActor::UpdateRing(const DirectX::XMFLOAT3& p, float deltaTime)
{
    if (!ringMesh) return;
    ringPulseElapsed += deltaTime;
    const float wave = 0.5f + 0.5f * std::sin(ringPulseElapsed * ringPulseSpeed * DirectX::XM_2PI);
    const float pulse = std::lerp(ringPulseMin, ringPulseMax, wave);
    ringMesh->SetRelativeLocationDirect({ p.x + ringPositionOffset.x, p.y + ringPositionOffset.y, p.z + ringPositionOffset.z });
    ringMesh->SetRelativeScaleDirect(ringScale);
    ringMesh->UpdateComponentToWorld();
    ringMesh->plusAlphaCBuffer->data.cpuColor = { ringColor.x, ringColor.y, ringColor.z, 1.0f };
    ringMesh->plusAlphaCBuffer->data.emissionPower = ringEmission * pulse;
    ringMesh->SetIsVisible(true);
}

void GameTutorialUIActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("Tutorial Guide Ring Position Offset", &ringPositionOffset.x, 0.01f);
    ImGui::DragFloat3("Tutorial Guide Ring Scale", &ringScale.x, 0.01f);
    ImGui::ColorEdit3("Tutorial Guide Ring Color", &ringColor.x);
    ImGui::DragFloat("Tutorial Guide Ring Emission", &ringEmission, 0.01f, 0.0f, 20.0f);
    ImGui::DragFloat("Tutorial Guide Ring Pulse Speed", &ringPulseSpeed, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Tutorial Guide Ring Pulse Min", &ringPulseMin, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Tutorial Guide Ring Pulse Max", &ringPulseMax, 0.01f, 0.0f, 1.0f);
    ImGui::SeparatorText("Judge Dodge Operation Tutorial");
    ImGui::DragFloat2("Judge Dodge Message Position", &explanationPosition.x, 1.0f);
    ImGui::DragFloat2("Judge Dodge Message Scale", &explanationScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat2("Judge Dodge Close Position", &closePosition.x, 1.0f);
    ImGui::DragFloat2("Judge Dodge Close Scale", &closeScale.x, 0.01f, 0.01f, 4.0f);

    ImGui::DragFloat2(U8("ä[çúÇÃOperation Prompt Offset X / Y"), &promptOffset.x, 0.5f, -500.0f, 500.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Operation Prompt Scale", &promptScale, 0.01f, 0.05f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat2("Operation Fixed Screen Position X / Y", &promptFixedPosition.x, 0.5f, -500.0f, 2420.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);


#endif
}

