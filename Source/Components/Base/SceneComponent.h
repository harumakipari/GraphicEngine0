#ifndef SCENE_COMPONENT_H
#define SCENE_COMPONENT_H

// C++ 標準ライブラリ
#include <memory>
#include <string>

// 他ライブラリ
#include <DirectXMath.h>

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "../External/imgui/imgui.h"
#endif

// プロジェクトの他のヘッダ
#include "Component.h"
#include "Components/Transform/Transform.h"


class Actor;


class SceneComponent :public Component, public std::enable_shared_from_this<SceneComponent>
{
public:
    SceneComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :Component(name, owner)
    {
    }

    virtual ~SceneComponent() {}

    // Tick で最終Transformを取得するときはこれを使う
    Transform GetFinalWorldTransform() const
    {
        return componentToWorld_;
    }

    // 消去処理
    virtual void Destroy() override;

    // 初期化時に困るから即時 Transform 更新処理
    void UpdateTransformImmediate();
protected:
    // SceneComponent や Transform 持ちクラスに追加
    DirectX::XMFLOAT3 inspectorEuler_ = { 0,0,0 };
    bool inspectorEulerInitialized_ = false;

    // このコンポーネントのワールド空間上でのTransform
    // final Transform_　親子関係を全て考慮した最終的なTransform
    Transform componentToWorld_;     // キャッシュ

protected:
    // 現在接続している親。　valid　なら　relativeLocation_ などはこのオブジェクトに対する相対値になる
    std::weak_ptr<SceneComponent> attachParent_; // 弱参照

    // 親のソケットノード (特定の接続ポイント) に接続する場合に使用されるオプションのインデックス
    int attachSocketNode_ = -1;

    // このコンポーネントに接続されている子供のシーンコンポーネントのリスト
    std::vector<std::shared_ptr<SceneComponent>> attachChildren_; // 子を保持（共有）

public:
    std::shared_ptr<SceneComponent> GetAttachParent() const
    {
        return attachParent_.lock();
    }

    int GetAttachSocketNode() const
    {
        return attachSocketNode_;
    }

    const std::vector<std::shared_ptr<SceneComponent>>& GetAttachChildren() const
    {
        return attachChildren_;
    }

    void DrawImGuiInspector() override
    {
#ifdef USE_IMGUI
        inspectorEuler_ = GetRelativeEulerRotation(); // ここをコメントアウトするかで、ImGuiがバグるか変わる。

        if (ImGui::TreeNodeEx((name_ + "  Transform").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat3("Relative Location", &relativeLocation_.x, 0.1f);

            static bool editingRotation = false;

            DirectX::XMFLOAT3 prevEuler = inspectorEuler_;

            if (ImGui::DragFloat3("RelativeAngle", &inspectorEuler_.x, 1.0f))
            {
                editingRotation = true;

                DirectX::XMFLOAT3 delta =
                {
                    inspectorEuler_.x - prevEuler.x,
                    inspectorEuler_.y - prevEuler.y,
                    inspectorEuler_.z - prevEuler.z
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
            else
            {
                editingRotation = false;
            }
            ImGui::DragFloat3("Relative Scale", &relativeScale_.x, 0.01f, 0.01f, 100.0f);
            ImGui::TreePop();
        }


        if (ImGui::TreeNode((name_ + "   Debug Info").c_str()))
        {
            ImGui::Text("Children: %zu", attachChildren_.size());
            ImGui::Text("Parent: %s", attachParent_.lock() ? attachParent_.lock()->GetName().c_str() : "None");
            ImGui::TreePop();
        }
#endif
    }
private:
    // 親からの相対的な位置
    DirectX::XMFLOAT3 relativeLocation_ = { 0.0f,0.0f,0.0f };

    // 親からの相対的なクォータニオン
    DirectX::XMFLOAT4 relativeRotation_ = { 0.0f,0.0f,0.0f,1.0f };

    // 親からの相対的なスケール
    DirectX::XMFLOAT3 relativeScale_ = { 1.0f,1.0f,1.0f };

private:



    // 0はfalse 1はtrue 符号なし8ビット整数

    // 相対的な位置、回転、スケールに基づいて worldTransform　を更新したことがあれば true
    // 開始時に　worldTrnasform が初期化されているかどうかを確認するために使う
    uint8_t componentToWorldTransformUpdate_ : 1 = 0; // <-１ビットだけ使う 

    // trueの時に、このコンポーネントやその子に対して、updateOverlapsを呼び出す必要がない
    // これは、ツリーを辿っても当たり判定の更新が不要な場合に、パフォーマンス最適化として使われる
    // 通常このフラグは　UpdateOverlaps 実行後に true　にセットされる
    // 状態（アタッチ変更、当たり判定設定など）が変わったときは、clearSkipUpdateOverlaps() を呼んでフラグを無効化する。
    uint8_t skipUpdateOverlaps_ : 1 = 0; //→ 当たり判定の更新処理をスキップして高速化するためのフラグ
    // 子コンポーネントに影響がない場合、無駄な処理を避ける

//　通常、シーンコンポーネントの位置などは親からの相対座標だけど、
// これらフラグがtrueだとワールドに対して直接指定される
// relativeLocation_ を親ではなくワールド座標系に対する位置とみなす場合に true
    uint8_t absoluteLocation_ : 1 = 0;
    // relativeRotation_ を親ではなくワールド座標系に対する位置とみなす場合に true
    uint8_t absoluteRotation_ : 1 = 0;
    // relativeScale_ を親ではなくワールド座標系に対する位置とみなす場合に true
    uint8_t absoluteScale_ : 1 = 0;


public:
    bool IsUsingAbsoluteLocation() const
    {
        return absoluteLocation_;
    }
    void SetUsingAbsoluteLocation(bool absoluteLocation)
    {
        absoluteLocation_ = absoluteLocation ? 1 : 0;
    }
    bool IsUsingAbsoluteRotation() const
    {
        return absoluteRotation_;
    }
    void SetUsingAbsoluteRotation(bool absoluteRotation)
    {
        absoluteRotation_ = absoluteRotation ? 1 : 0;
    }
    bool IsUsingAbsoluteScale() const
    {
        return absoluteScale_;
    }
    void SetUsingAbsoluteScale(bool absoluteScale)
    {
        absoluteScale_ = absoluteScale ? 1 : 0;
    }


public:
    // 相対的な座標を取得
    DirectX::XMFLOAT3 GetRelativeLocation() const
    {
        return relativeLocation_;
    }
    // 直接　相対的な座標を設定
    void SetRelativeLocationDirect(const DirectX::XMFLOAT3& newRelativeLocation)
    {
        relativeLocation_ = newRelativeLocation;
    }
    // 相対的なスケールを取得
    DirectX::XMFLOAT3 GetRelativeScale()const
    {
        return relativeScale_;
    }
    // 直接　相対的なスケールを設定
    void SetRelativeScaleDirect(const DirectX::XMFLOAT3& newRelativeScale)
    {
        relativeScale_ = newRelativeScale;
    }
    // 相対的な角度を取得   degree で返す
    DirectX::XMFLOAT3 GetRelativeEulerRotation()const
    {
        DirectX::XMFLOAT3 angle = MathHelper::QuaternionToEuler(relativeRotation_);

        return { DirectX::XMConvertToDegrees(angle.x),DirectX::XMConvertToDegrees(angle.y) ,DirectX::XMConvertToDegrees(angle.z) };
    }
    DirectX::XMFLOAT4 QuaternionFromEulerYXZ(const DirectX::XMFLOAT3& eulerRadians)
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
    // 直接　相対的な角度を設定
    void SetRelativeEulerRotationDirect(const DirectX::XMFLOAT3& newEulerRotaion)
    {
        DirectX::XMStoreFloat4(&relativeRotation_, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(newEulerRotaion.x), DirectX::XMConvertToRadians(newEulerRotaion.y), DirectX::XMConvertToRadians(newEulerRotaion.z)));
    }
    // 相対的なクォータニオンを取得
    const DirectX::XMFLOAT4& GetRelativeRotation() const
    {
        return relativeRotation_;
    }
    // 直接　相対的なクォータニオンを設定
    void SetRelativeRotationDirect(const DirectX::XMFLOAT4& newRelativeRotation)
    {
        relativeRotation_ = newRelativeRotation;
    }
    //  このコンポーネントのワールド空間上でのTransformを取得
    _NODISCARD const Transform& GetComponentWorldTransform() const
    {
        return componentToWorld_;
    }
    //  このコンポーネントのワールド空間上でのTransformを取得(書き換え用)
    _NODISCARD Transform& GetComponentWorldTransform()
    {
        return componentToWorld_;
    }
    // ワールド空間でのこのコンポーネントの位置を取得
    DirectX::XMFLOAT3 GetComponentLocation() const
    {
        return GetComponentWorldTransform().GetLocation();
    }
    // ワールド空間でのこのコンポーネントのクォータニオンを取得
    DirectX::XMFLOAT4 GetComponentRotation() const
    {
        return GetComponentWorldTransform().GetRotation();
    }
    // ワールド空間でのこのコンポーネントのスケールを取得
    DirectX::XMFLOAT3 GetComponentScale() const
    {
        return GetComponentWorldTransform().GetScale();
    }
    // ワールド空間でのこのコンポーネントの角度を取得
    DirectX::XMFLOAT3 GetComponentEulerRotation() const
    {
        return GetComponentWorldTransform().GetEulerRotation();
    }
    // ワールド空間でのこのコンポーネントの座標を設定
    // （親がある場合は、親のワールド行列を使って相対位置に変換してセットする）
    virtual void SetWorldLocationDirect(const DirectX::XMFLOAT3& newWorldLocation)
    {
#if 0
        GetComponentWorldTransform().SetTranslation(newWorldLocation);
#else
        if (auto parent = attachParent_.lock())
        {
            // 親のワールドトランスフォームの逆行列を使って、相対位置を求める
#if 1
            DirectX::XMMATRIX parentWorld = parent->GetComponentWorldTransform().ToMatrix();
            DirectX::XMMATRIX parentInv = DirectX::XMMatrixInverse(nullptr, parentWorld);
            DirectX::XMVECTOR worldPos = DirectX::XMLoadFloat3(&newWorldLocation);
            DirectX::XMVECTOR localPos = DirectX::XMVector3TransformCoord(worldPos, parentInv);
            DirectX::XMFLOAT3 relative;
            DirectX::XMStoreFloat3(&relative, localPos);
            SetRelativeLocationDirect(relative);
#else
            parent->SetRelativeLocationDirect(newWorldLocation);
#endif
        }
        else
        {
            SetRelativeLocationDirect(newWorldLocation);
        }
#endif
    }
    // ワールド空間でのこのコンポーネントのクォータニオンを設定
    void SetWorldRotationDirect(const DirectX::XMFLOAT4& newWorldRotation)
    {
#if 0
        if (auto parent = attachParent_.lock())
        {
            parent->SetRelativeRotationDirect(newWorldRotation);
        }
        else
        {
            SetRelativeRotationDirect(newWorldRotation);
        }
#else
        using namespace DirectX;

        if (auto parent = attachParent_.lock())
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
#endif // 0
    }
    // ワールド空間でのこのコンポーネントの角度を設定
    void SetWorldEulerRotationDirect(const DirectX::XMFLOAT3& newWorldEuler)
    {
        if (auto parent = attachParent_.lock())
        {
            parent->SetRelativeEulerRotationDirect(newWorldEuler);
        }
        else
        {
            SetRelativeEulerRotationDirect(newWorldEuler);
        }
    }
    // ワールド空間でのこのコンポーネントのスケールを設定
    void SetWorldScaleDirect(const DirectX::XMFLOAT3& newWorldScale)
    {
        if (auto parent = attachParent_.lock())
        {
            parent->SetRelativeScaleDirect(newWorldScale);
        }
        else
        {
            SetRelativeScaleDirect(newWorldScale);
        }
    }

    // ワールド空間でのこのコンポーネントの Transform を直接設定
    void SetWorldMatrixDirect(const DirectX::XMFLOAT4X4& worldMatrix)
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
        //SetWorldScaleDirect(scl);
    }

    // 親からの相対的なこのコンポーネントの Transform を直接設定
    void SetRelativeMatrixDirect(const DirectX::XMFLOAT4X4& matrix)
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

        //SetRelativeLocationDirect(pos);
        SetRelativeRotationDirect(rot);
        //SetRelativeScaleDirect(scl);
    }


    // このコンポーネントが動いた時にコールバック(呼び出)される関数
    virtual void OnUpdateTransform(UpdateTransformFlags updateTransformFlags, TeleportType teleport = TeleportType::None)
    {
    }

    // コンポーネントの worldTransform の　update が　false だったら　worldTrasnformUpdate を呼ぶ
    void ConditionalUpdateComponentWorldTransform()
    {
        if (!componentToWorldTransformUpdate_)
        {
            UpdateComponentToWorld();
        }
    }

    // このコンポーネントにアタッチされている全ての子コンポーネントたちの Transform を更新する
    void UpdateChildTransforms(UpdateTransformFlags updateTransformFlags = UpdateTransformFlags::None, TeleportType teleport = TeleportType::None);

        // このコンポーネントの親からの相対的な Transform を返す
    Transform GetRelativeTransform() const
    {
        return Transform(relativeLocation_, relativeRotation_, relativeScale_);
    }


protected:

    // 指定されたソケットノードのワールド空間のTransformを返す
    // ソケットが見つからなかった場合は、自身の WorldTransform を返す
    virtual Transform GetSocketTransform(int socketNode) const
    {
        // TODO: // 自身のワールド空間のトランスフォーム

        return componentToWorld_;
    }

    // このコンポーネントの新しい　componentToWorld_Transformを計算する
    // parent は省略可能で、任意の sceneComponent を使って計算できる
    // 指定されない場合は、コンポーネント自身の　attachParent を使う
    Transform CalculateNewComponentToWorldTransform(const Transform& newRelativeTransform, const SceneComponent* parent = nullptr, int socketNode = -1)const
    {
        // socketNode と parent を指定されていなければ、アタッチされた情報から取得
        socketNode = parent ? socketNode : attachSocketNode_;
        parent = parent ? parent : attachParent_.lock().get();
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
        else
        {// 親が存在しない時は、自分の相対 Transform がそのまま WorldTransform になる
            return newRelativeTransform;
        }
    }

    // 親の Transform を使用して、絶対指定の要素を考慮した　componentToWorldTransform を計算する
    Transform CalculateNewComponentToWorldGeneralCase(const Transform& newRelativeTransform, const SceneComponent* parent, int socketNode)const
    {
        if (parent != nullptr)
        {
            const Transform parentToWorld = parent->GetSocketTransform(socketNode);
            Transform newComponentToWorldTransform = newRelativeTransform * parentToWorld;

            // 絶対位置が有効なら、位置だけは親の影響を無視
            if (absoluteLocation_)
            {
                newComponentToWorldTransform.translation_ = newRelativeTransform.translation_;
            }
            // 絶対回転が有効なら、回転だけは親の影響を無視
            if (absoluteRotation_)
            {
                newComponentToWorldTransform.rotation_ = newRelativeTransform.rotation_;
            }
            // 絶対スケールが有効なら、スケールだけは親の影響を無視
            if (absoluteScale_)
            {
                //newComponentToWorldTransform.scale_ = newRelativeTransform.scale_;
                // ただの代入ではなく、符号の補正を加えたスケール合成
                newComponentToWorldTransform.scale_ = DirectX::XMVectorMultiply(newRelativeTransform.scale_, MathHelper::VectorSign(newComponentToWorldTransform.scale_));
            }

            return newComponentToWorldTransform;
        }
        else
        {
            return newRelativeTransform;
        }
    }


    // このコンポーネントを現在の親のコンポーネントから切り離す
    void DetachFromParent();


private:
    //「Transformが変わった時や物理的に移動した時に、自分とその子たちを正しく更新するための中心的な処理」
    // 自身の Transform の変更を反映し、必要に応じて子コンポーネントにもそれを伝播させる関数
    void PropagateTransformUpdate(bool transformChanged, UpdateTransformFlags updateTransformFlags = UpdateTransformFlags::None, TeleportType teleport = TeleportType::None);

    // 親の Transform をもとに、自分の Transform（= ComponentToWorld）を更新し、必要に応じてその変更を子へ伝播する
    void UpdateComponentToWorldWithParent(SceneComponent* parent, int socketNode, UpdateTransformFlags updateTransformFlags, TeleportType teleport);


    //  指定された component が、このコンポーネントの子孫（子、孫など）であるかを判定する関数。
    // component が自分自身または子孫であれば true を返して、それ以外は false を返す
    bool IsAttachBelow(SceneComponent* component)
    {
        // 自分自身と一致する場合は true（＝component は自分、よって子孫関係にある）
        if (component == this)
        {
            return true;
        }

        // 子コンポーネントそれぞれに対して、再帰的に子孫関係をチェック
        for (const std::shared_ptr<SceneComponent>& child : attachChildren_)
        {
            if (child->IsAttachBelow(component))   // 再帰的チェック
            {
                return true;
            }
        }

        // どの子孫にも該当しなければ false
        return false;
    }

    // 指定された component が、このコンポーネントの祖先（親、祖父母など）であるかを判定する関数。
    // component が祖先であれば true を返し、それ以外は false を返す。
    bool IsAttachAbove(const SceneComponent* component) const
    {
        // 親から辿っていって、指定された component に一致するかをチェック
        for (SceneComponent* parent = attachParent_.lock().get(); parent; parent = parent->attachParent_.lock().get())
        {
            if (parent == component)
            {
                return true;
            }
        }

        // どの親にも一致しなければ false
        return false;
    }

    friend class Actor;

public:
    virtual void UpdateComponentToWorld(UpdateTransformFlags updateTransformFlags = UpdateTransformFlags::None, TeleportType teleport = TeleportType::None) override final;

    void Tick(float deltaTime) override {}

    virtual void Initialize()override {};

    virtual void OnRegister()override {} // 派生クラスで override して登録処理を書く

    virtual void OnUnregister()override {} // 派生クラスで override して解除処理を書く


    // 親子関係セット
    void AttachTo(const std::shared_ptr<SceneComponent>& parent)
    {
        attachParent_ = parent;
        parent->attachChildren_.push_back(shared_from_this());
        // もう一度確認
    }

    // このコンポーネントを、指定された親コンポーネントにアタッチ（接続）する
// parent はアタッチ先の親コンポーネント　
// socketNode 接続先のソケットノード番号( -1 ならデフォルト)
    void AttachToComponent(const std::shared_ptr<SceneComponent>& parent, int socketNode);


    void AddWorldOffset(const DirectX::XMFLOAT3& offset);

    bool isDirty = true;

    // ワールド空間でのこのコンポーネントの回転をオフセット分だけ加算する
    void AddLocalRotation(const DirectX::XMFLOAT3& offset);


};


#endif  //SCENE_COMPONENT_H