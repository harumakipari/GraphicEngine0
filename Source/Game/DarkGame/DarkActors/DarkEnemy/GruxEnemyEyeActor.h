#pragma once
#include "Core/Actor.h"

class EasingRunner;

class GruxEnemyEyeActor :public Actor
{
public:
    GruxEnemyEyeActor() = default;
    ~GruxEnemyEyeActor() override {}

    GruxEnemyEyeActor(const std::string& modelName) :Actor(modelName)
    {
    }

    //コピーコンストラクタとコピー代入演算子を禁止にする
    GruxEnemyEyeActor(const GruxEnemyEyeActor&) = delete;
    GruxEnemyEyeActor& operator=(const GruxEnemyEyeActor&) = delete;

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    void DrawImGuiDetails() override;

    void StartEyeFlash(const std::function<void()>& onFinished = nullptr);

    // 目のモデルを小さくする処理
    void ToSmallEyeModel();

private:
    // 目のフレアのスケールが大きくなる処理を開始
    void StartEyeFlareScale();

private:
    // 目のフレアのスケール表現用のコンポーネント
    std::unique_ptr<EasingRunner> eyeFlareEasingComponent;
    // 目のエミッシブ表現用のコンポーネント
    std::unique_ptr<EasingRunner> eyeEmissiveEasingComponent;


    // 左目の描画用コンポーネントを追加　暗闇で光る目の表現用
    std::shared_ptr<SkeletalMeshComponent> leftEyeMeshComponent;
    // 左目の描画用コンポーネントを追加　横に光るフレアの表現用
    std::shared_ptr<SkeletalMeshComponent> leftEyeFlareMeshComponent;
    // 右目の描画用コンポーネントを追加　暗闇で光る目の表現用
    std::shared_ptr<SkeletalMeshComponent> rightEyeMeshComponent;
    // 右目の描画用コンポーネントを追加　横に光るフレアの表現用
    std::shared_ptr<SkeletalMeshComponent> rightEyeFlareMeshComponent;

    float emissionEyeFactor = 10.0f;
    float emissionEyeFlareFactor = 10.0f;

    DirectX::XMFLOAT4 eyeColor = { 0.011f,0.034f,0.0f,1.0f };
    DirectX::XMFLOAT3 eyeFlareScale = { 0.0f,1.0f,0.05f };
    DirectX::XMFLOAT3 eyeFlareDegreeAngle = { 8.0f,90.0f,0.0f };

    std::function<void()> onFinished;  // 目玉のフレアの演出が終わった時に呼び出す関数
    float eyeFlareScaleEasingFactor = 0.0f;

    float eyeEmissiveEasingFactor = 0.0f;   // 目のエミッシブのイージング値

    // 目玉のフレアのスケールが大きくなる時間
    float eyeFlareAddScaleTime = 0.1f;
    // 目玉のフレアのスケールを小さくするまでの維持時間
    float eyeFlareWaitScaleTime = 0.3f;
    // 目玉のフレアのスケールが小さくきくなる時間
    float eyeFlareSubtractScaleTime = 0.6f;


    // 調整値
    
};
