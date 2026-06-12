#include "pch.h"
#include "AnimationController.h"

#include <imgui.h>
#include <magic_enum.hpp>
#include <ranges>

#include "Game/Actors/Base/Character.h"

void AnimationController::OnUpdate(const float deltaTime)
{
    prevAnimationTime = animationTime;
    animationTime += deltaTime * animationRate;

    if (target_->model->animations.size() == 0)
    {// アニメーションがないモデルの場合
        return;
    }

    // NotifyTrack のイベント処理
    auto& asset =
        animationNotifyAssets[notifyAnimationClip];

    auto& notifyTrack = asset.notifyTrack;
    for (auto& state : notifyTrack.states)
    {
        bool wasInside =
            prevAnimationTime >= state.startTime &&
            prevAnimationTime < state.endTime;

        bool isInside =
            animationTime >= state.startTime &&
            animationTime < state.endTime;

        if (!wasInside && isInside)
        {
            OnNotifyBegin(state);
        }

        if (wasInside && !isInside)
        {
            OnNotifyEnd(state);
        }
    }

    for (auto& event : notifyTrack.events)
    {
        if (prevAnimationTime < event.time &&
            animationTime >= event.time)
        {
            OnNotifyEvent(event);
        }
    }

    // アニメーション遷移の準備
    switch (transitionState)
    {
    case AnimationTransitionState::NotStarted:
        //target_->model->Animate(animationClip, animationTime, animationNodes[Origin]);
        target_->model->Animate(this->animationNextClip, 0.0f, animationNodes[Next]);
        blendElapsedTime = 0.0f;
        //animationTime = 0.0f;
        blendFactor = 0.0f;

        transitionState = AnimationTransitionState::Inprogress;
        break;
    case AnimationTransitionState::Inprogress:
        blendElapsedTime += deltaTime * animationRate;
        if (transitionTime > 0.0f)
        {
            blendFactor = blendElapsedTime / transitionTime;     //ゼロ除算を防ぐため
        }
        else
        {
            blendFactor = 1.0f;
        }
        blendFactor =
            std::clamp(blendFactor, 0.0f, 1.0f);

        target_->model->Animate(this->animationNextClip, animationTime, animationNodes[Next]);

        // blend
        target_->model->BlendAnimations(
            animationNodes[Origin],
            animationNodes[Next],
            blendFactor,
            finalNodes);



        if (blendFactor >= 1.0f)
        {
            // 遷移終了
            transitionState = AnimationTransitionState::Completed;
            // 現在のアニメーションクリップを次のアニメーションクリップに変更する
            this->animationClip = this->animationNextClip;


            InterleavedGltfModel::Node& node = animationNodes[Next][rootNodeIndex];

            /*  previousPosition =
              {
                  node.globalTransform._41,
                  node.globalTransform._42,
                  node.globalTransform._43
              };*/

        }
        break;
    case AnimationTransitionState::Completed:
        target_->model->Animate(this->animationClip, animationTime, finalNodes);

        // 終わったら通常時に戻す
        if (target_->model->animations.at(animationClip).duration < animationTime)
        {
            if (isAnimationLoop)
            {//アニメーションをループするとき
                if (requestStopLoop)
                {
                    isAnimationLoop = false;    // ループしないモードにする
                    animationTime = 0.0f;
                    requestStopLoop = false;
                }
                else
                {
                    animationTime = 0;
                    ResetRootMotion(static_cast<int>(animationClip));
                }
            }
            else
            {
                isAnimationFinished = true;
            }
        }

        break;
    default:
        break;
    }

    if (enableRootMotion)
    {
        InterleavedGltfModel::Node& node = finalNodes.at(rootNodeIndex);

        if (!ignoreRootMotion)
        {
            DirectX::XMFLOAT4X4 worldTransform = owner->GetWorldTransform();
            // グローバル空間
            DirectX::XMFLOAT3 position =
            {
                node.globalTransform._41,
                node.globalTransform._42,
                node.globalTransform._43
            };

            if (resetRootMotionDelta)
            {
                previousPosition = position;
                resetRootMotionDelta = false;
            }

            // グローバル空間
            DirectX::XMFLOAT3 displacement =
            {
                position.x - previousPosition.x,
                position.y - previousPosition.y,
                position.z - previousPosition.z
            };
            DirectX::XMStoreFloat3(&displacement, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&displacement), DirectX::XMLoadFloat4x4(&worldTransform))); // ワールド空間

            DirectX::XMFLOAT3 translation = owner->GetPosition();

            translation.x += displacement.x;
            //translation.y += displacement.y;
            translation.z += displacement.z;

            owner->SetPosition(translation);

            previousPosition = position;
        }
        // ルートノードの変位量を初期姿勢の値に設定。
        node.translation = zeroTranslation;

        // 子ノードのグローバル変換を再帰的に更新する。
        target_->model->CumulateTransforms(finalNodes);
    }
    target_->SetModelNodes(finalNodes);

    target_->UpdateChildTransforms(
        UpdateTransformFlags::None,
        TeleportType::None);


}

void AnimationController::ResetRootMotion(const std::string& animationName, const bool loop, const bool isBlend, const float blendTime)
{
    notifyAnimationClip = animationNameToIndex_[animationName];

    animationTime = 0.0f;
    prevAnimationTime = 0.0f;

    this->animationNextClip = animationNameToIndex_[animationName];
    this->isAnimationFinished = false;
    currentAnimationName = animationName;
    InterleavedGltfModel::Node& node = finalNodes.at(rootNodeIndex);
    //previousPosition = { node.globalTransform._41, node.globalTransform._42, node.globalTransform._43 }; // グローバル空間
    resetRootMotionDelta = true;
    zeroTranslation = node.translation;
    isAnimationLoop = loop;

    if (isBlend)
    {
        isBlendingAnimation = true;
        transitionTime = blendTime;
        animationNodes[Origin] = finalNodes;
        transitionState = AnimationController::AnimationTransitionState::NotStarted;
    }
    else
    { // ブレンドしないなら現在のアニメーションを次のアニメーションに変更する
        this->animationClip = animationNameToIndex_[animationName];
        transitionState = AnimationController::AnimationTransitionState::Completed;
    }
}

// ルートモーションをリセットする
void AnimationController::ResetRootMotion(int animationClip)
{

    this->isAnimationFinished = false;
    transitionState = AnimationController::AnimationTransitionState::Completed;
    this->animationClip = animationClip;
    prevAnimationTime = 0.0f;
    animationTime = 0;
    target_->model->Animate(animationClip, 0, finalNodes);
    InterleavedGltfModel::Node& node = finalNodes.at(rootNodeIndex);
    previousPosition = { node.globalTransform._41, node.globalTransform._42, node.globalTransform._43 }; // グローバル空間
    resetRootMotionDelta = true;
    zeroTranslation = node.translation;
}

void AnimationController::DrawImGui()
{
#ifdef USE_IMGUI
    DrawTimeline();

    if (!ImGui::CollapsingHeader("Animation Debug"))
        return;


    auto& node = finalNodes[181];

    ImGui::Text("Weapon Socket Pos: %.2f %.2f %.2f",
        node.globalTransform._41,
        node.globalTransform._42,
        node.globalTransform._43);



    ImGui::Text("Current: %s", currentAnimationName.c_str());
    ImGui::Text("Playing: %s", isAnimationFinished ? "No" : "Yes");

    ImGui::Checkbox("Loop", &isAnimationLoop);
    ImGui::Checkbox("Blend", &isBlendingAnimation);
    ImGui::SliderFloat("Blend Time", &transitionTime, 0.0f, 1.0f);
    ImGui::SliderFloat("Rate", &animationRate, 0.0f, 3.0f);

    ImGui::Checkbox("enableRootMotion", &enableRootMotion);
    ImGui::Checkbox("ignoreRootMotion", &ignoreRootMotion);

    ImGui::Separator();

    for (const auto& name : animationImGUiOrder)
    {
        if (ImGui::Button(name.c_str()))
        {
            ResetRootMotion(name, isAnimationLoop, isBlendingAnimation, transitionTime);
        }
    }
#endif
}


void AnimationController::DrawTimeline()
{
#ifdef USE_IMGUI
    ImGui::Begin("Animation Sequence");

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 200.0f);

    for (auto clip : animationAssetOrder)
    {
        auto& asset = animationNotifyAssets[clip];

        bool selected = selectedTimelineClip == clip;

        if (ImGui::Selectable(asset.animationName.c_str(), selected))
        {
            selectedTimelineClip = clip;
            ResetRootMotion(asset.animationName,false,false,0.0f);
        }
    }

    ImGui::NextColumn();

    auto it = animationNotifyAssets.find(selectedTimelineClip);

    if (it == animationNotifyAssets.end())
    {
        ImGui::End();
        return;
    }

    auto& asset = it->second;

    float duration =target_->model->animations[asset.animationClip].duration;

    ImGui::Text("Timeline");
    ImGui::Separator();

    ImGui::Text(
        "Time : %.3f / %.3f",
        animationTime,
        duration);

    ImGui::SliderFloat(
        "##AnimationTime",
        &animationTime,
        0.0f,
        duration);


    if (ImGui::Button("Play"))
    {
        ResetRootMotion(asset.animationName,false,false,0.0f);
    }

    ImGui::SameLine();

    if (ImGui::Button("Stop"))
    {
        animationTime = 0.0f;
    }

    float width = 800.0f;
    float height = 30.0f;

    ImDrawList* drawList =
        ImGui::GetWindowDrawList();

    ImVec2 timelinePos =
        ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(
        "TimelineSeek",
        ImVec2(width, height));

    if (ImGui::IsItemActive())
    {
        float mouseX =
            ImGui::GetIO().MousePos.x;

        float normalized =
            (mouseX - timelinePos.x) / width;

        normalized =
            std::clamp(
                normalized,
                0.0f,
                1.0f);

        animationTime =
            normalized * duration;
    }

    drawList->AddRectFilled(
        timelinePos,
        ImVec2(
            timelinePos.x + width,
            timelinePos.y + height),
        IM_COL32(60, 60, 60, 255));

    for (int i = 0; i <= 10; i++)
    {
        float x =
            timelinePos.x +
            width * i / 10.0f;

        float t =
            duration * i / 10.0f;

        drawList->AddLine(
            ImVec2(x, timelinePos.y),
            ImVec2(x, timelinePos.y + 5),
            IM_COL32(255, 255, 255, 255));

        char buffer[32];
        sprintf_s(buffer, "%.2f", t);

        drawList->AddText(
            ImVec2(x - 10, timelinePos.y - 18),
            IM_COL32(255, 255, 255, 255),
            buffer);
    }
    ImGui::Dummy(ImVec2(0, 40));
    float trackHeight = 24.0f;
    float labelWidth = 150.0f;
    for (auto& state : asset.notifyTrack.states)
    {
        ImGui::Text("%s",
            magic_enum::enum_name(state.type).data());

        ImGui::SameLine(labelWidth);

        ImVec2 rowPos =
            ImGui::GetCursorScreenPos();

        drawList->AddRectFilled(
            rowPos,
            ImVec2(
                rowPos.x + width,
                rowPos.y + trackHeight),
            IM_COL32(40, 40, 40, 255));

        float x0 =
            rowPos.x +
            (state.startTime / duration) * width;

        float x1 =
            rowPos.x +
            (state.endTime / duration) * width;

        drawList->AddRectFilled(
            ImVec2(x0, rowPos.y),
            ImVec2(x1, rowPos.y + trackHeight),
            IM_COL32(0, 255, 0, 255));

        ImGui::Dummy(
            ImVec2(width, trackHeight));
    }
    for (auto& event : asset.notifyTrack.events)
    {
        float x =
            timelinePos.x +
            (event.time / duration) * width;

        drawList->AddLine(
            ImVec2(x, timelinePos.y),
            ImVec2(x, timelinePos.y + 20),
            IM_COL32(255, 255, 0, 255),
            2.0f);
    }

    if (asset.animationClip == animationClip)
    {
        float currentX =
            timelinePos.x +
            (animationTime / duration)
            * width;

        drawList->AddLine(
            ImVec2(currentX, timelinePos.y),
            ImVec2(currentX, timelinePos.y + height),
            IM_COL32(255, 0, 0, 255),
            3.0f);
    }

#if 0
    ImGui::Text("Time : %.2f / %.2f", animationTime, GetCurrentAnimationLength());
    float length = GetCurrentAnimationLength();

    ImGui::SliderFloat("Time", &animationTime, 0.0f, length);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = 400.0f;
    float height = 30.0f;
    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(60, 60, 60, 255));
    float normalized = animationTime / GetCurrentAnimationLength();
    float x = pos.x + width * normalized;
    drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + height), IM_COL32(255, 255, 255, 255), 2.0f);
#endif
    ImGui::End();

#endif
}


void AnimationController::OnNotifyBegin(const AnimationNotifyState& state)
{
    if (!owner)
    {
        Logger::Warning(U8("アニメーションコントローラーでownerがnullptrです！"));
        return;
    }
    if (owner)
    {
        owner->OnAnimationNotifyBegin(state);
    }
}

void AnimationController::OnNotifyEnd(const AnimationNotifyState& state)
{
    if (!owner)
    {
        Logger::Warning(U8("アニメーションコントローラーでownerがnullptrです！"));
        return;
    }

    if (owner)
    {
        owner->OnAnimationNotifyEnd(state);
    }
}

void AnimationController::OnNotifyEvent(const AnimationNotifyEvent& event)
{
    if (!owner)
    {
        Logger::Warning(U8("アニメーションコントローラーでownerがnullptrです！"));
        return;
    }

    if (owner)
    {
        owner->OnAnimationNotifyEvent(event);
    }
}