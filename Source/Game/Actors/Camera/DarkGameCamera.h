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
    struct CameraCompositionSettings
    {
        float distance = 5.0f;
        float height = 0.0f;
        float lookTargetHeight = 0.0f;
        float fovDegree = 35.0f;
        float horizontalOffset = 0.0f;
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
        if (mode == requestMode)
        {
            return;
        }
        const CameraMode previousRequestMode = requestMode;
        if ((previousRequestMode == CameraMode::TPS && mode == CameraMode::LockOn) ||
            (previousRequestMode == CameraMode::LockOn && mode == CameraMode::TPS))
        {
            BeginLockOnTransitionDiagnostics(previousRequestMode, mode);
        }
        if (mode == CameraMode::LockOn && requestMode != CameraMode::LockOn)
        {
            ResetLockOnAdaptiveState();
        }
        if (isBlending)
        {
            requestMode = mode;
            StartBlend(currentMode, requestMode);
            return;
        }
        requestMode = mode;
    }

    // ブレンドを開始する
    void StartBlend(CameraMode current, CameraMode request);

    // カメラをplayerのforward方向に向ける
    void RotateToPlayerForward();

    // 外部のカメラアクターとのブレンド用の関数
    void StartExternalBlend(const CameraPose& start, const CameraPose& target, float duration,std::function<void()> finishExternalBlend);

    // ムービーカメラコンポーネントからカメラポーズを作成する
    CameraPose CreatePoseFromMovie(const std::shared_ptr<MovieCameraComponent>& movieCamera);

    // Focus状態のポーズを作成する
    CameraPose CreateFocusPose();
private:
    // 外部のカメラアクターとのブレンド状態を更新する
    void UpdateExternalBlend(float deltaTime);

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

    // LockOn構図の適応値を更新する
    void UpdateLockOnComposition(float deltaTime);

    // LockOn開始時に前回の適応値を持ち越さない
    void ResetLockOnAdaptiveState();

    // TPS <-> LockOn切替時の壁際構図を追跡する
    void BeginLockOnTransitionDiagnostics(CameraMode from, CameraMode to);
    void UpdateLockOnTransitionDiagnostics();

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
    float blendStartFovDegree = 35.0f;
    float blendTargetFovDegree = 35.0f;
    float blendStartEyeYaw = 0.0f;
    float blendStartEyePitch = 0.0f;
    float blendStartEyeDistance = 0.0f;

    bool isBlending = false;

    // 調整値
    float rotateSpeed = 2.0f;
    float minPitchDegree = -50.0f; // pitchの最小角度
    float maxPitchDegree = 40.0f;  // pitchの最大角度
    float cameraDistance = 5.0f;
    float cameraHeight = 0.0f;
    float focusDistance = 0.0f;

    // モード別の構図調整値（初期値は従来値相当）
    CameraCompositionSettings tpsSettings = { 6.45f, 0.05f, 0.75f, 35.0f, 0.0f };
    //CameraCompositionSettings tpsSettings = { 7.15f, 0.5f, 1.05f, 35.0f, 0.0f };
    CameraCompositionSettings focusSettings = { 6.45f, 0.05f, 0.75f, 35.0f, 0.0f };
    CameraCompositionSettings lockOnSettings = { 9.7f, 0.4f, -0.3f, 35.0f, 0.0f };
    float lockOnPlayerLookHeight = -.15f;
    float lockOnEnemyLookHeight = -1.4f;

    // LockOn適応構図。広い場所では基準値を変更しない。
    float lockOnCollisionStartRatio = 0.9f;
    float lockOnCollisionFullRatio = 0.45f;
    float lockOnWallTargetWeight = 0.72f;
    float lockOnWallHorizontalScale = 1.0f;
    float lockOnWallDistanceScale = 0.85f;
    float lockOnCollisionRatioHysteresis = 0.02f;
    float lockOnDistanceStart = 4.0f;
    float lockOnDistanceFull = 9.0f;
    float lockOnMaxDistanceAdd = 2.0f;
    float lockOnZoomOutSpeed = 0.01f;
    float lockOnZoomInSpeed = 0.01f;
    float lockOnDistanceDeadZone = 0.15f;
    float lockOnCompositionLerpSpeed = 6.0f;

    float lockOnCollisionStrength = 0.0f;
    float lockOnCollisionRatioForAdaptive = 1.0f;
    float lockOnDistanceStrength = 0.0f;
    float lockOnDistanceForZoom = 0.0f;
    float desiredLockOnCameraDistance = 6.55f;
    float currentLockOnCameraDistance = 6.55f;
    float currentLockOnZoomSpeed = 0.0f;
    float adaptiveLockOnTargetWeight = 0.72f;
    float adaptiveLockOnHorizontalOffset = 0.0f;
    float adaptiveLockOnCameraDistance = 6.55f;

    // ランタイム調査値
    DirectX::XMFLOAT3 desiredEyePosition{};
    DirectX::XMFLOAT3 collisionPreEyePosition{};
    DirectX::XMFLOAT3 collisionPostEyePosition{};
    float desiredCameraDistance = 0.0f;
    float actualCameraDistance = 0.0f;
    float lockOnEnemyDistance = 0.0f;

    // TPS <-> LockOn診断（切替後30フレームを記録）
    bool lockOnTransitionDiagnosticsActive = false;
    int lockOnTransitionDiagnosticsFrame = 0;
    CameraMode lockOnTransitionTo = CameraMode::TPS;
    DirectX::XMFLOAT3 transitionStartDesiredEye{};
    DirectX::XMFLOAT3 transitionStartCollisionPostEye{};
    float transitionStartCollisionRatio = 1.0f;
    float transitionStartAdaptiveDistance = 0.0f;
    float transitionStartAdaptiveTargetWeight = 0.0f;
    float transitionMaxDesiredEyeDelta = 0.0f;
    float transitionMaxCollisionPostEyeDelta = 0.0f;
    float transitionMaxCollisionRatioDelta = 0.0f;
    float transitionMaxAdaptiveDistanceDelta = 0.0f;
    float transitionMaxAdaptiveTargetWeightDelta = 0.0f;

    // ロックオンカメラの調整値
    // カメラを敵方向から何度横へ振るか
    float lockOnYawOffsetDegree = -0.0f;
    // targetをどこに置くか
    // 0 = Player
    // 1 = Enemy
    float lockOnTargetWeight = 0.72f;
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

    // フォーカス時の
    // float focus

    // 外部のカメラとのBlend用の変数
    CameraPose externalStartPose;
    CameraPose externalTargetPose;

    float externalBlendTime = 0.0f;
    float externalBlendDuration = 0.0f;

    bool isExternalBlending = false;
    std::function<void()> finishedExternalBlend;

    float startDistance = 5.0f;
    float targetDistance = 5.0f;
};
