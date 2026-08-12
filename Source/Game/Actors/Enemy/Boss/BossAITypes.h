#pragma once

#include <cstdint>
#include <string>
#include <DirectXMath.h>

enum class PlayerRelativeRegion : uint8_t
{
    Front,
    Side,
    Back,
};

struct BossTargetContext
{
    bool valid = false;
    float xzDistance = 0.0f;
    DirectX::XMFLOAT3 directionToPlayer{};
    float forwardDot = 1.0f;
    float signedAngleDegrees = 0.0f;
    float absoluteAngleDegrees = 0.0f;
    PlayerRelativeRegion region = PlayerRelativeRegion::Front;
};

enum class BossAIMode : uint8_t
{
    CombatAI,
    DebugFixedAttack,
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

struct BossAttackData
{
    BossAttackType type = BossAttackType::PrimaryAttackLA;
    std::string animationName;
    float minDistance = 0.0f;
    float maxDistance = 0.0f;
    float weight = 1.0f;
};
