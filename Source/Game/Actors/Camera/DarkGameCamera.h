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
        Death,
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
    struct CameraShakePreset
    {
        float intensity = 1.0f;
        float duration = 0.28f;
        float frequency = 12.0f;
        float positionAmount = 0.035f;
        float targetAmount = 0.18f;
    };
    struct DeathCameraSettings
    {
        float foregroundDistance = 3.15f;
        float sideOffset = -1.1f;
        float cameraHeight = -0.3f;
        float lookHeight = -0.85f;
        float bossLookWeight = 0.15f;
        float deathBlendTime = 1.0f;
    };
    //struct DeathCameraSettings
    //{
    //    float foregroundDistance = 3.6f;
    //    float sideOffset = -2.5f;
    //    float cameraHeight = -0.55f;
    //    float lookHeight = 0.5f;
    //    float bossLookWeight = 0.65f;
    //    float deathBlendTime = 1.0f;
    //};

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

    std::shared_ptr<SceneComponent> GetEnemyHead()const
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

    // プレイヤーやボスに対する死亡時のカメラに変更
    void StartDeathMode(std::function<void()> onBlendFinished);

    // カメラをplayerのforward方向に向ける
    void RotateToPlayerForward();

    // 外部のカメラアクターとのブレンド用の関数
    void StartExternalBlend(const CameraPose& start, const CameraPose& target, float duration,std::function<void()> finishExternalBlend);

    // ムービーカメラコンポーネントからカメラポーズを作成する
    CameraPose CreatePoseFromMovie(const std::shared_ptr<MovieCameraComponent>& movieCamera);

    // Focus状態のポーズを作成する
    CameraPose CreateFocusPose();

    void PlayCameraShake(float intensity, float duration, float frequency,float positionAmount, float targetAmount);
    void PlayCameraShakePreset(const std::string& presetName);
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

    void UpdateCameraShake(float deltaTime);

    const CameraShakePreset* FindCameraShakePreset(const std::string& presetName) const;

    // Eyeを計算する関数
    CameraPose CalculatePose(CameraMode mode, const DirectX::XMFLOAT3& playerPos, float yaw, float pitch) const;

    // LockOn構図の適応値を更新する
    void UpdateLockOnComposition(float deltaTime);

    float CalculateRequiredLockOnFramingDistance();

    // Collision 後の実カメラ位置から、Safe Frame を満たす垂直 FOV を逆算する
    float CalculateRequiredLockOnFov(bool& outValid) const;
    void UpdateLockOnFovFallback(float deltaTime);

    // LockOn開始時に前回の適応値を持ち越さない
    void ResetLockOnAdaptiveState();

    // TPS <-> LockOn切替時の壁際構図を追跡する
    void BeginLockOnTransitionDiagnostics(CameraMode from, CameraMode to);
    void UpdateLockOnTransitionDiagnostics();

    bool IsBossBattle() const;
    float GetFovDegreeForMode(CameraMode mode) const;
    void ResetCameraTuning();

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
    std::function<void()> deathBlendFinished;

    // 調整値
    float rotateSpeed = 2.0f;
    float minPitchDegree = -50.0f; // pitchの最小角度
    float maxPitchDegree = 40.0f;  // pitchの最大角度
    float cameraDistance = 5.0f;
    float cameraHeight = 0.0f;
    float focusDistance = 0.0f;

    // カメラシェイクのプリセット名
    inline static constexpr const char* BossHeavyLandingPresetName = "BossHeavyLanding";
    inline static constexpr const char* BossWallImpactPresetName = "BossWallImpact";
    inline static constexpr const char* RushFinalPresetName = "RushFinal";
    // カメラシェイクのプリセット
    CameraShakePreset bossHeavyLandingShake{1.0f,0.28f,12.0f,0.035f,0.18f};
    CameraShakePreset bossWallImpactShake{ 1.15f, 0.36f, 10.0f, 0.05f, 0.22f };
    CameraShakePreset rushFinalShake{ 0.9f, 0.16f, 18.0f, 0.02f, 0.14f };

    // Active shake state. Preset values are copied here when a shake starts.
    bool shakeActive = false;
    float shakeElapsedTime = 0.0f;
    float shakeDuration = 0.0f;
    float shakeIntensity = 0.0f;
    float shakeFrequency = 0.0f;
    float shakePositionAmount = 0.0f;
    float shakeTargetAmount = 0.0f;
    float currentShakeEnvelope = 0.0f;
    DirectX::XMFLOAT3 shakePositionOffset{};
    DirectX::XMFLOAT3 shakeTargetOffset{};

    // モード別の構図調整値（初期値は従来値相当）
    CameraCompositionSettings tpsSettings = { 6.45f, 0.05f, 0.75f, 35.0f, 0.0f };
    CameraCompositionSettings focusSettings = { 6.45f, 0.05f, 0.75f, 35.0f, 0.0f };
    CameraCompositionSettings lockOnSettings = { 10.0f, 0.4f, -0.3f, 45.0f, 0.0f };
    DeathCameraSettings deathCameraSettings{};
    float bossTpsFovDegree = 44.0f;
    float lockOnPlayerLookHeight = -.15f;
    float lockOnEnemyLookHeight = -0.75f;

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
    float lockOnZoomOutSpeed = 1.2f;
    float lockOnZoomInSpeed = 1.2f;
    float lockOnDistanceDeadZone = 0.15f;
    float lockOnCompositionLerpSpeed = 6.0f;
    float lockOnSafeFrameHorizontalMargin = 0.12f;
    float lockOnSafeFrameVerticalMargin = 0.12f;
    float lockOnMaxFallbackFovDegree = 52.0f;
    float lockOnFovExpandSpeed = 35.0f;
    float lockOnFovReturnSpeed = 12.0f;
    float lockOnFovFallbackEnterDeficit = 0.25f;
    float lockOnFovFallbackExitDeficit = 0.08f;
    float lockOnFovFallbackReturnDelay = 0.20f;

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
    float lockOnHorizontalExtent = 0.0f;
    float lockOnVerticalExtent = 0.0f;
    float lockOnRequiredXDistance = 0.0f;
    float lockOnRequiredYDistance = 0.0f;
    float lockOnRequiredFramingDistance = 0.0f;
    float lockOnExistingAdaptiveDistance = 0.0f;
    float lockOnFramingDeficit = 0.0f;
    bool lockOnFramingActive = false;
    float lockOnRequiredFovDegree = 45.0f;
    float lockOnTargetFovDegree = 45.0f;
    float lockOnCurrentFovDegree = 45.0f;
    float lockOnFovReturnDelayElapsed = 0.0f;
    bool lockOnFovFallbackActive = false;

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
    float lockOnTargetWeight = 0.3f;
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
    std::string cameraCollisionHitName = "None";
    bool showCameraCollisionDebug = false;

    CameraCompositionSettings initialTpsSettings{};
    CameraCompositionSettings initialLockOnSettings{};
    float initialBossTpsFovDegree = 44.0f;
    float initialLockOnEnemyLookHeight = 0.0f;
    float initialLockOnTargetWeight = 0.0f;
    float initialLockOnZoomInSpeed = 0.0f;
    float initialLockOnZoomOutSpeed = 0.0f;

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
