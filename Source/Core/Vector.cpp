#include "pch.h"
#include "Vector.h"

float Vector3::Length() const
{
	DirectX::XMVECTOR v = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(this));
	DirectX::XMVECTOR length = DirectX::XMVector3Length(v);
	return DirectX::XMVectorGetX(length);
}

float Vector3::LengthSq() const
{
	DirectX::XMVECTOR v = XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(this));
	DirectX::XMVECTOR lengthSq = DirectX::XMVector3LengthSq(v);
	return DirectX::XMVectorGetX(lengthSq);
}

Vector3 Vector3::Normalize() const
{
    DirectX::XMVECTOR v = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(this));
	v = DirectX::XMVector3Normalize(v);
    DirectX::XMFLOAT3 result;
	DirectX::XMStoreFloat3(&result, v);
	return Vector3(result);
}

float Vector3::Dot(const Vector3& v) const
{
    DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(this));
    DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&v));
	DirectX::XMVECTOR dot = DirectX::XMVector3Dot(v1, v2);
	return DirectX::XMVectorGetX(dot);
}

Vector3 Vector3::Cross(const Vector3& v) const
{
	DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(this));
	DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&v));
	DirectX::XMVECTOR cross = DirectX::XMVector3Cross(v1, v2);
	DirectX::XMFLOAT3 result;
	DirectX::XMStoreFloat3(&result, cross);
	return Vector3(result);
}
