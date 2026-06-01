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

StructuredBuffer<ClothVertex> inVertices : register(t0);
StructuredBuffer<ClothEdge> clothEdgeSRV : register(t1);
RWStructuredBuffer<ClothVertex> outVertices : register(u0);

#define CLOTH_EDGES_PER_VERTEX 4
#define SPRING_FORCE 2000.0
#define MASS 0.1
#define GRAVITY 0.98
#define DAMPING 0.04
#define MAX_EDGES 8


cbuffer SPHERE_CBUFFER : register(b7)
{
    float3 worldPos;
    float radius;
    row_major float4x4 sphereWorld;
    row_major float4x4 sphereInvWorld;
}

cbuffer PRIMITIVE_CONSTANT_BUFFER : register(b0)
{
    row_major float4x4 world;
    
    int material;
    bool hasTangent;
    int skin;
    int padding;
    
    row_major float4x4 invWorld;
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

cbuffer SCENE_CONSTANT_BUFFER : register(b1)
{
    bool enableSSAO;
    float reflectionIntensity;
    float time;
    // shader のフラグ
    bool enableCascadedShadowMaps;
    
    bool enableSSR;
    bool enableFog;
    bool enableBloom;
    float deltaTime;
    
    float gravity;
    float3 aaa;
    
    //row_major float4x4 clothInvWorld;
}

cbuffer CLOTH_SIMULATE_CBUFFER : register(b10)
{
    float g;
    uint vertexCount;
    float windPhaseOffset;
    float align;
};

float4x4 RightMultiMatrix(float4 a)
{
    return float4x4(
    float4(a.w, -a.z, a.y, -a.x),
    float4(a.z, a.w, -a.x, -a.y),
    float4(-a.y, a.x, a.w, -a.z),
    float4(a.x, a.y, a.z, a.w));
}

float4x4 LeftMultiMatrix(float4 b)
{
    return float4x4(
    float4(b.w, b.z, -b.y, -b.x),
    float4(-b.z, b.w, b.x, -b.y),
    float4(b.y, -b.x, b.w, -b.z),
    float4(b.x, b.y, b.z, b.w));
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
        v.position += n * penetration * 0.8;

        // 速度補正
        float vn = dot(v.velocity, n);
        if (vn < 0.0f)
        {
            float3 velNormal = vn * n;
            float3 velTangent = v.velocity - velNormal;

            // restitutionとfrictionを適用
            v.velocity = velTangent * (1.0 - friction) - velNormal * restitution;
        }
    }
}


#define NUM_THREAD_X 16

[numthreads(NUM_THREAD_X, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= vertexCount)
    {
        return;
    }

    ClothVertex inVertex = inVertices[id.x];
    ClothVertex outVertex = inVertex;
    
    
    if (inVertex.isPinned == 1)
    {
        outVertex.position = inVertex.position;
        outVertex.velocity = float3(0.0, 0.0, 0.0);
        outVertices[id.x] = outVertex;
        return;
    }

    float4x4 errorMatrix = 0.0;
    float3 currentPosition = inVertices[id.x].position;
    uint baseEdge = id.x * MAX_EDGES; // 8個ごと
    for (uint i = 0; i < MAX_EDGES; ++i)
    {
        ClothEdge edge = clothEdgeSRV[baseEdge + i];
        if (edge.neighbor == 0xffffffffu)
        { // 近傍点がなかったら
            continue;
        }
        float3 delta = edge.delta;
        float3 currentDelta = inVertices[edge.neighbor].position - currentPosition;

        float4x4 m = RightMultiMatrix(float4(delta, 0.0)) - LeftMultiMatrix(float4(currentDelta, 0.0));
        errorMatrix += mul(transpose(m), m);
    }
    
    float4 currentRotation = inVertex.rotation;
    // quaternion gradient step
    float4 rotationGrad = 2.0 * mul(currentRotation, errorMatrix);
    const float thread = 0.25;
    float4 newRotation = normalize(currentRotation - rotationGrad * thread);
    outVertex.rotation = newRotation;
    //outVertex.oldVelocity = inVertex.velocity;
    outVertex.velocity = inVertex.velocity;
    outVertex.position = inVertex.position;
    
    
    
    outVertices[id.x] = outVertex;
}