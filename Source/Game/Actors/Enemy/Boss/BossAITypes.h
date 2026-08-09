#pragma once

#include <cstdint>

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
