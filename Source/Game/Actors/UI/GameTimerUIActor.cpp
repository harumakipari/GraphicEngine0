#include "pch.h"
#include "GameTimerUIActor.h"
#include "Engine/Scene/Scene.h"
#include "UI/UIManager.h"

void GameTimerUIActor::Initialize(const Transform& transform)
{
    auto uiManager = GetOwnerScene()->GetUIManager();

    // 砂時計の
    battleTimerHourglassFrame = std::make_shared<UIImageComponent>("./Data/Textures/UI/BattleTimerHourglassFrame.png", "BattleTimerHourglassFrame");
    battleTimerHourglassSand = std::make_shared<UIImageComponent>("./Data/Textures/UI/BattleTimerHourglassSandTop.png", "BattleTimerHourglassSand");

    for (const auto& hourglass : { battleTimerHourglassFrame, battleTimerHourglassSand })
    {
        hourglass->SetPivot({ 0.5f, 0.5f });
        hourglass->SetWorldAngleDegree(0.0f);
        hourglass->zOrder = 16;
        uiManager->Add(hourglass);
    }

    for (int i = 0; i < static_cast<int>(battleTimerDigits.size()); ++i)
    {
        const bool colon = i == 2;
        const std::string path = colon ? "./Data/Textures/UI/timer_colon.png" : "./Data/Textures/UI/number.png";
        battleTimerDigits[i] = std::make_shared<UIImageComponent>(path, "BattleTimerDigit" + std::to_string(i));
        battleTimerDigits[i]->SetSize(colon ? DirectX::XMFLOAT2{ 48.0f, 128.0f } : DirectX::XMFLOAT2{ 96.0f, 128.0f });
        battleTimerDigits[i]->SetPivot({ 0.5f, 0.5f });
        battleTimerDigits[i]->zOrder = 16;
        uiManager->Add(battleTimerDigits[i]);
    }

    SetVisible(false);
    SetElapsedTime(0.0f, 198.0f, 300.0f);
}

void GameTimerUIActor::SetVisible(const bool visible)
{
    battleTimerVisible = visible;
    if (battleTimerHourglassFrame) battleTimerHourglassFrame->SetVisible(visible);
    if (battleTimerHourglassSand) battleTimerHourglassSand->SetVisible(visible);
    for (auto& digit : battleTimerDigits)
        if (digit) digit->SetVisible(visible);
}

void GameTimerUIActor::SetElapsedTime(const float elapsedTime, const float numberTexWidth, const float numberTexHeight) const
{
    const int totalSeconds = (std::min)(static_cast<int>(std::floor(elapsedTime)), 99 * 60 + 59);
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    const std::array<int, 5> values =
    { minutes / 10, minutes % 10, -1, seconds / 10, seconds % 10 };

    const DirectX::XMFLOAT2 hourglassPosition
    {
        battleTimerUiPosition.x + battleTimerHourglassOffset.x,
        battleTimerUiPosition.y + battleTimerHourglassOffset.y
    };

    const float hourglassWidth = battleTimerHourglassSize.x * battleTimerUiScale.x;
    const float sandWidthRatio = 524.0f / 556.0f;
    const float sandHeightRatio = 278.0f / 594.0f;

    if (battleTimerHourglassFrame)
    {
        battleTimerHourglassFrame->SetWorldPosition(hourglassPosition);
        battleTimerHourglassFrame->SetSize(battleTimerHourglassSize);
        battleTimerHourglassFrame->SetScale(battleTimerUiScale);
        battleTimerHourglassFrame->SetWorldAngleDegree(battleTimerHourglassAngle);
    }
    if (battleTimerHourglassSand)
    {
        const DirectX::XMFLOAT2 sandSize
        { battleTimerHourglassSize.x * sandWidthRatio, battleTimerHourglassSize.y * sandHeightRatio };
        const float angle = DirectX::XMConvertToRadians(battleTimerHourglassAngle);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const DirectX::XMFLOAT2 rotatedSandOffset
        {
            battleTimerHourglassSandOffset.x * cosine - battleTimerHourglassSandOffset.y * sine,
            battleTimerHourglassSandOffset.x * sine + battleTimerHourglassSandOffset.y * cosine
        };
        battleTimerHourglassSand->SetWorldPosition(
            { hourglassPosition.x + rotatedSandOffset.x, hourglassPosition.y + rotatedSandOffset.y });
        battleTimerHourglassSand->SetSize(sandSize);
        battleTimerHourglassSand->SetScale(battleTimerUiScale);
        battleTimerHourglassSand->SetWorldAngleDegree(battleTimerHourglassAngle);
    }

    const float firstDigitX = battleTimerUiPosition.x + hourglassWidth * 0.5f + battleTimerHourglassNumberSpacing * battleTimerUiScale.x;

    for (int i = 0; i < static_cast<int>(battleTimerDigits.size()); ++i)
    {
        auto& digit = battleTimerDigits[i];
        if (!digit)
            continue;
        digit->SetVisible(battleTimerVisible);
        DirectX::XMFLOAT2 position = { firstDigitX + i * battleTimerNumberSpacing * battleTimerUiScale.x,battleTimerUiPosition.y };
        DirectX::XMFLOAT2 scale = battleTimerUiScale;
        if (values[i] == -1)
        {
            position.x += battleTimerColonOffset.x;
            position.y += battleTimerColonOffset.y;
            scale.x *= battleTimerColonScale;
            scale.y *= battleTimerColonScale;
        }
        digit->SetWorldPosition(position);
        digit->SetScale(scale);
        if (values[i] >= 0)
            digit->SetUV({ values[i] * numberTexWidth, 0.0f, numberTexWidth, numberTexHeight });
    }
}

void GameTimerUIActor::DrawTimerSettings()
{
#ifdef USE_IMGUI
    ImGui::DragFloat2("Timer UI Position", &battleTimerUiPosition.x, 1.0f);
    ImGui::DragFloat2("Timer UI Scale", &battleTimerUiScale.x, 0.01f, 0.01f, 4.0f);
    ImGui::DragFloat2(U8("砂時計サイズ"), &battleTimerHourglassSize.x, 1.0f, 1.0f, 1000.0f);
    ImGui::DragFloat("Hourglass Offset X", &battleTimerHourglassOffset.x, 1.0f);
    ImGui::DragFloat("Hourglass Offset Y", &battleTimerHourglassOffset.y, 1.0f);
    ImGui::DragFloat("Hourglass Sand Offset X", &battleTimerHourglassSandOffset.x, 1.0f);
    ImGui::DragFloat("Hourglass Sand Offset Y", &battleTimerHourglassSandOffset.y, 1.0f);
    ImGui::DragFloat(U8("数字間隔"), &battleTimerNumberSpacing, 1.0f, 1.0f, 200.0f);
    ImGui::DragFloat(U8("砂時計と数字の間隔"), &battleTimerHourglassNumberSpacing, 1.0f, 0.0f, 500.0f);
    ImGui::DragFloat("Colon Offset X", &battleTimerColonOffset.x, 1.0f);
    ImGui::DragFloat("Colon Offset Y", &battleTimerColonOffset.y, 1.0f);
    ImGui::DragFloat("Colon Scale", &battleTimerColonScale, 0.01f, 0.01f, 4.0f);
#endif
}
