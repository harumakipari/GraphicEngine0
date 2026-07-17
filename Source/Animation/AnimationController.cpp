#include "pch.h"
#include "AnimationController.h"

#include <imgui.h>
#include <magic_enum.hpp>
#include <ranges>
#include <json.hpp>
#include <fstream>

#include "Utility/SceneJsonUtils.h"
#include "Game/Actors/Base/Character.h"

AnimationController::AnimationController(Character* character, SkeletalMeshComponent* target, const int rootNodeIndex)
{
    owner = character;
    target_ = target;
    // アニメーションブレンドに使用するノード
    animationNodes[AnimNode::Origin] = target_->model->GetNodes();
    animationNodes[AnimNode::Next] = target_->model->GetNodes();

    // BlendSpaceの完成結果に使用するノード
    blendSpaceNodes = target_->model->GetNodes();
    blendSpaceClipA = target_->model->GetNodes();
    blendSpaceClipB = target_->model->GetNodes();
    for (auto& pose : blendSpacePoses)
    {
        pose = target_->model->GetNodes();
    }
    // 描画に使用するノード
    finalNodes = target_->model->GetNodes();

    this->rootNodeIndex = rootNodeIndex < 0 ? 0 : rootNodeIndex;
}


void AnimationController::OnUpdate(const float deltaTime)
{
    prevAnimationTime = animationTime;

    const auto& asset = animationNotifyAssets[animationClip];
    float curveRate = 1.0f;
    switch (transitionState)
    {
    case AnimationTransitionState::Inprogress:
        //curveRate = 1.0f;
        //break;
    case AnimationTransitionState::NotStarted:
    case AnimationTransitionState::Completed:
        curveRate = asset.speedCurve.Evaluate(animationTime);
        break;
    }

    float playRate = animationRate * asset.playRate * curveRate;
    animationTime += deltaTime * playRate;
    locomotionTime += deltaTime; // ブレンドスペースのためのタイム   

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

        if (prevAnimationTime < state.startTime &&
            animationTime >= state.startTime)
        {
            OnNotifyBegin(state);
        }
        if (prevAnimationTime < state.endTime &&
            animationTime >= state.endTime)
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
        if (useBlendSpace)
        {
            UpdateBlendSpace(deltaTime);
        }
        else
        {
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

    ImGui::DragFloat2("blendInput", &blendInput.x, 0.1f, -1.0f, 1.0f);
    ImGui::Checkbox("useBlendSpace", &useBlendSpace);

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

void AnimationController::DrawAnimationSettings(AnimationNotifyAsset& asset, float duration)
{
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
    std::string filename = "./Data/Animation/" + ownerName + "/" + asset.animationName + ".json";

    if (ImGui::Button("Save"))
    {
        SaveNotifyAsset(filename, asset);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        LoadNotifyAsset(filename, asset);
    }


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

    ImGui::Separator();
    ImGui::Text("Animation Settings");
    ImGui::Checkbox("Loop", &isAnimationLoop);
    //ImGui::Checkbox("Loop", &asset.loop);
    ImGui::SameLine();
    ImGui::DragFloat("Play Rate", &asset.playRate, 0.01f, 0.1f, 3.0f);
    // 次のコンボを設定する
    if (ImGui::BeginCombo("Next Combo", asset.nextCombo.c_str()))
    {
        for (auto clip : animationAssetOrder)
        {
            auto& comboAsset = animationNotifyAssets[clip];

            bool selected = comboAsset.animationName == asset.nextCombo;

            if (ImGui::Selectable(comboAsset.animationName.c_str(), selected))
            {
                asset.nextCombo = comboAsset.animationName;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
}

// タイムラインステート設定のImGui描画
void AnimationController::DrawStateTimeline(AnimationNotifyAsset& asset, float duration, float width, float height, float labelWidth, float trackHeight, float handleSize, ImDrawList* drawList, ImVec2 timelinePos)
{
    ImGui::InvisibleButton("TimelineSeek", ImVec2(width, height));

    if (ImGui::IsItemActive())
    {
        float mouseX = ImGui::GetIO().MousePos.x;
        float normalized = (mouseX - timelinePos.x) / width;
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        animationTime = normalized * duration;
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
}

// タイムラインイベント設定のImGui描画
void AnimationController::DrawEventTimeline(AnimationNotifyAsset& asset, float duration, float width, float labelWidth, float trackHeight, ImDrawList* drawList)
{
    ImGui::Text("Events");
    ImGui::SameLine(labelWidth);

    ImVec2 eventRow = ImGui::GetCursorScreenPos();
    float eventAreaHeight = std::max<float>(trackHeight, trackHeight * (float)asset.notifyTrack.events.size());
    drawList->AddRectFilled(eventRow, ImVec2(eventRow.x + width, eventRow.y + eventAreaHeight), IM_COL32(40, 40, 40, 255));
    for (int eventIndex = 0; eventIndex < asset.notifyTrack.events.size(); eventIndex++)
    {
        auto& event = asset.notifyTrack.events[eventIndex];

        float x = eventRow.x + (event.time / duration) * width;
        float y = eventRow.y + eventIndex * trackHeight;
        char id[64];
        sprintf_s(id, "Event_%d", eventIndex);

        ImGui::SetCursorScreenPos(ImVec2(x - 6, y));
        ImGui::InvisibleButton(id, ImVec2(12, trackHeight));

        if (ImGui::IsItemClicked())
        {
            selectedEventIndex = eventIndex;
            selectedStateIndex = -1;
        }

        if (ImGui::IsItemActive())
        {
            float deltaTime = (ImGui::GetIO().MouseDelta.x / width) * duration;

            event.time += deltaTime;

            event.time = std::clamp(event.time, 0.0f, duration);
        }

        drawList->AddLine(ImVec2(x, y), ImVec2(x, y + trackHeight), IM_COL32(255, 255, 0, 255), 3.0f);

        char eventText[32];

        sprintf_s(eventText, "%.2f", event.time);

        drawList->AddText(ImVec2(x + 5, y), IM_COL32(255, 255, 0, 255), eventText);
    }
    ImGui::Dummy(ImVec2(width, trackHeight * asset.notifyTrack.events.size()));


    ImVec2 popupPos = eventRow;
    ImGui::SetCursorScreenPos(popupPos);
    ImGui::Dummy(ImVec2(width, eventAreaHeight));

    ImGui::InvisibleButton("EventTrackArea", ImVec2(width, eventAreaHeight));

    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
    {
        float mouseX = ImGui::GetIO().MousePos.x;
        popupCreateTime = ((mouseX - eventRow.x) / width) * duration;
        popupCreateTime = std::clamp(popupCreateTime, 0.0f, duration);
        ImGui::OpenPopup("EventPopup");
    }
    if (ImGui::BeginPopup("EventPopup"))
    {
        if (ImGui::BeginMenu("Add Event"))
        {
            for (auto type : magic_enum::enum_values<AnimationNotifyEvent::Type>())
            {
                if (ImGui::MenuItem(magic_enum::enum_name(type).data()))
                {
                    AddEvent(asset.notifyTrack, type, popupCreateTime);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add State"))
        {
            for (auto type : magic_enum::enum_values<AnimationNotifyState::Type>())
            {
                if (ImGui::MenuItem(magic_enum::enum_name(type).data()))
                {
                    AddState(asset.notifyTrack, type, popupCreateTime);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

}

// カーブエディタのImGui描画
void AnimationController::DrawCurveEditor(AnimationNotifyAsset& asset, float duration, float width, float height, ImDrawList* drawList, ImVec2 timelinePos)
{
    float curveHeight = 150.0f;
    ImVec2 curvePos = ImGui::GetCursorScreenPos();
    curvePos.y += 50.0f;
    // 背景
    drawList->AddRectFilled(curvePos, ImVec2(curvePos.x + width, curvePos.y + curveHeight), IM_COL32(35, 35, 35, 255));

    // 縦線
    for (int i = 0; i <= 10; i++)
    {
        float x = curvePos.x + width * i / 10.0f;

        drawList->AddLine(ImVec2(x, curvePos.y), ImVec2(x, curvePos.y + curveHeight), IM_COL32(70, 70, 70, 255));
    }

    // 横線
    for (int i = 0; i <= 4; i++)
    {
        float y = curvePos.y + curveHeight * i / 4.0f;

        drawList->AddLine(ImVec2(curvePos.x, y), ImVec2(curvePos.x + width, y), IM_COL32(70, 70, 70, 255));
    }

    float maxSpeed = 2.0f;

    for (int i = 0; i < asset.speedCurve.keys.size(); ++i)
    {
        auto& key = asset.speedCurve.keys[i];
        float x = curvePos.x + (key.time / duration) * width;
        float y = curvePos.y + (1.0f - key.value / maxSpeed) * curveHeight;


        // 当たり判定を追加
        char id[32];
        sprintf_s(id, "CurveKey_%d", i);
        ImGui::SetCursorScreenPos(ImVec2(x - 6, y - 6));
        ImGui::InvisibleButton(id, ImVec2(20, 20));

        if (ImGui::IsItemClicked())
        {
            selectedCurveKey = i;
            selectedStateIndex = -1;
            selectedEventIndex = -1;
        }

        if (ImGui::IsItemActive())
        {
            float delta = (ImGui::GetIO().MouseDelta.x / width) * duration;
            key.time += delta;
            float deltaValue = -ImGui::GetIO().MouseDelta.y / curveHeight * maxSpeed;
            key.value += deltaValue;

            key.time = std::clamp(key.time, 0.0f, duration);
            key.value = std::clamp(key.value, 0.0f, maxSpeed);
        }

        if (ImGui::IsItemDeactivated())
        {
            float selectedTime = key.time;
            float selectedValue = key.value;

            std::sort(
                asset.speedCurve.keys.begin(),
                asset.speedCurve.keys.end(),
                [](const CurveKey& a, const CurveKey& b)
                {
                    return a.time < b.time;
                });

            for (int j = 0; j < asset.speedCurve.keys.size(); j++)
            {
                if (asset.speedCurve.keys[j].time == selectedTime &&
                    asset.speedCurve.keys[j].value == selectedValue)
                {
                    selectedCurveKey = j;
                    break;
                }
            }
        }


        ImU32 color = selectedCurveKey == i ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 100, 0, 255);
        drawList->AddCircleFilled(ImVec2(x, y), 5, color);

    }

    ImGui::Text("Selected = %d", selectedCurveKey);
    if (selectedCurveKey >= 0 && selectedCurveKey < asset.speedCurve.keys.size())
    {
        auto& selectedKey = asset.speedCurve.keys[selectedCurveKey];
        ImGui::Text("Selected Key");
        ImGui::Text("Time  = %.3f", selectedKey.time);
        ImGui::Text("Speed = %.3f", selectedKey.value);
    }
    else
    {
        ImGui::Text("No Key Selected");
    }

    ImGui::SetCursorScreenPos(curvePos);
    for (size_t i = 0; i + 1 < asset.speedCurve.keys.size(); i++)
    {
        auto& a = asset.speedCurve.keys[i];
        float ax = curvePos.x + (a.time / duration) * width;
        float ay = curvePos.y + (1.0f - a.value / maxSpeed) * curveHeight;

        auto& b = asset.speedCurve.keys[i + 1];
        float bx = curvePos.x + (b.time / duration) * width;
        float by = curvePos.y + (1.0f - b.value / maxSpeed) * curveHeight;

        drawList->AddLine(ImVec2(ax, ay), ImVec2(bx, by), IM_COL32(0, 255, 255, 255), 2.0f);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        if (selectedCurveKey >= 0)
        {
            asset.speedCurve.keys.erase(asset.speedCurve.keys.begin() + selectedCurveKey);
            selectedCurveKey = -1;
        }
    }

    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool inside = mousePos.x >= curvePos.x && mousePos.x <= curvePos.x + width && mousePos.y >= curvePos.y && mousePos.y <= curvePos.y + curveHeight;
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && inside)
    {// カーブエディタ内なら
        float time = (mousePos.x - curvePos.x) / width;
        time = std::clamp(time, 0.0f, 1.0f);
        time *= duration;
        float value = 1.0f - (mousePos.y - curvePos.y) / curveHeight;
        value *= maxSpeed;
        value = std::clamp(value, 0.0f, maxSpeed);

        curveCreateTime = time;
        curveCreateValue = value;
        ImGui::OpenPopup("CurvePopup");

    }




    if (ImGui::BeginPopup("CurvePopup"))
    {
        if (ImGui::MenuItem("Add Key"))
        {
            CurveKey key;

            key.time = curveCreateTime;
            key.value = curveCreateValue;

            asset.speedCurve.keys.push_back(key);

            std::sort(
                asset.speedCurve.keys.begin(),
                asset.speedCurve.keys.end(),
                [](const CurveKey& a, const CurveKey& b)
                {
                    return a.time < b.time;
                });

            for (int i = 0; i < asset.speedCurve.keys.size(); i++)
            {// 追加したキーを自動で選択する
                if (asset.speedCurve.keys[i].time == curveCreateTime &&
                    asset.speedCurve.keys[i].value == curveCreateValue)
                {
                    selectedCurveKey = i;
                    break;
                }
            }
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
            ImVec2(currentX, curvePos.y),
            ImVec2(currentX, curvePos.y + height),
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
                curvePos.y - 18),
            IM_COL32(
                255,
                0,
                0,
                255),
            currentText);
    }

    // 現在時間表示
    float currentX = curvePos.x + (animationTime / duration) * width;
    drawList->AddLine(ImVec2(currentX, curvePos.y), ImVec2(currentX, curvePos.y + curveHeight), IM_COL32(255, 0, 0, 255), 2.0f);
    // 速度表示
    float speed = asset.speedCurve.Evaluate(animationTime);
    char text[64];
    sprintf_s(text, "%.2f", speed);
    drawList->AddText(ImVec2(currentX + 5, curvePos.y), IM_COL32(255, 255, 0, 255), text);

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
}

// Notifyの詳細設定のImGui描画
void AnimationController::DrawNotifyInspector(AnimationNotifyAsset& asset)
{
    ImGui::Separator();
    ImGui::Text("Notify Inspector");
    // ステートが選択されている時
    if (selectedStateIndex != -1)
    {
        auto& state = asset.notifyTrack.states[selectedStateIndex];

        ImGui::Text("Type");
        ImGui::Text("%s", magic_enum::enum_name(state.type).data());
        ImGui::DragFloat("Start", &state.startTime, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("End", &state.endTime, 0.01f, 0.0f, 10.0f);

        switch (state.type)
        {
        case AnimationNotifyState::Type::HitBox:
        {
            char buffer[128];
            strcpy_s(buffer, state.parameter.c_str());
            if (ImGui::InputText("Weapon", buffer, sizeof(buffer)))
            {
                state.parameter = buffer;
            }
            break;
        }
        case AnimationNotifyState::Type::InputWindow:
            break;
        case AnimationNotifyState::Type::Invincible:
            break;
        case AnimationNotifyState::Type::TransitionWindow:
            break;
        case AnimationNotifyState::Type::JustDodgeWindow:
            break;
        case AnimationNotifyState::Type::DangerWindow:
            ImGui::DragFloat2(U8("ジャスト回避の矩形の範囲"), &state.justDodgeAreaSize.x, 0.1f, 0, 6);
            ImGui::DragFloat3(U8("ジャスト回避の矩形のオフセット"), &state.justDodgeAreaOffset.x, 0.1f, 0, 20);
            break;
        case AnimationNotifyState::Type::ShowTrail:
            break;
        case AnimationNotifyState::Type::ShowEmissive:
            ImGui::DragFloat("Power", &state.value, 0.1f, 0, 20);
            break;
        case AnimationNotifyState::Type::MotionWarp:
            ImGui::DragFloat3("moveDirection", &state.moveDirection.x, 0.1f, -1.0f, 1.0f);
            ImGui::DragFloat("distance", &state.moveDistance, 0.1f, 0, 20);
            break;
        }
    }
    // イベントが選択されている時
    if (selectedEventIndex != -1)
    {
        auto& event = asset.notifyTrack.events[selectedEventIndex];
        ImGui::Text("Type");
        ImGui::Text("%s", magic_enum::enum_name(event.type).data());
        ImGui::DragFloat("Time", &event.time, 0.01f, 0, 10);
        switch (event.type)
        {
        case AnimationNotifyEvent::Type::PlaySE:
        {
            char buffer[128];
            strcpy_s(buffer, event.parameter.c_str());
            if (ImGui::InputText("Sound", buffer, sizeof(buffer)))
            {
                event.parameter = buffer;
            }
            ImGui::DragFloat("Volume", &event.value, 0.01f, 0, 1);
            break;
        }
        case AnimationNotifyEvent::Type::SpawnEffect:
            break;
        }
    }
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
            std::string filename = "./Data/Animation/" + ownerName + "/" + asset.animationName + ".json";
            LoadNotifyAsset(filename, asset);
            ResetRootMotion(asset.animationName, false, false, 0.0f);
            // 値を初期化する
            selectedStateIndex = -1;
            selectedEventIndex = -1;
            popupCreateTime = 0.0f;
            selectedCurveKey = -1;
            curveCreateTime = 0.0f;
            curveCreateValue = 1.0f;
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

    // ImGui
    float trackHeight = 24.0f;
    float labelWidth = 150.0f;
    float handleSize = 40.0f;
    float width = 800.0f;
    float height = 30.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 timelinePos = ImGui::GetCursorScreenPos();

    DrawAnimationSettings(asset, duration);
    DrawStateTimeline(asset, duration, width, height, labelWidth, trackHeight, handleSize, drawList, timelinePos);
    DrawEventTimeline(asset, duration, width, labelWidth, trackHeight, drawList);
    DrawNotifyInspector(asset);

    ImGui::End();

    ImGui::Begin("CurveEditor");
    ImDrawList* curveDrawList = ImGui::GetWindowDrawList();
    ImVec2 curvePos = ImGui::GetCursorScreenPos();
    DrawCurveEditor(asset, duration, width, height, curveDrawList, curvePos);
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

// 全てのNotifyAssetsをロードする
void AnimationController::LoadAllNotifyAssets(const std::string& ownerName)
{
    std::string directory = "./Data/Animation/" + ownerName;
    for (auto& file : std::filesystem::directory_iterator(directory))
    {
        if (file.path().extension() != ".json")
            continue;
        AnimationNotifyAsset asset;
        LoadNotifyAsset(file.path().string(), asset);
        if (owner)
        {
            owner->OnAnimationChanged();
        }
        animationNotifyAssets[asset.animationClip] = asset;
    }
}

// ブレンドスペースを更新する
void AnimationController::UpdateBlendSpace(float deltaTime)
{
    Logger::Log("BlendSpace Update");
#if 0
    BlendPair pair = CalculateBlendPair(blendInput);



    size_t clipA = pair.clipA;
    size_t clipB = pair.clipB;
    float weight = pair.weight;

    float durationA = target_->model->animations[clipA].duration;
    float durationB = target_->model->animations[clipB].duration;
    float normalizedTime = locomotionTime / durationA;
    float timeA = normalizedTime * durationA;
    float timeB = normalizedTime * durationB;

    target_->model->Animate(clipA, timeA, blendSpaceClipA);
    target_->model->Animate(clipB, timeB, blendSpaceClipB);
    target_->model->BlendAnimations(blendSpaceClipA, blendSpaceClipB, weight, blendSpaceNodes);

#else
    BlendResult blend = CalculateBlendSpace(blendInput, blendSpeed);
    for (int i = 0; i < blend.count; i++)
    {
        size_t clip = blend.samples[i].clip;

        float duration =
            target_->model->animations[clip].duration;

        float normalized =
            locomotionTime / duration;

        float time =
            normalized * duration;

        target_->model->Animate(
            clip,
            time,
            blendSpacePoses[i]);
    }

    target_->model->BlendAnimations(blendSpacePoses,blend,blendSpaceNodes);


#endif // 0



    // BlendSpaceへの遷移
    if (blendSpaceTransition)
    {
        blendSpaceElapsed += deltaTime;

        float t = std::clamp(
            blendSpaceElapsed / blendSpaceTransitionTime,
            0.0f,
            1.0f);

        target_->model->BlendAnimations(
            animationNodes[Origin],
            blendSpaceNodes,
            t,
            finalNodes);

        if (t >= 1.0f)
        {
            blendSpaceTransition = false;
        }
    }
    else
    {
        finalNodes = blendSpaceNodes;
    }

    // ループ
    float runDuration =
        target_->model->animations[
            animationNameToIndex_["Jog_Fwd"]].duration;

    if (locomotionTime >= runDuration)
    {
        locomotionTime -= runDuration;
    }
}

// 入力方向から２つのアニメーションクリップとブレンドの重さを決定する関数
AnimationController::BlendPair AnimationController::CalculateBlendPair(const DirectX::XMFLOAT2& input)
{
    BlendPair result;
    auto& samples = locomotionBlendSpace.GetSamples();
    if (samples.size() < 4)
        return result;
    // 入力なし
    if (std::abs(input.x) <= FLT_EPSILON && std::abs(input.y) <= FLT_EPSILON)
    {
        result.directionA = MoveDirection::Idle;
        result.directionB = result.directionA;
        result.weight = 0.0f;
        return result;
    }
    // 前方向
    if (input.y > 0.0f)
    {
        // 前右
        if (input.x > 0.0f)
        {
            result.directionA = MoveDirection::Forward;
            result.directionB = MoveDirection::Right;
            float total = input.x + input.y;
            result.weight = input.x / total;
        }
        // 前左
        else if (input.x < 0.0f)
        {
            result.directionA = MoveDirection::Forward;
            result.directionB = MoveDirection::Left;
            float total = fabs(input.x) + input.y;
            result.weight = fabs(input.x) / total;
        }
        // 前だけ
        else
        {
            result.directionA = MoveDirection::Forward;
            result.directionB = result.directionA;
            result.weight = 0.0f;
        }
    }
    // 後方向
    else if (input.y < 0.0f)
    {
        // 後右
        if (input.x > 0.0f)
        {
            result.directionA = MoveDirection::Backward;
            result.directionB = MoveDirection::Right;
            float total = input.x + fabs(input.y);
            result.weight = input.x / total;
        }
        // 後左
        else if (input.x < 0.0f)
        {
            result.directionA = MoveDirection::Backward;
            result.directionB = MoveDirection::Left;
            float total = fabs(input.x) + fabs(input.y);
            result.weight = fabs(input.x) / total;
        }
        // 後ろだけ
        else
        {
            result.directionA = MoveDirection::Backward;;
            result.directionB = result.directionA;
            result.weight = 0.0f;
        }
    }
    // 横移動だけ
    else
    {
        if (input.x > 0)
        {
            result.directionA = MoveDirection::Right;
            result.directionB = result.directionA;
            result.weight = 0.0f;
        }
        else
        {
            result.directionA = MoveDirection::Left;
            result.directionB = result.directionA;
            result.weight = 0.0f;
        }
    }
    return result;
}

AnimationController::SpeedWeight AnimationController::CalculateSpeedWeight(float speed) const
{
    SpeedWeight result;

    speed = std::clamp(speed, 0.0f, 1.0f);

    float walkPoint = 2.0f / 5.0f;  // ここを後で walkSpeed / runSpeed に変更する

    if (speed <= walkPoint)
    {
        // Idle ⇔ Walk
        result.walk = speed / walkPoint;
        result.idle = 1.0f - result.walk;
        result.run = 0.0f;
    }
    else
    {
        // Walk ⇔ Run
        result.run = (speed - walkPoint) / (1.0f - walkPoint);
        result.walk = 1.0f - result.run;
        result.idle = 0.0f;
    }

    return result;
}

BlendResult AnimationController::CalculateBlendSpace(DirectX::XMFLOAT2 direction, float speed)
{
    BlendResult result = {};
    // 方向とその重み 例：Forward 70% / Right   30 %
    BlendPair pair = CalculateBlendPair(direction);
    // 速さとその重み 例：Walk 20 / Run 80 %
    SpeedWeight speedWeight = CalculateSpeedWeight(speed);

    if (speedWeight.walk > 0.0f)
    {
        result.samples[result.count].clip =
            GetBlendSpaceAnimationClip(pair.directionA,
                MoveSpeed::Walk);

        result.samples[result.count].weight =
            (1.0f - pair.weight)
            * speedWeight.walk;

        result.count++;

        result.samples[result.count].clip =
            GetBlendSpaceAnimationClip(pair.directionB,
                MoveSpeed::Walk);

        result.samples[result.count].weight =
            pair.weight
            * speedWeight.walk;

        result.count++;
    }
    if (speedWeight.run > 0)
    {
        result.samples[result.count].clip =
            GetBlendSpaceAnimationClip(pair.directionA,
                MoveSpeed::Run);

        result.samples[result.count].weight =
            (1.0f - pair.weight)
            * speedWeight.run;

        result.count++;

        result.samples[result.count].clip =
            GetBlendSpaceAnimationClip(pair.directionB,
                MoveSpeed::Run);

        result.samples[result.count].weight =
            pair.weight
            * speedWeight.run;

        result.count++;
    }
    if (speedWeight.idle > 0)
    {
        result.samples[result.count].clip =
            animationNameToIndex_["Idle"];

        result.samples[result.count].weight =
            speedWeight.idle;

        result.count++;
    }
    return result;
}

// 方向とスピードからアニメーションを取得する
size_t AnimationController::GetBlendSpaceAnimationClip(MoveDirection direction, MoveSpeed speed)
{
    switch (speed)
    {
    case MoveSpeed::Walk:

        switch (direction)
        {
        case MoveDirection::Forward:
            return animationNameToIndex_["Walk_Fwd"];

        case MoveDirection::Backward:
            return animationNameToIndex_["Walk_Bwd"];

        case MoveDirection::Left:
            return animationNameToIndex_["Walk_Left"];

        case MoveDirection::Right:
            return animationNameToIndex_["Walk_Right"];

        default:
            return animationNameToIndex_["Idle"];
        }

    case MoveSpeed::Run:

        switch (direction)
        {
        case MoveDirection::Forward:
            return animationNameToIndex_["Jog_Fwd"];

        case MoveDirection::Backward:
            return animationNameToIndex_["Jog_Bwd"];

        case MoveDirection::Left:
            return animationNameToIndex_["Jog_Left"];

        case MoveDirection::Right:
            return animationNameToIndex_["Jog_Right"];

        default:
            return animationNameToIndex_["Idle"];
        }

    default:
        return animationNameToIndex_["Idle"];
    }
}


// NotifyAssetを保存する
void AnimationController::SaveNotifyAsset(const std::string& filename, const AnimationNotifyAsset& asset)
{
    using json = nlohmann::json;

    json root;
    // アニメーション情報だけ保存
    root["animationName"] = asset.animationName;
    root["animationClip"] = asset.animationClip;
    root["nextCombo"] = asset.nextCombo;

    root["loop"] = asset.loop;
    root["playRate"] = asset.playRate;

    // SpeedCurveを保存
    for (const auto& key : asset.speedCurve.keys)
    {
        json curve;

        curve["time"] = key.time;
        curve["value"] = key.value;

        root["speedCurve"]["keys"].push_back(curve);
    }

    // NotifyState保存
    for (const auto& state : asset.notifyTrack.states)
    {
        json j;
        j["startTime"] = state.startTime;
        j["endTime"] = state.endTime;
        j["type"] = std::string(magic_enum::enum_name(state.type));
        j["parameter"] = state.parameter;
        j["value"] = state.value;
        j["moveDistance"] = state.moveDistance;
        j["moveDirection"] = state.moveDirection;
        j["justDodgeAreaSize"] = state.justDodgeAreaSize;
        j["justDodgeAreaOffset"] = state.justDodgeAreaOffset;
        root["states"].push_back(j);
    }

    // Event 保存
    for (const auto& event : asset.notifyTrack.events)
    {
        json j;

        j["time"] = event.time;

        j["type"] =
            std::string(
                magic_enum::enum_name(event.type));

        j["parameter"] = event.parameter;
        j["value"] = event.value;

        root["events"].push_back(j);
    }

    std::ofstream ofs(filename);

    if (!ofs.is_open())
        return;

    ofs << root.dump(4);
}

// NotifyAssetをロードする
void AnimationController::LoadNotifyAsset(const std::string& filename, AnimationNotifyAsset& asset)
{
    using json = nlohmann::json;
    // jsonファイルを開く
    std::ifstream ifs(filename);

    if (!ifs.is_open())
        return;

    json root;
    ifs >> root;

    // アニメーション情報を読み込む
    asset.animationName = root.value("animationName", "");
    asset.animationClip = root.value("animationClip", 0);
    asset.nextCombo = root.value("nextCombo", "");

    asset.loop = root.value("loop", false);
    asset.playRate = root.value("playRate", 1.0f);

    // SpeedCurveをロードする
    asset.speedCurve.keys.clear();
    if (root.contains("speedCurve"))
    {
        for (auto& j : root["speedCurve"]["keys"])
        {
            CurveKey key;

            key.time = j.value("time", 0.0f);
            key.value = j.value("value", 1.0f);

            asset.speedCurve.keys.push_back(key);
        }
    }

    // Stateをロードする
    asset.notifyTrack.states.clear();
    if (root.contains("states"))
    {
        for (auto& j : root["states"])
        {
            AnimationNotifyState state;

            state.startTime = j.value("startTime", 0.0f);

            state.endTime = j.value("endTime", 0.0f);

            auto typeName = j.value("type", "HitBox");
            auto type = magic_enum::enum_cast<AnimationNotifyState::Type>(typeName);

            if (type.has_value())
                state.type = type.value();

            state.parameter = j.value("parameter", "");
            state.value = j.value("value", 1.0f);
            if (j.contains("moveDirection"))
            {
                j["moveDirection"].get_to(state.moveDirection);
            }
            state.moveDistance = j.value("moveDistance", 0.0f);
            if (j.contains("justDodgeAreaSize"))
            {
                j["justDodgeAreaSize"].get_to(state.justDodgeAreaSize);
            }
            if (j.contains("justDodgeAreaOffset"))
            {
                j["justDodgeAreaOffset"].get_to(state.justDodgeAreaOffset);
            }
            asset.notifyTrack.states.push_back(state);
        }
    }

    // Eventをロードする
    asset.notifyTrack.events.clear();
    if (root.contains("events"))
    {
        for (auto& j : root["events"])
        {
            AnimationNotifyEvent event;

            event.time =
                j.value("time", 0.0f);

            auto typeName =
                j.value("type", "PlaySE");

            auto type =
                magic_enum::enum_cast<AnimationNotifyEvent::Type>(typeName);

            if (type.has_value())
                event.type = type.value();

            event.parameter =
                j.value("parameter", "");

            event.value =
                j.value("value", 0.0f);

            asset.notifyTrack.events.push_back(event);
        }
    }

    // 最後にソートする
    std::sort(
        asset.notifyTrack.events.begin(),
        asset.notifyTrack.events.end(),
        [](const AnimationNotifyEvent& a, const AnimationNotifyEvent& b)
        {
            return a.time < b.time;
        });

    std::sort(
        asset.notifyTrack.states.begin(),
        asset.notifyTrack.states.end(),
        [](const AnimationNotifyState& a, const AnimationNotifyState& b)
        {
            return a.startTime < b.startTime;
        });

    std::sort(
        asset.speedCurve.keys.begin(),
        asset.speedCurve.keys.end(),
        [](const CurveKey& a, const CurveKey& b)
        {
            return a.time < b.time;
        });
}

// イベントを追加する
void AnimationController::AddEvent(AnimationNotifyTrack& track, AnimationNotifyEvent::Type type, float time)
{
    AnimationNotifyEvent event;
    event.time = popupCreateTime;
    event.type = type;
    track.events.push_back(event);
    std::sort(
        track.events.begin(),
        track.events.end(),
        [](const AnimationNotifyEvent& a, const AnimationNotifyEvent& b)
        {
            return a.time < b.time;
        });

}

// ステートを追加する
void AnimationController::AddState(AnimationNotifyTrack& track, AnimationNotifyState::Type type, float startTime)
{
    AnimationNotifyState state;

    state.startTime = popupCreateTime;
    state.endTime = popupCreateTime + 0.2f;
    state.type = type;

    track.states.push_back(state);

    std::sort(
        track.states.begin(),
        track.states.end(),
        [](const AnimationNotifyState& a, const AnimationNotifyState& b)
        {
            return a.startTime < b.startTime;
        });
}
