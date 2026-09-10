#pragma once
#include "JudgementBase.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"

// TooCloseNode‚É‘JˆÚ‚Å‚«‚é‚©”»’è
class TooCloseJudgment : public JudgementBase
{
public:
	TooCloseJudgment(GruxEnemy* enemy) :JudgementBase(enemy) {};
	// ”»’è
	bool Judgment()override;
};




#include "GruxFastComboBT.h"
