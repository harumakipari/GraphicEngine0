#include "pch.h"
#include "SceneComponent.h"

#include "Core/Actor.h"
#include "Core/ActorManager.h"
#include "Engine/Utility/Win32Utils.h"


void SceneComponent::UpdateComponentToWorld(UpdateTransformFlags updateTransformFlags, TeleportType teleport)
{
    UpdateComponentToWorldWithParent(GetAttachParent().get(), GetAttachSocketNode(), updateTransformFlags, teleport);
}

// 親の Transform をもとに、自分の Transform（= ComponentToWorld）を更新し、必要に応じてその変更を子へ伝播する
void SceneComponent::UpdateComponentToWorldWithParent(SceneComponent* parent, int socketNode, UpdateTransformFlags updateTransformFlags, TeleportType teleport)
{
    // 親がまだ更新されていない場合は、親の階層を上って更新する必要がある
    if (parent && !parent->componentToWorldTransformUpdate_)
    {
        parent->UpdateComponentToWorld();
        // 親の更新によって自分自身の更新が完了しているかもしれないので、そうなら処理を中断する
        if (componentToWorldTransformUpdate_)
        {
            return;
        }
    }
    componentToWorldTransformUpdate_ = true;

    // 新しい ComponentToWorld 変換を計算する
    const Transform relativeTransform(relativeLocation_, relativeRotation_, relativeScale_);

    Transform newTransform = CalculateNewComponentToWorldTransform(relativeTransform, parent, socketNode);

    // Transform に変更があったか判定する（浮動小数点の誤差を許容して比較）
    bool hasChanged = !GetComponentWorldTransform().Equals(newTransform, 1.0e-8f);

    // Transform に変更がある、または Teleport 指定がされているならば、
    // 他のコンポーネントが Teleport を検知する必要がある可能性もある
    if (hasChanged || teleport != TeleportType::None)
    {
        // Transform を更新
        componentToWorld_ = newTransform;

        // 変更を子に伝播させる
        PropagateTransformUpdate(true, updateTransformFlags, teleport);
    }
    else
    {
        // Transform は変わっていないが、子コンポーネントなどのために伝播する必要がある
        PropagateTransformUpdate(false);
    }
}

// このコンポーネントにアタッチされている全ての子コンポーネントの Transform を更新する
void SceneComponent::UpdateChildTransforms(UpdateTransformFlags updateTransformFlags, TeleportType teleport)
{
    if (attachChildren_.size() > 0)
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
                if (!childComponent->componentToWorldTransformUpdate_)
                {// まだ一度も更新されていない子ならそのまま更新
                    childComponent->UpdateComponentToWorld(updateTransformFlagsFromParent, teleport);
                }
                else
                {// すでに更新された子コンポーネントなら条件に応じてスキップ or 更新：

                    if (onlyUpdateIfUsingSocket && (childComponent->attachSocketNode_ == -1))
                    {// 「ソケット接続がある子だけ更新」指定あり、かつソケットが使われていなければスキップ
                        continue;
                    }
                    if (childComponent->IsUsingAbsoluteLocation() && childComponent->IsUsingAbsoluteRotation() && childComponent->IsUsingAbsoluteScale())
                    {// 子がすべて絶対座標（位置・回転・スケール）を使っているなら更新不要
                        continue;
                    }

                    childComponent->UpdateComponentToWorld(updateTransformFlagsFromParent, teleport);
                }
            }
        }
    }
}

// 自身の Transform の変更を反映し、必要に応じて子コンポーネントにもそれを伝播させる関数
void SceneComponent::PropagateTransformUpdate(bool transformChanged, UpdateTransformFlags updateTransformFlags, TeleportType teleport)
{
    // アタッチされている子コンポーネントの一覧を取得
    const std::vector<std::shared_ptr<SceneComponent>>& attachedChildren = GetAttachChildren();

    if (transformChanged)
    {
        if (attachChildren_.size() > 0)
        {// 子コンポーネントがいる場合は、それらを更新
            // 子供には skipPhysicsUpdate フラグを渡さない（物理エンジンが制御している場合を除く）
            UpdateTransformFlags childrenFlagNoPhysics = ~UpdateTransformFlags::SkipPhysicsUpdate & updateTransformFlags;
            // 子供たちの Transform を更新（再帰的）
            UpdateChildTransforms(childrenFlagNoPhysics, teleport);
        }
    }
    else
    {
        // 子コンポーネントがいるなら、デフォルト設定で更新を伝播
        if (attachChildren_.size() > 0)
        {
            UpdateChildTransforms();
        }
    }
}

// このコンポーネントを、指定された親コンポーネントにアタッチ（接続）する
// parent はアタッチ先の親コンポーネント　
// socketNode 接続先のソケットノード番号( -1 ならデフォルト)
void SceneComponent::AttachToComponent(const std::shared_ptr<SceneComponent>& parent, int socketNode)
{
    // エラー: parent が nullptr の場合はアサート（無効なポインタ）
    _ASSERT_EXPR(parent != nullptr, L"親コンポーネントのポインタが null です");
    // エラー: 自分自身にアタッチしようとしている場合
    _ASSERT_EXPR(parent.get() != this, L"自分自身にはアタッチ出来ません");
    // エラー: 同じ Actor 内で、ルートコンポーネントが他のコンポーネントにアタッチされようとしている
    _ASSERT_EXPR(!(owner_.lock() == parent->owner_.lock() && owner_.lock() && owner_.lock()->GetRootComponent().get() == this),
        L"同じアクター内のルートコンポーネントは他にアタッチできません");
    // エラー: サイクル（循環）を作ろうとしている場合（親が既にこのコンポーネントにぶら下がっている）
    _ASSERT_EXPR(!parent->IsAttachAbove(this), L"循環参照が発生します");

    // すでに他にアタッチされている場合はデタッチ (親を変更)
    if (attachParent_.lock())
    {
        DetachFromParent();
    }

    // 親の子供リストに自分を追加
    parent->attachChildren_.emplace_back(std::dynamic_pointer_cast<SceneComponent>(shared_from_this()));

    // 親への参照とソケットノードを記録
    attachParent_ = parent;
    attachSocketNode_ = socketNode;
}

// このコンポーネントを現在の親のコンポーネントから切り離す
void SceneComponent::DetachFromParent()
{
    std::shared_ptr<SceneComponent> parent = attachParent_.lock();
    if (parent)
    {
        // デタッチ時にもサイクルチェック（念のため）
        _ASSERT_EXPR(!parent->IsAttachAbove(this), L"循環参照が発生します");

        // 親の子供リストから自分を削除
        for (decltype(parent->attachChildren_)::iterator child = parent->attachChildren_.begin(); child != parent->attachChildren_.end();)
        {
            if (child->get() == this)
            {
                parent->attachChildren_.erase(child);
                break;
            }
            child++;
        }

        // 親の参照をクリア
        attachParent_.reset();
    }
    // ソケット情報もリセット
    attachSocketNode_ = -1;
}

// 消去処理
void SceneComponent::Destroy()
{
    // 子から先に削除
    for (auto& child : attachChildren_)
    {
        if (child)
        {
            child->Destroy();
        }
    }
    attachChildren_.clear();

    // 親との接続も解除
    DetachFromParent();

    OnUnregister();
    SetActive(false);
}

// 初期化時に困るから即時 Transform 更新処理
void SceneComponent::UpdateTransformImmediate()
{
    UpdateComponentToWorld();
    for (auto& child : attachChildren_)
    {
        child->UpdateTransformImmediate();
    }
}


void SceneComponent::AddWorldOffset(const DirectX::XMFLOAT3& offset)
{
    // 現在のワールド座標を取得
    //DirectX::XMMATRIX worldMat = componentToWorld_.ToMatrix();
    //DirectX::XMVECTOR currentPos = DirectX::XMVector3TransformCoord(DirectX::XMVectorZero(), worldMat);

    // offset を加算
    DirectX::XMFLOAT3 worldPos = GetComponentLocation();
    //DirectX::XMStoreFloat3(&worldPos, currentPos);

    worldPos.x += offset.x;
    worldPos.y += offset.y;
    worldPos.z += offset.z;

    GetOwner()->SetPosition(worldPos);

    //SetWorldLocationDirect(worldPos);
#if 0
    SetWorldPosition(worldPos);
    UpdateWorldMatrix();
#endif
}

void SceneComponent::AddLocalRotation(const DirectX::XMFLOAT3& offset)
{
    using namespace DirectX;

    // 現在の回転
    XMVECTOR q = XMLoadFloat4(&relativeRotation_);

    // オフセット回転（degree → rad）
    XMVECTOR dq = XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(offset.x),
        XMConvertToRadians(offset.y),
        XMConvertToRadians(offset.z)
    );

    // 回転を合成
    q = XMQuaternionMultiply(q, dq);

    // 保存
    XMStoreFloat4(&relativeRotation_, q);

    // Transform更新フラグ
    componentToWorldTransformUpdate_ = false;
}