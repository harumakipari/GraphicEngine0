#include "pch.h"
#include "GamePlayerDeathController.h"
#include "Game/Actors/Player/Player.h"
#include "Game/DarkGame/DarkActors/DarkEnemy/GruxEnemy.h"
#include "Game/Actors/UI/GameDefeatUIActor.h"
#include "Game/DarkGame/GameBgmDirector.h"
#include "Game/Actors/Camera/Camera.h"
#include "Game/Actors/Camera/DarkGameCamera.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Audio/Audio.h"

void GamePlayerDeathController::SetCueCallback(std::function<void(Cue)> callback)
{
    deathPresentationCueCallback = std::move(callback);
}

void GamePlayerDeathController::StartPresentation(float battleTime, GameDefeatUIActor& defeatUIActor)
{
    deathPresentationElapsed = 0.0f;
    deathAttemptTime = battleTime;
    defeatUIActor.SetBattleTime(deathAttemptTime);
    ResetDeathPresentationCues();
}

void GamePlayerDeathController::ResetDeathCameraStart()
{
    deathCameraStartRequested = false;
}

void GamePlayerDeathController::BeginHudFade(Player* player, GruxEnemy* boss)
{
    if (player)
        player->BeginGameplayHudFadeOut();
    if (boss)
        boss->BeginHpBarFadeOut();
    NotifyGameplayHudFade();
}

void GamePlayerDeathController::PrepareDeathActor(Player* player, DarkCameraActor* camera, std::function<void()> cameraStartCallback)
{
    if (camera && player)
    {
        player->SetDeathCameraTransparencyDisabled(true);
        player->SetDeathCameraStartCallback(std::move(cameraStartCallback));
        if (auto stateMachine = player->GetStateMachine())
            stateMachine->ChangeState("Death");
    }
    else if (player && player->GetStateMachine())
    {
        player->SetDeathCameraTransparencyDisabled(true);
        player->GetStateMachine()->ChangeState("Death");
    }
}

void GamePlayerDeathController::StartDeathCamera(DarkCameraActor* camera, const std::shared_ptr<GruxEnemy>& boss)
{
    if (deathCameraStartRequested)
        return;

    deathCameraStartRequested = true;
    if (camera)
        camera->StartDeathMode(boss, nullptr);
}
void GamePlayerDeathController::NotifyGameplayHudFade()
{
    FireDeathPresentationCue(Cue::GameplayHudFade, deathHudFadeCueFired);
}

void GamePlayerDeathController::ResetPresentationElapsed()
{
    deathPresentationElapsed = 0.0f;
}

bool GamePlayerDeathController::AcceptSelection(GameDefeatUIActor& defeatUIActor, float confirmVolume)
{
    if (!defeatUIActor.IsInputEnabled()) return false;
    CoreAudio::PlayOneShot("./Data/Sound/SE/button_push.wav", confirmVolume);
    defeatUIActor.ResetPresentation();
    return true;
}
void GamePlayerDeathController::UpdateFrameData(GameDefeatUIActor& defeatUIActor, float numberTexWidth, float numberTexHeight, float moveVolume) const
{
    defeatUIActor.SetFrameData(deathPresentationElapsed, deathOverlayStartTime, deathDefeatedStartTime,
        deathBattleTimeStartTime, deathButtonsStartTime, deathSelectLineStartTime,
        numberTexWidth, numberTexHeight, moveVolume);
}

bool GamePlayerDeathController::Update(float deltaTime, bool allowInput, Player* player, GruxEnemy* boss,
    GameDefeatUIActor& defeatUIActor, GameBgmDirector& bgmDirector, CinemaCamera* camera,
    float numberTexWidth, float numberTexHeight, float moveVolume)
{
    deathPresentationElapsed += deltaTime;
    UpdateDeathResultPresentation(player, boss, &defeatUIActor, bgmDirector, camera, numberTexWidth, numberTexHeight, moveVolume);
    if (allowInput && !defeatUIActor.IsInputEnabled() && deathPresentationElapsed >= deathResultInputEnableTime)
    {
        if (camera)
            if (auto* component = dynamic_cast<CinematicCameraComponent*>(camera->GetCameraComponent()))
                component->SetDebugInputEnabled(false);
        defeatUIActor.EnableInput();
        InputSystem::SetInputEnabled(true);
        return true;
    }
    return false;
}
void GamePlayerDeathController::ResetDeathPresentationCues()
{
    deathHudFadeCueFired = deathOverlayCueFired = deathDefeatedCueFired = false;
    deathBattleTimeCueFired = deathButtonsCueFired = false;
}

void GamePlayerDeathController::FireDeathPresentationCue(const Cue cue, bool& fired)
{
    if (fired) return;
    fired = true;
    if (deathPresentationCueCallback) deathPresentationCueCallback(cue);
}

void GamePlayerDeathController::UpdateDeathResultPresentation(Player* player, GruxEnemy* gruxEnemyActor, GameDefeatUIActor* defeatUIActor, GameBgmDirector& bgmDirector, CinemaCamera* cinemaCameraActor, float numberTexWidth, float numberTexHeight, float deathResultMoveSeVolume)
{
    const auto fade = [this](const float start, const float duration)
        {
            return std::clamp((deathPresentationElapsed - start) / (std::max)(0.001f, duration), 0.0f, 1.0f);
        };
    const float hudAlpha = 1.0f - fade(0.0f, deathHudFadeDuration);
    if (player) player->SetGameplayHudFadeAlpha(hudAlpha);
    if (gruxEnemyActor) gruxEnemyActor->SetHpBarFadeAlpha(hudAlpha);

    defeatUIActor->SetFrameData(deathPresentationElapsed, deathOverlayStartTime, deathDefeatedStartTime,
        deathBattleTimeStartTime, deathButtonsStartTime, deathSelectLineStartTime,
        numberTexWidth, numberTexHeight, deathResultMoveSeVolume);
    if (!defeatUIActor->IsVisible())
    {
        bgmDirector.PlayPlayerDeathBgm();
        if (cinemaCameraActor)
            if (auto* camera = dynamic_cast<CinematicCameraComponent*>(cinemaCameraActor->GetCameraComponent()))
                camera->SetDebugInputEnabled(false);
        defeatUIActor->SetVisible(true);
    }
    defeatUIActor->UpdatePresentation();

    if (deathPresentationElapsed >= deathOverlayStartTime) FireDeathPresentationCue(Cue::OverlayFade, deathOverlayCueFired);
    if (deathPresentationElapsed >= deathDefeatedStartTime) FireDeathPresentationCue(Cue::DefeatedFade, deathDefeatedCueFired);
    if (deathPresentationElapsed >= deathBattleTimeStartTime) FireDeathPresentationCue(Cue::BattleTimeFade, deathBattleTimeCueFired);
    if (deathPresentationElapsed >= deathButtonsStartTime) FireDeathPresentationCue(Cue::ButtonsFade, deathButtonsCueFired);
}

GamePlayerDeathController::DeathStagingArea GamePlayerDeathController::DetermineDeathStagingArea(
    const DirectX::XMFLOAT3& originalPlayerPosition) const
{
    const bool front = originalPlayerPosition.x < deathStagingMinPlayerX;
    const bool back = originalPlayerPosition.x > deathStagingMaxPlayerX;
    const bool right = originalPlayerPosition.z < deathStagingMinPlayerZ;
    const bool left = originalPlayerPosition.z > deathStagingMaxPlayerZ;

    if (front && left) return DeathStagingArea::FrontLeft;
    if (front && right) return DeathStagingArea::FrontRight;
    if (back && left) return DeathStagingArea::BackLeft;
    if (back && right) return DeathStagingArea::BackRight;
    if (front) return DeathStagingArea::Front;
    if (back) return DeathStagingArea::Back;
    if (left) return DeathStagingArea::Left;
    if (right) return DeathStagingArea::Right;
    return DeathStagingArea::Center;
}

void GamePlayerDeathController::StageDeathActors(Player* player, GruxEnemy* gruxEnemyActor)
{
    if (!player || !gruxEnemyActor)
        return;

    const DirectX::XMFLOAT3 originalPlayerPosition = player->GetPosition();
    const DeathStagingArea stagingArea = DetermineDeathStagingArea(originalPlayerPosition);
    const float targetBossDistance = deathStagingAreaSettings[static_cast<size_t>(stagingArea)].bossDistance;

    const float stagingMinX = (std::min)(deathStagingMinPlayerX, deathStagingMaxPlayerX);
    const float stagingMaxX = (std::max)(deathStagingMinPlayerX, deathStagingMaxPlayerX);
    const float stagingMinZ = (std::min)(deathStagingMinPlayerZ, deathStagingMaxPlayerZ);
    const float stagingMaxZ = (std::max)(deathStagingMinPlayerZ, deathStagingMaxPlayerZ);
    const float wallInsetX = std::clamp(deathStagingWallInsetX, 0.0f, (stagingMaxX - stagingMinX) * 0.5f);
    const float wallInsetZ = std::clamp(deathStagingWallInsetZ, 0.0f, (stagingMaxZ - stagingMinZ) * 0.5f);
    const float safeMinX = stagingMinX + wallInsetX;
    const float safeMaxX = stagingMaxX - wallInsetX;
    const float safeMinZ = stagingMinZ + wallInsetZ;
    const float safeMaxZ = stagingMaxZ - wallInsetZ;

    DirectX::XMFLOAT3 safePlayerPosition = originalPlayerPosition;
    safePlayerPosition.x = std::clamp(safePlayerPosition.x, safeMinX, safeMaxX);
    safePlayerPosition.z = std::clamp(safePlayerPosition.z, safeMinZ, safeMaxZ);

    const DirectX::XMFLOAT3 stagingOffset{
        safePlayerPosition.x - originalPlayerPosition.x,
        0.0f,
        safePlayerPosition.z - originalPlayerPosition.z };
    const DirectX::XMFLOAT3 originalBossPosition = gruxEnemyActor->GetPosition();
    DirectX::XMFLOAT3 stagedBossPosition{
        originalBossPosition.x + stagingOffset.x,
        originalBossPosition.y,
        originalBossPosition.z + stagingOffset.z };

    if (std::abs(stagingOffset.x) > FLT_EPSILON ||
        std::abs(stagingOffset.z) > FLT_EPSILON)
    {
        player->SetPosition(safePlayerPosition);
    }

    DirectX::XMFLOAT3 playerToBoss = MathHelper::Subtract(
        stagedBossPosition, safePlayerPosition);
    playerToBoss.y = 0.0f;
    DirectX::XMFLOAT3 direction = playerToBoss;
    if (MathHelper::Length(direction) > FLT_EPSILON)
    {
        direction = MathHelper::Normalize(direction);
    }
    else
    {
        direction = player->GetForward();
        direction.y = 0.0f;
        if (MathHelper::Length(direction) <= FLT_EPSILON)
            direction = { 0.0f, 0.0f, 1.0f };
        else
            direction = MathHelper::Normalize(direction);
    }

    stagedBossPosition.x = safePlayerPosition.x + direction.x * targetBossDistance;
    stagedBossPosition.z = safePlayerPosition.z + direction.z * targetBossDistance;
    if (std::abs(stagedBossPosition.x - originalBossPosition.x) > FLT_EPSILON ||
        std::abs(stagedBossPosition.z - originalBossPosition.z) > FLT_EPSILON)
    {
        gruxEnemyActor->SetPosition(stagedBossPosition);
    }

    DirectX::XMFLOAT3 bossToPlayer = MathHelper::Subtract(
        safePlayerPosition, stagedBossPosition);
    bossToPlayer.y = 0.0f;
    if (MathHelper::Length(bossToPlayer) <= FLT_EPSILON)
    {
        bossToPlayer = player->GetForward();
        bossToPlayer.x *= -1.0f;
        bossToPlayer.y = 0.0f;
        bossToPlayer.z *= -1.0f;
    }
    gruxEnemyActor->SetDirectionImmediate(bossToPlayer);
}

#ifdef USE_IMGUI
void GamePlayerDeathController::DrawDebugGui(float& moveVolume, float& confirmVolume,
    float& texWidth, float& texHeight, float battleElapsedTime)
{
    DrawPresentationDebugGui();
    ImGui::SliderFloat("Result Move SE Volume", &moveVolume, 0.0f, 2.0f);
    ImGui::SliderFloat("Result Confirm SE Volume", &confirmVolume, 0.0f, 2.0f);
    ImGui::DragFloat("numberTexWidth", &texWidth, 1.0f, 1.0f, 200.0f);
    ImGui::DragFloat("numberTexHeight", &texHeight, 1.0f, 1.0f, 300.0f);

    if (ImGui::TreeNode("Death Staging"))
    {
        DrawStagingDebugGui();
        ImGui::TreePop();
        DrawAttemptTimeDebugGui();
        ImGui::Text("battleElapsedTime : %.3f", battleElapsedTime);
    }
}

void GamePlayerDeathController::DrawPresentationDebugGui()
{
    ImGui::DragFloat(U8("HUDフェードアウト時間"), &deathHudFadeDuration, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat(U8("背景暗転 開始時間"), &deathOverlayStartTime, 0.01f, 0.0f, 30.0f);

    ImGui::DragFloat(U8("DEFEATED 表示開始時間"), &deathDefeatedStartTime, 0.01f, 0.0f, 30.0f);

    ImGui::DragFloat(U8("Battle Time 表示開始時間"), &deathBattleTimeStartTime, 0.01f, 0.0f, 30.0f);

    ImGui::DragFloat(U8("ボタン 表示開始時間"), &deathButtonsStartTime, 0.01f, 0.0f, 30.0f);

    ImGui::DragFloat(U8("選択ライン 表示開始時間"), &deathSelectLineStartTime, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat(U8("操作受付 開始時間"), &deathResultInputEnableTime, 0.01f, 0.0f, 30.0f);
}

void GamePlayerDeathController::DrawAttemptTimeDebugGui() const
{
    ImGui::Text("deathAttemptTime : %.3f", deathAttemptTime);
}

void GamePlayerDeathController::DrawStagingDebugGui()
{
    ImGui::DragFloat("Min Player X", &deathStagingMinPlayerX, 0.01f);
    ImGui::DragFloat("Max Player X", &deathStagingMaxPlayerX, 0.01f);
    ImGui::DragFloat("Min Player Z", &deathStagingMinPlayerZ, 0.01f);
    ImGui::DragFloat("Max Player Z", &deathStagingMaxPlayerZ, 0.01f);
    ImGui::DragFloat("Wall Inset X", &deathStagingWallInsetX, 0.01f, 0.0f, 3.0f);
    ImGui::DragFloat("Wall Inset Z", &deathStagingWallInsetZ, 0.01f, 0.0f, 3.0f);
    static constexpr const char* areaNames[] =
    { "Center", "Front", "Back", "Left", "Right", "FrontLeft", "FrontRight", "BackLeft", "BackRight" };
    for (size_t i = 0; i < deathStagingAreaSettings.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        ImGui::DragFloat(areaNames[i], &deathStagingAreaSettings[i].bossDistance, 0.01f, 0.0f, 20.0f);
        ImGui::PopID();
    }
}
#endif
