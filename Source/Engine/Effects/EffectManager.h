#pragma once

#include <memory>
#include <string>

#include "Engine/Effects/CoreComputeParticleSystem.h"
#include "Core/Vector.h"
#include "Core/CoreColor.h"

class SceneComponent;

template<typename T>
struct Range
{
    // 範囲の最小値と最大値
    T min;
    T max;

    // min から max の範囲でランダムな値を取得
    T GetRandom() const
    {
        if (min == max) return min;
        if constexpr (std::is_integral_v<T>)
        {
            // 整数用（min?max の整数）
            return min + rand() % (max - min + 1);
        }
        else
        {
            // 浮動小数・Vector など
            float t = static_cast<float>(rand()) / RAND_MAX;
            return min + (max - min) * t;
        }
    }
};

struct CurvePoint
{
    float time;   // 0.0～1.0
    float value;
};

struct FloatCurve
{
    std::vector<CurvePoint> points;

    float Evaluate(float t) const
    {
        if (points.empty()) return 1.0f;

        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            const auto& p0 = points[i];
            const auto& p1 = points[i + 1];

            if (t >= p0.time && t <= p1.time)
            {
                float lerpT = (t - p0.time) / (p1.time - p0.time);
                return p0.value + (p1.value - p0.value) * lerpT;
            }
        }

        return points.back().value;
    }
};


// エフェクトハンドル
typedef int EffectHandle;


struct EffectAttachInfo
{
    EffectHandle handle;
    std::weak_ptr<SceneComponent> target;

    bool followPosition = true;
    bool followRotation = true;

    float emitInterval = 0.03f;   // ★ ここ重要
    float emitTimer = 0.0f;

    float lifeTime = 1.0f;        // ★ エフェクト全体寿命
    float elapsed = 0.0f;
};


class EffectManager
{
public:
    EffectManager() = default;
    ~EffectManager() = default;

    struct EmitterShapeData;

    // エフェクトデータクリア
    static void ClearAll();

    // 新しいエフェクトデータ追加用のハンドル取得
    static EffectHandle CreateEffectData();

    // エフェクトデータ読み込み
    static EffectHandle LoadEffectData(const std::string& filePath);

    // エフェクトデータ読み込み（ダイアログ表示）
    static EffectHandle LoadEffectDataWithDialog();

    // エフェクトデータ保存
    static void SaveEffectData(EffectHandle handle, const std::string& filePath);

    // エフェクトデータ保存（ダイアログ表示）
    static void SaveEffectDataWithDialog(EffectHandle handle);

    // エフェクト再生
    static void Play(EffectHandle handle, const DirectX::XMFLOAT3& position = {}, const DirectX::XMFLOAT3& rotationEulerDegree = {});

    // エフェクト再生（コンポーネントにアタッチ）
    static void PlayAttached(EffectHandle handle, const std::shared_ptr<SceneComponent>& target, bool followPosition = true, bool followRotation = true);

    // 全エフェクト停止
    static void StopAll();

    // エフェクトデータコピー
    static EffectHandle CopyEffectData(EffectHandle srcHandle);

    // エフェクトデータ取得
    struct EffectData;
    static EffectData& GetEffectData(EffectHandle handle);

public:

    //初期化
    static void Initialize();

    //更新
    static void Update(float deltaTime);

    //描画
    static void Render(ID3D11DeviceContext* immediateContext);


    //エディタGUI描画
    //static void DrawGUI();

        //
    static void EmitParticle(EffectHandle handle, const XMFLOAT3& pos, const XMFLOAT3& rot);

private:
    static int RegisterCurve(const FloatCurve& curve); // カーブ → 1Dテクスチャ化関数

    static void ClearEffectData(); // エフェクトデータクリア

    static void SaveAllEffectData(); // 全エフェクトデータ保存

    static void ReInitializeParticleSystem(); // パーティクルシステム再初期化

    // 形状エミッタ設定適用
    static void ApplyShapeEmitterSettings(const EmitterShapeData& settings, CoreComputeParticleSystem::EmitParticleData& emitData, int index, int emitCount);


    // ランダム値取得
    static float Random(float min, float max);

    // ランダムなボックス内位置取得
    static Vector3 RandomBoxPosition(const Vector3& size);

    // ランダム方向ベクトル取得
    static Vector3 RandomDirection();

    // ランダム上半球方向ベクトル取得
    static Vector3 RandomHemisphereDirection(const Vector3& normal);

    // 指定角度内のランダム方向ベクトル取得
    static Vector3 RandomConeDirection(const Vector3& dir, float coneAngle);

    static void UpdateCurveTexture();

public:
    // 描画モード
    enum class RenderingMode : uint8_t
    {
        Billboard = 0,		// ビルボード
        StretchedBillboard,	// ストレッチドビルボード
        FixedRotation,		// 固定回転
    };
    // 形状定義
    enum class ShapeType : uint8_t
    {
        Point = 0,			// 点
        Ring,				// リング
        Sphere,				// 球
        Cylinder,			// 円柱
    };
    // 方向生成モード
    enum class DirectionMode : uint8_t
    {
        Default = 0,   // EmitterMotionData::velocity に従う
        Axis,          // 指定軸方向
        Random,        // ランダム方向
        Outward,       // 中心から外へ
        Inward,        // 中心に向かう
        Normal,        // 形状法線方向
    };
    // エミット設定構造体
    struct EmitterEmitData
    {
        int maxParticles{ 1000 };			// 最大パーティクル数 (未使用)
        Range<int> emitCount{ 10,10 };			// エミット数
        Range<float> initialDelay{ 0,0 };		// 初期遅延時間
        Range<float> emitInterval{ 0,0 };		// エミット間隔
        float emitRate = 10.0f; // per second// 1秒あたり何個出すか
        float emitterLifeTime = 0.5f; //エミッタの寿命（蛇口の寿命）
        bool loop = false;// エミッタのループフラグ
        bool isBurst = false;// バースト
        int burstCount = 0;
        float emissivePower = 1.0f; // 発光強度
        Vector3 positionOffset;					// 生成位置
        Range<Vector3> rotationEuler;					// 回転
    };
    // 形状エミッタ設定構造体
    struct EmitterShapeData
    {
        ShapeType shape = ShapeType::Point;						// 形状タイプ
        DirectionMode directionMode = DirectionMode::Default;	// 方向生成モード
        Vector3 directionAxis{ 0,1,0 };							// 方向軸（DirectionMode::Axisで使用）
        Range<float> speed = { 1.0f,1.0f };						// 速度（DirectionModeで使用）
        float radius = 1.0f;									// 円/球で使用
        float height = 1.0f;									// Cylinderで使用
    };
    // 動作設定構造体
    struct EmitterMotionData
    {
        Range<Vector3> velocity;					// 初速
        Range<Vector3> acceleration;				// 加速度
        Range<float> lifeTime{ 1.0f, 1.0f };		// particleの 生存時間 
        bool useGravity{ false };					// 重力使用フラグ
    };
    // ビジュアル設定構造体
    struct EmitterVisualData
    {
        RenderingMode renderingMode = RenderingMode::Billboard; // 描画モード
        std::string texturePath;								// テクスチャパス
        DirectX::XMUINT2 textureSplitCount{ 1, 1 };				// テクスチャ分割数
        BLEND_STATE blendState = BLEND_STATE::ADD;	// ブレンドステート
        Range<Vector2> startSize{ { 1,1 }, { 1,1 } };			// 開始サイズ
        Range<Vector2> endSize{ { 1,1 }, { 1,1 } };				// 終了サイズ
        Range<CoreColor> startColor;								// 開始色
        Range<CoreColor> endColor;									// 終了色

        FloatCurve sizeCurve;
        bool dirty = true; // GUIで編集されたかどうか（テクスチャ再生成フラグ）
        int curveIndex = 1; // カーブテクスチャ内のインデックス
    };
    // エミッタデータ構造体
    struct ParticleEmitterData
    {
        std::string name;				// エミッタ名

        EmitterEmitData emitData;		// エミット設定
        EmitterShapeData shapeData;		// 形状エミッタ設定
        EmitterMotionData motionData;	// 動作設定
        EmitterVisualData visualData;	// ビジュアル設定
    };
    // エフェクトデータ構造体
    struct EffectData
    {
        std::string name; // エフェクト名
        std::vector<ParticleEmitterData> emitters; // エミッタデータリスト
    private:
        friend class EffectManager;
        EffectHandle handle = -1; // エフェクトハンドル
        std::string filePath; // エフェクトデータファイルパス
    };

    struct ActiveEmitter
    {
        EffectHandle handle;
        const ParticleEmitterData* data;

        float emitAccumulator = 0.0f;

        float lifeTime = 1.0f;
        float elapsed = 0.0f;
        bool loop = true;
        bool hasBurst = false;

        XMFLOAT3 position;
        XMFLOAT3 rotation;
    };

    static inline std::vector<EffectData> effectData; // エフェクトデータリスト

    static inline std::vector<EffectAttachInfo> attachedEffects; // エフェクトデータリスト

    static inline std::vector<ActiveEmitter> activeEmitters;
private:
    friend class EffectEditor;
    //エディタが開いているか
    static inline bool isOpen = false;

    //パーティクルシステム管理用マップ定義
    using ParticleSystems = std::unordered_map<std::string/*texturePath*/, std::unique_ptr<CoreComputeParticleSystem>>;

    //パーティクルシステムリスト
    static inline std::unordered_map<std::string/*effectFilePath*/, ParticleSystems> particleSystems;

    static inline std::vector<std::vector<float>> curveData; // 各カーブのサンプル
    static inline Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> curveArraySRV;
};
