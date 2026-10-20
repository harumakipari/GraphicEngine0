#pragma once
#include "Core/Actor.h"

class UIImageComponent;

class GameTimerUIActor : public Actor
{
public:
    explicit GameTimerUIActor(const std::string& actorName) : Actor(actorName) {}

    void Initialize(const Transform& transform) override;
    void DrawImGuiDetails()override;

    void SetVisible(bool visible);
    void SetElapsedTime(float elapsedTime, float numberTexWidth, float numberTexHeight) const;
    

private:

    std::shared_ptr<UIImageComponent> battleTimerHourglassFrame;
    std::shared_ptr<UIImageComponent> battleTimerHourglassSand;
    std::array<std::shared_ptr<UIImageComponent>, 5> battleTimerDigits{};

    DirectX::XMFLOAT2 battleTimerUiPosition{ 1744.0f, 130.0f };
    DirectX::XMFLOAT2 battleTimerUiScale{ 0.27f, 0.27f };
    DirectX::XMFLOAT2 battleTimerHourglassSize{ 197.0f, 233.0f };
    DirectX::XMFLOAT2 battleTimerHourglassOffset{ 88.0f,-52.0f };
    DirectX::XMFLOAT2 battleTimerHourglassSandOffset{ 0.0f, 19.0f };
    DirectX::XMFLOAT2 battleTimerColonOffset{ 2.0f,0.0f };
    float battleTimerColonScale = 2.4f;
    float battleTimerHourglassAngle = 0.0f;
    float battleTimerNumberSpacing = 80.0f;
    float battleTimerHourglassNumberSpacing = 72.0f;
    bool battleTimerVisible = false;
};
