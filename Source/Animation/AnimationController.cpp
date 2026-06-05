#include "pch.h"
#include "AnimationController.h"

#include <imgui.h>
#include <ranges>

#include "Core/Actor.h"

void AnimationController::OnUpdate(const float deltaTime)
{
    animationTime += deltaTime * animationRate;

    if (target_->model->animations.size() == 0)
    {// アニメーションがないモデルの場合
        return;
    }

    // NotifyTrack のイベント処理
    float prevTime = animationTime;
    auto& notifies = notifyTracks[animationClip];

    for (auto& notify : notifies)
    {
        if (prevTime < notify.time &&
            animationTime >= notify.time)
        {
            switch (notify.type)
            {
            case AnimationNotify::Type::HitStart:
                break;
            case AnimationNotify::Type::HitEnd:
                break;
            case AnimationNotify::Type::ComboEnable:
                break;
            }
        }
    }

    // アニメーション遷移の準備
    switch (transitionState)
    {
    case AnimationTransitionState::NotStarted:
        //target_->model->Animate(animationClip, animationTime, animationNodes[Origin]);
        target_->model->Animate(this->animationNextClip, 0.0f, animationNodes[Next]);
        blendElapsedTime = 0.0f;
        animationTime = 0.0f;
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
#if 0
    this->isAnimationFinished = false;
    transitionState = AnimationController::AnimationTransitionState::Completed;
    this->animationClip = animationNameToIndex_[animationName];
    currentAnimationName = animationName;
    animationTime = 0; // blend している時に
    InterleavedGltfModel::Node& node = finalNodes.at(rootNodeIndex);
    previousPosition = { node.globalTransform._41, node.globalTransform._42, node.globalTransform._43 }; // グローバル空間
    zeroTranslation = node.translation;
#else
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
#endif // 0
}

// ルートモーションをリセットする
void AnimationController::ResetRootMotion(int animationClip)
{
    this->isAnimationFinished = false;
    transitionState = AnimationController::AnimationTransitionState::Completed;
    this->animationClip = animationClip;
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
    if (!ImGui::CollapsingHeader("Animation Debug"))
        return;

    DrawTimeline();

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

    auto& notifies = notifyTracks[animationClip];

    for (auto& notify : notifies)
    {
        float normalized = notify.time / length;

        float notifyX = pos.x + width * normalized;

        drawList->AddLine(ImVec2(notifyX, pos.y), ImVec2(notifyX, pos.y + height), IM_COL32(255, 0, 0, 255), 2.0f);
    }

#endif
}