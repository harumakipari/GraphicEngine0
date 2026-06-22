#pragma once

// アニメーションに入れる
struct AnimationNotifyState
{
    float startTime;
    float endTime;

    enum class Type :uint8_t
    {
        HitBox,         // 当たり判定有効
        ComboWindow,    // コンボ受付
        Invincible,     // 無敵時間
        TransitionWindow,   // アニメーション遷移
        JustDodgeWindow, // ジャスト回避
        AnimationSpeed, // アニメーションスピード
        DangerWindow, // 攻撃の危険時間
    };

    Type type;

    std::string parameter;

    float animationSpeed = 1.0f;
};

struct AnimationNotifyEvent
{
    float time;

    enum class Type : uint8_t
    {
        PlaySE,
        SpawnEffect,
    };

    Type type;

    std::string parameter;
};

struct AnimationNotifyTrack
{
    std::vector<AnimationNotifyEvent> events;
    std::vector<AnimationNotifyState> states;
};

struct AnimationNotifyAsset
{
    std::string animationName = "";
    size_t animationClip = 0;
    std::string nextCombo = "";

    AnimationNotifyTrack notifyTrack;
};