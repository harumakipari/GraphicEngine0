#include "pch.h"
#include "Cherry.h"

void Cherry::Launch(const XMFLOAT3& startPos, const XMFLOAT3& velocity)
{
    SetPosition(startPos);
    velocity_ = velocity;
    isFlying_ = true;
}

void Cherry::Update(float deltaTime)
{
    if (isFlying_)
    {
        // 速度に重力を加算
        velocity_.y += gravity_ * deltaTime;
        // 位置を更新
        XMFLOAT3 position = GetPosition();
        position.x += velocity_.x * deltaTime;
        position.y += velocity_.y * deltaTime;
        position.z += velocity_.z * deltaTime;
        SetPosition(position);
        // 地面に到達したら停止
        if (position.y <= 0.0f)
        {
            position.y = 0.0f;
            SetPosition(position);
            isFlying_ = false;
            // エフェクトを再生
            if (particleComponent)
            {
                particleComponent->Play();
            }
        }

    }
    else
    {
        if (!particleComponent->IsPlaying())
        {
            MarkPendingKill();
        }
    }
}