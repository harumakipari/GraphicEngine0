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
    struct ComboAttack
    {
        std::string animationName;

        int nextComboIndex = -1;
    };

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

    void OnAnimationNotifyBegin(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEnd(const AnimationNotifyState& state)override;

    void OnAnimationNotifyEvent(const AnimationNotifyEvent& event)override;

    // アニメーションステート関連のフラグをリセットする
    void ResetAnimationStateFlag();

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

    // 攻撃開始時の処理
    void StartAttack();

    // ジャスト回避成功時の処理
    void StartJustDodgeSuccess();

    // ジャスト回避を受け付けるかどうか
    bool GetJustDodgeWindow()const { return  justDodgeWindow; }

private:
    // プレイヤーのマックスHP
    int maxHp = 100;

    // インタラクト対象検索
    IInteractable* FindInteractable();

public:
    // ヒット中に当たった敵を記録する
    std::unordered_set<Actor*> hitActors;

    //当たった相手を記録するためのセット 火花エフェクトの生成やダメージの適用を一度だけ行うために使用
    std::unordered_set<Actor*> hitTargets;
    bool hasPrevSwordTip = false; // 前フレームの剣先の位置が有効かどうか
    bool hasSpawnedThisAttack = false; // 今攻撃でエフェクトを生成したかどうか


    std::string currentAttackAnimation = "Primary_Attack_Fast_A";
    std::string startAttackAnimation = "Primary_Attack_Fast_A";    // コンボ開始のアニメーション

    bool comboQueued = false;   // コンボ攻撃がキューに入っているかどうか
    bool comboWindow = false;   // コンボ受付をするかどうか
    bool hitBox = false;   // 武器の当たり判定をつける
    bool transitionWindow = false;  // ステート遷移してもいいかどうか
    bool justDodgeWindow = false;  // ジャスト回避受付時間
    bool justDodgeSuccess = false; // ジャスト回避成功フラグ
    bool invincibleWindow = false; // 無敵状態かどうか

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

    friend class PlayerStateBase;
};
