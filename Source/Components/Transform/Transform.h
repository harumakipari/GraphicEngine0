#pragma once

#include <DirectXMath.h>
#include "Math/MathHelper.h"

struct alignas(16) Transform
{
    DirectX::XMVECTOR translation_;
    DirectX::XMVECTOR rotation_;	// ÉNÉHÅ[É^ÉjÉIÉì
    DirectX::XMVECTOR scale_;

    Transform() : translation_(DirectX::XMVectorZero()), rotation_(DirectX::XMQuaternionIdentity()), scale_(DirectX::XMVectorSet(1, 1, 1, 0))
    {
    }
    Transform(const DirectX::XMFLOAT3 translation, const DirectX::XMFLOAT3 euler_rotation, const DirectX::XMFLOAT3 scale) :
        translation_(DirectX::XMLoadFloat3(&translation)),
        rotation_(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(euler_rotation.x), DirectX::XMConvertToRadians( euler_rotation.y), DirectX::XMConvertToRadians(euler_rotation.z))),
        scale_(DirectX::XMLoadFloat3(&scale))
    {
    }
    Transform(const DirectX::XMFLOAT3 translation, const DirectX::XMFLOAT4 rotation/*quaternion*/, const DirectX::XMFLOAT3 scale) :
        translation_(DirectX::XMLoadFloat3(&translation)),
        rotation_(DirectX::XMLoadFloat4(&rotation)),
        scale_(DirectX::XMLoadFloat3(&scale))
    {
    }
    Transform(const DirectX::XMVECTOR translation, const DirectX::XMVECTOR rotation/*quaternion*/, const DirectX::XMVECTOR scale) :
        translation_(translation),
        rotation_(rotation),
        scale_(scale)
    {
    }
    Transform(DirectX::XMMATRIX matrix)
    {
        DirectX::XMMatrixDecompose(&scale_, &rotation_, &translation_, matrix);
    }

    DirectX::XMFLOAT3 GetTranslation() const
    {
        DirectX::XMFLOAT3 translation;
        DirectX::XMStoreFloat3(&translation, translation_);
        return translation;
    }
    DirectX::XMFLOAT3 GetLocation() const
    {
        return GetTranslation();
    }
    void SetTranslation(const DirectX::XMFLOAT3 translation)
    {
        translation_ = DirectX::XMLoadFloat3(&translation);
    }
    DirectX::XMFLOAT3 GetScale() const
    {
        DirectX::XMFLOAT3 scale;
        DirectX::XMStoreFloat3(&scale, scale_);
        return scale;
    }
    void SetScale(const DirectX::XMFLOAT3 scale)
    {
        scale_ = DirectX::XMLoadFloat3(&scale);
    }

    DirectX::XMFLOAT4 GetRotation() const
    {
        DirectX::XMFLOAT4 rotation;
        DirectX::XMStoreFloat4(&rotation, rotation_);
        return rotation;
    }
    void SetRotation(const DirectX::XMFLOAT4 rotation) 
    {
        rotation_ = DirectX::XMLoadFloat4(&rotation);
    }
    DirectX::XMFLOAT3 GetEulerRotation() const  
    {
        DirectX::XMFLOAT4 rotationQuaternion;
        DirectX::XMStoreFloat4(&rotationQuaternion, rotation_);
        return MathHelper::QuaternionToEuler(rotationQuaternion);
    }
    void SetEularRotation(const DirectX::XMFLOAT3& eulerRotation)
    {
        rotation_ = DirectX::XMQuaternionRotationRollPitchYaw(eulerRotation.x, eulerRotation.y, eulerRotation.z);
    }

    // ílÇ…NanÇ™ì¸Ç¡ÇƒÇ¢Ç»Ç¢Ç©ämîF
    bool ContainsNanOrInfinite() const
    {
        return MathHelper::VectorContainsNanOrInfinite(translation_) || MathHelper::VectorContainsNanOrInfinite(scale_) || MathHelper::VectorContainsNanOrInfinite(rotation_);
    }

    // transformÅ@Ç matrix Ç…ïœä∑
    DirectX::XMMATRIX ToMatrix() const
    {
        _ASSERT_EXPR(!ContainsNanOrInfinite(), L"Invalid transform : one or more components (translation, rotation, or scale) contain NaN.");
        return DirectX::XMMatrixScalingFromVector(scale_) * DirectX::XMMatrixRotationQuaternion(rotation_) * DirectX::XMMatrixTranslationFromVector(translation_);
    }

    // transformÅ@Ç worldTransform Ç…ïœä∑
    DirectX::XMFLOAT4X4 ToWorldTransform() const 
    {
        DirectX::XMFLOAT4X4 worldTransform{};
        _ASSERT_EXPR(!ContainsNanOrInfinite(), L"Invalid transform : one or more components (translation, rotation, or scale) contain NaN.");
        DirectX::XMStoreFloat4x4(&worldTransform, DirectX::XMMatrixScalingFromVector(scale_) * DirectX::XMMatrixRotationQuaternion(rotation_) * DirectX::XMMatrixTranslationFromVector(translation_));
        return worldTransform;

    }

    // Transform ìØémÇä|ÇØéZÇ∑ÇÈ
    Transform operator*(const Transform& rhs) const
    {
        Transform composed;
        // épê®ÇÃä|ÇØéZÇÅ@transformÇ…äiî[Ç∑ÇÈ
        DirectX::XMMatrixDecompose(&composed.scale_, &composed.rotation_, &composed.translation_, ToMatrix() * rhs.ToMatrix());
        return composed;
    }

    // ëºÇÃTransformÇ∆î‰är
    bool Equals(const Transform& other, const float tolerance) const
    {
        DirectX::XMVECTOR epsilon = DirectX::XMVectorReplicate(tolerance);
        return DirectX::XMVector3NearEqual(translation_, other.translation_, epsilon)
            && DirectX::XMVector4NearEqual(rotation_, other.rotation_, epsilon)
            && DirectX::XMVector3NearEqual(scale_, other.scale_, epsilon);
    }

};

