#include "pch.h"
#include "CurveEditorWidget.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "ImCurveEdit.h"

namespace
{
    constexpr float kTimeScale = 100.0f;

    class EffectCurveDelegate final : public ImCurveEdit::Delegate
    {
    public:
        EffectCurveDelegate(FloatCurve& sourceCurve, const CurveEditorWidgetSettings& sourceSettings)
            : curve(sourceCurve), settings(sourceSettings)
        {
            curve.Sanitize(settings.valueMin, settings.valueMax);
            SyncFromCurve();
            viewMin = { 0.0f, settings.valueMin };
            viewMax = { kTimeScale - 1.0f, settings.valueMax };
        }

        size_t GetCurveCount() override { return 1; }
        ImCurveEdit::CurveType GetCurveType(size_t) const override { return ImCurveEdit::CurveLinear; }
        ImVec2& GetMin() override { return viewMin; }
        ImVec2& GetMax() override { return viewMax; }
        size_t GetPointCount(size_t) override { return points.size(); }
        uint32_t GetCurveColor(size_t) override { return IM_COL32(90, 220, 255, 255); }
        ImVec2* GetPoints(size_t) override { return points.data(); }
        unsigned int GetBackgroundColor() override { return IM_COL32(28, 28, 32, 255); }

        int EditPoint(size_t, int pointIndex, ImVec2 value) override
        {
            if (pointIndex < 0 || pointIndex >= static_cast<int>(points.size())) return pointIndex;

            value.x = std::clamp(value.x, 0.0f, kTimeScale);
            value.y = std::clamp(value.y, settings.valueMin, settings.valueMax);
            if (pointIndex == 0) value.x = 0.0f;
            if (pointIndex == static_cast<int>(points.size()) - 1) value.x = kTimeScale;

            points[pointIndex] = value;
            SyncToCurve();
            changed = true;

            const float editedTime = value.x / kTimeScale;
            for (int i = 0; i < static_cast<int>(curve.points.size()); ++i)
            {
                if (std::abs(curve.points[i].time - editedTime) <= 1.0e-5f) return i;
            }
            return pointIndex;
        }

        void AddPoint(size_t, ImVec2 value) override
        {
            if (curve.points.size() >= settings.maxPoints) return;
            curve.points.push_back({
                std::clamp(value.x / kTimeScale, 0.0f, 1.0f),
                std::clamp(value.y, settings.valueMin, settings.valueMax) });
            curve.Sanitize(settings.valueMin, settings.valueMax);
            SyncFromCurve();
            changed = true;
        }

        void SyncFromCurve()
        {
            points.clear();
            points.reserve(curve.points.size());
            for (const auto& point : curve.points)
                points.emplace_back(point.time * kTimeScale, point.value);
        }

        void SyncToCurve()
        {
            for (size_t i = 0; i < points.size() && i < curve.points.size(); ++i)
            {
                curve.points[i].time = points[i].x / kTimeScale;
                curve.points[i].value = points[i].y;
            }
            curve.Sanitize(settings.valueMin, settings.valueMax);
            SyncFromCurve();
        }

        FloatCurve& curve;
        const CurveEditorWidgetSettings& settings;
        std::vector<ImVec2> points;
        ImVec2 viewMin{};
        ImVec2 viewMax{};
        bool changed = false;
    };

    struct EffectCurveWidgetState
    {
        ImCurveEdit::EditState editState;
        int selectedPoint = -1;
        float contextTime = 0.0f;
        float contextValue = 0.0f;
    };

    std::unordered_map<ImGuiID, EffectCurveWidgetState> widgetStates;
}
#endif

CurveEditorWidgetResult CurveEditorWidget::Draw(
    const char* label,
    FloatCurve& curve,
    const CurveEditorWidgetSettings& settings)
{
    CurveEditorWidgetResult result;
#ifdef USE_IMGUI
    ImGui::PushID(label);
    const ImGuiID widgetId = ImGui::GetID("CurveEditor");
    auto& widgetState = widgetStates[widgetId];
    EffectCurveDelegate delegate(curve, settings);
    ImVector<ImCurveEdit::EditPoint> selection;

    ImGui::TextUnformatted(label);
    const ImVec2 graphPos = ImGui::GetCursorScreenPos();
    const float graphWidth = (std::max)(ImGui::GetContentRegionAvail().x, 240.0f);
    const ImVec2 graphSize(graphWidth, settings.height);
    if (ImCurveEdit::Edit(delegate, graphSize, widgetId, nullptr, &selection, &widgetState.editState) != 0)
        result.changed = true;

    if (delegate.changed) result.changed = true;
    if (delegate.focused && !selection.empty())
        widgetState.selectedPoint = selection[0].pointIndex;

    const ImVec2 graphMax(graphPos.x + graphSize.x, graphPos.y + graphSize.y);
    if (ImGui::IsMouseHoveringRect(graphPos, graphMax) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        const ImVec2 mousePosition = ImGui::GetMousePos();
        widgetState.contextTime = std::clamp((mousePosition.x - graphPos.x) / graphSize.x, 0.0f, 1.0f);
        const float valueRatio = 1.0f - std::clamp((mousePosition.y - graphPos.y) / graphSize.y, 0.0f, 1.0f);
        widgetState.contextValue = std::lerp(settings.valueMin, settings.valueMax, valueRatio);
        ImGui::OpenPopup("CurveContextMenu");
    }

    int& selected = widgetState.selectedPoint;
    if (selected >= static_cast<int>(curve.points.size())) selected = -1;

    const auto deleteSelectedPoint = [&]()
    {
        if (selected <= 0 || selected >= static_cast<int>(curve.points.size()) - 1) return;
        curve.points.erase(curve.points.begin() + selected);
        curve.Sanitize(settings.valueMin, settings.valueMax);
        widgetState.editState.selection.clear();
        selected = -1;
        result.changed = true;
    };

    if (ImGui::BeginPopup("CurveContextMenu"))
    {
        const bool canAdd = curve.points.size() < settings.maxPoints;
        ImGui::BeginDisabled(!canAdd);
        if (ImGui::MenuItem("Add Key"))
        {
            delegate.AddPoint(0, { widgetState.contextTime * kTimeScale, widgetState.contextValue });
            result.changed = true;
            selected = -1;
            for (int i = 0; i < static_cast<int>(curve.points.size()); ++i)
            {
                if (std::abs(curve.points[i].time - widgetState.contextTime) <= 1.0e-5f)
                {
                    selected = i;
                    break;
                }
            }
            widgetState.editState.selection.clear();
            if (selected >= 0)
                widgetState.editState.selection.insert({ 0, selected });
        }
        ImGui::EndDisabled();

        const bool canDelete = selected > 0 && selected < static_cast<int>(curve.points.size()) - 1;
        ImGui::BeginDisabled(!canDelete);
        if (ImGui::MenuItem("Delete Key"))
            deleteSelectedPoint();
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int i = 1; i < 10; ++i)
    {
        const float x = graphPos.x + graphSize.x * (static_cast<float>(i) / 10.0f);
        drawList->AddLine({ x, graphPos.y }, { x, graphPos.y + graphSize.y }, IM_COL32(255, 255, 255, 24));
    }
    for (int i = 1; i < 4; ++i)
    {
        const float y = graphPos.y + graphSize.y * (static_cast<float>(i) / 4.0f);
        drawList->AddLine({ graphPos.x, y }, { graphPos.x + graphSize.x, y }, IM_COL32(255, 255, 255, 24));
    }

    if (delegate.focused && ImGui::IsKeyPressed(ImGuiKey_Delete) && selected > 0 && selected < static_cast<int>(curve.points.size()) - 1)
        deleteSelectedPoint();

    if (selected >= 0 && selected < static_cast<int>(curve.points.size()))
    {
        ImGui::SeparatorText("Selected Key");
        auto& point = curve.points[selected];
        if (selected == 0 || selected == static_cast<int>(curve.points.size()) - 1)
        {
            ImGui::BeginDisabled();
            ImGui::DragFloat("Time", &point.time, 0.01f, 0.0f, 1.0f, "%.3f");
            ImGui::EndDisabled();
        }
        else if (ImGui::DragFloat("Time", &point.time, 0.01f, 0.0f, 1.0f, "%.3f"))
        {
            result.changed = true;
        }
        if (ImGui::DragFloat(settings.valueLabel, &point.value, 0.01f, settings.valueMin, settings.valueMax, "%.3f"))
            result.changed = true;

        if (result.changed)
        {
            const float selectedTime = point.time;
            curve.Sanitize(settings.valueMin, settings.valueMax);
            selected = 0;
            for (int i = 0; i < static_cast<int>(curve.points.size()); ++i)
            {
                if (std::abs(curve.points[i].time - selectedTime) <= 1.0e-5f)
                {
                    selected = i;
                    break;
                }
            }
        }
    }
    else
    {
        ImGui::TextDisabled("Right-click the graph to add a key (maximum %zu).", settings.maxPoints);
    }

    result.selectedPoint = selected;
    ImGui::PopID();
#endif
    return result;
}
