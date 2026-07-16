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
    // Z注目の情報
    struct FocusInfo
    {
        DirectX::XMFLOAT3 direction;
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

    DirectX::XMFLOAT3 CameraForwardXZ() const
    {
        auto forward = mainCameraComponent->GetForward();
        forward.y = 0.0f;
        return MathHelper::Normalize(forward);
    }

    DirectX::XMFLOAT3 CameraRightXZ() const
    {
        auto right = mainCameraComponent->GetRight();
        right.y = 0.0f;
        return MathHelper::Normalize(right);
    }

    // カメラモードを取得する
    CameraMode GetCameraMode()const { return currentMode; }

    // カメラモードをセットする
    void SetRequestMode(const CameraMode mode) { requestMode = mode; }

    // ブレンドを開始する
    void StartBlend(CameraMode current, CameraMode request);

private:
    // ブレンド状態を更新する
    void UpdateBlend(float deltaTime);

    // フォーカスカメラの情報を作成する
    FocusInfo CreateFocusInfo();
    //void SyncFocusCamera();

    // 目標の方向を更新する関数
    void UpdateDesireRotation(float deltaTime);

    // 実際の方向を更新する関数
    void UpdateRotation(float deltaTime);

    // Eyeを計算する関数
    //void CalculatePose(CameraMode cameraMode);
    CameraPose CalculatePose(CameraMode mode, const DirectX::XMFLOAT3& playerPos, float yaw, float pitch) const;

    // 当たり判定を考慮する関数
    void ResolveCollision();

    // 適応する


private:
    std::weak_ptr<SceneComponent> enemyHead; // 敵の位置
    std::weak_ptr<SceneComponent> playerHead; // プレイヤーの位置

    std::shared_ptr<InputComponent> inputComponent;

    CameraMode currentMode = CameraMode::TPS;   // 現在有効なモード
    CameraMode requestMode = CameraMode::TPS;   // 入力が要求しているモード


    CameraPose currentPose;

    float currentYaw = 0.0f;
    float desiredYaw = 0.0f;

    float currentPitch = 0.0f;
    float desiredPitch = 0.0f;

    // ブレンド用のPoseを作成する
    CameraPose blendStartPose;
    CameraPose blendTargetPose;

    float blendTime = 0.0f;
    float blendDuration = 0.25f;

    bool isBlending = false;

    // 調整値
    float rotateSpeed = 2.0f;
    float minPitchDegree = -50.0f; // pitchの最小角度
    float maxPitchDegree = 40.0f;  // pitchの最大角度
    float cameraDistance = 5.0f;
    float cameraHeight = 0.0f;
    float focusDistance = 0.0f;

    // フォーカス時の
    // float focus
};

