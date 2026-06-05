#pragma once

// C++ 標準ライブラリ
#include <string>
#include <unordered_map>

// プロジェクトの他のヘッダ
#include "Components/Render/MeshComponent.h"
#include "Graphics/Resource/InterleavedGltfModel.h"


struct AnimationNotify
{
    float time;

    enum class Type:uint8_t
    {
        HitStart,
        HitEnd,
        ComboEnable,
    };
    Type type;
};

// アニメーションのコントローラー  
class AnimationController
{
public:
    struct AnimationState
    {
        size_t clip = 0;

        float time = 0.0f;

        bool loop = true;

        std::vector<InterleavedGltfModel::Node>* nodes = nullptr;
    };

public:
    AnimationController(SkeletalMeshComponent* target, int rootNodeIndex = 0) :target_(target), rootNodeIndex(rootNodeIndex)
    {
        // アニメーションブレンドに使用するノード
        animationNodes[AnimNode::Origin] = target_->model->GetNodes();
        animationNodes[AnimNode::Next] = target_->model->GetNodes();

        // 描画に使用するノード
        finalNodes = target_->model->GetNodes();

        owner = target->GetOwner();
    }

    void AddAnimation(const std::string& animationName, const size_t animationClip)
    {
        animationNameToIndex_[animationName] = animationClip;
        animationImGUiOrder.push_back(animationName);
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
    void AddNotify(size_t clip,float time,AnimationNotify::Type type)
    {
        notifyTracks[clip].push_back(
            {
                time,
                type
            });

        std::sort(
            notifyTracks[clip].begin(),
            notifyTracks[clip].end(),
            [](const auto& a, const auto& b)
            {
                return a.time < b.time;
            });
    }


private:
    // ルートモーションをリセットする
    void ResetRootMotion(int animationClip);

    SkeletalMeshComponent* target_ = nullptr;
    Actor* owner = nullptr;

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

    //アニメーション時間
    float animationTime = 0.0f;

    // 今再生しているアニメーションのインデックス
    size_t animationClip = 0;

    // 次再生したいアニメーションのインデックス
    size_t animationNextClip = 0;

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

    bool ignoreRootMotion = true; // ルートモーションを無視する

    float blendElapsedTime = 0.0f;  // ブレンド時に経過した時間

    bool resetRootMotionDelta = false;   // ルートモーションのリセットが必要かどうか

    // アニメーションクリップごとのイベント
    std::unordered_map<size_t,std::vector<AnimationNotify>> notifyTracks; 
};

