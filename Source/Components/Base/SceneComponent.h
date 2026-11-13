#pragma once

#include <memory>
#include <string>
#include <DirectXMath.h>

#include "Component.h"
#include "Components/Transform/Transform.h"

class Actor;

class SceneComponent :public Component, public std::enable_shared_from_this<SceneComponent>
{
public:
    SceneComponent(const std::string& name, const std::shared_ptr<Actor>& owner) :Component(name, owner) {}

    ~SceneComponent() override = default;

    void Initialize()override {}

    void Tick(float deltaTime) override {}

    void UpdateComponentToWorld(UpdateTransformFlags updateTransformFlags = UpdateTransformFlags::None) final;

    void Destroy() override;

    void OnRegister()override {}

    void OnUnregister()override {}

    void DrawImGuiInspector() override;

    // 新規Component用の簡易接続。既存の親から付け替える場合はAttachToComponentを使用する
    void AttachTo(const std::shared_ptr<SceneComponent>& parent)
    {
        attachParent = parent;
        parent->attachChildren.push_back(shared_from_this());
    }

    // 既存の親を解除して接続する。socketNodeが-1の場合はソケットを指定しない
    void AttachToComponent(const std::shared_ptr<SceneComponent>& parent, int socketNode);

    void AddWorldOffset(const DirectX::XMFLOAT3& offset) const;

    // ローカル回転にオフセットを加える
    void AddLocalRotation(const DirectX::XMFLOAT3& offset);

    Transform GetFinalWorldTransform() const
    {
        return componentToWorld;
    }

    std::shared_ptr<SceneComponent> GetAttachParent() const
    {
        return attachParent.lock();
    }

    int GetAttachSocketNode() const
    {
        return attachSocketNode;
    }

    const std::vector<std::shared_ptr<SceneComponent>>& GetAttachChildren() const
    {
        return attachChildren;
    }

    bool IsUsingAbsoluteLocation() const { return absoluteLocation; }

    void SetUsingAbsoluteLocation(const bool absoluteLocation) { this->absoluteLocation = absoluteLocation ? 1 : 0; }

    bool IsUsingAbsoluteRotation() const { return absoluteRotation; }

    void SetUsingAbsoluteRotation(const bool absoluteRotation) { this->absoluteRotation = absoluteRotation ? 1 : 0; }

    bool IsUsingAbsoluteScale() const { return absoluteScale; }

    void SetUsingAbsoluteScale(const bool absoluteScale) { this->absoluteScale = absoluteScale ? 1 : 0; }

    // 自身と子コンポーネントのワールドTransformを前フレーム用に保存する
    void CapturePreviousTransform();

    DirectX::XMFLOAT3 GetRelativeLocation() const { return relativeLocation; }
    void SetRelativeLocationDirect(const DirectX::XMFLOAT3& newRelativeLocation) { relativeLocation = newRelativeLocation; }
    DirectX::XMFLOAT3 GetRelativeScale()const { return relativeScale; }
    void SetRelativeScaleDirect(const DirectX::XMFLOAT3& newRelativeScale) { relativeScale = newRelativeScale; }
    // 相対回転を度数法のEuler角で返す
    DirectX::XMFLOAT3 GetRelativeEulerRotation()const
    {
        DirectX::XMFLOAT3 angle = MathHelper::QuaternionToEuler(relativeRotation);
        return { DirectX::XMConvertToDegrees(angle.x),DirectX::XMConvertToDegrees(angle.y) ,DirectX::XMConvertToDegrees(angle.z) };
    }

    DirectX::XMFLOAT4 QuaternionFromEulerYXZ(const DirectX::XMFLOAT3& eulerRadians);

    // 度数法のEuler角を相対回転に設定する
    void SetRelativeEulerRotationDirect(const DirectX::XMFLOAT3& newEulerRotation)
    {
        DirectX::XMStoreFloat4(&relativeRotation, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(newEulerRotation.x), DirectX::XMConvertToRadians(newEulerRotation.y), DirectX::XMConvertToRadians(newEulerRotation.z)));
    }
    const DirectX::XMFLOAT4& GetRelativeRotation() const { return relativeRotation; }
    void SetRelativeRotationDirect(const DirectX::XMFLOAT4& newRelativeRotation) { relativeRotation = newRelativeRotation; }
    _NODISCARD const Transform& GetComponentWorldTransform() const { return componentToWorld; }
    _NODISCARD Transform& GetComponentWorldTransform() { return componentToWorld; }
    _NODISCARD const Transform& GetPreviousComponentWorldTransform() const { return previousComponentToWorld; }
    _NODISCARD Transform& GetPreviousComponentWorldTransform() { return previousComponentToWorld; }
    DirectX::XMFLOAT3 GetComponentLocation() const
    {
        return GetComponentWorldTransform().GetLocation();
    }
    DirectX::XMFLOAT4 GetComponentRotation() const
    {
        return GetComponentWorldTransform().GetRotation();
    }
    DirectX::XMFLOAT3 GetComponentScale() const
    {
        return GetComponentWorldTransform().GetScale();
    }
    DirectX::XMFLOAT3 GetComponentEulerRotation() const
    {
        return GetComponentWorldTransform().GetEulerRotation();
    }
    // 親がある場合は、ワールド座標を親からの相対座標に変換して設定する
    virtual void SetWorldLocationDirect(const DirectX::XMFLOAT3& newWorldLocation);

    void SetWorldRotationDirect(const DirectX::XMFLOAT4& newWorldRotation);

    void SetWorldEulerRotationDirect(const DirectX::XMFLOAT3& newWorldEuler);
    void SetWorldScaleDirect(const DirectX::XMFLOAT3& newWorldScale);

    void SetWorldMatrixDirect(const DirectX::XMFLOAT4X4& worldMatrix);

    void SetRelativeMatrixDirect(const DirectX::XMFLOAT4X4& matrix);

    // ワールドTransformが未更新の場合に更新する
    void ConditionalUpdateComponentWorldTransform();

    void UpdateChildTransforms(UpdateTransformFlags updateTransformFlags = UpdateTransformFlags::None) const;

    // このコンポーネントの親からの相対的な Transform を返す
    Transform GetRelativeTransform() const
    {
        return Transform(relativeLocation, relativeRotation, relativeScale);
    }


protected:
    // 派生クラスでソケットのTransformを取得する。基底クラスでは自身のワールドTransformを返す
    virtual Transform GetSocketTransform(int socketNode) const
    {
        return componentToWorld;
    }

    // 親を指定しない場合は、現在の接続先を使ってワールドTransformを計算する
    Transform CalculateNewComponentToWorldTransform(const Transform& newRelativeTransform, const SceneComponent* parent = nullptr, int socketNode = -1)const;

    // 位置・回転・スケールの絶対指定を考慮してワールドTransformを計算する
    Transform CalculateNewComponentToWorldGeneralCase(const Transform& newRelativeTransform, const SceneComponent* parent, const int socketNode)const;

    // このコンポーネントを現在の親のコンポーネントから切り離す
    void DetachFromParent();
private:
    // Transformの変更状態と更新フラグに応じて、子コンポーネントへ更新を伝播する
    void PropagateTransformUpdate(bool transformChanged, UpdateTransformFlags updateTransformFlags = UpdateTransformFlags::None) const;

    // 指定された親とソケットを使ってワールドTransformを更新し、必要に応じて子へ伝播する
    void UpdateComponentToWorldWithParent(SceneComponent* parent, int socketNode, UpdateTransformFlags updateTransformFlags);

    // 指定されたコンポーネントが自身の祖先にあたるか判定する
    bool IsAttachAbove(const SceneComponent* component) const;

protected:
    DirectX::XMFLOAT3 inspectorEuler = { 0,0,0 };

    Transform componentToWorld;     // 親子関係を反映したワールドTransformのキャッシュ

    Transform previousComponentToWorld; // 前フレームのワールドTransform。モーションベクトル等で使用する

    // 親コンポーネントを弱参照で保持する
    std::weak_ptr<SceneComponent> attachParent; 

    // 接続先のソケット番号。-1はソケット指定なし
    int attachSocketNode = -1;

    // 子コンポーネントを共有参照で保持する
    std::vector<std::shared_ptr<SceneComponent>> attachChildren; 

private:
    DirectX::XMFLOAT3 relativeLocation = { 0.0f,0.0f,0.0f };
    DirectX::XMFLOAT4 relativeRotation = { 0.0f,0.0f,0.0f,1.0f };
    DirectX::XMFLOAT3 relativeScale = { 1.0f,1.0f,1.0f };

    // ワールドTransformが初期化・更新済みかを示す
    uint8_t componentToWorldTransformUpdate : 1 = 0; 

    // trueの場合は親のTransformではなく、ワールド座標系を基準に扱う
    uint8_t absoluteLocation : 1 = 0;
    uint8_t absoluteRotation : 1 = 0;
    uint8_t absoluteScale : 1 = 0;

    friend class Actor;
};

