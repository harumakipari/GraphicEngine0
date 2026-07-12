#pragma once
#include "Components/Controller/ControllerComponent.h"
#include "Game/Actors/Base/Character.h"
#include "Graphics/Renderer/TrailRenderer.h"

class TitlePlayer :public Character
{
public:
    explicit TitlePlayer(const std::string& modelName) :Character(modelName)
    {
        mass = 50.0f;
        hp = maxHp;
    }
    void Initialize(const Transform& transform)override;

    void Update(float deltaTime)override;

    void DrawImGuiDetails()override;

    void Finalize()override {}

    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

    void OnAnimationChanged() override;

    // アニメーションステート関連のフラグをリセットする
    void ResetAnimationStateFlag();

    // 軌跡を描画する処理
    void RenderTrail(ID3D11DeviceContext* immediateContext);

private:
    // プレイヤーのマックスHP
    int maxHp = 100;
public:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;


    std::shared_ptr<SceneComponent> swordRootComponent; // 剣の根元のコンポーネント
    std::shared_ptr<SceneComponent> swordMiddleComponent; // 剣の中間のコンポーネント
    std::shared_ptr<SceneComponent> swordTipComponent;  // 剣の先端のコンポーネント



    float elapsedTime_ = 0.0f;
    bool isGrounded_ = false;

    struct TrailPoint
    {
        DirectX::XMFLOAT3 position;
        float life;
    };
    std::vector<TrailPoint> trailPoints;


    // 剣の描画用メッシュコンポーネント
    std::shared_ptr<SkeletalMeshComponent> swordMeshComponent;

    // 剣の残像用
    struct SwordGhost
    {
        std::shared_ptr<SkeletalMeshComponent> swordMeshComp;
        DirectX::XMFLOAT4X4 world = { };
        float alpha;
        bool isVisible = false;
    };
    std::array<SwordGhost, 8> ghosts;

private:
    DirectX::XMFLOAT3 prevSwordTip; // 前フレームの剣先の位置
    float hitStopTimer = 0.0f; // ヒットストップのタイマー

    DirectX::XMFLOAT3 prevSwordRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevSwordMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevSwordTipPos = { 0.0f,0.0f,0.0f };

    // 剣をしまうときの描画用メッシュコンポーネント
    std::shared_ptr<SkeletalMeshComponent> swordSheathMeshComponent;

    // 剣の残像の調整値
    float swordGhostElapsedTime = 0.0f;
    int swordGhostIndex = 0;
    float ghostInterval = 0.015f; // 残像を出す間隔
    float ghostFadeTime = 0.6f; // 残像が残る時間
    DirectX::XMFLOAT3 swordGhostColor = { 0.5f,0.8f,1.6f }; // 残像の剣のベースカラー
    float swordGhostEmissive = 2.0f;    // 残像のemissiveColor
    bool isAttackActive = false;    // プレイヤーが攻撃状態に入る
    DirectX::XMFLOAT3 ghostEdgeColor = { 0.0f,0.042f,0.253f };  // 残像の剣のベースカラー
    DirectX::XMFLOAT3 ghostInnerColor = { 1.0f,1.0f,1.0f }; // 残像の剣のベースカラー
    float ghostEdgeWidth = 1.0f; // 残像の輪郭
    DirectX::XMFLOAT4X4 prevSwordWorld; // 前回の姿勢
    bool isPrevSwordWorldValid = false;

    // 剣の軌跡
    Trail trail;
    float trailRemainTime = 0.8f; // 残像が残る時間

};
