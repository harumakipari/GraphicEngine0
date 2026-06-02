#pragma once
#include <stack>
#include <memory>
#include "Game/Actors/Base/Character.h"
#include "Engine/Input/GamePad.h"

#include "Components/Controller/ControllerComponent.h"
#include "Components/Render/MeshComponent.h"

#include "Core/ActorManager.h"
#include "Components/Effect/ParticleComponent.h"

class IInteractable;

class Player :public Character
{
public:
    explicit Player(const std::string& modelName) :Character(modelName)
    {
        mass = 50.0f;
        hp = maxHp;
    }
    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    void DrawImGuiDetails()override;

    void Finalize()override {}
private:
    // 火花エフェクトの生成
    void SpawnSpark(DirectX::XMFLOAT3 hitPosition);

    // 剣の攻撃判定
    void CheckSwordLineHit(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end);

public:
    //当たった時の処理
    void TakeDamage(int damage);

    // 攻撃ヒット時の処理
    void DoAttackHit();

public:
    //当たった相手を記録するためのセット 火花エフェクトの生成やダメージの適用を一度だけ行うために使用
    std::unordered_set<Actor*> hitTargets;
    bool hasPrevSwordTip = false; // 前フレームの剣先の位置が有効かどうか
    bool hasSpawnedThisAttack = false; // 今攻撃でエフェクトを生成したかどうか

    bool invincible = false; // 無敵状態かどうか

    //上方向への力
    float jumpPower = 5.0f;

    //武器のノード番号
    //size_t nodeAttackIndex = 153; //"VB root Weapon"
    size_t nodeAttackIndex = 0; //"VB root Weapon"

private:
    GamePad pad;
    //頭の天辺のノード番号
    size_t nodeTopIndex = 126;   //"hair_top_mid_01"
    //頭の下のノード番号
    size_t nodeBottomIndex = 146;    //"ik_foot_root"

    // プレイヤーの現在のスピード
    float currentSpeed = 5.0f;
    // プレイヤーはアイテムを持っていないときのスピード
    float noItemSpeed = 10.0f;
    // プレイヤーの Max スピード
    float maxSpeed = 5.0f;
    // プレイヤーの Min スピード
    float minSpeed = 2.0f;
    // プレイヤーの現在の回転スピード
    float currentTurnSpeed = 720.0f;
    // プレイヤーの Max 回転スピード
    float maxTurnSpeed = 720.0f;
    // プレイヤーの Min 回転スピード
    float minTurnSpeed = 90.0f;
    // プレイヤーのマックスHP
    int maxHp = 100;

    bool isIdleEnd = false;
public:
    // EraseInArea で使用
    // 次のフレームで適応する無敵時間のフラグを立てる
    bool applyInvincibilityNextFrame = false;
    // 初回だけダメージを換算したいから
    bool hasDamageThisFrame = false;
    // ダメージ記録
    bool hitLeftThisFrame = false;
    bool hitRightThisFrame = false;
    int currentFrameDamage = 0;

    DirectX::XMFLOAT3 angle = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prePosition = { 0.0f,0.0f,0.0f };

    int leftItemMax = 15;
    int rightItemMax = 15;

    // 初回のサイズ
    DirectX::XMFLOAT3 leftFirstPos = { -0.5f,-0.5f,0.2f };
    DirectX::XMFLOAT3 rightFirstPos = { 0.5f,-0.5f,0.2f };

private:
    // プレイヤー被弾時の点滅
    float hitBlinkElapsed = 0.0f;
    float hitBlinkInterval = 0.1f;
    float hitBlinkTotalTime = 1.5f;
    bool isHitBlinking = false;
    bool isRed = false;

    void BlinkInit()
    {
        isHitBlinking = true;
        hitBlinkElapsed = 0.0f;
    }

    void SetBlinkColor(bool isRed)
    {
        if (isRed)
        {
            color = { 1.0f,0.2f,0.2f };
        }
        else
        {
            color = { 1.0f,1.0f,1.0f };
        }
    }

    DirectX::XMFLOAT3 color = { 1.0f,1.0f,1.0f };

    // インタラクト対象検索
    IInteractable* FindInteractable() ;

public:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<ParticleComponent> sparkComponent; // 火花エフェクト用コンポーネント
    std::shared_ptr<InputComponent> inputComponent;
    std::shared_ptr<RotationComponent> rotationComponent;
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;
    std::shared_ptr<CapsuleComponent> swordCollisionComp;
    std::shared_ptr<SceneComponent> swordPointComp;

    float elapsedTime_ = 0.0f;

    bool isGrounded_ = false;

    struct TrailPoint
    {
        XMFLOAT3 position;
        float life;
    };
    std::vector<TrailPoint> trailPoints;

private:
    DirectX::XMFLOAT3 prevSwordTip; // 前フレームの剣先の位置
    bool isAttackActive = false;
    float hitStopTimer = 0.0f;// ヒットストップのタイマー
};
