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
    struct AttackData
    {
        std::string animationName;

        float hitStart;
        float hitEnd;

        float comboWindowStart;
        float comboWindowEnd;

        int nextComboIndex = -1;

        float moveSpeed = 0.0f;
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
private:
    // 火花エフェクトの生成
    void SpawnSpark(DirectX::XMFLOAT3 hitPosition);

    // 剣の攻撃判定
    void CheckSwordLineHit(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end);

    // アタックエディタ
    void DrawAttackEditorImGui();

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

private:
    // プレイヤーのマックスHP
    int maxHp = 100;


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

    std::vector<AttackData> comboAttacks; // コンボ攻撃のデータ
    int currentComboIndex = 0; // 現在のコンボ攻撃のインデックス
    bool comboQueued = false;   // コンボ攻撃がキューに入っているかどうか

private:
    DirectX::XMFLOAT3 prevSwordTip; // 前フレームの剣先の位置
    bool isAttackActive = false;
    float hitStopTimer = 0.0f;// ヒットストップのタイマー

};
