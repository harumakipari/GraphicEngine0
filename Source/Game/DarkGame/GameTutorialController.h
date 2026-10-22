#pragma once

class GameTutorialController
{
public:
    enum class ExplanationState { Unseen, Showing, Completed };
    enum class RingAction { None, Hide, Update };

    void Initialize(bool explanationAlreadySeen);
    ExplanationState GetExplanationState() const { return explanationState; }
    bool BeginExplanation(bool isPaused);
    bool CloseExplanation(bool startedInAttackRange);
    RingAction UpdateRing(bool bossRoomTransition, bool invalidSkeleton,
        bool inAttackRange, float unscaledDeltaTime);
    void ResetForSceneStart();
    bool IsCompletedForBossBattle() const { return completedForBossBattle; }
    bool CompleteForBossBattle();

    enum class GuideUpdate { Skip, HideX, Proceed };
    void ResetGuideProgress();
    GuideUpdate BeginGuideUpdate(bool hasPlayer, bool hasPassiveSkeleton);
    bool SetPassiveLockOnCandidate(bool candidate);
    bool ShouldRevealY(bool yVisible) const;
    bool ShouldLearnLT(bool selected, bool selectedTutorialSkeleton, bool ltVisible) const;
    bool ShouldCheckYDamage(bool visible, bool learned) const;
    void CaptureYBaseline(bool dodgeSkeleton, int hp);
    bool DetectYDamage(bool dodgeSkeleton, int currentHp);
    bool SetDodgeAttackRange(bool inRange);
    void RecordJustDodge(bool justDodgeOnTarget);
    bool ShouldLearnX(bool visible, bool learned, bool dodging) const;
    bool IsPassiveLockOnCandidate() const { return passiveLockOnCandidate; }
    bool IsDodgeAttackRange() const { return dodgeAttackRange; }
    bool IsDodgeGuideActivated() const { return dodgeGuideActivated; }
    bool HasDodgeSkeletonJustDodged() const { return dodgeSkeletonJustDodged; }

private:
    bool completedForBossBattle = false;
    ExplanationState explanationState = ExplanationState::Unseen;
    bool ringActive = false;
    bool ringFinished = false;
    float ringElapsed = 0.0f;
    float ringRangeGraceDuration = 0.25f;
    bool ringStartedInAttackRange = false;
    bool ringPreviousAttackRange = false;
    bool passiveLockOnCandidate = false;
    int passiveYHpBaseline = -1;
    bool passiveYHpBaselineCaptured = false;
    int dodgeYHpBaseline = -1;
    bool dodgeYHpBaselineCaptured = false;
    bool dodgeAttackRange = false;
    bool dodgeGuideActivated = false;
    bool dodgeSkeletonJustDodged = false;
};
