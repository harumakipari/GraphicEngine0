#include "pch.h"
#include "ElasticComponent.h"
#include "Core/Actor.h"
#include "Engine/Camera/CameraConstants.h"
#include "Engine/Debug/DebugRender.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Utility/Time.h"
#include "Game/Actors/Camera/Camera.h"
#include "Physics/CollisionFunction.h"

void ElasticMeshComponent::Initialize()
{
    auto actor = owner_.lock();
    DirectX::XMFLOAT3 position = actor->GetPosition();
    //DirectX::XMFLOAT3 position = GetRelativeLocation();
    modelHeight = model->GetModelSize().y * actor->GetScale().y;

    // 定数バッファの作成
    elasticBuildingCBuffer = std::make_unique<ConstantBuffer<ElasticConstants>>(Graphics::GetDevice());
    elasticConstants =
    {
        /*p1*/ DirectX::XMFLOAT4(position.x, position.y, position.z, 1.0f),
        /*p2*/ DirectX::XMFLOAT4(position.x,position.y, position.z, 1.0f),
        /*p3*/ DirectX::XMFLOAT4(position.x, position.y + modelHeight, position.z, 1.0f),
        /*maxAngleDegree*/ 100.0f, // 度以上は曲がらない
        /*modelHeight*/ modelHeight,
    };
    elasticBuildingCBuffer->data = elasticConstants;

    p3Current = {
    position.x,
    position.y + modelHeight,
    position.z
    };
    p3Target = p3Current;
}

void ElasticMeshComponent::Tick(float deltaTime)
{
    DirectX::XMFLOAT3 position = owner_.lock()->GetPosition();

    p3Base = {
position.x,
position.y + modelHeight,
position.z
    };

    UpdatePushElastic(deltaTime);
}

void ElasticMeshComponent::UpdatePushElastic(float deltaTime)
{
    DirectX::XMFLOAT3 position = owner_.lock()->GetPosition();


    if (!elasticEnabled)
    {
        // Elastic OFF → まっすぐ元の形に固定
        p3Current = p3Base;

        float midY = position.y + modelHeight * 0.5f;

        elasticConstants.p1 = { position.x,position.y,position.z,1.0f };
        elasticConstants.p2 = { position.x,midY,position.z ,1.0f };
        elasticConstants.p3 = { p3Base.x,p3Base.y,p3Base.z,1.0f };
        return;
    }

    using namespace DirectX;
    ClearForce();
    bool hasExternalForce = false;
    if (useMouseInput)
    {
        hasExternalForce = UpdateFromMouse(deltaTime);
    }
    p3Target = p3Base;
    p3Target.x += mouseForce.x;
    p3Target.y += mouseForce.y;
    p3Target.z += mouseForce.z;
    p3Target.x += cherryForce.x;
    p3Target.y += cherryForce.y;
    p3Target.z += cherryForce.z;

    //p3Current = p3Target;

    //if (!hasExternalForce)
    {// 左ボタンを押していない時
        pullInfo.active = false;

#if 0
        float targetX = position.x;
        float targetY = position.y + modelHeight;
        float targetZ = position.z;
#else
        float targetX = p3Target.x;
        float targetY = p3Target.y;
        float targetZ = p3Target.z;
#endif // 1

        float gradX = p3Current.x - targetX;// x - a
        float gradY = p3Current.y - targetY;// x - a
        float gradZ = p3Current.z - targetZ;// x - a


        elasticParameters.momentumX = elasticParameters.damping * elasticParameters.momentumX + gradX/*parmator*/;
        elasticParameters.momentumY = elasticParameters.damping * elasticParameters.momentumY + gradY/*parmator*/;
        elasticParameters.momentumZ = elasticParameters.damping * elasticParameters.momentumZ + gradZ/*parmator*/;
#if 0
        p3Current.x -= Time::UnscaledDeltaTime() * elasticParameters.stiffness * elasticParameters.momentumX;
        p3Current.y -= Time::UnscaledDeltaTime() * elasticParameters.stiffness * elasticParameters.momentumY;
        p3Current.z -= Time::UnscaledDeltaTime() * elasticParameters.stiffness * elasticParameters.momentumZ;
#else
        float dt = Time::UnscaledDeltaTime();

        // バネ（Hookeっぽい）
        float k = elasticParameters.stiffness;   // 硬さ
        float d = elasticParameters.damping;     // 減衰

        // 目標との差
        DirectX::XMFLOAT3 diff;
        diff.x = p3Current.x - p3Target.x;
        diff.y = (p3Current.y - p3Target.y) * 0.2f; // ←縦弱め
        diff.z = p3Current.z - p3Target.z;

        // 加速度 = -kx - dv
        velocity.x += (-k * diff.x - d * velocity.x) * dt;
        velocity.y += (-k * diff.y - d * velocity.y) * dt;
        velocity.z += (-k * diff.z - d * velocity.z) * dt;

        // 位置更新
        p3Current.x += velocity.x * dt;
        p3Current.y += velocity.y * dt;
        p3Current.z += velocity.z * dt;
#endif // 0

    }
    elasticConstants.maxAngleDegree = elasticParameters.maxAngleDegrees;

    float midY = position.y + modelHeight * 0.5f;
    elasticConstants.p1 = { position.x,position.y,position.z,1.0f };
    elasticConstants.p2 = { position.x,midY,position.z ,1.0f };
    elasticConstants.p3 = { p3Current.x,p3Current.y,p3Current.z,1.0f };


    DebugRender::DrawSphere({ elasticConstants.p1.x,elasticConstants.p1.y,elasticConstants.p1.z }, 0.03f, { 1, 0, 0, 1 });
    DebugRender::DrawSphere({ elasticConstants.p2.x,elasticConstants.p2.y,elasticConstants.p2.z }, 0.03f, { 0, 1, 0, 1 });
    DebugRender::DrawSphere({ elasticConstants.p3.x,elasticConstants.p3.y,elasticConstants.p3.z }, 0.03f, { 0, 0, 1, 1 });
}

bool ElasticMeshComponent::UpdateFromMouse(float deltaTime)
{
    //DirectX::XMFLOAT3 position = owner_.lock()->GetPosition();
    DirectX::XMFLOAT3 position = GetRelativeLocation();

    // ポーズ中はゲーム入力を一切受け付けない
    if (Scene::GetCurrentScene()->IsPaused())
    {
        return false;
    }


    // UIがマウスを使っているならゲーム操作しない
    if (Scene::GetCurrentScene()->GetUIManager()->IsMouseCaptured())
    {
        return false;
    }

    // マウスカーソルを取得
    if (InputSystem::GetInputState("MouseLeft"))
    {// 左ボタンを押している間
        DirectX::XMFLOAT2 cursor;
        if (!InputSystem::GetMousePositionUI(cursor))
        {
            return false;
        }

        HitResultWithActor result;
        XMFLOAT3 intersectPos;
        XMFLOAT3 buildCurveDir;
        if (CollisionFunction::RaycastFromMouse(cursor, result, CollisionHelper::ToBit(CollisionLayer::WorldStatic)))
        {
            XMFLOAT3 intersectNormal;
            intersectPos = result.hitPoint;
            intersectNormal = result.normal;
            buildCurveDir = { intersectPos.x - position.x,0.0f,intersectPos.z - position.z };
        }
        else
        {
            intersectPos = { 0.0f,0.0f,0.0f };
            buildCurveDir = { 0.0f,0.0f,0.0f };
        }
        DebugRender::DrawSphere(result.hitPoint, 1.03f, { 1, 1, 0, 1 });

        // これでマウスのpositionによって、建物を曲げる方向を見つける
        XMVECTOR BuildCurveDir = XMLoadFloat3(&buildCurveDir);
        BuildCurveDir = XMVector3Normalize(BuildCurveDir);
        BuildCurveDir = XMVectorScale(BuildCurveDir, modelHeight);
        XMStoreFloat3(&buildCurveDir, BuildCurveDir);

        //　建物の一番上からのマウスの移動量によって、建物を曲げる量を決める
        // ワールド座標からスクリーン座標へ変換
        DirectX::XMFLOAT3 buildTop = { position.x,position.y + modelHeight,position.z };
        DirectX::XMVECTOR WorldPosition;
        WorldPosition = DirectX::XMLoadFloat3(&buildTop);
        // スクリーン座標
        DirectX::XMFLOAT2 screenPosition = WorldToUI(buildTop);

        // とりあえずｘだけの移動量
        float moveAmount = cursor.x - screenPosition.x;


        // 建物中心と交点の差ベクトル (水平)
        DirectX::XMFLOAT3 dirXZ = {
            intersectPos.x - position.x,
            0.0f,
            intersectPos.z - position.z
        };
        float angle = 0.0f;
        DirectX::XMVECTOR dirVec = DirectX::XMLoadFloat3(&dirXZ);
        if (DirectX::XMVector3LengthSq(dirVec).m128_f32[0] < 1e-6f)
        {
            // マウスが真上＝曲げ方向なし
            angle = 0.0f;
        }
        else
        {
            // atan2(Z,X) で角度をラジアン取得
            angle = atan2f(dirXZ.x, dirXZ.z);
        }

        // 交点ベクトルを正規化＋距離クランプ
        DirectX::XMVECTOR basePos = DirectX::XMLoadFloat3(&position);
        DirectX::XMVECTOR intersect = DirectX::XMLoadFloat3(&intersectPos);
        DirectX::XMVECTOR diff = intersect - basePos;
        diff = DirectX::XMVectorSetY(diff, 0.0f); // 高さは無視して水平方向だけのベクトルにする

        float dist = DirectX::XMVectorGetX(DirectX::XMVector3Length(diff));
        float maxDist = elasticParameters.maxDist; // 好みで調整
        float scale = (dist > maxDist) ? (maxDist / dist) : 1.0f;
        DirectX::XMVECTOR clampedDir = diff * scale;
        //DirectX::XMVECTOR clampedDir = diff * moveAmount; // mouse の変化量 を使う場合

        // p3 = 建物の上端＋方向ベクトル
        DirectX::XMStoreFloat3(&p3Current, basePos + DirectX::XMVectorSet(0, modelHeight, 0, 0) + clampedDir);

        XMStoreFloat3(&mouseForce, clampedDir);

        pullInfo.active = true;
        pullInfo.radianAngle = angle;
        pullInfo.amount = std::clamp(dist / maxDist, 0.0f, 1.0f);
        return true;
    }
    return false;
}


void ElasticMeshComponent::AddImpulse(DirectX::XMFLOAT3 impulse)
{
    // 重さ × 重力方向
    DirectX::XMFLOAT3 position = owner_.lock()->GetPosition();
    XMFLOAT3 down = { impulse };
    down = { 0.0f, -0.5f, 0.0f };
    cherryForce = down;
    float power = 2.0f;
    velocity.x += (rand() % 100 / 100.0f - 0.5f) * power;
    velocity.y += 0.0f; // 縦は抑える
    velocity.z += (rand() % 100 / 100.0f - 0.5f) * power;

    p3Target = {
        p3Base.x + down.x,
        p3Base.y + down.y,
        p3Base.z + down.z
    };
    p3Current = {
        p3Base.x + down.x,
        p3Base.y + down.y,
        p3Base.z + down.z
    };
}


void ElasticMeshComponent::SetElasticEnabled(const bool enabled)
{
    elasticEnabled = enabled;

    if (!enabled)
    {
        // OFFにした瞬間に余計な運動量を消す
        elasticParameters.momentumX = 0.0f;
        elasticParameters.momentumY = 0.0f;
        elasticParameters.momentumZ = 0.0f;
    }
}

// サクランボのためにプリンの表面の位置を取得する関数
void  ElasticMeshComponent::GetSurfacePositionTangent(DirectX::XMFLOAT3& surfacePosition, DirectX::XMFLOAT3& tangent)
{
    XMVECTOR p2 = XMLoadFloat3((XMFLOAT3*)&elasticConstants.p2);
    XMVECTOR p3 = XMLoadFloat3((XMFLOAT3*)&elasticConstants.p3);

    XMVECTOR Tangent = XMVector3Normalize(2.0f * (p3 - p2)); // 接線ベクトル t=1.0fだから

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR normal = XMVector3Normalize(XMVector3Cross(Tangent, XMVector3Cross(up, Tangent)));
    // プリンの半径
    XMVECTOR surfacePos = p3 + Tangent * cherryOffset;
    DirectX::XMStoreFloat3(&surfacePosition, surfacePos);

    // 表面
    DebugRender::DrawSphere(surfacePosition, 0.03f, { 1,1,0,1 });
    // 接線
    XMVECTOR Debug = XMVectorAdd(p3, Tangent);
    XMFLOAT3 debugPos;
    XMStoreFloat3(&debugPos, Debug);
    DebugRender::DrawLine({ elasticConstants.p3.x,elasticConstants.p3.y,elasticConstants.p3.z },
        debugPos, { 1,0,1,1 });

    XMStoreFloat3(&tangent, Tangent);
}


void ElasticMeshComponent::UpdateConstantBuffer(ID3D11DeviceContext* immediateContext) const
{
    elasticBuildingCBuffer->data = elasticConstants;
    elasticBuildingCBuffer->Activate(immediateContext, 6);
}

void ElasticPullController::Tick(float dt)
{
    if (!elasticMesh) return;
    if (!InputSystem::GetInputState("MouseLeft")) return;

    DirectX::XMFLOAT2 cursor;
    if (!InputSystem::GetMousePositionUI(cursor)) return;

    HitResultWithActor result;
    if (!CollisionFunction::RaycastFromMouse(
        cursor,
        result,
        CollisionHelper::ToBit(CollisionLayer::WorldStatic)))
        return;

    // ===== 引っ張り方向 =====
    DirectX::XMFLOAT3 meshPos = elasticMesh->GetOwner()->GetPosition();
    DirectX::XMFLOAT3 dir = {
        result.hitPoint.x - meshPos.x,
        0.0f,
        result.hitPoint.z - meshPos.z
    };

    DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&dir);
    float dist = XMVectorGetX(XMVector3Length(v));
    v = XMVector3Normalize(v);
    XMStoreFloat3(&dir, v);

    // ===== 力として送る =====
    ElasticForce f;
    f.point = result.hitPoint;
    f.dir = dir;
    f.strength = std::clamp(dist, 0.0f, 5.0f) * 30.0f;

    //elasticMesh->AddForce(f);
}
