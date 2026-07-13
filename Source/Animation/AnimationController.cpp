#include "pch.h"
#include "AnimationController.h"

#include <imgui.h>
#include <magic_enum.hpp>
#include <ranges>

#include "Game/Actors/Base/Character.h"

void AnimationController::OnUpdate(const float deltaTime)
{
    prevAnimationTime = animationTime;
#if 0
    animationTime += deltaTime * animationRate;
#else
    const auto& asset = animationNotifyAssets[animationClip];

    // 正規化時間
    float duration = target_->model->animations[animationClip].duration;

    float normalizedTime = duration > 0.0f ? animationTime / duration : 0.0f;

    // カーブ評価
    float curveRate = asset.speedCurve.Evaluate(normalizedTime);

    // 実際の再生速度
    float playRate = animationRate * asset.playRate * curveRate;

    // 時間更新
    animationTime += deltaTime * playRate;
#endif

    if (target_->model->animations.size() == 0)
    {// アニメーションがないモデルの場合
        return;
    }

    // NotifyTrack のイベント処理
    const auto& notifyAsset = animationNotifyAssets[notifyAnimationClip];

    auto& notifyTrack = notifyAsset.notifyTrack;
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
        if (prevAnimationTime < event.time && animationTime >= event.time)
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
        blendElapsedTime += deltaTime * playRate;
        //blendElapsedTime += deltaTime * animationRate;
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
        target_->model->BlendAnimations(animationNodes[Origin], animationNodes[Next], blendFactor, finalNodes);

        if (blendFactor >= 1.0f)
        {
            // 遷移終了
            transitionState = AnimationTransitionState::Completed;
            // 現在のアニメーションクリップを次のアニメーションクリップに変更する
            this->animationClip = this->animationNextClip;
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

    target_->UpdateChildTransforms(UpdateTransformFlags::None, TeleportType::None);
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
    resetRootMotionDelta = true;
    zeroTranslation = node.translation;
    isAnimationLoop = loop;
    // アニメーションが変わった時にステートなどを変更する
    owner->OnAnimationChanged();

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
            ResetRootMotion(asset.animationName, false, false, 0.0f);
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

    float duration = target_->model->animations[asset.animationClip].duration;

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
        ResetRootMotion(asset.animationName, false, false, 0.0f);
    }

    ImGui::SameLine();

    if (ImGui::Button("Stop"))
    {
        Stop();
        animationTime = 0.0f;
    }

    float width = 800.0f;
    float height = 30.0f;

    ImDrawList* drawList =
        ImGui::GetWindowDrawList();

    ImVec2 timelinePos = ImGui::GetCursorScreenPos();

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

        animationTime = normalized * duration;

        //target_->model->Animate(this->notifyAnimationClip, animationTime, finalNodes);
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
            ImVec2(
                x - 10,
                timelinePos.y + 12),
            IM_COL32(255, 255, 255, 255),
            buffer);
    }
    ImGui::Dummy(ImVec2(0, 40));
    float trackHeight = 24.0f;
    float labelWidth = 150.0f;
    float handleSize = 40.0f;
    float visualHandleSize = 6.0f;
    for (int stateIndex = 0;
        stateIndex < asset.notifyTrack.states.size();
        stateIndex++)
    {
        ImU32 color =
            (selectedStateIndex == stateIndex)
            ?
            IM_COL32(
                0,
                255,
                100,
                255)
            :
            IM_COL32(
                0,
                180,
                0,
                255);

        auto& state =
            asset.notifyTrack.states[stateIndex];

        ImGui::Text(
            "%s",
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
            (state.startTime / duration)
            * width;

        float x1 =
            rowPos.x +
            (state.endTime / duration)
            * width;

        float barWidth =
            x1 - x0;

        //---------------------------------
        // Move
        //---------------------------------

        {
            char id[64];
            sprintf_s(id, "Move_%d", stateIndex);

            ImGui::SetCursorScreenPos(ImVec2(x0, rowPos.y));

            ImGui::InvisibleButton(id, ImVec2(barWidth, trackHeight));

            if (ImGui::IsItemClicked())
            {
                selectedStateIndex = stateIndex;
                selectedEventIndex = -1;
            }


            if (ImGui::IsItemActive())
            {
                float deltaTime =
                    (ImGui::GetIO().MouseDelta.x
                        / width)
                    * duration;

                state.startTime += deltaTime;
                state.endTime += deltaTime;

                float length =
                    state.endTime
                    - state.startTime;

                state.startTime =
                    std::clamp(
                        state.startTime,
                        0.0f,
                        duration - length);

                state.endTime =
                    state.startTime
                    + length;
            }
        }

        //---------------------------------
        // Left Handle
        //---------------------------------

        {
            char id[64];
            sprintf_s(id,
                "Left_%d",
                stateIndex);

            ImGui::SetCursorScreenPos(
                ImVec2(
                    x0 - handleSize,
                    rowPos.y));

            ImGui::InvisibleButton(
                id,
                ImVec2(
                    handleSize * 2.0f,
                    trackHeight));

            if (ImGui::IsItemActive())
            {
                float deltaTime =
                    (ImGui::GetIO().MouseDelta.x
                        / width)
                    * duration;

                state.startTime += deltaTime;

                state.startTime =
                    std::clamp(
                        state.startTime,
                        0.0f,
                        state.endTime - 0.01f);
            }
        }

        //---------------------------------
        // Right Handle
        //---------------------------------

        {
            char id[64];
            sprintf_s(id,
                "Right_%d",
                stateIndex);

            ImGui::SetCursorScreenPos(
                ImVec2(
                    x1 - handleSize,
                    rowPos.y));

            ImGui::InvisibleButton(
                id,
                ImVec2(
                    handleSize * 2.0f,
                    trackHeight));

            if (ImGui::IsItemActive())
            {
                float deltaTime =
                    (ImGui::GetIO().MouseDelta.x
                        / width)
                    * duration;

                state.endTime += deltaTime;

                state.endTime =
                    std::clamp(
                        state.endTime,
                        state.startTime + 0.01f,
                        duration);
            }
        }

        //---------------------------------
        // Draw State
        //---------------------------------

        drawList->AddRectFilled(
            ImVec2(x0, rowPos.y),
            ImVec2(x1, rowPos.y + trackHeight),
            color);

        char stateText[64];

        sprintf_s(
            stateText,
            "%.2f - %.2f",
            state.startTime,
            state.endTime);

        ImVec2 textSize =
            ImGui::CalcTextSize(stateText);

        drawList->AddText(
            ImVec2(
                (x0 + x1) * 0.5f - textSize.x * 0.5f,
                rowPos.y + 4),
            IM_COL32(255, 255, 255, 255),
            stateText);

        drawList->AddRectFilled(
            ImVec2(
                x0 - 3,
                rowPos.y),
            ImVec2(
                x0 + 3,
                rowPos.y + trackHeight),
            IM_COL32(
                255,
                255,
                255,
                255));

        drawList->AddRectFilled(
            ImVec2(
                x1 - 3,
                rowPos.y),
            ImVec2(
                x1 + 3,
                rowPos.y + trackHeight),
            IM_COL32(
                255,
                255,
                255,
                255));

        ImGui::Dummy(
            ImVec2(
                width,
                trackHeight));
    }

    ImGui::Text("Events");

    ImGui::SameLine(labelWidth);

    ImVec2 eventRow = ImGui::GetCursorScreenPos();

    drawList->AddRectFilled(
        eventRow,
        ImVec2(
            eventRow.x + width,
            eventRow.y + trackHeight),
        IM_COL32(40, 40, 40, 255));
    for (int eventIndex = 0;
        eventIndex < asset.notifyTrack.events.size();
        eventIndex++)
    {
        auto& event =
            asset.notifyTrack.events[eventIndex];

        float x =
            eventRow.x +
            (event.time / duration)
            * width;

        char id[64];
        sprintf_s(id,
            "Event_%d",
            eventIndex);

        ImGui::SetCursorScreenPos(
            ImVec2(
                x - 6,
                eventRow.y));

        ImGui::InvisibleButton(id, ImVec2(12, trackHeight));

        if (ImGui::IsItemClicked())
        {
            selectedEventIndex = eventIndex;
            selectedStateIndex = -1;
        }

        if (ImGui::IsItemActive())
        {
            float deltaTime =
                (ImGui::GetIO().MouseDelta.x
                    / width)
                * duration;

            event.time += deltaTime;

            event.time =
                std::clamp(
                    event.time,
                    0.0f,
                    duration);
        }

        drawList->AddLine(
            ImVec2(x, eventRow.y),
            ImVec2(x, eventRow.y + trackHeight),
            IM_COL32(255, 255, 0, 255),
            3.0f);

        char eventText[32];

        sprintf_s(
            eventText,
            "%.2f",
            event.time);

        drawList->AddText(
            ImVec2(
                x + 5,
                eventRow.y),
            IM_COL32(
                255,
                255,
                0,
                255),
            eventText);
    }

    ImGui::Dummy(ImVec2(width, trackHeight));

    ImGui::Text("Speed Curve");
    float curveHeight = 150.0f;

    ImVec2 curvePos =
        ImGui::GetCursorScreenPos();

    drawList->AddRectFilled(
        curvePos,
        ImVec2(
            curvePos.x + width,
            curvePos.y + curveHeight),
        IM_COL32(35, 35, 35, 255));

    ImGui::InvisibleButton(
        "CurveArea",
        ImVec2(width, curveHeight));
    float maxSpeed = 2.0f;

    for (const auto& key : asset.speedCurve.keys)
    {
        float x = curvePos.x + key.time * width;
        float y = curvePos.y + (1.0f - key.value / maxSpeed) * curveHeight;
        drawList->AddCircleFilled(ImVec2(x, y), 5, IM_COL32(255, 100, 0, 255));
    }
    for (size_t i = 0; i + 1 < asset.speedCurve.keys.size(); i++)
    {
        auto& a = asset.speedCurve.keys[i];
        float ax = curvePos.x + a.time * width;
        float ay = curvePos.y + (1.0f - a.value / maxSpeed) * curveHeight;

        auto& b = asset.speedCurve.keys[i + 1];
        float bx = curvePos.x + b.time * width;
        float by = curvePos.y + (1.0f - b.value / maxSpeed) * curveHeight;

        drawList->AddLine(ImVec2(ax, ay),ImVec2(bx, by),IM_COL32(0, 255, 255, 255),2.0f);
    }

    ImGui::SetCursorScreenPos(eventRow);

    ImGui::InvisibleButton(
        "EventTrackArea",
        ImVec2(width, trackHeight));

    if (ImGui::IsItemHovered() &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Right))
    {
        float mouseX =
            ImGui::GetIO().MousePos.x;

        popupCreateTime =
            ((mouseX - eventRow.x) / width)
            * duration;

        popupCreateTime =
            std::clamp(
                popupCreateTime,
                0.0f,
                duration);

        ImGui::OpenPopup("EventPopup");
    }
    if (ImGui::BeginPopup("EventPopup"))
    {
        if (ImGui::MenuItem("Add Event"))
        {
            AnimationNotifyEvent event;

            event.time = popupCreateTime;

            event.type =
                AnimationNotifyEvent::Type::PlaySE;

            asset.notifyTrack.events.push_back(
                event);
        }
        if (ImGui::MenuItem("Add HitBox"))
        {
            AnimationNotifyState state;

            state.startTime = animationTime;
            state.endTime = animationTime + 0.2f;

            state.type =
                AnimationNotifyState::Type::HitBox;

            asset.notifyTrack.states.push_back(state);
        }
        ImGui::EndPopup();
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

        char currentText[32];

        sprintf_s(
            currentText,
            "%.3f",
            animationTime);

        drawList->AddText(
            ImVec2(
                currentX - 15,
                timelinePos.y - 18),
            IM_COL32(
                255,
                0,
                0,
                255),
            currentText);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        if (selectedStateIndex >= 0)
        {
            asset.notifyTrack.states.erase(
                asset.notifyTrack.states.begin()
                + selectedStateIndex);

            selectedStateIndex = -1;
        }

        if (selectedEventIndex >= 0)
        {
            asset.notifyTrack.events.erase(
                asset.notifyTrack.events.begin()
                + selectedEventIndex);

            selectedEventIndex = -1;
        }
    }

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