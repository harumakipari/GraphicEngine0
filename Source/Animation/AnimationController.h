#pragma once

// C++ 標準ライブラリ
#include <string>
#include <unordered_map>

// プロジェクトの他のヘッダ
#include "Components/Render/MeshComponent.h"
#include "Graphics/Resource/InterleavedGltfModel.h"
#include "AnimationState.h"

class Character;

// アニメーションのコントローラー  
class AnimationController
{
public:
    AnimationController(Character* character, SkeletalMeshComponent* target, const int rootNodeIndex) :owner(character), target_(target)
    {
        // アニメーションブレンドに使用するノード
        animationNodes[AnimNode::Origin] = target_->model->GetNodes();
        animationNodes[AnimNode::Next] = target_->model->GetNodes();

        // 描画に使用するノード
        finalNodes = target_->model->GetNodes();

        this->rootNodeIndex = rootNodeIndex < 0 ? 0 : rootNodeIndex;
    }

    void AddAnimation(const std::string& animationName, const size_t animationClip)
    {
        animationNameToIndex_[animationName] = animationClip;
        animationImGUiOrder.push_back(animationName);

        auto& asset =
            animationNotifyAssets[animationClip];

        asset.animationName = animationName;
        asset.animationClip = animationClip;

        animationAssetOrder.push_back(animationClip);
    }

    // アニメーション再生しているかどうか
    bool IsPlayAnimation() const
    {
        return !(this->isAnimationFinished);
    }

    void OnUpdate(const float deltaTime);

    // アニメーションの再生倍率を変更する関数
    void SetAnimationRate(const float animationRate) { this->animationRate = animationRate; }

    // アニメーションを止める処理
    void Stop()
    {
        isAnimationFinished = true;
        transitionState = AnimationTransitionState::NotStarted;
    }

    // アニメーションのループを切りよく終了させるフラグ
    void RequestStopLoop()
    {
        requestStopLoop = true;
    }

    float GetCurrentTimeNormalized() const
    {
        float duration = target_->model->animations[animationClip].duration;
        return animationTime / duration;
    }

    float GetCurrentAnimationTime() const
    {
        return animationTime;
    }

    void ResetRootMotion(const std::string& animationName, const bool loop = false, const bool isBlend = true, const float blendTime = 0.3f);

    void DrawImGui();

    void DrawTimeline();

    size_t GetAnimationClip()const { return animationClip; }

    const std::string& GetCurrentAnimationName()const { return currentAnimationName; }

    float GetCurrentAnimationLength() const { return target_->model->animations[animationClip].duration; }

    // NotifyTrack にイベントを追加する関数
    void AddNotifyState(
        const std::string& animationName,
        const float start,
        const float end,
        const AnimationNotifyState::Type type)
    {
        const size_t clip = animationNameToIndex_[animationName];
        animationNotifyAssets[clip].notifyTrack.states.push_back({ start,end,type });
    }

    void AddNotifyEvent(
        const std::string& animationName,
        const float time,
        const AnimationNotifyEvent::Type type, const std::string& parameter = "")
    {
        const size_t clip = animationNameToIndex_[animationName];
        animationNotifyAssets[clip].notifyTrack.events.push_back({ time,type,parameter });

        std::sort(
            animationNotifyAssets[clip].notifyTrack.events.begin(),
            animationNotifyAssets[clip].notifyTrack.events.end(),
            [](const auto& a, const auto& b)
            {
                return a.time < b.time;
            });
    }

    void AddCombo(const std::string& animationName, const std::string& nextComboName)
    {
        const size_t clip = animationNameToIndex_[animationName];
        animationNotifyAssets[clip].nextCombo = nextComboName;
    }

    void OnNotifyBegin(const AnimationNotifyState& state);

    void OnNotifyEnd(const AnimationNotifyState& state);

    void OnNotifyEvent(const AnimationNotifyEvent& event);

    const AnimationNotifyAsset* GetAnimationAsset(const std::string& animationName) const
    {
        auto it = animationNameToIndex_.find(animationName);

        if (it == animationNameToIndex_.end())
            return nullptr;

        auto assetIt = animationNotifyAssets.find(it->second);

        if (assetIt == animationNotifyAssets.end())
            return nullptr;

        return &assetIt->second;
    }

private:
    // ルートモーションをリセットする
    void ResetRootMotion(int animationClip);

    SkeletalMeshComponent* target_ = nullptr;
    Character* owner = nullptr;

    std::unordered_map<std::string, size_t> animationNameToIndex_;

    // アニメーションブレンドに使用するノード
    enum AnimNode
    {
        Origin, // 元のアニメーション
        Next    // 次のアニメーション
    };
    std::vector<InterleavedGltfModel::Node> animationNodes[2];


    // 描画に使用するノード
    std::vector<InterleavedGltfModel::Node> finalNodes;


    enum class AnimationTransitionState :uint8_t
    {
        NotStarted,
        Inprogress,
        Completed,
    };

    //遷移ステート
    AnimationTransitionState transitionState = AnimationTransitionState::NotStarted;

    //アニメーションの再生倍率
    float animationRate = 1.0f;     //デフォルト 1,0f

    // 前フレームのアニメーション時間
    float prevAnimationTime = 0.0f;

    //アニメーション時間
    float animationTime = 0.0f;

    // 今再生しているアニメーションのインデックス
    size_t animationClip = 0;

    // 次再生したいアニメーションのインデックス
    size_t animationNextClip = 0;

    // ステートの管理に使用するアニメーションのインデックス　別にわけないと
    size_t notifyAnimationClip = 0;


    // アニメーションをループするか
    bool isAnimationLoop = true;

    // 現在のブレンドの比率
    float blendFactor = 0.0f;

    // ブレンド中かどうか
    bool isBlendingAnimation = false;

    // ブレンドしている時間
    float transitionTime = 0.0f;

    // アニメーションが終了したかどうか
    bool isAnimationFinished = false;

    // ループ終了フラグ 
    bool requestStopLoop = false; // 切りよくループを終わらせる

    // 今再生しているアニメーションの名前
    std::string currentAnimationName;

    // ImGuiで表示するための
    std::vector<std::string> animationImGUiOrder;

    int rootNodeIndex = 0;
    DirectX::XMFLOAT3 previousPosition = {}; // world 空間
    DirectX::XMFLOAT3 zeroTranslation = {}; // 親ノード空間

    bool enableRootMotion = true;  // ルートモーションがある場合

    bool ignoreRootMotion = false; // ルートモーションを無視する

    float blendElapsedTime = 0.0f;  // ブレンド時に経過した時間

    bool resetRootMotionDelta = false;   // ルートモーションのリセットが必要かどうか

    size_t selectedTimelineClip = 0;    // タイムラインの対象アニメーション

    // アニメーションクリップごとのイベント
    std::unordered_map<size_t, AnimationNotifyAsset> animationNotifyAssets;

    std::vector<size_t> animationAssetOrder;    // アニメーションの表示を追加順にするための変数
};

