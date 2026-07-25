#pragma once

// C++ 標準ライブラリ
#include <string>
#include <unordered_map>

// プロジェクトの他のヘッダ
#include "Components/Render/MeshComponent.h"
#include "Graphics/Resource/InterleavedGltfModel.h"
#include "AnimationState.h"
#include "BlendSpaceAnimation.h"

class Character;

// アニメーションのコントローラー  
class AnimationController
{
public:
    enum class LocomotionGroup :uint8_t
    {
        Forward,
        Backward,
    };


    enum class MoveDirection :uint8_t
    {
        Idle,
        Forward,
        ForwardRight,
        ForwardLeft,
        BackRight,
        Backward,
        BackLeft,
    };

    enum class MoveSpeed :uint8_t
    {
        Idle,
        Walk,
        Run
    };

    struct SpeedWeight
    {
        float idle = 0.0f;
        float walk = 0.0f;
        float run = 0.0f;
    };

    struct BlendSpaceAnimation
    {
        size_t clip;
        // Blendする位置
        // x = 横方向
        // y = 前後方向
        DirectX::XMFLOAT2 position;
    };

    struct BlendPair
    {
        MoveDirection directionA = MoveDirection::Idle;
        MoveDirection directionB = MoveDirection::Idle;

        // clipA → clipB の割合
        // 0.0 : clipA 100%
        // 1.0 : clipB 100%
        float weight = 0.0f;

        bool isForwardGroup = true;
    };

public:
    AnimationController(Character* character, SkeletalMeshComponent* target, const int rootNodeIndex);

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

    // アニメーションの再生倍率をリセットする関数
    void ResetAnimationRate() { this->animationRate = 1.0f; }


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

    float GetAnimationLength(const std::string& animationName)
    {
        const size_t clip = animationNameToIndex_[animationName];
        return target_->model->animations[clip].duration;
    }

    // NotifyTrack にイベントを追加する関数
    void AddNotifyState(const std::string& animationName, const float start, const float end,
        const AnimationNotifyState::Type type, const std::string& parameter = "", float animationSpeed = 1.0f)
    {
        const size_t clip = animationNameToIndex_[animationName];
        animationNotifyAssets[clip].notifyTrack.states.push_back({ start,end,type,parameter,animationSpeed });
    }

    void AddNotifyEvent(const std::string& animationName, const float time,
        const AnimationNotifyEvent::Type type, const std::string& parameter = "", float value = 1.0f)
    {
        const size_t clip = animationNameToIndex_[animationName];
        animationNotifyAssets[clip].notifyTrack.events.push_back({ time,type,parameter,value });

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

    void AddAnimationCurve(const std::string& animationName, const AnimationCurve& curve)
    {
        const size_t clip = animationNameToIndex_[animationName];
        animationNotifyAssets[clip].speedCurve = curve;
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

    // ルートモーションを無視するかどうか
    void SetIgnoreRootMotion(const bool ignoreRootMotion) { this->ignoreRootMotion = ignoreRootMotion; }

    // オーナーの名前を設定する
    void SetOwnerName(const std::string& name) { ownerName = name; }

    // 全てのNotifyAssetsをロードする
    void LoadAllNotifyAssets(const std::string& ownerName);

    // ブレンドスペースでブレンドするアニメーションを追加する
    // x +右-左　y +前-後
    void AddBlendAnimation(const std::string& name, float x, float y)
    {
        const size_t clip = animationNameToIndex_[name];
        locomotionBlendSpace.AddAnimation(clip, { x,y });
    }

    void SetBlendInput(const float x, const float y, const float speed)
    {
        blendInput = { x, y };
        blendSpeed = speed;
    }

    void SetUseBlendSpace(const bool useBlendSpace)
    {
        // 変更がないなら何もしない
        if (this->useBlendSpace == useBlendSpace)
            return;

        this->useBlendSpace = useBlendSpace;

        if (useBlendSpace)
        {// ブレンドスペース開始
            // ルートモーションを切る
            enableRootMotion = true;
            ignoreRootMotion = true;

            locomotionTime = 0.0f;
            // ブレンドスペースに入る時に補間処理をするため
            blendSpaceTransition = true;
            blendSpaceElapsed = 0.0f;
            animationNodes[Origin] = finalNodes;

            Logger::Log("Enable BlendSpace");
        }
        else
        {
            enableRootMotion = true;

            ResetRootMotion("Jog_Fwd", true, true, 0.2f);
            Logger::Log("Disable BlendSpace");
        }
    }

    void AddTarget(SkeletalMeshComponent* target)
    {
        extraTargets_.push_back(target);
    }

    // 方向とスピードからアニメーションを取得する 
    size_t GetBlendSpaceAnimationClip(MoveDirection direction, MoveSpeed speed);

    // 方向とスピードからアニメーションの名前を取得する 
    const std::string GetBlendSpaceAnimationName(MoveDirection direction, MoveSpeed speed);

    // blendInputから方向を決める関数
    MoveDirection CalculateMoveDirection(const DirectX::XMFLOAT2& input, MoveDirection currentMoveDirection);


private:
    // それぞれのアニメーション再生時間を取る
    float GetLocomotionDuration();

    // ブレンドスペースを更新する
    void UpdateBlendSpace(float deltaTime);

    // 入力方向から２つのアニメーションクリップとブレンドの重さを決定する関数
    BlendPair CalculateBlendPair(const DirectX::XMFLOAT2& input);

    // スピードから待機歩き走りの重みを計算する
    SpeedWeight CalculateSpeedWeight(float speed) const;

    // 方向とスピードからアニメーションと重みを計算する
    BlendResult CalculateBlendSpace(DirectX::XMFLOAT2 direction, float speed);

    // NotifyAssetを保存する
    void SaveNotifyAsset(const std::string& filename, const AnimationNotifyAsset& asset);

    // NotifyAssetをロードする
    void LoadNotifyAsset(const std::string& filename, AnimationNotifyAsset& asset);

    // イベントを追加する
    void AddEvent(AnimationNotifyTrack& track, AnimationNotifyEvent::Type type, float time);

    // ステートを追加する
    void AddState(AnimationNotifyTrack& track, AnimationNotifyState::Type type, float startTime);

    // ルートモーションをリセットする
    void ResetRootMotion(int animationClip);

    // アニメーション設定のImGui描画
    void DrawAnimationSettings(AnimationNotifyAsset& asset, float duration);

    // タイムラインステート設定のImGui描画
    void DrawStateTimeline(AnimationNotifyAsset& asset, float duration, float width, float height, float labelWidth, float trackHeight, float handleSize, ImDrawList* drawList, ImVec2 timelinePos);

    // タイムラインイベント設定のImGui描画
    void DrawEventTimeline(AnimationNotifyAsset& asset, float duration, float width, float labelWidth, float trackHeight, ImDrawList* drawList);

    // カーブエディタのImGui描画
    void DrawCurveEditor(AnimationNotifyAsset& asset, float duration, float width, float height, ImDrawList* drawList, ImVec2 timelinePos);

    // Notifyの詳細設定のImGui描画
    void DrawNotifyInspector(AnimationNotifyAsset& asset);


    std::vector<SkeletalMeshComponent*> extraTargets_;
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

    // BlendSpaceの完成結果
    std::vector<InterleavedGltfModel::Node> blendSpaceNodes;

    std::vector<InterleavedGltfModel::Node> blendSpaceClipA;
    std::vector<InterleavedGltfModel::Node> blendSpaceClipB;

    std::array<std::vector<InterleavedGltfModel::Node>, 4> blendSpacePoses;

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

    bool ignoreRootMotion = true; // ルートモーションを無視する

    float blendElapsedTime = 0.0f;  // ブレンド時に経過した時間

    bool resetRootMotionDelta = false;   // ルートモーションのリセットが必要かどうか

    size_t selectedTimelineClip = 0;    // タイムラインの対象アニメーション

    // アニメーションクリップごとのイベント
    std::unordered_map<size_t, AnimationNotifyAsset> animationNotifyAssets;
    std::vector<size_t> animationAssetOrder;    // アニメーションの表示を追加順にするための変数

    int selectedStateIndex = -1;
    int selectedEventIndex = -1;
    float popupCreateTime = 0.0f;

    int selectedCurveKey = -1;
    float curveCreateTime = 0.0f;
    float curveCreateValue = 1.0f;

    std::string ownerName = "";    // コントローラーを所有しているオーナーの名前

    // ブレンドスペース
    BlendSpace locomotionBlendSpace;
    // ブレンドスペースで使用する入力値
    DirectX::XMFLOAT2 blendInput;
    size_t locomotionClip;
    size_t locomotionNextClip;

    bool locomotionChanging;

    float locomotionBlendElapsed;
    float blendSpeed;

    bool useBlendSpace = false;
    float locomotionTime = 0.0f;
    float locomotionPhase = 0.0f;
    float locomotionPlayRate = 1.0f;
    // ブレンドスペースに入る時の補完処理
    bool blendSpaceTransition = false;
    float blendSpaceTransitionTime = 0.2f;
    float blendSpaceElapsed = 0.0f;

    MoveDirection currentMoveDirection = MoveDirection::Idle;
    LocomotionGroup locomotionGroup = LocomotionGroup::Forward;

    friend class Player;
};

