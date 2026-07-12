#pragma once
#include "Core/Actor.h"

class EasingRunner;

class GruxEnemyEyeActor :public Actor
{
public:
    GruxEnemyEyeActor() = default;
    ~GruxEnemyEyeActor() override {}

    GruxEnemyEyeActor(const std::string& modelName) :Actor(modelName) {}

    //コピーコンストラクタとコピー代入演算子を禁止にする
    GruxEnemyEyeActor(const GruxEnemyEyeActor&) = delete;
    GruxEnemyEyeActor& operator=(const GruxEnemyEyeActor&) = delete;

    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    void DrawImGuiDetails() override;

    void StartEyeFlash(const std::function<void()>& onFinished = nullptr);

    // 目のモデルを小さくする処理
    void ToSmallEyeModel(float duration, std::function<void()> finished = nullptr);

private:
    // 目のフレアのスケールが大きくなる処理を開始
    void StartEyeFlareScale();

private:
    // 目のフレアのスケール表現用のコンポーネント
    std::unique_ptr<EasingRunner> eyeFlareEasingComponent;
    // 目のエミッシブ表現用のコンポーネント
    std::unique_ptr<EasingRunner> eyeEmissiveEasingComponent;

    // 目のスケール表現用のコンポーネント
    std::unique_ptr<EasingRunner> eyeEasingComponent;


    // 左目の描画用コンポーネントを追加　暗闇で光る目の表現用
    std::shared_ptr<SkeletalMeshComponent> leftEyeMeshComponent;
    // 左目の描画用コンポーネントを追加　横に光るフレアの表現用
    std::shared_ptr<SkeletalMeshComponent> leftEyeFlareMeshComponent;
    // 右目の描画用コンポーネントを追加　暗闇で光る目の表現用
    std::shared_ptr<SkeletalMeshComponent> rightEyeMeshComponent;
    // 右目の描画用コンポーネントを追加　横に光るフレアの表現用
    std::shared_ptr<SkeletalMeshComponent> rightEyeFlareMeshComponent;

    DirectX::XMFLOAT3 eyeFlareOffset = { 0.2f,0.0f,0.f };

    float emissionEyeFactor = 8.0f;
    float emissionEyeFlareFactor = 10.0f;

    DirectX::XMFLOAT4 eyeColor = { 1.0f,0.0f,0.0f,1.0f };
    DirectX::XMFLOAT3 eyeFlareScale = { 0.02f,1.05f,0.0f };
    DirectX::XMFLOAT3 eyeFlareDegreeAngle = { 0.0f,-92.0f,-200.0f };
    DirectX::XMFLOAT3 eyeDegreeAngle = { 8.0f,90.0f,0.0f };
    DirectX::XMFLOAT3 eyeInitScale = { 0.01f,0.01f,0.01f };
    DirectX::XMFLOAT3 eyeScale = { 0.03f,0.02f,0.02f };

    DirectX::XMFLOAT3 leftEyePosition = { -1.572f,1.71f,10.881f };
    DirectX::XMFLOAT3 rightEyePosition = { -1.572f,1.71f,11.2f };


    std::function<void()> onFinished;  // 目玉のフレアの演出が終わった時に呼び出す関数
    float eyeFlareScaleEasingFactor = 0.0f;

    float eyeEmissiveEasingFactor = 0.0f;   // 目のエミッシブのイージング値
    float eyeScaleEasingFactor = 1.0f;   // 目のスケールのイージング値

    // 目玉のフレアのスケールが大きくなる時間
    float eyeFlareAddScaleTime = 0.1f;
    // 目玉のフレアのスケールを小さくするまでの維持時間
    float eyeFlareWaitScaleTime = 0.3f;
    // 目玉のフレアのスケールが小さくきくなる時間
    float eyeFlareSubtractScaleTime = 0.6f;


    // 調整値

};
