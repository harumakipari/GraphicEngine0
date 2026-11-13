#include "pch.h"
#include "SceneComponent.h"

#include "Core/Actor.h"
#include "Core/ActorManager.h"
#include "Engine/Utility/Win32Utils.h"

void SceneComponent::UpdateComponentToWorld(const UpdateTransformFlags updateTransformFlags)
{
    UpdateComponentToWorldWithParent(GetAttachParent().get(), GetAttachSocketNode(), updateTransformFlags);
}

// 消去処理
void SceneComponent::Destroy()
{
    // 子から先に削除
    const auto children = attachChildren;
    for (const auto& child : children)
    {
        if (child)
        {
            child->Destroy();
        }
    }
    attachChildren.clear();

    // 親との接続も解除
    DetachFromParent();

    OnUnregister();
    SetActive(false);
}

void SceneComponent::DrawImGuiInspector()
{
#ifdef USE_IMGUI
    inspectorEuler = GetRelativeEulerRotation();

    if (ImGui::TreeNodeEx((name + "  Transform").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat3("Relative Location", &relativeLocation.x, 0.1f);

        DirectX::XMFLOAT3 prevEuler = inspectorEuler;

        if (ImGui::DragFloat3("RelativeAngle", &inspectorEuler.x, 1.0f))
        {
            DirectX::XMFLOAT3 delta =
            {
                inspectorEuler.x - prevEuler.x,
                inspectorEuler.y - prevEuler.y,
                inspectorEuler.z - prevEuler.z
            };

            DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&GetRelativeRotation());

            DirectX::XMVECTOR dq = DirectX::XMQuaternionRotationRollPitchYaw(
                DirectX::XMConvertToRadians(delta.x),
                DirectX::XMConvertToRadians(delta.y),
                DirectX::XMConvertToRadians(delta.z)
            );

            q = DirectX::XMQuaternionMultiply(q, dq);

            DirectX::XMFLOAT4 newRot;
            DirectX::XMStoreFloat4(&newRot, q);

            SetRelativeRotationDirect(newRot);
        }
        ImGui::DragFloat3("Relative Scale", &relativeScale.x, 0.01f, 0.01f, 100.0f);
        ImGui::TreePop();
    }


    if (ImGui::TreeNode((name + "   Debug Info").c_str()))
    {
        ImGui::Text("Children: %zu", attachChildren.size());
        ImGui::Text("Parent: %s", attachParent.lock() ? attachParent.lock()->GetName().c_str() : "None");
        ImGui::TreePop();
    }
#endif
}

// 既存の親を解除して接続する。socketNodeが-1の場合はソケットを指定しない
void SceneComponent::AttachToComponent(const std::shared_ptr<SceneComponent>& parent, const int socketNode)
{
    // エラー: parent が nullptr の場合はアサート（無効なポインタ）
    _ASSERT_EXPR(parent != nullptr, L"親コンポーネントのポインタが null です");
    // エラー: 自分自身にアタッチしようとしている場合
    _ASSERT_EXPR(parent.get() != this, L"自分自身にはアタッチ出来ません");
    // エラー: 同じ Actor 内で、ルートコンポーネントが他のコンポーネントにアタッチされようとしている
    _ASSERT_EXPR(!(owner.lock() == parent->owner.lock() && owner.lock() && owner.lock()->GetRootComponent().get() == this),
        L"同じアクター内のルートコンポーネントは他にアタッチできません");
    // エラー: サイクル（循環）を作ろうとしている場合（親が既にこのコンポーネントにぶら下がっている）
    _ASSERT_EXPR(!parent->IsAttachAbove(this), L"循環参照が発生します");

    // すでに他にアタッチされている場合はデタッチ (親を変更)
    if (attachParent.lock())
    {
        DetachFromParent();
    }

    // 親の子供リストに自分を追加
    parent->attachChildren.emplace_back(std::dynamic_pointer_cast<SceneComponent>(shared_from_this()));

    // 親への参照とソケットノードを記録
    attachParent = parent;
    attachSocketNode = socketNode;
}

void SceneComponent::AddWorldOffset(const DirectX::XMFLOAT3& offset) const
{
    DirectX::XMFLOAT3 worldPos = GetComponentLocation();

    worldPos.x += offset.x;
    worldPos.y += offset.y;
    worldPos.z += offset.z;

    GetOwner()->SetPosition(worldPos);
}

void SceneComponent::AddLocalRotation(const DirectX::XMFLOAT3& offset)
{
    using namespace DirectX;

    // 現在の回転
    XMVECTOR q = XMLoadFloat4(&relativeRotation);

    // オフセット回転（degree → rad）
    XMVECTOR dq = XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(offset.x),
        XMConvertToRadians(offset.y),
        XMConvertToRadians(offset.z)
    );

    // 回転を合成
    q = XMQuaternionMultiply(q, dq);

    // 保存
    XMStoreFloat4(&relativeRotation, q);

    // Transform更新フラグ
    componentToWorldTransformUpdate = false;
}

// 自身と子コンポーネントのワールドTransformを前フレーム用に保存する
void SceneComponent::CapturePreviousTransform()
{
    previousComponentToWorld = componentToWorld;

    for (auto& child : attachChildren)
    {
        child->CapturePreviousTransform();
    }
}

DirectX::XMFLOAT4 SceneComponent::QuaternionFromEulerYXZ(const DirectX::XMFLOAT3& eulerRadians)
{
    using namespace DirectX;
    XMVECTOR qx = XMQuaternionRotationAxis({ 1,0,0 }, eulerRadians.x);
    XMVECTOR qy = XMQuaternionRotationAxis({ 0,1,0 }, eulerRadians.y);
    XMVECTOR qz = XMQuaternionRotationAxis({ 0,0,1 }, eulerRadians.z);

    // 順序 Y → X → Z（YXZ intrinsic）
    XMVECTOR q = XMQuaternionMultiply(qz, XMQuaternionMultiply(qx, qy));
    XMFLOAT4 result;
    XMStoreFloat4(&result, q);
    return result;
}

// 親がある場合は、ワールド座標を親からの相対座標に変換して設定する
void SceneComponent::SetWorldLocationDirect(const DirectX::XMFLOAT3& newWorldLocation)
{
    if (auto parent = attachParent.lock())
    {
        // 親のワールドトランスフォームの逆行列を使って、相対位置を求める
        DirectX::XMMATRIX parentWorld = parent->GetComponentWorldTransform().ToMatrix();
        DirectX::XMMATRIX parentInv = DirectX::XMMatrixInverse(nullptr, parentWorld);
        DirectX::XMVECTOR worldPos = DirectX::XMLoadFloat3(&newWorldLocation);
        DirectX::XMVECTOR localPos = DirectX::XMVector3TransformCoord(worldPos, parentInv);
        DirectX::XMFLOAT3 relative;
        DirectX::XMStoreFloat3(&relative, localPos);
        SetRelativeLocationDirect(relative);
    }
    else
    {
        SetRelativeLocationDirect(newWorldLocation);
    }
}

// ワールド空間でのこのコンポーネントのクォータニオンを設定
void SceneComponent::SetWorldRotationDirect(const DirectX::XMFLOAT4& newWorldRotation)
{
    using namespace DirectX;

    if (auto parent = attachParent.lock())
    {
        XMVECTOR worldRot = XMLoadFloat4(&newWorldRotation);
        XMFLOAT4 rot = parent->GetComponentWorldTransform().GetRotation();

        XMVECTOR parentWorldRot =
            XMLoadFloat4(&rot);

        XMVECTOR parentInv = XMQuaternionInverse(parentWorldRot);

        XMVECTOR localRot = XMQuaternionMultiply(parentInv, worldRot);

        XMFLOAT4 relative;
        XMStoreFloat4(&relative, localRot);

        SetRelativeRotationDirect(relative);
    }
    else
    {
        SetRelativeRotationDirect(newWorldRotation);
    }
}

// ワールド空間でのこのコンポーネントの角度を設定
void SceneComponent::SetWorldEulerRotationDirect(const DirectX::XMFLOAT3& newWorldEuler)
{
    if (auto parent = attachParent.lock())
    {
        parent->SetRelativeEulerRotationDirect(newWorldEuler);
    }
    else
    {
        SetRelativeEulerRotationDirect(newWorldEuler);
    }
}

// ワールド空間でのこのコンポーネントのスケールを設定
void SceneComponent::SetWorldScaleDirect(const DirectX::XMFLOAT3& newWorldScale)
{
    if (auto parent = attachParent.lock())
    {
        parent->SetRelativeScaleDirect(newWorldScale);
    }
    else
    {
        SetRelativeScaleDirect(newWorldScale);
    }
}

// ワールド空間でのこのコンポーネントの Transform を直接設定
void SceneComponent::SetWorldMatrixDirect(const DirectX::XMFLOAT4X4& worldMatrix)
{
    using namespace DirectX;

    XMMATRIX m = XMLoadFloat4x4(&worldMatrix);

    XMVECTOR scale;
    XMVECTOR rotation;
    XMVECTOR translation;

    XMMatrixDecompose(&scale, &rotation, &translation, m);

    XMFLOAT3 pos;
    XMFLOAT3 scl;
    XMFLOAT4 rot;

    XMStoreFloat3(&pos, translation);
    XMStoreFloat3(&scl, scale);
    XMStoreFloat4(&rot, rotation);

    SetWorldLocationDirect(pos);
    SetWorldRotationDirect(rot);
}

// 親からの相対的なこのコンポーネントの Transform を直接設定
void SceneComponent::SetRelativeMatrixDirect(const DirectX::XMFLOAT4X4& matrix)
{
    using namespace DirectX;

    XMMATRIX m = XMLoadFloat4x4(&matrix);

    XMVECTOR scale;
    XMVECTOR rotation;
    XMVECTOR translation;

    XMMatrixDecompose(&scale, &rotation, &translation, m);

    XMFLOAT3 pos;
    XMFLOAT3 scl;
    XMFLOAT4 rot;

    XMStoreFloat3(&pos, translation);
    XMStoreFloat3(&scl, scale);
    XMStoreFloat4(&rot, rotation);

    SetRelativeRotationDirect(rot);
}

// ワールドTransformが未更新の場合に更新する
void SceneComponent::ConditionalUpdateComponentWorldTransform()
{
    if (!componentToWorldTransformUpdate)
    {
        UpdateComponentToWorld();
    }
}

// このコンポーネントにアタッチされている全ての子コンポーネントの Transform を更新する
void SceneComponent::UpdateChildTransforms(const UpdateTransformFlags updateTransformFlags) const
{
    if (attachChildren.size() > 0)
    {// 子コンポーネントが存在していたら、

        // OnlyUpdateIfUsingSocket フラグが設定されているかどうかを判定。
        // このフラグが有効なときは、「親とソケット接続されていない子」は Transform 更新対象外になる。
        const bool onlyUpdateIfUsingSocket = !!static_cast<bool>(updateTransformFlags & UpdateTransformFlags::OnlyUpdateIfUsingSocket);

        // OnlyUpdateIfUsingSocket フラグを除外した更新フラグを作成
        const UpdateTransformFlags updateTransformNoSocketSkip = ~UpdateTransformFlags::OnlyUpdateIfUsingSocket & updateTransformFlags;
        // PropagateFromParent　フラグ（「親から伝播してきた更新」という印）を追加。
        const UpdateTransformFlags updateTransformFlagsFromParent = updateTransformNoSocketSkip | UpdateTransformFlags::PropagateFromParent;

        // すべての子コンポーネントに対して処理を行う。
        for (const std::shared_ptr<SceneComponent>& childComponent : GetAttachChildren())
        {
            if (childComponent != nullptr)
            {
                if (!childComponent->componentToWorldTransformUpdate)
                {// まだ一度も更新されていない子ならそのまま更新
                    childComponent->UpdateComponentToWorld(updateTransformFlagsFromParent);
                }
                else
                {// すでに更新された子コンポーネントなら条件に応じてスキップ or 更新：

                    if (onlyUpdateIfUsingSocket && (childComponent->attachSocketNode == -1))
                    {// 「ソケット接続がある子だけ更新」指定あり、かつソケットが使われていなければスキップ
                        continue;
                    }
                    if (childComponent->IsUsingAbsoluteLocation() && childComponent->IsUsingAbsoluteRotation() && childComponent->IsUsingAbsoluteScale())
                    {// 子がすべて絶対座標（位置・回転・スケール）を使っているなら更新不要
                        continue;
                    }

                    childComponent->UpdateComponentToWorld(updateTransformFlagsFromParent);
                }
            }
        }
    }
}

// 親を指定しない場合は、現在の接続先を使ってワールドTransformを計算する
Transform SceneComponent::CalculateNewComponentToWorldTransform(const Transform& newRelativeTransform, const SceneComponent* parent, int socketNode)const
{
    // socketNode と parent を指定されていなければ、アタッチされた情報から取得
    socketNode = parent ? socketNode : attachSocketNode;
    parent = parent ? parent : attachParent.lock().get();
    if (parent)
    {
        // 「絶対位置・絶対回転・絶対スケール」のいずれかを使っているか
        const bool general = IsUsingAbsoluteLocation() || IsUsingAbsoluteRotation() || IsUsingAbsoluteScale();
        if (!general)
        {// 絶対座標指定がされていないなら（＝通常の親からの相対的な座標なら）
            return newRelativeTransform * parent->GetSocketTransform(socketNode);
        }
        // 絶対指定が含まれているから、特殊な合成処理を行う
        return CalculateNewComponentToWorldGeneralCase(newRelativeTransform, parent, socketNode);
    }
    // 親が存在しない時は、自分の相対 Transform がそのまま WorldTransform になる
    return newRelativeTransform;

}

// 位置・回転・スケールの絶対指定を考慮してワールドTransformを計算する
Transform SceneComponent::CalculateNewComponentToWorldGeneralCase(const Transform& newRelativeTransform, const SceneComponent* parent, const int socketNode)const
{
    if (parent != nullptr)
    {
        const Transform parentToWorld = parent->GetSocketTransform(socketNode);
        Transform newComponentToWorldTransform = newRelativeTransform * parentToWorld;

        // 絶対位置が有効なら、位置だけは親の影響を無視
        if (absoluteLocation)
        {
            newComponentToWorldTransform.translation_ = newRelativeTransform.translation_;
        }
        // 絶対回転が有効なら、回転だけは親の影響を無視
        if (absoluteRotation)
        {
            newComponentToWorldTransform.rotation_ = newRelativeTransform.rotation_;
        }
        // 絶対スケールが有効なら、スケールだけは親の影響を無視
        if (absoluteScale)
        {
            // ただの代入ではなく、符号の補正を加えたスケール合成
            newComponentToWorldTransform.scale_ = DirectX::XMVectorMultiply(newRelativeTransform.scale_, MathHelper::VectorSign(newComponentToWorldTransform.scale_));
        }

        return newComponentToWorldTransform;
    }
    return newRelativeTransform;
}

// このコンポーネントを現在の親のコンポーネントから切り離す
void SceneComponent::DetachFromParent()
{
    std::shared_ptr<SceneComponent> parent = attachParent.lock();
    if (parent)
    {
        // デタッチ時にもサイクルチェック（念のため）
        _ASSERT_EXPR(!parent->IsAttachAbove(this), L"循環参照が発生します");

        // 親の子供リストから自分を削除
        for (decltype(parent->attachChildren)::iterator child = parent->attachChildren.begin(); child != parent->attachChildren.end();)
        {
            if (child->get() == this)
            {
                parent->attachChildren.erase(child);
                break;
            }
            child++;
        }

        // 親の参照をクリア
        attachParent.reset();
    }
    // ソケット情報もリセット
    attachSocketNode = -1;
}

// Transformの変更状態と更新フラグに応じて、子コンポーネントへ更新を伝播する
void SceneComponent::PropagateTransformUpdate(const bool transformChanged, const UpdateTransformFlags updateTransformFlags) const
{
    if (transformChanged)
    {
        if (attachChildren.size() > 0)
        {// 子コンポーネントがいる場合は、それらを更新
            // 子供には skipPhysicsUpdate フラグを渡さない（物理エンジンが制御している場合を除く）
            UpdateTransformFlags childrenFlagNoPhysics = ~UpdateTransformFlags::SkipPhysicsUpdate & updateTransformFlags;
            // 子供たちの Transform を更新（再帰的）
            UpdateChildTransforms(childrenFlagNoPhysics);
        }
    }
    else
    {
        // 子コンポーネントがいるなら、デフォルト設定で更新を伝播
        if (attachChildren.size() > 0)
        {
            UpdateChildTransforms();
        }
    }
}

// 指定された親とソケットを使ってワールドTransformを更新し、必要に応じて子へ伝播する
void SceneComponent::UpdateComponentToWorldWithParent(SceneComponent* parent, const int socketNode, const UpdateTransformFlags updateTransformFlags)
{
    // 親がまだ更新されていない場合は、親の階層を上って更新する必要がある
    if (parent && !parent->componentToWorldTransformUpdate)
    {
        parent->UpdateComponentToWorld();
        // 親の更新によって自分自身の更新が完了しているかもしれないので、そうなら処理を中断する
        if (componentToWorldTransformUpdate)
        {
            return;
        }
    }
    componentToWorldTransformUpdate = true;

    // 新しい ComponentToWorld 変換を計算する
    const Transform relativeTransform(relativeLocation, relativeRotation, relativeScale);

    Transform newTransform = CalculateNewComponentToWorldTransform(relativeTransform, parent, socketNode);

    // Transform に変更があったか判定する（浮動小数点の誤差を許容して比較）
    bool hasChanged = !GetComponentWorldTransform().Equals(newTransform, 1.0e-8f);

    // Transform に変更がある
    if (hasChanged)
    {
        // Transform を更新
        componentToWorld = newTransform;

        // 変更を子に伝播させる
        PropagateTransformUpdate(true, updateTransformFlags);
    }
    else
    {
        // Transform は変わっていないが、子コンポーネントなどのために伝播する必要がある
        PropagateTransformUpdate(false);
    }
}

// 指定されたコンポーネントが自身の祖先にあたるか判定する
bool SceneComponent::IsAttachAbove(const SceneComponent* component) const
{
    // 親から辿っていって、指定された component に一致するかをチェック
    for (SceneComponent* parent = attachParent.lock().get(); parent; parent = parent->attachParent.lock().get())
    {
        if (parent == component)
        {
            return true;
        }
    }

    // どの親にも一致しなければ false
    return false;
}
