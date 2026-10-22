#include "pch.h"
#include "GameTutorialController.h"

void GameTutorialController::Initialize(bool explanationAlreadySeen)
{
    explanationState = explanationAlreadySeen ? ExplanationState::Completed : ExplanationState::Unseen;
    ringActive = false;
    ringFinished = false;
    ringElapsed = 0.0f;
    ringStartedInAttackRange = false;
    ringPreviousAttackRange = false;
}

bool GameTutorialController::BeginExplanation(bool isPaused)
{
    if (explanationState != ExplanationState::Unseen || isPaused)
        return false;
    explanationState = ExplanationState::Showing;
    return true;
}

bool GameTutorialController::CloseExplanation(bool startedInAttackRange)
{
    if (explanationState != ExplanationState::Showing)
        return false;
    ringFinished = false;
    ringElapsed = 0.0f;
    ringStartedInAttackRange = startedInAttackRange;
    ringPreviousAttackRange = startedInAttackRange;
    ringActive = true;
    explanationState = ExplanationState::Completed;
    return true;
}

GameTutorialController::RingAction GameTutorialController::UpdateRing(
    bool bossRoomTransition, bool invalidSkeleton, bool inAttackRange, float unscaledDeltaTime)
{
    if (bossRoomTransition)
    {
        ringActive = false;
        ringFinished = true;
        return RingAction::Hide;
    }
    if (!ringActive || ringFinished)
        return RingAction::None;
    if (invalidSkeleton)
    {
        ringActive = false;
        ringFinished = true;
        return RingAction::Hide;
    }
    ringElapsed += unscaledDeltaTime;
    const bool enteredRange = !ringPreviousAttackRange && inAttackRange;
    const bool graceExpired = ringStartedInAttackRange && ringElapsed >= ringRangeGraceDuration;
    ringPreviousAttackRange = inAttackRange;
    if (enteredRange || graceExpired)
    {
        ringActive = false;
        ringFinished = true;
        return RingAction::Hide;
    }
    return RingAction::Update;
}

void GameTutorialController::ResetForSceneStart()
{
    completedForBossBattle = false;
}

bool GameTutorialController::CompleteForBossBattle()
{
    if (completedForBossBattle)
        return false;
    completedForBossBattle = true;
    return true;
}

void GameTutorialController::ResetGuideProgress()
{
    passiveLockOnCandidate = false;
    passiveYHpBaseline = -1;
    passiveYHpBaselineCaptured = false;
    dodgeYHpBaseline = -1;
    dodgeYHpBaselineCaptured = false;
    dodgeAttackRange = false;
    dodgeGuideActivated = false;
    dodgeSkeletonJustDodged = false;
}

GameTutorialController::GuideUpdate GameTutorialController::BeginGuideUpdate(
    bool hasPlayer, bool hasPassiveSkeleton)
{
    passiveLockOnCandidate = false;
    if (completedForBossBattle || !hasPlayer || !hasPassiveSkeleton)
    {
        dodgeAttackRange = false;
        return !completedForBossBattle && hasPlayer ? GuideUpdate::HideX : GuideUpdate::Skip;
    }
    return GuideUpdate::Proceed;
}

bool GameTutorialController::SetPassiveLockOnCandidate(bool candidate)
{
    passiveLockOnCandidate = candidate;
    return candidate;
}

bool GameTutorialController::ShouldRevealY(bool yVisible) const
{
    return !yVisible;
}

bool GameTutorialController::ShouldLearnLT(bool selected, bool selectedTutorialSkeleton, bool ltVisible) const
{
    return selected && selectedTutorialSkeleton && ltVisible;
}

bool GameTutorialController::ShouldCheckYDamage(bool visible, bool learned) const
{
    return visible && !learned;
}

void GameTutorialController::CaptureYBaseline(bool dodgeSkeleton, int hp)
{
    int& baseline = dodgeSkeleton ? dodgeYHpBaseline : passiveYHpBaseline;
    bool& captured = dodgeSkeleton ? dodgeYHpBaselineCaptured : passiveYHpBaselineCaptured;
    baseline = hp;
    captured = true;
}

bool GameTutorialController::DetectYDamage(bool dodgeSkeleton, int currentHp)
{
    int& baseline = dodgeSkeleton ? dodgeYHpBaseline : passiveYHpBaseline;
    bool& captured = dodgeSkeleton ? dodgeYHpBaselineCaptured : passiveYHpBaselineCaptured;
    if (!captured)
    {
        baseline = currentHp;
        captured = true;
        return false;
    }
    return currentHp < baseline;
}

bool GameTutorialController::SetDodgeAttackRange(bool inRange)
{
    dodgeAttackRange = inRange;
    if (inRange && !dodgeGuideActivated)
    {
        dodgeGuideActivated = true;
        return true;
    }
    return false;
}

void GameTutorialController::RecordJustDodge(bool justDodgeOnTarget)
{
    if (dodgeGuideActivated && !dodgeSkeletonJustDodged && justDodgeOnTarget)
        dodgeSkeletonJustDodged = true;
}

bool GameTutorialController::ShouldLearnX(bool visible, bool learned, bool dodging) const
{
    return dodgeGuideActivated && visible && !learned && dodging;
}
