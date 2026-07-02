#pragma once
#include <stack>
#include <memory>
#include "Game/Actors/Base/Character.h"
#include "Engine/Input/GamePad.h"

#include "Components/Controller/ControllerComponent.h"
#include "Components/Render/MeshComponent.h"

#include "Core/ActorManager.h"
#include "Components/Effect/ParticleComponent.h"

class AudioSourceComponent;
class IInteractable;
class Enemy;

class Player :public Character
{
public:
    struct ComboAttack
    {
        std::string animationName;

        int nextComboIndex = -1;
    };

    enum class InputCommand :uint8_t
    {
        None,
        Attack,
        Dodge,
        Jump,
        Interact,
    };

    struct BufferInput
    {
        InputCommand command = InputCommand::None;
        float remainTime = 0.0f;
    };

public:
    explicit Player(const std::string& modelName) :Character(modelName)
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

private:
    // 火花エフェクトの生成
    void SpawnSpark(DirectX::XMFLOAT3 hitPosition);

    // 剣の攻撃判定
    void CheckSwordLineHit(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end);

    // 入力処理をまとめる
    void HandleInput(float deltaTimes);
public:
    // 入力コマンドによってステートが変わるかどうか
    bool TryHandleGlobalTransition();

    // 入力を消費する処理
    void ConsumeBufferCommand();

    //当たった時の処理
    void TakeDamage(int damage);

    // 攻撃ヒット時の処理
    void DoAttackHit();

    // 攻撃開始時の処理
    void StartAttack();

    // ジャスト回避成功時の処理
    void StartJustDodgeSuccess(const std::shared_ptr<Enemy>& enemy);

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
    bool inputWindow = false;   // コンボ受付をするかどうか
    bool hitBox = false;   // 武器の当たり判定をつける
    bool transitionWindow = false;  // ステート遷移してもいいかどうか
    bool justDodgeWindow = false;  // ジャスト回避受付時間
    bool justDodgeSuccess = false; // ジャスト回避成功フラグ
    float rushInputTimer = 0.0f; // ジャスト回避時にラッシュを受け付ける時間(この時間の間スローになる)

    bool invincibleWindow = false; // アニメーションによる無敵状態かどうか
    bool invincible = false; // 無敵状態かどうか

    float dodgeSpeed = 3.0f; // 回避する時のスピード
    float dodgeDuration = 0.5f; // 回避するときの時間

    std::weak_ptr<Enemy> rushTarget; // ターゲットを選択

    // 入力受付のコマンド
    BufferInput bufferCommand = { InputCommand::None,0.0f }; // 入力コマンド
    DirectX::XMFLOAT3 dodgeDirection = { 0.0f,0.0f,0.0f };// 避ける方向
public:
    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;
    std::shared_ptr<ParticleComponent> sparkComponent; // 火花エフェクト用コンポーネント
    std::shared_ptr<InputComponent> inputComponent;
    std::shared_ptr<RotationComponent> rotationComponent;
    std::shared_ptr<CharacterMovementComponent> characterMovementComponent;
    std::shared_ptr<CapsuleComponent> swordCollisionComp;

    std::shared_ptr<SceneComponent> swordPointComp;
    std::shared_ptr<AudioSourceComponent> runAudioComp;    // 走りのSE

    std::shared_ptr<SceneComponent> swordRootComponent; // 剣の根元のコンポーネント
    std::shared_ptr<SceneComponent> swordMiddleComponent; // 剣の中間のコンポーネント
    std::shared_ptr<SceneComponent> swordTipComponent;  // 剣の先端のコンポーネント

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

    DirectX::XMFLOAT3 prevSwordRootPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevSwordMidPos = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT3 prevSwordTipPos = { 0.0f,0.0f,0.0f };



    friend class PlayerStateBase;
};
