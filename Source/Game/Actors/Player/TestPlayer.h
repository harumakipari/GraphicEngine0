#pragma once
#include "Game/Actors/Base/Character.h"


class TestPlayer :public Character
{
public:
    explicit TestPlayer(const std::string& modelName) :Character(modelName)
    {
        mass = 50.0f;
    }
    void Initialize(const Transform& transform)override;

    void Update(float elapsedTime)override;

    // 遅延更新処理
    void LateUpdate(float elapsedTime)override{}

	void DrawImGuiDetails()override;

    void Finalize()override {}

    // 描画用コンポーネントを追加
    std::shared_ptr<SkeletalMeshComponent> skeletalMeshComponent;


	enum {
		EVADE_B,
		EVADE_F,
		EVADE_L,
		EVADE_R,
		IDLE,
		WALK_B,
		WALK_F,
		WALK_L,
		WALK_R,
		SLASH,
	};
	bool enable_root_motion = true;
	bool ignore_root_motion = false;
	int clip_index = IDLE;

};
