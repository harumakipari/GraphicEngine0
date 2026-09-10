#pragma once
#include "ActionBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

class StartJumpAttack : public ActionBase
{
public:
	StartJumpAttack(GruxEnemy* enemy) :ActionBase(enemy) {}
	ActionBase::State Run(float elapsedTime);
};

class ExecuteJumpAttack : public ActionBase
{
public:
	ExecuteJumpAttack(GruxEnemy* enemy) :ActionBase(enemy) {}
	ActionBase::State Run(float elapsedTime);
};

