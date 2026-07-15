#pragma once
#include "Camera.h"
#include "Components/Controller/ControllerComponent.h"

class DarkCameraActor :public Camera
{
public:
    enum class CameraMode :uint8_t
    {
        TPS,
        Focus,
        LockOn,
    };
    struct CameraPose
    {
        DirectX::XMFLOAT3 eye;
        DirectX::XMFLOAT3 target;
        float yaw;
        float pitch;
    };
public:
    //引数付きコンストラクタ
    explicit DarkCameraActor(const std::string& actorName) :Camera(actorName)
    {
    }
    virtual ~DarkCameraActor() = default;
    virtual void Initialize(const Transform& transform)override;

    void Update(float deltaTime) override;

    // パースペクティブ設定
    void SetPerspective(const float fovY, const float aspect, const float nearZ, const float farZ)const
    {
        mainCameraComponent->SetPerspective(fovY, aspect, nearZ, farZ);
    }

    // Yaw と Pitch を最初に更新する
    void InitSetYawAndPitch(const float yaw, const float pitch)const
    {
        mainCameraComponent->SetYawAndPitch(yaw, pitch);
    }

    // 敵のコンポーネントを設定
    void SetEnemyHead(const std::shared_ptr<SceneComponent>& enemyHead)
    {
        this->enemyHead = enemyHead;
    }

    // プレイヤーのコンポーネントを設定
    void SetPlayerHead(const std::shared_ptr<SceneComponent>& playerHead)
    {
        this->playerHead = playerHead;
    }

    void DrawImGuiDetails() override;

private:
    // 目標の方向を更新する関数
    void UpdateDesireRotation();

    // 実際の方向を更新する関数
    void UpdateRotation();

    // Eyeを計算する関数
    void CalculatePose();

    // 当たり判定を考慮する関数
    void ResolveCollision();

    // 適応する


private:
    std::weak_ptr<SceneComponent> enemyHead; // 敵の位置
    std::weak_ptr<SceneComponent> playerHead; // プレイヤーの位置

    std::shared_ptr<InputComponent> inputComponent;

    CameraMode cameraMode = CameraMode::TPS;

    CameraPose currentPose;

    float currentYaw = 0.0f;
    float desiredYaw = 0.0f;

    float currentPitch = 0.0f;
    float desiredPitch = 0.0f;


    // 調整値
    float rotateSpeed = 2.0f;
    float minPitchDegree = -26.5f; // pitchの最小角度
    float maxPitchDegree = 22.5f;  // pitchの最大角度

    // フォーカス時の
    // float focus
};

