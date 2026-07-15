#include "pch.h"
#include "IceFragmentEffectActor.h"

void IceFragmentEmitterActor::Initialize(const Transform& transform)
{
    rotationComponent = AddComponent<RotationComponent>(parentName);

    // 結晶を生成する関数
    for (int i = 0; i < 15; i++)
    {
        Fragment fragment;
        fragment.meshComponent = AddComponent<InstanceMeshComponent>("Fragment_" + std::to_string(i), parentName);
        fragment.meshComponent->SetModel("./Data/Models/ParticleMesh/Fragment/SM_Aurora_Fragment.gltf");
        fragment.meshComponent->SetIsVisible(false);
        fragment.meshComponent->plusAlphaCBuffer->data.cpuColor = { 1,0,0,1 };
        PipeLineStateDesc pipeLineState;
        HRESULT hr = CreatePsFromCSO(Graphics::GetDevice(), "./Data/Shaders/InstanceModelIceEffectPS.cso", pipeLineState.pixelShader.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        fragment.meshComponent->SetPipeLineState(pipeLineState);
        fragments.push_back(fragment);
    }
}

void IceFragmentEmitterActor::Update(float deltaTime)
{
    for (auto& fragment : fragments)
    {
        // 移動
        auto pos = fragment.meshComponent->GetComponentLocation();
        pos.x += fragment.velocity.x * deltaTime;
        pos.y += fragment.velocity.y * deltaTime;
        pos.z += fragment.velocity.z * deltaTime;
        fragment.meshComponent->SetWorldLocationDirect(pos);

        // 回転
        //auto rot = fragment.meshComponent->GetComponentEulerRotation();
        //rot.x += fragment.rotationSpeed.x * deltaTime;
        //rot.y += fragment.rotationSpeed.y * deltaTime;
        //rot.z += fragment.rotationSpeed.z * deltaTime;
        //fragment.meshComponent->SetWorldEulerRotationDirect(rot);

        // 重力
        fragment.velocity.y -= gravity * deltaTime;

        // 寿命
        fragment.life -= deltaTime;
        float alpha = fragment.life / lifeTime;
        if (alpha <= 0.0f)
        {
            fragment.meshComponent->SetIsVisible(false);
        }
    }
}

void IceFragmentEmitterActor::DrawImGuiDetails()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("Spread Angle", &spreadAngle, 1.0f, 0.0f, 90.0f);
    ImGui::DragFloat("Speed Min", &speedMin, 0.1f);
    ImGui::DragFloat("Speed Max", &speedMax, 0.1f);
    ImGui::DragFloat("Gravity", &gravity, 0.5f);
    ImGui::DragFloat("Life", &lifeTime, 0.01f, 0.1f, 5.0f);

    if (ImGui::Button(U8("エフェクト生成")))
    {
        SetDirection(GetPosition(), { 0,0,1 });
    }
#endif
}

// 飛ぶ方向を決定する
void IceFragmentEmitterActor::SetDirection(DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal)
{
    for (auto& fragment : fragments)
    {
        fragment.life = 0.5f;
        fragment.meshComponent->SetIsVisible(true);
        fragment.meshComponent->SetWorldLocationDirect(hitPos);
        DirectX::XMFLOAT3 forward = MathHelper::Normalize(hitNormal);
        DirectX::XMFLOAT3 worldUp = { 0,1,0 };
        DirectX::XMFLOAT3 right = MathHelper::Normalize(MathHelper::Cross(worldUp, forward));
        DirectX::XMFLOAT3 up = MathHelper::Cross(forward, right);
        float angle = DirectX::XMConvertToRadians(spreadAngle);
        float x = MathHelper::RandomRange(-tanf(angle), tanf(angle));
        float y = MathHelper::RandomRange(-tanf(angle), tanf(angle));
        DirectX::XMFLOAT3 dir = forward;
        dir = MathHelper::Add(dir, MathHelper::Multiply(right, x));
        dir = MathHelper::Add(dir, MathHelper::Multiply(up, y));
        dir = MathHelper::Normalize(dir);
        float speed = MathHelper::RandomRange(speedMin, speedMax);
        fragment.velocity = MathHelper::Multiply(dir, speed);
        DirectX::XMFLOAT4 rotation=MathHelper::LookRotation(dir, { 0,1,0 });
        fragment.meshComponent->SetRelativeRotationDirect(rotation);
    }

    //if (rotationComponent)
    //{
    //    rotationComponent->SetDirection(hitNormal);
    //}
}