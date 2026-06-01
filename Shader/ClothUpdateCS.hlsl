struct ClothVertex
{
    float3 position;
    float3 normal;
    float4 tangent;
    float2 texcoord;
    float3 velocity;
    float3 oldPosition;
    float3 oldVelocity;
    float4 rotation;
    uint isPinned;
};

struct ClothEdge
{
    uint neighbor; // 隣接頂点のインデックス
    float3 delta; // 初期の相対ベクトル
    float restLength; // 初期距離
};

cbuffer PRIMITIVE_CONSTANT_BUFFER : register(b0)
{
    row_major float4x4 world;
    
    int material;
    bool hasTangent;
    int skin;
    int padding;
    
    row_major float4x4 invWorld;
}



StructuredBuffer<ClothVertex> inVertices : register(t0);
StructuredBuffer<ClothEdge> clothEdgeSRV : register(t1);
RWStructuredBuffer<ClothVertex> outVertices : register(u0);

//#define CLOTH_EDGES_PER_VERTEX 4
#define MAX_EDGES 8
#define SPRING_FORCE 20000.0
//#define MASS 0.5
#define MASS 0.5
#define GRAVITY 0.98
#define DAMPING 0.04

// 定数
#define SPRING_STIFFNESS 40.0   // ばね定数 40-200
#define SPRING_DAMPING   25.0     // ばね粘性（相対速度に対する減衰）
#define GLOBAL_DAMPING   0.05     // 全体指数減衰

cbuffer SCENE_CONSTANT_BUFFER : register(b1)
{
    float elapsedTime;
    float deltaTime;
    float gravity;
}
cbuffer VIEW_CONSTANTS_BUFFER : register(b4)
{
    row_major float4x4 viewProjection;
    float4 cameraPositon;
    row_major float4x4 view;
    row_major float4x4 projection;
    row_major float4x4 inverseProjection;
    row_major float4x4 inverseViewProjection;
    row_major float4x4 invView;
}

cbuffer CLOTH_SIMULATE_CBUFFER : register(b10)
{
    float g;
    uint vertexCount;
    float windPhaseOffset;
    float windBase;

    float3 windEmitPosition;
};

cbuffer SPHERE_CBUFFER : register(b7)
{
    float3 worldPos;
    float radius;
    row_major float4x4 sphereWorld;
    row_major float4x4 sphereInvWorld;
}

void CollideSphere(inout ClothVertex v)
{
 // 球のワールド情報
    float3 sphereCenterWorld = worldPos;
    float sphereRadiusWorld = radius;
    float friction = 0.4;
    float restitution = 0.1;

    // ワールド→ローカル変換
    float4 centerLocal4 = mul(float4(sphereCenterWorld, 1.0), invWorld);
    float3 sphereCenterLocal = centerLocal4.xyz;

    // ローカルでの押し出し判定
    float3 dir = v.position - sphereCenterLocal;
    float dist = length(dir);

    if (dist < sphereRadiusWorld)
    {
        float3 n = normalize(dir);
        float penetration = sphereRadiusWorld - dist;
        
        // 安定化付き押し出し
        v.position += n * penetration * 1.0;
        //v.position += n * penetration * 0.8;

        // 速度補正
        float vn = dot(v.velocity, n);
        if (vn < 0.0f)
        {
            float3 velNormal = vn * n;
            float3 velTangent = v.velocity - velNormal;

            // restitutionとfrictionを適用
            //v.velocity = velTangent * (1.0 - friction) - velNormal * restitution;
            v.velocity = velTangent * 0.9 - velNormal * 0.5;
        }
    }
}

struct Plane
{
    float3 normal;
    float d;
};

cbuffer PLANE_CBUFFER : register(b6)
{
    Plane planes[4];
}

void CollideFloor(inout ClothVertex v, Plane p)
{
    //Plane worldFloor = { float3(1, 1, 0), 1 }; // y=0 の床
    
    //Plane worldFloor = { float3(0, 1, 0), 0 }; // y=0 の床
    Plane worldFloor = p; // y=0 の床
    
    float3 localNormal = mul(float4(worldFloor.normal, 0.0f), invWorld).xyz;
    localNormal = normalize(localNormal);

    // 平面上の1点を求める（ワールド空間で）
    float3 worldPoint = worldFloor.normal * worldFloor.d;

    // それをローカル空間に変換
    float3 localPoint = mul(float4(worldPoint, 1.0f), invWorld).xyz;

    // 布と平面の距離を求める
    float dist = dot(localNormal, v.position - localPoint);

    if (dist < 0.0f)
    {
        // 押し出し
        v.position -= localNormal * dist * 0.8f;

        // 速度補正
        float vn = dot(v.velocity, localNormal);
        if (vn < 0.0f)
        {
            float3 velNormal = vn * localNormal;
            float3 velTangent = v.velocity - velNormal;
            float friction = 0.4f;
            float restitution = 0.1f;

            v.velocity = velTangent * (1.0f - friction) - velNormal * restitution;
        }
    }

}

#define NUM_THREAD_X 16

[numthreads(NUM_THREAD_X, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (vertexCount <= id.x)
    {
        return;
    }
    ClothVertex inVertex = inVertices[id.x];
    ClothVertex outVertex = inVertices[id.x];
    
    outVertex = inVertex;
    
    float3 currentPos = inVertex.position;
    
    float3 force = float3(0, 0, 0);
    float3 avgVelocity = float3(0, 0, 0);
    uint edgeCount = 0;
    
    
    if (outVertex.isPinned == 1)
    { // 固定点だったら
        outVertex.position = inVertex.position;
        outVertex.velocity = float3(0.0, 0.0, 0.0);
        outVertex.oldPosition = inVertex.position;
        outVertex.oldVelocity = inVertex.velocity;
        outVertices[id.x] = outVertex;
        return;
    }
    
    outVertex.oldPosition = inVertex.position; // 前フレームの情報
    outVertex.oldVelocity = inVertex.velocity;

    uint baseEdge = id.x * MAX_EDGES; // 8個ごと
    
    for (uint i = 0; i < MAX_EDGES; ++i)
    {
        ClothEdge e = clothEdgeSRV[baseEdge + i];
        if (e.neighbor == 0xffffffffu)
        { // 近傍点がなかったら
            continue;
        }
        float3 neighborPos = inVertices[e.neighbor].position;
        float3 delta = neighborPos - currentPos;
        float distance = length(delta);
        if (distance > 1e-6f)
        {
            // 向き　＊　f 剛性　x　(距離-自然長)
            float springDelta = distance - e.restLength;
            float3 n = normalize(delta);
            // フックの法則００
            float springStiffness = SPRING_STIFFNESS; // 大きいほど硬くなる 剛性
            force += n * springStiffness * springDelta;
            
            // 近傍速度の平均
            avgVelocity += inVertices[e.neighbor].oldVelocity;
            
            edgeCount++;
        }
    }

    if (edgeCount > 0)
    {
        // 運動方程式 重力 × 質量
        // float4 forceWorld = float4(0, 0, 0, 0);
        float4 forceWorld = float4(0, -4.0 * MASS, 0, 0);
        // 重力をモデル空間に変換する
        float4 forceLocal4 = mul(forceWorld, invWorld);
        force += forceLocal4.xyz;
        float3 currentVelocity = inVertex.velocity;
#if 1
        // 風の影響
        float3 windDir = windEmitPosition - outVertex.oldPosition;
        float4 windDirWorld = normalize(float4(windDir, 0));
        float4 windDirLocal4 = mul(windDirWorld, invWorld);
        //float windBase = 5.0f;
        float windVariation = windPhaseOffset;

        float phase = elapsedTime * 2.0f + windPhaseOffset - currentPos.y * 1.5f /*+ windPhaseOffset*/;

        // 時間と位置によるゆらぎ
        float w = sin(phase) * 0.5f + 0.5f;
        float strength = windBase + windVariation * w;

        // 局所風速（ちょっとゆらぐ）
        float3 windVel = windDirLocal4 * strength;

        // 相対速度抗力
        float3 relativeVel = windVel - currentVelocity;
        float dragCoef = 0.2f;
        float3 windForce = dragCoef * relativeVel;

        // 強制的に揺らす外力（速度に依存しない）
        float wave = sin(elapsedTime * 2.5f - currentPos.y * 1.8f);

        float3 driveForce =
    float3(0.0f, 0.0f, wave) * 6.0f;

        force += driveForce;


        float heightFactor = saturate(-currentPos.y * 0.3f + 1.0f);
        windForce *= lerp(1.0f, 3.0f, heightFactor);

    //    // 波として伝播する風の力（直接加速）
    //    float waveForce =
    //sin(elapsedTime * 2.0f - currentPos.y * 1.5f);

    //    float3 oscillationForce =
    //float3(0.0f, 0.0f, waveForce) * 3.0f; // ←ここ超重要

    //    force += oscillationForce;

        force += windForce;
#endif
        avgVelocity /= float(edgeCount);
        avgVelocity *= 0.2f;
        // 減衰量
        const float damping = SPRING_DAMPING; // ばねの粘性
        float3 newVelocity = currentVelocity + (force / MASS) * deltaTime /** exp(-damping * deltaTime)*/;
        newVelocity *= (1.0f - GLOBAL_DAMPING);
        
        const float smmothing = 5.0;
        float3 smoothedVelocity = lerp(avgVelocity, newVelocity, exp(-smmothing * deltaTime));
        
        
        outVertex.velocity = smoothedVelocity;
        outVertex.position = currentPos + smoothedVelocity * deltaTime;
        
    }
    
    CollideSphere(outVertex);
    for (uint j = 0; j < 2; j++)
    {
        CollideFloor(outVertex, planes[j]);
    }

    outVertices[id.x] = outVertex;
}