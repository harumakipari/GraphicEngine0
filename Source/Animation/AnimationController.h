#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Components/Render/MeshComponent.h"
#include "Graphics/Resource/InterleavedGltfModel.h"
#include "AnimationState.h"
#include "BlendSpaceAnimation.h"

class Character;

class AnimationController
{
public:
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
public:
    AnimationController(Character* character, SkeletalMeshComponent* target, const int rootNodeIndex);

    void AddAnimation(const std::string& animationName, const size_t animationClip)
    {
        animationNameToIndex_[animationName] = animationClip;
        auto& asset =animationNotifyAssets[animationClip];

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

    float GetCurrentAnimationTime() const
    {
        return animationTime;
    }

    bool EvaluateWeaponVisualState(bool& outShowTrail, float& outEmissivePower,bool& outHasShowTrail, bool& outHasShowEmissive) const;
    void ResetRootMotion(const std::string& animationName, const bool loop = false, const bool isBlend = true, const float blendTime = 0.3f);
    bool PlayAnimationImmediate(const std::string& animationName, bool loop = true, bool ignoreRootMotion = false);

    void DrawImGui();

    void DrawTimeline();

    bool IsEditorPreviewActive() const { return editorPreviewActive; }
    bool IsEditorPreviewPlaying() const { return editorPreviewPlaying; }
    bool IsEditorPreviewHitBoxSweepVisible() const { return editorPreviewShowHitBoxSweep; }
    float GetCurrentSampledAnimationTime() const
    {
        return editorPreviewActive ? editorPreviewTime : animationTime;
    }

    const std::string& GetCurrentAnimationName()const { return currentAnimationName; }

    float GetAnimationLength(const std::string& animationName)
    {
        const size_t clip = animationNameToIndex_[animationName];
        return target_->model->animations[clip].duration;
    }

    bool ConfigureLocalPoseOverride(
        const std::string& sourceAnimationName,
        float sourceTime,
        const std::vector<std::string>& boneNames);
    void SetLocalPoseOverrideWeight(float weight);
    void ClearLocalPoseOverride();

    // NotifyTrack にイベントを追加する関数
    bool SetPlaybackRange(float startTime, float endTime);
    bool HoldAnimationPose(const std::string& animationName, float time);
    bool BlendToHeldAnimationPose(const std::string& animationName, float time, float blendTime);
    void ReleaseRuntimeHeldAnimationPose(bool preserveBlendSource = false);
    void ReleaseHeldAnimationPose(bool preserveBlendSource = false);
    bool LatchCurrentVisualPose();
    void ReleaseLatchedVisualPose();
    bool IsVisualPoseLatched() const { return visualPoseLatchActive; }

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

    // Animation単位でRoot TranslationをPoseから除去する。
    void SetRemoveRootTranslationFromPose(const std::string& animationName, bool remove);

    // ルートモーションを無視するかどうか
    void SetIgnoreRootMotion(const bool ignoreRootMotion) { this->ignoreRootMotion = ignoreRootMotion; }

    void SetEnableRootMotion(const bool enableRootMotion)
    {
        this->enableRootMotion = enableRootMotion;

        if (!enableRootMotion)
        {
            blendSpaceRootMotionDelta = { 0.0f, 0.0f, 0.0f };
            blendSpaceRootMotionValid = false;
            resetLocomotionRootMotion = true;
        }
    }

    // オーナーの名前を設定する
    void SetOwnerName(const std::string& name) { ownerName = name; }

    // 全てのNotifyAssetsをロードする
    void LoadAllNotifyAssets(const std::string& ownerName);

    enum class RuntimeDangerObbSaveResult
    {
        Saved,
        FileNotFound,
        DangerWindowNotFound,
        JsonParseFailed,
        JsonWriteFailed,
    };
    RuntimeDangerObbSaveResult SaveDangerObbForRuntimeTuning(
        size_t clip, size_t stateIndex, const AnimationNotifyState& state,
        std::string& outFilePath);

    AnimationNotifyAsset* GetNotifyAssetForRuntimeTuning(size_t clip)
    {
        const auto it = animationNotifyAssets.find(clip);
        return it != animationNotifyAssets.end() ? &it->second : nullptr;
    }
    const AnimationNotifyAsset* GetNotifyAssetForRuntimeTuning(size_t clip) const
    {
        const auto it = animationNotifyAssets.find(clip);
        return it != animationNotifyAssets.end() ? &it->second : nullptr;
    }

    // 単一の360度Locomotion BlendSpaceへアニメーションを追加する
    void AddLocomotionBlendAnimation(
        const std::string& name,
        float angle,
        float phaseOffset = 0.0f)
    {
        const size_t clip = animationNameToIndex_[name];
        const float normalizedOffset =
            name == "Jog_Fwd" ? 0.0f : BlendSpace::WrapPhase(phaseOffset);
        locomotionBlendSpace.AddAnimation(clip, angle, normalizedOffset);
    }

    void SetBlendInput(const float x, const float y, const float speed)
    {
        blendInput = { x, y };
        blendSpeed = speed;
    }

    void SetBlendDebugRawInput(const float x, const float z)
    {
        locomotionDebugRawStickX = x;
        locomotionDebugRawStickZ = z;
    }

    void SetUseBlendSpace(const bool use)
    {
        // 変更がないなら何もしない
        if (this->useBlendSpace == use)
            return;

        // 変更前の状態を保存
        const bool wasUsingBlendSpace = this->useBlendSpace;
        this->useBlendSpace = use;

        // ルートモーションを使わない
        enableRootMotion = false;
        ignoreRootMotion = true;

        // BlendSpace -> 通常アニメーション
        if (wasUsingBlendSpace && !useBlendSpace)
        {
            Logger::Log("Disable BlendSpace");

            // 位相転送はRootMotion抑制とは別に管理する
            const float blendDuration = GetLocomotionDuration();

            pendingLocomotionPhase = 0.0f;

            if (blendDuration > FLT_EPSILON)
            {
                pendingLocomotionPhase = locomotionTime / blendDuration;
                // 0~1へ収める
                pendingLocomotionPhase -= std::floor(pendingLocomotionPhase);
            }

            // 次に通常アニメーションが開始される時、
            // この位相をanimationTime へ反映する
            pendingLocomotionPhaseTransfer = true;

            // BlendSpaceから通常アニメーションへの遷移中は、
            // ブレンド済みRoot位置の差分をActorへ適用しない
            suppressNormalRootMotionUntilTransitionCompleted = true;
            suppressNormalRootMotionObservedTransition = false;

            blendSpaceRootMotionDelta = {};
            blendSpaceRootMotionValid = false;
            // 通常Root Motionの基準位置を再同期する
            resetRootMotionDelta = true;

            return;
        }
        // 通常アニメーション → BlendSpace
        if (!wasUsingBlendSpace && use)
        {
            Logger::Log("Enable BlendSpace");

            bool phaseTransferSucceeded = false;

            const size_t phaseSourceClip =
                transitionState ==
                AnimationTransitionState::Completed
                ? animationClip
                : animationNextClip;

            const float blendSpaceDuration =GetLocomotionDuration();

            if (target_ &&
                target_->model &&
                phaseSourceClip <
                target_->model
                ->animations
                .size() &&
                blendSpaceDuration >
                FLT_EPSILON)
            {
                const float normalDuration =
                    target_->model
                    ->animations
                    .at(phaseSourceClip)
                    .duration;

                if (normalDuration >
                    FLT_EPSILON)
                {
                    float normalizedPhase = 0.0f;
                    normalizedPhase =
                        animationTime /
                        normalDuration;

                    normalizedPhase -=
                        std::floor(
                            normalizedPhase);

                    locomotionTime =
                        normalizedPhase *
                        blendSpaceDuration;

                    previousLocomotionPhase =
                        normalizedPhase;

                    resetLocomotionRootMotion =
                        false;

                    phaseTransferSucceeded =
                        true;
                }
            }

            if (!phaseTransferSucceeded)
            {
                locomotionTime = 0.0f;
                previousLocomotionPhase = 0.0f;

                resetLocomotionRootMotion =
                    true;
            }

            // 前回のBlendSpace RMを破棄する
            blendSpaceRootMotionDelta = {};
            blendSpaceRootMotionValid = false;

            // 通常PoseからBlendSpace Poseへ補間
            blendSpaceElapsed = 0.0f;
            blendSpaceTransition = true;
            animationNodes[Origin] = finalNodes;

            return;
        }
    }

    // BlendSpaceから抽出したRoot Motionを取得して消費する
    bool ConsumeBlendSpaceRootMotion(DirectX::XMFLOAT3& outDelta);

private:

    struct LocalPoseOverrideBone
    {
        size_t nodeIndex = 0;
        DirectX::XMFLOAT3 translation = {};
        DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    };

    void ApplyLocalPoseOverride();


    // それぞれのアニメーション再生時間を取る
    float GetLocomotionDuration();

    // ブレンドスペースを更新する
    void UpdateBlendSpace(float deltaTime);

    // NotifyAssetを保存する
    void SaveNotifyAsset(const std::string& filename, const AnimationNotifyAsset& asset);

    // NotifyAssetをロードする
    void LoadNotifyAsset(const std::string& filename, AnimationNotifyAsset& asset);

    // イベントを追加する
    void AddEvent(AnimationNotifyTrack& track, AnimationNotifyEvent::Type type, float time);

    // ステートを追加する
    void AddState(AnimationNotifyTrack& track, AnimationNotifyState::Type type, float startTime);

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

    void UpdateEditorPreview(float deltaTime);
    void ApplyEditorPreviewPose();
    void DrawEditorPreviewStates();
    void ProcessEditorPreviewEvents(float previousTime, float currentTime, float duration, bool wrapped);
    void ProcessEditorPreviewStatesForward(float previousTime, float currentTime);
    void SyncEditorPreviewStates(float time);
    void EndAllEditorPreviewStates();
    bool IsEditorPreviewStateDispatchable(const AnimationNotifyState& state) const;
    void BeginEditorPreviewState(size_t stateIndex);
    void EndEditorPreviewState(size_t stateIndex);
    void BeginEditorPreview(bool playing, bool resetTime);
    void SetEditorPreviewTime(float time);
    void CaptureEditorRuntimeSnapshot();
    void EndEditorPreview();
    bool IsVisualPoseLatchValid() const;
    const std::vector<InterleavedGltfModel::Node>& GetRuntimeVisualPoseNodes();
    void ApplyRuntimeVisualPose();

private:
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

    std::vector<InterleavedGltfModel::Node> runtimeHeldPoseNodes;
    bool runtimeHeldPoseActive = false;
    bool runtimeHeldPoseTransitionActive = false;

    struct VisualPoseLatchTarget
    {
        SkeletalMeshComponent* target = nullptr;
        const InterleavedGltfModel* model = nullptr;
        size_t nodeCount = 0;
    };
    std::vector<InterleavedGltfModel::Node> visualPoseLatchNodes;
    std::vector<VisualPoseLatchTarget> visualPoseLatchTargets;
    bool visualPoseLatchActive = false;

    std::vector<LocalPoseOverrideBone> localPoseOverrideBones;
    float localPoseOverrideWeight = 0.0f;
    bool localPoseOverrideActive = false;

    // BlendSpaceの完成結果
    std::vector<InterleavedGltfModel::Node> blendSpaceNodes;
    std::array<std::vector<InterleavedGltfModel::Node>, 4> blendSpacePoses;

    enum class AnimationTransitionState :uint8_t
    {
        NotStarted,
        Inprogress,
        Completed,
    };

    struct EditorRuntimeSnapshot
    {
        bool valid = false;
        float animationTime = 0.0f;
        float playbackEndTime = -1.0f;
        float prevAnimationTime = 0.0f;
        size_t animationClip = 0;
        size_t animationNextClip = 0;
        size_t notifyAnimationClip = 0;
        AnimationTransitionState transitionState = AnimationTransitionState::NotStarted;
        float blendFactor = 0.0f;
        float blendElapsedTime = 0.0f;
        float transitionTime = 0.0f;
        bool isBlendingAnimation = false;
        bool isAnimationFinished = false;
        bool isAnimationLoop = false;
        std::string currentAnimationName;
        std::vector<InterleavedGltfModel::Node> animationOriginNodes;
        std::vector<InterleavedGltfModel::Node> animationNextNodes;
        std::vector<InterleavedGltfModel::Node> finalNodes;
        DirectX::XMFLOAT3 previousPosition{};
        DirectX::XMFLOAT3 zeroTranslation{};
        bool suppressNormalRootMotionUntilTransitionCompleted = false;
        bool suppressNormalRootMotionObservedTransition = false;
    };

    //遷移ステート
    AnimationTransitionState transitionState = AnimationTransitionState::NotStarted;

    //アニメーションの再生倍率
    float animationRate = 1.0f;     //デフォルト 1,0f

    // 前フレームのアニメーション時間
    float prevAnimationTime = 0.0f;

    //アニメーション時間
    float animationTime = 0.0f;
    float playbackEndTime = -1.0f;

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


    // 今再生しているアニメーションの名前
    std::string currentAnimationName;

    int rootNodeIndex = 0;
    DirectX::XMFLOAT3 previousPosition = {}; // world 空間
    DirectX::XMFLOAT3 zeroTranslation = {}; // 親ノード空間

    bool enableRootMotion = true;  // ルートモーションがある場合

    bool ignoreRootMotion = false; // ルートモーションを無視する

    std::unordered_set<size_t> removeRootTranslationFromPoseClips;
    bool currentAnimationRemovesRootTranslation = false;
    bool removeRootTranslationDuringTransition = false;

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

    bool editorPreviewActive = false;
    bool editorPreviewPlaying = false;
    float editorPreviewTime = 0.0f;
    float previousEditorPreviewTime = 0.0f;
    std::unordered_map<size_t, AnimationNotifyState> activeEditorPreviewStates;
    bool editorPreviewShowDangerWindow = true;
    bool editorPreviewShowHitBox = true;
    bool editorPreviewShowHitBoxSweep = true;
    EditorRuntimeSnapshot editorRuntimeSnapshot;

    std::string ownerName = "";    // コントローラーを所有しているオーナーの名前

    // ブレンドスペース
    BlendSpace locomotionBlendSpace;

    // ブレンドスペースで使用する入力値
    DirectX::XMFLOAT2 blendInput;

    float blendSpeed;

    bool useBlendSpace = false;
    float locomotionTime = 0.0f;
    float locomotionPlayRate = 1.0f;
    // ブレンドスペースに入る時の補完処理
    bool blendSpaceTransition = false;
    float blendSpaceTransitionTime = 0.2f;
    float blendSpaceElapsed = 0.0f;
    // ブレンドスペースのルートモーション処理
    float previousLocomotionPhase = 0.0f;
    bool resetLocomotionRootMotion = true;

    DirectX::XMFLOAT3 blendSpaceRootMotionDelta = { 0.0f, 0.0f, 0.0f };

    bool blendSpaceRootMotionValid = false;
    // 前回時刻の RootMotion 取得用 Pose
    std::vector<InterleavedGltfModel::Node> previousBlendSpaceRootMotionNodes;
    // ルートモーションの最初を記録する　Pose
    std::vector<InterleavedGltfModel::Node> blendSpaceRootMotionStartNodes;
    // ルートモーションの最後を記録する　Pose
    std::vector<InterleavedGltfModel::Node> blendSpaceRootMotionEndNodes;
    // BlendSpace終了後、通常アニメーション遷移中の
    // ブレンド済みrootからRoot Motionを抽出しないためのフラグ
    bool suppressNormalRootMotionUntilTransitionCompleted = false;
    // SetUseBlendSpace(false)以前からCompletedだった状態と、
    // 新しく開始した通常遷移の完了を区別する
    bool suppressNormalRootMotionObservedTransition = false;

    // BlendSpace解除時の位相を次のLocomotion clipへ一度だけ渡す
    bool pendingLocomotionPhaseTransfer = false;
    float pendingLocomotionPhase = 0.0f;

    float locomotionBackwardSideMaxWeight = 0.15f;
    float locomotionDebugBwdWeight = 0.5f;
    float locomotionDebugRawStickX = 0.0f;
    float locomotionDebugRawStickZ = 0.0f;
    float locomotionDebugEvaluationAngle = 0.0f;
    float locomotionDebugFrozenCommonPhase = 0.0f;
    // ブレンドスペースではない
    // ルートモーションの最初を記録する　Pose
    std::vector<InterleavedGltfModel::Node> normalRootMotionStartNodes;
    // ルートモーションの最後を記録する　Pose
    std::vector<InterleavedGltfModel::Node> normalRootMotionEndNodes;
    friend class Player;
};

