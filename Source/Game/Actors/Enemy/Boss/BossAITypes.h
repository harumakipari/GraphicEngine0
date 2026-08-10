#pragma once

#include <cstdint>
#include <string>

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
