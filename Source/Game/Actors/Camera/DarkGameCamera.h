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
    struct CameraDirectionInfo
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

    const std::shared_ptr<SceneComponent>& GetEnemyHead()const
    {
        return this->enemyHead.lock();
    }

    float GetCameraCollisionRatio()const
    {
        return cameraCollisionRatio;
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

    // カメラモードで
    CameraMode GetMovementMode()const { return requestMode; }

    // カメラモードをセットする
    void SetRequestMode(const CameraMode mode)
    {
        if (isBlending)
        {
            return;
        }
        requestMode = mode;
    }

    // ブレンドを開始する
    void StartBlend(CameraMode current, CameraMode request);

    //壁近いときの関数
    void AddWallNearFunc(const std::function<void()>& wallNearFunc)
    {
        this->wallNearFunc = wallNearFunc;
    }
private:
    // ブレンド状態を更新する
    void UpdateBlend(float deltaTime);

    // フォーカスカメラの情報を作成する
    CameraDirectionInfo CreateFocusInfo();

    // ロックオンカメラの情報を作成する
    CameraDirectionInfo CreateLockOnInfo();

    // 目標の方向を更新する関数
    void UpdateDesireRotation(float deltaTime);

    // 実際の方向を更新する関数
    void UpdateRotation(float deltaTime);

    // Eyeを計算する関数
    CameraPose CalculatePose(CameraMode mode, const DirectX::XMFLOAT3& playerPos, float yaw, float pitch) const;

    // ロックオンのカメラ距離を計算する関数
    float CalculateLockOnDistance() const;

    // 当たり判定を考慮する関数
    DirectX::XMFLOAT3 ResolveCameraCollision(DirectX::XMFLOAT3 target, DirectX::XMFLOAT3 eye);

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
    float blendDuration = 0.30f;

    bool isBlending = false;

    // 調整値
    float rotateSpeed = 2.0f;
    float minPitchDegree = -50.0f; // pitchの最小角度
    float maxPitchDegree = 40.0f;  // pitchの最大角度
    float cameraDistance = 5.0f;
    float cameraHeight = 0.0f;
    float focusDistance = 0.0f;

    // ロックオンカメラの調整値
    // カメラを敵方向から何度横へ振るか
    float lockOnYawOffsetDegree = -26.0f;
    // targetをどこに置くか
    // 0 = Player
    // 1 = Enemy
    float lockOnTargetWeight = 0.1f;
    // 基本距離
    float lockOnCameraDistance = 5.0f;
    // 敵との距離による増加量
    float lockOnDistanceScale = 0.25f;
    // 最大距離
    float lockOnMaxDistance = 8.0f;
    // Pitch
    float lockOnPitchDegree = -10.0f;
    // 当たり判定のスフィアキャストの球の大きさ
    float sphereCastRadius = 0.01f;

    float lockOnTargetNearWall = 0.005f;
    float lockOnTargetNormal = 0.11f;
    float wallDistance = 7.1f;
    float cameraHitDistance = 0.0f; // カメラと壁の距離
    bool cameraHitWall = false;   // カメラが壁に当たったかどうか
    float cameraFov = 35.0f;
    float wallBlend = 0.0f;
    float smoothHitDistance = 0.0f;
    float cameraCollisionRatio = 1.0f;

    int unstableFrameCount = 0; // 

    std::function<void()> wallNearFunc;
    // フォーカス時の
    // float focus
};

