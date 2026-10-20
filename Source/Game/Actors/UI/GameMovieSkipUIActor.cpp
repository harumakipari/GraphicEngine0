#include "pch.h"
#include "GameMovieSkipUIActor.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Utility/Time.h"
#include "UI/UIManager.h"

void GameMovieSkipUIActor::Initialize(const Transform& transform)
{
    movieSkipPromptImage = std::make_shared<UIImageComponent>("./Data/Textures/UI/skip_B.png", "MovieSkipPrompt");
    movieSkipPromptImage->SetSize({ 624.0f, 225.0f });
    movieSkipPromptImage->SetPivot({ 1.0f, 1.0f });
    movieSkipPromptImage->SetScale({ movieSkipPromptScale, movieSkipPromptScale });
    movieSkipPromptImage->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, 0.0f });
    movieSkipPromptImage->SetVisible(false);
    movieSkipPromptImage->SetEnable(false);
    movieSkipPromptImage->zOrder = 1100;
    GetOwnerScene()->GetUIManager()->Add(movieSkipPromptImage);
}

void GameMovieSkipUIActor::SetActiveMovieType(const MovieSkipType type)
{
    if (type != movieSkipType)
    {
        ResetPresentation();
        movieSkipType = type;
        waitingForButtonsReleased = type != MovieSkipType::None && InputSystem::IsAnyGamepadButtonPressedRaw();
    }
}

void GameMovieSkipUIActor::ResetPresentation()
{
    movieSkipType = MovieSkipType::None;
    movieSkipPromptVisible = false;
    movieSkipConfirmed = false;
    skipRequest = false;
    waitingForButtonsReleased = false;
    movieSkipPromptTimer = 0.0f;
    movieSkipPromptFade = 0.0f;
    if (movieSkipPromptImage)
    {
        movieSkipPromptImage->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, 0.0f });
        movieSkipPromptImage->SetVisible(false);
    }
}

bool GameMovieSkipUIActor::ConsumeSkipRequest()
{
    const bool requested = skipRequest;
    skipRequest = false;
    return requested;
}

void GameMovieSkipUIActor::UpdatePresentation()
{
    if (movieSkipType == MovieSkipType::None || !movieSkipPromptImage)
        return;

    if (waitingForButtonsReleased)
    {
        if (!InputSystem::IsAnyGamepadButtonPressedRaw())
            waitingForButtonsReleased = false;
        return;
    }

    const float deltaTime = Time::UnscaledDeltaTime();
    const bool anyButtonTriggered = InputSystem::IsAnyGamepadButtonTriggeredRaw();
    const bool bTriggered = InputSystem::IsGamepadButtonTriggeredRaw(XINPUT_GAMEPAD_B);

    if (!movieSkipConfirmed)
    {
        if (!movieSkipPromptVisible)
        {
            if (anyButtonTriggered)
            {
                movieSkipPromptVisible = true;
                movieSkipPromptTimer = 0.0f;
            }
        }
        else if (bTriggered)
        {
            movieSkipConfirmed = true;
            movieSkipPromptVisible = false;
            movieSkipPromptTimer = 0.0f;
            skipRequest = true;
            return;
        }
        else if (anyButtonTriggered)
        {
            movieSkipPromptTimer = 0.0f;
        }
        else
        {
            movieSkipPromptTimer += deltaTime;
            if (movieSkipPromptTimer >= movieSkipPromptTimeout)
                movieSkipPromptVisible = false;
        }
    }

    const float fadeTarget = movieSkipPromptVisible ? 1.0f : 0.0f;
    if (movieSkipPromptFadeDuration <= FLT_EPSILON)
    {
        movieSkipPromptFade = fadeTarget;
    }
    else
    {
        const float fadeStep = deltaTime / movieSkipPromptFadeDuration;
        movieSkipPromptFade = fadeTarget > movieSkipPromptFade ? (std::min)(movieSkipPromptFade + fadeStep, fadeTarget) : (std::max)(movieSkipPromptFade - fadeStep, fadeTarget);
    }

    const float screenWidth = Graphics::GetScreenWidth();
    const float screenHeight = Graphics::GetScreenHeight();
    movieSkipPromptImage->SetWorldPosition({ screenWidth - movieSkipPromptRightBottomOffset.x,screenHeight - movieSkipPromptRightBottomOffset.y });
    movieSkipPromptImage->SetScale({ movieSkipPromptScale, movieSkipPromptScale });
    movieSkipPromptImage->SetColor(CoreColor{ 1.0f, 1.0f, 1.0f, movieSkipPromptFade });
    movieSkipPromptImage->SetVisible(movieSkipPromptFade > FLT_EPSILON);
}

void GameMovieSkipUIActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::DragFloat2("Movie Skip Right Bottom Offset", &movieSkipPromptRightBottomOffset.x, 1.0f, 0.0f, 1000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Movie Skip Scale", &movieSkipPromptScale, 0.01f, 0.10f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
#endif
}
