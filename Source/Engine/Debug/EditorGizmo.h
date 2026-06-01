#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

class SceneComponent;

class EditorGizmo
{
public:
    static void Draw(
        const std::shared_ptr<SceneComponent>& target,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& proj, const ImVec2& rectPos,
        const ImVec2& rectSize);

    static void SetOperation(const ImGuizmo::OPERATION op);
private:
    static inline ImGuizmo::OPERATION operation_ = ImGuizmo::TRANSLATE;
};
