#pragma once

#include "Engine/Effects/EffectManager.h"

struct CurveEditorWidgetSettings
{
    float valueMin = 0.0f;
    float valueMax = 1.0f;
    const char* valueLabel = "Value";
    float height = 150.0f;
    size_t maxPoints = FloatCurve::MaxPoints;
};

struct CurveEditorWidgetResult
{
    bool changed = false;
    int selectedPoint = -1;
};

class CurveEditorWidget
{
public:
    static CurveEditorWidgetResult Draw(
        const char* label,
        FloatCurve& curve,
        const CurveEditorWidgetSettings& settings);
};
