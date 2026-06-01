#include "pch.h"
#include "EditorGizmo.h"

#include "Components/Base/SceneComponent.h"

struct DecomposedTransform
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 rotation;
    DirectX::XMFLOAT3 scale;
};

inline DecomposedTransform DecomposeMatrix(const DirectX::XMMATRIX& m)
{
    using namespace DirectX;

    DecomposedTransform out{};

    XMVECTOR scale;
    XMVECTOR rotation;
    XMVECTOR translation;

    XMMatrixDecompose(&scale, &rotation, &translation, m);

    XMStoreFloat3(&out.scale, scale);
    XMStoreFloat4(&out.rotation, rotation);
    XMStoreFloat3(&out.position, translation);

    return out;
}

void EditorGizmo::SetOperation(const ImGuizmo::OPERATION op)
{
    operation_ = op;
}

void EditorGizmo::Draw(
    const std::shared_ptr<SceneComponent>& target,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj,
    const ImVec2& rectPos,
    const ImVec2& rectSize)
{
    if (!target) return;

    Transform targetTransform = target->GetComponentWorldTransform();
    DirectX::XMMATRIX world = targetTransform.ToMatrix();

ImGuizmo::SetOrthographic(false);

    ImGuizmo::SetDrawlist();

    
    ImGuizmo::SetRect(
        rectPos.x,
        rectPos.y,
        rectSize.x,
        rectSize.y
    );

    bool manipulated = ImGuizmo::Manipulate(
        &view.r[0].m128_f32[0],
        &proj.r[0].m128_f32[0],
        operation_,
        ImGuizmo::WORLD,
        &world.r[0].m128_f32[0]);

    if (manipulated)
    {
        auto decomposed = DecomposeMatrix(world);

        target->SetWorldLocationDirect(decomposed.position);
        target->SetWorldRotationDirect(decomposed.rotation);
        target->SetWorldScaleDirect(decomposed.scale);
    }
}