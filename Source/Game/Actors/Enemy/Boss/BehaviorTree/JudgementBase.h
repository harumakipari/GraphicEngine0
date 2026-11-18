#pragma once

class GruxEnemy;

// ノードが実行可能か判定する抽象クラス
class JudgementBase
{
public:
	JudgementBase(GruxEnemy* enemy) :owner(enemy) {}
	virtual ~JudgementBase() = default;
	virtual bool Judgment() = 0;
protected:
	GruxEnemy* owner;
};
