#pragma once

struct CurveKey
{
    float time;
    float value;
};

struct AnimationCurve
{
    std::vector<CurveKey> keys;
    float Evaluate(const float t)const;
};

// アニメーションに入れる
struct AnimationNotifyState
{
    float startTime;
    float endTime;

    enum class Type :uint8_t
    {
        HitBox,         // 当たり判定有効
        InputWindow,    // 入力受付
        Invincible,     // 無敵時間
        TransitionWindow,   // アニメーション遷移
        JustDodgeWindow, // ジャスト回避
        DangerWindow, // 攻撃の危険時間
        ShowTrail, // 軌跡を出現させる時間
        ShowEmissive,  // 剣が光る時間
        MotionWarp, // 移動値を入れる
    };

    Type type;

    std::string parameter;

    // Sphere radius used by HitBox states. 0.8 preserves legacy Boss assets.
    float hitBoxRadius = 0.8f;

    float value = 1.0f; // 剣の光るエミッシブの強さなど

    // 移動値
    DirectX::XMFLOAT3 moveDirection{ 0.0f,0.0f,0.0f };
    float moveDistance = 0.0f;
    // ジャスト回避の矩形の範囲
    // Full size: X=Right/Width, Y=Up/Height, Z=Forward/Depth.
    DirectX::XMFLOAT3 justDodgeAreaSize = { 0.0f,2.0f,0.0f };
    // ジャスト回避の矩形のオフセット
    DirectX::XMFLOAT3 justDodgeAreaOffset = { 0.0f,0.0f,0.0f };
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

    float value = 0.0f; // 音のvolumeなど
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

    bool loop = false;              // ループするか
    float playRate = 1.0f;          // 基本再生速度

    AnimationCurve speedCurve;      // 再生速度カーブ

    AnimationNotifyTrack notifyTrack;
};

struct AnimationMotionWarp
{
    const AnimationNotifyState* state;
    DirectX::XMFLOAT3 direction;
    float speed;
    float notifyMoveDistance = 0.0f;
    float actualWarpDistance = 0.0f;
    DirectX::XMFLOAT3 startPosition{};
};
