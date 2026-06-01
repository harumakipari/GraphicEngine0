#pragma once
#include "Components/Base/SceneComponent.h"
#include "Engine/Effects/EffectManager.h"

class ParticleComponent : public SceneComponent
{
public:
    ParticleComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :SceneComponent(name, owner) {}
    ~ParticleComponent() override = default;
public:
    struct LineData
    {
        bool useLine = false;	// 線を使うかどうか

        // 線分構造体
        struct Segment
        {
            Transform* start = nullptr; // 線の開始Transform
            Transform* end = nullptr;   // 線の終了Transform
            int segmentCount = 5;    // 線分の分割数
        };
        std::vector<Segment> segments; 	// 線分リスト
    };

    // 追加設定構造体
    struct AddSettings
    {
        bool loop = false;					//ループ再生するか
        float startDelay = 0.0f;			//再生開始遅延時間（秒）
        LineData lineData;					//線情報
        std::function<void()> onPreEmit;		//エフェクト発生前コールバック
    };
    // 追加設定取得
    const AddSettings& GetAddSettings() const { return settings; }

    // 追加設定設定
    void SetAddSettings(const AddSettings& settings) { this->settings = settings; }

    // エフェクトデータ読み込み
    void Load(const std::string& filePath);

    // エフェクト再生
    void Play();

    // エフェクトをアタッチ先に再生
    void PlayAttached();

    // エフェクト停止
    void Stop();

    // 再生中かを返す
    bool IsPlaying() const { return isPlaying; }

    // エフェクトハンドル取得
    EffectHandle GetEffectHandle() const { return effectHandle; }

    // フレーム更新
    void Tick(float deltaTime) override;

    // エフェクト全体の寿命時間を計算して取得
    float CalculateDuration() const;

    // デバッグGUI描画
    void DrawImGuiInspector() override;
private:
    EffectHandle effectHandle = -1; 	// エフェクトハンドル
    bool isPlaying = false;				// 再生中フラグ
    AddSettings settings; 				// 追加設定
    float elapsedDelayTime = 0.0f;		// 再生開始遅延時間の経過時間

    float elapsedTimeSincePlay = 0.0f;   // 再生開始からの経過時間
    float duration = 0.0f; // エフェクト全体の寿命時間

    float emitInterval = 0.8f;
    float emitTimer = 0.0f;
};
