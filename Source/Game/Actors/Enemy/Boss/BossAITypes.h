#pragma once

#include <cstdint>
#include <string>
#include <DirectXMath.h>
#include <optional>

enum class PlayerRelativeRegion : uint8_t
{
    Front,
    Side,
    Back,
};

enum class BossDistanceRegion : uint8_t
{
    Near,
    Middle,
    Far,
};

enum class BossPositioningDirection : uint8_t
{
    TowardPlayer,
    AwayFromPlayer,
};

enum class BossPositioningCompletionType : uint8_t
{
    TravelDistance,
    TargetDistance,
};

struct BossTargetContext
{
    bool valid = false;
    float xzDistance = 0.0f;
    BossDistanceRegion distanceRegion = BossDistanceRegion::Near;
    DirectX::XMFLOAT3 directionToPlayer{};
    float forwardDot = 1.0f;
    float signedAngleDegrees = 0.0f;    // 符号付き角度、正の値は右側、負の値は左側 左右を知りたい場合に使用
    float absoluteAngleDegrees = 0.0f;  // 符号なし角度、0度は正面、180度は背面、90度は側面　正面からのずれを知りたい場合に使用
    PlayerRelativeRegion region = PlayerRelativeRegion::Front;
};

enum class BossAIMode : uint8_t
{
    CombatAI,
    DebugFixedAttack,
};

enum class BossIntentType : uint8_t
{
    CloseCombat,
    DashAttackPlan,
};

enum class BossIntentStep : uint8_t
{
    Selecting,
    Positioning,
    AttackPending,
    Completed,
    Failed,
};

enum class BossIntentRangeStatus : uint8_t
{
    TooClose,
    InRange,
    TooFar,
};

struct BossIntentData
{
    BossIntentType type = BossIntentType::CloseCombat;
    float nearWeight = 1.0f;
    float middleWeight = 1.0f;
    float farWeight = 1.0f;
    float preferredMinDistance = 0.0f;
    float preferredMaxDistance = 1.0f;
};

enum class BossActionType :uint8_t
{
    AttackLA,
    AttackRA,
    FastCombo,
    JumpAttack,
    DashAttack,
    Approach,
    Retreat,
};

enum class BossAttackType : uint8_t
{
    PrimaryAttackLA,
    PrimaryAttackRA,
    FastCombo,
    JumpAttack,
    Dash,
    DashAttack,
    LongRangeAttack,
};


enum class BossActionCandidateReason : uint8_t
{
    NoActiveIntent,
    Candidate,
    NotForCurrentIntent,
    WrongDistance,
    Cooldown,
    ZeroWeight,
};

struct BossActionData
{
    BossActionType type = BossActionType::AttackLA;
    std::optional<BossAttackType> attackType = BossAttackType::PrimaryAttackLA;

    // 各アクションがどのDistanceRegionで候補になるかを指定する。
    BossDistanceRegion minDistanceRegion = BossDistanceRegion::Near;
    BossDistanceRegion maxDistanceRegion = BossDistanceRegion::Far;

    float weight = 1.0f;
    float cooldownDuration = 0.0f;
};

struct BossPositioningData
{
    BossActionType actionType = BossActionType::Approach;
    BossPositioningDirection direction = BossPositioningDirection::TowardPlayer;
    float maxMoveDistance = 3.0f;
    float moveSpeed = 5.0f;
    float timeout = 3.0f;
    float stuckTimeThreshold = 0.5f;
    float stuckMovementThreshold = 0.1f;
    BossPositioningCompletionType completionType = BossPositioningCompletionType::TravelDistance;
    float targetDistance = 0.0f;
};


struct BossAttackData
{
    BossAttackType type = BossAttackType::PrimaryAttackLA;
    std::string animationName;
    float minDistance = 0.0f;
    float maxDistance = 0.0f;
    float weight = 1.0f;
    float recoveryDuration = 0.5f;
    int damagePerHit = 1;
};
