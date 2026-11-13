struct ClothVertex
{
    float4 position;
    float4 normal;
    float4 tangent;
    float4 texcoord;
    
    float4 oldVelocity;
    float4 velocity;
    float4 newPosition;
};

struct ClothEdge
{
    uint id;
    float restLength;
    float3 delta;
};

float Rand(float n)
{
    return frac(sin(n) * 43758.5453123);
}

float RandRange(float seed, float min, float max)
{
    return lerp(min, max, Rand(seed));
}

StructuredBuffer<ClothVertex> clothVerticesSRV : register(t0);
StructuredBuffer<ClothEdge> clothEdges : register(t1);
RWStructuredBuffer<ClothVertex> clothVerticesUAV : register(u0);


#define CLOTH_EDGES_PER_VERTEX 4
#define SPRING_FORCE 20000.0
#define MASS 0.1
#define GRAVITY 0.98
#define DAMPING 0.04

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
}

#define NUM_THREAD_X 16

[numthreads(NUM_THREAD_X, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    ClothVertex readVertex = clothVerticesSRV[id.x];
    ClothVertex writeVertex = clothVerticesUAV[id.x];

     // --- 固定点はスキップ ---
    if (readVertex.oldVelocity.x == 1.0)
    {
        // 速度はゼロにして、位置を元のまま固定
        writeVertex.velocity = float4(0, 0, 0, 0);
        writeVertex.newPosition = readVertex.position;
        writeVertex.position = readVertex.position;
        clothVerticesUAV[id.x] = writeVertex;
        return;
    }
    
    float3 curretPosition = readVertex.position.xyz;
    float3 currentVelocity = readVertex.velocity.xyz;
    float3 force = float3(0.0, 0.0, 0.0);
    int edgeCount = 0;

    // --- スプリング力計算 ---
    for (int i = 0; i < CLOTH_EDGES_PER_VERTEX; i++)
    {
        ClothEdge edge = clothEdges[id.x * CLOTH_EDGES_PER_VERTEX + i];
        if (edge.id == 0xffffffff)
            continue;

        ClothVertex neighbor = clothVerticesSRV[edge.id];
        float3 neighborPos = neighbor.position.xyz;

        float3 delta = curretPosition - neighborPos;
        float dist = length(delta);
        if (dist > 1e-6f)
        {
            force += -SPRING_FORCE * (dist - edge.restLength) * (delta / dist);
        }
        edgeCount++;
    }

    // --- 重力 ---
    force += float3(0.0f, -MASS * gravity, 0.0f);

    // --- ダンピング ---
    force += -DAMPING * currentVelocity;

    // --- 加速度, 速度, 位置更新 ---
    float3 acceleration = force / MASS;
    float3 newVelocity = (currentVelocity + acceleration * deltaTime);
    float3 newPosition = curretPosition + newVelocity * deltaTime;

    // 仮の地面衝突
    if (newPosition.y < 0.0f)
    {
        newPosition.y = 0.0f;
        newVelocity.y = 0.0f;
    }

    // 書き戻し
    writeVertex.velocity = float4(newVelocity, 0.0f);
    writeVertex.newPosition = float4(newPosition, 1.0f);
    writeVertex.position = float4(newPosition, 1.0f);
    
    clothVerticesUAV[id.x] = writeVertex;
}